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

import { AgentState } from '../core/AgentState';
import { AgentNoteEntry, AgentObservation, AgentTabInfo } from '../core/AgentTypes';
import { BrowserAgentObservationService } from '../core/ObservationService';

import {
  ManipulateState,
  BrowserPerceptionPayload,
  BrowserRuntimeState,
  BrowserTabInfo,
} from '../../../../shared/domain/BrowserAutomationTypes';

export class BrowserAgentStateMapper {
  static toAgentState(runtime: BrowserRuntimeState): AgentState {
    const state = new AgentState();
    state.startTime = Date.now();
    state.task = runtime.task;
    state.phase = runtime.phase;
    state.observation = runtime.currentPerception ?
      BrowserAgentStateMapper.toObservation(runtime.currentPerception) :
      null;
    state.summary = runtime.lastSummary;
    state.note = BrowserAgentStateMapper.toNotes(runtime.noteHistory);
    state.tabs = BrowserAgentStateMapper.toAgentTabs(runtime.manipulateState.tabs);
    state.taskTree = null;
    state.manipulateStep = runtime.actionHistory.length;
    state.status = runtime.errorMessage.length >0 ? runtime.errorMessage : 'success';
    state.stopRequested = runtime.stopRequested;
    state.errorMessage = runtime.errorMessage;
    return state;
  }

  static applyAgentState(runtime: BrowserRuntimeState, state: AgentState): BrowserRuntimeState {
    const nextRuntime = BrowserAgentStateMapper.cloneRuntime(runtime);
    nextRuntime.task = state.task;
    nextRuntime.phase = state.phase;
    nextRuntime.currentPerception = state.observation ?
      BrowserAgentStateMapper.toPerception(state.observation) :
      null;
    nextRuntime.manipulateState.tabs = BrowserAgentStateMapper.toBrowserTabs(state.tabs);
    nextRuntime.lastSummary = state.summary;
    nextRuntime.actionHistory = state.history.map(x=>x.action);
    nextRuntime.noteHistory = BrowserAgentStateMapper.toNoteHistory(state.note);
    nextRuntime.stopRequested = state.stopRequested;
    nextRuntime.errorMessage = state.errorMessage;
    nextRuntime.updatedAt = Date.now();
    return nextRuntime;
  }

  private static toObservation(perception: BrowserPerceptionPayload): AgentObservation {
    return BrowserAgentObservationService.fromPerception(perception);
  }

  private static toPerception(observation: AgentObservation): BrowserPerceptionPayload {
    const perception = new BrowserPerceptionPayload();
    perception.currentUrl = observation.currentUrl;
    perception.currentTitle = observation.currentTitle;
    perception.liteDomTree = BrowserAgentObservationService.cloneLiteDomTree(observation.liteDomTree);
    perception.nearbyLiteDomTree = BrowserAgentObservationService.flattenOutsideDOMTree(observation.outsideDOMTree);
    perception.liteDomSummary = observation.liteDomSummary;
    perception.originalScreenshot = observation.originalScreenshot;
    perception.annotatedScreenshot = observation.annotatedScreenshot;
    perception.viewport.width = observation.viewport.width;
    perception.viewport.height = observation.viewport.height;
    perception.hasOpenDialog = observation.hasOpenDialog;
    perception.dialogState = observation.dialogState;
    return perception;
  }

  private static cloneObservation(observation: AgentObservation): AgentObservation {
    const nextObservation = new AgentObservation();
    nextObservation.currentUrl = observation.currentUrl;
    nextObservation.currentTitle = observation.currentTitle;
    nextObservation.html = observation.html;
    nextObservation.liteDomTree = BrowserAgentObservationService.cloneLiteDomTree(observation.liteDomTree);
    nextObservation.outsideDOMTree = BrowserAgentObservationService.cloneOutsideDOMTree(observation.outsideDOMTree);
    nextObservation.tabs = observation.tabs.map(tab => {
      const t = new AgentTabInfo();
      t.title = tab.title;
      t.url = tab.url;
      t.active = tab.active;
      return t;
    });
    nextObservation.liteDomSummary = observation.liteDomSummary;
    nextObservation.outsideDomSummary = observation.outsideDomSummary;
    nextObservation.htmlHash = observation.htmlHash;
    nextObservation.originalScreenshot = observation.originalScreenshot;
    nextObservation.annotatedScreenshot = observation.annotatedScreenshot;
    nextObservation.viewport.width = observation.viewport.width;
    nextObservation.viewport.height = observation.viewport.height;
    nextObservation.siteId = observation.siteId;
    nextObservation.pageStateId = observation.pageStateId;
    nextObservation.pageStateName = observation.pageStateName;
    nextObservation.hasOpenDialog = observation.hasOpenDialog;
    nextObservation.dialogState = observation.dialogState;
    nextObservation.tabStatus = observation.tabStatus;
    return nextObservation;
  }

