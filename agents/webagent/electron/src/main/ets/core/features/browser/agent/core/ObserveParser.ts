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

import { AgentObserveResult } from './AgentTypes';
import { AgentKeyPointsUtils, keyPointTrackingEnabled } from './AgentKeyPoints';
import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN = 0x6020;
const TAG = 'BrowserAgentGraph';

export class BrowserAgentObserveParser {
  static parse(rawContent: string): AgentObserveResult {
    const result = new AgentObserveResult();
    const trimmed = BrowserAgentObserveParser.normalizeRawText(rawContent);
    if (trimmed.length <= 0) {
      result.success = false;
      return result;
    }

    result.observationSummary = BrowserAgentObserveParser.extractJsonValue(trimmed, 'observation');
    result.previousAction = BrowserAgentObserveParser.extractJsonValue(trimmed, 'previous_action');
    result.takeNote = BrowserAgentObserveParser.normalizeOptionalValue(
      BrowserAgentObserveParser.extractJsonValue(trimmed, 'take_note')
    );
    result.summary = BrowserAgentObserveParser.extractJsonValue(trimmed, 'summary');
    result.needHtmlTextNext = BrowserAgentObserveParser.extractNeedHtmlTextNext(trimmed);
    result.trajectory = BrowserAgentObserveParser.normalizeOptionalValue(
      BrowserAgentObserveParser.extractJsonValue(trimmed, 'current_trajectory')
    );
    result.advice = BrowserAgentObserveParser.normalizeOptionalValue(
      BrowserAgentObserveParser.extractJsonValue(trimmed, 'advice')
    );

    result.relevantElementIds = BrowserAgentObserveParser.extractRelevantElementIds(trimmed);
    if (keyPointTrackingEnabled()) {
      result.keyPointProgress = AgentKeyPointsUtils.parseKeyPointUpdateFromContent(trimmed);
    }

    result.success = result.observationSummary.length > 0 ||
      result.previousAction.length > 0 ||
      result.takeNote.length > 0 ||
      result.summary.length > 0 ||
      result.trajectory.length > 0 ||
      result.advice.length > 0;
    return result;
  }

  private static extractRelevantElementIds(content: string): number[] {
    // Parse "key_elements": {"23": [...], "5": [...], ...} — extract the numeric keys
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

  private static extractJsonValue(content: string, key: string): string {
    const stringPattern = new RegExp(`"${key}"\\s*:\\s*"((?:\\\\.|[^"])*)"`);
    const stringMatch = content.match(stringPattern);
    if (stringMatch && stringMatch.length >= 2) {
      return BrowserAgentObserveParser.decodeEscapedText(stringMatch[1]);
    }

    const nullPattern = new RegExp(`"${key}"\\s*:\\s*null`);
    if (nullPattern.test(content)) {
      return 'null';
    }
    return '';
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

  private static extractNeedHtmlTextNext(content: string): boolean {
    const booleanMatch = content.match(/"need_html_text_next"\s*:\s*(true|false)(?=\s*[,}])/);
    if (booleanMatch && booleanMatch.length >= 2) {
      return booleanMatch[1] === 'true';
    }
    hilog.warn(DOMAIN, TAG, 'need_html_text_next not found in content, fallback to true');
    return true;
  }

  private static decodeEscapedText(content: string): string {
    return content
      .replace(/\\n/g, '\n')
      .replace(/\\r/g, '\r')
      .replace(/\\t/g, '\t')
      .replace(/\\"/g, '"')
      .replace(/\\\\/g, '\\');
  }

  private static normalizeRawText(rawContent: string): string {
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
}
