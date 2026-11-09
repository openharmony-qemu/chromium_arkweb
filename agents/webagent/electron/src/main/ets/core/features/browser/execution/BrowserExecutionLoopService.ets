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
  BrowserPerceptionPayload,
  BrowserRuntimeState,
  BrowserTransportRequest
} from '../../../shared/domain/BrowserAutomationTypes';
import { BrowserExecutionOrchestratorService } from './BrowserExecutionOrchestratorService';
import { BrowserTransportCommandService } from '../transport/BrowserTransportCommandService';

export class BrowserExecutionLoopUpdate {
  public runtime: BrowserRuntimeState = new BrowserRuntimeState();
  public request: BrowserTransportRequest | null = null;
}

export class BrowserExecutionLoopService {
  static beginObservation(runtime: BrowserRuntimeState): BrowserExecutionLoopUpdate {
    const next = new BrowserExecutionLoopUpdate();
    const beginResult = BrowserTransportCommandService.beginPerception(runtime);
    next.runtime = beginResult.runtime;
    next.request = beginResult.request;
    return next;
  }

  static applyPerception(
    runtime: BrowserRuntimeState,
    perception: BrowserPerceptionPayload
  ): BrowserExecutionLoopUpdate {
    const next = new BrowserExecutionLoopUpdate();
    next.runtime = BrowserExecutionOrchestratorService.applyPerception(runtime, perception);
    return next;
  }

  static completePendingAction(runtime: BrowserRuntimeState): BrowserExecutionLoopUpdate {
    const next = new BrowserExecutionLoopUpdate();
    next.runtime = BrowserExecutionOrchestratorService.completeAction(runtime);
    return next;
  }
}
