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

// MemorySystem 主入口（端上版本）：替代原 Mac:5001 Flask 后端 + Python mem0.MemorySystem 子集。
// 对外只暴露 searchProcedural / addProcedural / getAll / clean / status；
// 输入用本地 Request 类，输出统一为 MemoryTypes.ets 中的 MemorySearchResult / MemoryAddResult，
// 与现有 MemoryClient HTTP 路径返回结构完全一致（plan §10 调用入口契约）。

import { hilog } from '@kit.PerformanceAnalysisKit';
import {
  MemoryAddData,
  MemoryAddResult,
  MemoryMessagePayload,
  MemoryResultEntry,
  MemorySearchData,
  MemorySearchResult,
  MemoryToolCallPayload
} from '../../MemoryTypes';
import { MemoryStore, MemoryStoreFilters } from '../store/MemoryStore';
import { MemoryLLMAdapter } from '../llm/MemoryLLMAdapter';
import { MemoryLocalConfig } from '../config/MemoryLocalConfig';
import { MemoryTypeName } from '../domain/Constants';
import { AddPipeline, AddRequest } from './AddPipeline';
import { SearchPipeline, SearchPipelineResult, SearchRequest } from './SearchPipeline';
import { AgentLLMPort } from '../../../ports/AgentLLMPort';

const DOMAIN = 0x6020;
const TAG = 'MemorySystem';

// MemoryClient.searchProceduralMemory 的入参结构（端上）
export class LocalSearchInput {
  public userId: string = '';
  public agentId: string = '';
  public query: string = '';
  public contextMetadata: Record<string, Object> = {};
  public conversationHistory: string[] = [];
  public searchPreset: string = 'lite';
  public limit: number = 5;
  public threshold: number = 0;
}

// MemoryClient.addProceduralMemory 的入参结构（端上）
export class LocalAddInput {
  public userId: string = '';
  public agentId: string = '';
  // 注意：messages 是 MemoryPayloadBuilder 输出的 MemoryMessagePayload[]，content 字段类型是 Object（string 或 MemoryToolEntry[]）
  public messages: MemoryMessagePayload[] = [];
  // toolCalls 中的 argsJson / argsConcreteHintJson 是字符串（plan §10 第二个隐式契约）；
  // 这里仅作为元信息传入；AddPipeline 主要从 messages role=tool 重新抽取。
  public toolCalls: MemoryToolCallPayload[] = [];
  public metadata: Record<string, string> = {};
}

export class MemoryStatusInfo {
  public totalCount: number = 0;
  public typeCounts: Record<string, number> = {};
}

export class MemorySystem {
  private llm: MemoryLLMAdapter;
  private addPipeline: AddPipeline;
  private searchPipeline: SearchPipeline;

  constructor(llm: AgentLLMPort, config: MemoryLocalConfig | null = null) {
    const cfg = config ?? MemoryLocalConfig.defaults();
    this.llm = new MemoryLLMAdapter(llm);
    this.addPipeline = new AddPipeline(this.llm, cfg);
    this.searchPipeline = new SearchPipeline(this.llm);
  }

  // -------- search --------

  async searchProcedural(input: LocalSearchInput): Promise<MemorySearchResult> {
    await MemoryStore.init();
    const result = new MemorySearchResult();
    try {
      const req = new SearchRequest();
      req.userId = input.userId;
      req.agentId = input.agentId;
      req.query = input.query;
      req.contextMetadata = input.contextMetadata ?? {};
      req.conversationHistory = input.conversationHistory ?? [];
      req.searchPreset = input.searchPreset ?? 'lite';
      req.searchConfigDict = {};
      req.limit = input.limit > 0 ? input.limit : 5;
      req.threshold = input.threshold;

      const pipelineResult = await this.searchPipeline.run(req);
      result.responseCode = 200;
      result.message = '程序记忆查询成功';
      result.data = MemorySystem.toSearchData(pipelineResult);
    } catch (error) {
      result.responseCode = 1001;
      result.message = error instanceof Error ? error.message : `${error}`;
      result.data = null;
      hilog.warn(DOMAIN, TAG, 'searchProcedural failed: %{public}s', result.message);
    }
    return result;
  }

  // -------- add --------

  async addProcedural(input: LocalAddInput): Promise<MemoryAddResult> {
    await MemoryStore.init();
    const result = new MemoryAddResult();
    try {
      const req = new AddRequest();
      req.userId = input.userId;
      req.agentId = input.agentId;
      req.messages = MemorySystem.messagesToObjects(input.messages);
      req.topLevelToolCalls = MemorySystem.toolCallPayloadsToObjects(input.toolCalls);
      // metadata 传入 Record<string, string>，统一转成 Record<string, Object>，便于下游读 scenarioId/queryType 等
      const metaObj: Record<string, Object> = {};
      const metaKeys: string[] = input.metadata ? Object.keys(input.metadata) : [];
      for (let i = 0; i < metaKeys.length; i++) {
        metaObj[metaKeys[i]] = input.metadata[metaKeys[i]];
      }
      req.metadata = metaObj;

      const pipelineResult = await this.addPipeline.run(req);
      const data = new MemoryAddData();
      const firstEvent = pipelineResult.results.length > 0
        ? ((pipelineResult.results[0] as Record<string, Object>).event as string ?? '')
        : '';
      data.event = firstEvent;
      const wrapper: Record<string, Object> = {};
      wrapper.results = pipelineResult.results;
      data.result = wrapper;
      data.segmentationInfo = pipelineResult.segmentationInfo;

      result.responseCode = 200;
      result.message = '程序记忆添加成功';
      result.data = data;
    } catch (error) {
      result.responseCode = 1001;
      result.message = error instanceof Error ? error.message : `${error}`;
      result.data = null;
      hilog.warn(DOMAIN, TAG, 'addProcedural failed: %{public}s', result.message);
    }
    return result;
  }

