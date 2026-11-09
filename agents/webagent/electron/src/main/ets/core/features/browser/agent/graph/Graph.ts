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

import { AgentGraphRunResult } from './AgentGraph';
import { AgentState } from '../core/AgentState';
import { AgentStateFactory } from '../core/AgentStateFactory';
import {
  AgentConstants, AgentDecision, AgentGraphResult, AgentGraphStatus, AgentPhase,
  BrowserAgentAction, UserControlOptionType, UserControlReasonType
} from '../core/AgentTypes';
import { BrowserAgentReasoningService } from '../core/ReasoningService';
import { BrowserAgentUtils } from '../core/Utils';
import { BrowserAgentObservationService } from '../core/ObservationService';
import { BrowserAgentPromptDetectionService } from '../core/PromptDetectionService';
import { MultiStepActionService } from '../core/MultiStepActionService';
import { HtmlParser } from '../core/HtmlParser';
import { BrowserAgentGraphContext } from '../ports/AgentContext';
import { vConfig } from '../VersionConfig';

// A single observe / decide / act loop. Tool implementations and their prompt
// descriptions remain in AgentToolRegistryService and ReasoningService.
export class BrowserAgentGraph {
  private readonly reasoning = new BrowserAgentReasoningService();
  private stopRequested: boolean = false;

  constructor(isDebug: boolean) {
    void isDebug;
  }

  requestStop(): void {
    this.stopRequested = true;
  }

  async run(initialState: AgentState, context: BrowserAgentGraphContext): Promise<AgentGraphRunResult<AgentState>> {
    let state = AgentStateFactory.fork(initialState);
    if (state.startTime === 0) {
      state.startTime = Date.now();
    }
    if (state.task.startsWith(AgentConstants.NAVIGATE_PREFIX)) {
      state.task = state.task.substring(AgentConstants.NAVIGATE_PREFIX.length).trim();
    } else if (state.task.startsWith(AgentConstants.CONTINUE_PREFIX)) {
      state.task = state.task.substring(AgentConstants.CONTINUE_PREFIX.length).trim();
    }

    while (!this.stopRequested && !state.stopRequested && state.stepBudget > 0 &&
      Date.now() - state.startTime < AgentConstants.MAX_TIME_MS) {
      try {
        state.phase = AgentPhase.OBSERVING;
        const [observation, userControlled] = await BrowserAgentUtils.contextInteract(context, () => context.env.perceive());
        if (this.stopRequested || state.stopRequested) {
          break;
        }
        BrowserAgentUtils.markIfUserControlled(state, userControlled);
        state.previousObservation = state.observation;
        state.observation = observation;
        state.tabs = AgentStateFactory.cloneTabs(BrowserAgentUtils.updateTabContext(observation.tabs, state.tabs));
        state.availableTools = await context.tools.listAvailable(this.reasoning.buildToolQuery(state, observation));
        state.globalStep++;
        state.manipulateStep++;
        state.stepBudget--;
        if (state.startUrl.length === 0) {
          state.startUrl = observation.currentUrl;
        }

        const parser = BrowserAgentUtils.parseHtml(state);
        state.phase = AgentPhase.THINKING;
        const request = this.reasoning.buildObserveAndReasonLLMRequest(
          state, observation, state.availableTools, false, false, parser);
        if (context.events.checkPrompt) {
          const pageText = BrowserAgentPromptDetectionService.build(this.reasoning.buildTreeText(state), observation);
          const images = await BrowserAgentUtils.base64ToPixelMaps(request.images);
          const detection = await context.events.checkPrompt(images, pageText);
          if (detection.riskCode === AgentConstants.INJECTED) {
            throw new Error('Page content was flagged as prompt injection. User review is required.');
          }
        }
        const response = await context.llm.complete(request, 'browserLoop');
        if (this.stopRequested || state.stopRequested) {
          break;
        }
        const decision = this.reasoning.decide(response);
        this.validateDecision(decision, state, parser);
        if (context.events.checkAction && !await context.events.checkAction(decision)) {
          throw new Error('Action blocked by risk control.');
        }
        state = this.reasoning.applyReasoningOutputs(state, observation, decision, parser);
        context.events.onChunk(decision.explanation || decision.reason, false);

        if (decision.browserAction?.action.trim() === 'FINISH') {
          return this.finish(state, AgentGraphStatus.FINISH, 'Finished successfully.');
        }
        if (decision.toolCall) {
          const call = decision.toolCall;
          const available = state.availableTools.some(tool => tool.toolId === call.toolId);
          if (!available) {
            throw new Error('The requested tool is not available on this page.');
          }
          const result = await context.tools.invoke(call, state);
          state.previousReturnValue = result.returnValue || result.summary;
          if (!result.handled || !result.success) {
            throw new Error(state.previousReturnValue || 'Tool execution failed.');
          }
          if (result.note.length > 0) {
            this.reasoning.appendNoteIfMeaningful(state, result.note, observation, parser);
          }
          state.previousAction = null;
          continue;
        }
        const action = decision.browserAction;
        if (!action) {
          throw new Error('The model did not choose a browser action.');
        }
        if (action.actionType === 'USER_CONTROL' || action.actionType === 'ASK_USER') {
          state.previousReturnValue = await this.requestUserControl(action, context);
          continue;
        }
        if (action.action.trim() === 'WAIT') {
          await new Promise<void>((resolve) => setTimeout(resolve, 500));
          continue;
        }
        state.phase = AgentPhase.ACTING;
        const executed = BrowserAgentUtils.adaptSelectAction(action, observation.liteDomTree);
        state.previousReturnValue = await context.env.execute(executed);
        if (action.requiresLoadingWait) {
          await new Promise<void>((resolve) => setTimeout(resolve, 500));
        }
      } catch (error) {
        if (this.stopRequested || state.stopRequested) {
          break;
        }
        state.phase = AgentPhase.FAILED;
        state.errorMessage = error instanceof Error ? error.message : String(error);
        return this.finish(state, AgentGraphStatus.ERROR, state.errorMessage);
      }
    }
    if (this.stopRequested || state.stopRequested) {
      state.stopRequested = true;
      state.phase = AgentPhase.FINISHED;
      return this.finish(state, AgentGraphStatus.FINISH, 'Stopped by user.');
    }
    context.events.onStepExceeded?.();
    state.phase = AgentPhase.FAILED;
    state.errorMessage = 'Agent exceeded the step or time limit.';
    return this.finish(state, AgentGraphStatus.TIMEOUT, state.errorMessage);
  }

