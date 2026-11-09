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

import { webSocket } from '@kit.NetworkKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import BuildProfile from 'BuildProfile';
import { BridgeResponse } from '../../../shared/domain/BrowserAutomationTypes';
import { BrowserTransportProtocolService } from './BrowserTransportProtocolService';

const DOMAIN = 0x6020;
const TAG = 'BrowserWebSocketSample';
const PORT = 18900;
const MAX_MESSAGE_CHARS = 16 * 1024 * 1024;

/** Debug-only, loopback WebSocket sample carrying the existing business JSON. */
export class BrowserTransportServerService {
  private static server: webSocket.WebSocketServer | null = null;
  private static connection: webSocket.WebSocketConnection | null = null;
  private static browserId: number = 0;
  private static responseListener: ((response: BridgeResponse) => void) | null = null;
  private static disconnectListener: ((browserId: number, reason: string) => void) | null = null;

  static setResponseListener(listener: (response: BridgeResponse) => void): void {
    this.responseListener = listener;
  }

  static setDisconnectListener(listener: (browserId: number, reason: string) => void): void {
    this.disconnectListener = listener;
  }

  static getListeningPort(): number {
    return this.server === null ? 0 : PORT;
  }

  static getWebSocketUrl(): string {
    return this.server === null ? '' : `ws://127.0.0.1:${PORT}/extension`;
  }

  static hasActiveConnection(): boolean {
    return this.connection !== null;
  }

  static async ensureStarted(): Promise<boolean> {
    // The original Native hello proved the caller's identity. A plain WS
    // sample must never expose browser control in a release build.
    if (!BuildProfile.DEBUG) {
      return false;
    }
    if (this.server !== null) {
      return true;
    }
    const server = webSocket.createWebSocketServer();
    server.on('connect', (connection: webSocket.WebSocketConnection): void => {
      if (this.connection !== null) {
        void server.close(connection, { code: 1008, reason: 'connection occupied' });
        return;
      }
      this.connection = connection;
      this.browserId = 0;
    });
    server.on('messageReceive', (message: webSocket.WebSocketMessage): void => {
      if (!this.isCurrent(message.clientConnection)) {
        return;
      }
      if (typeof message.data !== 'string' || message.data.length > MAX_MESSAGE_CHARS) {
        void server.close(message.clientConnection, { code: 1002, reason: 'invalid text message' });
        return;
      }
      const response = BrowserTransportProtocolService.parseExtensionMessage(message.data);
      if (response.type.length === 0) {
        void server.close(message.clientConnection, { code: 1002, reason: 'invalid business message' });
        return;
      }
      if (response.browserId > 0) {
        this.browserId = response.browserId;
      }
      if (BrowserTransportProtocolService.isPing(response)) {
        void server.send(BrowserTransportProtocolService.buildPongMessage(response.timestamp), message.clientConnection);
        return;
      }
      this.responseListener?.(response);
    });
    server.on('close', (connection: webSocket.WebSocketConnection): void => {
      if (this.isCurrent(connection)) {
        this.connection = null;
        const browserId = this.browserId;
        this.browserId = 0;
        this.disconnectListener?.(browserId, 'disconnected');
      }
    });
    server.on('error', (error: Error): void => {
      hilog.error(DOMAIN, TAG, 'WebSocket error: %{public}s', error.message);
    });
    try {
      const started = await server.start({
        serverIP: '127.0.0.1', serverPort: PORT,
        maxConcurrentClientsNumber: 1, maxConnectionsForOneClient: 1
      });
      if (started) {
        this.server = server;
        return true;
      }
    } catch (error) {
      hilog.error(DOMAIN, TAG, 'WebSocket start failed: %{public}s', String(error));
    }
    try {
      await server.stop();
    } catch (_) {
      // The server may not have reached the listening state.
    }
    return false;
  }

  static async sendActionRequest(action: string, params: Record<string, Object>, reqId: string,
    taskId: string, agentReason: string): Promise<boolean> {
    const server = this.server;
    const connection = this.connection;
    if (!server || !connection) {
      return false;
    }
    const text = BrowserTransportProtocolService.buildActionRequest(action, params, reqId, taskId, agentReason);
    if (text.length > MAX_MESSAGE_CHARS) {
      return false;
    }
    try {
      return await server.send(text, connection);
    } catch (error) {
      hilog.warn(DOMAIN, TAG, 'WebSocket send failed: %{public}s', String(error));
      return false;
    }
  }

  static async shutdown(): Promise<void> {
    const server = this.server;
    this.server = null;
    this.connection = null;
    this.browserId = 0;
    if (server) {
      await server.stop();
      server.off('connect');
      server.off('messageReceive');
      server.off('close');
      server.off('error');
    }
  }

  private static isCurrent(connection: webSocket.WebSocketConnection): boolean {
    return this.connection !== null &&
      this.connection.clientIP === connection.clientIP &&
      this.connection.clientPort === connection.clientPort;
  }
}
