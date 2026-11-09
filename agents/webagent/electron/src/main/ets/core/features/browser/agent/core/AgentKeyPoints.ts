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

import { vConfig } from '../VersionConfig';
import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN = 0x6020;
const TAG = 'KeyPoints';

export class KeypointTypeBoolean {
  finished: boolean = false;
}

export class KeypointTypeNumber {
  required: number;
  finished: number;
}

export class KeypointTypeList {
  required: string [];
  finished: boolean []; // 和required长度相同
}

export class AgentKeyPoint {
  index: number = 0;
  description: string = '';
  status: KeypointTypeBoolean | KeypointTypeNumber | KeypointTypeList;
}

export class ProgressTypeBoolean {
  index: number = 0;
  update: boolean = false;
}

export class ProgressTypeNumber {
  index: number = 0;
  update: number = 0;
}

export class ProgressTypeList {
  index: number = 0;
  update: { name: string; status: boolean }[] = [];
}

export type AgentKeyPointUpdate = ProgressTypeBoolean | ProgressTypeNumber | ProgressTypeList;

export class AgentKeyPointProgress {
  markFinished: number[];
}

export function keyPointTrackingEnabled(): boolean {
  return vConfig.analyze_instruction && vConfig.keypoint_tracking;
}

export class AgentKeyPointsUtils {
  static cloneKeyPoints(points: AgentKeyPoint[] | null): AgentKeyPoint[] | null {
    if (!points) {
      return null;
    }
    const cloned: AgentKeyPoint[] = [];
    for (let i = 0; i < points.length; i++) {
      const item = new AgentKeyPoint();
      item.index = points[i].index;
      item.description = points[i].description;
      item.status = AgentKeyPointsUtils.cloneStatus(points[i].status);
      cloned.push(item);
    }
    return cloned;
  }

  private static cloneStatus(status: KeypointTypeBoolean | KeypointTypeNumber | KeypointTypeList): KeypointTypeBoolean | KeypointTypeNumber | KeypointTypeList {
    if (status instanceof KeypointTypeBoolean) {
      const cloned = new KeypointTypeBoolean();
      cloned.finished = status.finished;
      return cloned;
    }
    if (status instanceof KeypointTypeNumber) {
      const cloned = new KeypointTypeNumber();
      cloned.required = status.required;
      cloned.finished = status.finished;
      return cloned;
    }
    if (status instanceof KeypointTypeList) {
      const cloned = new KeypointTypeList();
      cloned.required = [...status.required];
      cloned.finished = [...status.finished];
      return cloned;
    }
    return status;
  }

  static cloneKeyPointUpdate(update: AgentKeyPointUpdate): AgentKeyPointUpdate {
    if (update instanceof ProgressTypeBoolean) {
      const cloned = new ProgressTypeBoolean();
      cloned.index = update.index;
      cloned.update = update.update;
      return cloned;
    }
    if (update instanceof ProgressTypeNumber) {
      const cloned = new ProgressTypeNumber();
      cloned.index = update.index;
      cloned.update = update.update;
      return cloned;
    }
    if (update instanceof ProgressTypeList) {
      const cloned = new ProgressTypeList();
      cloned.index = update.index;
      cloned.update = update.update.map(item => ({ name: item.name, status: item.status }));
      return cloned;
    }
    return update;
  }

  static formatKeyPointsForPrompt(points: AgentKeyPoint[] | null): string {
    if (!points || points.length === 0) {
      return 'Key points: []';
    }
    const lines: string[] = [
      'Key points:',
    ];
    for (let i = 0; i < points.length; i++) {
      const item = points[i];
      let status: string = '';
      let description: string = '';
      let type = AgentKeyPointsUtils.getKeypointType(item);
      if (item.status instanceof KeypointTypeBoolean) {
        description = item.description;
        status = item.status.finished ? 'finished' : 'unfinished';
      } else if (item.status instanceof KeypointTypeNumber) {
        description = item.description;
        status = `completed: ${item.status.finished} / ${item.status.required}`;
      } else if (item.status instanceof KeypointTypeList) {
        description = item.description;
        const all: Record<number, string> = {};
        const finished: string[] = [];
        const unfinished: string[] = [];
        for (let j = 0; j < item.status.required.length && j < item.status.finished.length; j++) {
          all[j] = item.status.required[j];
          (item.status.finished[j] ? finished : unfinished).push(item.status.required[j]);
        }
        description += ` [Required items]: ${JSON.stringify(all)}`;
        status = `Completed: ${JSON.stringify(finished)}. Uncompleted: ${JSON.stringify(unfinished)}`;
      }
      lines.push(`${i + 1}. [${type}] ${description} [Status]: ${status}`);
    }
    return `${lines.join('\n')}\n`;
  }

