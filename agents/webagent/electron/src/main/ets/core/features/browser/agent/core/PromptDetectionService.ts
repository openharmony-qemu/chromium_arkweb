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

import { AgentObservation } from './AgentTypes';
import { BrowserAgentDialogObservationService } from './DialogObservationService';

/**
 * Builds the complete text context inspected by the prompt-injection detector.
 *
 * Page metadata and JavaScript dialog fields are controlled by the webpage too,
 * so they must be checked alongside screenshots and DOM text.
 */
export class BrowserAgentPromptDetectionService {
  static build(treeText: string, observation: AgentObservation | null): string {
    if (!observation) {
      return treeText;
    }

    const sections: string[] = [treeText, observation.currentTitle, observation.currentUrl];
    if (observation.hasOpenDialog || observation.dialogState) {
      sections.push(BrowserAgentDialogObservationService.buildDialogData(observation.dialogState));
    } else {
      sections.push(observation.liteDomSummary);
    }
    return sections.join(',');
  }
}
