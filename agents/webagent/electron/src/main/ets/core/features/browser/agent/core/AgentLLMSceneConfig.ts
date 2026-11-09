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

import { AgentLLMScenes } from './AgentLLMScenes';

export class AgentLLMSceneConfig {
  public sceneId: string = '';
  public maxTokens: number = 512;
  public temperature: number = 0.7;
  public stream: boolean = false;
  public responseFormat: string = 'text';
  public enableThinking: boolean = false;
  public enableSearch: boolean = false;
  public searchOptionsJson: string = '{}';
}

export class AgentLLMSceneConfigService {
  private static readonly SEARCH_OPTIONS_JSON: string =
    '{"search_strategy":"turbo","forced_search":true,"enable_source":true,' +
    '"enable_citation":true,"citation_format":"[ref_<number>]","assigned_site_list":[]}';

  static get(sceneId: string): AgentLLMSceneConfig | null {
    const config = new AgentLLMSceneConfig();
    config.sceneId = sceneId;
    if (sceneId === AgentLLMScenes.CHAT) {
      config.maxTokens = 1024;
      config.temperature = 0.7;
      config.stream = true;
      return config;
    }
    if (sceneId === AgentLLMScenes.ROUTE) {
      return AgentLLMSceneConfigService.json(config, 256, 0.1);
    }
    if (sceneId === AgentLLMScenes.PLAN_GENERATION) {
      return AgentLLMSceneConfigService.json(config, 2048, 0.1);
    }
    if (sceneId === AgentLLMScenes.BROWSER_DECISION) {
      return AgentLLMSceneConfigService.json(config, 2048, 0.2);
    }
    if (sceneId === AgentLLMScenes.BROWSER_DECISION_VISION) {
      return AgentLLMSceneConfigService.json(config, 8192, 0.1);
    }
    if (sceneId === AgentLLMScenes.STEP_REACT_DECISION) {
      return AgentLLMSceneConfigService.json(config, 1024, 0.1);
    }
    if (sceneId === AgentLLMScenes.REACT_THINK) {
      config.maxTokens = 1024;
      config.temperature = 0.1;
      return config;
    }
    if (sceneId === AgentLLMScenes.CODE_AGENT) {
      config.maxTokens = 4096;
      config.temperature = 0.1;
      config.stream = true;
      return config;
    }
    if (sceneId === AgentLLMScenes.DEEP_SEARCH) {
      config.maxTokens = 4096;
      config.temperature = 0.1;
      config.stream = true;
      config.enableSearch = true;
      config.searchOptionsJson = AgentLLMSceneConfigService.SEARCH_OPTIONS_JSON;
      return config;
    }
    if (sceneId === AgentLLMScenes.NATIVE_DEFAULT_TEXT) {
      config.maxTokens = 4096;
      config.temperature = 0.1;
      config.stream = true;
      return config;
    }
    if (sceneId === AgentLLMScenes.NATIVE_SIMPLIFIER) {
      config.maxTokens = 2048;
      config.temperature = 0.1;
      return config;
    }
    if (sceneId === AgentLLMScenes.NATIVE_GENUI_GENERATION) {

      config.maxTokens = 50000;
      config.temperature = 0.2;
      return config;
    }
    return null;
  }

  private static json(config: AgentLLMSceneConfig,
    maxTokens: number, temperature: number): AgentLLMSceneConfig {
    config.maxTokens = maxTokens;
    config.temperature = temperature;
    config.responseFormat = 'json_object';
    return config;
  }
}
