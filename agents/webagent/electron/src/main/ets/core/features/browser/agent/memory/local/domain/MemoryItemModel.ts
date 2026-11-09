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

// 对齐 mem0/mem0/memory/main.py 中的 MemoryItem dataclass。
// title 为检索索引（30-50 字），content 为结构化描述（150-300 字 markdown），
// tools 为最近一次工具调用清单，history 为 {tool_signature_json: count}。

import { MemoryMetaDataModel } from './MemoryMetaDataModel';

export class ToolCallEntry {
  public name: string = '';
  public args: Record<string, Object> = {};
  public argsConcreteHint: Record<string, Object> = {};
  public result: string = '';
}

export class MemoryItemModel {
  public title: string = '';
  public content: string = '';
  public tools: ToolCallEntry[] = [];
  public history: Record<string, number> = {};
  public memId: string = 'unknown';
  public metadata: MemoryMetaDataModel = new MemoryMetaDataModel();
}

// 持久化到 store 的 payload 形态（与 Python self.vector_store.payload 对齐：扁平字段 + tools/history/sourceTrace）。
// MemoryStore 内部把它存为单条 JSON 记录的 payload 字段。
export class StoredMemoryPayload {
  public userId: string = '';
  public agentId: string = '';
  public type: string = '';
  public subType: string = '';
  public subSubType: string = '';
  public title: string = '';
  public content: string = '';
  public summarizedTitle: string = '';
  public createdAt: string = '';
  public updatedAt: string = '';
  public oldMemory: string = '';
  public scenarioId: string = '';
  // 复杂字段以 JSON 字符串形式落库；运行时解析为对象（避免 ArkTS 严格类型对 unknown shape 的限制）
  public toolsJson: string = '[]';
  public historyJson: string = '{}';
  public sourceTraceJson: string = '[]';
}

// 从 store 读出后的内存形态（id + payload + 已解析的 tools/history/sourceTrace）
export class StoredMemoryRecord {
  public id: string = '';
  public payload: StoredMemoryPayload = new StoredMemoryPayload();
}

// list/search 返回的最小信息（payload 已展开为 Record，避免后续重复解析）
export class StoredMemoryView {
  public id: string = '';
  public payload: Record<string, Object> = {};
}
