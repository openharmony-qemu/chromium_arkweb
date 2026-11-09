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

// 纯函数工具集，对齐 mem0/mem0/memory/main.py 中的若干 helper：
//   _extract_tool_calls_from_messages / _deduplicate_tool_calls / _make_history_key
//   _build_payload / _append_source_trace / _update_content_history_summary
//   _iter_calls_from_history / _collect_tool_candidates / _prune_tool_candidates
//   _assign_tool_refs / _dedupe_selected_calls / _build_fallback_query

import { MemoryItemModel, ToolCallEntry, StoredMemoryPayload } from '../domain/MemoryItemModel';
import { MemoryMetaDataModel, MemorySourceTraceEntry } from '../domain/MemoryMetaDataModel';
import { formatHistorySummary } from '../prompts/MemoryDecisionPrompts';
import {hilog} from '@kit.PerformanceAnalysisKit';
const DOMAIN = 0x6020;
const TAG = 'MemoryHelpers';

// ============================================================================
// 工具调用抽取 / 去重
// ============================================================================

// 与 _extract_tool_calls_from_messages 等价：从 role=tool 的 message 中收集 tool 实体。
// 端上 MemoryPayloadBuilder 已经把 toolMsg.content = [MemoryToolEntry{name, args, argsConcreteHint, result}]，
// 因此每条 tool 消息的 content 就是一个数组。
export function extractToolCallsFromMessages(messages: Object[]): ToolCallEntry[] {
  const out: ToolCallEntry[] = [];
  if (!messages || messages.length <= 0) {
    return out;
  }
  for (let i = 0; i < messages.length; i++) {
    const m = messages[i];
    if (typeof m !== 'object' || m === null) {
      continue;
    }
    const msg = m as Record<string, Object>;
    if (msg.role !== 'tool') {
      continue;
    }
    const content = msg.content;
    extractAndPushToolCalls(content, out);
  }
  return out;
}

function extractAndPushToolCalls(content: Object, out: ToolCallEntry[]): void {
  if (Array.isArray(content)) {
    pushToolCallsFromArray(content as Object[], out);
  } else if (typeof content === 'object' && content !== null) {
    const entry = toToolCallEntry(content);
    if (entry) {
      out.push(entry);
    }
  } else if (typeof content === 'string') {
    pushStringToolCall(content as string, out);
  }
}

function pushToolCallsFromArray(arr: Object[], out: ToolCallEntry[]): void {
  for (let j = 0; j < arr.length; j++) {
    const entry = toToolCallEntry(arr[j]);
    if (entry) {
      out.push(entry);
    }
  }
}

function pushStringToolCall(name: string, out: ToolCallEntry[]): void {
  // 旧形态：role=tool 的 content 直接是一个字符串（tool 名）
  if (name.length > 0) {
    const entry = new ToolCallEntry();
    entry.name = name;
    out.push(entry);
  }
}

function toToolCallEntry(value: Object): ToolCallEntry | null {
  if (typeof value !== 'object' || value === null) {
    return null;
  }
  const obj = value as Record<string, Object>;
  const name = (typeof obj.name === 'string') ? obj.name as string :
    ((typeof obj.tool === 'string') ? obj.tool as string : '');
  if (name.length <= 0) {
    return null;
  }
  const entry = new ToolCallEntry();
  entry.name = name;
  if (typeof obj.args === 'object' && obj.args !== null && !Array.isArray(obj.args)) {
    entry.args = obj.args as Record<string, Object>;
  }
  if (typeof obj.argsConcreteHint === 'object' && obj.argsConcreteHint !== null &&
    !Array.isArray(obj.argsConcreteHint)) {
    entry.argsConcreteHint = obj.argsConcreteHint as Record<string, Object>;
  }
  if (typeof obj.result === 'string') {
    entry.result = obj.result as string;
  }
  return entry;
}

