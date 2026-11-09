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
import { AgentHistoryEntry, AgentLLMMessage, AgentLLMRequest, AgentLLMResponse } from '../core/AgentTypes';
import { AgentLLMPort } from '../ports/AgentLLMPort';
import { AgentLLMScenes } from '../core/AgentLLMScenes';
import {
  MemoryMessagePayload,
  MemoryToolCallPayload,
  MemoryToolEntry
} from './MemoryTypes';
import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN = 0x6020;
const TAG = 'MemoryPayloadBuilder';

const MAX_CLUSTERS_BEFORE_LLM: number = 25;
const MAX_PLAN_LEN: number = 200;
const MAX_REASON_LEN: number = 80;
const MAX_RESULT_LEN: number = 60;
const MAX_FINAL_LEN: number = 1500;
const MAX_DIGEST_LEN: number = 4000;

const VERB_VOCABULARY: string[] = [
  'navigate_to',
  'search_query',
  'apply_filter',
  'pick_item',
  'fill_field',
  'select_option',
  'extract_listing',
  'extract_detail',
  'call_site_tool',
  'request_user_takeover',
  'finish'
];

const ABSTRACTION_SYSTEM_PROMPT: string = [
  'You are a memory extraction agent for a browser automation system.',
  'Given an executed task trajectory (already pre-clustered), output a JSON object that captures',
  '  (a) a brief retrospective plan of what actually happened, and',
  '  (b) one abstract tool call per cluster step, suitable for cross-task reuse.',
  '',
  '=== Allowed step name vocabulary (must pick one per step) ===',
  '- navigate_to: enter a site / a section of the site',
  '- search_query: type into a search field and submit',
  '- apply_filter: switch tab / category / sort order',
  '- pick_item: click on a listing item or a search result',
  '- fill_field: fill a form field (not paired with submit)',
  '- select_option: select from dropdown',
  '- extract_listing: collect items from a listing page',
  '- extract_detail: capture info from a detail page',
  '- call_site_tool: invoke a registered site-specific tool',
  '- request_user_takeover: ask user to take control',
  '- finish: produce final answer',
  '',
  '=== args schema (for portable args) ===',
  '{',
  '  "site": "<domain like weibo.com or none>",',
  '  "section": "<page section like 热搜榜, or empty>",',
  '  "target_role": "<semantic role of the target element, e.g. 搜索框/24小时筛选器/列表项, or empty>",',
  '  "value": "<user-supplied value such as the query string, or empty>",',
  '  "scope": "<scope hint such as 全站/分类/24h, or empty>",',
  '  "position": "<first/second/third/last, only for pick_item, or empty>"',
  '}',
  '',
  'CRITICAL: in args/result, NEVER include:',
  '- DOM ids like [10] or numeric element indexes',
  '- xpath / css selectors',
  '- pixel offsets or scroll distances',
  '- timestamps',
  '- screenshot references',
  '- ephemeral list indexes that depend on this run\'s data',
  '',
  'Only include cross-task reusable info: site domain, section name,',
  'semantic role of the target element, user-supplied values.',
  '',
  '=== Output JSON shape (strict) ===',
  '{',
  '  "plan": "## 任务拆解\\n1. ...\\n2. ... (≤ 200 chars total)",',
  '  "steps": [',
  '    {',
  '      "name": "<one of the vocabulary above>",',
  '      "reason": "<≤ 80 chars summary of why this step happened>",',
  '      "args": { /* portable args, see schema above */ },',
  '      "result": "<≤ 60 chars semantic outcome>"',
  '    }',
  '  ]',
  '}',
  '',
  'If a clustered step represents a noisy or unimportant action, you MAY drop it from steps.',
  'Output JSON only, no surrounding prose.'
].join('\n');

class ClusteredStep {
  public kind: string = '';
  public rawActions: string[] = [];
  public reason: string = '';
  public url: string = '';
  public takeNote: string = '';
  public toolCallJson: string = '';
}

export class MemoryAbstractedStep {
  public name: string = '';
  public reason: string = '';
  public args: Record<string, Object> = {};
  public argsConcreteHint: Record<string, Object> = {};
  public result: string = '';
}

