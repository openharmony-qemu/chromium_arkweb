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

// 会话级话题记忆服务 (Session Memory / 话题接续)
//
// 复用 MemoryStore 的存储和检索能力，通过 type='session_memory' + user_id=sessionKey 区分。
// 跳过 AddPipeline / SearchPipeline 的 LLM 管道，直接操作 MemoryStore。
// 检索方式：取当前 session 最近 N 条，不做 BM25。

import { hilog } from '@kit.PerformanceAnalysisKit';
import { MemoryStore, MemoryStoreFilters } from '../local/store/MemoryStore';
import { MemoryTypeName } from '../local/domain/Constants';
import { StoredMemoryPayload, StoredMemoryView } from '../local/domain/MemoryItemModel';
import { BrowserRuntimeState } from '../../../../../shared/domain/BrowserAutomationTypes';
import { AgentState } from '../../core/AgentState';

const DOMAIN = 0x6020;
const TAG = 'SessionMemory';
const MAX_SUMMARY_LENGTH = 2000;
const MAX_ACTIONS_PER_ENTRY = 10;
const MAX_ENTRIES_PER_SESSION = 10;

export class SessionMemoryService {
  private static turnCounters: Map<string, number> = new Map<string, number>();

  /**
   * 任务完成后存储 session 记忆条目。
   * 仅从 runtime 状态提取数据，不读取 runtime.sessionMemory / runtime.proceduralMemory。
   */
  static async storeSessionResult(runtime: BrowserRuntimeState, agentState: AgentState | null): Promise<void> {

    await MemoryStore.init();
    const sessionKey = runtime.sessionKey;
    if (sessionKey.length <= 0) {
      return;
    }
    hilog.info(
      DOMAIN,
      TAG,
      'storeSessionResult begin phase=%{public}s actions=%{public}d entriesBefore=%{public}d storePath=%{public}s',
      runtime.phase,
      runtime.actionHistory.length,
      MemoryStore.getSessionMemoryEntryCount(),
      MemoryStore.getStorePath()
    );

    // 提取结构化数据（仅来自 runtime 状态，不含任何检索结果）
    const task = runtime.task.length > 0 ? runtime.task : '';
    const rawResult = runtime.lastSummary.length > MAX_SUMMARY_LENGTH
      ? runtime.lastSummary.substring(0, MAX_SUMMARY_LENGTH)
      : runtime.lastSummary;
    const resultSummary = SessionMemoryService.cleanResultSummary(rawResult);

    const actionSummary: string[] = [];
    const allActions = runtime.actionHistory;
    const startIdx = allActions.length > MAX_ACTIONS_PER_ENTRY
      ? allActions.length - MAX_ACTIONS_PER_ENTRY
      : 0;
    for (let i = startIdx; i < allActions.length; i++) {
      actionSummary.push(allActions[i]);
    }

    const noteSummary = SessionMemoryService.cleanNotes(runtime.noteHistory, MAX_ACTIONS_PER_ENTRY);

    // 计算 turnIndex（需在构建 content 之前）
    const existingFilters = new MemoryStoreFilters();
    existingFilters.userId = sessionKey;
    existingFilters.type = MemoryTypeName.SESSION;
    const existing = MemoryStore.list(existingFilters, -1);
    const turnIndex = SessionMemoryService.getNextTurnIndex(sessionKey, existing);

    // 从 agentState 提取补充字段（null 时用空字符串兜底）
    const refinedTask = agentState ? agentState.task : '';
    const resultStatus = agentState ? agentState.result.status : '';
    const answer = agentState ? agentState.result.answer : '';
    const errorFromState = agentState ? agentState.errorMessage : '';
    const analysis = agentState ? agentState.analysis : '';
    const reference = agentState ? agentState.result.reference : '';
    const startURL = agentState ? agentState.startUrl : '';
    const finishedAt = new Date().toISOString();

    // 构建 content JSON
    const contentObj: Record<string, string[]> = {
      'resultSummary': [resultSummary],
      'actionSummary': actionSummary,
      'noteSummary': noteSummary,
      'refinedTask': [refinedTask],
      'status': [resultStatus],
      'answer': [answer],
      'errorMessage': [errorFromState],
      'analysis': [analysis],
      'reference': [reference],
      'startURL': [startURL],
      'finishedAt': [finishedAt],
      'taskPhase': [runtime.stopRequested ? 'STOPPED' : runtime.phase],
      'taskID': [`${sessionKey}_${turnIndex}`]
    };
    const contentStr = JSON.stringify(contentObj);

    // 生成 id
    const id = SessionMemoryService.makeId(sessionKey, turnIndex);

    const now = new Date().toISOString();
    const payload = new StoredMemoryPayload();
    payload.userId = sessionKey;
    payload.agentId = 'ultron_browser_agent';
    payload.type = MemoryTypeName.SESSION;
    payload.subType = '话题接续';
    payload.subSubType = '';
    payload.title = task;
    payload.content = contentStr;
    payload.summarizedTitle = '';
    payload.createdAt = now;
    payload.updatedAt = now;
    payload.oldMemory = '';
    payload.scenarioId = '';
    payload.toolsJson = '[]';
    payload.historyJson = '{}';
    payload.sourceTraceJson = '[]';

    await MemoryStore.insert(id, payload);

    // 淘汰超限的旧条目
    if (existing.length + 1 > MAX_ENTRIES_PER_SESSION) {
      await SessionMemoryService.evictOldest(sessionKey, existing);
    }
  }

