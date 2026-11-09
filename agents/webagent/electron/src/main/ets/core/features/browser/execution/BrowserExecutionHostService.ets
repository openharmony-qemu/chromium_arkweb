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

import { hilog } from '@kit.PerformanceAnalysisKit';
import {
  BrowserAgentConfig,
  BrowserRuntimeState,
  BrowserTransportResponse,
  BrowserTransportStatus,
  BridgeAction,
  BrowserTransportConstants
} from '../../../shared/domain/BrowserAutomationTypes';
import { BrowserDelayConfigService } from '../BrowserDelayConfigService';
import { BrowserAgentRunner } from '../agent/runner/Runner';
import { BrowserExecutionLifecycleService } from './BrowserExecutionLifecycleService';
import { BrowserExecutionMemoryService } from './BrowserExecutionMemoryService';
import { BrowserExecutionRequest } from './BrowserExecutionTypes';
import { BrowserRuntimeService } from './BrowserRuntimeService';
import { BrowserSessionRecorderService } from './BrowserSessionRecorderService';
import { BrowserTransportHostService } from '../transport/BrowserTransportHostService';
import { BrowserSessionWorkspaceService } from './BrowserSessionWorkspaceService';
import { BrowserAutomationFacadeService } from '../host/BrowserAutomationFacadeService';
import {emitter, Callback} from '@kit.BasicServicesKit';
import BuildProfile from 'BuildProfile';
import { JSON } from '@kit.ArkTS';
import { BrowserAgentEventCallbacks } from '../agent/ports/AgentContext';
import { MemoryClient } from '../agent/memory/MemoryClient';
import { AgentLLMPort } from '../agent/ports/AgentLLMPort';
import { ExternalLLMAdapter } from '../agent/adapters/ExternalLLMAdapter';
import { AgentBuildConfig } from '../agent/core/AgentBuildConfig';

const DOMAIN = 0x6020;
const TAG = 'BrowserExecutionHost';
// 扩展忙碌时 USER_CONTROL 会等待原始动作真正退出，需长于普通 action 的本地等待窗口。
const USER_CONTROL_HANDOFF_TIMEOUT_MS = 180000;

export class BrowserExecutionHostService {
  private static stopFlag: boolean = false;
  private static currentRunner: BrowserAgentRunner | null = null;

  static requestStop(): void {
    BrowserExecutionHostService.stopFlag = true;
    BrowserExecutionHostService.currentRunner?.requestStop();
  }

