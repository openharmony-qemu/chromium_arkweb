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

import {
  AgentHistoryEntry,
  AgentNoteEntry,
  AgentQuestionEntry,
  AgentObservation,
  AgentPhase,
  AgentTabInfo,
  AgentTaskResult,
  AgentToolCall,
  AgentToolDescriptor,
  BrowserAgentAction,
  AgentConstants,
  DetectionResult
} from './AgentTypes';
import { TaskTree } from './TaskTree';
import { AgentKeyPoint } from './AgentKeyPoints';
import { vConfig } from '../VersionConfig';

// 后台指令分析（defer_first_analysis）的结果载体。
export class AgentInstructionAnalysis {
  analysisText: string = '';
  keyPoints: AgentKeyPoint[] | null = null;
}

export class AgentState {
  startTime: number = 0;
  startUrl: string = '';
  manipulateStep: number = 0;
  globalStep: number = 0;
  stepBudget: number = AgentConstants.MAX_STEP;
  previousNode: string = '';
  task: string = '';
  analysis: string = '';
  keyPoints: AgentKeyPoint[] | null = null;
  questions: AgentQuestionEntry[] = []; // agent询问用户的问题和回答
  // defer_first_analysis 时，后台运行的指令分析 Promise；在下一次 perceive await 并应用后置空。
  pendingAnalysis: Promise<AgentInstructionAnalysis> | null = null;
  phase: string = AgentPhase.IDLE;
  observation: AgentObservation | null = null;
  previousObservation: AgentObservation | null = null;
  // actionQueue 的"原始"observation：LLM 推理出整串队列时的 observation。
  // handleBacklog 每次校验队列动作都用它做 preDom（而非"上一动作之后的"previousObservation），
  // 这样 A1/A2/A3 每个都对照"原始 Obs_N"验，A2 执行后 A3 仍能检出 A1→A2 间的变化。
  // 仅在决策产出 actionQueue 时设值；队列耗尽/清空后由下一轮决策点重置。
  queueOriginObservation: AgentObservation | null = null;
  previousAction: BrowserAgentAction | null = null;
  history: AgentHistoryEntry[] = [];
  availableTools: AgentToolDescriptor[] = [];
  summary: string = '';
  actionQueue: BrowserAgentAction[] = [];
  note: AgentNoteEntry[] = [];
  tabs: AgentTabInfo[] = [];
  taskTree: TaskTree | null = null;
  status: string = 'success';
  previousReturnValue: string = '';
  previousAdvice: string = '';
  loopAdvice: string = '';
  previousIneffective: number = 0;
  treeText: string = '';
  liteTreeText: string = ''; // 只保留litedom结点之间的层级关系，不保留不含litedom元素的子树
  /** 本次与上次 litedom 的变更摘要，在 reason 节点算出，供 prompt 直接渲染。 */
  liteDomDiff: string = '';
  /**
   * actionQueue 执行过程中逐步累积的 diff 文本。每执行完一步（包括首步 browserAction
   * 以及 actionQueue 里的每个后续动作），在进入 reason 节点时计算「本步执行后 vs 前一步」
   * 的 diff 并追加一行 "After action N <action>: <diff>"。队列全部跑完、再次进入真正推理时，
   * 若累积长度未超阈值，直接作为 liteDomDiff 输出；超阈值则回退为整串队列的总 diff。
   * 仅在 queueOriginObservation 非空（即队列模式下）才累积；新一轮推理产出 actionQueue 时清空。
   */
  queueStepDiffs: string = '';
  /** 当前队列已执行到第几步（从 1 起），用于 "After action N" 的编号。 */
  queueStepCount: number = 0;
  // 首步 HTML 文本默认值由配置决定：default_no_html_text=true 时不发（见 9.7.5），否则发送。
  needHtmlTextNext: boolean = !vConfig.default_no_html_text;
  needScreenshotNext: boolean = false;
  inDispatch: boolean = false;
  justEnteredDispatch: boolean = false;
  injectContent: DetectionResult | null = null; //在重推理时有用，此时不再进行注入检测。只允许新赋值，不允许修改
  selfHandleIgnorePermitted: boolean = false;
  // 本次任务内按敏感操作识别模块返回的原因记录“始终允许”。
  sensitiveActionAlwaysPermittedReasons: string[] = [];
  shouldConcludeAfterSensitiveAction: boolean = false;
  result: AgentTaskResult = new AgentTaskResult();
  stopRequested: boolean = false;
  errorMessage: string = '';
  pendingBrowserAction: BrowserAgentAction | null = null;
  pendingToolCall: AgentToolCall | null = null;
}
