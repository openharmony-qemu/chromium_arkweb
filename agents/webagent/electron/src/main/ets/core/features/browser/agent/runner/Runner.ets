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

import { BrowserRuntimeState, BrowserPhase,
  BrowserTransportResponse, BrowserTransportConstants } from '../../../../shared/domain/BrowserAutomationTypes';
import {
  AgentGraphStatus,
  AgentPhase,
  AgentConstants,
  BrowserAgentAction,
  DetectionResult,
  isNavigationUrlAllowed
} from '../core/AgentTypes';
import { AgentState } from '../core/AgentState';
import { BrowserAgentUtils } from '../core/Utils';
import { BrowserAgentStateMapper } from '../adapters/StateMapper';
import { BrowserEnvironmentAdapter } from '../adapters/BrowserEnvironmentAdapter';
import { BrowserElementCropService } from '../adapters/BrowserElementCropService';
import { BrowserRuntimeAccessor } from '../adapters/BrowserRuntimeBridge';
import { BrowserWorkspaceAdapter } from '../adapters/BrowserWorkspaceAdapter';
import { NavigateConfigAdapter } from '../adapters/NavigateConfigAdapter';
import { AgentToolRegistryService } from '../tools/AgentToolRegistryService';
import { BrowserRuntimeService } from '../../execution/BrowserRuntimeService';
import { AgentGraphRunResult } from '../graph/AgentGraph';
import { BrowserAgentGraph } from '../graph/Graph';
import { BrowserAgentEventCallbacks } from '../ports/AgentContext';
import { AgentLLMPort } from '../ports/AgentLLMPort';
import { vConfig } from '../VersionConfig';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { BrowserTransportHostService } from '../../transport/BrowserTransportHostService';
import { BrowserTransportProtocolService } from '../../transport/BrowserTransportProtocolService';
import { AgentLLMRequest, BrowserPromptConstants, AgentTabInfo, AgentObservation } from '../core/AgentTypes';
import { AgentLLMScenes } from '../core/AgentLLMScenes';
import { AgentEnvironmentPort } from '../ports/AgentEnvironmentPort';
import BuildProfile from 'BuildProfile';
import { BrowserAgentObservationService } from '../core/ObservationService';
import { BrowserAgentPromptDetectionService } from '../core/PromptDetectionService';

const DOMAIN = 0x6020;
const TAG = 'BrowserAgentGraph';

class InMemoryBrowserRuntimeAccessor implements BrowserRuntimeAccessor {
  private runtime: BrowserRuntimeState;

  constructor(runtime: BrowserRuntimeState) {
    this.runtime = runtime;
  }

  getRuntime(): BrowserRuntimeState {
    return this.runtime;
  }

  setRuntime(runtime: BrowserRuntimeState): void {
    this.runtime = runtime;
  }
}

class DecideStartPointResponse {
  public reason: string = '';
  public decision: boolean = false;
  public url: string = '';
};

interface TimePayload {
  lastCreateTabTs: number;
};

export class BrowserAgentRunner {
  private readonly runtimeAccessor: BrowserRuntimeAccessor;
  private readonly graph: BrowserAgentGraph;
  private readonly agentType: string;
  private readonly llm: AgentLLMPort;
  private readonly events: BrowserAgentEventCallbacks;
  private finalAgentState: AgentState | null = null;
  private groupName: string = '';
  private callLLMTime: number = 0;

  constructor(initialRuntime: BrowserRuntimeState, agentType: string = 'browser',
    llm: AgentLLMPort, events: BrowserAgentEventCallbacks, groupName: string = '') {
    this.runtimeAccessor = new InMemoryBrowserRuntimeAccessor(initialRuntime);
    this.graph = new BrowserAgentGraph(BuildProfile.DEBUG);
    this.agentType = agentType;
    this.llm = llm;
    this.events = events;
    this.groupName = groupName;
  }

  requestStop(): void {
    this.graph.requestStop();
  }