  /**
   * 获取当前 session 最近 N 条记忆，格式化为 markdown。
   * 返回空字符串表示无数据。
   */
  static async getSessionMemory(sessionKey: string, lastN: number = 5): Promise<string> {
    await MemoryStore.init();
    if (sessionKey.length <= 0) {
      return '';
    }
    hilog.debug(DOMAIN, TAG, 'getSessionMemory begin lastN=%{public}d storePath=%{private}s',
      lastN, MemoryStore.getStorePath());

    const filters = new MemoryStoreFilters();
    filters.userId = sessionKey;
    filters.type = MemoryTypeName.SESSION;
    const all = MemoryStore.list(filters, -1);
    if (all.length <= 0) {
      hilog.debug(DOMAIN, TAG, 'getSessionMemory empty storePath=%{private}s',
        MemoryStore.getStorePath());
      return '';
    }

    // 按 createdAt 升序显式排序，取尾部 lastN 条为「最近」。
    // 不再依赖 MemoryStore.list 的插入顺序（跨重启 / 与程序记忆交错时该顺序不可靠）。
    SessionMemoryService.sortByCreatedAtAsc(all);
    const startIdx = all.length > lastN ? all.length - lastN : 0;
    const recent = all.slice(startIdx);

    const lines: string[] = [
      '## 话题上下文 (当前会话)',
      '以下是本次会话中最近执行的任务（按时间顺序，可能含未完成/失败任务，已在其状态中标注）。用户的新请求可能基于这些上下文。',
      '请结合这些历史理解用户的真实意图，合理规划执行步骤。'
    ];
    for (let i = 0; i < recent.length; i++) {
      const view = recent[i];
      const payload = view.payload;
      const task = (typeof payload.title === 'string') ? payload.title as string : '';
      const contentRaw = (typeof payload.content === 'string') ? payload.content as string : '';

      let resultSummary = '';
      let taskPhase = '';
      let errorMessage = '';
      const actions: string[] = [];
      const notes: string[] = [];
      if (contentRaw.length > 0) {
        try {
          const parsed = JSON.parse(contentRaw) as Record<string, string[]>;
          const rs = parsed.resultSummary;
          if (rs && rs.length > 0) {
            resultSummary = rs[0];
          }
          const tp = parsed.taskPhase;
          if (tp && tp.length > 0) {
            taskPhase = tp[0];
          }
          const em = parsed.errorMessage;
          if (em && em.length > 0) {
            errorMessage = em[0];
          }
          const as_ = parsed.actionSummary;
          if (as_) {
            for (let j = 0; j < as_.length; j++) {
              actions.push(as_[j]);
            }
          }
          const ns = parsed.noteSummary;
          if (ns) {
            for (let j = 0; j < ns.length; j++) {
              if (ns[j].trim().length > 0) {
                notes.push(ns[j]);
              }
            }
          }
        } catch (_e) {
          resultSummary = contentRaw.length > 200 ? contentRaw.substring(0, 200) : contentRaw;
        }
      }

      const idx = i + 1;
      lines.push(`${idx}. **任务**: ${task.length > 0 ? task : '(未知)'}`);
      // 未完成任务（非 FINISHED）标注状态，避免模型把残缺结果当作已完成的事实。
      if (taskPhase.length > 0 && taskPhase !== 'FINISHED') {
        if (taskPhase === 'STOPPED') {
          // 用户主动中断：与「失败」区分，可能只是改变主意，不代表任务本身办不成。
          lines.push('   **状态**: ⏹️ 该任务被用户主动中断，可能未完成，其结果仅供参考。');
        } else {
          // FAILED：区分基础设施错误（浏览器/连接问题）与业务原因，前者对规划无意义，不透传技术细节。
          let reason = '';
          if (errorMessage.length > 0) {
            reason = SessionMemoryService.isInfraError(errorMessage)
              ? '（因浏览器/连接中断，非任务本身逻辑问题）'
              : `（原因：${errorMessage.length > 100 ? errorMessage.substring(0, 100) + '...' : errorMessage}）`;
          }
          lines.push(`   **状态**: ⚠️ 该任务未完成/失败${reason}，其结果仅供参考，不能当作已成功的事实。`);
        }
      }
      if (resultSummary.length > 0) {
        lines.push(`   **结果**: ${resultSummary}`);
      }
      if (notes.length > 0) {
        lines.push(`   **已收集的信息**:`);
        for (let n = 0; n < notes.length; n++) {
          const noteText = notes[n].length > 200 ? notes[n].substring(0, 200) + '...' : notes[n];
          lines.push(`     - ${noteText}`);
        }
      }
      if (actions.length > 0) {
        const actionPreview = actions.slice(0, 5).join(' → ');
        lines.push(`   **关键操作**: ${actionPreview}`);
      }
    }

    return lines.join('\n');
  }

