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

import { AgentToolRegistryPort } from '../ports/AgentToolRegistryPort';
import { vConfig } from '../VersionConfig';
import { AgentState } from '../core/AgentState';
import { AgentToolCall, AgentToolDescriptor, AgentToolQuery, AgentToolResult } from '../core/AgentTypes';
import { ScrollAndNoteToolService } from '../components/scroll_note/main';
import { WeiboSearchToolService } from '../components/weibo_search/main';
import { SelectFromPickerToolService } from '../components/time_select/main';
import { AgentEnvironmentPort } from '../ports/AgentEnvironmentPort';
import { AgentLLMPort } from '../ports/AgentLLMPort';
import { AgentWorkspacePort } from '../ports/AgentWorkspacePort';
import { AgentElementCropPort } from '../ports/AgentElementCropPort';
import { NavigateConfigPort } from '../ports/NavigateConfigPort';
import { AgentSiteTool } from './AgentSiteTool';
import { AgentToolContext, BrowserAgentEventCallbacks } from '../ports/AgentContext';
import { BrowserAgentUtils } from '../core/Utils';
import { hilog } from '@kit.PerformanceAnalysisKit';
const DOMAIN = 0x6020;
const TAG = 'BrowserAgentGraph';

export class AgentToolRegistryService implements AgentToolRegistryPort {
  private readonly context: AgentToolContext;
  private readonly tools: AgentSiteTool[];
  private readonly agentType: string;

  constructor(
    cropper: AgentElementCropPort,
    env: AgentEnvironmentPort,
    llm: AgentLLMPort,
    workspace: AgentWorkspacePort,
    urlConfig: NavigateConfigPort,
    agentType: string = 'browser',
    events: BrowserAgentEventCallbacks,
  ) {
    this.context = {
      cropper: cropper,
      env: env,
      llm: llm,
      workspace: workspace,
      urlConfig: urlConfig,
      events: events,
    }
    this.tools = [
      new ScrollAndNoteToolService(),
      new WeiboSearchToolService(),
      new SelectFromPickerToolService()
    ];
    this.agentType = agentType;
  }

  async listAvailable(query: AgentToolQuery): Promise<AgentToolDescriptor[]> {
    void this.agentType;
    const tools: AgentToolDescriptor[] = [];
    for (let i = 0; i < this.tools.length; i++) {
      if (vConfig.two_phase_reasoning && this.tools[i].toolId === ScrollAndNoteToolService.TOOL_ID) {
        continue;
      }
      if (this.tools[i].supports(query)) {
        tools.push(this.tools[i].buildDescriptor());
      }
    }
    return tools;
  }

  async invoke(call: AgentToolCall, state: AgentState): Promise<AgentToolResult> {
    hilog.debug(DOMAIN, TAG, 'node execute invoke tool %{public}s', call.toolId);
    void this.agentType;
    for (let i = 0; i < this.tools.length; i++) {
      if (this.tools[i].canHandle(call)) {
        try {
          return await this.tools[i].invoke(call, state, this.context);
        } catch (error) {
          const nextResult = new AgentToolResult();
          nextResult.handled = true;
          nextResult.success = false;
          nextResult.summary = `${call.toolId} failed: ${BrowserAgentUtils.stringifyError(error as Object)}`;
          nextResult.returnValue = nextResult.summary;
          hilog.warn(DOMAIN, TAG, 'tool invoke failed %{public}s', nextResult.summary);
          return nextResult;
        }
      }
    }
    const nextResult = new AgentToolResult();
    nextResult.handled = false;
    nextResult.success = false;
    nextResult.summary = `browser site tools disabled: ${call.toolName.length > 0 ? call.toolName : call.toolId}`;
    return nextResult;
  }
}