  static async run(request: BrowserExecutionRequest): Promise<BrowserRuntimeState> {
    AgentBuildConfig.configure(BuildProfile.DEBUG);
    BrowserExecutionHostService.stopFlag = false;

    let runtime = BrowserRuntimeService.createRuntime(
      request.task,
      new BrowserAgentConfig(),
      request.sessionKey,
      request.sessionDir,
      request.needClearBrowser,
      request.calledAt
    );
    runtime.cacheScreenshot = request.cacheScreenshot;
    runtime.screenshotCacheDir = BrowserSessionWorkspaceService.getScreenshotCacheDir();
    if (request.cacheScreenshot) {
      BrowserSessionWorkspaceService.clearDir(runtime.screenshotCacheDir);
    }
    const events: BrowserAgentEventCallbacks | null = request.events;
    if (!events) {
      return BrowserExecutionLifecycleService.markFailed(runtime, 'callback events not provided');
    }
    const llmCallback = events.onLLMComplete;
    if (!llmCallback) {
      events.onTaskFailed?.('INTERNAL_ERROR', 'Did not receive external LLM provided.');
      return BrowserExecutionLifecycleService.markFailed(runtime, 'Did not receive external LLM provided.');
    }
    const emitChunk = (chunk: string, isFinished: boolean): void => {
      events.onChunk?.(chunk, isFinished);
      if (runtime.sessionDir.length > 0 && BuildProfile.DEBUG) {
        BrowserSessionRecorderService.recordChunk(runtime.sessionDir, chunk, isFinished);
      }
    };
    try {
      const transportReady = await BrowserTransportHostService.ensureReady(10000);
      if (!transportReady) {
        const startupError = BrowserTransportHostService.getLastStartupErrorMessage() ||
          'transport layer failed to start';
        events.onTaskFailed?.('INTERNAL_ERROR', startupError);
        return BrowserExecutionLifecycleService.markFailed(runtime, startupError);
      }

      const connected = await BrowserTransportHostService.waitForConnection(30000);
      if (!connected) {
        events.onTaskFailed?.('BROWSER_CONNECTION_FAILED', 'browser extension connection timeout');
        return BrowserExecutionLifecycleService.markFailed(runtime, 'browser extension connection timeout');
      }

      runtime = BrowserExecutionLifecycleService.markLaunchReady(runtime);
      runtime = BrowserRuntimeService.updateRuntime(runtime);
      emitChunk('浏览器已就绪，开始执行任务...', false);
      hilog.info(DOMAIN, TAG, '浏览器已就绪，开始执行任务...');


      if (BrowserExecutionHostService.stopFlag) {
        runtime.stopRequested = true;
        runtime = BrowserRuntimeService.updateRuntime(runtime);
      }

      BrowserExecutionHostService.subscribeEvent(runtime.browserId, runtime.sessionKey, events.onStop, request.cacheScreenshot? runtime.screenshotCacheDir:'');
      events.onChunk('Registered event listeners', false);
      hilog.debug(DOMAIN, TAG, 'Registered event listeners');
      hilog.debug(DOMAIN, TAG, 'hasRequestedTaskStop: %{public}s', AppStorage.get<boolean>(BrowserTransportConstants.HAS_REQUESTED_TASK_STOP));
      // 如果在事件监听注册之前，就被请求stop的话，会收不到事件。在这里用状态确认
      if (AppStorage.get<boolean>(BrowserTransportConstants.HAS_REQUESTED_TASK_STOP)) {
        BrowserExecutionHostService.emitEvent(BrowserTransportConstants.FINISH_EVENT);
        AppStorage.set<boolean>(BrowserTransportConstants.HAS_REQUESTED_TASK_STOP, false);
        return BrowserExecutionLifecycleService.markFailed(runtime, '用户主动停止任务');
      }

      // Runner and all memory paths share this task-scoped LLM dependency.
      const taskLlm: AgentLLMPort = new ExternalLLMAdapter(
        llmCallback, runtime.screenshotCacheDir, () => BrowserExecutionHostService.stopFlag, runtime.sessionDir);
      MemoryClient.configureLLM(taskLlm);
      runtime = await BrowserExecutionMemoryService.prepareMemory(runtime, (line: string): void => {
        emitChunk(line + '\n', false);
      });
      taskLlm.getUsage();
      hilog.info(DOMAIN, TAG, 'before runner: proceduralMemory.length=%{public}d', runtime.proceduralMemory.length);

      const runner = new BrowserAgentRunner(runtime, request.agentType, taskLlm, events, request.groupName);
      BrowserExecutionHostService.currentRunner = runner;
      let nextRuntime: BrowserRuntimeState;
      try {
        nextRuntime = await runner.run();
      } catch (e) {
        BrowserExecutionHostService.currentRunner = null;
        if (BrowserExecutionHostService.stopFlag) {
          return BrowserExecutionLifecycleService.markFailed(runtime, '用户主动停止任务');
        }
        throw new Error(e.message);
      } finally {
        BrowserExecutionHostService.currentRunner = null;
      }

      if (request.cacheScreenshot) {
        BrowserSessionWorkspaceService.clearDir(runtime.screenshotCacheDir);
      }
      BrowserExecutionHostService.unsubscribeEvent();

      hilog.debug(DOMAIN, TAG, 'runtime result = %{private}s', JSON.stringify(nextRuntime));
      if (nextRuntime.phase === 'FINISHED') {
        const finishSummary = nextRuntime.lastSummary.length > 0 ? nextRuntime.lastSummary : 'browser task completed';
        emitChunk('finishSummary: ' + finishSummary, true);
      } else {
        emitChunk('finishSummary: WebAgent 任务过程中由于错误中断。 其运行结果为: \n' + nextRuntime.lastSummary + '\n其错误信息为：' + nextRuntime.errorMessage, true);
      }

      runtime = nextRuntime;
      if (nextRuntime.phase === 'FAILED') { //之前undefined is not callable 问题就是因为errorMessage在某次赋值变成了object类型！结果includes报错了。已修复
        // 失败/中断任务的 session 记忆记录（受 vConfig.sessionMemoryRecordUnfinished 开关控制；
        // 开关默认关时该调用为 no-op，成功路径行为完全不受影响）。
        await BrowserExecutionMemoryService.persistSessionMemory(
          nextRuntime,
          runner.getFinalAgentState(),
          (line: string): void => { emitChunk(line + '\n', false); }
        );
        if (nextRuntime.errorMessage.includes('remote websocket close') || nextRuntime.errorMessage.includes('browser is shutting down')) {
          events.onTaskFailed?.('BROWSER_CRASHED', nextRuntime.errorMessage);
        } else if (nextRuntime.errorMessage.includes('CONTROL_STATE_IDLE') || nextRuntime.stopRequested) {
          events.onStop?.();
        } else {
          events.onTaskFailed?.('INTERNAL_ERROR', nextRuntime.errorMessage);
        }
        const errorMessage = nextRuntime.errorMessage.length > 0 ? nextRuntime.errorMessage : 'browser agent graph failed';
        return BrowserExecutionLifecycleService.markFailed(runtime, errorMessage);
      }
      if (nextRuntime.disconnected) { //这个目前应该没有用，agent里没更新这个，如果断连是会phase failed被上面捕获
        const errorMessage = nextRuntime.errorMessage.length > 0 ? nextRuntime.errorMessage : 'browser agent graph failed';
        return BrowserExecutionLifecycleService.markFailed(runtime, errorMessage);
      }

      // 记忆持久化：程序记忆写入 + session 记忆存储
      const persistLine = await BrowserExecutionMemoryService.persistProceduralMemory(
        runner.getFinalAgentState(), nextRuntime, taskLlm
      );
      if (persistLine !== null && persistLine.length > 0) {
        emitChunk(persistLine + '\n', false);
      }
      await BrowserExecutionMemoryService.persistSessionMemory(
        nextRuntime,
        runner.getFinalAgentState(),
        (line: string): void => {
          emitChunk(line + '\n', false);
      });

      return nextRuntime;
    } catch (error) {
      try {
        const response = await BrowserTransportHostService.dispatchBridgeAction(
          runtime.browserId,
          BridgeAction.FINISH,
          JSON.parse('{}') as Record<string, Object>,
          runtime.sessionKey
        );
        BrowserExecutionHostService.logDispatchFailure(BridgeAction.FINISH, response);
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'the transport has already closed');
      }
      events.onTaskFailed?.('INTERNAL_ERROR', `${error}`);
      const errorMessage = `${error.message}`;
      hilog.error(DOMAIN, TAG, 'browser execution failed: %{private}s', errorMessage);
      if (request.cacheScreenshot) {
        BrowserSessionWorkspaceService.clearDir(runtime.screenshotCacheDir);
      }
      runtime = BrowserExecutionLifecycleService.markFailed(runtime, errorMessage);
      runtime = BrowserRuntimeService.updateRuntime(runtime);
      BrowserExecutionHostService.unsubscribeEvent();
      emitChunk(errorMessage, true);
      return runtime;
    }
  }

  private static subscribeEvent(browserId: number, taskId: string, onStop: (() => void) | undefined, cacheDir: string) {
    let event: emitter.InnerEvent = {
      eventId: BrowserTransportConstants.USER_CONTROL_EVENT
    };
    let callback: Callback<emitter.EventData> = async (eventData: emitter.EventData): Promise<void> => {
      try {
        const response = await BrowserTransportHostService.dispatchBridgeAction(
          browserId,
          BridgeAction.USER_CONTROL,
          JSON.parse('{}') as Record<string, Object>,
          '',
          USER_CONTROL_HANDOFF_TIMEOUT_MS
        );
        BrowserExecutionHostService.logDispatchFailure(BridgeAction.USER_CONTROL, response);
      } catch (e) {
        hilog.error(DOMAIN, TAG, 'send user_control to browser failed: %{private}s', e.message);
      }
    }
    emitter.on(event, callback);

    event = {
      eventId: BrowserTransportConstants.FINISH_EVENT
    };
    callback = async (eventData: emitter.EventData): Promise<void> => {
      hilog.info(DOMAIN, TAG, 'received stop event');
      try { //即使插件返回了消息，如果response.status !== SUCCESS，好像也会被dispatchBridgeAction抛出错误
        const response = await BrowserTransportHostService.dispatchBridgeAction(
          browserId,
          BridgeAction.FINISH,
          JSON.parse('{}') as Record<string, Object>,
          taskId
        );
        BrowserExecutionHostService.logDispatchFailure(BridgeAction.FINISH, response);
      } catch (e) {
        hilog.error(DOMAIN, TAG, 'send finish to browser failed: %{private}s', e.message);
      }
      if (cacheDir.length) {
        BrowserSessionWorkspaceService.clearDir(cacheDir);
      }
      BrowserAutomationFacadeService.shutdown();
      onStop?.();
    }
    emitter.on(event, callback);

    // 插件主动断开 WebSocket 时，BrowserTransportHostService 的 disconnectListener 会 emit 此事件。
    // 不再向插件 dispatch 任何 bridge action（连接已断），直接触发 shutdown 链停止 agent task。
    event = {
      eventId: BrowserTransportConstants.PLUGIN_DISCONNECTED_EVENT
    };
    callback = async (eventData: emitter.EventData): Promise<void> => {
      hilog.warn(DOMAIN, TAG, 'plugin websocket disconnected, stopping agent task');
      if (cacheDir.length) {
        BrowserSessionWorkspaceService.clearDir(cacheDir);
      }
      BrowserAutomationFacadeService.shutdown();
      onStop?.();
    }
    emitter.on(event, callback);
  }

  private static logDispatchFailure(action: string, response: BrowserTransportResponse): void {
    if (response.status === BrowserTransportStatus.SUCCESS) {
      return;
    }
    hilog.error(
      DOMAIN,
      TAG,
      'browser bridge action dispatch failed: action=%{private}s status=%{private}s error=%{private}s',
      action,
      response.status,
      response.errorMessage
    );
  }

  private static unsubscribeEvent() {
    emitter.off(BrowserTransportConstants.USER_CONTROL_EVENT);
    emitter.off(BrowserTransportConstants.FINISH_EVENT);
    emitter.off(BrowserTransportConstants.PLUGIN_DISCONNECTED_EVENT);
  }

  private static emitEvent(id: number) {
    let event: emitter.InnerEvent = {
      eventId: id,
      priority: emitter.EventPriority.HIGH
    };
    let eventData: emitter.EventData = {
      data: {
      }
    }
    emitter.emit(event, eventData);
  }
}