export class MemoryAbstractedTrajectory {
  public plan: string = '';
  public steps: MemoryAbstractedStep[] = [];
}

export class MemoryPayloadBundle {
  public messages: MemoryMessagePayload[] = [];
  public toolCalls: MemoryToolCallPayload[] = [];
  public metadata: Record<string, string> = {};
}

export class MemoryPayloadBuilder {

  static async build(state: AgentState, llm: AgentLLMPort): Promise<MemoryPayloadBundle> {
    const clustered = MemoryPayloadBuilder.clusterSteps(state);
    hilog.debug(DOMAIN, TAG, 'clustered steps: %{public}d (from %{public}d history entries)',
      clustered.length, state.history.length);

    let abstracted: MemoryAbstractedTrajectory;
    try {
      abstracted = await MemoryPayloadBuilder.abstractWithLLM(state, clustered, llm);
      MemoryPayloadBuilder.attachConcreteHints(abstracted, clustered);
    } catch (error) {
      hilog.warn(DOMAIN, TAG, 'LLM abstraction failed, fallback to rules: %{public}s',
        error instanceof Error ? error.message : `${error}`);
      abstracted = MemoryPayloadBuilder.fallbackAbstract(state, clustered);
    }

    const bundle = new MemoryPayloadBundle();
    bundle.messages = MemoryPayloadBuilder.assembleMessages(state, abstracted);
    bundle.toolCalls = MemoryPayloadBuilder.buildToolCalls(abstracted);
    bundle.metadata = MemoryPayloadBuilder.buildMetadata(state, abstracted);
    return bundle;
  }

  // -------------------- clusterSteps --------------------

  static clusterSteps(state: AgentState): ClusteredStep[] {
    const out: ClusteredStep[] = [];
    const history = state.history;
    let i = 0;
    while (i < history.length) {
      const entry = history[i];
      const verb = MemoryPayloadBuilder.readVerb(entry.action);

      if (MemoryPayloadBuilder.isToolCallEntry(entry)) {
        const cluster = new ClusteredStep();
        cluster.kind = 'call_site_tool';
        cluster.rawActions = [entry.toolCall];
        cluster.reason = entry.reason;
        cluster.url = entry.observation;
        cluster.takeNote = entry.takeNote;
        cluster.toolCallJson = entry.toolCall;
        out.push(cluster);
        i += 1;
        continue;
      }

      if (MemoryPayloadBuilder.isDroppableVerb(verb, entry)) {
        i += 1;
        continue;
      }

      if (verb === 'FILL' && i + 1 < history.length) {
        const nextVerb = MemoryPayloadBuilder.readVerb(history[i + 1].action);
        if (nextVerb === 'SEARCH' || nextVerb === 'PRESS' || nextVerb === 'CLICK') {
          const cluster = new ClusteredStep();
          cluster.kind = 'search_query';
          cluster.rawActions = [entry.action, history[i + 1].action];
          cluster.reason = history[i + 1].reason.length > 0 ? history[i + 1].reason : entry.reason;
          cluster.url = entry.observation;
          cluster.takeNote = MemoryPayloadBuilder.mergeNotes(entry.takeNote, history[i + 1].takeNote);
          out.push(cluster);
          i += 2;
          continue;
        }
      }

      if (verb === 'SCROLL') {
        let j = i + 1;
        const collected: AgentHistoryEntry[] = [entry];
        while (j < history.length && MemoryPayloadBuilder.readVerb(history[j].action) === 'SCROLL') {
          collected.push(history[j]);
          j += 1;
        }
        const meaningful = MemoryPayloadBuilder.collectMeaningfulNotes(collected);
        if (meaningful.length > 0) {
          const cluster = new ClusteredStep();
          cluster.kind = 'extract_listing';
          cluster.rawActions = collected.map((c) => c.action);
          const lastEntry = collected[collected.length - 1];
          cluster.reason = lastEntry.reason;
          cluster.url = lastEntry.observation;
          cluster.takeNote = meaningful.join(' | ');
          out.push(cluster);
        }
        i = j;
        continue;
      }

      if (verb === 'NAVIGATION' || verb === 'NAVIGATE_TO') {
        const cluster = new ClusteredStep();
        cluster.kind = 'navigate_to';
        cluster.rawActions = [entry.action];
        cluster.reason = entry.reason;
        cluster.url = MemoryPayloadBuilder.extractFirstBracket(entry.action);
        if (cluster.url.length === 0) {
          cluster.url = entry.observation;
        }
        cluster.takeNote = entry.takeNote;
        out.push(cluster);
        i += 1;
        while (i < history.length && MemoryPayloadBuilder.readVerb(history[i].action) === 'WAIT') {
          i += 1;
        }
        continue;
      }

      const mapped = MemoryPayloadBuilder.mapVerb(verb, entry);
      if (mapped.length === 0) {
        i += 1;
        continue;
      }
      const cluster = new ClusteredStep();
      cluster.kind = mapped;
      cluster.rawActions = [entry.action];
      cluster.reason = entry.reason;
      cluster.url = entry.observation;
      cluster.takeNote = entry.takeNote;
      cluster.toolCallJson = entry.toolCall;
      out.push(cluster);
      i += 1;
    }

    const deduped = MemoryPayloadBuilder.dedupConsecutive(out);
    if (deduped.length <= MAX_CLUSTERS_BEFORE_LLM) {
      return deduped;
    }
    return MemoryPayloadBuilder.capClusters(deduped, MAX_CLUSTERS_BEFORE_LLM);
  }

