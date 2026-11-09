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

import { AgentGraphResult } from '../core/AgentTypes';

// Result types kept for the runner and the optional site-tool dispatcher.
export class AgentGraphNodeResult<T extends object> {
  state: T;
  nextNode: string;

  constructor(state: T, nextNode: string) {
    this.state = state;
    this.nextNode = nextNode;
  }
}

export class AgentGraphRunResult<T extends object> {
  state: T;
  result: AgentGraphResult;

  constructor(state: T, result: AgentGraphResult) {
    this.state = state;
    this.result = result;
  }
}
