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
const DOMAIN = 0x6020;
const TAG = 'VersionConfig';

export class Config {
  public version = '';
  public evaluate = false;
  public summary = 0;
  public note = 0;
  public recursive_plan = 0;
  public separate_judge = 0;
  public replan = 0;
  public parallel = true;
  public current_history = 0;
  public replan_on_new_page = true;
  public action_type = 'element';
  public take_note_in_observe = false;
  public summary_in_observe = false;
  public model = 'Qwen2.5-VL-72B-Instruct';
  public replan_with_decompose = false;
  public decompose_after_replan = true;
  public conventional = 0;
  public previous_screen = false;
  public original_screen = false;
  public input_tree_text = false;
  public outside_element = false;
  public litedom_use_tree = false; //litedom是否使用树形结构组织
  public separate_observe = false;
  public analyze_instruction = false;
  public keypoint_tracking = true;
  public keypoint_type = 'all'; // boolean | all
  public observe_original = false;
  public key_element = true;
  public dom_per_line = false;
  public quick = false;
  public one_step = false; //在quick为false时有效。
  public judge_trajectory = true;
  public text_only = false;
  public observe_filter_dom = false;
  public explain_action = false;
  public normal_analyse = false;
  // 首步「当前页 vs 新开」判断走纯文本（去截图）：即使视觉模式，也把这一步降为文本调用，
  // 省掉一次带图 vision 请求 + 一次 ASK_PERCEPTION 抓屏，显著降低首步时延。
  // 代价：仅凭 query+url+title 判断相关性，隐式指代当前页的模糊任务可能略受影响。默认 false=原始带图。
  public start_tab_text_only = false;
  // 首步指令分析（analyseInstruction/keypoints）后台化：不阻塞首个 reason，与其并行跑，
  // 结果在下一次 perceive 应用（keypoints 从第 2 步就位）。降低首步「命令→首个动作」等待。
  // 默认 false=原始同步阻塞。
  public defer_first_analysis = false;
  // 是否输出 observation 字段：默认 true 保留原始行为；false 时移除该输入/输出字段（精简 decode）。
  public output_observation = true;
  // previous_action / summary / reason 是否用精简措辞：默认 false 用原始完整；true 用精简版降 decode。
  public concise_output = false;
  // 整页 HTML 文本首步默认是否不发：默认 false 保持原始（'yes'，首步发）；true 时首步不发、由模型按需拉取。
  public default_no_html_text = false;
  public support_coordinate_action = false;
  public judge_loop = false;
  public two_phase_reasoning = false;
  public json_action_format = false;
  // litedom 变更摘要：比对前后两次感知的 litedom，为变化元素补出整页 HTML 里的全量属性
  // （class / aria-* / data-* 等状态字段，litedom 本身不带）。默认 false=不发该输入。
  public litedom_diff = false;

  public proceduralMemoryEnabled = false;
  // Session memory（话题接续）独立开关
  public sessionMemoryEnabled = true;
  // 程序记忆搜索策略：'always' | 'first_turn' | 'off'
  public proceduralMemorySearchStrategy = 'off';
  // 程序记忆存储开关（proceduralMemoryEnabled 为 true 时生效）
  public proceduralMemoryStoreEnabled = false;
  // 程序记忆存储的最小步数阈值（低于此值不触发存储）
  public proceduralMemoryStoreMinSteps = 5;
  // Session 条目数达到此阈值时触发程序记忆总结（0 = 关闭）
  public sessionSummarizeThreshold = 5;
  // 记录未完成任务（FAILED / 用户中断）的 session 记忆。
  // 默认关：保持原有「只有 FINISHED 才记录」的行为。
  // 开启后：失败/中断任务也会写入 session 记忆（带失败标记），供下次同 session 话题接续。
  public sessionMemoryRecordUnfinished = false;
  // 记录未完成任务的最小动作数门槛：动作数 < 此值的失败/中断任务视为「无实质进展」，不记录。
  // 仅在 sessionMemoryRecordUnfinished 开启时生效；FINISHED 任务不受此限制。
  public sessionMemoryUnfinishedMinSteps = 3;

