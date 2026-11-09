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

// 对齐 mem0/mem0/memory/main.py 中的 SearchConfig dataclass。
// 本端走 BM25-only：retrievalMode 默认 'bm25'，'vector'/'hybrid' 不实现，仅作为字段保留。

export class SearchConfig {
  public enableQueryAnalysis: boolean = true;
  public enableEnrich: boolean = false;
  public enableRerank: boolean = false;
  public enableSummary: boolean = true;
  public fallbackWithContext: boolean = true;
  public maxResults: number = 5;
  // 端上仅实现 'bm25'。其他取值 ('vector'/'hybrid') 在本地实现里会回退到 bm25。
  public retrievalMode: string = 'bm25';
  // 仅作为字段保留，不参与计算（C 方案）
  public hybridFusion: string = 'rrf';
  public rrfK: number = 60;
  public vectorWeight: number = 0.7;
  public bm25Weight: number = 0.3;
  public enableAdaptive: boolean = false;

  llmCallCount(): number {
    let n = 0;
    if (this.enableQueryAnalysis) {
      n += 1;
    }
    if (this.enableRerank) {
      n += 1;
    }
    if (this.enableSummary) {
      n += 1;
    }
    return n;
  }

  static simple(): SearchConfig {
    const c = new SearchConfig();
    c.enableQueryAnalysis = false;
    c.enableEnrich = false;
    c.enableRerank = false;
    c.enableSummary = false;
    c.enableAdaptive = false;
    return c;
  }

  static full(): SearchConfig {
    const c = new SearchConfig();
    c.enableQueryAnalysis = true;
    c.enableEnrich = true;
    c.enableRerank = true;
    c.enableSummary = true;
    return c;
  }

  static lite(): SearchConfig {
    const c = new SearchConfig();
    c.enableQueryAnalysis = true;
    c.enableEnrich = true;
    c.enableRerank = false;
    c.enableSummary = true;
    return c;
  }

  static noLlm(): SearchConfig {
    const c = new SearchConfig();
    c.enableQueryAnalysis = false;
    c.enableEnrich = true;
    c.enableRerank = false;
    c.enableSummary = false;
    c.enableAdaptive = false;
    return c;
  }

  static fromPreset(preset: string): SearchConfig | null {
    if (preset === 'full') {
      return SearchConfig.full();
    }
    if (preset === 'lite') {
      return SearchConfig.lite();
    }
    if (preset === 'no_llm') {
      return SearchConfig.noLlm();
    }
    if (preset === 'simple') {
      return SearchConfig.simple();
    }
    return null;
  }

  // 从 dict 形态合并到 SearchConfig（容忍 unknown key）
  static fromDict(dict: Record<string, Object>): SearchConfig {
    const c = SearchConfig.lite();
    const q = dict.enableQueryAnalysis;
    if (typeof q === 'boolean') {
      c.enableQueryAnalysis = q;
    }
    const e = dict.enableEnrich;
    if (typeof e === 'boolean') {
      c.enableEnrich = e;
    }
    const r = dict.enableRerank;
    if (typeof r === 'boolean') {
      c.enableRerank = r;
    }
    const s = dict.enableSummary;
    if (typeof s === 'boolean') {
      c.enableSummary = s;
    }
    const f = dict.fallbackWithContext;
    if (typeof f === 'boolean') {
      c.fallbackWithContext = f;
    }
    const a = dict.enableAdaptive;
    if (typeof a === 'boolean') {
      c.enableAdaptive = a;
    }
    const mr = dict.maxResults;
    if (typeof mr === 'number') {
      c.maxResults = mr;
    }
    const rk = dict.rrfK;
    if (typeof rk === 'number') {
      c.rrfK = rk;
    }
    const vw = dict.vectorWeight;
    if (typeof vw === 'number') {
      c.vectorWeight = vw;
    }
    const bw = dict.bm25Weight;
    if (typeof bw === 'number') {
      c.bm25Weight = bw;
    }
    const rm = dict.retrievalMode;
    if (typeof rm === 'string') {
      c.retrievalMode = rm;
    }
    const hf = dict.hybridFusion;
    if (typeof hf === 'string') {
      c.hybridFusion = hf;
    }
    return c;
  }
}
