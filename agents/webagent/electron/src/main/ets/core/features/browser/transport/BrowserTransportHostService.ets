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

import fs from '@ohos.file.fs';
import { emitter } from '@kit.BasicServicesKit';
import BuildProfile from 'BuildProfile';
import {
  BridgeAction, BridgeResponse, BridgeResponseType, BridgeStatus,
  BrowserTransportConstants, BrowserTransportMessageType, BrowserTransportRequest,
  BrowserTransportResponse, BrowserTransportStatus, Point
} from '../../../shared/domain/BrowserAutomationTypes';
import { BrowserTransportErrorType } from '../../../shared/domain/BrowserTransportErrors';
import { BrowserActionService } from '../BrowserActionService';
import { vConfig } from '../agent/VersionConfig';
import { BrowserTransportServerService } from './BrowserTransportServerService';

interface PendingRequest {
  resolve: (response: BrowserTransportResponse) => void;
  reject: (error: Error) => void;
  timeout: number;
  request: BrowserTransportRequest;
  action: string;
}

/** Request/response correlation over the sample WebSocket JSON channel. */
export class BrowserTransportHostService {
  private static seed: number = 0;
  private static pending: Map<string, PendingRequest> = new Map<string, PendingRequest>();
  private static dialogTargetByTaskId: Map<string, number> = new Map<string, number>();
  private static lastStartupErrorMessage: string = '';

  static isAvailable(): boolean {
    return BuildProfile.DEBUG;
  }

  static getWebSocketPort(): number {
    return BrowserTransportServerService.getListeningPort();
  }

  static getWebSocketUrl(): string {
    return BrowserTransportServerService.getWebSocketUrl();
  }

  static getLastStartupErrorMessage(): string {
    return this.lastStartupErrorMessage;
  }

  static async ensureReady(timeoutMs: number): Promise<boolean> {
    void timeoutMs;
    BrowserTransportServerService.setResponseListener((response: BridgeResponse): void => {
      this.consumeResponse(response);
    });
    BrowserTransportServerService.setDisconnectListener((browserId: number, reason: string): void => {
      this.rejectAll(this.transportError(BrowserTransportErrorType.DISCONNECTED, reason));
      setTimeout((): void => {
        emitter.emit(
          { eventId: BrowserTransportConstants.PLUGIN_DISCONNECTED_EVENT, priority: emitter.EventPriority.HIGH },
          { data: { browserId: browserId, reason: reason } }
        );
      }, 0);
    });
    const started = await BrowserTransportServerService.ensureStarted();
    this.lastStartupErrorMessage = started ? '' :
      'Sample WebSocket channel is available only in debug mode on loopback port 18900';
    return started;
  }

  static waitForConnection(timeoutMs: number): Promise<boolean> {
    return new Promise<boolean>((resolve) => {
      const deadline = Date.now() + timeoutMs;
      const poll = (): void => {
        if (this.isConnected()) {
          resolve(true);
        } else if (Date.now() >= deadline) {
          resolve(false);
        } else {
          setTimeout(poll, BrowserTransportConstants.CONNECTION_POLL_INTERVAL_MS);
        }
      };
      poll();
    });
  }

  static isConnected(): boolean {
    return BrowserTransportServerService.hasActiveConnection();
  }

  static resetTaskState(): void {
    this.rejectAll(new Error('browser task stopped'));
    this.dialogTargetByTaskId.clear();
  }

  static shutdown(): Promise<void> {
    this.resetTaskState();
    this.lastStartupErrorMessage = '';
    return BrowserTransportServerService.shutdown();
  }

  static dispatchAndWait(request: BrowserTransportRequest): Promise<BrowserTransportResponse> {
    const envelope = this.translateRequest(request);
    return this.sendAndWait(request, envelope.action, envelope.params, envelope.reason);
  }