  private static toAgentTabs(tabs: BrowserTabInfo[]): AgentTabInfo[] {
    const nextTabs: AgentTabInfo[] = [];
    for (let i = 0; i < tabs.length; i++) {
      const tab = new AgentTabInfo();
      tab.tabId = tabs[i].tabId;
      tab.windowId = tabs[i].windowId;
      tab.title = tabs[i].title;
      tab.url = tabs[i].url;
      tab.active = tabs[i].active;
      nextTabs.push(tab);
    }
    return nextTabs;
  }

  private static toBrowserTabs(tabs: AgentTabInfo[]): BrowserTabInfo[] {
    const nextTabs: BrowserTabInfo[] = [];
    for (let i = 0; i < tabs.length; i++) {
      const tab = new BrowserTabInfo();
      tab.tabId = tabs[i].tabId;
      tab.windowId = tabs[i].windowId;
      tab.title = tabs[i].title;
      tab.url = tabs[i].url;
      tab.active = tabs[i].active;
      nextTabs.push(tab);
    }
    return nextTabs;
  }

  private static toNotes(noteHistory: string[]): AgentNoteEntry[] {
    const notes: AgentNoteEntry[] = [];
    for (let i = 0; i < noteHistory.length; i++) {
      const note = new AgentNoteEntry();
      note.webpageTitle = '';
      note.webpageUrl = '';
      note.information = noteHistory[i];
      notes.push(note);
    }
    return notes;
  }

  private static toNoteHistory(notes: AgentNoteEntry[]): string[] {
    const noteHistory: string[] = [];
    for (let i = 0; i < notes.length; i++) {
      noteHistory.push(notes[i].information);
    }
    return noteHistory;
  }

  private static cloneRuntime(runtime: BrowserRuntimeState): BrowserRuntimeState {
    const nextRuntime = new BrowserRuntimeState();
    nextRuntime.sessionKey = runtime.sessionKey;
    nextRuntime.sessionDir = runtime.sessionDir;
    nextRuntime.needClearBrowser = runtime.needClearBrowser;
    nextRuntime.browserId = runtime.browserId;
    nextRuntime.transportOwner = runtime.transportOwner;
    nextRuntime.protocolVersion = runtime.protocolVersion;
    nextRuntime.phase = runtime.phase;
    nextRuntime.task = runtime.task;
    nextRuntime.manipulateState = BrowserAgentStateMapper.cloneManipulateState(runtime.manipulateState);
    nextRuntime.currentPerception = runtime.currentPerception;
    nextRuntime.lastSummary = runtime.lastSummary;
    nextRuntime.actionHistory = runtime.actionHistory.slice();
    nextRuntime.noteHistory = runtime.noteHistory.slice();
    nextRuntime.pendingRequestId = runtime.pendingRequestId;
    nextRuntime.ready = runtime.ready;
    nextRuntime.stopRequested = runtime.stopRequested;
    nextRuntime.disconnected = runtime.disconnected;
    nextRuntime.disconnectedReason = runtime.disconnectedReason;
    nextRuntime.errorMessage = runtime.errorMessage;
    nextRuntime.proceduralMemory = runtime.proceduralMemory;
    nextRuntime.sessionMemory = runtime.sessionMemory;
    nextRuntime.createdAt = runtime.createdAt;
    nextRuntime.updatedAt = runtime.updatedAt;
    return nextRuntime;
  }

  private static cloneManipulateState(state: ManipulateState): ManipulateState {
    const copy = new ManipulateState();
    copy.instruction = state.instruction;
    copy.startUrl = state.startUrl;
    copy.manipulateStep = state.manipulateStep;
    copy.globalStep = state.globalStep;
    copy.stepBudget = state.stepBudget;
    copy.previousImage = state.previousImage;
    copy.previousOriginalImage = state.previousOriginalImage;
    copy.currentImage = state.currentImage;
    copy.currentOriginalImage = state.currentOriginalImage;
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

  private static readString(source: Record<string, Object>, key: string, fallback: string): string {
    const value = source[key];
    return typeof value === 'string' ? value : fallback;
  }

  private static readNumber(source: Record<string, Object>, key: string, fallback: number): number {
    const value = source[key];
    return typeof value === 'number' ? value : fallback;
  }
}
