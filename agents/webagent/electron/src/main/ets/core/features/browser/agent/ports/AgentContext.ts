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

import { AgentEnvironmentPort } from '../ports/AgentEnvironmentPort';
import { AgentLLMPort } from '../ports/AgentLLMPort';
import { AgentToolRegistryPort } from '../ports/AgentToolRegistryPort';
import { AgentWorkspacePort } from '../ports/AgentWorkspacePort';
import { AgentElementCropPort } from '../ports/AgentElementCropPort';
import { NavigateConfigPort } from '../ports/NavigateConfigPort';
import { ExternalLLMRequest, ExternalLLMResponse } from '../core/ExternalLLMTypes';
import {
  AgentDecision,
  DetectionResult,
  UserControlOptionType,
  UserControlReason,
  UserControlResponse
} from '../core/AgentTypes';
import Image from '@ohos.multimedia.image';

export interface BrowserAgentEventCallbacks {
  onChunk: (chunk: string, isFinished: boolean) => void;
  onScreenshot?: (path: string, id: number, showText: string) => void;
  onNeedUserControl?: (
    reason: UserControlReason,
    keys: UserControlOptionType[]
  ) => Promise<UserControlResponse>;
  onSwitchControl?: (controller: string) => void;
  onStop?: () => void;
  onTaskFailed?: (reasonCode: string, reason: string) => void;
  onLLMComplete?: (request: ExternalLLMRequest) => Promise<ExternalLLMResponse>;
  onStepExceeded?: () => void;
  checkPrompt?: (pixelMaps: Image.PixelMap[], treeText: string, prompt?: string, url?: string) => Promise<DetectionResult>;
  checkText?:(text: string) => Promise<boolean>;
  checkAction?:(decision: AgentDecision) => Promise<boolean>
}

export interface BrowserAgentControls {
  needClearBrowser: boolean;
  cacheScreenshot: boolean;
  proceduralMemory: string;
  sessionMemory: string;
}

export interface BrowserAgentGraphContext {
  env: AgentEnvironmentPort;
  llm: AgentLLMPort;
  tools: AgentToolRegistryPort;
  workspace: AgentWorkspacePort;
  urlConfig: NavigateConfigPort;
  events: BrowserAgentEventCallbacks;
  controls: BrowserAgentControls;
}

export interface AgentToolContext {
  cropper: AgentElementCropPort;
  env: AgentEnvironmentPort;
  llm: AgentLLMPort;
  workspace: AgentWorkspacePort;
  urlConfig: NavigateConfigPort;
  events: BrowserAgentEventCallbacks;
}