  /**
   * 清除指定 session 的所有记忆条目。
   */
  static async clearSession(sessionKey: string): Promise<void> {
    await MemoryStore.init();
    if (sessionKey.length <= 0) {
      return;
    }
    const filters = new MemoryStoreFilters();
    filters.userId = sessionKey;
    filters.type = MemoryTypeName.SESSION;
    const removed = await MemoryStore.clean(filters);
    SessionMemoryService.turnCounters.delete(sessionKey);
  }

  /**
   * 获取指定 session 的记忆条目数量（轻量，不格式化）。
   */
  static async getEntryCount(sessionKey: string): Promise<number> {
    await MemoryStore.init();
    if (sessionKey.length <= 0) {
      return 0;
    }
    const filters = new MemoryStoreFilters();
    filters.userId = sessionKey;
    filters.type = MemoryTypeName.SESSION;
    return MemoryStore.list(filters, -1).length;
  }

  /**
   * 获取指定 session 最近 N 条原始数据（不格式化为 markdown）。
   */
  static async getSessionEntriesRaw(sessionKey: string, lastN: number): Promise<StoredMemoryView[]> {
    await MemoryStore.init();
    if (sessionKey.length <= 0) {
      return [];
    }
    hilog.debug(DOMAIN, TAG, 'getSessionEntriesRaw begin lastN=%{public}d storePath=%{private}s',
      lastN, MemoryStore.getStorePath());
    const filters = new MemoryStoreFilters();
    filters.userId = sessionKey;
    filters.type = MemoryTypeName.SESSION;
    const all = MemoryStore.list(filters, -1);
    SessionMemoryService.sortByCreatedAtAsc(all);
    const startIdx = all.length > lastN ? all.length - lastN : 0;
    return all.slice(startIdx);
  }

  // ---------- 内部工具 ----------

  // 判定 errorMessage 是否为基础设施/连接类错误（对任务规划无参考价值）。
  private static isInfraError(msg: string): boolean {
    const lower = msg.toLowerCase();
    return lower.includes('websocket close') ||
      lower.includes('shutting down') ||
      lower.includes('control_state_idle') ||
      lower.includes('connection') ||
      lower.includes('transport') ||
      lower.includes('disconnect');
  }

