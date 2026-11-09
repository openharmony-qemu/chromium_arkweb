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

// SearchPipeline：浏览器程序记忆检索流水线（端上版本）。
//
// 对齐 Python search_memory：
//   Step 1 query 分析 -> Step 2 BM25 -> Step 3 enrich -> Step 4 rerank（可关）-> Step 5 summary
// 端上不实现 vector/hybrid（plan §3：BM25-only），retrieval_mode 任意取值都回退到 bm25。

import { hilog } from '@kit.PerformanceAnalysisKit';
import { SearchConfig } from '../domain/SearchConfig';
import { QueryProfile } from '../domain/QueryProfile';
import { MemoryTypeName, VALID_CLARITY, VALID_COMPLEXITY, VALID_ACTION_STRATEGY, VALID_MISSING_SLOTS, TOPK_BY_COMPLEXITY } from '../domain/Constants';
import { StoredMemoryView } from '../domain/MemoryItemModel';
import { MemoryStore, MemoryStoreFilters } from '../store/MemoryStore';
import { MemoryLLMAdapter } from '../llm/MemoryLLMAdapter';
import { Bm25 } from '../retrieval/Bm25';
import { BROWSER_PROCEDURAL_MEMORY_RETRIEVAL_PROMPT, BROWSER_PROCEDURAL_MEMORY_SUMMARY_PROMPT } from '../prompts/BrowserMemoryPrompts';
import { PROCEDURAL_MEMORY_CONSOLIDATED_SUMMARY_PROMPT, PROCEDURAL_MEMORY_RERANK_PROMPT } from '../prompts/MemoryDecisionPrompts';
import {
  buildFallbackQuery,
  collectToolCandidates,
  pruneToolCandidates,
  assignToolRefs,
  dedupeSelectedCalls,
  ToolCandidate
} from './Helpers';

const DOMAIN = 0x6020;
const TAG = 'MemorySearchPipeline';

export class SearchRequest {
  public userId: string = '';
  public agentId: string = '';
  public query: string = '';
  public contextMetadata: Record<string, Object> = {};
  public conversationHistory: string[] = [];
  public searchPreset: string = '';
  public searchConfigDict: Record<string, Object> = {};
  public limit: number = 5;
  public threshold: number = 0;
}

export class SearchSummary {
  public proceduralTitle: string = '';
  public proceduralContent: string = '';
  public functionCallingResult: Object[] = [];
  public suggestedAction: string = '';
  public confidence: string = 'low';
  public sourceMemoryIds: string[] = [];
  public coreIntent: string = '';
  public relevantPreferences: string = '';
}

export class SearchPipelineResult {
  public results: Object[] = [];
  public rewrittenQuery: string = '';
  public needRetrieval: boolean = true;
  public retrievalReason: string = '';
  public memorySummary: SearchSummary | null = null;
  public memorySummaryText: string = '';
  public configUsed: Record<string, Object> = {};
  public queryProfile: Record<string, Object> | null = null;
  public actionStrategy: string = '';
  public missingSlots: string[] = [];
}

export class SearchPipeline {
  private llm: MemoryLLMAdapter;

  constructor(llm: MemoryLLMAdapter) {
    this.llm = llm;
  }

