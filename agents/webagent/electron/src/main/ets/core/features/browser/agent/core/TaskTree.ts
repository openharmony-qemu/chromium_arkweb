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

import { AgentBuildConfig } from './AgentBuildConfig';
import { hilog } from '@kit.PerformanceAnalysisKit';
import _ from 'lodash';
const DOMAIN = 0x6020;
const TAG = 'BrowserAgentGraph';

export const about_task_tree = '';

export enum Status {
  current = 'current',
  running = 'running',
  completed = 'completed',
  pending = 'pending',
  killed = 'killed',
}

export interface TaskAction {
  timeStep?: string;
  globalStep?: number;
  url?: string;
  reason?: string;
  action?: string;
  time?: number;
  input_token?: number;
  output_token?: number;
  trajectory?: TaskAction[];
  [key: string]: unknown;
}

export interface FlattenedTaskAction {
  global_step?: number;
  url?: string;
  subtask: string;
  reason?: string;
  action?: string;
  time?: number;
  input_token?: number;
  output_token?: number;
}

export type FlattenedEntry = FlattenedTaskAction | TaskAction;

export interface TaskNode {
  step: string;
  task: string;
  status: Status | string;
  actions: TaskAction[];
  subtasks?: TaskNode[];
  [key: string]: unknown;
}

interface HiddenTaskNode extends Omit<TaskNode, 'actions' | 'subtasks'> {
  actions?: TaskAction[];
  subtasks?: HiddenTaskNode[] | string;
}

export function deepClone<T>(value: T): T {
  if (typeof globalThis.structuredClone === 'function') {
    return globalThis.structuredClone(value);
  }
  return _.cloneDeep(value);
}

export function get_task(task: Pick<TaskNode, 'task'>): string {
  return task.task;
}

export class TaskTree {
  [key: string]: unknown;

  public constructor(instruction: string | TaskNode | TaskTree) {
    if (typeof instruction === 'string') {
      this.step = 'root';
      this.task = instruction;
      this.status = Status.current;
      this.actions = [];
      return;
    }

    Object.assign(this, deepClone(instruction));
  }

  public toString(): string {
    return JSON.stringify(this);
  }

  public hide(task: TaskNode): string {
    const hidden = deepClone(task) as HiddenTaskNode;
    this.hide_completed(hidden);
    return JSON.stringify(hidden);
  }

  public finished(): boolean {
    return this.status === Status.completed;
  }

  public get_current(): TaskNode | Error {
    return this.search(this.asNode());
  }

  public get_current_task(): string | Error {
    const temp = this.search(this.asNode());
    if (temp instanceof Error) {
      return temp;
    }
    return temp.task;
  }

  public get_current_father(): TaskNode | null | Error {
    if (!('subtasks' in this) || !Array.isArray(this.subtasks)) {
      return null;
    }
    return this.search2(this.asNode());
  }

  public get_current_actions(): Array<{
    TimeStep: string | undefined;
    reason: unknown;
    action: unknown;
  }> | Error {
    const current = this.search(this.asNode());
    if (current instanceof Error) {
      return current;
    }
    return current.actions.map((item) => ({
      TimeStep: item.timeStep,
      reason: item.reason,
      action: item.action,
    }));
  }

  public step_forward(): void {
    this._step_forward(this.asNode());
  }

  public take_action(action: TaskAction): null | Error {
    const current = this.get_current();
    if (current instanceof Error) {
      return current;
    }
    current.actions.push(action);
    return null;
  }

  public break_down_current(new_tasks: string[]): Error | null {
    const current = this.get_current();
    if (current instanceof Error) {
      return current;
    }
    current.subtasks = [];
    const stepPrefix = this._get_step_prefix(current);
    new_tasks.forEach((task, index) => {
      current.subtasks!.push({
        step: `${stepPrefix}${index + 1}`,
        task,
        status: index === 0 ? Status.current : Status.pending,
        actions: [],
      });
    });
    current.status = Status.running;
    return null;
  }

