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

// AddPipeline：浏览器程序记忆写入流水线（端上版本）。
//
// 对齐 Python mem0/mem0/memory/main.py 中：
//   add_memory → _extract_procedural_memory → _retrieve_memory(候选) → _updated_procedural_memory(LLM 四元)
//   → _handle_{add,update,delete,noop}_event
//
// 端侧最小阶段：
//   - 固定关闭话题切分（enableTopicSegmentation=false）
//   - 候选检索改用 BM25(title) 替代 vector 检索（plan §0：BM25-only）
//   - LLM 抽取仍走 BROWSER_ADD_PROCEDURAL_MEMORY_PROMPT；title 是否取 user 原话由 useRawUserQueryAsTitle 决定

import { hilog } from '@kit.PerformanceAnalysisKit';
import { MemoryItemModel, ToolCallEntry, StoredMemoryPayload, StoredMemoryView } from '../domain/MemoryItemModel';
import { MemoryMetaDataModel } from '../domain/MemoryMetaDataModel';
import { MemoryEvent, MemoryTypeName, isValidCategory, defaultCategory, CATEGORY_PREFIX } from '../domain/Constants';
import { MemoryStore, MemoryStoreFilters } from '../store/MemoryStore';
import { MemoryLLMAdapter } from '../llm/MemoryLLMAdapter';
import { MemoryLocalConfig } from '../config/MemoryLocalConfig';
import { Bm25 } from '../retrieval/Bm25';
import { BROWSER_ADD_PROCEDURAL_MEMORY_PROMPT } from '../prompts/BrowserMemoryPrompts';
import { buildProceduralMemoryUpdateMessages, OldMemoryEntry, formatHistorySummary } from '../prompts/MemoryDecisionPrompts';
import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { util } from '@kit.ArkTS';
import {
  extractToolCallsFromMessages,
  deduplicateToolCalls,
  makeHistoryKey,
  buildPayload,
  appendSourceTrace,
  updateContentHistorySummary,
  extractFirstUserMessageContent,
  stableStringify
} from './Helpers';

const DOMAIN = 0x6020;
const TAG = 'MemoryAddPipeline';

export class AddRequest {
  public userId: string = '';
  public agentId: string = '';
  public messages: Object[] = [];
  // 顶层 toolCalls（来自 MemoryPayloadBuilder 的 buildToolCalls 输出，argsJson/argsConcreteHintJson 是字符串）
  public topLevelToolCalls: Object[] = [];
  public metadata: Record<string, Object> = {};
}

export class AddResult {
  public results: Object[] = [];
  public segmentationInfo: Record<string, Object> = {};
}

export class AddPipeline {
  private llm: MemoryLLMAdapter;
  private config: MemoryLocalConfig;

  constructor(llm: MemoryLLMAdapter, config: MemoryLocalConfig) {
    this.llm = llm;
    this.config = config;
  }