  async run(req: SearchRequest): Promise<SearchPipelineResult> {
    const config = SearchPipeline.resolveConfig(req);
    const limit = req.limit > 0 ? req.limit : config.maxResults;
    const filters = new MemoryStoreFilters();
    filters.userId = req.userId;
    filters.agentId = req.agentId;
    filters.type = MemoryTypeName.PROCEDURAL;

    hilog.info(DOMAIN, TAG, 'search query=%{private}s preset=%{private}s llmCalls=%{private}d',
      SearchPipeline.shortText(req.query), req.searchPreset, config.llmCallCount());

    // ---------- Step 1：query 分析 ----------
    const queryProfile = await this.queryStep(req.query, req.contextMetadata, req.conversationHistory, config);
    hilog.info(DOMAIN, TAG, 'query analysis need=%{public}s rewritten=%{private}s reason=%{private}s',
      `${queryProfile.needRetrieval}`, SearchPipeline.shortText(queryProfile.rewrittenQuery),
      queryProfile.reason);
    if (!queryProfile.needRetrieval) {
      const out = SearchPipeline.buildEmptyResult(queryProfile, config, limit);
      return out;
    }

    // 自适应（端上仅复用 topK 调整；retrieval_mode 永远是 bm25）
    let effectiveLimit = limit;
    if (config.enableAdaptive) {
      const adaptiveK = TOPK_BY_COMPLEXITY[queryProfile.complexity];
      if (typeof adaptiveK === 'number') {
        effectiveLimit = adaptiveK;
        config.maxResults = adaptiveK;
      }
      // simple → 关 rerank/summary；complex → 都打开
      if (queryProfile.complexity === 'simple') {
        config.enableRerank = false;
        config.enableSummary = false;
      } else if (queryProfile.complexity === 'complex') {
        config.enableRerank = true;
        config.enableSummary = true;
      }
    }

    // ---------- Step 2：BM25 检索（端上唯一通路）----------
    const allDocs = MemoryStore.list(filters, -1);
    hilog.info(DOMAIN, TAG, 'bm25 docs=%{public}d query=%{private}s',
      allDocs.length, SearchPipeline.shortText(queryProfile.rewrittenQuery));
    const bm25Hits = Bm25.search(queryProfile.rewrittenQuery, allDocs, 'title', effectiveLimit);
    hilog.info(DOMAIN, TAG, 'bm25 hits=%{public}d topScore=%{private}s',
      bm25Hits.length, bm25Hits.length > 0 ? `${bm25Hits[0].score}` : 'N/A');
    let results: Record<string, Object>[] = [];
    let filteredByThreshold = 0;
    for (let i = 0; i < bm25Hits.length; i++) {
      const r = bm25Hits[i];
      if (req.threshold > 0 && r.score < req.threshold) {
        filteredByThreshold++;
        continue;
      }
      const item: Record<string, Object> = {};
      item.id = r.id;
      item.memory = (typeof r.payload.title === 'string') ? r.payload.title as string : '';
      item.score = r.score;
      item.vectorScoreRaw = '';
      item.bm25ScoreRaw = r.score;
      item.rrfScore = '';
      item.retrievalMode = 'bm25';
      item.metadata = r.payload;
      results.push(item);
    }
    if (filteredByThreshold > 0) {
      hilog.info(DOMAIN, TAG, 'threshold=%{public}s filtered=%{public}d of %{public}d',
        `${req.threshold}`, filteredByThreshold, bm25Hits.length);
    }
    if (results.length <= 0) {
      const out = SearchPipeline.buildEmptyResult(queryProfile, config, effectiveLimit);
      out.needRetrieval = true;
      return out;
    }

    // ---------- Step 3：enrich 程序记忆字段 ----------
    if (config.enableEnrich) {
      results = SearchPipeline.enrichProcedural(results);
    }

    // ---------- Step 4：rerank ----------
    if (config.enableRerank && results.length > 1) {
      results = await this.rerankWithLlm(req.query, queryProfile.rewrittenQuery, req.contextMetadata, results, config);
    } else if (config.enableEnrich) {
      results = SearchPipeline.heuristicRerank(req.contextMetadata, results);
    }
    if (results.length > config.maxResults) {
      results = results.slice(0, config.maxResults);
    }

    // ---------- Step 5：summary ----------
    let summary: SearchSummary | null = null;
    let summaryText = '';
    if (config.enableSummary && results.length > 0) {
      summary = await this.summarizeWithLlm(req.query, req.contextMetadata, results);
      summaryText = SearchPipeline.formatSummaryText(summary, queryProfile);
    } else if (config.enableEnrich && results.length > 0) {
      summary = SearchPipeline.buildNoLlmSummary(results, req.contextMetadata);
      summaryText = SearchPipeline.formatSummaryText(summary, queryProfile);
    } else {
      summaryText = '';
    }

    const out = new SearchPipelineResult();
    out.results = results as Object[];
    out.rewrittenQuery = queryProfile.rewrittenQuery;
    out.needRetrieval = true;
    out.retrievalReason = queryProfile.reason;
    out.memorySummary = summary;
    out.memorySummaryText = summaryText.length > 0 ? summaryText : (results.length <= 0 ? '未找到相关记忆。' : '');
    out.configUsed = SearchPipeline.buildConfigUsed(config, effectiveLimit);
    out.queryProfile = SearchPipeline.serializeProfile(queryProfile);
    out.actionStrategy = queryProfile.actionStrategy;
    out.missingSlots = queryProfile.missingSlots;
    return out;
  }

