/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import { AgentEnvironmentPort } from '../ports/AgentEnvironmentPort';
import { AgentObservation,
  AgentTabInfo,
  AgentTabSnapshot,
  BrowserAgentAction,
  AgentConstants,
  AgentPoint
} from '../core/AgentTypes';
import { BrowserRuntimeAccessor } from './BrowserRuntimeBridge';
import {
  BridgeAction,
  BrowserActionCommand,
  BrowserPerceptionPayload,
  BrowserTabInfo,
  BrowserTabSnapshot,
  BrowserTransportResponse,
  BrowserRuntimeState,
  Point,
  BrowserViewport
} from '../../../../shared/domain/BrowserAutomationTypes';
import { BrowserExecutionLoopService } from '../../execution/BrowserExecutionLoopService';
import { BrowserRuntimeService } from '../../execution/BrowserRuntimeService';
import { BrowserTransportCommandService } from '../../transport/BrowserTransportCommandService';
import { BrowserTransportHostService } from '../../transport/BrowserTransportHostService';
import { BrowserTransportProtocolService } from '../../transport/BrowserTransportProtocolService';
import { BrowserPageStateRegistryService } from '../../state/BrowserPageStateRegistryService';
import { BrowserDelayConfigService } from '../../BrowserDelayConfigService';
import { BrowserTransportMessageType } from '../../../../shared/domain/BrowserAutomationTypes';
import { BrowserAgentObservationService } from '../core/ObservationService';
import {BrowserAgentUtils} from '../core/Utils';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { image } from '@kit.ImageKit';
import { BrowserBinaryPayloadService } from '../../execution/BrowserBinaryPayloadService';
import { util } from '@kit.ArkTS';

const DOMAIN = 0x6020;
const TAG = 'EnvironmentAdapter';

class BrowserTabsPayload {
  public tabs: BrowserTabInfo[] = [];
  public activeTargetTabId: number = 0;
}

export class BrowserEnvironmentAdapter implements AgentEnvironmentPort {
  private readonly runtimeAccessor: BrowserRuntimeAccessor;
  private currentTabId: number = 0;

  constructor(runtimeAccessor: BrowserRuntimeAccessor) {
    this.runtimeAccessor = runtimeAccessor;
  }

  async perceive(): Promise<AgentObservation> {
    let runtime = this.persistRuntime(this.runtimeAccessor.getRuntime());
    const observationUpdate = BrowserExecutionLoopService.beginObservation(runtime);
    runtime = this.persistRuntime(observationUpdate.runtime);
    if (!observationUpdate.request) {
      throw new Error('browser observation request unavailable');
    }

    let perception: BrowserPerceptionPayload;
    let observation: AgentObservation = new AgentObservation();
    try {
      const response = await BrowserTransportHostService.dispatchAndWait(observationUpdate.request);
      perception = BrowserTransportProtocolService.parsePerceptionPayload(response.payloadText);
      runtime = this.persistRuntime(BrowserExecutionLoopService.applyPerception(runtime, perception).runtime);
      observation = this.mapObservation(runtime, perception);
      if (perception.hasOpenDialog) {
        this.currentTabId = perception.tabId;
        return observation;
      }
    } catch (e) {
      const error = BrowserAgentUtils.resolveErrorType(e as Object);
      if (error.errorType === AgentConstants.PROTOCOL_CDP_ERROR) {
        hilog.warn(DOMAIN, TAG, 'protocol cdp error: %{private}s', e.message);
      } else {
        hilog.warn(DOMAIN, TAG, 'ask_perception failed: %{private}s', e.message);
      }
      if (BrowserAgentUtils.hasUserIntervened(error.errorType) || error.errorType === AgentConstants.GROUP_CLOSED) {
        throw new Error(e.message);
      }
      // 不throw，继续获取html和tabs。这样可以获取到tab信息
    }

    hilog.warn(DOMAIN, TAG, 'start fetch tabs');
    const tabsPayloadText = await this.fetchTabsPayload(runtime, true);
    const tabsPayload = this.parseTabsPayload(tabsPayloadText);

    observation.tabs = this.toAgentTabs(tabsPayload.tabs);
    this.currentTabId = tabsPayload.activeTargetTabId;
    this.applyActiveTabObservationContext(observation);

    observation.html = await this.fetchCurrentHtml(runtime);

    return observation;
  }

