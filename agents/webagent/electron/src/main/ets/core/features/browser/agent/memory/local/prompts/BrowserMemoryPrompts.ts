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

// 浏览器路径用到的程序记忆 prompt（端上版本）。
// 直接迁自 procmemory_for_browser/config/prompt_engine/prompts.py：
//   BROWSER_ADD_PROCEDURAL_MEMORY_PROMPT
//   BROWSER_PROCEDURAL_MEMORY_RETRIEVAL_PROMPT
//   BROWSER_PROCEDURAL_MEMORY_SUMMARY_PROMPT
// Python 里 f-string 占位符 `{{` / `}}` 在 .format() 后会变成字面 `{` / `}`，
// 所以这里直接用单层大括号；动态变量保留 `{key}` 占位符，由 MemoryLLMAdapter.formatTemplate 替换。

export const BROWSER_ADD_PROCEDURAL_MEMORY_PROMPT: string =
`你是浏览器自动化Agent的记忆整理助手。基于浏览器Agent完成单次任务的完整执行轨迹，输出结构化长期记忆。

=== category 分类规则（互斥，四选一）===
- website_navigation：跨多个网站的导航流程（如从搜索引擎→目标网站→详情页）
- page_interaction：特定网站/页面类型的操作模式（如搜索框使用方式、筛选器操作）
- success_pattern：验证有效的操作序列或策略组合
- failure_avoid：应避免的操作或已知的失败模式

判定流程（命中即停止）：
1) 涉及跨网站跳转查找信息 → website_navigation
2) 涉及特定网站UI交互技巧 → page_interaction
3) 执行轨迹整体成功 → success_pattern
4) 执行轨迹包含失败/重试 → failure_avoid

=== title 生成规则（18-45字，用于向量检索）===
- 必须包含：网站域名 + 任务类型 + 关键模式词
- 禁止类别前缀、禁止DOM元素ID、禁止像素值
- 示例："京东商品搜索-使用FILL+SEARCH替代点击搜索按钮"
- 示例："淘宝价格筛选-优先用区间输入而非价格排序按钮"
- 示例："微博时间范围搜索-使用advancedSearch工具"

=== content 生成规则（markdown，150-200字）===
## 用户意图
用户的原始任务描述

## 网站流程
经过的关键网站/页面

## 模式描述
[成功经验] 验证有效的粗粒度操作模式
[失败教训] 失败原因和应避免的做法

## 触发条件
何时此记忆应当被检索（网站+任务特征）

注意：禁止包含具体元素ID（#47）、像素值（300px）、时间戳等无法跨会话复用的信息。
仅保留粗粒度的操作模式：动作类型+目标描述，不保留具体坐标。

=== 输出 JSON ===
{"category": "website_navigation/page_interaction/success_pattern/failure_avoid",
 "title": "核心描述（不含类别前缀，系统自动拼接）",
 "content": "markdown格式内容"}
`;

