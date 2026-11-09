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

import { AgentState } from '../core/AgentState';
import { AgentToolCall, AgentToolDescriptor, AgentToolQuery, AgentToolResult } from '../core/AgentTypes';
import {AgentToolContext} from '../ports/AgentContext';

export abstract class AgentSiteTool {
  abstract readonly toolId: string;
  abstract readonly displayName: string;
  abstract readonly description: string;
  readonly domain: string = '';

  buildDescriptor(): AgentToolDescriptor {
    const tool = new AgentToolDescriptor();
    tool.toolId = this.toolId;
    tool.displayName = this.displayName;
    tool.description = this.description;
    tool.domain = this.domain;
    return tool;
  }

  canHandle(call: AgentToolCall): boolean {
    const toolId = call.toolId.trim().toLowerCase();
    const toolName = call.toolName.trim().toLowerCase();
    return toolId === this.toolId.trim().toLowerCase() ||
      toolName === this.toolId.trim().toLowerCase() ||
      toolName === this.displayName.trim().toLowerCase();
  }

  abstract supports(query: AgentToolQuery): boolean;
  abstract invoke(call: AgentToolCall, state: AgentState, context: AgentToolContext): Promise<AgentToolResult>;
}