  // ============================================================================
  // Step 1
  // ============================================================================

  private async queryStep(query: string, contextMetadata: Record<string, Object>,
    conversationHistory: string[], config: SearchConfig): Promise<QueryProfile> {
    if (!config.enableQueryAnalysis) {
      return QueryProfile.fallback(buildFallbackQuery(query, contextMetadata, config.fallbackWithContext),
        '直接检索');
    }
    const histStr = (conversationHistory && conversationHistory.length > 0)
      ? conversationHistory.slice(-10).join('\n') : '无';
    const ctxStr = JSON.stringify(contextMetadata ?? {});
    const prompt = MemoryLLMAdapter.formatTemplate(BROWSER_PROCEDURAL_MEMORY_RETRIEVAL_PROMPT, {
      'conversation_history': histStr,
      'user_query': query,
      'context': ctxStr
    });
    const resp = await this.llm.runJson(prompt, '请仅返回 JSON 查询画像对象。', 'memory_query_analyze');
    if (!resp.ok || !resp.json) {
      hilog.warn(DOMAIN, TAG, 'query analysis failed, fallback: %{public}s', resp.errorMessage);
      return QueryProfile.fallback(query, 'LLM失败');
    }
    return SearchPipeline.parseQueryProfile(resp.json, query);
  }

  private static parseQueryProfile(j: Record<string, Object>, originalQuery: string): QueryProfile {
    const p = new QueryProfile();
    const rawNeed = j.need_retrieval;
    if (typeof rawNeed === 'string') {
      const lower = (rawNeed as string).trim().toLowerCase();
      p.needRetrieval = (lower === 'true' || lower === '1' || lower === 'yes');
    } else if (typeof rawNeed === 'boolean') {
      p.needRetrieval = rawNeed as boolean;
    }
    p.rewrittenQuery = (typeof j.rewritten_query === 'string' && (j.rewritten_query as string).length > 0)
      ? j.rewritten_query as string : originalQuery;
    p.reason = (typeof j.reason === 'string') ? j.reason as string : '';
    let clarity = (typeof j.clarity_level === 'string')
      ? (j.clarity_level as string).trim().toLowerCase() : 'medium';
    if (VALID_CLARITY.indexOf(clarity) < 0) {
      clarity = 'medium';
    }
    p.clarityLevel = clarity;
    let complexity = (typeof j.complexity === 'string')
      ? (j.complexity as string).trim().toLowerCase() : 'moderate';
    if (VALID_COMPLEXITY.indexOf(complexity) < 0) {
      complexity = 'moderate';
    }
    p.complexity = complexity;
    let action = (typeof j.action_strategy === 'string')
      ? (j.action_strategy as string).trim().toLowerCase() : 'execute';
    if (VALID_ACTION_STRATEGY.indexOf(action) < 0) {
      action = 'execute';
    }
    p.actionStrategy = action;
    const rawMissing = j.missing_slots;
    const missing: string[] = [];
    if (Array.isArray(rawMissing)) {
      const arr = rawMissing as Object[];
      for (let i = 0; i < arr.length; i++) {
        const s = String(arr[i]).trim();
        if (VALID_MISSING_SLOTS.indexOf(s) >= 0 && missing.indexOf(s) < 0) {
          missing.push(s);
        }
      }
    }
    p.missingSlots = missing;
    if (p.actionStrategy === 'execute') {
      p.missingSlots = [];
    } else if (p.actionStrategy === 'clarify' && p.missingSlots.length <= 0) {
      p.actionStrategy = 'execute';
    }
    return p;
  }

