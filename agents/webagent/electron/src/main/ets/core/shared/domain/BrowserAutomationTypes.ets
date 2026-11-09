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

// Shared browser automation domain types used by the browser bridge
// and graph-based agent runtime.

export class BrowserPhase {
  public static readonly IDLE: string = 'IDLE';
  public static readonly LAUNCHING: string = 'LAUNCHING';
  public static readonly OBSERVING: string = 'OBSERVING';
  public static readonly THINKING: string = 'THINKING';
  public static readonly ACTING: string = 'ACTING';
  public static readonly WAITING: string = 'WAITING';
  public static readonly STOPPED: string = 'STOPPED';
  public static readonly FAILED: string = 'FAILED';
  public static readonly FINISHED: string = 'FINISHED';
}

export class BridgeAction {
  public static readonly ASK_PERCEPTION: string = 'ASK_PERCEPTION';
  public static readonly SCREENSHOT: string = 'SCREENSHOT';
  public static readonly NAVIGATE_TO: string = 'NAVIGATE_TO';
  public static readonly NAV_BACK: string = 'NAV_BACK';
  public static readonly NAV_FORWARD: string = 'NAV_FORWARD';
  public static readonly NAV_REFRESH: string = 'NAV_REFRESH';
  public static readonly CLICK: string = 'CLICK';
  public static readonly HOVER: string = 'HOVER';
  public static readonly FILL: string = 'FILL';
  public static readonly SEARCH: string = 'SEARCH';
  public static readonly TYPE: string = 'TYPE';
  public static readonly SELECT: string = 'SELECT';
  public static readonly SELECT_TEXT: string = 'SELECT_TEXT';
  public static readonly PRESS: string = 'PRESS';
  public static readonly SCROLL: string = 'SCROLL';
  public static readonly DRAG: string = 'DRAG';
  public static readonly FOCUS: string = 'FOCUS';
  public static readonly WAIT: string = 'WAIT';
  public static readonly DIALOG: string = 'DIALOG';
  public static readonly UPLOAD: string = 'UPLOAD';
  public static readonly DOWNLOAD_CLICK: string = 'DOWNLOAD_CLICK';
  public static readonly WAIT_FOR_DOWNLOAD: string = 'WAIT_FOR_DOWNLOAD';
  public static readonly RICH_TEXT: string = 'RICH_TEXT';
  public static readonly PDF: string = 'PDF';
  public static readonly TAB_NEW: string = 'TAB_NEW';
  public static readonly TAB_CLOSE: string = 'TAB_CLOSE';
  public static readonly TAB_FOCUS: string = 'TAB_FOCUS';
  public static readonly TABS: string = 'TABS';
  public static readonly NEW_WINDOW: string = 'NEW_WINDOW';
  public static readonly MINIMIZE_WINDOW: string = 'MINIMIZE_WINDOW';
  public static readonly RESIZE: string = 'RESIZE';
  public static readonly GET_RUNTIME_STATE: string = 'GET_RUNTIME_STATE';
  public static readonly GET_PAGE_HTML: string = 'GET_PAGE_HTML';
  public static readonly START_SCREENCAST: string = 'START_SCREENCAST';
  public static readonly STOP_SCREENCAST: string = 'STOP_SCREENCAST';
  public static readonly USER_CONTROL: string = 'USER_CONTROL';
  public static readonly AGENT_CONTROL: string = 'AGENT_CONTROL';
  public static readonly REQUEST_HUMAN_TAKEOVER: string = 'REQUEST_HUMAN_TAKEOVER';
  public static readonly QUERY: string = 'QUERY';
  public static readonly FINISH: string = 'FINISH';
  public static readonly BATCH_RUNTIME_RESET: string = 'BATCH_RUNTIME_RESET';
}

export class BridgeResponseType {
  public static readonly ACTION_RESULT: string = 'ACTION_RESULT';
  public static readonly PERCEPTION_RESPONSE: string = 'PERCEPTION_RESPONSE';
  public static readonly DIALOG_OPENING: string = 'DIALOG_OPENING';
  public static readonly FILE_CHOOSER_OPENED: string = 'FILE_CHOOSER_OPENED';
  public static readonly SCREENCAST_FRAME: string = 'SCREENCAST_FRAME';
  public static readonly PING: string = 'PING';
  public static readonly PONG: string = 'PONG';
}

export class BridgeStatus {
  public static readonly SUCCESS: string = 'SUCCESS';
  public static readonly ERROR: string = 'ERROR';
}

export class BridgeRequest {
  public action: string = '';
  public params: Record<string, Object> = {};
  public reqId: string = '';
  public taskId: string = '';
}