  static areAllKeyPointsFinished(points: AgentKeyPoint[] | null): boolean {
    if (!points || points.length === 0) {
      return true;
    }
    for (let i = 0; i < points.length; i++) {
      const status = points[i].status;
      if (status instanceof KeypointTypeBoolean && !status.finished) {
        return false;
      }
      if (status instanceof KeypointTypeNumber && status.finished < status.required) {
        return false;
      }
      if (status instanceof KeypointTypeList && !status.finished.every(Boolean)) {
        return false;
      }
    }
    return true;
  }

  static getKeypointType(point: AgentKeyPoint): string {
    if (point.status instanceof KeypointTypeBoolean) {
      return 'boolean';
    }
    if (point.status instanceof KeypointTypeNumber) {
      return 'number';
    }
    if (point.status instanceof KeypointTypeList) {
      return 'list';
    }
    return 'unknown';
  }

  static applyKeyPointUpdate(points: AgentKeyPoint[], updates: AgentKeyPointUpdate[]): AgentKeyPoint[] | null | Error {
    const ret = AgentKeyPointsUtils.cloneKeyPoints(points);
    if (!ret) {
      return null;
    }
    for (let i = 0; i < updates.length; i++) {
      const update = AgentKeyPointsUtils.cloneKeyPointUpdate(updates[i]);
      update.index -= 1; //输出用1-based
      if (update.index < 0 || update.index >= ret.length) {
        return new Error(`Error parsing keypoint update: found keypoint index ${update.index + 1} exceeding border.`);
      }
      const point = ret[update.index];
      const type = AgentKeyPointsUtils.getKeypointType(point);
      if (update instanceof ProgressTypeBoolean) {
        if (!(point.status instanceof KeypointTypeBoolean)) {
          return new Error(`Error parsing keypoint update: predicted keypoint indexed ${update.index + 1} is of type boolean, but corresponding keypoint is of type ${type}.`);
        }
        point.status.finished = update.update;
      }
      else if (update instanceof ProgressTypeNumber) {
        if (!(point.status instanceof KeypointTypeNumber)) {
          return new Error(`Error parsing keypoint update: predicted keypoint indexed ${update.index + 1} is of type number, but corresponding keypoint is of type ${type}.`);
        }
        point.status.finished = update.update;
      }
      else if (update instanceof ProgressTypeList) {
        if (!(point.status instanceof KeypointTypeList)) {
          return new Error(`Error parsing keypoint update: predicted keypoint indexed ${update.index + 1} is of type list, but corresponding keypoint is of type ${type}.`);
        }
        const error = AgentKeyPointsUtils.applyListKeypointUpdate(update, point.status);
        if (typeof error === 'string') {
          return new Error(error);
        }
      }
    }
    return ret;
  }

  static applyListKeypointUpdate(update: ProgressTypeList, status: KeypointTypeList): string | null {
    for (let j = 0; j < update.update.length; j++) {
      const itemName = update.update[j].name;
      const itemIndex = status.required.indexOf(itemName);
      if (itemIndex < 0 || itemIndex >= status.finished.length) {
        return `Error parsing keypoint update: predicted keypoint has item "${itemName}" not found in required list.`;
      }
      status.finished[itemIndex] = update.update[j].status;
    }
    return null;
  }

  static parseKeyPointsFromAnalyse(raw: string): AgentKeyPoint[] | null {
    const objectText = AgentKeyPointsUtils.extractObjectText(raw);
    if (objectText.length <= 0) {
      return null;
    }
    try {
      const parsed = JSON.parse(objectText) as Record<string, Object>;
      const rawPoints = parsed['Key Points'] ?? parsed.key_points ?? parsed.keyPoints;
      return AgentKeyPointsUtils.normalizeKeyPoints(rawPoints);
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parseKeyPointsFromAnalyse error: %{public}s', e.message);
      return null;
    }
  }