// 与 _deduplicate_tool_calls 等价：按 (name, location/room) 去重，后入覆盖前者。
// 浏览器场景下大部分 tool 没有 location/room，回退为按 name 去重。
export function deduplicateToolCalls(toolCalls: ToolCallEntry[]): ToolCallEntry[] {
  if (!toolCalls || toolCalls.length <= 0) {
    return [];
  }
  const dedup: Map<string, ToolCallEntry> = new Map();
  for (let i = 0; i < toolCalls.length; i++) {
    const t = toolCalls[i];
    const args = t.args ?? {};
    let location = '';
    if (typeof args.location === 'string') {
      location = args.location as string;
    } else if (typeof args.room === 'string') {
      location = args.room as string;
    }
    const key = location.length > 0 ? `${t.name}:${location}` : t.name;
    dedup.set(key, t);
  }
  return Array.from(dedup.values());
}

// 与 _make_history_key 等价：把 tool_calls 规范化（仅保留 name+args），按"以排序后的对象 JSON 字符串
// 作为排序键"做稳定排序，再以 sortedKeys 形式 JSON.stringify 输出。
export function makeHistoryKey(toolCalls: ToolCallEntry[]): string {
  const normalized: Object[] = [];
  for (let i = 0; i < toolCalls.length; i++) {
    const t = toolCalls[i];
    const item: Record<string, Object> = {};
    item.name = t.name;
    item.args = (t.args ?? {}) as Record<string, Object>;
    normalized.push(item);
  }
  normalized.sort((a: Object, b: Object): number => {
    const sa = stableStringify(a);
    const sb = stableStringify(b);
    if (sa === sb) {
      return 0;
    }
    return sa < sb ? -1 : 1;
  });
  return stableStringify(normalized);
}

// 与 Python json.dumps(..., sort_keys=True, ensure_ascii=False) 等价的稳定序列化：递归对对象 key 排序。
export function stableStringify(value: Object): string {
  if (value === null || value === undefined) {
    return 'null';
  }
  if (typeof value === 'string') {
    return JSON.stringify(value);
  }
  if (typeof value === 'number' || typeof value === 'boolean') {
    return JSON.stringify(value);
  }
  if (Array.isArray(value)) {
    const arr = value as Object[];
    const parts: string[] = [];
    for (let i = 0; i < arr.length; i++) {
      parts.push(stableStringify(arr[i]));
    }
    return '[' + parts.join(',') + ']';
  }
  if (typeof value === 'object') {
    const obj = value as Record<string, Object>;
    const keys: string[] = Object.keys(obj).sort();
    const parts: string[] = [];
    for (let i = 0; i < keys.length; i++) {
      const k = keys[i];
      parts.push(JSON.stringify(k) + ':' + stableStringify(obj[k]));
    }
    return '{' + parts.join(',') + '}';
  }
  return JSON.stringify(value);
}

// ============================================================================
// Payload 构建 / sourceTrace / 历史统计文本
// ============================================================================

// 与 _build_payload 等价：把 MemoryItemModel 拍平为 StoredMemoryPayload。
export function buildPayload(item: MemoryItemModel): StoredMemoryPayload {
  const p = new StoredMemoryPayload();
  p.userId = item.metadata.userId;
  p.agentId = item.metadata.agentId;
  p.type = item.metadata.type;
  p.subType = item.metadata.subType;
  p.subSubType = item.metadata.subSubType;
  p.title = item.title;
  p.content = item.content;
  p.summarizedTitle = item.metadata.summarizedTitle;
  p.createdAt = item.metadata.createdAt;
  p.updatedAt = item.metadata.updatedAt;
  p.oldMemory = item.metadata.oldMemory;
  p.scenarioId = item.metadata.scenarioId;
  p.toolsJson = JSON.stringify(toolEntriesToObjects(item.tools));
  p.historyJson = JSON.stringify(item.history ?? {});
  p.sourceTraceJson = JSON.stringify(item.metadata.sourceTrace ?? []);
  return p;
}

