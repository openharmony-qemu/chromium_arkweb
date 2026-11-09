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

import { AgentDecision, AgentLiteDomNode, AgentObservation, BrowserAgentAction } from '../../core/AgentTypes';
import { BrowserAgentUtils } from '../../core/Utils';
import { AgentToolContext } from '../../ports/AgentContext';
import { PickerScrollToViewBuilder } from './PickerScrollToViewBuilder';

export class PickerScrollActionRecord {
  action: string = '';
  status: string = 'attempted';
}

export class PickerScrollRunResult {
  observation: AgentObservation = new AgentObservation();
  executedCount: number = 0;
  payloadText: string = '';
  error: Error | null = null;
  scrollActions: PickerScrollActionRecord[] = [];
}

class PickerScrollSession {
  action: BrowserAgentAction = new BrowserAgentAction();
  previousDistance: number = 0;
  nonImproving: number = 0;
}

/** Agent-only corrections using existing SCROLL and perception calls; no LLM calls. */
export class PickerScrollToViewRunner {
  static async run(
    context: AgentToolContext, target: string, initialAction: BrowserAgentAction, initial: AgentObservation
  ): Promise<PickerScrollRunResult> {
    const result = new PickerScrollRunResult();
    result.observation = initial;
    const originalContainer = initial.liteDomTree.find((node: AgentLiteDomNode) => node.id === initialAction.element);
    if (!originalContainer || (originalContainer.xpath.length === 0 && originalContainer.selector.length === 0)) {
      result.error = new Error('SCROLL_TO_VIEW: container identity unavailable.');
      return result;
    }
    try {
      await PickerScrollToViewRunner.runAttempts(
        context, target, initialAction, initial, originalContainer, result);
    } catch (error) {
      // Propagate unexpected failures to the tool's existing recovery path.
      result.error = error instanceof Error ? error : new Error(BrowserAgentUtils.stringifyError(error as Object));
    }
    return result;
  }

  private static async runAttempts(
    context: AgentToolContext,
    target: string,
    initialAction: BrowserAgentAction,
    initial: AgentObservation,
    originalContainer: AgentLiteDomNode,
    result: PickerScrollRunResult
  ): Promise<void> {
    const session = new PickerScrollSession();
    session.action = initialAction;
    session.previousDistance = PickerScrollToViewRunner.initialDistance(initialAction, originalContainer);
    for (let attempt = 0; attempt < 6; attempt++) {
      const done = await PickerScrollToViewRunner.runOneAttempt(
        context, target, initialAction, initial, originalContainer, result, session, attempt);
      if (done) {
        return;
      }
    }
    result.error = new Error('SCROLL_TO_VIEW: target is still outside view after 6 scroll attempts.');
  }

  private static async runOneAttempt(
    context: AgentToolContext,
    target: string,
    initialAction: BrowserAgentAction,
    initial: AgentObservation,
    originalContainer: AgentLiteDomNode,
    result: PickerScrollRunResult,
    session: PickerScrollSession,
    attempt: number
  ): Promise<boolean> {
    result.error = await PickerScrollToViewRunner.executeScroll(context, session.action, result, attempt > 0);
    if (result.error) {
      return true;
    }
    if (result.observation.currentUrl !== initial.currentUrl) {
      result.error = new Error('SCROLL_TO_VIEW: page changed during scrolling.');
      return true;
    }
    const container = PickerScrollToViewRunner.findContainer(result.observation, originalContainer);
    if (!container) {
      result.error = new Error('SCROLL_TO_VIEW: original container is no longer available.');
      return true;
    }
    const plan = PickerScrollToViewBuilder.buildPlan(target, container.id, result.observation, initialAction.reason);
    if (plan instanceof Error) {
      result.error = plan;
      return true;
    }
    if (!plan.action) {
      result.payloadText = `SCROLL_TO_VIEW: "${target}" is fully visible after ${result.executedCount} scrolls.`;
      return true;
    }
    session.nonImproving = plan.distance >= session.previousDistance - 1 ? session.nonImproving + 1 : 0;
    if (session.nonImproving >= 2) {
      result.error = new Error('SCROLL_TO_VIEW: repeated corrections did not reduce the target distance.');
      return true;
    }
    session.previousDistance = plan.distance;
    session.action = plan.action;
    return false;
  }

  private static async executeScroll(
    context: AgentToolContext, action: BrowserAgentAction, result: PickerScrollRunResult, checkRisk: boolean
  ): Promise<Error | null> {
    try {
      const riskError = await PickerScrollToViewRunner.rejectRiskyCorrection(context, action, checkRisk);
      if (riskError) {
        return riskError;
      }
      const record = new PickerScrollActionRecord();
      record.action = action.action;
      result.scrollActions.push(record);
      const [payload] = await BrowserAgentUtils.contextInteract(context, () => context.env.execute(action));
      record.status = 'executed';
      result.executedCount++;
      result.payloadText = payload;
      await PickerScrollToViewRunner.delay(500);
      const [observation] = await BrowserAgentUtils.contextInteract(context, () => context.env.perceive());
      result.observation = observation;
      return null;
    } catch (error) {
      PickerScrollToViewRunner.markLastScrollFailed(result);
      return error instanceof Error ? error : new Error(BrowserAgentUtils.stringifyError(error as Object));
    }
  }

  private static async rejectRiskyCorrection(
    context: AgentToolContext, action: BrowserAgentAction, checkRisk: boolean
  ): Promise<Error | null> {
    if (!checkRisk || !context.events.checkAction) {
      return null;
    }
    const decision = new AgentDecision();
    decision.browserAction = action;
    decision.explanation = 'Correct picker scroll to bring the exact target into view.';
    if (await context.events.checkAction(decision)) {
      return null;
    }
    return new Error('SCROLL_TO_VIEW correction blocked by risk control.');
  }

  private static markLastScrollFailed(result: PickerScrollRunResult): void {
    const lastRecord = result.scrollActions[result.scrollActions.length - 1];
    if (!lastRecord || lastRecord.status !== 'attempted') {
      return;
    }
    lastRecord.status = 'failed';
  }

  private static initialDistance(action: BrowserAgentAction, container: AgentLiteDomNode): number {
    const axisSize = action.params[0] === 'UP' || action.params[0] === 'DOWN' ?
      container.scrollViewportHeight : container.scrollViewportWidth;
    return Math.abs(Number(action.params[2])) * axisSize;
  }

  private static findContainer(observation: AgentObservation, original: AgentLiteDomNode): AgentLiteDomNode | null {
    const matches = observation.liteDomTree.filter((node: AgentLiteDomNode) =>
      node.element.tagName === original.element.tagName &&
      (original.selector.length > 0 ? node.selector === original.selector : node.xpath === original.xpath));
    return matches.length === 1 ? matches[0] : null;
  }

  private static async delay(ms: number): Promise<void> {
    await new Promise<void>((resolve) => setTimeout(resolve, ms));
  }
}
