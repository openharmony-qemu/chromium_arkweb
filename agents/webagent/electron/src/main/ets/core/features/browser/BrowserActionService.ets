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

import {
  AgentActionType,
  BridgeAction,
  BrowserActionCommand,
  BrowserActionKind,
  Point
} from '../../shared/domain/BrowserAutomationTypes';
import { BrowserAgentUtils } from './agent/core/Utils';
import { isNavigationUrlAllowed } from './agent/core/AgentTypes';
import { hilog } from '@kit.PerformanceAnalysisKit';
const DOMAIN = 0x6020;
const TAG = 'BrowserActionService';

class BridgeParams { //这个东西待修改，一堆不同action的参数堆在一起，一坨
  public elementId: string = '';
  public value: string = '';
  public direction: string = '';
  public amount: string = '';
  public selector: string = '';
  public url: string = '';
  public key: string = '';
  public files: string[] = [];
  public mode: string = '';
  public format: string = '';
  public text: string = '';
  public occurrence: number = 0;
}

function makeParams(): BridgeParams {
  return new BridgeParams();
}

function paramsToRecord(params: BridgeParams): Record<string, Object> {
  const record: Record<string, Object> = {};
  if (params.elementId.length > 0) {
    record.elementId = params.elementId;
  }
  if (params.value.length > 0) {
    record.value = params.value;
  }
  if (params.direction.length > 0) {
    record.direction = params.direction;
  }
  if (params.amount.length > 0) {
    record.amount = params.amount;
  }
  if (params.selector.length > 0) {
    record.selector = params.selector;
  }
  if (params.url.length > 0) {
    record.url = params.url;
  }
  if (params.key.length > 0) {
    record.key = params.key;
  }
  if (params.files.length > 0) {
    record.files = params.files;
  }
  if (params.mode.length > 0) {
    record.mode = params.mode;
  }
  if (params.format.length > 0) {
    record.format = params.format;
  }
  if (params.text.length > 0) {
    record.text = params.text;
  }
  if (params.occurrence > 0) {
    record.occurrence = params.occurrence as Object;
  }
  return record;
}

export class BridgeActionResult {
  public action: string;
  public params: Record<string, Object>;

  constructor(action: string, params: Record<string, Object>) {
    this.action = action;
    this.params = params;
  }
}

export class BrowserActionService {
  static buildUnknownCommand(actionText: string): BrowserActionCommand {
    const command = new BrowserActionCommand();
    command.kind = BrowserActionKind.UNKNOWN;
    command.action = actionText.trim();
    command.actionType = AgentActionType.UNKNOWN;
    command.payloadText = command.action;
    return command;
  }

