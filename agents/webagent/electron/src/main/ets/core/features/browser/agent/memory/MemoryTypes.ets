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

export class MemoryResultEntry {
  public id: string = '';
  public proceduralTitle: string = '';
  public proceduralContent: string = '';
  public score: number = 0;
  public vectorScoreRaw: number | null = null;
  public bm25ScoreRaw: number | null = null;
  public retrievalMode: string = '';
  public metadata: Record<string, Object> = {};
}

export class MemorySearchData {
  public results: MemoryResultEntry[] = [];
  public memorySummaryText: string = '';
  public rewrittenQuery: string = '';
  public retrievalPerformed: boolean = false;
}

export class MemorySearchResult {
  public responseCode: number = 0;
  public message: string = '';
  public data: MemorySearchData | null = null;
}

export class MemoryAddData {
  public event: string = '';
  public result: Object | null = null;
  public segmentationInfo: Record<string, Object> = {};
}

export class MemoryAddResult {
  public responseCode: number = 0;
  public message: string = '';
  public data: MemoryAddData | null = null;
}

export class MemoryContextMetaPayload {
  public task: string = '';
  public currentUrl: string = '';
  public siteId: string = '';
  public startUrl: string = '';
}

export class MemoryContextMetadataRecord {
  public task: string = '';
  public currentUrl: string = '';
  public siteId: string = '';
  public startUrl: string = '';
}

export class MemorySearchMetadataPayload {
  public contextMetadata: MemoryContextMetadataRecord = new MemoryContextMetadataRecord();
  public agentId: string = '';
}

export class MemorySearchRequestPayload {
  public userId: string = '';
  public query: string = '';
  public metadata: MemorySearchMetadataPayload = new MemorySearchMetadataPayload();
  public limit: number = 3;
  public threshold: number = 0.3;
  public searchPreset: string = 'lite';
}

export class MemoryToolEntry {
  public name: string = '';
  public args: Record<string, Object> = {};
  public argsConcreteHint: Record<string, Object> = {};
  public result: string = '';
}

export class MemoryMessagePayload {
  public role: string = '';
  public content: Object = '';
}

export class MemoryToolCallPayload {
  public tool: string = '';
  public name: string = '';
  public argsJson: string = '';
  public argsConcreteHintJson: string = '';
  public result: string = '';
}

export class MemoryAddRequestPayload {
  public userId: string = '';
  public agentId: string = '';
  public messages: MemoryMessagePayload[] = [];
  public toolCalls: MemoryToolCallPayload[] = [];
  public metadata: Record<string, string> = {};
  public enableTopicSegmentation: boolean = false;
}
