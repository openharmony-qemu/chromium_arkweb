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
  BrowserActionCommand,
  BrowserRuntimeState,
  BrowserTransportMessageType,
  BrowserTransportRequest,
  Point
} from '../../../shared/domain/BrowserAutomationTypes';
import { BrowserExecutionOrchestratorService } from '../execution/BrowserExecutionOrchestratorService';

export class BrowserTransportCommandUpdate {
  public runtime: BrowserRuntimeState = new BrowserRuntimeState();
  public request: BrowserTransportRequest = new BrowserTransportRequest();
}

/**
 * EXECUTE_ACTION 发给扩展侧的业务 payload。
 *
 * 包含动作文本、Agent 解释和目标 tab 信息。
 */
class BrowserActionPayload {
  public action: string = '';
  public reason: string = '';
  public agentReason: string = '';
  public targetTabId: number = 0;
  public skipSensitiveActionCheck: boolean = false;
  public point: Point | undefined = undefined;
  // agent 侧已解析的目标元素（number=elementId / string=selector / null=回退文本），供 transport 直接用
  public element: number | string | null = null;
}

/**
 * 浏览器执行流到 transport request 的命令构造器。
 *
 * 负责把 perceive/action/wait loading 等运行态动作转换成 pending request，并推进 runtime 状态机。
 */
export class BrowserTransportCommandService {
  private static seed: number = 0;

  static beginPerception(runtime: BrowserRuntimeState): BrowserTransportCommandUpdate {
    const request = BrowserTransportCommandService.createRequest(
      runtime.browserId,
      BrowserTransportMessageType.ASK_PERCEPTION,
      runtime.task,
      runtime.sessionKey
    );
    const nextRuntime = BrowserExecutionOrchestratorService.beginObserving(runtime, request.reqId);
    const update = new BrowserTransportCommandUpdate();
    update.runtime = nextRuntime;
    update.request = request;
    return update;
  }

  static beginAction(runtime: BrowserRuntimeState, command: BrowserActionCommand): BrowserTransportCommandUpdate {
    const payload = new BrowserActionPayload();
    payload.action = command.action;
    payload.reason = 'browser agent graph';
    payload.agentReason = command.reason;
    payload.targetTabId = command.targetTabId;
    payload.skipSensitiveActionCheck = command.skipSensitiveActionCheck;
    payload.point = command.point;
    payload.element = command.element;
    const request = BrowserTransportCommandService.createRequest(
      runtime.browserId,
      BrowserTransportMessageType.EXECUTE_ACTION,
      JSON.stringify(payload),
      runtime.sessionKey
    );
    const nextRuntime = BrowserExecutionOrchestratorService.beginWaiting(runtime, command, request.reqId);
    const update = new BrowserTransportCommandUpdate();
    update.runtime = nextRuntime;
    update.request = request;
    return update;
  }

  static beginWaitLoading(
    runtime: BrowserRuntimeState, command: BrowserActionCommand | null): BrowserTransportCommandUpdate {
    const request = BrowserTransportCommandService.createRequest(
      runtime.browserId,
      BrowserTransportMessageType.WAIT_LOADING,
      runtime.currentPerception ? runtime.currentPerception.currentUrl : '',
      runtime.sessionKey
    );
    const nextRuntime = BrowserExecutionOrchestratorService.beginWaiting(runtime, command, request.reqId);
    const update = new BrowserTransportCommandUpdate();
    update.runtime = nextRuntime;
    update.request = request;
    return update;
  }

  private static createRequest(browserId: number, messageType: string, payloadText: string,
    taskId: string): BrowserTransportRequest {
    BrowserTransportCommandService.seed++;
    const request = new BrowserTransportRequest();
    request.browserId = browserId;
    request.messageType = messageType;
    request.payloadText = payloadText;
    request.reqId = `ts_browser_req_${Date.now()}_${BrowserTransportCommandService.seed}`;
    request.msgId = `browser_msg_${Date.now()}_${BrowserTransportCommandService.seed}`;
    request.taskId = taskId;
    request.timestamp = Date.now();
    request.timeoutMs = messageType === BrowserTransportMessageType.EXECUTE_ACTION ? 20000 : 30000;
    return request;
  }
}