function toolEntriesToObjects(tools: ToolCallEntry[]): Object[] {
  const out: Object[] = [];
  if (!tools) {
    return out;
  }
  for (let i = 0; i < tools.length; i++) {
    const t = tools[i];
    const o: Record<string, Object> = {};
    o.name = t.name;
    o.args = (t.args ?? {}) as Record<string, Object>;
    if (Object.keys(t.argsConcreteHint ?? {}).length > 0) {
      o.argsConcreteHint = t.argsConcreteHint as Record<string, Object>;
    }
    if (t.result && t.result.length > 0) {
      o.result = t.result;
    }
    out.push(o);
  }
  return out;
}

// 与 _append_source_trace + _build_source_trace_entry 等价：仅当 metadata.scenarioId 存在时追加。
export function appendSourceTrace(
  payload: StoredMemoryPayload,
  scenarioId: string,
  operation: string,
  queryType: string,
  originalTurns: number,
  maxEntries: number = 50
): StoredMemoryPayload {
  if (!scenarioId || scenarioId.length <= 0) {
    return payload;
  }
  const entry = new MemorySourceTraceEntry();
  entry.scenarioId = scenarioId;
  entry.operation = operation;
  entry.timestamp = MemoryMetaDataModel.nowIso();
  if (queryType.length > 0) {
    entry.queryType = queryType;
  }
  if (originalTurns > 0) {
    entry.originalTurns = originalTurns;
  }
  // 解析既有 sourceTraceJson
  let existing: Object[] = [];
  try {
    const parsed = JSON.parse(payload.sourceTraceJson);
    if (Array.isArray(parsed)) {
      existing = parsed as Object[];
    }
  } catch (e) {
    hilog.warn(DOMAIN, TAG, 'Helpers appendSourceTrace failed: %{public}s', e.message);
    existing = [];
  }
  existing.push(entry);
  if (existing.length > maxEntries) {
    existing = existing.slice(existing.length - maxEntries);
  }
  payload.sourceTraceJson = JSON.stringify(existing);
  payload.scenarioId = scenarioId;
  return payload;
}

