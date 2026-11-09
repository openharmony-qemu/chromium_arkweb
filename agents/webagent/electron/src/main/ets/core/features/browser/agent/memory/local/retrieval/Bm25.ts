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

// 字符级 BM25Okapi 实现，对齐 procmemory_for_browser/mem0/mem0/memory/retrieval_bm25.py 的语义。
// 端上不接 jieba，统一走字符级分词。BM25 参数与 rank-bm25 默认值一致。

import { StoredMemoryView } from '../domain/MemoryItemModel';

const DEFAULT_K1: number = 1.5;
const DEFAULT_B: number = 0.75;
const DEFAULT_EPSILON: number = 0.25;

export class Bm25Result {
  public id: string = '';
  public score: number = 0.0;
  public payload: Record<string, Object> = {};
}

export class Bm25 {
  // 把文本切成字符 token；空白字符过滤。
  static tokenize(text: string): string[] {
    if (!text || text.length <= 0) {
      return [];
    }
    const trimmed = text.trim();
    if (trimmed.length <= 0) {
      return [];
    }
    const tokens: string[] = [];
    for (let i = 0; i < trimmed.length; i++) {
      const ch = trimmed.charAt(i);
      if (ch === ' ' || ch === '\t' || ch === '\n' || ch === '\r') {
        continue;
      }
      tokens.push(ch);
    }
    return tokens;
  }

  // 在给定全量 docs 上做 BM25 检索；按字段 textField (通常是 'title') 取文本。
  // 与 Python bm25_search 的返回结构一致（{id, score, metadata}），这里 metadata 就是 view.payload。
  static search(
    query: string,
    docs: StoredMemoryView[],
    textField: string,
    limit: number
  ): Bm25Result[] {
    if (!docs || docs.length <= 0) {
      return [];
    }
    const corpus: string[][] = [];
    const docTexts: string[] = [];
    for (let i = 0; i < docs.length; i++) {
      const text = Bm25.extractText(docs[i].payload, textField);
      docTexts.push(text);
      corpus.push(Bm25.tokenize(text));
    }
    const queryTokens = Bm25.tokenize(query);

    // query 为空时按入库顺序返回前 limit 条，分数填 0.0（与 Python 行为一致）
    if (queryTokens.length <= 0) {
      const out: Bm25Result[] = [];
      const cap = limit > 0 ? Math.min(limit, docs.length) : docs.length;
      for (let i = 0; i < cap; i++) {
        const r = new Bm25Result();
        r.id = docs[i].id;
        r.score = 0.0;
        r.payload = docs[i].payload;
        out.push(r);
      }
      return out;
    }

    const scores = Bm25.computeScores(corpus, queryTokens);
    // 取分数 top-limit
    const indices: number[] = [];
    for (let i = 0; i < scores.length; i++) {
      indices.push(i);
    }
    indices.sort((a: number, b: number): number => {
      const sa = scores[a];
      const sb = scores[b];
      if (sa === sb) {
        return a - b;
      }
      return sa < sb ? 1 : -1;
    });
    const cap = limit > 0 ? Math.min(limit, indices.length) : indices.length;
    const out: Bm25Result[] = [];
    for (let i = 0; i < cap; i++) {
      const idx = indices[i];
      const r = new Bm25Result();
      r.id = docs[idx].id;
      r.score = scores[idx];
      r.payload = docs[idx].payload;
      out.push(r);
    }
    return out;
  }

  // 构建 docFreqs 和 df map
  private static buildDocFreqs(corpus: string[][]): {
    docFreqs: Map<string, number>[];
    df: Map<string, number>;
  } {
    const N = corpus.length;
    const docFreqs: Map<string, number>[] = [];
    const df: Map<string, number> = new Map();
    for (let i = 0; i < N; i++) {
      const tf: Map<string, number> = new Map();
      const tokens = corpus[i];
      for (let j = 0; j < tokens.length; j++) {
        const t = tokens[j];
        tf.set(t, (tf.get(t) ?? 0) + 1);
      }
      docFreqs.push(tf);
      const seen: string[] = Array.from(tf.keys());
      for (let j = 0; j < seen.length; j++) {
        const t = seen[j];
        df.set(t, (df.get(t) ?? 0) + 1);
      }
    }
    return { docFreqs, df };
  }

  // 计算 IDF 值并处理负值
  private static computeIdf(N: number, df: Map<string, number>): Map<string, number> {
    const idf: Map<string, number> = new Map();
    let idfSum = 0;
    const negativeTerms: string[] = [];
    const dfKeys: string[] = Array.from(df.keys());
    for (let i = 0; i < dfKeys.length; i++) {
      const term = dfKeys[i];
      const n = df.get(term) ?? 0;
      const value = Math.log(N - n + 0.5) - Math.log(n + 0.5);
      idf.set(term, value);
      idfSum += value;
      if (value < 0) {
        negativeTerms.push(term);
      }
    }
    const avgIdf = idf.size > 0 ? idfSum / idf.size : 0;
    const eps = DEFAULT_EPSILON * avgIdf;
    for (let i = 0; i < negativeTerms.length; i++) {
      idf.set(negativeTerms[i], eps);
    }
    return idf;
  }