  public initSuccess: boolean = false;

  public constructor(version: string) {
    this.version = version;
    this.initSuccess = false;
    if (this.set(version)) {
      this.initSuccess = true;
    }
  }

  public set(version: string): boolean {
    const preset = PROFILE_PRESETS[version];

    if (!preset) {
      hilog.error(DOMAIN, TAG, `Version config failed: Unknown version config: ${version}`);
      return false;
    }

    Object.assign(this, new ConfigDefaults(), preset);
    this.version = version;

    if (this.separate_judge && !this.recursive_plan) {
      hilog.error(DOMAIN, TAG, 'separate_judge requires recursive_plan.');
      return false;
    }

    if (this.replan && !(this.recursive_plan && this.separate_judge)) {
      hilog.error(DOMAIN, TAG, 'replan requires recursive_plan and separate_judge.');
      return false;
    }
    return true;
  }

  /** 运行时切换纯文本/视觉模式 */
  public setTextOnly(enabled: boolean): void {
    // 保存 UI 运行时修改的记忆配置，避免被 preset 重置
    const savedProceduralMemoryEnabled = this.proceduralMemoryEnabled;
    const savedSessionMemoryEnabled = this.sessionMemoryEnabled;
    const savedProceduralMemorySearchStrategy = this.proceduralMemorySearchStrategy;
    const savedProceduralMemoryStoreEnabled = this.proceduralMemoryStoreEnabled;
    const savedProceduralMemoryStoreMinSteps = this.proceduralMemoryStoreMinSteps;
    const savedSessionSummarizeThreshold = this.sessionSummarizeThreshold;
    const savedSessionMemoryRecordUnfinished = this.sessionMemoryRecordUnfinished;
    const savedSessionMemoryUnfinishedMinSteps = this.sessionMemoryUnfinishedMinSteps;

    this.initSuccess = false;
    if (this.set(version)) {
      this.initSuccess = true;
    }
    this.text_only = enabled;
    // 联动：纯文本模式下关闭 previous_screen / original_screen
    if (enabled) {
      this.previous_screen = false;
      this.original_screen = false;
    }

    // 恢复 UI 运行时修改的记忆配置
    this.proceduralMemoryEnabled = savedProceduralMemoryEnabled;
    this.sessionMemoryEnabled = savedSessionMemoryEnabled;
    this.proceduralMemorySearchStrategy = savedProceduralMemorySearchStrategy;
    this.proceduralMemoryStoreEnabled = savedProceduralMemoryStoreEnabled;
    this.proceduralMemoryStoreMinSteps = savedProceduralMemoryStoreMinSteps;
    this.sessionSummarizeThreshold = savedSessionSummarizeThreshold;
    this.sessionMemoryRecordUnfinished = savedSessionMemoryRecordUnfinished;
    this.sessionMemoryUnfinishedMinSteps = savedSessionMemoryUnfinishedMinSteps;
  }
}

class ConfigDefaults extends Config {
  public constructor() {
    super('__defaults__');
  }

  public override set(_version: string): boolean {
    return false;
  }
}

type VersionPreset = Partial<Config>;

