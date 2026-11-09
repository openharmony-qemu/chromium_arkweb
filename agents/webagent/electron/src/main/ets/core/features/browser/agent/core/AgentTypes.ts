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

import { AgentKeyPointProgress, AgentKeyPointUpdate } from './AgentKeyPoints';

/** 判断原始 URL 是否以允许的小写 HTTP 前缀开头。 */
export function isNavigationUrlAllowed(url: string): boolean {
  return url.startsWith('http://') || url.startsWith('https://');
}

export class AgentConstants {
  public static readonly MAX_STEP: number = 50;
  public static readonly MAX_TIME_MS: number = 345000;
  public static readonly USER_RESPONSE_TIMEOUT: number = 24 * 3600 * 1000; //小艺24小时没通知用户选择
  public static readonly USER_COMPLETE_TIMEOUT: number = 3600 * 1000; //用户1小时没完成接管
  public static readonly BROWSER_MESSAGE_USER_CONTROLLING = 'CONTROL_STATE_HUMAN';
  public static readonly BROWSER_MESSAGE_USER_HAS_CONTROLLED = 'HUMAN_TAKEOVER';
  public static readonly IDLE_MESSAGE = 'CONTROL_STATE_IDLE';
  public static readonly ACTION_BUSY_MESSAGE = 'ACTION_BUSY';
  public static readonly DIALOG_OPENING_MESSAGE = 'DIALOG_OPENING';
  public static readonly PROTOCOL_CDP_ERROR = 'PROTOCOL_CDP_ERROR';
  public static readonly ACTION_NOT_SUPPORTED_MESSAGE = 'ACTION_UNSUPPORTED';
  public static readonly GROUP_CLOSED = 'TASK_GROUP_ALL_TABS_CLOSED';
  public static readonly ACTION_BUSY_WAIT_TIMEOUT: number = 30000;
  public static readonly ACTION_BUSY_RETRY_INTERVAL: number = 500;
  public static readonly SENSITIVE_ACTION_MESSAGE = 'SecurityCheckConfirmation';
  public static readonly NAVIGATE_PREFIX: string = '从头开始：';
  public static readonly CONTINUE_PREFIX: string = '从当前页面开始：';
  public static readonly NO_INJECTION: number = 0;
  public static readonly INJECTED: number = 1;
  public static readonly PROMPT_FAILED: number = -1;
  public static readonly IMAGE_RESIZE_MAX_PIXELS: number = 2560 * 1440;
  public static readonly LLM_TIMEOUT_MS: number = 40000;
  public static readonly LITEDOM_MAX_OPTIONS: number = 100;
  public static readonly LITEDOM_MAX_TITLE_LENGTH: number = 100;
  public static readonly LITEDOM_MAX_SUMMARY_LENGTH: number = 12000;
  public static readonly TAB_STATUS_LOADING = 'loading';
}

export enum UserControlOptionType {
  TAKEOVER = 'TAKEOVER',
  CONTINUE = 'CONTINUE',
  ALWAYS_PERMIT = 'ALWAYS_PERMIT',
  ABORT = 'ABORT'
}

export enum UserControlReasonType {
  USER_CONTROL = 'UserControl',
  DANGEROUS_OPERATION = 'DangerousOperation',
  PROMPT_INJECTION = 'PromptInjection',
  SENSITIVE_OPERATION = 'SensitiveOperation',
  ASK_USER = 'AskUser',
}

export interface UserControlReason {
  type: UserControlReasonType;
  sensitiveReason?: string;
  explanation?: string;
  injectionContext?: string;
  question?: string;
}

export interface UserControlResponse {
  choiceKey?: UserControlOptionType;
  answer?: string;
}

export class AgentPhase {
  static readonly IDLE: string = 'IDLE';
  static readonly OBSERVING: string = 'OBSERVING';
  static readonly THINKING: string = 'THINKING';
  static readonly ACTING: string = 'ACTING';
  static readonly FINISHED: string = 'FINISHED';
  static readonly FAILED: string = 'FAILED';
}

export class AgentLiteDomRect {
  left: number = 0;
  top: number = 0;
  right: number = 0;
  bottom: number = 0;
  width: number = 0;
  height: number = 0;
}

export type OutsideDirection = 'above' | 'below' | 'left' | 'right';

export class AgentViewport {
  width: number = 0;
  height: number = 0;
}

export class AgentLiteDomElement {
  tagName: string = '';
  type: string = '';
  ariaLabel: string = '';
  dataId: string = '';
  title: string = '';
  value: string = '';
  placeholder: string = '';
  options: string[] = [];
  optionValues: string[] = [];
}

export class AgentLiteDomNode {
  id: number = 0;
  text: string = '';
  include: boolean = false;
  area: number = 0;
  xpath: string = '';
  selector: string = '';
  type: string = '';
  element: AgentLiteDomElement = new AgentLiteDomElement();
  rects: AgentLiteDomRect[] = [];
  scrollViewportWidth: number = 0;
  scrollViewportHeight: number = 0;
}

export class AgentOutsideDOMTree {
  above: AgentLiteDomNode[] = [];
  below: AgentLiteDomNode[] = [];
  left: AgentLiteDomNode[] = [];
  right: AgentLiteDomNode[] = [];
  idDirectionMap: Map<string, OutsideDirection> = new Map<string, OutsideDirection>();
}

