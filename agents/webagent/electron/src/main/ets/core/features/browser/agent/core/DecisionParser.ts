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
  AgentDecision,
  AgentToolCall,
  BrowserAgentAction,
  AgentPoint
} from './AgentTypes';
import { AgentKeyPointsUtils, keyPointTrackingEnabled } from './AgentKeyPoints';
import { resolveActionNeedWait, synthesizeActionString } from '../prompts/Actions';
import { BrowserAgentUtils } from './Utils';
import { vConfig } from '../VersionConfig';
import { hilog } from '@kit.PerformanceAnalysisKit';
const DOMAIN = 0x6020;
const TAG = 'BrowserAgentGraph';

interface StructuredActionObject {
  name?: Object;
  args?: Object;
}

export class BrowserAgentDecisionParser {
  static parse(rawContent: string): AgentDecision {
    const trimmed = BrowserAgentDecisionParser.normalizeRawDecisionText(rawContent);
    if (trimmed.length <= 0) {
      return BrowserAgentDecisionParser.buildFallbackDecision('empty browser decision');
    }

    const decision = new AgentDecision();
    decision.reason = BrowserAgentDecisionParser.extractJsonString(trimmed, 'reason');
    decision.summary = BrowserAgentDecisionParser.extractJsonString(trimmed, 'summary');
    decision.observationSummary = BrowserAgentDecisionParser.extractJsonString(trimmed, 'observation');
    decision.takeNote = BrowserAgentDecisionParser.extractJsonString(trimmed, 'take_note');
    decision.explanation = BrowserAgentDecisionParser.extractJsonString(trimmed, 'explain');
    [decision.browserAction, decision.actionQueue] = BrowserAgentDecisionParser.parseAction(trimmed);
    if (decision.browserAction) {
      decision.browserAction.reason = decision.explanation;
    }
    for (const queuedAction of decision.actionQueue) {
      queuedAction.reason = decision.explanation;
    }
    hilog.debug(DOMAIN, TAG, 'node reason parse action %{private}s', JSON.stringify(decision.browserAction));
    hilog.debug(DOMAIN, TAG, 'node reason parse action queue %{private}s', JSON.stringify(decision.actionQueue));

    decision.toolCall = BrowserAgentDecisionParser.parseToolCall(trimmed);
    hilog.debug(DOMAIN, TAG, 'node reason parse tool %{private}s', JSON.stringify(decision.toolCall));

    BrowserAgentDecisionParser.applyControlFlags(decision);
    BrowserAgentDecisionParser.populateFinishSummary(decision);
    
    decision.previousAction = BrowserAgentDecisionParser.extractJsonString(trimmed, 'previous_action');
    decision.needHtmlTextNext = BrowserAgentDecisionParser.extractNeedHtmlTextNext(trimmed);
    decision.needScreenshot = BrowserAgentDecisionParser.extractNeedScreenshot(trimmed);
    decision.trajectory = BrowserAgentDecisionParser.normalizeOptionalValue(
      BrowserAgentDecisionParser.extractJsonString(trimmed, 'current_trajectory')
    );
    decision.advice = BrowserAgentDecisionParser.normalizeOptionalValue(
      BrowserAgentDecisionParser.extractJsonString(trimmed, 'advice')
    );
    decision.relevantElementIds = BrowserAgentDecisionParser.extractRelevantElementIds(trimmed);
    if (keyPointTrackingEnabled()) {
      decision.keyPointProgress = AgentKeyPointsUtils.parseKeyPointUpdateFromContent(trimmed);
    }
    
    return decision;
  }

  static buildFallbackDecision(reason: string): AgentDecision {
    const decision = new AgentDecision();
    decision.reason = reason;
    return decision;
  }