  async run(req: AddRequest): Promise<AddResult> {
    const out = new AddResult();
    out.segmentationInfo.enabled = false;

    if (!req.messages || req.messages.length <= 0) {
      hilog.info(DOMAIN, TAG, 'add skipped: empty messages');
      return out;
    }

    // ---------- Step 1：LLM 抽取 title/content/category ----------
    const candidate = await this.extractProceduralMemory(req);
    if (!candidate) {
      hilog.info(DOMAIN, TAG, 'add skipped: extract returned empty');
      return out;
    }

    // ---------- Step 2：BM25 候选检索（同 user / agent / type=procedural） ----------
    const filters = new MemoryStoreFilters();
    filters.userId = req.userId;
    filters.agentId = req.agentId;
    filters.type = MemoryTypeName.PROCEDURAL;
    const allDocs = MemoryStore.list(filters, -1);
    const bm25Hits = Bm25.search(candidate.title, allDocs, 'title', this.config.addRetrieveCandidateTopK);
    hilog.debug(DOMAIN, TAG,
      'candidate retrieve user=%{private}s agent=%{private}s docs=%{private}d bm25Hits=%{private}d', req.userId,
      req.agentId, allDocs.length, bm25Hits.length);
    const oldEntries: OldMemoryEntry[] = [];
    const oldHits: StoredMemoryView[] = [];
    for (let i = 0; i < bm25Hits.length; i++) {
      const view: StoredMemoryView = {
        id: bm25Hits[i].id,
        payload: bm25Hits[i].payload
      } as StoredMemoryView;
      oldHits.push(view);
      const entry = new OldMemoryEntry();
      entry.id = view.id;
      entry.index = i;
      const payload = view.payload;
      const summarized = (typeof payload.summarizedTitle === 'string') ? payload.summarizedTitle as string : '';
      const title = (typeof payload.title === 'string') ? payload.title as string : '';
      entry.title = summarized.length > 0 ? summarized : title;
      entry.content = (typeof payload.content === 'string') ? payload.content as string : '';
      const tools = payload.tools;
      if (Array.isArray(tools)) {
        entry.lastOperation = tools as Object[];
      }
      const hist = payload.history;
      if (typeof hist === 'object' && hist !== null && !Array.isArray(hist)) {
        entry.historySummary = formatHistorySummary(hist as Record<string, number>);
      }
      oldEntries.push(entry);
    }

    // ---------- Step 3：LLM 四元决策 ----------
    const effectiveTitle = candidate.metadata.summarizedTitle.length > 0
      ? candidate.metadata.summarizedTitle
      : candidate.title;
    const newToolsObjects = AddPipeline.toolEntriesToObjects(candidate.tools);
    const decisionPrompt = buildProceduralMemoryUpdateMessages(
      oldEntries,
      effectiveTitle,
      candidate.content,
      newToolsObjects,
      ''
    );
    let action: string = MemoryEvent.ADD;
    let targetIndex: number = -1;
    let llmTitle: string = effectiveTitle;
    let llmContent: string = candidate.content;
    let decisionReason: string = '';
    const decisionResp = await this.llm.runJson(decisionPrompt, '请仅返回 JSON 决策对象。', 'memory_decision');
    if (decisionResp.ok && decisionResp.json) {
      const j = decisionResp.json;
      const rawAction = (typeof j.action === 'string') ? (j.action as string).toUpperCase() : '';
      if (rawAction === 'ADD' || rawAction === 'UPDATE' || rawAction === 'DELETE' || rawAction === 'NONE') {
        action = rawAction;
      }
      if (typeof j.target_index === 'number') {
        targetIndex = j.target_index as number;
      }
      if (typeof j.title === 'string') {
        llmTitle = j.title as string;
      }
      if (typeof j.content === 'string') {
        llmContent = j.content as string;
      }
      if (typeof j.reason === 'string') {
        decisionReason = j.reason as string;
      }
    } else {
      hilog.warn(DOMAIN, TAG, 'decision LLM failed, fallback to ADD: %{private}s', decisionResp.errorMessage);
    }

    // 工具组一致性兜底：若 UPDATE/NONE 但工具名集合无交集，强制 ADD（与 Python 一致）
    if ((action === MemoryEvent.UPDATE || action === MemoryEvent.NONE) &&
      targetIndex >= 0 && targetIndex < oldEntries.length) {
      const oldNames = AddPipeline.toolNameSet(oldEntries[targetIndex].lastOperation);
      const newNames = AddPipeline.toolNameSet(newToolsObjects);
      if (oldNames.size > 0 && newNames.size > 0 && AddPipeline.disjoint(oldNames, newNames)) {
        hilog.debug(DOMAIN, TAG, 'tool-group guard: forcing ADD (old=%{private}s vs new=%{private}s)',
          AddPipeline.setStr(oldNames), AddPipeline.setStr(newNames));
        action = MemoryEvent.ADD;
        decisionReason = `工具名集合无交集，降级为 ADD`;
      }
    }

    hilog.debug(DOMAIN, TAG, 'decision action=%{private}s targetIndex=%{private}d reason=%{private}s',
      action, targetIndex, decisionReason);

    // ---------- Step 4：handler dispatch ----------
    if (action === MemoryEvent.ADD) {
      const final = AddPipeline.applyLlmTitleContent(
        candidate, llmTitle, llmContent, this.config.useRawUserQueryAsTitle
      );
      const memId = await this.handleAdd(final, req.metadata);
      const r: Record<string, Object> = {};
      r.event = 'ADD';
      r.memory_item = AddPipeline.serializeMemoryItem(final, memId);
      r.proceduralTitle = final.title;
      out.results.push(r);
    } else if (action === MemoryEvent.UPDATE) {
      const idx = (targetIndex >= 0 && targetIndex < oldEntries.length) ? targetIndex : 0;
      const target = oldHits[idx];
      const final = AddPipeline.applyLlmTitleContent(
        candidate, llmTitle, llmContent, this.config.useRawUserQueryAsTitle
      );
      const memId = await this.handleUpdate(final, target, req.metadata);
      const r: Record<string, Object> = {};
      r.event = 'UPDATE';
      r.memory_item = AddPipeline.serializeMemoryItem(final, memId);
      r.proceduralTitle = final.title;
      out.results.push(r);
    } else if (action === MemoryEvent.DELETE) {
      const idx = (targetIndex >= 0 && targetIndex < oldEntries.length) ? targetIndex : 0;
      const target = oldHits[idx];
      const memId = await this.handleDelete(target);
      const r: Record<string, Object> = {};
      r.event = 'DELETE';
      r.memory_item = AddPipeline.serializeStoredView(target, memId);
      out.results.push(r);
    } else {
      // NONE：仅合并 history 到 target，不改变 title/content
      const idx = (targetIndex >= 0 && targetIndex < oldEntries.length) ? targetIndex : -1;
      if (idx < 0) {
        hilog.info(DOMAIN, TAG, 'NOOP: no target, skip');
      } else {
        const target = oldHits[idx];
        const memId = await this.handleNoop(candidate, target, req.metadata);
        const r: Record<string, Object> = {};
        r.event = 'NONE';
        r.memory_item = AddPipeline.serializeStoredView(target, memId);
        out.results.push(r);
      }
    }
    return out;
  }

