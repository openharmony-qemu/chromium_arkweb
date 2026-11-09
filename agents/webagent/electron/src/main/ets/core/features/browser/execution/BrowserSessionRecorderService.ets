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
  ManipulateState
} from '../../../shared/domain/BrowserAutomationTypes';
import { AgentLLMMessage, AgentLLMRequest, AgentLLMResponse } from '../agent/core/AgentTypes';
import { BrowserSessionWorkspaceService } from './BrowserSessionWorkspaceService';
import { hilog } from '@kit.PerformanceAnalysisKit';
import BuildProfile from 'BuildProfile';

const DOMAIN = 0x6020;
const TAG = 'BrowserSessionWorkspace';

class BrowserSessionRecordState {
  public actionCount: number = 0;
  public noteCount: number = 0;
  public perceptionCount: number = 0;
  public lastPerceptionKey: string = '';
}

class BrowserChunkRecord {
  public timestamp: number = 0;
  public isFinished: boolean = false;
  public chunk: string = '';
}

class BrowserActionRecord {
  public index: number = 0;
  public timestamp: number = 0;
  public action: string = '';
}

class BrowserNoteRecord {
  public index: number = 0;
  public timestamp: number = 0;
  public note: string = '';
}

class BrowserResultRecord {
  public sessionKey: string = '';
  public sessionDir: string = '';
  public phase: string = '';
  public summary: string = '';
  public errorMessage: string = '';
  public actionHistory: string[] = [];
  public noteHistory: string[] = [];
  public updatedAt: number = 0;
}

interface BrowserLlmCallDumpInput {
  systemPrompt: string;
  userPrompt: string;
  messages: AgentLLMMessage[];
  imagesCount: number;
  imagesList: string[];
  responseFormat: string;
  externalRequest?: Object;
}

interface BrowserLlmCallDumpOutput {
  text: string;
  rawText: string;
  promptTokens: number;
  completionTokens: number;
  totalTokens: number;
  promptCacheHitTokens: number;
  promptCacheMissTokens: number;
}

interface BrowserLlmCallDumpRecord {
  seq: number;
  tag: string;
  scene: string;
  model: string;
  timestamp: string;
  durationMs: number;
  input: BrowserLlmCallDumpInput;
  output: BrowserLlmCallDumpOutput;
}

export interface BrowserLlmCallRecordOptions {
  sessionDir: string;
  request: AgentLLMRequest;
  response: AgentLLMResponse;
  tag: string;
  scene: string;
  model: string;
  durationMs: number;
  externalRequest?: Object;
}

export class BrowserSessionRecorderService {
  private static readonly MAX_RUNTIME_TEXT_FIELD_LENGTH: number = 4000;
  private static readonly MAX_RUNTIME_HISTORY_ITEMS: number = 80;
  private static llmCallSeq: number = 0;
  private static readonly stateBySessionDir: Map<string, BrowserSessionRecordState> =
    new Map<string, BrowserSessionRecordState>();