export class AgentObservation {
  currentUrl: string = '';
  currentTitle: string = '';
  html: string = '';
  liteDomTree: AgentLiteDomNode[] = [];
  outsideDOMTree: AgentOutsideDOMTree = new AgentOutsideDOMTree();
  tabs: AgentTabInfo[] = [];
  liteDomSummary: string = '';
  outsideDomSummary: string = '';
  htmlHash: number = 0;
  originalScreenshot: string = '';
  annotatedScreenshot: string = '';
  viewport: AgentViewport = new AgentViewport();
  siteId: string = '';
  pageStateId: string = '';
  pageStateName: string = '';
  hasOpenDialog: boolean = false;
  dialogState: Record<string, Object> | null = null;
  tabStatus: string = '';
}

export class AgentHistoryEntry {
  reason: string = '';
  action: string = '';
  actionText: string = '';
  previousAction: string = '';
  observation: string = '';
  takeNote: string = '';
  toolCall: string = '';
  explanation: string = '';
}

export class AgentNoteEntry {
  webpageTitle: string = '';
  webpageUrl: string = '';
  information: string = '';
}

export class AgentQuestionEntry {
  question: string = '';
  answer: string = '';
}

export class AgentTabSnapshot {
  url: string = '';
  title: string = '';
}

export class AgentTabInfo {
  tabId: number = 0;
  windowId: number = 0;
  title: string = '';
  url: string = '';
  active: boolean = false;
  status: string = '';
  snapshots: AgentTabSnapshot[] = [];
  currentIndex: number = 0;
}

export class AgentTaskResult {
  success: boolean = false;
  status: string = '';
  message: string = '';
  statusCode: number = 0;
  answer: string = '';
  reference: string = '';
  tracePayloadText: string = '';
  version: string = '';
}

export class AgentPoint {
  public x: number = 0;
  public y: number = 0;
}

export class BrowserAgentAction {
  action: string = '';
  // dispatch不使用以下三个
  actionType: string = '';
  element: number | string | null = null; //id, xpath, or null
  params: string [] = []; //不包括element
  requiresLoadingWait: boolean = true;
  reason: string = '';
  skipSensitiveActionCheck?: boolean;
  point: AgentPoint | undefined = undefined; // 坐标（原始范围 [0, 1000]）
  // deferred 动作专用：原目标元素的 xpath，供 handleBacklog 做"沿路径左下区域稳定"精查。
  // 普通动作不设；deferred 动作由 maybeDeferOutsideElementAction 设为 node.xpath。
  xpath: string = '';
}


export class AgentToolDescriptor {
  toolId: string = '';
  displayName: string = '';
  description: string = '';
  domain: string = '';
}

export class AgentToolCall {
  toolId: string = '';
  toolName: string = '';
  inputText: string = '';
}

export class AgentToolResult {
  handled: boolean = false;
  success: boolean = false;
  summary: string = '';
  note: string = '';
  returnValue: string = '';
  resultPayloadText: string = '';
  finishSummary: string = '';
}

export class AgentToolQuery {
  task: string = '';
  currentUrl: string = '';
  siteId: string = '';
  pageStateId: string = '';
}

export class AgentObserveResult {
  success: boolean = false;
  observationSummary: string = '';
  previousAction: string = '';
  takeNote: string = '';
  summary: string = '';
  needHtmlTextNext: boolean = true;
  trajectory: string = '';
  advice: string = '';
  relevantElementIds: number[] = [];
  keyPointProgress: AgentKeyPointUpdate[] | null = null;
}

export class AgentDecision {
  observationSummary: string = '';
  previousAction: string = '';
  takeNote: string = '';
  summary: string = '';
  needHtmlTextNext: boolean = true;
  trajectory: string = '';
  advice: string = '';
  keyPointProgress: AgentKeyPointUpdate[] | null = null;
  relevantElementIds: number[] = [];
  reason: string = '';
  explanation: string = '';
  browserAction: BrowserAgentAction | null = null;
  actionQueue: BrowserAgentAction [] = []; //excluding browserAction
  toolCall: AgentToolCall | null = null;
  finishSummary: string = '';
  waitRequested: boolean = false;
  handoffToUserRequested: boolean = false;
  needScreenshot: boolean = false;
}

export class AgentGraphStatus {
  static readonly FINISH: string = 'finish';
  static readonly ERROR: string = 'error';
  static readonly TIMEOUT: string = 'timeout';
}

export class AgentGraphResult {
  status: string = AgentGraphStatus.FINISH;
  message: string = '';
  nodeName: string = '';
}

export class AgentLLMMessage {
  role: string = '';
  content: string = '';
}

export class AgentLLMRequest {
  systemPrompt: string = '';
  userPrompt: string = '';
  images: string[] = [];
  imageFileNames: string[] = [];
  scene: string = '';
  responseFormat: string = '';
  conversationId: string = '';
  threadId: string = '';
  messages: AgentLLMMessage[] = [];
}

export class AgentLLMResponse {
  text: string = '';
  rawText: string = '';
  promptTokens: number = 0;
  completionTokens: number = 0;
  totalTokens: number = 0;
  promptCacheHitTokens: number = 0;
  promptCacheMissTokens: number = 0;
}

export class BrowserLLMUsageRecord {
  tag: string = '';
  promptTokens: number = 0;
  completionTokens: number = 0;
  totalTokens: number = 0;
  promptCacheHitTokens: number = 0;
  promptCacheMissTokens: number = 0;
  durationMs: number = 0;
}

export class DetectionResult {
  riskCode: number = 0; //1恶意 0无风险 -1失败
  context: string = '';
  evidence: Record <string, number | string> = {};
  errCode: number = 0; //-1时有意义
  errMsg: string = ''; //-1时有意义
}

export class BrowserPromptConstants {
  public static readonly IMAGE_PLACEHOLDER: string = '<MyImagePlaceHolder>';
}
