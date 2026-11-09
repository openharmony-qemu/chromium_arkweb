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
  CONVENTIONAL_OBSERVE_SYSTEM_PROMPT,
  CONVENTIONAL_REASON_SYSTEM_PROMPT2,
  QUICK_REASON_SYSTEM_PROMPT,
  OBSERVE_AND_REASON_SYSTEM_PROMPT,
  OBSERVE_AND_REASON_TEXT_ONLY_SYSTEM_PROMPT
} from './ConventionalPrompt';
import {AgentState} from '../core/AgentState';
import { BrowserAgentDateUtils } from '../core/DateUtils';

/** 每次构建提示词时填入设备当前的本地日期。 */
function withCurrentDate(prompt: string): string {
  return prompt.replace(/<CURRENT_DATE>/g, BrowserAgentDateUtils.formatLocalDate());
}

export class BrowserAgentPromptService {
  static buildObserveSystemPrompt(): string {
    return withCurrentDate(CONVENTIONAL_OBSERVE_SYSTEM_PROMPT);
  }

  static buildReasonSystemPrompt(state: AgentState): string {
    let prompt:string = CONVENTIONAL_REASON_SYSTEM_PROMPT2;
    prompt = prompt.replace('<TOOL>', 'Tool: If there are available tools provided in the user prompt, optionally, you may choose one of the tools.');
    return withCurrentDate(prompt);
  }
  static buildQuickSystemPrompt(): string {
    return withCurrentDate(QUICK_REASON_SYSTEM_PROMPT);
  }

  static buildObserveAndReasonSystemPrompt(): string {
    let prompt: string = OBSERVE_AND_REASON_SYSTEM_PROMPT;
    prompt = prompt.replace('<TOOL>', 'Tool: If there are available tools provided in the user prompt, optionally, you may choose one of the tools.');
    return withCurrentDate(prompt);
  }

  static buildObserveAndReasonTextOnlySystemPrompt(): string {
    let prompt: string = OBSERVE_AND_REASON_TEXT_ONLY_SYSTEM_PROMPT;
    prompt = prompt.replace('<TOOL>', 'Tool: If there are available tools provided in the user prompt, optionally, you may choose one of the tools.');
    return withCurrentDate(prompt);
  }
}
