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

// 程序记忆"四元决策 / consolidated summary / rerank" 三条 prompt 与组装函数。
// 直接迁自 procmemory_for_browser/config/prompt_engine/prompts.py：
//   PROCEDURAL_MEMORY_CONSOLIDATED_SUMMARY_PROMPT
//   PROCEDURAL_MEMORY_RERANK_PROMPT
//   PROCEDURAL_MEMORY_UPDATE_DECISION_PROMPT
//   _format_history_summary
//   get_procedural_memory_update_messages
import {hilog} from '@kit.PerformanceAnalysisKit';
const DOMAIN = 0x6020;
const TAG = 'MemoryDecisionPrompts';

export const PROCEDURAL_MEMORY_CONSOLIDATED_SUMMARY_PROMPT: string =
`你是智能助手的记忆模块。请基于用户请求和检索到的历史记忆，同时完成：1）内容总结 2）工具过滤。

用户请求：{user_query}
当前场景：{context}

检索到的相关记忆：
{memories}

候选工具调用（保留具体参数，按调用签名去重）：
{all_tools}

请一次性完成以下任务，输出 JSON：
{
    "title": "一句话标题，概括用户意图（如：客厅灯光偏好设置）",
    "content": "综合所有记忆的偏好描述（50字以内）",
    "suggested_action": "建议的执行方案（结合场景和偏好）",
    "confidence": "high/medium/low",
    "relevant_tool_refs": ["T1", "T3"],
    "tool_selection_reason": "工具选择的简短理由"
}

工具过滤原则：
- 必须优先输出 relevant_tool_refs（来自候选列表中的 ref），不要只给工具名
- 同名工具如果参数不同（如 14:00 vs 15:00），必须按当前 query 选择正确参数对应的 ref
- 例如：用户说"15:00断网"，应选择包含 time=15:00 的 ref，而不是同名的其他参数版本
- 如果用户意图涉及多设备联动（如"回家模式"），可以选择多个工具
- 如果无法确定，宁可多选也不要漏选
- 工具数量约束：仔细分析用户 query 涉及的设备/操作数量，推荐的工具数应与之匹配。例如用户只提到一个设备的一种操作，不应推荐多个不同工具名的工具；同一工具名只推荐一个最匹配的参数版本，不要把同一工具的多个历史参数变体都返回

内容总结原则：
- title 应该像记忆标题一样简洁
- content 应该整合多条记忆的关键偏好，而不是简单罗列
- 如果多条记忆有冲突的偏好，以场景匹配度更高的为准
- 检索出的多条记忆不一定与用户的所有请求都相关，甚至可能产生干扰。 相关记忆中也可能包含与用户请求无关的额外工具， 请在回复和总结的时候注意剔除。
`;

export const PROCEDURAL_MEMORY_RERANK_PROMPT: string =
`你是记忆检索重排器。

任务：根据用户当前的意图和场景，对检索到的候选记忆进行重新排序。

排序原则（按优先级）：
1. 场景匹配：当前场景信息（位置、时间、季节等）与记忆中的场景越接近越优先
2. 意图匹配：用户要执行的操作与记忆中的操作一致
3. 偏好相关：记忆中包含用户偏好/习惯的优先（如参数设置等）
4. Function calling 可复用：记忆中的工具调用参数可以直接用于当前场景的优先

输入信息：
- 用户意图：{user_query}
- 改写后查询：{rewritten_query}
- 当前场景：{context}
- 候选记忆：
{candidates}

输出 JSON 格式：
{
    "ranking": ["记忆ID1", "记忆ID2", ...],
    "reason": "简短说明排序理由"
}
`;