  static dispatchBridgeAction(browserId: number, action: string, params: Record<string, Object>,
    taskId: string = '', timeoutMs: number = BrowserTransportConstants.DEFAULT_ACTION_TIMEOUT_MS): Promise<BrowserTransportResponse> {
    const request = new BrowserTransportRequest();
    request.browserId = browserId;
    request.messageType = BrowserTransportMessageType.ACTION_RESULT;
    request.reqId = this.nextRequestId();
    request.msgId = `browser_msg_${Date.now()}_${this.seed}`;
    request.taskId = taskId;
    request.timeoutMs = timeoutMs;
    request.timestamp = Date.now();
    return this.sendAndWait(request, action, params, '');
  }

  private static sendAndWait(request: BrowserTransportRequest, action: string,
    params: Record<string, Object>, reason: string): Promise<BrowserTransportResponse> {
    if (request.taskId.length > 0 && this.dialogTargetByTaskId.has(request.taskId) &&
      action !== BridgeAction.ASK_PERCEPTION && action !== BridgeAction.DIALOG) {
      return Promise.reject(this.transportError(BridgeResponseType.DIALOG_OPENING,
        'browser dialog is waiting; refresh perception before continuing'));
    }
    this.validateUploadFiles(action, params);
    if (!this.isConnected()) {
      return Promise.reject(this.transportError(BrowserTransportErrorType.DISCONNECTED, 'browser not connected'));
    }
    return new Promise<BrowserTransportResponse>((resolve, reject) => {
      const timeout = setTimeout((): void => {
        this.rejectPending(request.reqId, new Error(`${request.messageType} wait exceeded local timeout window`));
      }, request.timeoutMs + BrowserTransportConstants.LOCAL_TIMEOUT_BUFFER_MS);
      this.pending.set(request.reqId, { resolve, reject, timeout, request, action });
      void BrowserTransportServerService.sendActionRequest(action, params, request.reqId, request.taskId, reason)
        .then((sent: boolean): void => {
          if (!sent) {
            this.rejectPending(request.reqId,
              this.transportError(BrowserTransportErrorType.SEND_FAILED, 'browser transport send failed'));
          }
        }).catch((error: Error): void => {
          this.rejectPending(request.reqId,
            this.transportError(BrowserTransportErrorType.SEND_FAILED, String(error)));
        });
    });
  }

  private static consumeResponse(response: BridgeResponse): void {
    if (response.type === BridgeResponseType.DIALOG_OPENING) {
      const tabId = response.payload.tabId;
      if (response.taskId.length > 0 && typeof tabId === 'number' && tabId > 0) {
        this.dialogTargetByTaskId.set(response.taskId, tabId);
        this.pending.forEach((pending: PendingRequest, reqId: string): void => {
          if (pending.request.taskId === response.taskId &&
            pending.action !== BridgeAction.ASK_PERCEPTION && pending.action !== BridgeAction.DIALOG) {
            this.rejectPending(reqId, this.transportError(BridgeResponseType.DIALOG_OPENING,
              'browser dialog opened; refresh perception before continuing'));
          }
        });
      }
      return;
    }
    const pending = this.pending.get(response.replyToId);
    if (!pending) {
      return;
    }
    clearTimeout(pending.timeout);
    this.pending.delete(response.replyToId);
    if (response.status !== BridgeStatus.SUCCESS) {
      pending.reject(new Error(JSON.stringify({
        errorType: response.errorType, errorMessage: response.errorMessage
      })));
      return;
    }
    if (pending.action === BridgeAction.DIALOG) {
      this.dialogTargetByTaskId.delete(pending.request.taskId);
    } else if (pending.action === BridgeAction.ASK_PERCEPTION) {
      const pageInfo = response.payload.pageInfo;
      const hasOpenDialog = response.payload.hasOpenDialog === true ||
        (typeof pageInfo === 'object' && pageInfo !== null &&
          (pageInfo as Record<string, Object>).hasOpenDialog === true);
      if (!hasOpenDialog) {
        this.dialogTargetByTaskId.delete(pending.request.taskId);
      }
    }
    const result = new BrowserTransportResponse();
    result.messageType = pending.request.messageType;
    result.msgId = pending.request.msgId;
    result.browserId = pending.request.browserId;
    result.replyToId = response.replyToId;
    result.status = BrowserTransportStatus.SUCCESS;
    result.payloadText = response.payloadText;
    result.timestamp = Date.now();
    pending.resolve(result);
  }