  // ============================================================================
  // Step 3 / 4 / 5
  // ============================================================================

  private static enrichProcedural(results: Record<string, Object>[]): Record<string, Object>[] {
    const out: Record<string, Object>[] = [];
    for (let i = 0; i < results.length; i++) {
      const item = results[i];
      const meta = (typeof item.metadata === 'object' && item.metadata !== null)
        ? item.metadata as Record<string, Object> : ({} as Record<string, Object>);
      const enriched: Record<string, Object> = {};
      const itemKeys: string[] = Object.keys(item);
      for (let k = 0; k < itemKeys.length; k++) {
        enriched[itemKeys[k]] = item[itemKeys[k]];
      }
      const titleVal = (typeof meta.title === 'string' && (meta.title as string).length > 0)
        ? meta.title as string : ((typeof item.memory === 'string') ? item.memory as string : '');
      const contentVal = (typeof meta.content === 'string' && (meta.content as string).length > 0)
        ? meta.content as string : '';
      enriched.proceduralTitle = titleVal;
      enriched.proceduralContent = contentVal;
      enriched.functionCallingResult = (Array.isArray(meta.tools) ? meta.tools : []) as Object[];
      enriched.history = (typeof meta.history === 'object' && meta.history !== null)
        ? meta.history as Record<string, Object> : ({} as Record<string, Object>);
      out.push(enriched);
    }
    return out;
  }

  private static heuristicRerank(contextMetadata: Record<string, Object>,
    results: Record<string, Object>[]): Record<string, Object>[] {
    const matchFields: Record<string, Object> = {};
    const ctxKeys: string[] = Object.keys(contextMetadata);
    for (let i = 0; i < ctxKeys.length; i++) {
      const k = ctxKeys[i];
      if (k.startsWith('_')) {
        continue;
      }
      const v = contextMetadata[k];
      if (v) {
        matchFields[k] = v;
      }
    }
    const scored = results.slice();
    scored.sort((a: Record<string, Object>, b: Record<string, Object>): number => {
      return SearchPipeline.heuristicScore(b, matchFields) - SearchPipeline.heuristicScore(a, matchFields);
    });
    return scored;
  }

  private static heuristicScore(item: Record<string, Object>, matchFields: Record<string, Object>): number {
    let s = (typeof item.score === 'number') ? item.score as number : 0;
    const meta = (typeof item.metadata === 'object' && item.metadata !== null)
      ? item.metadata as Record<string, Object> : ({} as Record<string, Object>);
    const keys: string[] = Object.keys(matchFields);
    for (let i = 0; i < keys.length; i++) {
      const k = keys[i];
      const expected = matchFields[k];
      if (item[k] === expected || meta[k] === expected) {
        s += 0.15;
      }
    }
    if (Array.isArray(item.functionCallingResult) && (item.functionCallingResult as Object[]).length > 0) {
      s += 0.1;
    }
    return s;
  }