  private static parseAction(content: string): [BrowserAgentAction | null, BrowserAgentAction[]] {
    const data = JSON.parse(content);
    let rawActions: Object[] = [];

    if (Array.isArray(data.action)) {
      rawActions = data.action as Object[];
    } else if (typeof data.action === 'string') {
      // 容错：LLM 偶尔会把 action 输出为 JSON 字符串而非数组，例如
      // "action": "[{\"name\": \"CLICK\", \"args\": {\"elementId\": 45}}]"。
      // 解析回数组后仍走下方 legacy / structured 路径，与 action 格式正交。
      rawActions = BrowserAgentDecisionParser.parseStringifiedActions(data.action as string);
    }

    if (rawActions.length === 0) {
      return [null, []];
    }

    if (vConfig.json_action_format && typeof rawActions[0] === 'object' && rawActions[0] !== null) {
      return BrowserAgentDecisionParser.parseStructuredActions(rawActions);
    }
    return BrowserAgentDecisionParser.parseLegacyActions(rawActions.map(rawAction => String(rawAction)));
  }

  private static parseStringifiedActions(rawText: string): Object[] {
    const trimmed = rawText.trim();
    if (trimmed.length <= 0) {
      return [];
    }
    // 仅处理看起来像 JSON 数组/对象的字符串，避免把裸 DSL（如 "CLICK [45]"）误判为 JSON。
    if (!trimmed.startsWith('[') && !trimmed.startsWith('{')) {
      return [];
    }
    try {
      const parsed = JSON.parse(trimmed);
      if (Array.isArray(parsed)) {
        return parsed as Object[];
      }
      if (parsed !== null && typeof parsed === 'object') {
        // 单个 action 对象被字符串化时，包装成数组走后续解析。
        return [parsed as Object];
      }
    } catch (error) {
      hilog.warn(DOMAIN, TAG, 'parse stringified action failed: %{private}s', error.message);
    }
    return [];
  }

  private static parseLegacyActions(actions: string[]): [BrowserAgentAction | null, BrowserAgentAction[]] {
    const normalizedActionText = actions[0];
    const action = new BrowserAgentAction();
    action.action = normalizedActionText;
    BrowserAgentDecisionParser.resolveStructuredAction(action);
    action.requiresLoadingWait = BrowserAgentDecisionParser.resolveActionRequiresLoadingWait(normalizedActionText);

    let queuedActions: BrowserAgentAction[] = [];
    for (const text of actions.slice(1)) {
      const normalizedActionText = BrowserAgentDecisionParser.normalizeActionText(text);
      const queuedAction = new BrowserAgentAction();
      queuedAction.action = normalizedActionText;
      BrowserAgentDecisionParser.resolveStructuredAction(queuedAction);
      queuedAction.requiresLoadingWait = BrowserAgentDecisionParser.resolveActionRequiresLoadingWait(normalizedActionText);
      queuedActions.push(queuedAction);
    }
    return [action, queuedActions];
  }

  private static parseStructuredActions(rawActions: Object[]): [BrowserAgentAction | null, BrowserAgentAction[]] {
    const actionObj = rawActions[0] as StructuredActionObject;
    const name = actionObj.name !== undefined && actionObj.name !== null ? String(actionObj.name) : '';
    const argsRaw = actionObj.args;
    const args: Record<string, Object> = argsRaw !== undefined && argsRaw !== null && typeof argsRaw === 'object'
      ? argsRaw as Record<string, Object>
      : {};
    const dslString = synthesizeActionString(name, args);
    if (name.trim().length <= 0 || dslString.length <= 0) {
      return [null, []];
    }

    const action = new BrowserAgentAction();
    action.action = dslString;
    BrowserAgentDecisionParser.resolveStructuredAction(action);
    action.requiresLoadingWait = BrowserAgentDecisionParser.resolveActionRequiresLoadingWait(action.action);

    const queuedActions: BrowserAgentAction[] = [];
    for (const rawAction of rawActions.slice(1)) {
      const queuedActionObj = rawAction as StructuredActionObject;
      const queuedName = queuedActionObj.name !== undefined && queuedActionObj.name !== null
        ? String(queuedActionObj.name)
        : '';
      const queuedArgsRaw = queuedActionObj.args;
      const queuedArgs: Record<string, Object> =
        queuedArgsRaw !== undefined && queuedArgsRaw !== null && typeof queuedArgsRaw === 'object'
          ? queuedArgsRaw as Record<string, Object>
          : {};
      const queuedDslString = synthesizeActionString(queuedName, queuedArgs);
      if (queuedName.trim().length <= 0 || queuedDslString.length <= 0) {
        return [null, []];
      }

      const queuedAction = new BrowserAgentAction();
      queuedAction.action = queuedDslString;
      BrowserAgentDecisionParser.resolveStructuredAction(queuedAction);
      queuedAction.requiresLoadingWait = BrowserAgentDecisionParser.resolveActionRequiresLoadingWait(queuedAction.action);
      queuedActions.push(queuedAction);
    }
    return [action, queuedActions];
  }