  static syncRuntime(runtime: BrowserRuntimeState): BrowserRuntimeState {
    const nextRuntime = runtime;
    if (BuildProfile.DEBUG) {
      nextRuntime.sessionDir = BrowserSessionWorkspaceService.resolveSessionDir(
        nextRuntime.sessionKey,
        nextRuntime.sessionDir,
      );
    } else {
      nextRuntime.sessionDir = '';
    }
    if (nextRuntime.sessionDir.length <= 0) {
      return nextRuntime;
    }

    const state = BrowserSessionRecorderService.getOrCreateState(nextRuntime.sessionDir);
    try {
      if (BuildProfile.DEBUG) {
        BrowserSessionRecorderService.recordRuntime(nextRuntime);
        BrowserSessionRecorderService.recordActions(nextRuntime, state);
        BrowserSessionRecorderService.recordNotes(nextRuntime, state);
      }
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'BrowserSessionRecorderService.syncRuntime error: %{public}s', e.message);
    }
    return nextRuntime;
  }

  static recordChunk(sessionDir: string, chunk: string, isFinished: boolean): void {
    if (sessionDir.trim().length <= 0 || chunk.length <= 0) {
      return;
    }
    const record = new BrowserChunkRecord();
    record.timestamp = Date.now();
    record.isFinished = isFinished;
    record.chunk = chunk;
    const line = JSON.stringify(record) + '\n';
    BrowserSessionWorkspaceService.appendText(sessionDir, 'chunks.jsonl', line);
  }

  static recordLlmCall(options: BrowserLlmCallRecordOptions): void {
    if (options.sessionDir.length <= 0) {
      return;
    }

    try {
      BrowserSessionRecorderService.llmCallSeq += 1;
      const seq = BrowserSessionRecorderService.llmCallSeq;
      const tagPart = BrowserSessionRecorderService.sanitizeFileNamePart(options.tag);
      const baseName = `${seq}_${tagPart}`;
      const input: BrowserLlmCallDumpInput = {
        systemPrompt: options.request.systemPrompt,
        userPrompt: options.request.userPrompt,
        messages: options.request.messages,
        imagesCount: options.request.images.length,
        imagesList: options.request.imageFileNames.slice(),
        responseFormat: options.request.responseFormat,
      };
      const dump: BrowserLlmCallDumpRecord = {
        seq: seq,
        tag: options.tag,
        scene: options.scene,
        model: options.model,
        timestamp: new Date().toISOString(),
        durationMs: options.durationMs,
        input: input,
        output: {
          text: options.response.text,
          rawText: options.response.rawText,
          promptTokens: options.response.promptTokens,
          completionTokens: options.response.completionTokens,
          totalTokens: options.response.totalTokens,
          promptCacheHitTokens: options.response.promptCacheHitTokens,
          promptCacheMissTokens: options.response.promptCacheMissTokens
        }
      };

      BrowserSessionWorkspaceService.writeJson(
        options.sessionDir,
        `${baseName}.json`,
        dump
      );

      if (options.request.systemPrompt.length > 0) {
        BrowserSessionWorkspaceService.writeText(
          options.sessionDir,
          `${baseName}_system_prompt.txt`,
          options.request.systemPrompt
        );
      }
      if (options.request.userPrompt.length > 0) {
        BrowserSessionWorkspaceService.writeText(
          options.sessionDir,
          `${baseName}_user_prompt.txt`,
          options.request.userPrompt
        );
      }
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'record llm call failed: %{public}s', e.message);
    }
  }

  private static getOrCreateState(sessionDir: string): BrowserSessionRecordState {
    const current = BrowserSessionRecorderService.stateBySessionDir.get(sessionDir);
    if (current) {
      return current;
    }
    const nextState = new BrowserSessionRecordState();
    BrowserSessionRecorderService.stateBySessionDir.set(sessionDir, nextState);
    return nextState;
  }

  private static sanitizeFileNamePart(name: string): string {
    return name.replace(/[\/\\:*?"<>|]/g, '_').trim();
  }

  private static recordRuntime(runtime: BrowserRuntimeState): void {
    const snapshot = BrowserSessionRecorderService.buildRuntimeSnapshot(runtime);
    BrowserSessionWorkspaceService.writeJson(runtime.sessionDir, 'runtime.json', snapshot);
  }

  private static recordActions(runtime: BrowserRuntimeState, state: BrowserSessionRecordState): void {
    for (let i = state.actionCount; i < runtime.actionHistory.length; i++) {
      const record = new BrowserActionRecord();
      record.index = i;
      record.timestamp = runtime.updatedAt;
      record.action = runtime.actionHistory[i];
      const line = JSON.stringify(record) + '\n';
      BrowserSessionWorkspaceService.appendText(runtime.sessionDir, 'actions.jsonl', line);
    }
    state.actionCount = runtime.actionHistory.length;
  }

  private static recordNotes(runtime: BrowserRuntimeState, state: BrowserSessionRecordState): void {
    for (let i = state.noteCount; i < runtime.noteHistory.length; i++) {
      const record = new BrowserNoteRecord();
      record.index = i;
      record.timestamp = runtime.updatedAt;
      record.note = runtime.noteHistory[i];
      const line = JSON.stringify(record) + '\n';
      BrowserSessionWorkspaceService.appendText(runtime.sessionDir, 'notes.jsonl', line);
    }
    state.noteCount = runtime.noteHistory.length;
  }

  private static buildRuntimeSnapshot(runtime: BrowserRuntimeState): Record<string, Object> {
    const snapshot: Record<string, Object> = {};
    snapshot.sessionKey = runtime.sessionKey;
    snapshot.sessionDir = runtime.sessionDir;
    snapshot.needClearBrowser = runtime.needClearBrowser;
    snapshot.browserId = runtime.browserId;
    snapshot.transportOwner = runtime.transportOwner;
    snapshot.protocolVersion = runtime.protocolVersion;
    snapshot.phase = runtime.phase;
    snapshot.task = runtime.task;
    snapshot.config = runtime.config as Object;
    snapshot.manipulateState = BrowserSessionRecorderService.buildManipulateSnapshot(runtime.manipulateState);
    snapshot.currentPerception = runtime.currentPerception ?
      BrowserSessionRecorderService.buildPerceptionSnapshot(runtime.currentPerception) :
      BrowserSessionRecorderService.createEmptyRecord();
    snapshot.lastActionText = runtime.lastActionText;
    snapshot.lastSummary = BrowserSessionRecorderService.truncateText(runtime.lastSummary);
    snapshot.actionHistory = BrowserSessionRecorderService.tailStrings(runtime.actionHistory) as Object;
    snapshot.noteHistory = BrowserSessionRecorderService.tailStrings(runtime.noteHistory) as Object;
    snapshot.pendingRequestId = runtime.pendingRequestId;
    snapshot.ready = runtime.ready;
    snapshot.stopRequested = runtime.stopRequested;
    snapshot.disconnected = runtime.disconnected;
    snapshot.disconnectedReason = runtime.disconnectedReason;
    snapshot.errorMessage = runtime.errorMessage;
    snapshot.proceduralMemory = BrowserSessionRecorderService.truncateText(runtime.proceduralMemory);
    snapshot.sessionMemory = BrowserSessionRecorderService.truncateText(runtime.sessionMemory);
    snapshot.createdAt = runtime.createdAt;
    snapshot.updatedAt = runtime.updatedAt;
    snapshot.cacheScreenshot = runtime.cacheScreenshot;
    snapshot.screenshotCacheDir = runtime.screenshotCacheDir;
    return snapshot;
  }

  private static buildPerceptionSnapshot(perception: BrowserPerceptionPayload): Record<string, Object> {
    const snapshot: Record<string, Object> = {};
    snapshot.tabId = perception.tabId;
    snapshot.currentUrl = perception.currentUrl;
    snapshot.currentTitle = perception.currentTitle;
    snapshot.liteDomTree = [];
    snapshot.liteDomSummary = '';
    snapshot.originalScreenshot = '';
    snapshot.annotatedScreenshot = '';
    snapshot.viewport = perception.viewport as Object;
    snapshot.hasOpenDialog = perception.hasOpenDialog;
    snapshot.dialogState = perception.dialogState ?
      perception.dialogState :
      BrowserSessionRecorderService.createEmptyRecord();
    snapshot.rawPayloadText = '';
    snapshot.liteDomNodeCount = perception.liteDomTree.length;
    snapshot.liteDomSummaryLength = perception.liteDomSummary.length;
    snapshot.originalScreenshotLength = perception.originalScreenshot.length;
    snapshot.annotatedScreenshotLength = perception.annotatedScreenshot.length;
    snapshot.rawPayloadTextLength = perception.rawPayloadText.length;
    return snapshot;
  }

  private static buildManipulateSnapshot(state: ManipulateState): Record<string, Object> {
    const snapshot: Record<string, Object> = {};
    snapshot.instruction = state.instruction;
    snapshot.startUrl = state.startUrl;
    snapshot.manipulateStep = state.manipulateStep;
    snapshot.globalStep = state.globalStep;
    snapshot.stepBudget = state.stepBudget;
    snapshot.previousImage = '';
    snapshot.previousOriginalImage = '';
    snapshot.currentImage = '';
    snapshot.currentOriginalImage = '';
    snapshot.liteDomTree = [];
    snapshot.liteDomNodeCount = state.liteDomTree.length;
    snapshot.outsideElements = BrowserSessionRecorderService.truncateText(state.outsideElements);
    snapshot.summary = BrowserSessionRecorderService.truncateText(state.summary);
    snapshot.previousAction = state.previousAction;
    const history = state.history.length <= BrowserSessionRecorderService.MAX_RUNTIME_HISTORY_ITEMS ?
      state.history.slice() :
      state.history.slice(state.history.length - BrowserSessionRecorderService.MAX_RUNTIME_HISTORY_ITEMS);
    snapshot.history = history as Object;
    snapshot.tabs = state.tabs as Object;
    snapshot.note = state.note as Object;
    snapshot.knowledge = BrowserSessionRecorderService.truncateText(state.knowledge);
    snapshot.tools = BrowserSessionRecorderService.truncateText(state.tools);
    snapshot.analysis = BrowserSessionRecorderService.truncateText(state.analysis);
    snapshot.status = state.status;
    snapshot.previousIneffective = state.previousIneffective;
    snapshot.previousReturnValue = BrowserSessionRecorderService.truncateText(state.previousReturnValue);
    snapshot.actionQueue = state.actionQueue.slice() as Object;
    snapshot.currentUrl = state.currentUrl;
    return snapshot;
  }

  private static createEmptyRecord(): Record<string, Object> {
    return JSON.parse('{}') as Record<string, Object>;
  }

  private static truncateText(value: string): string {
    if (value.length <= BrowserSessionRecorderService.MAX_RUNTIME_TEXT_FIELD_LENGTH) {
      return value;
    }
    return value.substring(0, BrowserSessionRecorderService.MAX_RUNTIME_TEXT_FIELD_LENGTH) +
      ` ...(truncated ${value.length - BrowserSessionRecorderService.MAX_RUNTIME_TEXT_FIELD_LENGTH} chars)`;
  }

  private static tailStrings(values: string[]): string[] {
    const maxItems = BrowserSessionRecorderService.MAX_RUNTIME_HISTORY_ITEMS;
    if (values.length <= maxItems) {
      return values.slice();
    }
    return values.slice(values.length - maxItems);
  }
}