  private async rerankWithLlm(originalQuery: string, rewrittenQuery: string,
    contextMetadata: Record<string, Object>, results: Record<string, Object>[],
    config: SearchConfig): Promise<Record<string, Object>[]> {
    const cap = Math.min(results.length, config.maxResults);
    const subset = results.slice(0, cap);
    const lines: string[] = [];
    for (let i = 0; i < subset.length; i++) {
      const item = subset[i];
      const title = (typeof item.proceduralTitle === 'string') ? item.proceduralTitle as string :
        ((typeof item.memory === 'string') ? item.memory as string : '');
      const content = (typeof item.proceduralContent === 'string') ? item.proceduralContent as string : '';
      const score = (typeof item.score === 'number') ? `${item.score}` : 'N/A';
      lines.push(`记忆ID: ${item.id}\n  标题: ${title}\n  内容: ${SearchPipeline.shortText(content, 100)}\n  相似度: ${score}`);
    }
    const prompt = MemoryLLMAdapter.formatTemplate(PROCEDURAL_MEMORY_RERANK_PROMPT, {
      'user_query': originalQuery,
      'rewrittenQuery': rewrittenQuery,
      'context': JSON.stringify(contextMetadata ?? {}),
      'candidates': lines.join('\n\n')
    });
    const resp = await this.llm.runJson(prompt, '请仅返回 JSON 排序对象。', 'memory_rerank');
    if (!resp.ok || !resp.json) {
      return SearchPipeline.heuristicRerank(contextMetadata, results);
    }
    const rankingRaw = resp.json.ranking;
    if (!Array.isArray(rankingRaw)) {
      return SearchPipeline.heuristicRerank(contextMetadata, results);
    }
    const ranking = rankingRaw as Object[];
    const idMap: Map<string, Record<string, Object>> = new Map();
    for (let i = 0; i < results.length; i++) {
      const id = (typeof results[i].id === 'string') ? results[i].id as string : '';
      if (id.length > 0) {
        idMap.set(id, results[i]);
      }
    }
    const reranked: Record<string, Object>[] = [];
    for (let i = 0; i < ranking.length; i++) {
      const id = String(ranking[i]);
      const item = idMap.get(id);
      if (item) {
        reranked.push(item);
        idMap.delete(id);
      }
    }
    const remaining = Array.from(idMap.values());
    for (let i = 0; i < remaining.length; i++) {
      reranked.push(remaining[i]);
    }
    return reranked;
  }

  private async summarizeWithLlm(query: string, contextMetadata: Record<string, Object>,
    memories: Record<string, Object>[]): Promise<SearchSummary> {
    let candidates = collectToolCandidates(memories, contextMetadata);
    candidates = pruneToolCandidates(candidates, 30, 4);
    candidates = assignToolRefs(candidates);
    const toolRefMap: Map<string, ToolCandidate> = new Map();
    const toolNameMap: Map<string, ToolCandidate[]> = new Map();
    for (let i = 0; i < candidates.length; i++) {
      const c = candidates[i];
      toolRefMap.set(c.toolRef, c);
      const arr = toolNameMap.get(c.name) ?? [];
      arr.push(c);
      toolNameMap.set(c.name, arr);
    }

    const memoryDescs: string[] = [];
    for (let i = 0; i < memories.length; i++) {
      const m = memories[i];
      const title = (typeof m.proceduralTitle === 'string') ? m.proceduralTitle as string : '';
      const content = (typeof m.proceduralContent === 'string') ? m.proceduralContent as string : '';
      const id = (typeof m.id === 'string') ? m.id as string : 'N/A';
      memoryDescs.push(`[${i}] 标题：${title}\n    内容：${content}\n    记忆 ID: ${id}`);
    }
    const toolDescs: string[] = [];
    for (let i = 0; i < candidates.length; i++) {
      const c = candidates[i];
      toolDescs.push(`- ${c.toolRef}: ${c.name} args=${JSON.stringify(c.args)} (来源记忆[${c.memoryIndex}], 支持次数=${c.supportCount})`);
    }
    const prompt = MemoryLLMAdapter.formatTemplate(PROCEDURAL_MEMORY_CONSOLIDATED_SUMMARY_PROMPT, {
      'user_query': query,
      'context': JSON.stringify(contextMetadata ?? {}),
      'memories': memoryDescs.length > 0 ? memoryDescs.join('\n\n') : '无',
      'all_tools': toolDescs.length > 0 ? toolDescs.join('\n') : '无'
    });
    const resp = await this.llm.runJson(prompt, '请仅返回 JSON 对象。', 'memorySummary');
    if (!resp.ok || !resp.json) {
      return SearchPipeline.buildLlmFallback(query, memories);
    }
    const j = resp.json;
    const summary = new SearchSummary();
    summary.proceduralTitle = (typeof j.title === 'string') ? j.title as string : query;
    summary.proceduralContent = (typeof j.content === 'string') ? j.content as string : '';
    summary.suggestedAction = (typeof j.suggestedAction === 'string') ? j.suggestedAction as string : '';
    summary.confidence = (typeof j.confidence === 'string') ? j.confidence as string : 'medium';
    summary.coreIntent = summary.proceduralTitle;
    summary.relevantPreferences = summary.proceduralContent;

    const selected = SearchPipeline.extractSelectedTools(j, toolRefMap, toolNameMap);
    summary.functionCallingResult = dedupeSelectedCalls(selected);
    summary.sourceMemoryIds = SearchPipeline.extractSourceIds(j, memories);
    return summary;
  }

