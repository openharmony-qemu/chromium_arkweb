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

// 对齐 mem0/mem0/memory/main.py 中的 QueryProfile dataclass。

export class QueryProfile {
  public needRetrieval: boolean = true;
  public rewrittenQuery: string = '';
  public reason: string = '';
  public clarityLevel: string = 'medium'; // "precise" | "medium" | "fuzzy"
  public complexity: string = 'moderate'; // "simple" | "moderate" | "complex"
  public actionStrategy: string = 'execute'; // "execute" | "clarify"
  public missingSlots: string[] = [];

  static fallback(rewrittenQuery: string, reason: string): QueryProfile {
    const p = new QueryProfile();
    p.needRetrieval = true;
    p.rewrittenQuery = rewrittenQuery;
    p.reason = reason;
    return p;
  }
}