  // -------------------- LLM abstraction --------------------

  private static async abstractWithLLM(
    state: AgentState,
    clustered: ClusteredStep[],
    llm: AgentLLMPort
  ): Promise<MemoryAbstractedTrajectory> {
    if (clustered.length === 0) {
      const empty = new MemoryAbstractedTrajectory();
      const task = MemoryPayloadBuilder.safeString(state.task);
      empty.plan = task.length > 0 ? `## 任务拆解\n${MemoryPayloadBuilder.escapeHtml(task)}` : '';
      empty.steps = [];
      return empty;
    }
    const digest = MemoryPayloadBuilder.buildClusterDigest(state, clustered);
    const userPrompt = [
      '## 用户任务',
      state.task,
      '',
      `## 是否成功: ${state.result.success ? 'success' : 'failed'}`,
      '',
      '## 最终回答',
      state.result.answer.length > 0 ? state.result.answer : (state.result.message.length > 0 ? state.result.message : '(none)'),
      '',
      '## 执行轨迹聚合 (已规则化预处理)',
      digest,
      '',
      'Now produce the JSON described in the system prompt:'
    ].join('\n');

    const request = new AgentLLMRequest();
    request.systemPrompt = ABSTRACTION_SYSTEM_PROMPT;
    request.userPrompt = userPrompt;
    request.scene = AgentLLMScenes.BROWSER_DECISION;
    request.responseFormat = 'json_object';
    const llmMessage = new AgentLLMMessage();
    llmMessage.role = 'user';
    llmMessage.content = userPrompt;
    request.messages = [llmMessage];

    const response = await llm.complete(request);
    const rawText = response.text.length > 0 ? response.text : response.rawText;
    return MemoryPayloadBuilder.parseAbstractedJson(rawText, state, clustered);
  }

