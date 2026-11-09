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
import { BrowserRuntimeState } from '../../../shared/domain/BrowserAutomationTypes';
import { MemoryClient } from '../agent/memory/MemoryClient';
import { vConfig } from '../agent/VersionConfig';
import { MemoryContextMetaPayload, MemoryMessagePayload, MemoryToolCallPayload } from '../agent/memory/MemoryTypes';
import { MemoryPayloadBuilder, MemoryPayloadBundle } from '../agent/memory/MemoryPayloadBuilder';
import { AgentState } from '../agent/core/AgentState';
import { AgentLLMPort } from '../agent/ports/AgentLLMPort';
import { MemoryLocalConfig } from '../agent/memory/local/config/MemoryLocalConfig';
import { SessionMemoryService } from '../agent/memory/session/SessionMemoryService';

const DOMAIN = 0x6020;
const TAG = 'BrowserExecMemory';

export class BrowserExecutionMemoryService {

  /**
   * 任务执行前的记忆准备：程序记忆搜索 + session 记忆检索。
   * 返回更新后的 runtime（proceduralMemory / sessionMemory 已填充）。
   */
  static async prepareMemory(
    runtime: BrowserRuntimeState,
    onUiLine: (line: string) => void
  ): Promise<BrowserRuntimeState> {
    // 1. 程序记忆检索
    hilog.info(DOMAIN, TAG, 'procedural memory search decision: proceduralMemoryEnabled=%{private}s strategy=%{private}s',
      `${vConfig.proceduralMemoryEnabled}`, vConfig.proceduralMemorySearchStrategy);
    if (vConfig.proceduralMemoryEnabled && await BrowserExecutionMemoryService.shouldSearchProceduralMemory(runtime)) {
      let memoryUiLine: string = '';
      const memoryUserId = BrowserExecutionMemoryService.resolveMemoryUserId();
      const memorySearchThreshold = MemoryLocalConfig.defaults().defaultSearchThreshold;
      try {
        const contextMeta = new MemoryContextMetaPayload();
        contextMeta.task = runtime.task;
        contextMeta.currentUrl = runtime.currentPerception?.currentUrl ?? '';
        contextMeta.siteId = '';
        contextMeta.startUrl = '';
        hilog.info(DOMAIN, TAG, 'memory retrieve begin');
        const memResult = await MemoryClient.searchProceduralMemory(
          memoryUserId,
          runtime.task,
          contextMeta,
          3,
          memorySearchThreshold,
          'lite'
        );
        if (memResult.responseCode === 200 && memResult.data) {
          runtime.proceduralMemory = MemoryClient.formatMemoryResults(memResult.data);
          const hitCount = memResult.data.results.length;
          const summaryLen = memResult.data.memorySummaryText.length;
          hilog.info(DOMAIN, TAG, 'retrieved procedural memory (%{public}d results, formatted length=%{public}d)',
            hitCount, runtime.proceduralMemory.length);
          memoryUiLine =
            `[程序记忆] 本地检索：命中 ${hitCount} 条，摘要长度 ${summaryLen}，已注入 Agent。`;
        } else {
          memoryUiLine =
            `[程序记忆] 本地检索：未返回有效数据 (code=${memResult.responseCode})。`;
        }
      } catch (error) {
        const msg = error instanceof Error ? error.message : `${error}`;
        hilog.warn(DOMAIN, TAG, 'memory retrieval failed (non-fatal): %{private}s', msg);
        memoryUiLine = `[程序记忆] 检索异常（已忽略，任务继续）：${msg}`;
      }
      if (memoryUiLine.length > 0) {
        onUiLine(memoryUiLine);
      }
    }

    // 2. Session 记忆检索（话题接续：取当前 session 最近 N 条）
    if (vConfig.sessionMemoryEnabled && runtime.sessionKey.length > 0) {
      try {
        const sessionCtx = await SessionMemoryService.getSessionMemory(runtime.sessionKey, 5);
        if (sessionCtx.length > 0) {
          runtime.sessionMemory = sessionCtx;
          hilog.info(DOMAIN, TAG, 'session memory retrieved: %{public}d chars', sessionCtx.length);
          onUiLine(`[话题记忆] 已加载上下文 (${sessionCtx.length} 字符):\n${sessionCtx}`);
        }
      } catch (error) {
        hilog.warn(DOMAIN, TAG, 'session memory retrieval failed (non-fatal): %{private}s',
          error instanceof Error ? error.message : `${error}`);
      }
    }

    return runtime;
  }