  // ============================================================================
  // 抽取
  // ============================================================================

  private async extractProceduralMemory(req: AddRequest): Promise<MemoryItemModel | null> {
    const messages = AddPipeline.preprocessMessages(req.messages);
    const toolCallsRaw = extractToolCallsFromMessages(messages);
    const toolCalls = this.config.deduplicateToolCallsOnExtract
      ? deduplicateToolCalls(toolCallsRaw)
      : toolCallsRaw;
    const history: Record<string, number> = {};
    if (toolCalls.length > 0) {
      const key = makeHistoryKey(toolCalls);
      history[key] = 1;
    }

    const userPrompt = '以下是浏览器Agent完成单次任务的完整执行轨迹（pre-clustered 后），请按照系统提示总结：\n\n' +
      AddPipeline.serializeMessages(messages);
    const resp = await this.llm.runJson(BROWSER_ADD_PROCEDURAL_MEMORY_PROMPT, userPrompt, 'memory_extract');
    if (!resp.ok || !resp.json) {
      hilog.warn(DOMAIN, TAG, 'extract LLM failed: %{private}s', resp.errorMessage);
      return null;
    }
    const j = resp.json;
    const rawTitle = (typeof j.title === 'string') ? j.title as string : '';
    const content = (typeof j.content === 'string') ? j.content as string : '';
    let category = (typeof j.category === 'string') ? j.category as string : defaultCategory();
    if (!isValidCategory(category)) {
      category = defaultCategory();
    }
    if (rawTitle.length <= 0 || content.length <= 0) {
      hilog.warn(DOMAIN, TAG, 'extract returned empty title/content');
      return null;
    }

    // 拼 category 中文前缀
    const prefix = CATEGORY_PREFIX[category] ?? '';
    const summarizedTitle = (prefix.length > 0 && rawTitle.indexOf(prefix) !== 0)
      ? `${prefix} ${rawTitle}` : rawTitle;

    let finalTitle: string;
    let finalSummarized: string;
    if (this.config.useRawUserQueryAsTitle) {
      const firstUser = extractFirstUserMessageContent(messages);
      finalTitle = firstUser.length > 0 ? firstUser.trim() : summarizedTitle;
      finalSummarized = summarizedTitle;
    } else {
      finalTitle = summarizedTitle;
      finalSummarized = '';
    }

    const item = new MemoryItemModel();
    item.title = finalTitle;
    item.content = content;
    item.tools = toolCalls;
    item.history = history;
    const meta = MemoryMetaDataModel.create(req.userId, req.agentId);
    meta.type = MemoryTypeName.PROCEDURAL;
    meta.subType = '程序记忆';
    meta.subSubType = category;
    meta.summarizedTitle = finalSummarized;
    if (typeof req.metadata.scenarioId === 'string') {
      meta.scenarioId = req.metadata.scenarioId as string;
    }
    item.metadata = meta;
    return item;
  }