  private static parseAbstractedJson(
    rawText: string,
    state: AgentState,
    clustered: ClusteredStep[]
  ): MemoryAbstractedTrajectory {
    const jsonText = MemoryPayloadBuilder.extractJsonText(rawText);
    const trajectory = new MemoryAbstractedTrajectory();
    const parsed = MemoryPayloadBuilder.parseJsonRecord(jsonText);
    if (parsed === null) {
      return MemoryPayloadBuilder.fallbackAbstract(state, clustered);
    }
    trajectory.plan = typeof parsed.plan === 'string' ? MemoryPayloadBuilder.escapeHtml(parsed.plan as string) : '';

    const stepsValue = parsed.steps;
    if (!Array.isArray(stepsValue)) {
      trajectory.steps = MemoryPayloadBuilder.fallbackAbstract(state, clustered).steps;
      return trajectory;
    }
    const stepArray: Object[] = stepsValue as Object[];
    const out: MemoryAbstractedStep[] = [];
    for (let i = 0; i < stepArray.length && i < MAX_CLUSTERS_BEFORE_LLM; i++) {
      const item: Object = stepArray[i];
      if (typeof item !== 'object' || item === null) {
        continue;
      }
      const record = item as Record<string, Object>;
      const step = new MemoryAbstractedStep();
      step.name = MemoryPayloadBuilder.normalizeStepName(
        typeof record.name === 'string' ? record.name as string : ''
      );
      step.reason = typeof record.reason === 'string' ? record.reason as string : '';
      step.result = typeof record.result === 'string' ? record.result as string : '';
      const args = record.args;
      if (typeof args === 'object' && args !== null) {
        step.args = MemoryPayloadBuilder.sanitizeArgs(args as Record<string, Object>);
      }
      out.push(step);
    }
    trajectory.steps = out;
    if (trajectory.steps.length === 0) {
      trajectory.steps = MemoryPayloadBuilder.fallbackAbstract(state, clustered).steps;
    }
    return trajectory;
  }

  private static buildClusterDigest(state: AgentState, clustered: ClusteredStep[]): string {
    const lines: string[] = [];
    for (let i = 0; i < clustered.length; i++) {
      const c = clustered[i];
      const rawActions = MemoryPayloadBuilder.safeStringArray(c.rawActions);
      const rawAction = rawActions.length > 0 ? rawActions.join(' ; ') : '(none)';
      const urlRaw = MemoryPayloadBuilder.safeString(c.url);
      const url = urlRaw.length > 0 ? urlRaw : '(none)';
      const kind = MemoryPayloadBuilder.safeString(c.kind);
      const reasonRaw = MemoryPayloadBuilder.safeString(c.reason);
      const reason = reasonRaw.length > 200 ? reasonRaw.substring(0, 200) + '...' : reasonRaw;
      const noteRaw = MemoryPayloadBuilder.safeString(c.takeNote);
      const note = noteRaw.length > 200 ? noteRaw.substring(0, 200) + '...' : noteRaw;
      lines.push(`[${i + 1}] kind=${kind} | url=${url} | rawAction=${rawAction}`);
      if (reason.length > 0) {
        lines.push(`    reason: ${reason}`);
      }
      if (note.length > 0 && note.toLowerCase() !== 'none') {
        lines.push(`    takeNote: ${note}`);
      }
      const toolCallJson = MemoryPayloadBuilder.safeString(c.toolCallJson);
      if (toolCallJson.length > 0 && toolCallJson.toLowerCase() !== 'none') {
        const tcl = toolCallJson.length > 200 ? toolCallJson.substring(0, 200) + '...' : toolCallJson;
        lines.push(`    toolCall: ${tcl}`);
      }
    }
    const digest = lines.join('\n');
    return digest.length > MAX_DIGEST_LEN ? digest.substring(0, MAX_DIGEST_LEN) + '...' : digest;
  }

  // -------------------- fallback (rule-only) --------------------

