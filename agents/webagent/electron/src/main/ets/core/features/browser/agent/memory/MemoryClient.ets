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

import { hilog } from '@kit.PerformanceAnalysisKit';
import {
  MemorySearchResult,
  MemoryAddResult,
  MemoryContextMetaPayload,
  MemoryMessagePayload,
  MemoryToolCallPayload
} from './MemoryTypes';
import { LocalAddInput, LocalSearchInput, MemorySystem } from './local/core/MemorySystem';
import { AgentLLMPort } from '../ports/AgentLLMPort';

const DOMAIN = 0x6020;
const TAG = 'MemoryClient';

export class MemoryClient {
  public static readonly AGENT_ID: string = 'ultron_browser_agent';
  private static localSystem: MemorySystem | null = null;

  static configureLLM(llm: AgentLLMPort): void {
    // LLM adapters are task scoped; never reuse the previous caller's adapter.
    MemoryClient.localSystem = new MemorySystem(llm);
  }

  private static getLocalSystem(): MemorySystem {
    if (!MemoryClient.localSystem) {
      throw new Error('MemoryClient requires a task-scoped AgentLLMPort');
    }
    return MemoryClient.localSystem;
  }

  static async searchProceduralMemory(
    userId: string,
    query: string,
    contextMetadata: MemoryContextMetaPayload,
    limit: number = 3,
    threshold: number = 0.3,
    searchPreset: string = 'lite'
  ): Promise<MemorySearchResult> {
    try {
      hilog.info(DOMAIN, TAG, 'searchProceduralMemory begin userId=%{public}s limit=%{public}d threshold=%{public}s preset=%{public}s',
        userId, limit, `${threshold}`, searchPreset);
      const input = new LocalSearchInput();
      input.userId = userId;
      input.agentId = MemoryClient.AGENT_ID;
      input.query = query;
      input.contextMetadata = MemoryClient.contextToRecord(contextMetadata);
      input.searchPreset = searchPreset;
      input.limit = limit;
      input.threshold = threshold;
      const result = await MemoryClient.getLocalSystem().searchProcedural(input);
      hilog.info(DOMAIN, TAG, 'searchProceduralMemory code=%{public}d resultCount=%{public}d',
        result.responseCode, result.data ? (result.data.results?.length ?? 0) : 0);
      return result;
    } catch (error) {
      hilog.warn(DOMAIN, TAG, 'searchProceduralMemory failed: %{public}s',
        error instanceof Error ? error.message : `${error}`);
      const failed = new MemorySearchResult();
      failed.responseCode = 1001;
      failed.message = `${error}`;
      failed.data = null;
      return failed;
    }
  }

  static async addProceduralMemory(
    userId: string,
    messages: MemoryMessagePayload[],
    toolCalls: MemoryToolCallPayload[],
    metadata: Record<string, string>
  ): Promise<MemoryAddResult> {
    try {
      hilog.info(DOMAIN, TAG, 'addProceduralMemory begin userId=%{public}s messages=%{public}d toolCalls=%{public}d',
        userId, messages ? messages.length : 0, toolCalls ? toolCalls.length : 0);
      const input = new LocalAddInput();
      input.userId = userId;
      input.agentId = MemoryClient.AGENT_ID;
      input.messages = messages;
      input.toolCalls = toolCalls;
      input.metadata = metadata ?? {};
      const result = await MemoryClient.getLocalSystem().addProcedural(input);
      hilog.info(DOMAIN, TAG, 'addProceduralMemory code=%{public}d event=%{public}s',
        result.responseCode, result.data ? result.data.event : '');
      return result;
    } catch (error) {
      hilog.warn(DOMAIN, TAG, 'addProceduralMemory failed: %{public}s',
        error instanceof Error ? error.message : `${error}`);
      const failed = new MemoryAddResult();
      failed.responseCode = 1001;
      failed.message = `${error}`;
      failed.data = null;
      return failed;
    }
  }

  static formatMemoryResults(data: Object): string {
    const d = data as Record<string, Object>;
    const results = d.results as Array<Record<string, Object>> | undefined;
    if (!results || results.length <= 0) {
      return '';
    }
    const lines: string[] = ['## 历史经验记忆'];
    for (let i = 0; i < results.length; i++) {
      const r = results[i];
      const title = (r.proceduralTitle as string) ?? (r.memory as string) ?? '';
      const content = (r.proceduralContent as string) ?? '';
      const score = typeof r.score === 'number' ? (r.score as number).toFixed(2) : 'N/A';
      if (title.length > 0) {
        lines.push(`${i + 1}. **${title}** (score: ${score})`);
      }
      if (content.length > 0) {
        const preview = content.length > 200 ? content.substring(0, 200) + '...' : content;
        lines.push(`   ${preview}`);
      }
    }
    const summaryText = d.memorySummaryText as string;
    if (summaryText && summaryText.length > 0) {
      lines.push('');
      lines.push('### 记忆总结');
      lines.push(summaryText);
    }
    return lines.join('\n');
  }

  private static contextToRecord(contextMetadata: MemoryContextMetaPayload): Record<string, Object> {
    const r: Record<string, Object> = {};
    if (contextMetadata.task && contextMetadata.task.length > 0) {
      r.task = contextMetadata.task;
    }
    if (contextMetadata.currentUrl && contextMetadata.currentUrl.length > 0) {
      r.currentUrl = contextMetadata.currentUrl;
    }
    if (contextMetadata.siteId && contextMetadata.siteId.length > 0) {
      r.siteId = contextMetadata.siteId;
    }
    if (contextMetadata.startUrl && contextMetadata.startUrl.length > 0) {
      r.startUrl = contextMetadata.startUrl;
    }
    return r;
  }
}