export class BridgeResponse {
  public msgId: string = '';
  public packetType: string = 'response';
  public browserId: number = 0;
  public type: string = '';
  public replyToId: string = '';
  public taskId: string = '';
  public status: string = '';
  public payload: Record<string, Object> = {};
  public payloadText: string = '';
  public timestamp: number = 0;
  public errorType: string = '';
  public errorMessage: string = '';
}

export class BrowserTransportStatus {
  public static readonly PENDING: string = 'pending';
  public static readonly SUCCESS: string = 'success';
  public static readonly ERROR: string = 'error';
  public static readonly TIMEOUT: string = 'timeout';
  public static readonly DISCONNECTED: string = 'disconnected';
}

export class BrowserTransportPacketType {
  public static readonly REQUEST: string = 'request';
  public static readonly RESPONSE: string = 'response';
}

export class BrowserTransportMessageType {
  public static readonly ASK_PERCEPTION: string = 'ASK_PERCEPTION';
  public static readonly EXECUTE_ACTION: string = 'EXECUTE_ACTION';
  public static readonly WAIT_LOADING: string = 'WAIT_LOADING';
  public static readonly ACTION_RESULT: string = 'ACTION_RESULT';
  public static readonly PAGE_STATUS: string = 'PAGE_STATUS';
}

export class BrowserTransportConstants {
  public static readonly CONNECTION_POLL_INTERVAL_MS: number = 80;
  public static readonly DEFAULT_ACTION_TIMEOUT_MS: number = 30000;
  public static readonly LOCAL_TIMEOUT_BUFFER_MS: number = 500;
  public static readonly USER_CONTROL_EVENT: number = 1;
  public static readonly FINISH_EVENT: number = 2;
  public static readonly PLUGIN_DISCONNECTED_EVENT: number = 5;
  public static readonly HAS_REQUESTED_TASK_STOP: string = 'hasRequestedTaskStop';
}

export class BrowserTransportRequest {
  public packetType: string = BrowserTransportPacketType.REQUEST;
  public messageType: string = '';
  public msgId: string = '';
  public browserId: number = 0;
  public reqId: string = '';
  public replyToId: string = '';
  public status: string = BrowserTransportStatus.PENDING;
  public payloadText: string = '';
  public timestamp: number = 0;
  public timeoutMs: number = 15000;
  public taskId: string = '';
}

export class BrowserTransportResponse {
  public packetType: string = BrowserTransportPacketType.RESPONSE;
  public messageType: string = '';
  public msgId: string = '';
  public browserId: number = 0;
  public reqId: string = '';
  public replyToId: string = '';
  public status: string = '';
  public payloadText: string = '';
  public timestamp: number = 0;
  public errorMessage: string = '';
}

export class BrowserPendingRequest {
  public request: BrowserTransportRequest = new BrowserTransportRequest();
  public response: BrowserTransportResponse | BridgeResponse | null = null;
}

export class BrowserExecutorEvent {
  public eventType: string = '';
  public browserId: number = 0;
  public messageType: string = '';
  public replyToId: string = '';
  public status: string = '';
  public payloadText: string = '';
  public errorMessage: string = '';
  public timestamp: number = 0;
}

export class BrowserReadyPayload {
  public status: string = '';
  public viewId: number = 0;
  public automationSessionKey: string = '';
  public transportOwner: string = '';
  public protocolVersion: string = '';
}

export class BrowserReadyBindingMode {
  public static readonly EXACT: string = 'exact';
  public static readonly COMPATIBILITY: string = 'compatibility';
  public static readonly UNMATCHED: string = 'unmatched';
  public static readonly INVALID: string = 'invalid';
}

export class BrowserReadyBindingResult {
  public mode: string = BrowserReadyBindingMode.UNMATCHED;
  public browserId: number = 0;
  public sessionKey: string = '';
  public runtime: BrowserRuntimeState | null = null;
  public errorMessage: string = '';
}

export class BrowserTransportHostState {
  public listenerRegistered: boolean = false;
  public operatorServerStarted: boolean = false;
  public lastEventAt: number = 0;
  public exactBindingCount: number = 0;
  public compatibilityBindingCount: number = 0;
  public unmatchedReadyCount: number = 0;
  public lastBindingMode: string = '';
  public lastBoundBrowserId: number = 0;
  public lastBoundSessionKey: string = '';
  public lastTransportOwner: string = '';
  public lastProtocolVersion: string = '';
  public lastError: string = '';
}

export class BrowserLiteDomRect {
  public left: number = 0;
  public top: number = 0;
  public right: number = 0;
  public bottom: number = 0;
  public width: number = 0;
  public height: number = 0;
}

export class BrowserViewport {
  public width: number = 0;
  public height: number = 0;
}

