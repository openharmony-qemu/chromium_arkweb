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

import { AgentState } from './AgentState';
import {
  AgentHistoryEntry,
  AgentNoteEntry,
  AgentObservation,
  AgentTabInfo,
  AgentTaskResult,
  AgentToolCall,
  AgentToolDescriptor,
  BrowserAgentAction,
  AgentPoint,
  AgentQuestionEntry
} from './AgentTypes';
import { BrowserAgentObservationService } from './ObservationService';
import { TaskTree } from './TaskTree';
import { AgentKeyPointsUtils } from './AgentKeyPoints';

export class AgentStateFactory { //感觉应该改成static方法
  static fork(state: AgentState): AgentState {
    const nextState = new AgentState();
    nextState.startTime = state.startTime;
    nextState.startUrl = state.startUrl;
    nextState.manipulateStep = state.manipulateStep;
    nextState.globalStep = state.globalStep;
    nextState.stepBudget = state.stepBudget;
    nextState.previousNode = state.previousNode;
    nextState.task = state.task;
    nextState.analysis = state.analysis;
    nextState.keyPoints = AgentKeyPointsUtils.cloneKeyPoints(state.keyPoints);
    nextState.pendingAnalysis = state.pendingAnalysis; // Promise 仅传引用，多次 await 安全

    nextState.phase = state.phase;
    nextState.observation = state.observation ? this.cloneObservation(state.observation) : null;
    nextState.previousObservation = state.previousObservation ? this.cloneObservation(state.previousObservation) : null;
    nextState.queueOriginObservation = state.queueOriginObservation ? this.cloneObservation(state.queueOriginObservation) : null;
    nextState.history = this.cloneHistory(state.history);
    nextState.availableTools = this.cloneToolDescriptors(state.availableTools);
    nextState.pendingBrowserAction = state.pendingBrowserAction ? this.cloneAction(state.pendingBrowserAction) : null;
    nextState.pendingToolCall = state.pendingToolCall ? this.cloneToolCall(state.pendingToolCall) : null;
    nextState.previousAction = state.previousAction ? this.cloneAction(state.previousAction) : null;
    nextState.summary = state.summary;
    nextState.actionQueue = this.cloneActions(state.actionQueue);
    nextState.note = this.cloneNotes(state.note);
    nextState.questions = this.cloneQuestions(state.questions);
    nextState.tabs = this.cloneTabs(state.tabs);
    nextState.taskTree = state.taskTree ? new TaskTree(state.taskTree) : null;
    nextState.status = state.status;
    nextState.previousReturnValue = state.previousReturnValue;
    nextState.previousAdvice = state.previousAdvice;
    nextState.loopAdvice = state.loopAdvice;
    nextState.previousIneffective = state.previousIneffective;
    nextState.treeText = state.treeText;
    nextState.liteTreeText = state.liteTreeText;
    nextState.liteDomDiff = state.liteDomDiff;
    nextState.queueStepDiffs = state.queueStepDiffs;
    nextState.queueStepCount = state.queueStepCount;
    nextState.needHtmlTextNext = state.needHtmlTextNext;
    nextState.needScreenshotNext = state.needScreenshotNext;
    nextState.inDispatch = state.inDispatch;
    nextState.justEnteredDispatch = state.justEnteredDispatch;
    nextState.injectContent = state.injectContent;
    nextState.selfHandleIgnorePermitted = state.selfHandleIgnorePermitted;
    nextState.sensitiveActionAlwaysPermittedReasons = state.sensitiveActionAlwaysPermittedReasons.slice();
    nextState.shouldConcludeAfterSensitiveAction = state.shouldConcludeAfterSensitiveAction;
    nextState.result = this.cloneResult(state.result);
    nextState.stopRequested = state.stopRequested;
    nextState.errorMessage = state.errorMessage;
    return nextState;
  }