  private static buildLlmFallback(query: string, memories: Record<string, Object>[]): SearchSummary {
    const fallback = new SearchSummary();
    fallback.proceduralTitle = query;
    fallback.confidence = 'low';
    const sourceIds: string[] = [];
    for (let i = 0; i < memories.length; i++) {
      const id = memories[i].id;
      if (typeof id === 'string') {
        sourceIds.push(id as string);
      }
    }
    fallback.sourceMemoryIds = sourceIds;
    fallback.coreIntent = query;
    return fallback;
  }

  private static extractSelectedTools(j: Record<string, Object>,
    toolRefMap: Map<string, ToolCandidate>,
    toolNameMap: Map<string, ToolCandidate[]>): Object[] {
    const selected: Object[] = [];
    const refsRaw = j.relevantToolRefs;
    if (Array.isArray(refsRaw)) {
      SearchPipeline.collectToolsByRefs(refsRaw as Object[], toolRefMap, selected);
    }
    if (selected.length <= 0) {
      const callsRaw = j.relevantToolCalls;
      if (Array.isArray(callsRaw)) {
        SearchPipeline.collectToolsFromCalls(callsRaw as Object[], selected);
      }
    }
    if (selected.length <= 0) {
      const namesRaw = j.relevantTools;
      if (Array.isArray(namesRaw)) {
        SearchPipeline.collectToolsByNames(namesRaw as Object[], toolNameMap, selected);
      }
    }
    return selected;
  }

  private static collectToolsByRefs(refs: Object[], toolRefMap: Map<string, ToolCandidate>,
    selected: Object[]): void {
    for (let i = 0; i < refs.length; i++) {
      const ref = String(refs[i]);
      const c = toolRefMap.get(ref);
      if (c) {
        const o: Record<string, Object> = {};
        o.name = c.name;
        o.args = c.args as Record<string, Object>;
        selected.push(o);
      }
    }
  }

  private static collectToolsFromCalls(calls: Object[], selected: Object[]): void {
    for (let i = 0; i < calls.length; i++) {
      const tc = calls[i];
      if (typeof tc === 'object' && tc !== null) {
        const obj = tc as Record<string, Object>;
        const name = (typeof obj.name === 'string') ? obj.name as string : '';
        const args = (typeof obj.args === 'object' && obj.args !== null)
          ? obj.args as Record<string, Object> : ({} as Record<string, Object>);
        if (name.length > 0 && Object.keys(args).length > 0) {
          const o: Record<string, Object> = {};
          o.name = name;
          o.args = args;
          selected.push(o);
        }
      }
    }
  }

  private static collectToolsByNames(names: Object[], toolNameMap: Map<string, ToolCandidate[]>,
    selected: Object[]): void {
    for (let i = 0; i < names.length; i++) {
      const n = String(names[i]);
      const arr = toolNameMap.get(n) ?? [];
      for (let k = 0; k < arr.length; k++) {
        const o: Record<string, Object> = {};
        o.name = arr[k].name;
        o.args = arr[k].args as Record<string, Object>;
        selected.push(o);
      }
    }
  }