  public replan(new_tasks: string[]): Error | null {
    const father = this.get_current_father();
    const current = this.get_current();
    if (father instanceof Error) {
      return father;
    } else if (current instanceof Error) {
      return current;
    }
    if (!father || !father.subtasks) {
      return null;
    }

    const nextSubtasks: TaskNode[] = [];
    for (const item of father.subtasks) {
      if (item.status === Status.completed || item.status === Status.killed) {
        nextSubtasks.push(deepClone(item));
      }
    }

    if (new_tasks.length > 0 && current.task === new_tasks[0]) {
      nextSubtasks.push(deepClone(current));
      new_tasks = new_tasks.slice(1);
    } else if (current.actions.length > 0) {
      current.status = Status.killed;
      nextSubtasks.push(deepClone(current));
    }

    const keptCount = nextSubtasks.length;
    const stepPrefix = this._get_step_prefix(father);
    new_tasks.forEach((task, index) => {
      nextSubtasks.push({
        step: `${stepPrefix}${index + 1 + keptCount}`,
        task,
        status: index === 0 ? Status.current : Status.pending,
        actions: [],
      });
    });

    if (nextSubtasks.length === 1) {
      return null;
    }

    father.subtasks = nextSubtasks;
    if (new_tasks.length === 0) {
      if (father.subtasks.length > 0) {
        father.subtasks[father.subtasks.length - 1].status = Status.current;
      } else {
        delete father.subtasks;
        father.status = Status.current;
      }
      this.step_forward();
    }
    return null;
  }

  public flatten(): FlattenedEntry[] {
    return this._flatten(this.asNode());
  }

  private asNode(): TaskNode {
    return this as unknown as TaskNode;
  }

  private _get_step_prefix(task: TaskNode): string {
    if (task.step === 'root') {
      return '';
    }
    return `${task.step}.`;
  }

  private _step_forward(task: TaskNode): void {
    if (task.status === Status.current) {
      if ((task.subtasks ?? []).length !== 0) {
        hilog.error(DOMAIN, TAG, 'Tasktree step forward failed: Current task should not have subtasks.');
        return;
      }
      task.status = Status.completed;
      return;
    }

    if (!task.subtasks) {
      return;
    }

    for (let index = 0; index < task.subtasks.length; index += 1) {
      const item = task.subtasks[index];
      if (item.status !== Status.running && item.status !== Status.current) {
        continue;
      }
      this._step_forward(item);
      const updatedItem = task.subtasks[index];
      if (updatedItem.status === Status.completed) {
        if (index === task.subtasks.length - 1) {
          task.status = Status.completed;
        } else {
          this.start_first_task(task.subtasks[index + 1]);
        }
      }
      return;

    }
  }

  private start_first_task(task: TaskNode): void {
    if (!task.subtasks) {
      task.status = Status.current;
      return;
    }
    task.status = Status.running;
    this.start_first_task(task.subtasks[0]);
  }

  private search(task: TaskNode): TaskNode | Error {
    if (task.status === Status.current) {
      if ((task.subtasks ?? []).length !== 0) {
        return new Error(`Tasktree search failed: Current task should not have subtasks. Got ${task.subtasks}`);
      }
      return task;
    }

    if (!task.subtasks) {
      return new Error('Tasktree search failed: No current task found.');
    }

    for (const item of task.subtasks) {
      if (item.status === Status.running || item.status === Status.current) {
        return this.search(item);
      }
    }
    return new Error('Tasktree search failed: No running or current task found.');
  }

  private search2(task: TaskNode): TaskNode | Error {
    if (!task.subtasks) {
      return new Error('No current father found.');
    }

    for (const item of task.subtasks) {
      if (item.status === Status.current) {
        return task;
      }
      if (item.status === Status.running) {
        return this.search2(item);
      }
    }

    return new Error('No current father found.');
  }

  private hide_completed(hidden: HiddenTaskNode): void {
    delete hidden.actions;

    if (!hidden.subtasks || typeof hidden.subtasks === 'string') {
      return;
    }

    for (const subtask of hidden.subtasks) {
      delete subtask.actions;
      if (subtask.status === Status.completed && subtask.subtasks) {
        subtask.subtasks = 'hided as completed';
      } else if (subtask.status === Status.running) {
        this.hide_completed(subtask);
      }
    }
  }

  private _flatten(task: TaskNode): FlattenedEntry[] {
    const result: FlattenedEntry[] = [];
    if (AgentBuildConfig.DEBUG) {
      hilog.debug(DOMAIN, TAG, 'node conclude task tree flatten, current node = %{public}s', task.task);
    } else {
      hilog.debug(DOMAIN, TAG, 'node conclude task tree flatten, current node = %{private}s', task.task);
    }
    for (const item of task.actions) {
      if (item.trajectory && Array.isArray(item.trajectory)) {
        result.push(...item.trajectory);
        continue;
      }

      result.push({
        global_step: item.globalStep,
        url: item.url as string | undefined,
        subtask: task.task,
        reason: item.reason as string | undefined,
        action: item.action as string | undefined,
        time: item.time as number | undefined,
        input_token: item.input_token as number | undefined,
        output_token: item.output_token as number | undefined,
      });
    }

    if (task.subtasks) {
      for (const subtask of task.subtasks) {
        result.push(...this._flatten(subtask));
      }
    }

    return result;
  }
}