const BASE_PRESETS: Record<string, VersionPreset> = {
  '1': { summary: 0 },
  '1.1': { summary: 1 },
  '1.2': { summary: 2 },
  '2': { note: 1 },
  '2.1': { note: 2 },
  '2.2': { summary: 1, note: 1 },
  '2.3': { summary: 1, note: 2 },
  '3': { recursive_plan: 1 },
  '3.1': { note: 1 },
  '3.2': { note: 1, separate_judge: 1 },
  '3.3': { note: 1, separate_judge: 1, replan: 1 },
  '4': {
    recursive_plan: 1,
    note: 1,
    separate_judge: 1,
    replan: 1,
    parallel: false,
  },
  '4.1': { current_history: 1 },
  '5': {
    recursive_plan: 2,
    note: 1,
    separate_judge: 1,
    replan: 1,
    parallel: false,
    current_history: 1,
  },
  '5.1': { replan_on_new_page: false },
  '5.2': { replan_on_new_page: false, take_note_in_observe: true },
  '5.3': {
    replan_on_new_page: false,
    take_note_in_observe: true,
    summary_in_observe: true,
  },
  '5.4': { take_note_in_observe: true, summary_in_observe: true },
  '5.5': {
    take_note_in_observe: true,
    summary_in_observe: true,
  },
  '5.6': {
    take_note_in_observe: true,
    summary_in_observe: true,
    model: 'Qwen3-VL-8B-Instruct2',
  },
  '5.7': {
    take_note_in_observe: true,
    summary_in_observe: true,
    previous_screen: true,
  },
  '7': {
    recursive_plan: 2,
    note: 1,
    separate_judge: 1,
    replan: 1,
    parallel: false,
    current_history: 1,
    take_note_in_observe: true,
    summary_in_observe: true,
    replan_with_decompose: true,
  },
  '7.1': { input_tree_text: true },
  '7.2': { previous_screen: true, input_tree_text: true },
  '7.3': {
    previous_screen: true,
    input_tree_text: true,
    outside_element: true,
    analyze_instruction: true,
  },
  '7.4': {
    previous_screen: true,
    outside_element: true,
    analyze_instruction: true,
  },
  '8': {
    conventional: 1,
    note: 1,
    parallel: false,
  },
  '8.1': { previous_screen: true },
  '8.2': { previous_screen: true, model: 'Qwen3-VL-8B-Instruct2' },
  '8.3': { previous_screen: true, model: 'Qwen3-VL-30B-A3B-Instruct' },
  '8.4': { previous_screen: true, input_tree_text: true },
  '8.5': {
    previous_screen: true,
    input_tree_text: true,
    original_screen: true,
  },
  '8.6': {
    previous_screen: true,
    input_tree_text: true,
    outside_element: true,
  },
  '8.7': {
    previous_screen: true,
    input_tree_text: true,
    outside_element: true,
    separate_observe: true,
  },
  '8.8': {
    previous_screen: true,
    input_tree_text: true,
    outside_element: true,
    separate_observe: true,
    analyze_instruction: true,
  },
  '8.9': {
    previous_screen: true,
    input_tree_text: true,
    outside_element: true,
    separate_observe: true,
    analyze_instruction: true,
    observe_original: true,
  },
  '9': {
    conventional: 1,
    note: 1,
    parallel: false,
    previous_screen: true,
    input_tree_text: true,
    outside_element: true,
    separate_observe: true,
    analyze_instruction: true,
    observe_original: true,
  },
  '9.1': { model: 'Qwen3-VL-30B-A3B-Instruct' },
  '9.2': { input_tree_text: false },
  '9.3': { model: 'Qwen3-VL-8B-Instruct2' },
  '9.4': {
    previous_screen: false,
    dom_per_line: true,
    input_tree_text: false,
    outside_element: false,
    analyze_instruction: false,
    judge_trajectory: false,
    key_element: false,
  },
  '9.5': { outside_element: false, key_element: false},
  '9.6': { outside_element: false, key_element: false, judge_trajectory: false, observe_original: false},
  '9.7': { outside_element: false, key_element: false, judge_trajectory: false, observe_original: false, explain_action: true},
  '9.7.1': { outside_element: false, key_element: false, judge_trajectory: false, observe_original: false, explain_action: true, one_step: true},
  '9.7.2': { outside_element: false, key_element: false, judge_trajectory: false, observe_original: false, explain_action: true, one_step: true, normal_analyse: true},
  '9.7.3': { outside_element: false, key_element: false, judge_trajectory: false, observe_original: false, previous_screen: false, explain_action: true, one_step: true, normal_analyse: true},
  '9.7.4': { outside_element: false, key_element: false, judge_trajectory: false, observe_original: false, previous_screen: false, explain_action: true, one_step: true, normal_analyse: true, keypoint_tracking: false, keypoint_type: 'all'},
  // 9.7.5 = 9.7.4 + 首步提速（去截图/后台分析）+ 精简输入输出。切回 9.7.4 即纯净原版。
  '9.7.5': { outside_element: false, key_element: false, judge_trajectory: false, observe_original: false, previous_screen: false, explain_action: true, one_step: true, normal_analyse: true, keypoint_tracking: false, keypoint_type: 'all', start_tab_text_only: true, defer_first_analysis: true, output_observation: false, concise_output: true, default_no_html_text: true},
  '9.8': { outside_element: false, key_element: false, judge_trajectory: true, observe_original: false, explain_action: true},
  '9.8.1': { outside_element: false, key_element: false, judge_trajectory: true, observe_original: false, explain_action: true, one_step: true, normal_analyse: true, previous_screen: false},

  '10': {
    conventional: 1,
    note: 1,
    parallel: false,
    previous_screen: false,
    input_tree_text: false,
    analyze_instruction: false,
    key_element: false,
    dom_per_line: true,
    quick: true,
  },
  '11': { //9.7.4
    conventional: 1,
    note: 1,
    parallel: false,
    previous_screen: false,
    input_tree_text: true,
    outside_element: false,
    separate_observe: true,
    analyze_instruction: true,
    observe_original: false,
    key_element: false,
    judge_trajectory: false,
    explain_action: true,
    one_step: true,
    normal_analyse: true,
    keypoint_tracking: false},
  '11.1': {judge_loop: true},
  '11.2': {support_coordinate_action: true},
  '11.3': {judge_loop: true, support_coordinate_action: true},
  // 11.4 = 11.3（loophandler + 坐标操控）+ 首步纯文本判断（start_tab_text_only）。
  // 不含 defer_first_analysis：它会与首个 reason 并发跑一次指令分析，模型端暂不支持并发，故不纳入。
  // observation 在 11 本就默认开，保持不变。
  '11.4': {judge_loop: true, support_coordinate_action: true, start_tab_text_only: true},
  '11.41': {judge_loop: true, support_coordinate_action: true},
  '11.42': {judge_loop: true, support_coordinate_action: true, two_phase_reasoning: true, json_action_format: true},
  '11.5': {judge_loop: true, support_coordinate_action: true, two_phase_reasoning: true, json_action_format: true, outside_element: true},
  // 11.6 = 11.5 + litedom 变更摘要（litedom_diff）。
  '11.6': {judge_loop: true, support_coordinate_action: true, two_phase_reasoning: true, json_action_format: true, outside_element: true, litedom_diff: true},
  '11.7': {judge_loop: true, support_coordinate_action: true, two_phase_reasoning: true, json_action_format: true, outside_element: true, litedom_diff: true, litedom_use_tree: true},
};

function buildProfilePresets(): Record<string, VersionPreset> {
  const presets: Record<string, VersionPreset> = {};

  for (const version of Object.keys(BASE_PRESETS)) {
    presets[version] = { ...BASE_PRESETS[version] };
  }

  for (const version of Object.keys(presets)) {
    if (!version.includes('.')) {
      continue;
    }

    const baseVersion = version.split('.')[0];
    presets[version] = {
      ...presets[baseVersion],
      ...presets[version],
    };
  }
  return presets;
}

const PROFILE_PRESETS = buildProfilePresets();

const version = '11.7';
export const vConfig = new Config(version);
export const releaseVersion = '1.0.14';
hilog.info(DOMAIN, TAG, 'WebAgent version = %{public}s, config version = %{public}s', releaseVersion, version);