  static parseAgentAction(actionText: string): BrowserActionCommand {
    const command = new BrowserActionCommand();
    const normalized = actionText.trim();
    const funcName = normalized.split(' ')[0].toUpperCase();
    const brackets = BrowserAgentUtils.extractBracketParams(normalized);

    command.action = normalized;
    command.payloadText = normalized;
    command.actionType = funcName;

    if (funcName === 'DIALOG') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.DIALOG;
      command.target = brackets.length > 0 ? brackets[0].toLowerCase() : '';
      command.value = brackets.length > 1 ? brackets[1] : '';
      return command;
    }
    if (funcName === 'CLICK') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.CLICK;
      command.target = brackets.length > 0 ? brackets[0] : '';
      command.requiresLoadingWait = true;
      return command;
    }
    if (funcName === 'SEARCH' || funcName === 'SEARCH_BY_QUERY') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.SEARCH;
      command.target = funcName === 'SEARCH' && brackets.length > 0 ? brackets[0] : '';
      command.value = funcName === 'SEARCH' ? (brackets.length > 1 ? brackets[1] : '') : (brackets.length > 0 ? brackets[0] : '');
      command.requiresLoadingWait = true;
      return command;
    }
    if (funcName === 'FILL') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.FILL;
      const fillMode = brackets.length > 0 ? brackets[0].toUpperCase() : 'REPLACE';
      command.fillMode = fillMode.toLowerCase();
      command.fillFormat = 'plain';
      command.target = brackets.length > 1 ? brackets[1] : '';
      command.value = brackets.length > 2 ? brackets[2] : '';
      return command;
    }
    if (funcName === 'HOVER') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.HOVER;
      command.target = brackets.length > 0 ? brackets[0] : '';
      return command;
    }
    if (funcName === 'SCROLL') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.SCROLL;
      command.target = brackets.length > 0 ? brackets[0] : 'DOWN';
      command.value = brackets.length > 1 ? brackets[1] : '';
      command.amount = brackets.length > 2 ? brackets[2] : '';
      return command;
    }
    if (funcName === 'NAVIGATION') {
      command.kind = BrowserActionKind.NAVIGATION;
      command.actionType = AgentActionType.NAVIGATION;
      command.target = brackets.length > 0 ? brackets[0] : '';
      command.requiresLoadingWait = true;
      return command;
    }
    if (funcName === 'NAVIGATE_TO') {
      command.kind = BrowserActionKind.NAVIGATION;
      command.actionType = AgentActionType.NAVIGATE_TO;
      command.target = brackets.length > 0 ? brackets[0] : '';
      command.requiresLoadingWait = true;
      return command;
    }
    if (funcName === 'GO_BACK') {
      command.kind = BrowserActionKind.NAVIGATION;
      command.actionType = AgentActionType.GO_BACK;
      command.requiresLoadingWait = true;
      return command;
    }
    if (funcName === 'TAB_NEW') {
      command.kind = BrowserActionKind.NAVIGATION;
      command.actionType = AgentActionType.TAB_NEW;
      command.target = brackets.length > 0 ? brackets[0] : '';
      command.requiresLoadingWait = true;
      return command;
    }
    if (funcName === 'TAB_CLOSE') {
      command.kind = BrowserActionKind.NAVIGATION;
      command.actionType = AgentActionType.TAB_CLOSE;
      return command;
    }
    if (funcName === 'TAB_FOCUS') {
      command.kind = BrowserActionKind.NAVIGATION;
      command.actionType = AgentActionType.TAB_FOCUS;
      command.target = brackets.length > 0 ? brackets[0] : '';
      return command;
    }
    if (funcName === 'SCREENSHOT') {
      command.kind = BrowserActionKind.TASK_CONTROL;
      command.actionType = AgentActionType.SCREENSHOT;
      return command;
    }
    if (funcName === 'PRESS') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.PRESS;
      command.target = brackets.length > 0 ? brackets[0] : '';
      return command;
    }
    if (funcName === 'SELECT') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.SELECT;
      command.target = brackets.length > 0 ? brackets[0] : '';
      command.value = brackets.length > 1 ? brackets[1] : '';
      return command;
    }
    if (funcName === 'ARROW') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.ARROW;
      command.target = brackets.length > 0 ? brackets[0] : 'DOWN';
      return command;
    }
    if (funcName === 'WAIT') {
      command.kind = BrowserActionKind.TASK_CONTROL;
      command.actionType = AgentActionType.WAIT;
      return command;
    }
    if (funcName === 'FINISH') {
      command.kind = BrowserActionKind.TASK_CONTROL;
      command.actionType = AgentActionType.FINISH;
      return command;
    }
    if (funcName === 'USER_CONTROL') {
      command.kind = BrowserActionKind.TASK_CONTROL;
      command.actionType = AgentActionType.USER_CONTROL;
      return command;
    }
    if (funcName === 'AGENT_CONTROL') {
      command.kind = BrowserActionKind.TASK_CONTROL;
      command.actionType = AgentActionType.AGENT_CONTROL;
      return command;
    }
    if (funcName === 'REQUEST_HUMAN_TAKEOVER') {
      command.kind = BrowserActionKind.TASK_CONTROL;
      command.actionType = AgentActionType.REQUEST_HUMAN_TAKEOVER;
      return command;
    }
    if (funcName === 'UPLOAD') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.UPLOAD;
      command.target = brackets.length > 0 ? brackets[0] : '';
      command.value = brackets.length > 1 ? brackets.slice(1).join('|||') : '';
      return command;
    }
    if (funcName === 'QUERY') {
      command.kind = BrowserActionKind.TASK_CONTROL;
      command.actionType = AgentActionType.QUERY;
      return command;
    }
    if (funcName === 'SKIP') {
      command.kind = BrowserActionKind.TASK_CONTROL;
      command.actionType = AgentActionType.SKIP;
      return command;
    }
    if (funcName === 'TAKE_NOTE') {
      command.kind = BrowserActionKind.INFORMATION;
      command.actionType = AgentActionType.TAKE_NOTE;
      command.value = brackets.length > 0 ? brackets[0] : '';
      return command;
    }
    if (funcName === 'CALCULATE') {
      command.kind = BrowserActionKind.INFORMATION;
      command.actionType = AgentActionType.CALCULATE;
      command.value = brackets.length > 0 ? brackets[0] : '';
      return command;
    }
    if (funcName === 'RICH_TEXT') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.FILL;
      const fillMode = brackets.length > 0 ? brackets[0].toUpperCase() : 'REPLACE';
      command.fillMode = fillMode.toLowerCase();
      command.fillFormat = 'rich';
      command.target = brackets.length > 1 ? brackets[1] : '';
      command.value = brackets.length > 2 ? brackets[2] : '';
      return command;
    }
    if (funcName === 'GET_HREF') {
      command.kind = BrowserActionKind.INFORMATION;
      command.actionType = AgentActionType.GET_HREF;
      command.value = brackets.length > 0 ? brackets[0] : '';
      return command;
    }
    if (funcName === 'DISPATCH') {
      command.kind = BrowserActionKind.INFORMATION;
      command.actionType = AgentActionType.DISPATCH;
      command.target = brackets.length > 0 ? brackets[0] : '';
      command.value = brackets.length > 1 ? brackets[1] : '';
      return command;
    }
    if (funcName === 'SELECT_TEXT') {
      command.kind = BrowserActionKind.INPUT;
      command.actionType = AgentActionType.SELECT_TEXT;
      command.target = brackets.length > 0 ? brackets[0] : '';
      command.value = brackets.length > 1 ? brackets[1] : '';
      command.amount = brackets.length > 2 ? brackets[2] : '';
      return command;
    }
    return BrowserActionService.buildUnknownCommand(normalized);
  }

  static toBridgeAction(command: BrowserActionCommand): BridgeActionResult | null {
    const addTargetId = (record: Record<string, Object>): Record<string, Object> => {
      if (command.targetTabId > 0) {
        record.targetId = command.targetTabId as Object;
      }
      return record;
    };
    // 按 command.element 类型决定发 elementId（数字 id）还是 selector（CSS string）给插件。
    // element 为 null（未解析）时回退到 target 文本（兼容旧路径）。
    const applyElementTarget = (params: BridgeParams): void => {
      if (typeof command.element === 'string' && command.element.length > 0) {
        params.selector = command.element;
      } else if (typeof command.element === 'number') {
        params.elementId = String(command.element);
      } else {
        params.elementId = command.target;
      }
    };
    if (command.actionType === AgentActionType.DIALOG) {
      const params: Record<string, Object> = {};
      params.action = command.target;
      if (command.value.length > 0) {
        params.promptText = command.value;
      }
      return new BridgeActionResult(BridgeAction.DIALOG, addTargetId(params));
    }
    if (command.actionType === AgentActionType.CLICK) {
      const params = makeParams();
      if (!command.point) { // 若有point，不应设置element，否则插件优先使用elementId会导致无法进行坐标操控
        applyElementTarget(params);
      }
      const record = paramsToRecord(params);
      if (command.point) {
        const point = new Point();
        point.x = command.point.x;
        point.y = command.point.y;
        record.point = point as Object;
      }
      return new BridgeActionResult(BridgeAction.CLICK, addTargetId(record));
    }
    if (command.actionType === AgentActionType.SEARCH) {
      const params = makeParams();
      applyElementTarget(params);
      params.value = command.value;
      return new BridgeActionResult(BridgeAction.SEARCH, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.FILL) {
      const params = makeParams();
      applyElementTarget(params);
      params.value = command.value;
      params.mode = command.fillMode.length > 0 ? command.fillMode : 'replace';
      params.format = command.fillFormat.length > 0 ? command.fillFormat : 'plain';
      return new BridgeActionResult(BridgeAction.FILL, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.HOVER) {
      const params = makeParams();
      applyElementTarget(params);
      return new BridgeActionResult(BridgeAction.HOVER, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.SCROLL) {
      const params = makeParams();
      params.direction = command.target.toLowerCase();
      params.amount = command.amount.length > 0 ? command.amount : 'page';
      if (command.value.length > 0 && command.value.trim().toLowerCase() !== 'window') {
        params.elementId = command.value;
      } else {
        params.selector = 'window';
      }
      return new BridgeActionResult(BridgeAction.SCROLL, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.NAVIGATION) {
      if (!isNavigationUrlAllowed(command.target)) {
        hilog.debug(DOMAIN, TAG, 'blocked navigation URL: %{private}s', command.target);
        throw new Error(
          `Refuse to dispatch NAVIGATION: URL is invalid or blocked by navigation policy}`
        );
      }
      return new BridgeActionResult(
        BridgeAction.TAB_NEW,
        addTargetId(JSON.parse(`{"url":${JSON.stringify(command.target)},"active":true}`) as Record<string, Object>)
      );
    }
    if (command.actionType === AgentActionType.NAVIGATE_TO) {
      if (!isNavigationUrlAllowed(command.target)) {
        throw new Error(
          `Refuse to dispatch NAVIGATE_TO: URL is invalid or blocked by navigation policy, got: ${command.target}`
        );
      }
      return new BridgeActionResult(
        BridgeAction.NAVIGATE_TO,

        addTargetId(JSON.parse(`{"url":${JSON.stringify(command.target)}}`) as Record<string, Object>)
      );
    }
    if (command.actionType === AgentActionType.GO_BACK) {
      return new BridgeActionResult(BridgeAction.NAV_BACK, addTargetId({}));
    }
    if (command.actionType === AgentActionType.TAB_NEW) {
      const params = makeParams();
      if (command.target.length > 0) {
        if (!isNavigationUrlAllowed(command.target)) {
          throw new Error(
            `Refuse to dispatch TAB_NEW: URL is invalid or blocked by navigation policy, got: ${command.target}`
          );
        }
        params.url = command.target;
      }
      const record = paramsToRecord(params);
      record.active = true;
      return new BridgeActionResult(BridgeAction.TAB_NEW, addTargetId(record));
    }
    if (command.actionType === AgentActionType.TAB_CLOSE) {
      return new BridgeActionResult(BridgeAction.TAB_CLOSE, addTargetId({}));
    }
    if (command.actionType === AgentActionType.TAB_FOCUS) {
      const tabId = Number(command.target);
      if (!Number.isFinite(tabId) || tabId <= 0) {
        return null;
      }
      return new BridgeActionResult(
        BridgeAction.TAB_FOCUS,
        JSON.parse(`{"tabId":${Math.floor(tabId)}}`) as Record<string, Object>
      );
    }
    if (command.actionType === AgentActionType.SCREENSHOT) {
      return new BridgeActionResult(BridgeAction.SCREENSHOT, addTargetId({}));
    }
    if (command.actionType === AgentActionType.PRESS) {
      const params = makeParams();
      params.key = command.target;
      return new BridgeActionResult(BridgeAction.PRESS, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.SELECT) {
      const params = makeParams();
      applyElementTarget(params);
      params.value = command.value;
      return new BridgeActionResult(BridgeAction.SELECT, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.ARROW) {
      const direction = command.target.length > 0 ?
        `${command.target.charAt(0).toUpperCase()}${command.target.substring(1).toLowerCase()}` :
        'Down';
      const params = makeParams();
      params.key = `Arrow${direction}`;
      return new BridgeActionResult(BridgeAction.PRESS, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.WAIT) {
      return new BridgeActionResult(BridgeAction.WAIT, addTargetId({}));
    }
    if (command.actionType === AgentActionType.USER_CONTROL) {
      const params = makeParams();
      return new BridgeActionResult(BridgeAction.USER_CONTROL, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.REQUEST_HUMAN_TAKEOVER) {
      return new BridgeActionResult(BridgeAction.REQUEST_HUMAN_TAKEOVER, addTargetId({}));
    }
    if (command.actionType === AgentActionType.AGENT_CONTROL) {
      //现在传输链路一坨。agent_control走这个的时候，默认useCurrentTab为true，只用于用户接管继续情况。任务开始时的agent_control在runner里面绕过此链路。后续改进
      const params: Record<string, Object> = {};
      params.useCurrentTab = true as Object;
      return new BridgeActionResult(BridgeAction.AGENT_CONTROL, params);
    }
    if (command.actionType === AgentActionType.FINISH) {
      const params = makeParams();
      return new BridgeActionResult(BridgeAction.FINISH, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.QUERY) {
      return new BridgeActionResult(BridgeAction.QUERY, addTargetId({}));
    }
    if (command.actionType === AgentActionType.UPLOAD) {
      const params = makeParams();
      applyElementTarget(params);
      params.files = command.value.length > 0 ? command.value.split('|||') : [];
      return new BridgeActionResult(BridgeAction.UPLOAD, addTargetId(paramsToRecord(params)));
    }
    if (command.actionType === AgentActionType.SELECT_TEXT) {
      const params = makeParams();
      applyElementTarget(params);
      params.text = command.value;
      params.occurrence = Number(command.amount) > 0 ? Number(command.amount) : 1;
      return new BridgeActionResult(BridgeAction.SELECT_TEXT, addTargetId(paramsToRecord(params)));
    }
    return null;
  }

}
