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

import { BrowserPhase, BrowserRuntimeState } from '../../../shared/domain/BrowserAutomationTypes';
import { BrowserRuntimeService } from './BrowserRuntimeService';

export class BrowserExecutionLifecycleService {
  static markLaunchReady(runtime: BrowserRuntimeState): BrowserRuntimeState {
    const nextRuntime = BrowserRuntimeService.clone(runtime);
    nextRuntime.ready = true;
    nextRuntime.phase = BrowserPhase.OBSERVING;
    nextRuntime.updatedAt = Date.now();
    return nextRuntime;
  }
  
  static markFailed(runtime: BrowserRuntimeState, error: string): BrowserRuntimeState {
    const nextRuntime = BrowserRuntimeService.clone(runtime);
    nextRuntime.phase = BrowserPhase.FAILED;
    nextRuntime.errorMessage = error;
    nextRuntime.updatedAt = Date.now();
    return nextRuntime;
  }
}