  static cloneObservation(observation: AgentObservation): AgentObservation {
    const nextObservation = new AgentObservation();
    nextObservation.currentUrl = observation.currentUrl;
    nextObservation.currentTitle = observation.currentTitle;
    nextObservation.html = observation.html;
    nextObservation.liteDomTree = BrowserAgentObservationService.cloneLiteDomTree(observation.liteDomTree);
    nextObservation.outsideDOMTree = BrowserAgentObservationService.cloneOutsideDOMTree(observation.outsideDOMTree);
    nextObservation.tabs = this.cloneTabs(observation.tabs);
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

  static cloneToolDescriptors(tools: AgentToolDescriptor[]): AgentToolDescriptor[] {
    const nextTools: AgentToolDescriptor[] = [];
    for (let i = 0; i < tools.length; i++) {
      const tool = new AgentToolDescriptor();
      tool.toolId = tools[i].toolId;
      tool.displayName = tools[i].displayName;
      tool.description = tools[i].description;
      tool.domain = tools[i].domain;
      nextTools.push(tool);
    }
    return nextTools;
  }

  static cloneAction(action: BrowserAgentAction): BrowserAgentAction {
    const nextAction = new BrowserAgentAction();
    nextAction.action = action.action;
    nextAction.actionType = action.actionType;
    nextAction.element = action.element;
    nextAction.params = action.params.slice();
    nextAction.requiresLoadingWait = action.requiresLoadingWait;
    nextAction.reason = action.reason;
    nextAction.skipSensitiveActionCheck = action.skipSensitiveActionCheck;
    nextAction.xpath = action.xpath;
    if (action.point) {
      const point = new AgentPoint();
      point.x = action.point.x;
      point.y = action.point.y;
      nextAction.point = point;
    }
    return nextAction;
  }

  static cloneActions(actions: BrowserAgentAction[]): BrowserAgentAction[] {
    const nextActions: BrowserAgentAction[] = [];
    for (let i = 0; i < actions.length; i++) {
      nextActions.push(this.cloneAction(actions[i]));
    }
    return nextActions;
  }

  static cloneToolCall(toolCall: AgentToolCall | null): AgentToolCall | null {
    if (toolCall === null) {
      return null;
    }
    const nextCall = new AgentToolCall();
    nextCall.toolId = toolCall.toolId;
    nextCall.toolName = toolCall.toolName;
    nextCall.inputText = toolCall.inputText;
    return nextCall;
  }

  static cloneHistory(history: AgentHistoryEntry[]): AgentHistoryEntry[] {
    const nextHistory: AgentHistoryEntry[] = [];
    for (let i = 0; i < history.length; i++) {
      const entry = new AgentHistoryEntry();
      entry.reason = history[i].reason;
      entry.action = history[i].action;
      entry.actionText = history[i].actionText;
      entry.previousAction = history[i].previousAction;
      entry.observation = history[i].observation;
      entry.takeNote = history[i].takeNote;
      entry.toolCall = history[i].toolCall;
      entry.explanation = history[i].explanation;
      nextHistory.push(entry);
    }
    return nextHistory;
  }

  static cloneNotes(notes: AgentNoteEntry[]): AgentNoteEntry[] {
    const nextNotes: AgentNoteEntry[] = [];
    for (let i = 0; i < notes.length; i++) {
      const note = new AgentNoteEntry();
      note.webpageTitle = notes[i].webpageTitle;
      note.webpageUrl = notes[i].webpageUrl;
      note.information = notes[i].information;
      nextNotes.push(note);
    }
    return nextNotes;
  }

  static cloneQuestions(notes: AgentQuestionEntry[]): AgentQuestionEntry[] {
    const nextNotes: AgentQuestionEntry[] = [];
    for (let i = 0; i < notes.length; i++) {
      const note = new AgentQuestionEntry();
      note.question = notes[i].question;
      note.answer = notes[i].answer;
      nextNotes.push(note);
    }
    return nextNotes;
  }

  static cloneTabs(tabs: AgentTabInfo[]): AgentTabInfo[] {
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

  static cloneResult(result: AgentTaskResult): AgentTaskResult {
    const nextResult = new AgentTaskResult();
    nextResult.success = result.success;
    nextResult.status = result.status;
    nextResult.message = result.message;
    nextResult.statusCode = result.statusCode;
    nextResult.answer = result.answer;
    nextResult.reference = result.reference;
    nextResult.tracePayloadText = result.tracePayloadText;
    nextResult.version = result.version;
    return nextResult;
  }
}