  private static parseToolCall(content: string): AgentToolCall | null {
    let toolId = BrowserAgentDecisionParser.extractNestedJsonString(content, 'tool', 'toolId');
    let parametersText = BrowserAgentDecisionParser.extractNestedJsonObjectText(content, 'tool', 'parameters');
    if (toolId.length <= 0) {
      toolId = BrowserAgentDecisionParser.extractJsonString(content, 'toolId');
    }
    if (parametersText.length <= 0) {
      parametersText = BrowserAgentDecisionParser.extractJsonObjectText(content, 'parameters');
    }
    if (toolId.length <= 0) {
      return null;
    }

    const call = new AgentToolCall();
    call.toolId = BrowserAgentDecisionParser.normalizeToolId(toolId);
    call.toolName = call.toolId;
    call.inputText = parametersText;
    return call;
  }
  private static resolveStructuredAction(action: BrowserAgentAction): void {
    const actionText = action.action.trim();
    const actionName = actionText.split(/\s+/)[0].toUpperCase();
    const params = BrowserAgentUtils.extractBracketParams(actionText);

    action.actionType = actionName;
    action.params = params;

    const elementIndex = BrowserAgentDecisionParser.getElementParamIndex(actionName);
    if (elementIndex >= 0 && params.length > elementIndex) {
      const elementParam = params[elementIndex];
      const point = BrowserAgentDecisionParser.parseCoordinate(elementParam);
      if (point) {
        action.point = point;
      } else {
        action.element = /^\d+$/.test(elementParam) ? Number(elementParam) : elementParam;
      }
    }
  }

  private static getElementParamIndex(actionName: string): number {
    switch (actionName) {
      case 'CLICK':
      case 'HOVER':
      case 'SEARCH':
      case 'SEARCH_BY_QUERY':
      case 'SELECT_TEXT':
      case 'SELECT':
      case 'UPLOAD':
        return 0;
      case 'FILL':
      case 'RICH_TEXT':
        return 1;
      case 'SCROLL':
        return 1;
      default:
        return -1;
    }
  }