  async execute(action: BrowserAgentAction): Promise<string> {
    let runtime = this.persistRuntime(this.runtimeAccessor.getRuntime());
    const viewport = runtime.currentPerception?.viewport;
    const command = this.toActionCommand(action, viewport);
    const actionUpdate = BrowserTransportCommandService.beginAction(runtime, command);
    runtime = this.persistRuntime(actionUpdate.runtime);
    let response: BrowserTransportResponse = new BrowserTransportResponse();
    try {
      try {
        response = await BrowserTransportHostService.dispatchAndWait(actionUpdate.request);
      } catch (error) {
        if (this.isTabCloseAction(action.action)) {
          response = await this.retryTabCloseAfterDialog(runtime.browserId, error as Object);
        } else if (!this.isGoBackAction(action.action)) {
          throw new Error(this.stringifyError(error as Object));
        } else {
          response = await BrowserTransportHostService.dispatchBridgeAction(
            runtime.browserId,
            BridgeAction.TAB_CLOSE,
            this.createEmptyParams()
          );
        }
      }
      runtime = this.persistRuntime(BrowserExecutionLoopService.completePendingAction(runtime).runtime);
    } catch (error) {
      this.completePendingActionBestEffort(runtime);
      throw new Error(this.stringifyError(error as Object));
    }
    if (response.messageType === BrowserTransportMessageType.PAGE_STATUS) {
      const postLoadedDelayMs = BrowserDelayConfigService.getPostLoadedDelayMs();
      if (postLoadedDelayMs > 0) {
        await BrowserDelayConfigService.waitPostLoadedDelay();
      }
      return response.payloadText;
    }
    if (command.requiresLoadingWait) {
      const postActionDelayMs = BrowserDelayConfigService.getPostActionDelayMs();
      if (postActionDelayMs > 0 &&
        response.messageType === BrowserTransportMessageType.ACTION_RESULT) {
        await BrowserDelayConfigService.waitPostActionDelay();
      }
    }
    return response.payloadText;
  }

  async perceiveTabs(needHtml: boolean, specifySession: boolean): Promise<AgentObservation> {
    //当perceive 错误时，大概率是ask_perception出错。获取tabs以支持go_back_to_start
    let runtime = this.persistRuntime(this.runtimeAccessor.getRuntime());
    const tabsPayloadText = await this.fetchTabsPayload(runtime, specifySession);
    const tabsPayload = this.parseTabsPayload(tabsPayloadText);

    const observation = new AgentObservation();
    observation.tabs = this.toAgentTabs(tabsPayload.tabs);
    this.currentTabId = tabsPayload.activeTargetTabId;
    this.applyActiveTabObservationContext(observation);
    if (needHtml) {
      observation.html = await this.fetchCurrentHtml(runtime);
    }
    return observation;
  }

