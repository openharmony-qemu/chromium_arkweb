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
  BrowserAgentConfig,
  BrowserPerceptionPayload,
  ManipulateState,
  BrowserPhase,
  BrowserRuntimeState
} from '../../../shared/domain/BrowserAutomationTypes';
import { BrowserSessionRecorderService } from './BrowserSessionRecorderService';
import { BrowserSessionWorkspaceService } from './BrowserSessionWorkspaceService';
import { BrowserManipulateStateService } from '../state/BrowserManipulateStateService';
import BuildProfile from 'BuildProfile';

export class BrowserRuntimeService {
  private static runtimeSeed: number = 0;

  static createRuntime(
    task: string,
    config: BrowserAgentConfig | null = null,
    sessionKey: string = '',
    sessionDir: string = '',
    needClearBrowser: boolean = false,
    calledAt: number = 0,
  ): BrowserRuntimeState {
    const runtime = new BrowserRuntimeState();
    runtime.sessionKey = sessionKey.length > 0 ? sessionKey : BrowserRuntimeService.nextSessionKey();
    if (BuildProfile.DEBUG) {
      let preferredSessionDir = sessionDir;
      if (preferredSessionDir.length <= 0) {
        const filesDir = BrowserSessionWorkspaceService.getFilesDir().replace(/\\/g, '/');
        const runStartedAt = calledAt > 0 ? calledAt : Date.now();
        if (filesDir.length > 0) {
          preferredSessionDir =
            `${filesDir}/agent_sessions/${runtime.sessionKey}/run_${runStartedAt}`;
        }
      }
      runtime.sessionDir = BrowserSessionWorkspaceService.resolveSessionDir(
        runtime.sessionKey,
        preferredSessionDir
      );
    } else {
      runtime.sessionDir = '';
    }
    runtime.needClearBrowser = needClearBrowser;
    runtime.phase = BrowserPhase.LAUNCHING;
    runtime.task = task;
    runtime.config = config ? BrowserRuntimeService.cloneConfig(config) : new BrowserAgentConfig();
    runtime.manipulateState = BrowserManipulateStateService.createInitialState(task, runtime.config);
    runtime.createdAt = calledAt;
    runtime.updatedAt = runtime.createdAt;
    BrowserSessionRecorderService.syncRuntime(runtime);
    return BrowserRuntimeService.clone(runtime);
  }

  static updateRuntime(runtime: BrowserRuntimeState): BrowserRuntimeState {
    if (runtime.sessionKey.length <= 0) {
      runtime.sessionKey = BrowserRuntimeService.nextSessionKey();
    }
    runtime.updatedAt = Date.now();
    BrowserSessionRecorderService.syncRuntime(runtime);
    return BrowserRuntimeService.clone(runtime);
  }

  static clone(runtime: BrowserRuntimeState): BrowserRuntimeState {
    const copy = new BrowserRuntimeState();
    copy.sessionKey = runtime.sessionKey;
    copy.sessionDir = runtime.sessionDir;
    copy.needClearBrowser = runtime.needClearBrowser;
    copy.browserId = runtime.browserId;
    copy.transportOwner = runtime.transportOwner;
    copy.protocolVersion = runtime.protocolVersion;
    copy.phase = runtime.phase;
    copy.task = runtime.task;
    copy.config = BrowserRuntimeService.cloneConfig(runtime.config);
    copy.manipulateState = BrowserRuntimeService.cloneManipulateState(runtime.manipulateState);
    copy.currentPerception = runtime.currentPerception
      ? BrowserRuntimeService.clonePerception(runtime.currentPerception)
      : null;
    copy.lastActionText = runtime.lastActionText;
    copy.lastSummary = runtime.lastSummary;
    copy.actionHistory = runtime.actionHistory.slice();
    copy.noteHistory = runtime.noteHistory.slice();
    copy.pendingRequestId = runtime.pendingRequestId;
    copy.ready = runtime.ready;
    copy.stopRequested = runtime.stopRequested;
    copy.disconnected = runtime.disconnected;
    copy.disconnectedReason = runtime.disconnectedReason;
    copy.errorMessage = runtime.errorMessage;
    copy.proceduralMemory = runtime.proceduralMemory;
    copy.sessionMemory = runtime.sessionMemory;
    copy.createdAt = runtime.createdAt;
    copy.updatedAt = runtime.updatedAt;
    copy.cacheScreenshot = runtime.cacheScreenshot;
    copy.screenshotCacheDir = runtime.screenshotCacheDir;
    return copy;
  }

  private static nextSessionKey(): string {
    BrowserRuntimeService.runtimeSeed += 1;
    return `browser_session_${Date.now()}_${BrowserRuntimeService.runtimeSeed}`;
  }

  private static cloneConfig(config: BrowserAgentConfig): BrowserAgentConfig {
    const copy = new BrowserAgentConfig();
    copy.summary = config.summary;
    copy.note = config.note;
    copy.recursivePlan = config.recursivePlan;
    copy.separateJudge = config.separateJudge;
    copy.replan = config.replan;
    copy.takeNoteInObserve = config.takeNoteInObserve;
    copy.knowledge = config.knowledge;
    copy.previousScreen = config.previousScreen;
    copy.inputTreeText = config.inputTreeText;
    copy.outsideElement = config.outsideElement;
    copy.analyzeInstruction = config.analyzeInstruction;
    copy.conventional = config.conventional;
    copy.domPerLine = config.domPerLine;
    copy.model = config.model;
    copy.stepBudget = config.stepBudget;
    return copy;
  }

  private static cloneManipulateState(state: ManipulateState): ManipulateState {
    const copy = new ManipulateState();
    copy.instruction = state.instruction;
    copy.startUrl = state.startUrl;
    copy.manipulateStep = state.manipulateStep;
    copy.globalStep = state.globalStep;
    copy.stepBudget = state.stepBudget;
    copy.previousImage = '';
    copy.previousOriginalImage = '';
    copy.currentImage = '';
    copy.currentOriginalImage = '';
    copy.liteDomTree = [];
    copy.outsideElements = state.outsideElements;
    copy.summary = state.summary;
    copy.previousAction = state.previousAction;
    copy.history = state.history.slice();
    copy.tabs = state.tabs.slice();
    copy.note = state.note.slice();
    copy.knowledge = state.knowledge;
    copy.tools = state.tools;
    copy.analysis = state.analysis;
    copy.status = state.status;
    copy.previousIneffective = state.previousIneffective;
    copy.previousReturnValue = state.previousReturnValue;
    copy.actionQueue = state.actionQueue.slice();
    copy.currentUrl = state.currentUrl;
    return copy;
  }

  private static clonePerception(perception: BrowserPerceptionPayload): BrowserPerceptionPayload {
    const copy = new BrowserPerceptionPayload();
    copy.tabId = perception.tabId;
    copy.currentUrl = perception.currentUrl;
    copy.currentTitle = perception.currentTitle;
    copy.liteDomTree = perception.liteDomTree.slice();
    copy.liteDomSummary = perception.liteDomSummary;
    copy.originalScreenshot = '';
    copy.annotatedScreenshot = '';
    copy.viewport.width = perception.viewport.width;
    copy.viewport.height = perception.viewport.height;
    copy.hasOpenDialog = perception.hasOpenDialog;
    copy.dialogState = perception.dialogState;
    copy.rawPayloadText = '';
    return copy;
  }
}