export const PROCEDURAL_MEMORY_UPDATE_DECISION_PROMPT: string =
`你是智能家居程序记忆管理器，负责决定新程序记忆与现有记忆的关系。

任务：比较新记忆与现有记忆，返回一个操作决策。

现有记忆附带信息：
- title / content：记忆的语义标题和详细描述
- lastOperation：该记忆最近一次实际执行的工具调用
- historySummary：该记忆的历史调用频次统计（如"制冷18度: 9次(90%); 制热30度: 1次(10%)"）

四种操作：
1. NONE：新记忆与现有记忆的场景意图等价，工具调用参数也相同
   - 即使用户措辞略有不同（如"打开空调"vs"开空调"），只要核心操作参数相同，就返回 NONE
   - 系统会自动累加该操作的调用频次，无需你处理
   - 示例：旧[0]="场景模式 游戏模式-卧室灯关+厨房灯关+客厅灯5%" + 新="场景模式 游戏模式-客厅灯5%+卧室灯关+厨房灯关" → NONE

2. ADD：全新场景，现有记忆中没有相似内容
   - 仅当现有记忆为空，或新记忆是完全不同的场景时才使用

3. UPDATE：同一场景但工具调用参数有实质性变化（如温度、亮度、模式等不同）
   - 示例：旧[0]="设备偏好 空调-制冷18度"（历史：制冷18度9次） + 新="设备偏好 空调-制热30度" → UPDATE index=0
   - content 生成规则：UPDATE 时生成的 content 使用 markdown 段落格式：
     - ## 用户意图（用户原始诉求）
     - ## 使用场景（何时触发此记忆）
     - ## 执行步骤（最新参数的设备操作清单）
     - ## 用户偏好（如有偏好表达）
     - ## 约束条件（复用限制）
     - 不要生成 "## 历史统计"（由系统自动维护）
   - 如果旧记忆是扁平文本格式，UPDATE 后改为 markdown 格式即可

4. DELETE：新记忆表明旧偏好被否定
   - 仅当用户明确表达"不喜欢"、"不对"、"太XX"、"以后不要"等否定词时使用
   - 强规则：若用户明确否定旧参数，且没有给出新的替代参数（只是否定，不是"改成XX"），优先 DELETE，不要判 UPDATE
   - 否定型新记忆（如"禁止26度/不要26度"）主要用于撤销旧偏好，不要求新增一条"替代偏好"记忆
   - 示例（必须 DELETE）：旧[0]="空调26度偏好" + 新="26度太冷了，以后不要26度"（未给新温度） → DELETE index=0
   - 示例：旧[0]="用户喜欢26度空调" + 新="用户说26度太冷了，以后不要" → DELETE index=0

判断优先级：NONE > UPDATE > DELETE > ADD
- 例外优先级（高于上面规则）：若命中"明确否定旧参数 + 未提供替代参数"，直接 DELETE
- 首先判断是否与某个现有记忆等价（场景相同且参数相同）→ NONE
- 其次判断是否是对某个现有记忆的更新（场景相同但参数不同）→ UPDATE
- 再次判断是否要删除某个旧记忆 → DELETE
- 最后，如果都不符合，才是全新记忆 → ADD
- 若新记忆是"否定约束"且已命中对应旧偏好，优先输出 DELETE，而不是把否定约束改写成新的参数型 UPDATE

输出格式（只返回一个操作）：
{
  "action": "<NONE|ADD|UPDATE|DELETE>",
  "target_index": <数字>,
  "title": "<新记忆标题>",
  "content": "<新记忆内容>",
  "reason": "<简短说明判断原因>"
}

示例：
- NONE: {"action": "NONE", "target_index": 0, "reason": "新旧记忆核心参数相同，系统自动累加频次"}
- ADD:  {"action": "ADD", "title": "...", "content": "## 用户意图\\n...", "reason": "全新场景"}
- UPDATE: {"action": "UPDATE", "target_index": 0, "title": "...", "content": "...", "reason": "参数变化"}
- DELETE: {"action": "DELETE", "target_index": 1, "reason": "用户明确否定了该偏好"}

注意：
- 仅返回 JSON，不要附加任何解释
- 只返回一个操作，不要返回多个
- 工具组一致性硬规则：比较新记忆 currentTools 与旧记忆 lastOperation 的工具名集合（仅比较 name，忽略参数）。若两个集合完全无交集，必须判 ADD，不允许 UPDATE 或 NONE。只有工具名集合存在交集时，才允许 UPDATE 或 NONE。
`;

type HistoryItem = {
  key: string;
  count: number;
};

function compareHistoryItemCountDesc(a: HistoryItem, b: HistoryItem): number {
  return b.count - a.count;
}

function truncateHistoryKey(keyStr: string): string {
  return keyStr.length > 50 ? keyStr.substring(0, 50) : keyStr;
}

function formatToolArgs(argsObj: Record<string, Object>): string {
  const argKeys: string[] = Object.keys(argsObj);
  const argParts: string[] = [];
  for (let i = 0; i < argKeys.length; i++) {
    const ak = argKeys[i];
    argParts.push(`${ak}=${argsObj[ak]}`);
  }
  return argParts.join(',');
}