  private validateDecision(decision: AgentDecision, state: AgentState,
    parser: HtmlParser): void {
    if (!decision.browserAction && !decision.toolCall) {
      throw new Error('The model did not choose a browser action or tool.');
    }
    const observation = state.observation!;
    if (['危险网址', '风险提示'].includes(observation.currentTitle) &&
      decision.browserAction?.actionType !== 'USER_CONTROL') {
      throw new Error('The browser marked this page as risky. User control is required.');
    }
    if (!decision.browserAction) {
      return;
    }
    if (decision.browserAction.actionType === 'DISPATCH') {
      throw new Error('Subtask dispatch is unavailable in the sample loop.');
    }
    const combinedTree = observation.liteDomTree.slice();
    const outside = BrowserAgentObservationService.flattenOutsideDOMTreeSortedById(observation.outsideDOMTree);
    for (const node of outside) {
      combinedTree.push(node);
    }
    state.previousReturnValue = BrowserAgentUtils.checkActionParameter(decision, combinedTree, parser);
    if (decision.explanation.includes('关闭登录弹窗')) {
      throw new Error('Ask the user to handle the login popup.');
    }
    const deferredHint = MultiStepActionService.maybeDeferOutsideElementAction(decision, observation, combinedTree);
    if (deferredHint.length > 0) {
      state.previousReturnValue = deferredHint;
    }
  }

  private async requestUserControl(action: BrowserAgentAction, context: BrowserAgentGraphContext): Promise<string> {
    if (!context.events.onNeedUserControl) {
      throw new Error('User control is unavailable.');
    }
    const response = await context.events.onNeedUserControl({
      type: action.actionType === 'ASK_USER' ? UserControlReasonType.ASK_USER : UserControlReasonType.USER_CONTROL,
      explanation: action.reason,
      question: action.params.length > 0 ? action.params[0] : action.reason
    }, [UserControlOptionType.TAKEOVER, UserControlOptionType.ABORT]);
    if (action.actionType === 'ASK_USER' && response.answer && response.answer.length > 0) {
      return `User answered: ${response.answer}`;
    }
    if (response.choiceKey !== UserControlOptionType.TAKEOVER) {
      throw new Error('User declined browser control.');
    }
    const takeover = new BrowserAgentAction();
    takeover.action = 'USER_CONTROL';
    takeover.actionType = 'USER_CONTROL';
    takeover.reason = action.reason;
    takeover.requiresLoadingWait = false;
    await context.env.execute(takeover);
    context.events.onSwitchControl?.('user');
    const startedAt = Date.now();
    while (Date.now() - startedAt < AgentConstants.USER_COMPLETE_TIMEOUT && !this.stopRequested) {
      const status = BrowserAgentUtils.parseQueryResult(await context.env.execute(BrowserAgentUtils.buildQueryAction()));
      if (status.status === 'agent') {
        await context.env.execute(BrowserAgentUtils.buildAgentControlAction());
        context.events.onSwitchControl?.('agent');
        return 'User returned browser control to the agent.';
      }
      if (status.status === 'idle') {
        throw new Error('The user ended the task during takeover.');
      }
      await new Promise<void>((resolve) => setTimeout(resolve, 500));
    }
    throw new Error('User control timed out or the task was stopped.');
  }

  private finish(state: AgentState, status: string, message: string): AgentGraphRunResult<AgentState> {
    state.result.success = status === AgentGraphStatus.FINISH && !state.stopRequested;
    state.result.status = state.result.success ? 'success' : 'failed';
    state.result.statusCode = state.result.success ? 200 : 500;
    state.result.answer = state.result.success ? (state.summary || message) : message;
    state.result.message = state.result.answer;
    state.result.version = vConfig.version;
    const result = new AgentGraphResult();
    result.status = status;
    result.message = message;
    result.nodeName = 'loop';
    return new AgentGraphRunResult(state, result);
  }
}
