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

/**
 * Stable scene identifiers carried by AgentLLMRequest.
 *
 * They live in Core so every shell-side AgentLLMPort implementation can route
 * requests without depending on a concrete LLM settings service.
 */
export class AgentLLMScenes {
  public static readonly CHAT: string = 'chat';
  public static readonly ROUTE: string = 'route';
  public static readonly PLAN_GENERATION: string = 'plan_generation';
  public static readonly BROWSER_DECISION: string = 'browser_decision';
  public static readonly BROWSER_DECISION_VISION: string = 'browser_decision_vision';
  public static readonly STEP_REACT_DECISION: string = 'step_react_decision';
  public static readonly REACT_THINK: string = 'react_think';
  public static readonly CODE_AGENT: string = 'code_agent';
  public static readonly DEEP_SEARCH: string = 'deep_search';
  public static readonly NATIVE_DEFAULT_TEXT: string = 'native_default_text';
  public static readonly NATIVE_SIMPLIFIER: string = 'native_simplifier';
  public static readonly NATIVE_GENUI_GENERATION: string = 'native_genui_generation';
}
