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

// 这个文件负责 browser 功能 中 Browser Page State Registry Service 的注册与检索。
import {
  BrowserPageState,
  BrowserPageStateResolver,
  BrowserRuntimeState
} from '../../../shared/domain/BrowserAutomationTypes';
import { RednotePageStateResolverService } from './RednotePageStateResolverService';

export class BrowserPageStateRegistryService {
  static resolveCurrentState(runtime: BrowserRuntimeState): BrowserPageState | null {
    const rednoteState = RednotePageStateResolverService.resolve(runtime);
    if (rednoteState) {
      return rednoteState;
    }
    return null;
  }

  static listResolvers(): BrowserPageStateResolver[] {
    return [RednotePageStateResolverService.getResolver()];
  }
}
