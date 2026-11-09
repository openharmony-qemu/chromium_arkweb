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

// 本地程序记忆常量与枚举（对齐 procmemory_for_browser/mem0/mem0/memory/main.py）

export class MemoryTypeName {
  public static readonly SEMANTIC: string = 'semantic_memory';
  public static readonly PROCEDURAL: string = 'procedural_memory';
  public static readonly SESSION: string = 'session_memory';
}

export class MemoryEvent {
  public static readonly ADD: string = 'ADD';
  public static readonly UPDATE: string = 'UPDATE';
  public static readonly DELETE: string = 'DELETE';
  public static readonly NONE: string = 'NONE';
}

// 程序记忆 category 枚举（与 Python PROCEDURAL_MEMORY_CATEGORIES 对齐：浏览器版本仍按"记忆用途视角"分类）
// 浏览器 prompt 实际产出的分类是 website_navigation / page_interaction / success_pattern / failure_avoid
// 但写入 metadata.sub_sub_type 时也允许沿用智能家居的 4 类，做兼容处理。
export class ProceduralCategory {
  public static readonly SCENE_MODE: string = 'scene_mode';
  public static readonly DEVICE_PRESET: string = 'device_preset';
  public static readonly ROUTINE_ACTION: string = 'routine_action';
  public static readonly OTHER: string = 'other';

  // 浏览器场景下的 4 个 category（来自 BROWSER_ADD_PROCEDURAL_MEMORY_PROMPT）
  public static readonly WEBSITE_NAVIGATION: string = 'website_navigation';
  public static readonly PAGE_INTERACTION: string = 'page_interaction';
  public static readonly SUCCESS_PATTERN: string = 'success_pattern';
  public static readonly FAILURE_AVOID: string = 'failure_avoid';
}

// category 的中文前缀（用于 summarized_title 拼接）
export const CATEGORY_PREFIX: Record<string, string> = {
  'scene_mode': '场景模式',
  'device_preset': '设备偏好',
  'routine_action': '日常操作',
  'other': '其他',
  'website_navigation': '网站导航',
  'page_interaction': '页面交互',
  'success_pattern': '成功模式',
  'failure_avoid': '失败规避'
};

export function isValidCategory(category: string): boolean {
  return CATEGORY_PREFIX[category] !== undefined;
}

export function defaultCategory(): string {
  // 浏览器路径默认归到 page_interaction；后续按 prompt 输出再覆盖
  return ProceduralCategory.PAGE_INTERACTION;
}

// complexity → 自适应 topK（与 Python TOPK_BY_COMPLEXITY 对齐）
export const TOPK_BY_COMPLEXITY: Record<string, number> = {
  'simple': 3,
  'moderate': 5,
  'complex': 8
};

// QueryProfile 字段允许值
export const VALID_CLARITY: string[] = ['precise', 'medium', 'fuzzy'];
export const VALID_COMPLEXITY: string[] = ['simple', 'moderate', 'complex'];
export const VALID_ACTION_STRATEGY: string[] = ['execute', 'clarify'];

// 浏览器版 missing_slots 词表
export const VALID_MISSING_SLOTS: string[] = [
  'target_website',
  'target_page_type',
  'search_keyword',
  'filter_condition',
  'time_range'
];