  private static parseCoordinate(text: string): AgentPoint | null {
    if (!text || !text.toUpperCase().startsWith('COORDINATE')) {
      return null;
    }
    const match = text.match(/^COORDINATE\s*\[\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*(\d+)\s*,\s*(\d+))?\s*\]$/i);
    if (!match) {
      return null;
    }
    const x1 = parseInt(match[1]);
    const y1 = parseInt(match[2]);
    const point = new AgentPoint();
    if (match[3] && match[4]) {
      const x2 = parseInt(match[3]);
      const y2 = parseInt(match[4]);
      point.x = Math.floor((x1 + x2) / 2);
      point.y = Math.floor((y1 + y2) / 2);
    } else {
      point.x = x1;
      point.y = y1;
    }
    return point;
  }
  private static applyControlFlags(decision: AgentDecision): void {
    if (!decision.browserAction) {
      return;
    }
    decision.waitRequested = BrowserAgentDecisionParser.startsWith(decision.browserAction.action, 'WAIT');
    decision.handoffToUserRequested = BrowserAgentDecisionParser.startsWith(decision.browserAction.action, 'USER_CONTROL');
  }

  private static populateFinishSummary(decision: AgentDecision): void {
    if (!BrowserAgentDecisionParser.isFinishAction(decision.browserAction) || decision.finishSummary.length > 0) {
      return;
    }
    if (decision.summary.length > 0) {
      decision.finishSummary = decision.summary;
      return;
    }
    if (decision.observationSummary.length > 0) {
      decision.finishSummary = decision.observationSummary;
      return;
    }
    if (decision.browserAction) {
      decision.finishSummary = decision.browserAction.action;
    }
  }

  private static isFinishAction(action: BrowserAgentAction | null): boolean {
    return !!action && BrowserAgentDecisionParser.startsWith(action.action, 'FINISH');
  }

  private static resolveActionRequiresLoadingWait(actionText: string): boolean {
    const actionName = actionText.trim().split(/\s+/)[0];
    if (actionName === 'TAB_NEW') {
      return true;
    }
    return resolveActionNeedWait(actionName);
  }

  private static normalizeActionText(actionText: string): string {
    const trimmed = actionText.trim();
    if (trimmed.length <= 0 || trimmed.indexOf('[') >= 0) {
      return trimmed;
    }

    const searchLike = trimmed.match(/^(SEARCH)\s+(\d+)\s+(.+)$/);
    if (searchLike) {
      return `${searchLike[1]} [${searchLike[2]}] [${searchLike[3].trim()}]`;
    }

    const fillLike = trimmed.match(/^(FILL|RICH_TEXT)\s+(INPUT|REPLACE|PREPEND|APPEND)\s+(\d+)\s+(.+)$/i);
    if (fillLike) {
      return `${fillLike[1]} [${fillLike[2].toUpperCase()}] [${fillLike[3]}] [${fillLike[4].trim()}]`;
    }

    const singleArg = trimmed.match(/^(CLICK|HOVER)\s+(\d+)$/);
    if (singleArg) {
      return `${singleArg[1]} [${singleArg[2]}]`;
    }

    const navigationLike = trimmed.match(/^(NAVIGATION|NAVIGATE_TO)\s+(.+)$/);
    if (navigationLike) {
      return `${navigationLike[1]} [${navigationLike[2].trim()}]`;
    }

    const tabNewLike = trimmed.match(/^TAB_NEW\s+(.+)$/);
    if (tabNewLike) {
      return `TAB_NEW [${tabNewLike[1].trim()}]`;
    }

    const selectLike = trimmed.match(/^SELECT\s+([^\s]+)\s+(.+)$/);
    if (selectLike) {
      return `SELECT [${selectLike[1].trim()}] [${selectLike[2].trim()}]`;
    }

    const selectTextLike = trimmed.match(/^SELECT_TEXT\s+(\d+)\s+(.+)\s+(\d+)$/);
    if (selectTextLike) {
      return `SELECT_TEXT [${selectTextLike[1]}] [${selectTextLike[2].trim()}] [${selectTextLike[3]}]`;
    }

    return trimmed;
  }

  private static extractJsonString(content: string, key: string): string {
    const MAX_CONTENT_LENGTH = 32 * 1024;
    if (content.length > MAX_CONTENT_LENGTH) {
      return '';
    }

    const pattern = new RegExp(`"${key}"\\s*:\\s*"((?:\\\\.|[^"\\\\])*)"`);
    const match = content.match(pattern);
    if (!match || match.length < 2) {
      return '';
    }
    return BrowserAgentDecisionParser.decodeEscapedText(match[1]);
  }

  private static extractJsonObjectText(content: string, key: string): string {
    const keyIndex = content.indexOf(`"${key}"`);
    if (keyIndex < 0) {
      return '';
    }
    const braceStart = content.indexOf('{', keyIndex);
    if (braceStart < 0) {
      return '';
    }

    let depth = 0;
    for (let i = braceStart; i < content.length; i++) {
      const currentChar = content.charAt(i);
      if (currentChar === '{') {
        depth += 1;
      } else if (currentChar === '}') {
        depth -= 1;
        if (depth === 0) {
          return content.substring(braceStart, i + 1);
        }
      }
    }
    return '';
  }

  private static extractNestedJsonString(content: string, objectKey: string, nestedKey: string): string {
    const objectText = BrowserAgentDecisionParser.extractJsonObjectText(content, objectKey);
    if (objectText.length <= 0) {
      return '';
    }
    return BrowserAgentDecisionParser.extractJsonString(objectText, nestedKey);
  }

  private static extractNestedJsonObjectText(content: string, objectKey: string, nestedKey: string): string {
    const objectText = BrowserAgentDecisionParser.extractJsonObjectText(content, objectKey);
    if (objectText.length <= 0) {
      return '';
    }
    return BrowserAgentDecisionParser.extractJsonObjectText(objectText, nestedKey);
  }

  private static decodeEscapedText(content: string): string {
    return content
      .replace(/\\n/g, '\n')
      .replace(/\\r/g, '\r')
      .replace(/\\t/g, '\t')
      .replace(/\\"/g, '"')
      .replace(/\\\\/g, '\\');
  }

  private static startsWith(content: string, prefix: string): boolean {
    return content.indexOf(prefix) === 0;
  }

  private static normalizeRawDecisionText(rawContent: string): string {
    let normalized = rawContent.trim();
    if (normalized.startsWith('```')) {
      const firstLineEnd = normalized.indexOf('\n');
      if (firstLineEnd >= 0) {
        normalized = normalized.substring(firstLineEnd + 1);
      }
      if (normalized.endsWith('```')) {
        normalized = normalized.substring(0, normalized.length - 3).trim();
      }
    }
    return normalized.trim();
  }

  private static normalizeToolId(toolId: string): string {
    return toolId.trim().toLowerCase();
  }

  private static extractNeedHtmlTextNext(content: string): boolean {
    const booleanMatch = content.match(/"need_html_text_next"\s*:\s*(true|false)(?=\s*[,}])/);
    if (booleanMatch && booleanMatch.length >= 2) {
      return booleanMatch[1] === 'true';
    }
    hilog.warn(DOMAIN, TAG, 'need_html_text_next not found in content, fallback to true');
    return true;
  }

  private static extractNeedScreenshot(content: string): boolean {
    const booleanMatch = content.match(/"need_screenshot"\s*:\s*(true|false)(?=\s*[,}])/);
    if (booleanMatch && booleanMatch.length >= 2) {
      return booleanMatch[1] === 'true';
    }
    return false;
  }

  private static normalizeOptionalValue(value: string): string {
    const normalized = value.trim();
    if (normalized.length <= 0) {
      return '';
    }
    const lowered = normalized.toLowerCase();
    if (lowered === 'no' || lowered === 'none' || lowered === 'null') {
      return '';
    }
    return normalized;
  }

  private static extractRelevantElementIds(content: string): number[] {
    const keyElementsPattern = /"key_elements"\s*:\s*\{([^}]*)\}/;
    const match = content.match(keyElementsPattern);
    if (!match || match.length < 2) {
      return [];
    }
    const ids: number[] = [];
    const keyPattern = /"(\d+)"\s*:/g;
    let keyMatch: RegExpExecArray | null;
    while ((keyMatch = keyPattern.exec(match[1])) !== null) {
      const id = Number(keyMatch[1]);
      if (!Number.isNaN(id)) {
        ids.push(id);
      }
    }
    return ids;
  }
}