  // ============================================================================
  // event handlers
  // ============================================================================

  private async handleAdd(item: MemoryItemModel, metadata: Record<string, Object>): Promise<string> {
    let payload = buildPayload(item);
    payload = appendSourceTrace(payload, payload.scenarioId, 'ADD',
      AddPipeline.readString(metadata, 'queryType'),
      AddPipeline.readNumber(metadata, 'originalTurns'));
    if (Object.keys(item.history).length > 0) {
      payload.content = updateContentHistorySummary(payload.content, item.history);
    }
    const memId = await AddPipeline.titleToId(payload.title);
    item.memId = memId;
    await MemoryStore.insert(memId, payload);
    hilog.debug(DOMAIN, TAG, 'ADD ok id=%{private}s', memId);
    return memId;
  }

  private async handleUpdate(item: MemoryItemModel, target: StoredMemoryView,
    metadata: Record<string, Object>): Promise<string> {
    let payload = buildPayload(item);
    payload.updatedAt = MemoryMetaDataModel.nowIso();
    // 合并 history
    const oldHistory = (typeof target.payload.history === 'object' && target.payload.history !== null &&
      !Array.isArray(target.payload.history))
      ? target.payload.history as Record<string, number>
      : ({} as Record<string, number>);
    const merged: Record<string, number> = {};
    const oldKeys: string[] = Object.keys(oldHistory);
    for (let i = 0; i < oldKeys.length; i++) {
      merged[oldKeys[i]] = oldHistory[oldKeys[i]] ?? 0;
    }
    const newKeys: string[] = Object.keys(item.history);
    for (let i = 0; i < newKeys.length; i++) {
      merged[newKeys[i]] = (merged[newKeys[i]] ?? 0) + (item.history[newKeys[i]] ?? 0);
    }
    payload.historyJson = JSON.stringify(merged);
    // 沿用 old sourceTrace（若新 payload 没有）
    if (payload.sourceTraceJson === '[]') {
      payload.sourceTraceJson = (typeof target.payload.sourceTrace === 'object' &&
        target.payload.sourceTrace !== null)
        ? JSON.stringify(target.payload.sourceTrace)
        : '[]';
    }
    if (Object.keys(merged).length > 0) {
      payload.content = updateContentHistorySummary(payload.content, merged);
    }
    payload = appendSourceTrace(payload, payload.scenarioId, 'UPDATE',
      AddPipeline.readString(metadata, 'queryType'),
      AddPipeline.readNumber(metadata, 'originalTurns'));
    item.memId = target.id;
    await MemoryStore.update(target.id, payload);
    hilog.debug(DOMAIN, TAG, 'UPDATE ok id=%{private}s', target.id);
    return target.id;
  }