  /**
   * 任务执行后的程序记忆持久化。
   * 返回 UI 提示行，或 null 表示跳过。
   */
  static async persistProceduralMemory(
    state: AgentState | null,
    runtime: BrowserRuntimeState,
    llm: AgentLLMPort
  ): Promise<string | null> {
    if (!vConfig.proceduralMemoryEnabled || !vConfig.proceduralMemoryStoreEnabled || runtime.phase !== 'FINISHED') {
      return null;
    }
    if (runtime.actionHistory.length < vConfig.proceduralMemoryStoreMinSteps) {
      hilog.info(DOMAIN, TAG, 'procedural memory store skipped: steps=%{public}d < min=%{public}d',
        runtime.actionHistory.length, vConfig.proceduralMemoryStoreMinSteps);
      return null;
    }
    let bundle: MemoryPayloadBundle | null = null;
    if (state) {
      try {
        bundle = await MemoryPayloadBuilder.build(state, llm);
      } catch (error) {
        hilog.warn(DOMAIN, TAG, 'MemoryPayloadBuilder failed, fallback to legacy runtime-based payload: %{private}s',
          error instanceof Error ? error.message : `${error}`);
      }
    } else {
      hilog.warn(DOMAIN, TAG, 'persistProceduralMemory: no AgentState available, falling back to legacy runtime-based payload');
    }
    if (!bundle) {
      bundle = BrowserExecutionMemoryService.buildLegacyBundle(runtime);
    }
    try {
      const memoryUserId = BrowserExecutionMemoryService.resolveMemoryUserId();
      const addResult = await MemoryClient.addProceduralMemory(
        memoryUserId,
        bundle.messages,
        bundle.toolCalls,
        bundle.metadata
      );
      if (addResult.responseCode !== 200) {
        hilog.warn(DOMAIN, TAG, 'persist procedural memory failed: code=%{public}d, msg=%{private}s',
          addResult.responseCode, addResult.message);
        return `[程序记忆] 本地写入失败 code=${addResult.responseCode}`;
      }
      const eventStr = addResult.data ? addResult.data.event : '';
      hilog.info(DOMAIN, TAG,
        'persist procedural memory success');
      return `[程序记忆] 本地写入完成 event=${eventStr.length > 0 ? eventStr : '(空)'}`;
    } catch (error) {
      const msg = error instanceof Error ? error.message : `${error}`;
      hilog.warn(DOMAIN, TAG, 'persist procedural memory exception (non-fatal): %{private}s', msg);
      return `[程序记忆] 写入异常（已忽略）：${msg}`;
    }
  }

  /**
   * 任务执行后的 session 记忆存储，并在阈值触发时进行阶段性总结→程序记忆。
   */
  static async persistSessionMemory(
    runtime: BrowserRuntimeState,
    state: AgentState | null,
    onUiLine: (line: string) => void
  ): Promise<void> {
    // FINISHED 始终记录；开启 sessionMemoryRecordUnfinished 时，FAILED / 用户中断（同样落到 FAILED）也记录，
    // 但需满足「有实质进展」——动作数达到 sessionMemoryUnfinishedMinSteps，滤掉刚开始就挂的无价值失败。
    const isFinished = runtime.phase === 'FINISHED';
    const recordUnfinished = vConfig.sessionMemoryRecordUnfinished &&
      runtime.phase === 'FAILED' &&
      runtime.actionHistory.length >= vConfig.sessionMemoryUnfinishedMinSteps;
    const recordable = isFinished || recordUnfinished;
    hilog.debug(DOMAIN, TAG, 'persistSessionMemory decision phase=%{private}s recordable=%{private}s actions=%{private}d',
      runtime.phase, `${recordable}`, runtime.actionHistory.length);
    if (!vConfig.sessionMemoryEnabled || runtime.sessionKey.length <= 0 || !recordable) {
      if (vConfig.sessionMemoryRecordUnfinished && runtime.phase === 'FAILED' && !recordUnfinished) {
        hilog.info(DOMAIN, TAG, 'unfinished task skipped: steps=%{public}d < min=%{public}d',
          runtime.actionHistory.length, vConfig.sessionMemoryUnfinishedMinSteps);
      }
      return;
    }
    try {
      await SessionMemoryService.storeSessionResult(runtime, state);

      // 检查是否需要触发阶段性总结 → 程序记忆
      if (vConfig.proceduralMemoryEnabled && vConfig.proceduralMemoryStoreEnabled &&
        vConfig.sessionSummarizeThreshold > 0) {
        const entryCount = await SessionMemoryService.getEntryCount(runtime.sessionKey);
        if (entryCount > 0 && entryCount % vConfig.sessionSummarizeThreshold === 0) {
          const summarizeLine = await BrowserExecutionMemoryService.summarizeSessionToProcedural(runtime.sessionKey);
          if (summarizeLine.length > 0) {
            onUiLine(summarizeLine);
          }
        }
      }
    } catch (error) {
      hilog.warn(DOMAIN, TAG, 'session memory storage failed (non-fatal): %{private}s',
        error instanceof Error ? error.message : `${error}`);
    }
  }