  async run(): Promise<BrowserRuntimeState> {
    const runtime = this.runtimeAccessor.getRuntime();
    if (!vConfig.initSuccess) {
      hilog.error(DOMAIN, TAG, 'Version config init failed!');
      runtime.phase = BrowserPhase.FAILED;
      runtime.errorMessage = 'Version config init failed!';
      return runtime;
    }
    this.events.onChunk(`start run`, false);

    if (AppStorage.get<boolean>(BrowserTransportConstants.HAS_REQUESTED_TASK_STOP) === true) {
      runtime.phase = BrowserPhase.FAILED;
      runtime.errorMessage = '用户主动停止任务';
      runtime.stopRequested = true;
      return runtime;
    }
    const state = BrowserAgentStateMapper.toAgentState(runtime);
    const env = new BrowserEnvironmentAdapter(this.runtimeAccessor);
    const cropper = new BrowserElementCropService();
    const llm: AgentLLMPort = this.llm;
    const workspace = new BrowserWorkspaceAdapter(this.runtimeAccessor);
    const urlConfig = new NavigateConfigAdapter();

    // 批跑的 needClearBrowser 只负责失败恢复和任务结束后的页面清理。
    // 批跑开始前已由 BATCH_RUNTIME_RESET 打开稳定的百度落地页，仍需像单跑一样
    // 通过 decideStartTab 判断应复用该页还是导航到任务目标站点。
    const startFromCurrentTab = await this.decideStartTab(runtime, llm, env);
    if (startFromCurrentTab.decision) {
      state.task = AgentConstants.CONTINUE_PREFIX + state.task;
    }
    if (AppStorage.get<boolean>(BrowserTransportConstants.HAS_REQUESTED_TASK_STOP) === true) {
      runtime.phase = BrowserPhase.FAILED;
      runtime.errorMessage = '用户主动停止任务';
      runtime.stopRequested = true;
      return runtime;
    }
    this.events.onChunk(`send agent control`, false);
    await this.sendAgentControl(runtime, startFromCurrentTab);

    this.events.onChunk(`DEBUG mode = ${BuildProfile.DEBUG}`, false);

    let graphResult: string | AgentGraphRunResult<AgentState>;
    try {
      graphResult = await this.graph.run(state, {
        env,
        llm,
        tools: new AgentToolRegistryService(cropper, env, llm, workspace, urlConfig,
          this.agentType, this.events),
        workspace,
        urlConfig,
        events: this.events,
        controls: {
          needClearBrowser: runtime.needClearBrowser,
          cacheScreenshot: runtime.cacheScreenshot,
          proceduralMemory: runtime.proceduralMemory,
          sessionMemory: runtime.sessionMemory
        }
      });
    } catch (e) {
      graphResult = `Agent graph run failed: ${e.message}`;
    }
    await this.sendFinish(env);

    if (typeof graphResult === 'string') {
      runtime.phase = BrowserPhase.FAILED;
      runtime.errorMessage = graphResult;
      return runtime;
    }
    const nextState = graphResult.state;
    const result = graphResult.result;
    if (result.status !== AgentGraphStatus.FINISH) {
      nextState.phase = AgentPhase.FAILED;
      nextState.errorMessage = result.message.length > 0 ? result.message : `browser agent graph failed at ${result.nodeName}`;
    }
    this.finalAgentState = nextState;
    const nextRuntime = BrowserAgentStateMapper.applyAgentState(this.runtimeAccessor.getRuntime(), nextState);
    const persistedRuntime = BrowserRuntimeService.updateRuntime(nextRuntime);
    this.runtimeAccessor.setRuntime(persistedRuntime);
    return persistedRuntime;
  }