export class BrowserLiteDomElement {
  public tagName: string = '';
  public type: string = '';
  public ariaLabel: string = '';
  public dataId: string = '';
  public title: string = '';
  public value: string = '';
  public placeholder: string = '';
  public options: string[] = [];
}

export class BrowserLiteDomNode {
  public text: string = '';
  public include: boolean = false;
  public area: number = 0;
  public xpath: string = '';
  public selector: string = '';
  public type: string = '';
  public element: BrowserLiteDomElement = new BrowserLiteDomElement();
  public rects: BrowserLiteDomRect[] = [];
  public scrollViewportWidth: number = 0;
  public scrollViewportHeight: number = 0;
}

export class BrowserPerceptionPayload {
  public tabId: number = 0;
  public currentUrl: string = '';
  public currentTitle: string = '';
  public liteDomTree: BrowserLiteDomNode[] = [];
  public nearbyLiteDomTree: BrowserLiteDomNode[] = [];
  public liteDomSummary: string = '';
  public originalScreenshot: string = '';
  public annotatedScreenshot: string = '';
  public viewport: BrowserViewport = new BrowserViewport();
  public hasOpenDialog: boolean = false;
  public dialogState: Record<string, Object> | null = null;
  public rawPayloadText: string = '';
}

export class AgentActionType {
  public static readonly NAVIGATION: string = 'NAVIGATION';
  public static readonly NAVIGATE_TO: string = 'NAVIGATE_TO';
  public static readonly GO_BACK: string = 'GO_BACK';
  public static readonly TAB_NEW: string = 'TAB_NEW';
  public static readonly TAB_CLOSE: string = 'TAB_CLOSE';
  public static readonly TAB_FOCUS: string = 'TAB_FOCUS';
  public static readonly SCREENSHOT: string = 'SCREENSHOT';
  public static readonly CLICK: string = 'CLICK';
  public static readonly DIALOG: string = 'DIALOG';
  public static readonly SEARCH: string = 'SEARCH';
  public static readonly FILL: string = 'FILL';
  public static readonly SELECT_TEXT: string = 'SELECT_TEXT';
  public static readonly HOVER: string = 'HOVER';
  public static readonly SCROLL: string = 'SCROLL';
  public static readonly SELECT: string = 'SELECT';
  public static readonly PRESS: string = 'PRESS';
  public static readonly ARROW: string = 'ARROW';
  public static readonly WAIT: string = 'WAIT';
  public static readonly FINISH: string = 'FINISH';
  public static readonly USER_CONTROL: string = 'USER_CONTROL';
  public static readonly AGENT_CONTROL: string = 'AGENT_CONTROL';
  public static readonly REQUEST_HUMAN_TAKEOVER: string = 'REQUEST_HUMAN_TAKEOVER';
  public static readonly QUERY: string = 'QUERY';
  public static readonly UPLOAD: string = 'UPLOAD';
  public static readonly SKIP: string = 'SKIP';
  public static readonly TAKE_NOTE: string = 'TAKE_NOTE';
  public static readonly CALCULATE: string = 'CALCULATE';
  public static readonly RICH_TEXT: string = 'RICH_TEXT';
  public static readonly GET_HREF: string = 'GET_HREF';
  public static readonly DISPATCH: string = 'DISPATCH';
  public static readonly UNKNOWN: string = 'UNKNOWN';
}

export class BrowserActionKind {
  public static readonly NAVIGATION: string = 'navigation';
  public static readonly INPUT: string = 'input';
  public static readonly TASK_CONTROL: string = 'task_control';
  public static readonly INFORMATION: string = 'information';
  public static readonly UNKNOWN: string = 'unknown';
}

export class Point {
  public x: number = 0;
  public y: number = 0;
}

export class BrowserActionCommand {
  public kind: string = BrowserActionKind.UNKNOWN;
  public action: string = '';
  public actionType: string = '';
  public target: string = '';
  public value: string = '';
  public amount: string = '';
  public payloadText: string = '';
  public requiresLoadingWait: boolean = false;
  public reason: string = '';
  public fillMode: string = '';
  public fillFormat: string = '';
  public targetTabId: number = 0;
  public skipSensitiveActionCheck: boolean = false;
  public point: Point | undefined = undefined;
  // agent 侧已解析的目标元素：number=elementId（数字 id），string=CSS selector（用于 deferred 动作），
  // null=未解析（回退到 target 文本）。transport 层据此决定发 elementId 还是 selector 给插件。
  public element: number | string | null = null;
}

export class TaskTreeStatus {
  public static readonly CURRENT: string = 'current';
  public static readonly RUNNING: string = 'running';
  public static readonly COMPLETED: string = 'completed';
  public static readonly PENDING: string = 'pending';
  public static readonly KILLED: string = 'killed';
}