  private async handleDelete(target: StoredMemoryView): Promise<string> {
    await MemoryStore.delete(target.id);
    hilog.debug(DOMAIN, TAG, 'DELETE ok id=%{private}s', target.id);
    return target.id;
  }

  private async handleNoop(candidate: MemoryItemModel, target: StoredMemoryView,
    metadata: Record<string, Object>): Promise<string> {
    if (Object.keys(candidate.history).length <= 0) {
      hilog.debug(DOMAIN, TAG, 'NOOP: no history to merge id=%{private}s', target.id);
      return target.id;
    }
    const oldPayloadObj = MemoryStore.get(target.id);
    if (!oldPayloadObj) {
      hilog.debug(DOMAIN, TAG, 'NOOP: target not found id=%{private}s', target.id);
      return target.id;
    }
    let payload: StoredMemoryPayload = oldPayloadObj;
    let oldHistory: Record<string, number> = {};
    try {
      const parsed: Object = JSON.parse(payload.historyJson) as Object;
      if (typeof parsed === 'object' && parsed !== null && !Array.isArray(parsed)) {
        oldHistory = parsed as Record<string, number>;
      }
    } catch (_e) {
      oldHistory = {};
    }
    const merged: Record<string, number> = {};
    const oldKeys: string[] = Object.keys(oldHistory);
    for (let i = 0; i < oldKeys.length; i++) {
      merged[oldKeys[i]] = oldHistory[oldKeys[i]] ?? 0;
    }
    const newKeys: string[] = Object.keys(candidate.history);
    for (let i = 0; i < newKeys.length; i++) {
      merged[newKeys[i]] = (merged[newKeys[i]] ?? 0) + (candidate.history[newKeys[i]] ?? 0);
    }
    payload.historyJson = JSON.stringify(merged);
    payload.content = updateContentHistorySummary(payload.content, merged);
    payload.updatedAt = MemoryMetaDataModel.nowIso();
    payload = appendSourceTrace(payload, payload.scenarioId, 'NONE',
      AddPipeline.readString(metadata, 'queryType'),
      AddPipeline.readNumber(metadata, 'originalTurns'));
    await MemoryStore.update(target.id, payload);
    hilog.debug(DOMAIN, TAG, 'NONE history merged id=%{private}s', target.id);
    return target.id;
  }

  // ============================================================================
  // 内部工具
  // ============================================================================

  private static preprocessMessages(messages: Object[]): Object[] {
    if (!messages) {
      return [];
    }
    const out: Object[] = [];
    for (let i = 0; i < messages.length; i++) {
      const m = messages[i];
      if (typeof m !== 'object' || m === null) {
        continue;
      }
      const obj = m as Record<string, Object>;
      if (obj.role === 'system') {
        continue;
      }
      out.push(m);
    }
    return out;
  }

  private static serializeMessages(messages: Object[]): string {
    const lines: string[] = [];
    for (let i = 0; i < messages.length; i++) {
      lines.push(JSON.stringify(messages[i]));
    }
    return lines.join('\n');
  }

  private static toolEntriesToObjects(tools: ToolCallEntry[]): Object[] {
    const out: Object[] = [];
    if (!tools) {
      return out;
    }
    for (let i = 0; i < tools.length; i++) {
      const t = tools[i];
      const o: Record<string, Object> = {};
      o.name = t.name;
      o.args = (t.args ?? {}) as Record<string, Object>;
      out.push(o);
    }
    return out;
  }

  private static toolNameSet(tools: Object[]): Set<string> {
    const s: Set<string> = new Set();
    if (!tools) {
      return s;
    }
    for (let i = 0; i < tools.length; i++) {
      const t = tools[i];
      if (typeof t === 'object' && t !== null) {
        const obj = t as Record<string, Object>;
        if (typeof obj.name === 'string' && (obj.name as string).length > 0) {
          s.add(obj.name as string);
        }
      }
    }
    return s;
  }