  private static fallbackAbstract(state: AgentState, clustered: ClusteredStep[]): MemoryAbstractedTrajectory {
    const trajectory = new MemoryAbstractedTrajectory();
    const planLines: string[] = [];
    planLines.push('## 任务拆解 (rule fallback)');
    const task = MemoryPayloadBuilder.safeString(state.task);
    if (task.length > 0) {
      planLines.push(`目标: ${MemoryPayloadBuilder.escapeHtml(task)}`);
    }
    planLines.push(`步骤数 (聚合后): ${clustered.length}`);
    trajectory.plan = planLines.join('\n');

    const steps: MemoryAbstractedStep[] = [];
    const site = MemoryPayloadBuilder.extractSite(state, clustered);
    for (let i = 0; i < clustered.length; i++) {
      const c = clustered[i];
      const step = new MemoryAbstractedStep();
      step.name = MemoryPayloadBuilder.normalizeStepName(MemoryPayloadBuilder.safeString(c.kind));
      step.reason = MemoryPayloadBuilder.truncate(MemoryPayloadBuilder.safeString(c.reason), MAX_REASON_LEN);
      const args: Record<string, Object> = {};
      if (site.length > 0) {
        args.site = site;
      }
      const rawActions = MemoryPayloadBuilder.safeStringArray(c.rawActions);
      const value = MemoryPayloadBuilder.extractValueFromRawActions(rawActions);
      if (value.length > 0) {
        args.value = value;
      }
      step.args = args;
      step.result = MemoryPayloadBuilder.truncate(MemoryPayloadBuilder.safeString(c.takeNote), MAX_RESULT_LEN);
      const concreteHint: Record<string, Object> = {};
      if (rawActions.length > 0) {
        concreteHint.raw_action = rawActions.join(' ; ');
      }
      const url = MemoryPayloadBuilder.safeString(c.url);
      if (url.length > 0) {
        concreteHint.url_at_step = url;
      }
      step.argsConcreteHint = concreteHint;
      steps.push(step);
    }
    trajectory.steps = steps;
    return trajectory;
  }

  private static attachConcreteHints(abstracted: MemoryAbstractedTrajectory, clustered: ClusteredStep[]): void {
    const len = Math.min(abstracted.steps.length, clustered.length);
    for (let i = 0; i < len; i++) {
      const c = clustered[i];
      const hint: Record<string, Object> = {};
      const rawActions = MemoryPayloadBuilder.safeStringArray(c.rawActions);
      if (rawActions.length > 0) {
        hint.raw_action = rawActions.join(' ; ');
      }
      const url = MemoryPayloadBuilder.safeString(c.url);
      if (url.length > 0) {
        hint.url_at_step = url;
      }
      const toolCallJson = MemoryPayloadBuilder.safeString(c.toolCallJson);
      if (toolCallJson.length > 0 && toolCallJson.toLowerCase() !== 'none') {
        hint.tool_call_json = toolCallJson;
      }
      abstracted.steps[i].argsConcreteHint = hint;
    }
  }

  // -------------------- assembleMessages --------------------

  static assembleMessages(state: AgentState, abstracted: MemoryAbstractedTrajectory): MemoryMessagePayload[] {
    const messages: MemoryMessagePayload[] = [];

    const userMsg = new MemoryMessagePayload();
    userMsg.role = 'user';
    userMsg.content = state.task;
    messages.push(userMsg);

    if (abstracted.plan.length > 0) {
      const planMsg = new MemoryMessagePayload();
      planMsg.role = 'assistant';
      planMsg.content = MemoryPayloadBuilder.truncate(abstracted.plan, MAX_PLAN_LEN);
      messages.push(planMsg);
    }

    for (let i = 0; i < abstracted.steps.length; i++) {
      const step = abstracted.steps[i];
      if (step.reason.length > 0) {
        const reasonMsg = new MemoryMessagePayload();
        reasonMsg.role = 'assistant';
        reasonMsg.content = MemoryPayloadBuilder.truncate(step.reason, MAX_REASON_LEN);
        messages.push(reasonMsg);
      }
      const toolMsg = new MemoryMessagePayload();
      toolMsg.role = 'tool';
      const toolEntry = new MemoryToolEntry();
      toolEntry.name = step.name;
      toolEntry.args = step.args;
      toolEntry.argsConcreteHint = step.argsConcreteHint;
      toolEntry.result = MemoryPayloadBuilder.truncate(step.result, MAX_RESULT_LEN);
      toolMsg.content = [toolEntry];
      messages.push(toolMsg);
    }

    const finalMsg = new MemoryMessagePayload();
    finalMsg.role = 'assistant';
    const answerLines: string[] = [];
    answerLines.push(state.result.success ? '## 最终回答' : '## 失败原因');
    const answerText = state.result.answer.length > 0 ?
      state.result.answer :
      (state.result.message.length > 0 ? state.result.message : '任务未产出回答');
    answerLines.push(answerText);
    if (state.result.reference.length > 0) {
      answerLines.push('## 参考');
      answerLines.push(state.result.reference);
    }
    finalMsg.content = MemoryPayloadBuilder.truncate(answerLines.join('\n'), MAX_FINAL_LEN);
    messages.push(finalMsg);

    return messages;
  }