  private static extractSourceIds(j: Record<string, Object>,
    memories: Record<string, Object>[]): string[] {
    const memoryAnalysisRaw = j.memory_analysis;
    if (Array.isArray(memoryAnalysisRaw)) {
      return SearchPipeline.extractSourceIdsFromAnalysis(memoryAnalysisRaw as Object[], memories);
    }
    return SearchPipeline.extractAllSourceIds(memories);
  }

  private static extractSourceIdsFromAnalysis(analysis: Object[], memories: Record<string, Object>[]): string[] {
    const sourceIds: string[] = [];
    for (let i = 0; i < analysis.length; i++) {
      const id = SearchPipeline.tryExtractSourceId(analysis[i], memories);
      if (id.length > 0) {
        sourceIds.push(id);
      }
    }
    return sourceIds;
  }

  private static tryExtractSourceId(analysisItem: Object, memories: Record<string, Object>[]): string {
    if (typeof analysisItem !== 'object' || analysisItem === null) {
      return '';
    }
    const obj = analysisItem as Record<string, Object>;
    const idx = (typeof obj.memory_index === 'number') ? obj.memory_index as number : -1;
    const isRel = obj.is_relevant === true;
    if (isRel && idx >= 0 && idx < memories.length) {
      const id = memories[idx].id;
      if (typeof id === 'string') {
        return id as string;
      }
    }
    return '';
  }

  private static extractAllSourceIds(memories: Record<string, Object>[]): string[] {
    const sourceIds: string[] = [];
    for (let i = 0; i < memories.length; i++) {
      const id = memories[i].id;
      if (typeof id === 'string') {
        sourceIds.push(id as string);
      }
    }
    return sourceIds;
  }

  private static buildNoLlmSummary(results: Record<string, Object>[],
    contextMetadata: Record<string, Object>): SearchSummary {
    const out = new SearchSummary();
    if (!results || results.length <= 0) {
      return out;
    }
    let candidates = collectToolCandidates(results.slice(0, 1), contextMetadata);
    candidates = pruneToolCandidates(candidates, 8, 1);
    const calls: Object[] = [];
    for (let i = 0; i < candidates.length; i++) {
      const o: Record<string, Object> = {};
      o.name = candidates[i].name;
      o.args = candidates[i].args as Record<string, Object>;
      calls.push(o);
    }
    const best = results[0];
    out.proceduralTitle = (typeof best.proceduralTitle === 'string') ? best.proceduralTitle as string : '';
    out.proceduralContent = (typeof best.proceduralContent === 'string') ? best.proceduralContent as string : '';
    out.functionCallingResult = dedupeSelectedCalls(calls);
    out.suggestedAction = `根据历史偏好执行：${out.proceduralTitle}`;
    out.confidence = results.length > 1 ? 'medium' : 'low';
    out.coreIntent = out.proceduralTitle;
    out.relevantPreferences = out.proceduralContent;
    const sourceIds: string[] = [];
    for (let i = 0; i < results.length; i++) {
      const id = results[i].id;
      if (typeof id === 'string') {
        sourceIds.push(id as string);
      }
    }
    out.sourceMemoryIds = sourceIds;
    return out;
  }