  static stripKeyPointsFromAnalyseText(raw: string): string {
    const objectText = AgentKeyPointsUtils.extractObjectText(raw);
    if (objectText.length <= 0) {
      return raw.trim();
    }
    try {
      const parsed = JSON.parse(objectText) as Record<string, Object>;
      return JSON.stringify(AgentKeyPointsUtils.copyWithoutKeyPoints(parsed), null, 2);
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'stripKeyPointsFromAnalyseText error: %{public}s', e.message);
      return raw.trim();
    }
  }

  private static copyWithoutKeyPoints(source: Record<string, Object>): Record<string, Object> {
    const filtered: Record<string, Object> = {};
    const keys = Object.keys(source);
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      if (key === 'Key Points' || key === 'key_points' || key === 'keyPoints') {
        continue;
      }
      filtered[key] = source[key];
    }
    return filtered;
  }

  static parseKeyPointProgressFromContent(content: string): AgentKeyPointProgress | null {
    const objectText = AgentKeyPointsUtils.extractObjectText(content);
    if (objectText.length <= 0) {
      return null;
    }
    try {
      const parsed = JSON.parse(objectText) as Record<string, Object>;
      const rawProgress = parsed.key_point_progress ?? parsed.keyPointProgress;
      if (typeof rawProgress !== 'object' || rawProgress === null) {
        return AgentKeyPointsUtils.parseTopLevelMarkFinished(parsed);
      }
      return AgentKeyPointsUtils.normalizeKeyPointProgress(rawProgress as Record<string, Object>);
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parseKeyPointProgressFromContent error: %{public}s', e.message);
      return null;
    }
  }

  static parseKeyPointUpdateFromContent(content: string): AgentKeyPointUpdate[] | null {
    const objectText = AgentKeyPointsUtils.extractObjectText(content);
    if (objectText.length <= 0) {
      return null;
    }
    try {
      const parsed = JSON.parse(objectText) as Record<string, unknown>;
      const rawProgress = parsed.key_point_progress ?? parsed.keyPointProgress;
      if (!Array.isArray(rawProgress)) {
        return null;
      }
      return AgentKeyPointsUtils.normalizeKeyPointUpdates(rawProgress);
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parseKeyPointUpdateFromContent error: %{public}s', e.message);
      return null;
    }
  }

  private static normalizeKeyPointUpdates(rawUpdates: unknown[]): AgentKeyPointUpdate[] | null {
    const updates: AgentKeyPointUpdate[] = [];
    for (const rawItem of rawUpdates) {
      if (typeof rawItem !== 'object' || rawItem === null) {
        continue;
      }
      const update = AgentKeyPointsUtils.parseSingleUpdate(rawItem as Record<string, unknown>);
      if (update) {
        updates.push(update);
      }
    }
    return updates.length > 0 ? updates : null;
  }

  private static parseSingleUpdate(rawUpdate: Record<string, unknown>): AgentKeyPointUpdate | null {
    const keys = Object.keys(rawUpdate);
    if (keys.length !== 1) {
      return null;
    }
    const keypointIdStr = keys[0];
    const index = parseInt(keypointIdStr, 10);
    if (isNaN(index) || index <= 0) { // 1-based
      return null;
    }
    const value = rawUpdate[keypointIdStr];
    if (typeof value === 'boolean') {
      const update = new ProgressTypeBoolean();
      update.index = index;
      update.update = value;
      return update;
    }
    if (typeof value === 'number') {
      const update = new ProgressTypeNumber();
      update.index = index;
      update.update = value;
      return update;
    }
    if (!Array.isArray(value)) {
      return null;
    }
    const listItems: { name: string; status: boolean }[] = [];
    for (const item of value) {
      if (typeof item !== 'object' || item === null) {
        continue;
      }
      const itemRecord = item as Record<string, unknown>;
      const itemKeys = Object.keys(itemRecord);
      for (const itemName of itemKeys) {
        const itemStatus = itemRecord[itemName];
        if (typeof itemStatus === 'boolean') {
          listItems.push({ name: itemName, status: itemStatus });
        }
      }
    }
    if (listItems.length > 0) {
      const update = new ProgressTypeList();
      update.index = index;
      update.update = listItems;
      return update;
    }
    return null;
  }

  private static parseTopLevelMarkFinished(parsed: Record<string, Object>): AgentKeyPointProgress | null {
    const rawFinished = parsed.mark_finished ?? parsed.markFinished;
    if (!Array.isArray(rawFinished)) {
      return null;
    }
    const progress = new AgentKeyPointProgress();
    progress.markFinished = AgentKeyPointsUtils.normalizeIndexNumbers(rawFinished);
    return progress.markFinished.length > 0 ? progress : null;
  }

  private static normalizeKeyPointProgress(rawProgress: Record<string, Object>): AgentKeyPointProgress | null {
    const rawFinished = rawProgress.mark_finished ?? rawProgress.markFinished;
    if (!Array.isArray(rawFinished)) {
      return null;
    }
    const progress = new AgentKeyPointProgress();
    progress.markFinished = AgentKeyPointsUtils.normalizeIndexNumbers(rawFinished);
    return progress;
  }

  private static normalizeKeyPoints(rawPoints: Object): AgentKeyPoint[] | null {
    if (!Array.isArray(rawPoints) || rawPoints.length === 0) {
      return null;
    }
    const points: AgentKeyPoint[] = [];
    for (let i = 0; i < rawPoints.length; i++) {
      const rawItem = rawPoints[i];
      const point = new AgentKeyPoint();
      if (typeof rawItem === 'string') {
        const description = AgentKeyPointsUtils.normalizeDescription(rawItem.trim());
        if (description.length <= 0) {
          continue;
        }
        point.description = description;
        point.status = new KeypointTypeBoolean();
        points.push(point);
        continue;
      }
      if (typeof rawItem !== 'object' || rawItem === null) {
        continue;
      }
      const record = rawItem as Record<string, unknown>;
      const description = record.description;
      point.description = typeof description === 'string'
        ? AgentKeyPointsUtils.normalizeDescription(description.trim())
        : '';
      if (point.description.length <= 0) {
        continue;
      }
      const typeValue = record.type;
      point.status = AgentKeyPointsUtils.parseKeypointType(typeValue);
      if (!point.status) {
        point.status = new KeypointTypeBoolean();
      }
      points.push(point);
    }
    if (points.length === 0) {
      return null;
    }
    for (let i = 0; i < points.length; i++) {
      points[i].index = i;
    }
    return points;
  }

  private static parseKeypointType(typeValue: unknown): KeypointTypeBoolean | KeypointTypeNumber | KeypointTypeList | null {
    if (typeValue === 'boolean' || typeValue === undefined || typeValue === null) {
      return new KeypointTypeBoolean();
    }
    if (typeof typeValue === 'object') {
      const typeObj = typeValue as Record<string, unknown>;
      if ('number' in typeObj && typeof typeObj.number === 'number') {
        const numType = new KeypointTypeNumber();
        numType.required = typeObj.number as number;
        numType.finished = 0;
        return numType;
      }
      if ('list' in typeObj && Array.isArray(typeObj.list)) {
        const listType = new KeypointTypeList();
        listType.required = typeObj.list.map(item => String(item));
        listType.finished = new Array(listType.required.length).fill(false);
        return listType;
      }
    }
    return null;
  }

  private static normalizeDescription(description: string): string {
    return AgentKeyPointsUtils.stripPlanPrefix(
      AgentKeyPointsUtils.stripLeadingNumberFromDescription(description)
    );
  }

  private static stripLeadingNumberFromDescription(description: string): string {
    const trimmed = description.trim();
    const numbered = trimmed.match(/^(?:step\s*)?\d+\s*[\.\):\-]\s*(.+)$/i);
    if (numbered && numbered[1]) {
      return numbered[1].trim();
    }
    return trimmed;
  }

  private static stripPlanPrefix(description: string): string {
    const trimmed = description.trim();
    const planPrefix = trimmed.match(/^(?:first|then|next|finally)\s*[,:\-]\s*(.+)$/i);
    if (planPrefix && planPrefix[1]) {
      return planPrefix[1].trim();
    }
    return trimmed
      .replace(/^you\s+(?:need\s+to|should|must)\s+/i, '')
      .replace(/^the\s+agent\s+(?:needs\s+to|should|must)\s+/i, '');
  }

  private static normalizeIndexNumbers(rawValues: Object[]): number[] {
    const result: number[] = [];
    for (let i = 0; i < rawValues.length; i++) {
      const value = rawValues[i];
      if (typeof value !== 'number' || Number.isNaN(value)) {
        continue;
      }
      const indexNum = Math.trunc(value);
      if (indexNum <= 0 || result.indexOf(indexNum) >= 0) {
        continue;
      }
      result.push(indexNum);
    }
    return result;
  }

  private static stripCodeFence(content: string): string {
    let normalized = content.trim();
    if (!normalized.startsWith('```')) {
      return normalized;
    }
    const firstLineEnd = normalized.indexOf('\n');
    if (firstLineEnd >= 0) {
      normalized = normalized.substring(firstLineEnd + 1);
    }
    if (normalized.endsWith('```')) {
      normalized = normalized.substring(0, normalized.length - 3);
    }
    return normalized.trim();
  }

  private static extractObjectText(content: string): string {
    const normalized = AgentKeyPointsUtils.stripCodeFence(content);
    const start = normalized.indexOf('{');
    if (start < 0) {
      return normalized;
    }
    let depth = 0;
    for (let i = start; i < normalized.length; i++) {
      const currentChar = normalized.charAt(i);
      if (currentChar === '{') {
        depth += 1;
      } else if (currentChar === '}') {
        depth -= 1;
        if (depth === 0) {
          return normalized.substring(start, i + 1);
        }
      }
    }
    return normalized.substring(start);
  }
}