  // -------------------- buildToolCalls (top-level index) --------------------

  static buildToolCalls(abstracted: MemoryAbstractedTrajectory): MemoryToolCallPayload[] {
    const out: MemoryToolCallPayload[] = [];
    const seen: Set<string> = new Set();
    for (let i = 0; i < abstracted.steps.length; i++) {
      const step = abstracted.steps[i];
      if (step.name.length === 0 || seen.has(step.name)) {
        continue;
      }
      seen.add(step.name);
      const payload = new MemoryToolCallPayload();
      payload.tool = step.name;
      payload.name = step.name;
      try {
        payload.argsJson = JSON.stringify(step.args);
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'MemoryPayloadBuilder build tool calls error: %{public}s',  e.message);
        payload.argsJson = '{}';
      }
      try {
        payload.argsConcreteHintJson = JSON.stringify(step.argsConcreteHint);
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'Memory buildToolCalls failed: %{public}s',  e.message);
        payload.argsConcreteHintJson = '{}';
      }
      payload.result = step.result;
      out.push(payload);
    }
    return out;
  }

  // -------------------- buildMetadata --------------------

  static buildMetadata(state: AgentState, abstracted: MemoryAbstractedTrajectory): Record<string, string> {
    const metadata: Record<string, string> = {};
    metadata.task = state.task;
    metadata.success = state.result.success ? 'true' : 'false';
    metadata.website = state.observation ? state.observation.currentUrl : '';
    metadata.workflow_step_count = `${abstracted.steps.length}`;
    metadata.source = 'browser_agent_runtime';
    if (state.observation && state.observation.siteId.length > 0) {
      metadata.siteId = state.observation.siteId;
    }
    return metadata;
  }

  // -------------------- helpers --------------------

  private static parseJsonRecord(text: string): Record<string, Object> | null {
    try {
      const parsed = JSON.parse(text) as Object;
      if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) {
        return null;
      }
      return parsed as Record<string, Object>;
    } catch (error) {
      hilog.warn(DOMAIN, TAG, 'parse abstraction JSON failed: %{public}s',
        error instanceof Error ? error.message : `${error}`);
      return null;
    }
  }

  private static safeString(value: Object | string | null | undefined): string {
    return typeof value === 'string' ? value as string : '';
  }

  private static safeStringArray(values: string[] | null | undefined): string[] {
    const out: string[] = [];
    if (!Array.isArray(values)) {
      return out;
    }
    for (let i = 0; i < values.length; i++) {
      const value = MemoryPayloadBuilder.safeString(values[i]);
      if (value.length > 0) {
        out.push(value);
      }
    }
    return out;
  }

  private static escapeHtml(text: string): string {
    return text
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  }

  private static readVerb(actionText: string): string {
    const trimmed = MemoryPayloadBuilder.safeString(actionText).trim();
    if (trimmed.length === 0) {
      return '';
    }
    const segments = trimmed.split(/\s+/);
    return segments.length > 0 ? segments[0] : '';
  }

  private static isToolCallEntry(entry: AgentHistoryEntry): boolean {
    const verb = MemoryPayloadBuilder.readVerb(entry.action);
    if (verb.length > 0 && verb !== 'None' && verb !== 'none') {
      return false;
    }
    const tc = MemoryPayloadBuilder.safeString(entry.toolCall).trim();
    return tc.length > 0 && tc.toLowerCase() !== 'none';
  }

  private static isDroppableVerb(verb: string, entry: AgentHistoryEntry): boolean {
    if (verb === '' || verb === 'None' || verb === 'none') {
      const toolCall = MemoryPayloadBuilder.safeString(entry.toolCall).trim();
      return toolCall.length === 0 || toolCall.toLowerCase() === 'none';
    }
    if (verb === 'WAIT' || verb === 'SCREENSHOT' || verb === 'AGENT_CONTROL' ||
        verb === 'CALCULATE' || verb === 'GET_HREF' || verb === 'TAB_NEW' ||
        verb === 'TAB_CLOSE' || verb === 'ARROW') {
      return true;
    }
    if (verb === 'GO_BACK') {
      return !MemoryPayloadBuilder.isMeaningfulText(entry.takeNote);
    }
    return false;
  }

  private static mapVerb(verb: string, entry: AgentHistoryEntry): string {
    if (verb === 'CLICK' || verb === 'HOVER') {
      return 'pick_item';
    }
    if (verb === 'FILL') {
      return 'fill_field';
    }
    if (verb === 'SEARCH' || verb === 'SEARCH_BY_QUERY') {
      return 'search_query';
    }
    if (verb === 'SELECT') {
      return 'select_option';
    }
    if (verb === 'PRESS') {
      return '';
    }
    if (verb === 'USER_CONTROL') {
      return 'request_user_takeover';
    }
    if (verb === 'FINISH') {
      return 'finish';
    }
    if (verb === 'DISPATCH') {
      return 'call_site_tool';
    }
    if (verb === 'GO_BACK') {
      return MemoryPayloadBuilder.isMeaningfulText(entry.takeNote) ? 'extract_detail' : '';
    }
    return '';
  }

  private static normalizeStepName(raw: string): string {
    const lowered = raw.trim().toLowerCase();
    for (let i = 0; i < VERB_VOCABULARY.length; i++) {
      if (lowered === VERB_VOCABULARY[i]) {
        return VERB_VOCABULARY[i];
      }
    }
    return 'pick_item';
  }

  private static sanitizeArgs(args: Record<string, Object>): Record<string, Object> {
    const out: Record<string, Object> = {};
    const allowedKeys: string[] = ['site', 'section', 'target_role', 'value', 'scope', 'position'];
    for (let i = 0; i < allowedKeys.length; i++) {
      const key = allowedKeys[i];
      const value = args[key];
      if (typeof value === 'string' && (value as string).length > 0) {
        out[key] = value;
      } else if (typeof value === 'number') {
        out[key] = value as Object;
      } else if (typeof value === 'boolean') {
        out[key] = value as Object;
      }
    }
    return out;
  }

  private static dedupConsecutive(clusters: ClusteredStep[]): ClusteredStep[] {
    if (clusters.length <= 1) {
      return clusters;
    }
    const out: ClusteredStep[] = [];
    out.push(clusters[0]);
    for (let i = 1; i < clusters.length; i++) {
      const prev = out[out.length - 1];
      const cur = clusters[i];
      const prevRawActions = MemoryPayloadBuilder.safeStringArray(prev.rawActions);
      const curRawActions = MemoryPayloadBuilder.safeStringArray(cur.rawActions);
      if (prev.kind === cur.kind && prev.url === cur.url &&
          prevRawActions.join('|') === curRawActions.join('|')) {
        continue;
      }
      out.push(cur);
    }
    return out;
  }

  private static capClusters(clusters: ClusteredStep[], cap: number): ClusteredStep[] {
    if (clusters.length <= cap) {
      return clusters;
    }
    const headSize = Math.max(1, Math.floor(cap * 0.4));
    const tailSize = Math.max(1, Math.floor(cap * 0.4));
    const head: ClusteredStep[] = clusters.slice(0, headSize);
    const tail: ClusteredStep[] = clusters.slice(clusters.length - tailSize);
    const middlePool: ClusteredStep[] = clusters.slice(headSize, clusters.length - tailSize);
    const middlePicked: ClusteredStep[] = [];
    const middleBudget = cap - head.length - tail.length;
    for (let i = 0; i < middlePool.length && middlePicked.length < middleBudget; i++) {
      if (MemoryPayloadBuilder.isMeaningfulText(middlePool[i].takeNote)) {
        middlePicked.push(middlePool[i]);
      }
    }
    const result: ClusteredStep[] = [];
    for (let i = 0; i < head.length; i++) {
      result.push(head[i]);
    }
    for (let i = 0; i < middlePicked.length; i++) {
      result.push(middlePicked[i]);
    }
    for (let i = 0; i < tail.length; i++) {
      result.push(tail[i]);
    }
    return result;
  }

  private static collectMeaningfulNotes(entries: AgentHistoryEntry[]): string[] {
    const notes: string[] = [];
    for (let i = 0; i < entries.length; i++) {
      if (MemoryPayloadBuilder.isMeaningfulText(entries[i].takeNote)) {
        notes.push(entries[i].takeNote);
      }
    }
    return notes;
  }

  private static mergeNotes(a: string, b: string): string {
    const aOk = MemoryPayloadBuilder.isMeaningfulText(a);
    const bOk = MemoryPayloadBuilder.isMeaningfulText(b);
    if (aOk && bOk) {
      return `${a} | ${b}`;
    }
    if (aOk) {
      return a;
    }
    if (bOk) {
      return b;
    }
    return '';
  }

  private static isMeaningfulText(value: string): boolean {
    const trimmed = MemoryPayloadBuilder.safeString(value).trim();
    if (trimmed.length === 0) {
      return false;
    }
    const lowered = trimmed.toLowerCase();
    return lowered !== 'none' && lowered !== 'no' && lowered !== 'null';
  }

  private static extractFirstBracket(actionText: string): string {
    const match = MemoryPayloadBuilder.safeString(actionText).match(/\[(.*?)\]/);
    return match && match.length > 1 ? match[1] : '';
  }

  private static extractValueFromRawActions(rawActions: string[]): string {
    for (let i = 0; i < rawActions.length; i++) {
      const rawAction = MemoryPayloadBuilder.safeString(rawActions[i]);
      const matches = rawAction.match(/\[(.*?)\]/g);
      if (!matches || matches.length < 2) {
        continue;
      }
      const second = matches[1];
      const stripped = second.substring(1, second.length - 1).trim();
      const asNum = Number(stripped);
      if (!Number.isNaN(asNum)) {
        continue;
      }
      if (stripped.length > 0) {
        return stripped;
      }
    }
    return '';
  }

  private static extractSite(state: AgentState, clustered: ClusteredStep[]): string {
    const url = state.observation ? state.observation.currentUrl : '';
    const fromCurrent = MemoryPayloadBuilder.hostFromUrl(url);
    if (fromCurrent.length > 0) {
      return fromCurrent;
    }
    for (let i = 0; i < clustered.length; i++) {
      const host = MemoryPayloadBuilder.hostFromUrl(clustered[i].url);
      if (host.length > 0) {
        return host;
      }
    }
    return '';
  }

  private static hostFromUrl(url: string): string {
    const normalizedUrl = MemoryPayloadBuilder.safeString(url);
    if (normalizedUrl.length === 0) {
      return '';
    }
    const match = normalizedUrl.match(/^https?:\/\/([^\/?#]+)/);
    return match && match.length > 1 ? match[1] : '';
  }

  private static extractJsonText(text: string): string {
    const normalizedText = MemoryPayloadBuilder.safeString(text);
    const start = normalizedText.indexOf('{');
    if (start < 0) {
      return normalizedText;
    }
    let depth = 0;
    for (let i = start; i < normalizedText.length; i++) {
      const ch = normalizedText.charAt(i);
      if (ch === '{') {
        depth += 1;
      } else if (ch === '}') {
        depth -= 1;
        if (depth === 0) {
          return normalizedText.substring(start, i + 1);
        }
      }
    }
    return normalizedText.substring(start);
  }

  private static truncate(text: string, max: number): string {
    const normalizedText = MemoryPayloadBuilder.safeString(text);
    if (normalizedText.length <= max) {
      return normalizedText;
    }
    if (max <= 3) {
      return normalizedText.substring(0, max);
    }
    return normalizedText.substring(0, max - 3) + '...';
  }
}