// 与 _update_content_history_summary 等价：在 content 中替换/追加 "## 历史统计" 段落。
export function updateContentHistorySummary(content: string, history: Record<string, number>): string {
  const summary = formatHistorySummary(history);
  const mdMarker = '## 历史统计';
  const mdIdx = content.indexOf(mdMarker);
  if (mdIdx >= 0) {
    let after = content.substring(mdIdx + mdMarker.length);
    // 跳过 marker 后的换行
    while (after.length > 0 && after.charAt(0) === '\n') {
      after = after.substring(1);
    }
    const nextMatch = after.match(/\n## /);
    if (nextMatch && nextMatch.index !== undefined) {
      const tail = after.substring(nextMatch.index);
      return content.substring(0, mdIdx) + `## 历史统计\n${summary}` + tail;
    }
    return content.substring(0, mdIdx) + `## 历史统计\n${summary}`;
  }

  // 旧扁平格式
  const flatMarker = '历史统计：';
  const flatIdx = content.indexOf(flatMarker);
  if (flatIdx >= 0) {
    const after = content.substring(flatIdx + flatMarker.length);
    const boundary = after.match(/(?:可复用性|最近操作|设备操作)/);
    if (boundary && boundary.index !== undefined) {
      let tail = after.substring(boundary.index);
      // strip leading 。；; 空白 \n
      while (tail.length > 0 && (tail.charAt(0) === '。' || tail.charAt(0) === '；' ||
        tail.charAt(0) === ';' || tail.charAt(0) === ' ' || tail.charAt(0) === '\n')) {
        tail = tail.substring(1);
      }
      return content.substring(0, flatIdx + flatMarker.length) + summary + '。' + tail;
    }
    return content.substring(0, flatIdx + flatMarker.length) + summary + '。';
  }

  // 两种标记都没找到：根据是否已是 markdown 决定追加方式
  if (content.indexOf('## ') >= 0) {
    return content.replace(/\s+$/, '') + `\n\n## 历史统计\n${summary}`;
  }
  return content.replace(/\s+$/, '') + ` 历史统计：${summary}。`;
}

// ============================================================================
// summary 阶段：候选工具收集 / 修剪 / 去重 / tool_ref 分配
// ============================================================================

export class ToolCandidate {
  public name: string = '';
  public args: Record<string, Object> = {};
  public contextMatchScore: number = 0;
  public supportCount: number = 0;
  public hasLatest: boolean = false;
  public memoryIndex: number = 0;
  public toolRef: string = '';
}

// 与 _iter_calls_from_history 等价：从 history map 展开每条 (name, args, count)。
export function iterCallsFromHistory(historyMap: Record<string, number>): Object[] {
  const out: Object[] = [];
  if (!historyMap) {
    return out;
  }
  const keys: string[] = Object.keys(historyMap);
  for (let i = 0; i < keys.length; i++) {
    const keyStr = keys[i];
    const count = historyMap[keyStr] ?? 0;
    let parsed: Object;
    try {
      parsed = JSON.parse(keyStr);
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'Helpers iterCallsFromHistory failed: %{public}s', e.message);
      continue;
    }
    let arr: Object[];
    if (Array.isArray(parsed)) {
      arr = parsed as Object[];
    } else if (typeof parsed === 'object' && parsed !== null) {
      arr = [parsed];
    } else {
      continue;
    }
    const freq = Math.max(count, 1);
    for (let j = 0; j < arr.length; j++) {
      const item = arr[j];
      if (typeof item !== 'object' || item === null) {
        continue;
      }
      const obj = item as Record<string, Object>;
      const name = (typeof obj.name === 'string') ? obj.name as string : '';
      const args = (typeof obj.args === 'object' && obj.args !== null && !Array.isArray(obj.args))
        ? obj.args as Record<string, Object>
        : ({} as Record<string, Object>);
      if (name.length > 0 && Object.keys(args).length > 0) {
        const tup: Record<string, Object> = {};
        tup.name = name;
        tup.args = args;
        tup.freq = freq;
        out.push(tup);
      }
    }
  }
  return out;
}

// 计算上下文匹配分数
function calculateContextMatch(meta: Record<string, Object>, contextMetadata: Record<string, Object>): number {
  let contextMatch = 0;
  const ctxKeys: string[] = Object.keys(contextMetadata);
  for (let i = 0; i < ctxKeys.length; i++) {
    const k = ctxKeys[i];
    const v = contextMetadata[k];
    if (v && meta[k] === v) {
      contextMatch += 1;
    }
  }
  return contextMatch;
}

// 从 memory 提取 history map
function extractHistoryMap(mem: Record<string, Object>, meta: Record<string, Object>): Record<string, number> {
  const hRaw = mem.history;
  if (typeof hRaw === 'object' && hRaw !== null && !Array.isArray(hRaw)) {
    return hRaw as Record<string, number>;
  }
  const metaH = meta.history;
  if (typeof metaH === 'object' && metaH !== null && !Array.isArray(metaH)) {
    return metaH as Record<string, number>;
  }
  return {};
}

// 从 function_calling_result 提取有效的工具调用对象
function extractValidToolCall(fc: Object): { name: string; args: Record<string, Object> } | null {
  if (typeof fc !== 'object' || fc === null) {
    return null;
  }
  const fcObj = fc as Record<string, Object>;
  const name = (typeof fcObj.name === 'string') ? fcObj.name as string : '';
  if (name.length <= 0) {
    return null;
  }
  const args = (typeof fcObj.args === 'object' && fcObj.args !== null && !Array.isArray(fcObj.args))
    ? fcObj.args as Record<string, Object>
    : ({} as Record<string, Object>);
  return { name, args };
}