function formatToolItem(item: Object): string {
  const obj = item as Record<string, Object>;
  const name = (typeof obj.name === 'string') ? obj.name as string : '';
  const argsObj = (typeof obj.args === 'object' && obj.args !== null) ?
    obj.args as Record<string, Object> : {};
  return `${name}(${formatToolArgs(argsObj)})`;
}

function formatToolArray(parsed: Object[]): string {
  const segments: string[] = [];
  for (let i = 0; i < parsed.length; i++) {
    const item = parsed[i];
    if (typeof item !== 'object' || item === null) {
      continue;
    }
    segments.push(formatToolItem(item));
  }
  return segments.join('+');
}

function formatHistoryKeyDescription(keyStr: string): string {
  try {
    const parsed = JSON.parse(keyStr);
    return Array.isArray(parsed) ? formatToolArray(parsed as Object[]) : truncateHistoryKey(keyStr);
  } catch (e) {
    hilog.warn(DOMAIN, TAG, 'formatHistoryKeyDescription error: %{public}s', e.message);
    return truncateHistoryKey(keyStr);
  }
}

// 与 Python _format_history_summary 等价：把 history dict 转可读文本。
// history 的 key 是 _make_history_key(tool_calls) 序列化后的 JSON，可能是 JSON 数组（多个工具）或对象。
export function formatHistorySummary(history: Record<string, number>): string {
  if (!history) {
    return '无历史记录';
  }
  const keys: string[] = Object.keys(history);
  if (keys.length <= 0) {
    return '无历史记录';
  }
  const items: HistoryItem[] = [];
  let total = 0;
  for (let i = 0; i < keys.length; i++) {
    const k = keys[i];
    const c = history[k] ?? 0;
    total += c;
    items.push({ key: k, count: c });
  }
  // 按 count 倒序
  items.sort(compareHistoryItemCountDesc);
  const limit = Math.min(items.length, 5);
  const parts: string[] = [];
  for (let i = 0; i < limit; i++) {
    const count = items[i].count;
    const desc = formatHistoryKeyDescription(items[i].key);
    const pct = total > 0 ? Math.floor(count * 100 / total) : 0;
    parts.push(`${desc}: ${count}次(${pct}%)`);
  }
  return `共${total}次调用。` + parts.join('；');
}

export class OldMemoryEntry {
  public id: string = '';
  public index: number = 0;
  public title: string = '';
  public content: string = '';
  public lastOperation: Object[] = [];
  public historySummary: string = '';
}

export class NewMemoryEntry {
  public title: string = '';
  public content: string = '';
  public currentTools: Object[] = [];
}

// 与 Python get_procedural_memory_update_messages 等价：组装四元决策 prompt。
export function buildProceduralMemoryUpdateMessages(
  retrievedOldMemory: OldMemoryEntry[],
  newTitle: string,
  newContent: string,
  newTools: Object[],
  customPrompt: string = ''
): string {
  const promptHead = customPrompt.length > 0 ? customPrompt : PROCEDURAL_MEMORY_UPDATE_DECISION_PROMPT;
  let oldMemoryStr: string;
  if (!retrievedOldMemory || retrievedOldMemory.length <= 0) {
    oldMemoryStr = '[] (无现有记忆)';
  } else {
    const list: Object[] = [];
    for (let i = 0; i < retrievedOldMemory.length; i++) {
      const mem = retrievedOldMemory[i];
      const entry: Record<string, Object> = {};
      entry.index = i;
      entry.id = mem.id;
      entry.title = mem.title;
      entry.content = mem.content;
      if (mem.lastOperation && mem.lastOperation.length > 0) {
        entry.lastOperation = mem.lastOperation;
      }
      if (mem.historySummary && mem.historySummary.length > 0) {
        entry.historySummary = mem.historySummary;
      }
      list.push(entry);
    }
    oldMemoryStr = JSON.stringify(list, null, 2);
  }
  const newDict: Record<string, Object> = {};
  newDict.title = newTitle;
  newDict.content = newContent;
  if (newTools && newTools.length > 0) {
    newDict.currentTools = newTools;
  }
  const newMemoryStr = JSON.stringify(newDict, null, 2);

  return `${promptHead}

**现有记忆**（每条记忆有唯一的 index，用于 UPDATE/DELETE/NONE 时引用）：
${oldMemoryStr}

**新记忆**（需要判断如何处理）：
${newMemoryStr}

请根据上述规则，返回一个 JSON 格式的操作决策。
`;
}