  // 计算单个文档的 BM25 分数
  private static computeDocScore(
    tf: Map<string, number>,
    queryTokens: string[],
    idf: Map<string, number>,
    denomNorm: number
  ): number {
    let score = 0;
    for (let q = 0; q < queryTokens.length; q++) {
      const term = queryTokens[q];
      const qFreq = tf.get(term) ?? 0;
      const termIdf = idf.get(term);
      if (termIdf === undefined || qFreq <= 0) {
        continue;
      }
      score += termIdf * (qFreq * (DEFAULT_K1 + 1) / (qFreq + denomNorm));
    }
    return score;
  }

  // 计算所有文档的 BM25 分数
  private static computeAllScores(
    N: number,
    docFreqs: Map<string, number>[],
    queryTokens: string[],
    idf: Map<string, number>,
    avgdl: number
  ): number[] {
    const scores: number[] = [];
    for (let i = 0; i < N; i++) {
      const dl = docFreqs[i].size;
      const denomNorm = DEFAULT_K1 * (1 - DEFAULT_B + DEFAULT_B * (avgdl > 0 ? dl / avgdl : 0));
      scores.push(Bm25.computeDocScore(docFreqs[i], queryTokens, idf, denomNorm));
    }
    return scores;
  }

  // 计算 BM25 分数。corpus[i] 是已分词的 doc tokens，queryTokens 是 query tokens。
  // 算法与 rank-bm25 BM25Okapi 一致：
  //   idf(t) = log(N - n(t) + 0.5) - log(n(t) + 0.5)，对 idf<0 的 term 用 epsilon * average_idf 替换。
  //   score(D) = Σ idf(t) * f(t,D) * (k1+1) / (f(t,D) + k1*(1-b+b*|D|/avgdl))
  private static computeScores(corpus: string[][], queryTokens: string[]): number[] {
    const N = corpus.length;
    const docLen: number[] = [];
    let totalLen = 0;
    for (let i = 0; i < N; i++) {
      docLen.push(corpus[i].length);
      totalLen += corpus[i].length;
    }
    const avgdl = N > 0 ? totalLen / N : 0;

    const { docFreqs, df } = Bm25.buildDocFreqs(corpus);
    const idf = Bm25.computeIdf(N, df);

    // 小语料库（N<=2）时所有 term 的 IDF 均为负，BM25 分数也为负，
    // 导致 threshold 过滤掉全部结果。此时回退到字符重叠评分。
    const hasPositiveIdf = Bm25.queryHasPositiveIdf(queryTokens, idf);
    if (!hasPositiveIdf && N <= 2) {
      return Bm25.overlapScores(corpus, queryTokens, docFreqs);
    }

    return Bm25.computeAllScores(N, docFreqs, queryTokens, idf, avgdl);
  }

  private static queryHasPositiveIdf(queryTokens: string[], idf: Map<string, number>): boolean {
    for (let i = 0; i < queryTokens.length; i++) {
      const v = idf.get(queryTokens[i]);
      if (v !== undefined && v > 0) {
        return true;
      }
    }
    return false;
  }

  // 字符重叠评分：匹配到的 query 字符占比，分值范围 0~1，适配 threshold 过滤。
  private static overlapScores(corpus: string[][], queryTokens: string[],
    docFreqs: Map<string, number>[]): number[] {
    const uniqueQuery: Set<string> = new Set();
    for (let i = 0; i < queryTokens.length; i++) {
      uniqueQuery.add(queryTokens[i]);
    }
    const scores: number[] = [];
    for (let i = 0; i < corpus.length; i++) {
      const tf = docFreqs[i];
      let matched = 0;
      const uniqueArr = Array.from(uniqueQuery);
      for (let q = 0; q < uniqueArr.length; q++) {
        if ((tf.get(uniqueArr[q]) ?? 0) > 0) {
          matched++;
        }
      }
      scores.push(uniqueQuery.size > 0 ? matched / uniqueQuery.size : 0);
    }
    return scores;
  }

  private static extractText(payload: Record<string, Object>, textField: string): string {
    const primary = payload[textField];
    if (typeof primary === 'string' && (primary as string).length > 0) {
      return primary as string;
    }
    const titleVal = payload.title;
    if (typeof titleVal === 'string' && (titleVal as string).length > 0) {
      return titleVal as string;
    }
    const contentVal = payload.content;
    if (typeof contentVal === 'string') {
      return contentVal as string;
    }
    return '';
  }
}