// 更新或添加 candidate
function updateOrAddCandidate(
  bySig: Map<string, ToolCandidate>,
  sig: string,
  name: string,
  args: Record<string, Object>,
  contextMatch: number,
  memIdx: number,
  supportIncrement: number,
  hasLatest: boolean
): void {
  const existing = bySig.get(sig);
  const cand = existing ?? new ToolCandidate();
  if (!existing) {
    cand.name = name;
    cand.args = args;
    cand.contextMatchScore = contextMatch;
    cand.memoryIndex = memIdx;
  }
  cand.supportCount += supportIncrement;
  if (hasLatest) {
    cand.hasLatest = true;
  }
  if (contextMatch > cand.contextMatchScore) {
    cand.contextMatchScore = contextMatch;
    cand.memoryIndex = memIdx;
  }
  bySig.set(sig, cand);
}

// 处理 latest tools
function processLatestTools(
  mem: Record<string, Object>,
  contextMatch: number,
  memIdx: number,
  bySig: Map<string, ToolCandidate>
): void {
  const fcArr = (Array.isArray(mem.function_calling_result) ? mem.function_calling_result : []) as Object[];
  for (let i = 0; i < fcArr.length; i++) {
    const toolCall = extractValidToolCall(fcArr[i]);
    if (!toolCall) {
      continue;
    }
    const sig = stableStringify({ name: toolCall.name, args: toolCall.args } as Object);
    updateOrAddCandidate(bySig, sig, toolCall.name, toolCall.args, contextMatch, memIdx, 1, true);
  }
}

// 处理 history variants
function processHistoryVariants(
  historyMap: Record<string, number>,
  contextMatch: number,
  memIdx: number,
  bySig: Map<string, ToolCandidate>
): void {
  const hist = iterCallsFromHistory(historyMap);
  for (let i = 0; i < hist.length; i++) {
    const h = hist[i] as Record<string, Object>;
    const name = h.name as string;
    const args = h.args as Record<string, Object>;
    const freq = h.freq as number;
    const sig = stableStringify({ name, args } as Object);
    updateOrAddCandidate(bySig, sig, name, args, contextMatch, memIdx, freq, false);
  }
}

// 与 _collect_tool_candidates 等价：按 name+args 聚合，融合 latest tools 与 history。
export function collectToolCandidates(
  memories: Record<string, Object>[],
  contextMetadata: Record<string, Object>
): ToolCandidate[] {
  const bySig: Map<string, ToolCandidate> = new Map();
  for (let memIdx = 0; memIdx < memories.length; memIdx++) {
    const mem = memories[memIdx];
    const meta = (typeof mem.metadata === 'object' && mem.metadata !== null)
      ? mem.metadata as Record<string, Object> : ({} as Record<string, Object>);
    const contextMatch = calculateContextMatch(meta, contextMetadata);
    processLatestTools(mem, contextMatch, memIdx, bySig);
    const historyMap = extractHistoryMap(mem, meta);
    processHistoryVariants(historyMap, contextMatch, memIdx, bySig);
  }
  const candidates: ToolCandidate[] = Array.from(bySig.values());
  candidates.sort((a: ToolCandidate, b: ToolCandidate): number => {
    const al = a.hasLatest ? 1 : 0;
    const bl = b.hasLatest ? 1 : 0;
    if (al !== bl) {
      return bl - al;
    }
    if (a.contextMatchScore !== b.contextMatchScore) {
      return b.contextMatchScore - a.contextMatchScore;
    }
    return b.supportCount - a.supportCount;
  });
  return candidates;
}