  // -------- getAll / clean / status（保留接口，给 MemoryClient.formatMemoryResults 之外的扩展用）--------

  async getAll(userId: string, agentId: string): Promise<MemoryResultEntry[]> {
    await MemoryStore.init();
    const filters = new MemoryStoreFilters();
    filters.userId = userId;
    filters.agentId = agentId;
    filters.type = MemoryTypeName.PROCEDURAL;
    const docs = MemoryStore.list(filters, -1);
    const out: MemoryResultEntry[] = [];
    for (let i = 0; i < docs.length; i++) {
      const e = new MemoryResultEntry();
      e.id = docs[i].id;
      const p = docs[i].payload;
      e.proceduralTitle = (typeof p.title === 'string') ? p.title as string : '';
      e.proceduralContent = (typeof p.content === 'string') ? p.content as string : '';
      e.score = 0;
      e.vectorScoreRaw = null;
      e.bm25ScoreRaw = null;
      e.retrievalMode = 'list';
      e.metadata = p;
      out.push(e);
    }
    return out;
  }

  async clean(userId: string, agentId: string): Promise<number> {
    await MemoryStore.init();
    const filters = new MemoryStoreFilters();
    filters.userId = userId;
    filters.agentId = agentId;
    filters.type = MemoryTypeName.PROCEDURAL;
    return await MemoryStore.clean(filters);
  }

  async deleteAll(): Promise<number> {
    await MemoryStore.init();
    return await MemoryStore.deleteAll();
  }

  async status(userId: string, agentId: string): Promise<MemoryStatusInfo> {
    await MemoryStore.init();
    const filters = new MemoryStoreFilters();
    filters.userId = userId;
    filters.agentId = agentId;
    const docs = MemoryStore.list(filters, -1);
    const info = new MemoryStatusInfo();
    info.totalCount = docs.length;
    for (let i = 0; i < docs.length; i++) {
      const t = (typeof docs[i].payload.type === 'string') ? docs[i].payload.type as string : 'unknown';
      info.typeCounts[t] = (info.typeCounts[t] ?? 0) + 1;
    }
    return info;
  }

  // -------- 内部映射 --------

  private static toSearchData(pipelineResult: SearchPipelineResult): MemorySearchData {
    const data = new MemorySearchData();
    data.rewrittenQuery = pipelineResult.rewrittenQuery;
    data.retrievalPerformed = pipelineResult.needRetrieval;
    data.memorySummaryText = pipelineResult.memorySummaryText;
    const results: MemoryResultEntry[] = [];
    for (let i = 0; i < pipelineResult.results.length; i++) {
      const item = pipelineResult.results[i] as Record<string, Object>;
      const e = new MemoryResultEntry();
      e.id = (typeof item.id === 'string') ? item.id as string : '';
      e.proceduralTitle = (typeof item.proceduralTitle === 'string')
        ? item.proceduralTitle as string
        : ((typeof item.memory === 'string') ? item.memory as string : '');
      e.proceduralContent = (typeof item.proceduralContent === 'string')
        ? item.proceduralContent as string : '';
      e.score = (typeof item.score === 'number') ? item.score as number : 0;
      e.vectorScoreRaw = MemorySystem.numberOrNull(item.vectorScoreRaw);
      e.bm25ScoreRaw = MemorySystem.numberOrNull(item.bm25ScoreRaw);
      e.retrievalMode = (typeof item.retrievalMode === 'string') ? item.retrievalMode as string : 'bm25';
      e.metadata = (typeof item.metadata === 'object' && item.metadata !== null)
        ? item.metadata as Record<string, Object> : ({} as Record<string, Object>);
      results.push(e);
    }
    data.results = results;
    return data;
  }

  private static numberOrNull(v: Object | null | undefined): number | null {
    if (typeof v === 'number') {
      return v as number;
    }
    return null;
  }

  private static messagesToObjects(messages: MemoryMessagePayload[]): Object[] {
    const out: Object[] = [];
    if (!messages) {
      return out;
    }
    for (let i = 0; i < messages.length; i++) {
      const m = messages[i];
      const obj: Record<string, Object> = {};
      obj.role = m.role;
      obj.content = m.content;
      out.push(obj);
    }
    return out;
  }

  private static toolCallPayloadsToObjects(payloads: MemoryToolCallPayload[]): Object[] {
    const out: Object[] = [];
    if (!payloads) {
      return out;
    }
    for (let i = 0; i < payloads.length; i++) {
      const p = payloads[i];
      const obj: Record<string, Object> = {};
      obj.tool = p.tool;
      obj.name = p.name;
      obj.args = MemorySystem.parseObject(p.argsJson) as Record<string, Object>;
      obj.argsConcreteHint = MemorySystem.parseObject(p.argsConcreteHintJson) as Record<string, Object>;
      obj.result = p.result;
      out.push(obj);
    }
    return out;
  }

  private static parseObject(text: string): Record<string, Object> {
    if (!text || text.length <= 0) {
      return {};
    }
    try {
      const parsed: Object = JSON.parse(text) as Object;
      if (typeof parsed === 'object' && parsed !== null && !Array.isArray(parsed)) {
        return parsed as Record<string, Object>;
      }
      return {};
    } catch (_e) {
      return {};
    }
  }
}