  private static disjoint(a: Set<string>, b: Set<string>): boolean {
    const itA = a.values();
    let r = itA.next();
    while (!r.done) {
      if (b.has(r.value)) {
        return false;
      }
      r = itA.next();
    }
    return true;
  }

  private static setStr(s: Set<string>): string {
    const arr: string[] = Array.from(s);
    return `{${arr.join(',')}}`;
  }

  // 把 LLM 在四元决策里返回的新 title/content 应用回 candidate（保持其 metadata/history/tools 不变）。
  private static applyLlmTitleContent(candidate: MemoryItemModel, llmTitle: string, llmContent: string,
    useRawUserQueryAsTitle: boolean): MemoryItemModel {
    const out = new MemoryItemModel();
    out.tools = candidate.tools;
    out.history = candidate.history;
    out.metadata = candidate.metadata;
    if (useRawUserQueryAsTitle) {
      // candidate.title 已经是 user 原话；不被 LLM 决策覆盖
      out.title = candidate.title;
      // 但 summarizedTitle 用 LLM 输出
      if (llmTitle.length > 0) {
        out.metadata.summarizedTitle = llmTitle;
      }
    } else {
      out.title = llmTitle.length > 0 ? llmTitle : candidate.title;
    }
    out.content = llmContent.length > 0 ? llmContent : candidate.content;
    return out;
  }

  private static serializeMemoryItem(item: MemoryItemModel, memId: string): Record<string, Object> {
    const r: Record<string, Object> = {};
    r.memId = memId;
    r.title = item.title;
    r.content = item.content;
    r.tools = AddPipeline.toolEntriesToObjects(item.tools);
    r.history = item.history as Record<string, Object>;
    r.metadata = AddPipeline.serializeMetadata(item.metadata);
    return r;
  }

  private static serializeMetadata(meta: MemoryMetaDataModel): Record<string, Object> {
    const r: Record<string, Object> = {};
    r.userId = meta.userId;
    r.agentId = meta.agentId;
    r.type = meta.type;
    r.subType = meta.subType;
    r.subSubType = meta.subSubType;
    r.createdAt = meta.createdAt;
    r.updatedAt = meta.updatedAt;
    r.summarizedTitle = meta.summarizedTitle;
    r.scenarioId = meta.scenarioId;
    return r;
  }

  private static serializeStoredView(view: StoredMemoryView, memId: string): Record<string, Object> {
    const r: Record<string, Object> = {};
    r.memId = memId;
    const title = (typeof view.payload.title === 'string') ? view.payload.title as string : '';
    const content = (typeof view.payload.content === 'string') ? view.payload.content as string : '';
    r.title = title;
    r.content = content;
    r.metadata = view.payload;
    return r;
  }

  // 使用 SHA-256 根据 title 生成确定性的主键 id，结果为 64 个十六进制字符。
  static async titleToId(title: string): Promise<string> {
    const md = cryptoFramework.createMd('SHA256');
    await md.update({ data: new util.TextEncoder('utf-8').encodeInto(title) });
    const digest = (await md.digest()).data;
    let hash = '';
    for (let i = 0; i < digest.length; i++) {
      const hex = digest[i].toString(16);
      hash += hex.length === 1 ? `0${hex}` : hex;
    }
    return hash;
  }

  private static toHex8(n: number): string {
    const s = (n >>> 0).toString(16);
    return s.length >= 8 ? s.substring(s.length - 8) : ('00000000' + s).substring(s.length);
  }

  private static readString(record: Record<string, Object>, key: string): string {
    const v = record[key];
    return typeof v === 'string' ? (v as string) : '';
  }

  private static readNumber(record: Record<string, Object>, key: string): number {
    const v = record[key];
    return typeof v === 'number' ? (v as number) : 0;
  }
}