  async waitForPageReady(currentUrl: string): Promise<void> {
    let runtime = this.persistRuntime(this.runtimeAccessor.getRuntime());
    if (currentUrl.length > 0 && runtime.currentPerception) {
      runtime.currentPerception.currentUrl = currentUrl;
      runtime = this.persistRuntime(runtime);
    }
    const waitUpdate = BrowserTransportCommandService.beginWaitLoading(runtime, null);
    runtime = this.persistRuntime(waitUpdate.runtime);
    let response: BrowserTransportResponse = new BrowserTransportResponse();
    try {
      response = await BrowserTransportHostService.dispatchAndWait(waitUpdate.request);
      runtime = this.persistRuntime(BrowserExecutionLoopService.completePendingAction(runtime).runtime);
    } catch (error) {
      this.completePendingActionBestEffort(runtime);
      throw new Error(this.stringifyError(error as Object));
    }
    const pageStatus = BrowserTransportProtocolService.parsePageStatus(response.payloadText);
    if (pageStatus.length > 0 && pageStatus !== 'loaded') {
      hilog.error(DOMAIN, TAG, `page loading failed: ${pageStatus}`);
      return;
    }
    const postLoadedDelayMs = BrowserDelayConfigService.getPostLoadedDelayMs();
    if (postLoadedDelayMs > 0) {
      await BrowserDelayConfigService.waitPostLoadedDelay();
    }
  }

  private mapObservation(runtime: BrowserRuntimeState, perception: BrowserPerceptionPayload): AgentObservation {
    const observation = BrowserAgentObservationService.fromPerception(perception);

    const pageState = BrowserPageStateRegistryService.resolveCurrentState(runtime);
    if (pageState) {
      observation.siteId = pageState.siteId;
      observation.pageStateId = pageState.stateId;
      observation.pageStateName = pageState.displayName;
    }
    return observation;
  }

