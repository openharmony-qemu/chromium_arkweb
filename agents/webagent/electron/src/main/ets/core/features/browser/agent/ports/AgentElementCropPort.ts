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

import { AgentObservation } from '../core/AgentTypes';

export class BrowserAgentCropImageResult {
  success: boolean = false;
  elementId: string = '';
  mimeType: string = 'image/png';
  dataUrl: string = '';
  width: number = 0;
  height: number = 0;
  errorMessage: string = '';
}

export interface AgentElementCropPort {
  cropByElementId(observation: AgentObservation | null, elementIdText: string): Promise<BrowserAgentCropImageResult>;
  cropByElementIdWithPadding(
    observation: AgentObservation | null,
    elementIdText: string,
    paddingCssPx: number
  ): Promise<BrowserAgentCropImageResult>;
}