// 与 _prune_tool_candidates 等价。
export function pruneToolCandidates(
  candidates: ToolCandidate[],
  maxTotal: number = 30,
  maxPerTool: number = 4
): ToolCandidate[] {
  if (!candidates || candidates.length <= 0) {
    return [];
  }
  const grouped: Map<string, number> = new Map();
  const out: ToolCandidate[] = [];
  for (let i = 0; i < candidates.length; i++) {
    const c = candidates[i];
    if (!c.name || c.name.length <= 0) {
      continue;
    }
    const cur = grouped.get(c.name) ?? 0;
    if (cur >= maxPerTool) {
      continue;
    }
    out.push(c);
    grouped.set(c.name, cur + 1);
    if (out.length >= maxTotal) {
      break;
    }
  }
  return out;
}

// 与 _assign_tool_refs 等价：编号 T1, T2, ...
export function assignToolRefs(candidates: ToolCandidate[]): ToolCandidate[] {
  for (let i = 0; i < candidates.length; i++) {
    candidates[i].toolRef = `T${i + 1}`;
  }
  return candidates;
}

// 与 _dedupe_selected_calls 等价：按 (name, args) 签名去重。
export function dedupeSelectedCalls(calls: Object[]): Object[] {
  const seen: Set<string> = new Set();
  const out: Object[] = [];
  if (!calls) {
    return out;
  }
  for (let i = 0; i < calls.length; i++) {
    const c = calls[i];
    if (typeof c !== 'object' || c === null) {
      continue;
    }
    const obj = c as Record<string, Object>;
    const name = (typeof obj.name === 'string') ? obj.name as string : '';
    if (name.length <= 0) {
      continue;
    }
    const args = (typeof obj.args === 'object' && obj.args !== null && !Array.isArray(obj.args))
      ? obj.args as Record<string, Object>
      : ({} as Record<string, Object>);
    const sig = stableStringify({ name, args } as Object);
    if (seen.has(sig)) {
      continue;
    }
    seen.add(sig);
    const item: Record<string, Object> = {};
    item.name = name;
    item.args = args;
    out.push(item);
  }
  return out;
}

// 与 _build_fallback_query 等价：把 context 中可读字段拼到 query 前。
export function buildFallbackQuery(query: string, contextMetadata: Record<string, Object>,
  useContext: boolean): string {
  if (!useContext || !contextMetadata) {
    return query;
  }
  const skipKeys: Set<string> = new Set([
    'userId', 'agentId', 'sessionId', 'searchPreset', 'searchConfig',
    'contextMetadata', 'conversationHistory', 'limit', 'threshold', 'enableTopicSegmentation'
  ]);
  const parts: string[] = [];
  const keys: string[] = Object.keys(contextMetadata);
  for (let i = 0; i < keys.length; i++) {
    const k = keys[i];
    if (skipKeys.has(k)) {
      continue;
    }
    const v = contextMetadata[k];
    if (typeof v === 'string' && (v as string).length > 0) {
      parts.push(v as string);
    } else if (typeof v === 'number') {
      parts.push(`${v}`);
    }
  }
  if (parts.length <= 0) {
    return query;
  }
  return `${parts.join(' ')} ${query}`.trim();
}

// 提取消息列表中第一条 user 消息的 content 作为"原始用户问句"。
export function extractFirstUserMessageContent(messages: Object[]): string {
  if (!messages) {
    return '';
  }
  for (let i = 0; i < messages.length; i++) {
    const m = messages[i];
    if (typeof m !== 'object' || m === null) {
      continue;
    }
    const obj = m as Record<string, Object>;
    if (obj.role !== 'user') {
      continue;
    }
    const content = obj.content;
    if (typeof content === 'string') {
      return content as string;
    }
    if (Array.isArray(content)) {
      return extractTextFromArray(content as Object[]);
    }
  }
  return '';
}

function extractTextFromArray(arr: Object[]): string {
  for (let j = 0; j < arr.length; j++) {
    const part = arr[j];
    if (typeof part === 'string') {
      return part as string;
    }
    if (typeof part === 'object' && part !== null) {
      const pobj = part as Record<string, Object>;
      if (typeof pobj.text === 'string') {
        return pobj.text as string;
      }
    }
  }
  return '';
}