  private static rejectPending(reqId: string, error: Error): void {
    const pending = this.pending.get(reqId);
    if (!pending) {
      return;
    }
    clearTimeout(pending.timeout);
    this.pending.delete(reqId);
    pending.reject(error);
  }

  private static rejectAll(error: Error): void {
    const ids = Array.from(this.pending.keys());
    for (const id of ids) {
      this.rejectPending(id, error);
    }
  }

  private static translateRequest(request: BrowserTransportRequest): { action: string,
    params: Record<string, Object>, reason: string } {
    if (request.messageType === BrowserTransportMessageType.ASK_PERCEPTION) {
      const params: Record<string, Object> = {};
      if (vConfig.text_only) {
        params.textOnly = true as Object;
      }
      const targetId = this.dialogTargetByTaskId.get(request.taskId);
      if (targetId && targetId > 0) {
        params.targetId = targetId as Object;
      }
      return { action: BridgeAction.ASK_PERCEPTION, params, reason: '' };
    }
    if (request.messageType === BrowserTransportMessageType.WAIT_LOADING) {
      return { action: BridgeAction.WAIT, params: {}, reason: '' };
    }
    if (request.messageType === BrowserTransportMessageType.EXECUTE_ACTION) {
      const payload = JSON.parse(request.payloadText) as Record<string, Object>;
      const text = typeof payload.action === 'string' ? payload.action as string : '';
      const command = BrowserActionService.parseAgentAction(text);
      command.targetTabId = typeof payload.targetTabId === 'number' ? payload.targetTabId as number : 0;
      command.element = typeof payload.element === 'number' || typeof payload.element === 'string'
        ? payload.element : null;
      const pointRaw = payload.point;
      if (typeof pointRaw === 'object' && pointRaw !== null) {
        const coordinates = pointRaw as Record<string, Object>;
        const point = new Point();
        point.x = typeof coordinates.x === 'number' ? coordinates.x as number : 0;
        point.y = typeof coordinates.y === 'number' ? coordinates.y as number : 0;
        command.point = point;
      }
      const bridgeAction = BrowserActionService.toBridgeAction(command);
      if (!bridgeAction) {
        return { action: BridgeAction.WAIT, params: {}, reason: '' };
      }
      bridgeAction.params.enableSafetyCheck = (payload.skipSensitiveActionCheck === true ? 0 : 1) as Object;
      const reason = typeof payload.agentReason === 'string' ? payload.agentReason as string : '';
      return { action: bridgeAction.action, params: bridgeAction.params, reason };
    }
    return { action: BridgeAction.WAIT, params: {}, reason: '' };
  }

  private static validateUploadFiles(action: string, params: Record<string, Object>): void {
    if (action !== BridgeAction.UPLOAD) {
      return;
    }
    const files = params.files;
    if (!Array.isArray(files) || files.length === 0) {
      throw new Error('upload_file_missing: 请提供要上传文件的绝对路径');
    }
    for (const entry of files as string[]) {
      const path = typeof entry === 'string' ? entry.trim() : '';
      if (!path || !(path.startsWith('/') || /^[A-Za-z]:[\\/]/.test(path) || path.startsWith('\\\\'))) {
        throw new Error(`upload_file_path_invalid: ${path}`);
      }
      try {
        if (!fs.statSync(path).isFile()) {
          throw new Error('not a regular file');
        }
      } catch (_) {
        throw new Error(`upload_file_not_accessible: ${path}`);
      }
    }
  }

  private static nextRequestId(): string {
    this.seed++;
    return `ts_bridge_req_${Date.now()}_${this.seed}`;
  }

  private static transportError(type: string, message: string): Error {
    return new Error(JSON.stringify({ errorType: type, errorMessage: message }));
  }
}