  private static formatSummaryText(summary: SearchSummary | null, profile: QueryProfile): string {
    if (!summary) {
      return '未检索到相关记忆。';
    }
    if (profile && profile.actionStrategy === 'clarify') {
      const lines: string[] = ['【需要确认信息】'];
      if (profile.missingSlots && profile.missingSlots.length > 0) {
        lines.push(`缺失关键信息：${profile.missingSlots.join(', ')}`);
      }
      lines.push('建议先向用户确认以上信息后再执行操作。');
      lines.push('');
      lines.push('【参考历史偏好】');
      if (summary.proceduralContent && summary.proceduralContent.length > 0) {
        lines.push(`相关偏好：${summary.proceduralContent}`);
      }
      const fc = summary.functionCallingResult ?? [];
      if (fc.length > 0) {
        lines.push('历史工具调用（仅供参考）：');
        for (let i = 0; i < fc.length; i++) {
          const obj = fc[i] as Record<string, Object>;
          const name = (typeof obj.name === 'string') ? obj.name as string : '';
          lines.push(`- ${name}: ${JSON.stringify(obj.args ?? {})}`);
        }
      }
      return lines.join('\n');
    }
    const lines: string[] = ['【记忆检索结果】'];
    if (summary.proceduralTitle.length > 0) {
      lines.push(`用户意图：${summary.proceduralTitle}`);
    }
    if (summary.proceduralContent.length > 0) {
      lines.push(`相关偏好：${summary.proceduralContent}`);
    }
    if (summary.suggestedAction.length > 0) {
      lines.push(`建议方案：${summary.suggestedAction}`);
    }
    if (summary.confidence.length > 0) {
      const cmap: Record<string, string> = { 'high': '高', 'medium': '中', 'low': '低' };
      lines.push(`置信度：${cmap[summary.confidence] ?? summary.confidence}`);
    }
    const fc = summary.functionCallingResult ?? [];
    if (fc.length > 0) {
      lines.push('');
      lines.push('可复用工具调用：');
      for (let i = 0; i < fc.length; i++) {
        const obj = fc[i] as Record<string, Object>;
        const name = (typeof obj.name === 'string') ? obj.name as string : '';
        lines.push(`- ${name}: ${JSON.stringify(obj.args ?? {})}`);
      }
    }
    return lines.join('\n');
  }

  // ============================================================================
  // utils
  // ============================================================================

  private static resolveConfig(req: SearchRequest): SearchConfig {
    if (req.searchPreset && req.searchPreset.length > 0) {
      const preset = SearchConfig.fromPreset(req.searchPreset);
      if (preset) {
        return preset;
      }
    }
    if (req.searchConfigDict && Object.keys(req.searchConfigDict).length > 0) {
      return SearchConfig.fromDict(req.searchConfigDict);
    }
    return SearchConfig.lite();
  }

  private static buildEmptyResult(profile: QueryProfile, config: SearchConfig, limit: number): SearchPipelineResult {
    const out = new SearchPipelineResult();
    out.results = [];
    out.rewrittenQuery = profile.rewrittenQuery;
    out.needRetrieval = profile.needRetrieval;
    out.retrievalReason = profile.reason;
    out.memorySummary = null;
    out.memorySummaryText = '未找到相关记忆。';
    out.configUsed = SearchPipeline.buildConfigUsed(config, limit);
    out.queryProfile = SearchPipeline.serializeProfile(profile);
    out.actionStrategy = profile.actionStrategy;
    out.missingSlots = profile.missingSlots;
    return out;
  }

  private static buildConfigUsed(config: SearchConfig, retrievalLimit: number): Record<string, Object> {
    const r: Record<string, Object> = {};
    r.enableQueryAnalysis = config.enableQueryAnalysis;
    r.enableEnrich = config.enableEnrich;
    r.enableRerank = config.enableRerank;
    r.enableSummary = config.enableSummary;
    r.enableAdaptive = config.enableAdaptive;
    r.llm_calls = config.llmCallCount();
    r.retrievalMode = 'bm25';
    r.hybridFusion = config.hybridFusion;
    r.rrfK = config.rrfK;
    r.vectorWeight = config.vectorWeight;
    r.bm25Weight = config.bm25Weight;
    r.retrieval_limit = retrievalLimit;
    r.maxResults = config.maxResults;
    r.embedding_model = '';
    return r;
  }

  private static serializeProfile(p: QueryProfile): Record<string, Object> {
    const r: Record<string, Object> = {};
    r.clarity_level = p.clarityLevel;
    r.complexity = p.complexity;
    r.actionStrategy = p.actionStrategy;
    r.missingSlots = p.missingSlots as Object;
    return r;
  }

  private static shortText(text: string, max: number = 60): string {
    if (!text) {
      return '';
    }
    return text.length > max ? text.substring(0, max) + '…' : text;
  }
}