  private async fetchCurrentHtml(runtime: BrowserRuntimeState): Promise<string> {
    try {
      const params = this.createEmptyParams();
      params.targetId = this.currentTabId as Object;
      const response = await BrowserTransportHostService.dispatchBridgeAction(
        runtime.browserId,
        BridgeAction.GET_PAGE_HTML,
        params,
        runtime.sessionKey
      );
      return this.extractPageHtmlPayload(response.payloadText);
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'fetchCurrentHtml error: %{public}s', e.message);
      if (e.message.includes('ACTION_UNSUPPORTED')) {
        throw new Error(e.message);
      }
      return '';
    }
  }

  private async fetchTabsPayload(runtime: BrowserRuntimeState, specifySession: boolean): Promise<string> {
    try {
      const params = this.createEmptyParams();
      params.scope = specifySession? 'taskGroup' as Object : 'currentTab' as Object;
      const response = await BrowserTransportHostService.dispatchBridgeAction(
        runtime.browserId,
        BridgeAction.TABS,
        params,
        specifySession? runtime.sessionKey : '',
      );
      return response.payloadText;
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'fetchTabsPayload error: %{public}s',  e.message);
      return '';
    }
  }

  private createEmptyParams(): Record<string, Object> {
    return JSON.parse('{}') as Record<string, Object>;
  }

  private completePendingActionBestEffort(runtime: BrowserRuntimeState): void {
    try {
      this.persistRuntime(BrowserExecutionLoopService.completePendingAction(runtime).runtime);
    } catch (error) {
      hilog.warn(
        DOMAIN,
        TAG,
        'failed to clear pending browser operation after error: %{public}s',
        this.stringifyError(error as Object)
      );
    }
  }

  private parseTabsPayload(payloadText: string): BrowserTabsPayload {
    const payload = new BrowserTabsPayload();
    if (payloadText.length <= 0) {
      return payload;
    }
    try {
      const raw = JSON.parse(payloadText) as Record<string, Object>;
      payload.activeTargetTabId = this.readNumber(raw, 'agentTabId');
      const tabs = raw.tabs;
      if (!Array.isArray(tabs)) {
        return payload;
      }
      for (let i = 0; i < tabs.length; i++) {
        payload.tabs.push(this.parseBrowserTabInfo(tabs[i], payload.activeTargetTabId));
      }
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parseTabsPayload error: %{public}s',  e.message);
      return payload;
    }
    return payload;
  }

  private parseBrowserTabInfo(rawTab: Object, activeTargetTabId: number): BrowserTabInfo {
    const tab = new BrowserTabInfo();
    const record = typeof rawTab === 'object' && rawTab !== null ?
      rawTab as Record<string, Object> :
      this.createEmptyParams();
    const tabId = this.readNumber(record, 'tabId');
    tab.tabId = tabId;
    tab.windowId = this.readNumber(record, 'windowId');
    tab.title = this.readString(record, 'title');
    tab.url = this.readString(record, 'url');
    tab.active = activeTargetTabId > 0 ? tabId === activeTargetTabId : this.readBoolean(record, 'active'); //正常情况下，插件返回的activeTargetTabId有效；任务起始时不给sessionId时activeTargetTabId无效，找真正的前台。
    tab.status = this.readString(record, 'status');
    tab.snapshots = this.parseSnapshots(record);
    tab.currentIndex = this.readNumber(record, 'currentIndex');
    return tab;
  }

  private findFocusedTab(original: AgentTabInfo[], current: AgentTabInfo[]): number {
    //不考虑用户移动tab
    if (current.length > original.length) {
      //通常情况：只多一个。
      //如果有多个？取最后一个
      //如果除了多tab，还有tab的snapshot变多了？比如页面点个东西，页面本身变化，还弹出来一个新页面？新页面可能是广告？？依然取最后一个
      return current[current.length - 1].tabId;
    } else if (current.length<original.length) {
      //需要考虑当前tabs为空怎么办啊？此时array是1还是0，1的话url是什么
    }
    if (current.length <= 0) {
      hilog.error(DOMAIN, TAG, 'Cannot find active tab because current tabs are empty!');
      return 0;
    }
    hilog.error(DOMAIN, TAG, 'Cannot find active tab!');
    return current[current.length - 1].tabId;
  }

  private parseSnapshots(record: Record<string, Object>): BrowserTabSnapshot[] {
    const snapshots: BrowserTabSnapshot[] = [];
    const rawSnapshots = record.snapshots;
    if (!Array.isArray(rawSnapshots)) {
      return snapshots;
    }
    for (let i = 0; i < rawSnapshots.length; i++) {
      if (typeof rawSnapshots[i] === 'object' && rawSnapshots[i] !== null) {
        const snapshotRecord = rawSnapshots[i] as Record<string, Object>;
        const item = new BrowserTabSnapshot();
        item.url = this.readString(snapshotRecord, 'url');
        item.title = this.readString(snapshotRecord, 'title');
        snapshots.push(item);
      }
    }
    return snapshots;
  }

  private toAgentTabs(tabs: BrowserTabInfo[]): AgentTabInfo[] {
    const nextTabs: AgentTabInfo[] = [];
    for (let i = 0; i < tabs.length; i++) {
      const tab = new AgentTabInfo();
      tab.tabId = tabs[i].tabId;
      tab.windowId = tabs[i].windowId;
      tab.active = tabs[i].active;
      tab.status = tabs[i].status;
      tab.currentIndex = tabs[i].currentIndex;
      for (let j = 0; j < tabs[i].snapshots.length; j++) {
        const agentSnapshot = new AgentTabSnapshot();
        agentSnapshot.url = tabs[i].snapshots[j].url;
        agentSnapshot.title = tabs[i].snapshots[j].title;
        tab.snapshots.push(agentSnapshot);
      }
      if (tabs[i].snapshots.length > 0) {
        if (tab.currentIndex < 0 || tab.currentIndex >= tabs[i].snapshots.length) {
          throw new Error(`Parse perceive error: parse tabs error: currentIndex ${tab.currentIndex} out of range (${tabs[i].snapshots.length}).`);
        }
        tab.url = tabs[i].snapshots[tab.currentIndex].url;
        tab.title = tabs[i].snapshots[tab.currentIndex].title;
      } else {
        tab.url = tabs[i].url;
        tab.title = tabs[i].title;
      }
      nextTabs.push(tab);
    }
    return nextTabs;
  }

  private applyActiveTabObservationContext(observation: AgentObservation): void {
    const activeTab = this.findActiveTab(observation.tabs);
    if (!activeTab) {
      return;
    }
    if (activeTab.url.length > 0) {
      observation.currentUrl = activeTab.url;
    }
    if (activeTab.title.length > 0) {
      observation.currentTitle = activeTab.title;
    }
    observation.tabStatus = activeTab.status;
  }

  private findActiveTab(tabs: AgentTabInfo[]): AgentTabInfo | null {
    hilog.debug(DOMAIN, TAG, 'perceive currentTabId = %{public}s', this.currentTabId);
    if (this.currentTabId>0) { //tab分组，使用这个判断。
      for (let i = 0; i < tabs.length; i++) {
        if (tabs[i].tabId === this.currentTabId) {
          return tabs[i];
        }
      }
    }
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].active) {
        return tabs[i];
      }
    }
    return null;
  }

  private extractPageHtmlPayload(payloadText: string): string {
    if (payloadText.length <= 0) {
      return '';
    }
    try {
      const payload = JSON.parse(payloadText) as Record<string, Object>;
      return this.readString(payload, 'html');
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'extractPageHtmlPayload error: %{public}s', e.message);
      return '';
    }
  }

  private readString(value: Record<string, Object>, key: string): string {
    const nextValue = value[key];
    return typeof nextValue === 'string' ? nextValue : '';
  }

  private readNumber(value: Record<string, Object>, key: string): number {
    const nextValue = value[key];
    return typeof nextValue === 'number' ? nextValue : 0;
  }

  private readBoolean(value: Record<string, Object>, key: string): boolean {
    const nextValue = value[key];
    return typeof nextValue === 'boolean' ? nextValue : false;
  }

  private toActionCommand(action: BrowserAgentAction, viewport?: BrowserViewport): BrowserActionCommand {
    const command = new BrowserActionCommand();
    command.action = action.action;
    command.payloadText = action.action;
    command.requiresLoadingWait = action.requiresLoadingWait;
    command.reason = action.reason;
    command.targetTabId = this.currentTabId;
    command.skipSensitiveActionCheck = action.skipSensitiveActionCheck === true;
    command.element = action.element;
    if (action.point && viewport) {
      command.point = this.scaleCoordinate(action.point, viewport.width, viewport.height);
    }
    return command;
  }

  private scaleCoordinate(raw: AgentPoint, viewportWidth: number, viewportHeight: number): Point {
    const SCALE_RANGE = 1000;
    const point = new Point();
    point.x = Math.floor(raw.x * viewportWidth / SCALE_RANGE);
    point.y = Math.floor(raw.y * viewportHeight / SCALE_RANGE);
    return point;
  }

  private persistRuntime(runtime: BrowserRuntimeState): BrowserRuntimeState {
    const nextRuntime = BrowserRuntimeService.updateRuntime(runtime);
    this.runtimeAccessor.setRuntime(nextRuntime);
    return nextRuntime;
  }

  private isGoBackAction(actionText: string): boolean {
    return actionText === 'GO_BACK';
  }

  private isTabCloseAction(actionText: string): boolean {
    return actionText === 'TAB_CLOSE';
  }

  private async retryTabCloseAfterDialog(browserId: number, originalError: Object): Promise<BrowserTransportResponse> {
    const dialogParams = this.createEmptyParams();
    dialogParams.action = 'accept';
    await BrowserTransportHostService.dispatchBridgeAction(
      browserId,
      BridgeAction.DIALOG,
      dialogParams
    ).catch(() => {});

    try {
      return await BrowserTransportHostService.dispatchBridgeAction(
        browserId,
        BridgeAction.TAB_CLOSE,
        this.createEmptyParams()
      );
    } catch (retryError) {
      const original = this.stringifyError(originalError);
      const retry = this.stringifyError(retryError as Object);
      throw new Error(`Failed to close tab after accepting dialog. original=${original}; retry=${retry}`);
    }
  }

  private stringifyError(error: Object): string {
    if (error instanceof Error) {
      return error.message;
    }
    return `${error}`;
  }
}