  // ---- private helpers ----

  private static async shouldSearchProceduralMemory(runtime: BrowserRuntimeState): Promise<boolean> {
    const strategy = vConfig.proceduralMemorySearchStrategy;
    if (strategy === 'always') {
      hilog.info(DOMAIN, TAG, 'shouldSearchProceduralMemory: strategy=always → true');
      return true;
    }
    if (strategy === 'off') {
      hilog.info(DOMAIN, TAG, 'shouldSearchProceduralMemory: strategy=off → false');
      return false;
    }
    // 'first_turn': 只有当 session 中尚无记忆条目时才搜索
    const entryCount = await SessionMemoryService.getEntryCount(runtime.sessionKey);
    hilog.info(DOMAIN, TAG, 'shouldSearchProceduralMemory: strategy=%{private}s sessionEntries=%{public}d → %{private}s',
      strategy, entryCount, `${entryCount <= 0}`);
    return entryCount <= 0;
  }

  private static resolveMemoryUserId(): string {
    return MemoryLocalConfig.USER_ID_DEFAULT_FALLBACK;
  }

  private static buildLegacyBundle(runtime: BrowserRuntimeState): MemoryPayloadBundle {
    const bundle = new MemoryPayloadBundle();
    bundle.messages = BrowserExecutionMemoryService.buildLegacyMessages(runtime);
    bundle.toolCalls = BrowserExecutionMemoryService.buildLegacyToolCalls(runtime.actionHistory);
    bundle.metadata = BrowserExecutionMemoryService.buildLegacyMetadata(runtime);
    return bundle;
  }

  private static buildLegacyMessages(runtime: BrowserRuntimeState): MemoryMessagePayload[] {
    const userPayload = new MemoryMessagePayload();
    userPayload.role = 'user';
    userPayload.content = runtime.task;

    const assistantPayload = new MemoryMessagePayload();
    assistantPayload.role = 'assistant';
    const lines: string[] = [];
    lines.push(`## 任务结果`);
    lines.push(runtime.lastSummary.length > 0 ? runtime.lastSummary : '任务完成');

    if (runtime.actionHistory.length > 0) {
      lines.push('');
      lines.push('## 工作流动作链');
      const actionPreview = runtime.actionHistory.slice(0, 20);
      for (let i = 0; i < actionPreview.length; i++) {
        lines.push(`${i + 1}. ${actionPreview[i]}`);
      }
      if (runtime.actionHistory.length > actionPreview.length) {
        lines.push(`... 共 ${runtime.actionHistory.length} 步`);
      }
    }

    if (runtime.noteHistory.length > 0) {
      lines.push('');
      lines.push('## 关键观察');
      const notePreview = runtime.noteHistory.slice(0, 10);
      for (let i = 0; i < notePreview.length; i++) {
        lines.push(`- ${notePreview[i]}`);
      }
    }

    assistantPayload.content = lines.join('\n');
    return [userPayload, assistantPayload];
  }

  private static buildLegacyToolCalls(actionHistory: string[]): MemoryToolCallPayload[] {
    const toolCalls: MemoryToolCallPayload[] = [];
    const seen: Set<string> = new Set();
    for (let i = 0; i < actionHistory.length; i++) {
      const action = actionHistory[i].trim();
      if (action.length <= 0) {
        continue;
      }
      const actionPrefix = action.split(/\s+/)[0];
      if (actionPrefix.length <= 0 || seen.has(actionPrefix)) {
        continue;
      }
      seen.add(actionPrefix);
      const toolCall = new MemoryToolCallPayload();
      toolCall.tool = actionPrefix;
      toolCall.name = actionPrefix;
      toolCalls.push(toolCall);
    }
    return toolCalls;
  }

