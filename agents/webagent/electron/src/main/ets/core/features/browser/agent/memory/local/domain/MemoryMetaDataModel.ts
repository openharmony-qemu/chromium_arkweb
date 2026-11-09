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

// 对齐 mem0/mem0/memory/main.py 中的 MemoryMetaData dataclass。

export class MemorySourceTraceEntry {
  public scenarioId: string = '';
  public operation: string = '';
  public timestamp: string = '';
  public queryType: string = '';
  public originalTurns: number = 0;
}

export class MemoryMetaDataModel {
  public userId: string = '';
  // 主分类 —— "semantic_memory" / "procedural_memory"
  public type: string = '';
  // subType：程序记忆固定填 "程序记忆"
  public subType: string = '';
  // subSubType：程序记忆 category（如 scene_mode/page_interaction）
  public subSubType: string = '';
  public createdAt: string = '';
  public updatedAt: string = '';
  public oldMemory: string = '';
  public agentId: string = '';
  public summarizedTitle: string = '';
  public scenarioId: string = '';
  public sourceTrace: MemorySourceTraceEntry[] = [];

  static create(userId: string, agentId: string): MemoryMetaDataModel {
    const now = MemoryMetaDataModel.nowIso();
    const meta = new MemoryMetaDataModel();
    meta.userId = userId;
    meta.agentId = agentId;
    meta.createdAt = now;
    meta.updatedAt = now;
    return meta;
  }

  static nowIso(): string {
    // 与 Python pytz Asia/Shanghai isoformat 不严格一致，但在端上用于排序/审计已足够。
    // 用本地时间 + +08:00 偏移近似（鸿蒙端默认不一定就是 +08:00，这里硬编码以便日志排序）。
    const d = new Date();
    const utcMs = d.getTime();
    const shanghaiMs = utcMs + 8 * 3600 * 1000;
    const sd = new Date(shanghaiMs);
    const yyyy = sd.getUTCFullYear();
    const mm = String(sd.getUTCMonth() + 1).padStart(2, '0');
    const dd = String(sd.getUTCDate()).padStart(2, '0');
    const HH = String(sd.getUTCHours()).padStart(2, '0');
    const MM = String(sd.getUTCMinutes()).padStart(2, '0');
    const SS = String(sd.getUTCSeconds()).padStart(2, '0');
    return `${yyyy}-${mm}-${dd}T${HH}:${MM}:${SS}+08:00`;
  }
}