  async sendFinish(env: AgentEnvironmentPort) {
    try{
      const action = new BrowserAgentAction();
      action.action = 'FINISH';
      action.actionType = 'FINISH';
      action.element = null;
      action.params = [];
      action.requiresLoadingWait = false;
      action.reason = '';
      await env.execute(action); //通知插件进入idle状态
      this.events.onChunk('finish sent successfully', false);
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'send finish failed: %{public}s', e.message);
    }
  }

  async decideStartTab(
    runtime: BrowserRuntimeState,
    llm: AgentLLMPort,
    env: AgentEnvironmentPort
  ): Promise<DecideStartPointResponse> {
    // 首步「当前页 vs 新开」判断是否带截图。默认走纯文本（url+title 足以判断相关性），
    // 省掉带图 vision 请求与 ASK_PERCEPTION 抓屏，降低首步时延；start_tab_text_only=false 可回退到带图。
    const useScreenshot = !vConfig.start_tab_text_only && !vConfig.text_only;
    const params: Record<string, Object> = {};
    params.useActiveTab = true as Object;
    if (vConfig.text_only) {
      params.textOnly = true as Object;
    }
    let response: BrowserTransportResponse | null = null;
    let currentTab: AgentTabInfo | null = null;
    try {
      //这里绕过perceive链路，是为了发送params
      response = await BrowserTransportHostService.dispatchBridgeAction(
        runtime.browserId,
        'ASK_PERCEPTION',
        params,
        runtime.sessionKey
      );
      const observation = await env.perceiveTabs(false, false);
      //observation.currentUrl是用插件的currentTabId算出来的，不一定是前台的。需要手动算一次
      for (let i = 0; i < observation.tabs.length; i++) {
        if (observation.tabs[i].active) {
          currentTab = observation.tabs[i];
        }
      }
    } catch (e) { //在空tab感知会出错
      hilog.warn(DOMAIN, TAG, 'ask_perception failed during url decision, likely due to an empty page: %{private}s', e.message);
    }

    const request = this.buildDecideStartTabRequest(runtime.task, currentTab, response, useScreenshot, true);

    let promptDetectionObservation = new AgentObservation();
    if (response) {
      const perception = BrowserTransportProtocolService.parsePerceptionPayload(response.payloadText);
      promptDetectionObservation = BrowserAgentObservationService.fromPerception(perception);
    }
    if (currentTab) {
      promptDetectionObservation.currentTitle = currentTab.title;
      promptDetectionObservation.currentUrl = currentTab.url;
    }
    const promptDetectionText = BrowserAgentPromptDetectionService.build('', promptDetectionObservation);
    const checkPrompt = this.events.checkPrompt;
    const promptCheckPromise = checkPrompt ?
      BrowserAgentUtils.base64ToPixelMaps(request.images).then(pixelMaps =>
        checkPrompt(pixelMaps, promptDetectionText)
      ).catch((error: Object): Error => {
        const err = error instanceof Error ? error : new Error(String(error));
        hilog.error(DOMAIN, TAG, 'decideStartTab checkPrompt failed: %{private}s', err.message);
        return err;
      }) :
      Promise.resolve<DetectionResult | Error | null>(null);

    try {
      this.callLLMTime = 0;
      const firstResult: DecideStartPointResponse | Error =
        await this.completeDecideStartTab(request, llm, 'decideStartTab')
          .catch((error: Object): Error => error instanceof Error ? error : new Error(String(error)));
      const promptCheckResult = await promptCheckPromise;
      let result: DecideStartPointResponse;
      if (promptCheckResult && !(promptCheckResult instanceof Error) &&
        promptCheckResult.riskCode === AgentConstants.INJECTED) {
        this.events.onChunk('起始标签页检测到网页提示注入风险，忽略网页信息重新推理', false);
        const retryRequest = this.buildDecideStartTabRequest(runtime.task, null, null, false, false);
        result = await this.completeDecideStartTab(retryRequest, llm, 'decideStartTabWithoutPageInfo');
      } else if (firstResult instanceof Error) {
        throw firstResult;
      } else {
        result = firstResult;
      }

      result.url = this.normalizeStartUrl(result.url);
      const isValidUrl = isNavigationUrlAllowed(result.url);
      if (!isValidUrl) {
        result.decision = true;
        result.url = '';
        return result;
      }
      // 有问题，如果不用截图，currentTab就不会被ask_perception错误拦截，会是非空的，但是没法加url校验不然会触发敏感词拦截0
      if (result.url.length > 0 && response === null && currentTab === null) {
        result.decision = true; //若应该在某url开始，但当前是空页面：直接在当前页面跳转
        return result;
      }
      if (!result.decision) { // 不使用当前tab，此时可能使用url开新tab，但是有可能以前已经有任务了，此时如果之前任务的current是可用的就不再url了。
        const should = await this.shouldNavigate(result.url, env);
        if (!should) {
          result.url = '';
        }
      }
      return result;
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parse decideStartTab failed: %{private}s', e.message);
      return {reason: '', decision: true, url: ''};
    }
  }

  /**
   * 归一化模型返回的起始页 URL。
   * 模型偶发返回裸域名（www.example.com），会被 isNavigationUrlAllowed 判为非法
   * 而清空，导致起始页跳转失效。此处仅对不含任何 scheme 的输入补 https://，
   * 已带 scheme 的（含 javascript: 等）原样透传，仍由白名单校验拦截。
   * 注意：本函数不做安全判定，补全后的 URL 是否允许导航完全由
   * isNavigationUrlAllowed 决定，本改动不放宽其既有策略。
   */
  private normalizeStartUrl(url: string): string {
    const trimmed = url.trim();
    if (trimmed.length === 0) {
      return '';
    }
    // 仅到冒号，不含 '//'：javascript: 等伪协议会被识别为已带 scheme 而原样透传，
    // 避免补全后伪装成合法 https 链接绕过白名单
    if (/^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(trimmed)) {
      return trimmed;
    }
    return `https://${trimmed}`;
  }

  private buildDecideStartTabRequest(task: string, currentTab: AgentTabInfo | null,
    response: BrowserTransportResponse | null, useScreenshot: boolean,
    includePageInfo: boolean): AgentLLMRequest {
    const request = new AgentLLMRequest();
    if (includePageInfo) {
      request.systemPrompt =
        `You are a browser agent. The user instructed a task while he/she was on a webpage (the CURRENT page). You should decide on what page (START page/website) the task should start.
        Your input includes the ${useScreenshot ? 'url, title, and screenshot' : 'url and title'} of the current webpage.
        Output a json with "reason", "decision", and "url" field.
        In reason, briefly think whether the task should start on the current webpage.
        In decision, output a bool, true meaning start from current, false meaning the task is irrelevant to the current webpage and should start in a brand new tab.
        In url, output the full url including the scheme (e.g. https://www.example.com) of the start website if the decision is false, or an empty string otherwise.
        If the current tab is an empty tab or unknown, you should always output decision as false.
        If you really do not know the url of the start page/website, output a search engine website that can be accessed in China (use this carefully).
        Do not output fences.

        Output format:
        {"reason": "<string, one or two short sentences>", "decision": <true or false, no quotes>, "url": "<string, full url with scheme or empty string>"}`;
    } else {
      request.systemPrompt =
        `You are a browser agent. Determine the START page/website for the user's task using only the user instruction.
        Output a json with "reason", "decision", and "url" field.
        In reason, briefly explain which start page/website is appropriate for the task.
        In decision, output false because no current webpage information is available and the task should start in a brand new tab.
        In url, output the full url including the scheme (e.g. https://www.example.com) of the start website.
        If you really do not know the url of the start page/website, output a search engine website that can be accessed in China (use this carefully).
        Do not output fences.

        Output format:
        {"reason": "<string, one or two short sentences>", "decision": <always false, no quotes>, "url": "<string, full url with scheme>"}`;
    }

    request.userPrompt = `User instruction: ${task}`;
    if (!includePageInfo) {
      request.scene = AgentLLMScenes.BROWSER_DECISION;
      return request;
    }
    if (currentTab) {
      request.userPrompt += `
      The title of the current page: ${currentTab.title}
      The url of the current page: ${currentTab.url}`;
    } else {
      request.userPrompt += '\nThe title and url of the current page are unavailable.';
    }
    if (useScreenshot && response) {
      const perception = BrowserTransportProtocolService.parsePerceptionPayload(response.payloadText);
      request.userPrompt += `
      The screenshot of the current page: ${BrowserPromptConstants.IMAGE_PLACEHOLDER}`;
      request.images = [perception.originalScreenshot];
    } else if (useScreenshot) {
      request.userPrompt += `
      The screenshot: The current webpage's screenshot is unavailable.`;
    }
    request.scene = useScreenshot ? AgentLLMScenes.BROWSER_DECISION_VISION : AgentLLMScenes.BROWSER_DECISION;
    return request;
  }

  private async completeDecideStartTab(request: AgentLLMRequest, llm: AgentLLMPort,
    logTag: string): Promise<DecideStartPointResponse> {
    hilog.debug(DOMAIN, TAG, '%{public}s request = %{private}s', logTag, JSON.stringify(request));
    const startTime = Date.now();
    try {
      const llmResponse = await llm.complete(request, logTag);
      hilog.debug(DOMAIN, TAG, '%{public}s response = %{private}s', logTag, llmResponse.text);
      return JSON.parse(llmResponse.text) as DecideStartPointResponse;
    } finally {
      this.callLLMTime += Date.now() - startTime;
    }
  }

  async shouldNavigate(targetUrl: string, env: AgentEnvironmentPort): Promise<boolean> {
    try {
      const observation = await env.perceiveTabs(false, true);
      hilog.debug(DOMAIN, TAG, 'current url in session: %{private}s', observation.currentUrl);
      if (BrowserAgentUtils.shouldNavigate(observation.currentUrl, targetUrl)) {
        return true;
      }
      return false;
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'perceive tab within task group failed: %{private}s', e.message);
      return true;
    }
  }

  async sendAgentControl(runtime: BrowserRuntimeState, useCurrentTab: DecideStartPointResponse) {
    const agentControlParams: Record<string, Object> = {};
    agentControlParams.taskTitle = this.groupName as Object;
    agentControlParams.useCurrentTab = useCurrentTab.decision as Object;
    agentControlParams.url = useCurrentTab.url as Object;
    let response: BrowserTransportResponse;
    try {
      response = await BrowserTransportHostService.dispatchBridgeAction(
        runtime.browserId,
        'AGENT_CONTROL',
        agentControlParams,
        runtime.sessionKey
      );
    } catch (agentControlError) {
      const errorType = BrowserAgentUtils.resolveErrorType(agentControlError as Object).errorType;
      if (errorType !== AgentConstants.ACTION_BUSY_MESSAGE) {
        throw new Error(BrowserAgentUtils.stringifyError(agentControlError as Object));
      }
      // 扩展侧执行槽仍被占用（可能残留上一个任务的动作）。补发 FINISH 复位到 idle，
      // 成功后重试一次 AGENT_CONTROL，避免任务启动即失败。
      this.events.onChunk('浏览器扩展仍在执行上一个动作，先复位再重试 AGENT_CONTROL', false);
      try {
        await BrowserTransportHostService.dispatchBridgeAction(
          runtime.browserId,
          'FINISH',
          JSON.parse('{}') as Record<string, Object>,
          runtime.sessionKey
        );
      } catch (finishResetError) {
        // 正常情况下扩展侧在动作超时后会释放动作槽
        // 此处FINISH仍超时，说明扩展侧可能彻底陷入异常，不再尝试恢复
        hilog.warn(DOMAIN, TAG, 'AGENT_CONTROL busy: finish reset failed: %{public}s',
          BrowserAgentUtils.stringifyError(finishResetError as Object));
        throw new Error(BrowserAgentUtils.stringifyError(agentControlError as Object)); // FINISH 复位失败，保持原始 BUSY 错误
      }
      response = await BrowserTransportHostService.dispatchBridgeAction(
        runtime.browserId,
        'AGENT_CONTROL',
        agentControlParams,
        runtime.sessionKey
      );
    }
    hilog.debug(DOMAIN, TAG, 'agent control response = %{private}s', JSON.stringify(response));
    try {
      const temp = JSON.parse(response.payloadText) as TimePayload;
      if (temp.lastCreateTabTs) {
        hilog.info(DOMAIN, TAG, 'First tab opened at: %{public}d', temp.lastCreateTabTs);
        hilog.info(DOMAIN, TAG, 'First tab open time (without llm call and tab load): %{public}d', temp.lastCreateTabTs - runtime.createdAt - this.callLLMTime);
        hilog.info(DOMAIN, TAG, 'First tab open time (including tab load): %{public}d', Date.now() - runtime.createdAt - this.callLLMTime);
        hilog.info(DOMAIN, TAG, 'First tab open time (including llm call and tab load): %{public}d', Date.now() - runtime.createdAt);
      }
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'time count failed.');
    }
  }

  getRuntime(): BrowserRuntimeState {
    return this.runtimeAccessor.getRuntime();
  }

  getFinalAgentState(): AgentState | null {
    return this.finalAgentState;
  }
}