  // 按 payload.createdAt（ISO 字符串，字典序即时间序）升序排序。
  private static sortByCreatedAtAsc(views: StoredMemoryView[]): void {
    views.sort((a: StoredMemoryView, b: StoredMemoryView): number => {
      const ca = (typeof a.payload.createdAt === 'string') ? a.payload.createdAt as string : '';
      const cb = (typeof b.payload.createdAt === 'string') ? b.payload.createdAt as string : '';
      if (ca < cb) {
        return -1;
      }
      if (ca > cb) {
        return 1;
      }
      return 0;
    });
  }

  private static getNextTurnIndex(sessionKey: string, existing: StoredMemoryView[]): number {
    let counter = SessionMemoryService.turnCounters.get(sessionKey);
    if (counter === undefined) {
      let maxIndex = -1;
      for (let i = 0; i < existing.length; i++) {
        const idStr = existing[i].id;
        const lastUnderscore = idStr.lastIndexOf('_');
        if (lastUnderscore >= 0) {
          const parsed = Number(idStr.substring(lastUnderscore + 1));
          if (!Number.isNaN(parsed) && parsed > maxIndex) {
            maxIndex = parsed;
          }
        }
      }
      counter = maxIndex + 1;
    }
    SessionMemoryService.turnCounters.set(sessionKey, counter + 1);
    return counter;
  }

  /**
   * 清理 resultSummary：去掉参考链接段落（## 参考 / --- 后面的引用部分），只保留正文。
   */
  private static cleanResultSummary(raw: string): string {
    if (raw.length <= 0) {
      return raw;
    }
    // 去掉 markdown 参考段落
    const refIdx = raw.indexOf('\n---\n');
    if (refIdx >= 0) {
      return raw.substring(0, refIdx).trim();
    }
    const refHeader = raw.indexOf('\n## 参考');
    if (refHeader >= 0) {
      return raw.substring(0, refHeader).trim();
    }
    const refHeader2 = raw.indexOf('\n## References');
    if (refHeader2 >= 0) {
      return raw.substring(0, refHeader2).trim();
    }
    return raw;
  }

  /**
   * 清理 notes：去重、截断、只保留有信息量的条目。
   */
  private static cleanNotes(allNotes: string[], maxCount: number): string[] {
    const startIdx = allNotes.length > maxCount
      ? allNotes.length - maxCount
      : 0;
    const seen: Set<string> = new Set();
    const result: string[] = [];
    for (let i = allNotes.length - 1; i >= startIdx; i--) {
      const note = allNotes[i].trim();
      if (note.length <= 0) {
        continue;
      }
      const lower = note.toLowerCase();
      if (lower === 'no' || lower === 'none' || lower === 'null') {
        continue;
      }
      // 去重：取前 80 字符作为指纹，避免近似重复
      const fingerprint = note.length > 80 ? note.substring(0, 80) : note;
      if (seen.has(fingerprint)) {
        continue;
      }
      seen.add(fingerprint);
      // 每条 note 限制 300 字符
      const truncated = note.length > 300 ? note.substring(0, 300) + '...' : note;
      result.unshift(truncated);
    }
    return result;
  }

  private static makeId(sessionKey: string, turnIndex: number): string {
    // 简单拼接 + hash 避免特殊字符
    const raw = `session_${sessionKey}_turn_${turnIndex}`;
    let hash = 0;
    for (let i = 0; i < raw.length; i++) {
      const ch = raw.charCodeAt(i);
      hash = ((hash << 5) - hash) + ch;
      hash = hash & hash; // 转 32bit int
    }
    return `sm_${Math.abs(hash).toString(16)}_${turnIndex}`;
  }

  private static async evictOldest(sessionKey: string, existing: StoredMemoryView[]): Promise<void> {
    // 按插入序，删除最旧的条目直到数量回到上限
    const excess = existing.length + 1 - MAX_ENTRIES_PER_SESSION;
    for (let i = 0; i < excess && i < existing.length; i++) {
      await MemoryStore.delete(existing[i].id);
    }
    hilog.info(DOMAIN, TAG, 'session memory evicted %{public}d oldest entries', excess);
  }
}
