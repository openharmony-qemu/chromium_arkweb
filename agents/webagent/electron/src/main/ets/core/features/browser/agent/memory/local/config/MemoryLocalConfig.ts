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

// 本地记忆模块的运行时配置（替代 procmemory_for_browser/config/utils.py 的 yaml/initial_mem0_and_llm 中浏览器分支）。
// 端上不再走云端 vector / embedding，只保留与 add/search 行为决策相关的开关。

export class MemoryLocalConfig {
  // 默认 user_id（与 MemoryClient 上层传入对齐；调用方一般会传具体 sessionKey）
  public defaultUserId: string = 'browser_fixed_user';

  // 默认 agent_id（沿用 MemoryClient.AGENT_ID = 'ultron_browser_agent'）
  public defaultAgentId: string = 'ultron_browser_agent';

  // 端侧最小可用阶段固定关闭话题切分
  public enableTopicSegmentation: boolean = false;

  // 与 Python `use_raw_user_query_as_title=True` 等价：
  //   true  → title = state.task / 用户原话，summarized_title 落 metadata（让 BM25 直接命中用户原话）
  //   false → title = LLM 抽取的 summarized_title（旧行为）
  public useRawUserQueryAsTitle: boolean = true;

  // 与 Python `use_title_for_embedding` 等价：BM25-only 路径下不影响行为，但保留字段以便日后扩展
  public useTitleForEmbedding: boolean = false;

  // role=tool 抽取阶段是否按 (name, location) 去重
  public deduplicateToolCallsOnExtract: boolean = true;

  // 单 user 记忆条目上限（防止误用导致 list 爆炸；超出则按 created_at 截断）
  public maxStoredItemsPerUser: number = 2000;

  // 候选检索（add 流程）的 top-k
  public addRetrieveCandidateTopK: number = 5;

  // 默认 search 限制
  public defaultSearchLimit: number = 5;
  public defaultSearchThreshold: number = 0.0; // BM25 分数无固定上下限，默认不做阈值过滤

  // 全局存储文件大小上限（字节），超限时触发老化清理 session_memory
  public storageLimitBytes: number = 20 * 1024 * 1024; // 20 MB

  // 是否启用全局存储老化（调试用开关）
  public storageAgingEnabled: boolean = true;

  // 内部固定常量
  public static readonly USER_ID_DEFAULT_FALLBACK: string = 'browser_fixed_user';

  static defaults(): MemoryLocalConfig {
    return new MemoryLocalConfig();
  }
}