export class TaskTreeAction {
  public timeStep: number = 0;
  public globalStep: number = 0;
  public url: string = '';
  public reason: string = '';
  public action: string = '';
  public time: string = '';
  public inputTokens: number = 0;
  public outputTokens: number = 0;
}


export class BrowserNote {
  public webpage: BrowserNoteWebpage = new BrowserNoteWebpage();
  public information: string = '';
}

export class BrowserNoteWebpage {
  public title: string = '';
  public url: string = '';
  public active: boolean = true;
}

export class ManipulateState {
  public instruction: string = '';
  public startUrl: string = '';
  public manipulateStep: number = 0;
  public globalStep: number = 0;
  public stepBudget: number = 100;
  public previousImage: string = '';
  public previousOriginalImage: string = '';
  public currentImage: string = '';
  public currentOriginalImage: string = '';
  public liteDomTree: BrowserLiteDomNode[] = [];
  public outsideElements: string = '';
  public summary: string = 'None (first step)';
  public previousAction: string = '';
  public history: ManipulateHistoryEntry[] = [];
  public tabs: BrowserTabInfo[] = [];
  public note: BrowserNote[] = [];
  public knowledge: string = '';
  public tools: string = '';
  public analysis: string = '';
  public status: string = '';
  public previousIneffective: number = 0;
  public previousReturnValue: string = '';
  public actionQueue: string[] = [];
  public currentUrl: string = '';
}

export class ManipulateHistoryEntry {
  public step: number = 0;
  public url: string = '';
  public observation: string = '';
  public reason: string = '';
  public action: string = '';
  public summary: string = '';
}

export class BrowserTabSnapshot {
  public url: string = '';
  public title: string = '';
}

export class BrowserTabInfo {
  public tabId: number = 0;
  public windowId: number = 0;
  public title: string = '';
  public url: string = '';
  public active: boolean = false;
  public status: string = '';
  public snapshots: BrowserTabSnapshot[] = [];
  public currentIndex: number = 0;
}

export class BrowserAgentConfig {
  public summary: number = 1;
  public note: number = 2;
  public recursivePlan: number = 1;
  public separateJudge: number = 1;
  public replan: number = 1;
  public takeNoteInObserve: number = 1;
  public knowledge: number = 1;
  public previousScreen: number = 1;
  public inputTreeText: number = 1;
  public outsideElement: number = 1;
  public analyzeInstruction: number = 1;
  public conventional: number = 1;
  public domPerLine: number = 1;
  public model: string = '';
  public stepBudget: number = 100;
}

export class BrowserRuntimeState {
  public sessionKey: string = '';
  public sessionDir: string = '';
  public needClearBrowser: boolean = false;
  public browserId: number = 0;
  public transportOwner: string = '';
  public protocolVersion: string = '';
  public phase: string = BrowserPhase.IDLE;
  public task: string = '';
  public config: BrowserAgentConfig = new BrowserAgentConfig();
  public manipulateState: ManipulateState = new ManipulateState();
  public currentPerception: BrowserPerceptionPayload | null = null;
  public lastActionText: string = '';
  public lastSummary: string = '';
  public actionHistory: string[] = [];
  public noteHistory: string[] = [];
  public pendingRequestId: string = '';
  public ready: boolean = false;
  public stopRequested: boolean = false;
  public disconnected: boolean = false;
  public disconnectedReason: string = '';
  public errorMessage: string = '';
  public proceduralMemory: string = '';
  public sessionMemory: string = '';
  public createdAt: number = 0;
  public updatedAt: number = 0;
  public cacheScreenshot: boolean = true;
  public screenshotCacheDir: string = '';
}

export class BrowserSiteTool {
  public name: string = '';
  public displayName: string = '';
  public description: string = '';
  public siteId: string = '';
  public supportedStateIds: string[] = [];
}

export class BrowserSiteToolExecutionResult {
  public handled: boolean = false;
  public success: boolean = false;
  public summary: string = '';
  public finishSummary: string = '';
  public note: string = '';
  public requestsFinish: boolean = false;
  public payloadText: string = '';
}

export class BrowserToolAvailability {
  public toolId: string = '';
  public available: boolean = false;
  public matchedStateId: string = '';
  public reason: string = '';
}

export class BrowserPageState {
  public siteId: string = '';
  public stateId: string = '';
  public displayName: string = '';
  public currentUrl: string = '';
  public matchedBy: string = '';
}

export class BrowserPageStateResolver {
  public resolverId: string = '';
  public siteId: string = '';
  public displayName: string = '';
}