export const BROWSER_PROCEDURAL_MEMORY_RETRIEVAL_PROMPT: string =
`你是浏览器自动化Agent的程序记忆检索助手。分析用户在当前网页环境中的任务请求，判断是否需要检索历史记忆，并输出查询画像。

【总原则】
- 所有字段都必须按"互斥 + 全集"输出：每个字段只能取一个值，且必须有值。
- 对同一 query，优先按下面"判定流程"逐步判断，禁止自由发挥。
- 无法确定时使用兜底规则，确保结果稳定。

【need_retrieval 判定（互斥）】
按顺序判断：
1) 用户消息仅为寒暄/确认词（无任务意图）-> need_retrieval=false
2) 仅为页面内微小调整（如"往下翻""返回""等一下"）-> false
3) 明确的任务请求（含网站+操作意图，如"在京东搜耳机""看微博热搜"）-> true
4) 兜底：只要包含可识别的网站名/任务类型对象 -> true

【改写规则】输出 12-32 字检索短语，优先"保留原 query 的关键网站+任务类型"，不要抽象成过泛类别：
- 必须保留：目标网站/域名 + 任务类型关键词（如 搜索、筛选、查看评价、时间范围、比价）
- 尽量保留：用户指定的参数（如 100-300元、7天内、差评、最新）
- 禁止包含：具体元素ID、URL路径、像素值
- 可补充1-2个同义任务词以增强召回，但不要改变原意
- 当原 query 已经清晰且信息充分时，rewritten_query 可等于原 query

【查询画像（三维分析，互斥判定）】
- clarity_level（按顺序命中）：
  1) precise：出现明确网站名+具体参数值/时间范围/价格区间等（如 京东 100-300元、微博 7天内）
  2) fuzzy：只有抽象意图，缺少网站或关键对象（如"帮我查个东西""看看有什么"）
  3) medium：其余情况（有网站或任务类型，但参数不完整）
- complexity（按顺序命中）：
  1) complex：跨3+网站查找信息，或多步骤组合任务
  2) moderate：跨2个网站，或单网站但含多个筛选条件
  3) simple：其余单网站单操作请求
- action_strategy（按顺序命中）：
  1) clarify：关键信息缺失且上下文无法推断（如不知道去哪个网站查）
  2) execute：其余可执行情况

【missing_slots 约束】
- 仅当 action_strategy=clarify 时填写，取值仅可来自：
  target_website, target_page_type, search_keyword, filter_condition, time_range
- action_strategy=execute 时，missing_slots 必须是 []

【context 字段说明】
context 包含浏览器端实时信息：
- current_url：当前所在 URL
- site_id：网站标识（如 jd/taobao/weibo/bilibili/xiaohongshu）
- page_state：页面状态（如 search_results/product_detail/list/home）
- task：用户原始任务描述
- start_url：任务起始页面 URL

输入：
- 对话历史：{conversation_history}
- 用户消息：{user_query}
- 场景：{context}

输出 JSON（严格遵守）：
{"need_retrieval": true/false, "reason": "简短原因", "rewritten_query": "改写短语",
  "clarity_level": "precise/medium/fuzzy", "complexity": "simple/moderate/complex",
  "action_strategy": "execute/clarify", "missing_slots": []}

示例：
"帮我在京东搜一个100-300块的耳机" -> {"need_retrieval":true, "reason":"明确网站+价格区间", "rewritten_query":"京东 搜索 耳机 100-300元 价格筛选", "clarity_level":"precise", "complexity":"moderate", "action_strategy":"execute", "missing_slots":[]}
"看看今天微博有什么热搜" -> {"need_retrieval":true, "reason":"网站+任务类型明确", "rewritten_query":"微博 热搜 查看", "clarity_level":"medium", "complexity":"simple", "action_strategy":"execute", "missing_slots":[]}
"帮我查个东西" -> {"need_retrieval":false, "reason":"意图模糊无法检索", "rewritten_query":"帮我查个东西", "clarity_level":"fuzzy", "complexity":"simple", "action_strategy":"clarify", "missing_slots":["target_website","search_keyword"]}
`;

export const BROWSER_PROCEDURAL_MEMORY_SUMMARY_PROMPT: string =
`你是浏览器Agent的记忆模块。请基于用户当前的网页任务请求和检索到的历史操作记忆，生成一个简洁的操作建议。

用户任务：{user_query}
当前页面场景：{context}

检索到的相关记忆：
{memories}

请分析以上信息，输出 JSON：
{
    "core_intent": "用户的核心任务诉求（一句话）",
    "relevant_experience": "检索到的相关历史经验（简短列举，含成功/失败标记）",
    "suggested_action": "建议的操作策略（结合历史经验和当前场景的适配建议）",
    "confidence": "high/medium/low（基于记忆与当前场景的匹配程度）"
}

注意：
- 如果没有检索到相关记忆，relevant_experience 填写"无历史经验记录"，confidence 填写 "low"
- suggested_action 应该给出粗粒度的操作策略，不要包含具体的 DOM 元素 ID 或像素值
- 重点关注：可用工具提示、网站导航路径、已知陷阱/失败模式
`;
