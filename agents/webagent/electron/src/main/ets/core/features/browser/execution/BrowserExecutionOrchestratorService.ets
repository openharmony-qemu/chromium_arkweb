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
  BrowserActionCommand,
  BrowserPerceptionPayload,
  BrowserPhase,
  BrowserRuntimeState
} from '../../../shared/domain/BrowserAutomationTypes';
import { BrowserRuntimeService } from './BrowserRuntimeService';

export class BrowserExecutionOrchestratorService {

  static beginObserving(runtime: BrowserRuntimeState, requestId: string): BrowserRuntimeState {
    const nextRuntime = BrowserRuntimeService.clone(runtime);
    nextRuntime.phase = BrowserPhase.OBSERVING;
    nextRuntime.pendingRequestId = requestId;
    nextRuntime.updatedAt = Date.now();
    return nextRuntime;
  }

  static applyPerception(runtime: BrowserRuntimeState, perception: BrowserPerceptionPayload): BrowserRuntimeState {
    const nextRuntime = BrowserRuntimeService.clone(runtime);
    nextRuntime.phase = BrowserPhase.THINKING;
    nextRuntime.pendingRequestId = '';
    nextRuntime.currentPerception = perception;
    nextRuntime.manipulateState.currentUrl = perception.currentUrl;
    nextRuntime.manipulateState.currentImage = perception.annotatedScreenshot;
    nextRuntime.manipulateState.currentOriginalImage = perception.originalScreenshot;
    nextRuntime.manipulateState.liteDomTree = perception.liteDomTree.slice();
    nextRuntime.updatedAt = Date.now();
    return nextRuntime;
  }

  static beginWaiting(
    runtime: BrowserRuntimeState, command: BrowserActionCommand | null, requestId: string): BrowserRuntimeState {
    const nextRuntime = BrowserRuntimeService.clone(runtime);
    nextRuntime.pendingRequestId = requestId;
    nextRuntime.phase = command !== null && command.requiresLoadingWait ? BrowserPhase.WAITING : BrowserPhase.ACTING;
    nextRuntime.updatedAt = Date.now();
    return nextRuntime;
  }

  static completeAction(runtime: BrowserRuntimeState): BrowserRuntimeState {
    const nextRuntime = BrowserRuntimeService.clone(runtime);
    nextRuntime.phase = BrowserPhase.OBSERVING;
    nextRuntime.pendingRequestId = '';
    nextRuntime.updatedAt = Date.now();
    return nextRuntime;
  }
}