  private static buildLegacyMetadata(runtime: BrowserRuntimeState): Record<string, string> {
    const metadata: Record<string, string> = {};
    metadata.task = runtime.task;
    metadata.success = runtime.errorMessage.length > 0 ? 'false' : 'true';
    metadata.website = runtime.currentPerception?.currentUrl ?? '';
    metadata.workflow_step_count = `${runtime.actionHistory.length}`;
    metadata.source = 'browser_agent_runtime_legacy';
    return metadata;
  }

  /**
   * 将 session 阶段性记忆总结为程序记忆并存储。
   */
  private static async summarizeSessionToProcedural(sessionKey: string): Promise<string> {
    try {
      const entries = await SessionMemoryService.getSessionEntriesRaw(sessionKey, vConfig.sessionSummarizeThreshold);
      if (entries.length <= 0) {
        return '';
      }

      // 合并所有 session 条目的 task 和 result
      const tasks: string[] = [];
      const results: string[] = [];
      const allActions: Set<string> = new Set();
      for (let i = 0; i < entries.length; i++) {
        const payload = entries[i].payload;
        const title = (typeof payload.title === 'string') ? (payload.title as string) : '';
        if (title.length > 0) {
          tasks.push(title);
        }
        const contentRaw = (typeof payload.content === 'string') ? (payload.content as string) : '';
        if (contentRaw.length > 0) {
          try {
            const parsed = JSON.parse(contentRaw) as Record<string, string[]>;
            const rs = parsed.resultSummary;
            if (rs && rs.length > 0 && rs[0].length > 0) {
              results.push(rs[0]);
            }
            const actions = parsed.actionSummary;
            if (actions) {
              for (let j = 0; j < actions.length; j++) {
                const prefix = actions[j].trim().split(/\s+/)[0];
                if (prefix.length > 0) {
                  allActions.add(prefix);
                }
              }
            }
          } catch (_e) {
            // skip malformed
          }
        }
      }

      if (tasks.length <= 0) {
        return '';
      }

      // 构建 messages
      const userPayload = new MemoryMessagePayload();
      userPayload.role = 'user';
      userPayload.content = tasks.join('\n');

      const assistantPayload = new MemoryMessagePayload();
      assistantPayload.role = 'assistant';
      const lines: string[] = ['## 会话阶段性总结'];
      for (let i = 0; i < results.length; i++) {
        lines.push(`${i + 1}. ${results[i]}`);
      }
      assistantPayload.content = lines.join('\n');

      // 构建 toolCalls
      const toolCalls: MemoryToolCallPayload[] = [];
      const actionIter = allActions.values();
      let next = actionIter.next();
      while (!next.done) {
        const tc = new MemoryToolCallPayload();
        tc.tool = next.value;
        tc.name = next.value;
        toolCalls.push(tc);
        next = actionIter.next();
      }

      // 构建 metadata
      const metadata: Record<string, string> = {};
      metadata.task = tasks.join('; ');
      metadata.success = 'true';
      metadata.source = 'session_summarization';
      metadata.session_entry_count = `${entries.length}`;

      const memoryUserId = BrowserExecutionMemoryService.resolveMemoryUserId();
      const addResult = await MemoryClient.addProceduralMemory(
        memoryUserId,
        [userPayload, assistantPayload],
        toolCalls,
        metadata
      );
      if (addResult.responseCode === 200) {
        hilog.info(DOMAIN, TAG, 'session summarization to procedural memory success: entries=%{public}d', entries.length);
        return `[程序记忆] 本地会话阶段性总结已写入 (${entries.length} 条合并)`;
      }
      hilog.warn(DOMAIN, TAG, 'session summarization failed: code=%{public}d', addResult.responseCode);
      return `[程序记忆] 本地会话总结写入失败 code=${addResult.responseCode}`;
    } catch (error) {
      const msg = error instanceof Error ? error.message : `${error}`;
      hilog.warn(DOMAIN, TAG, 'session summarization exception (non-fatal): %{private}s', msg);
      return '';
    }
  }
}
