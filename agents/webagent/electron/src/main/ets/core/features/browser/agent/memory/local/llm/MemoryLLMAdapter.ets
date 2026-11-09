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

// 记忆模块对 LLM 的统一入口。
// 持有当前任务注入的 AgentLLMPort，把 systemPrompt+userPrompt 包成
// AgentLLMRequest，强制 json_object 输出与 browser_decision 场景，并对返回文本做 ```json 包裹剥离。

import { AgentLLMMessage, AgentLLMRequest } from '../../../core/AgentTypes';
import { AgentLLMPort } from '../../../ports/AgentLLMPort';
import { AgentLLMScenes } from '../../../core/AgentLLMScenes';
import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN = 0x6020;
const TAG = 'MemoryLLMAdapter';

export class MemoryLLMResponse {
  public ok: boolean = false;
  public rawText: string = '';
  public json: Record<string, Object> | null = null;
  public errorMessage: string = '';
}

export class MemoryLLMAdapter {
  private llm: AgentLLMPort;

  constructor(llm: AgentLLMPort) {
    this.llm = llm;
  }

  // 强制 json_object 模式调 LLM，并解析为 dict；失败时 ok=false。
  async runJson(systemPrompt: string, userPrompt: string,
    requestLabel: string = ''): Promise<MemoryLLMResponse> {
    const request = new AgentLLMRequest();
    request.systemPrompt = systemPrompt;
    request.userPrompt = userPrompt;
    request.scene = AgentLLMScenes.BROWSER_DECISION;
    request.responseFormat = 'json_object';
    const msg = new AgentLLMMessage();
    msg.role = 'user';
    msg.content = userPrompt;
    request.messages = [msg];

    const out = new MemoryLLMResponse();
    try {
      const resp = await this.llm.complete(request);
      const rawText = resp.text.length > 0 ? resp.text : resp.rawText;
      out.rawText = rawText;
      const text = MemoryLLMAdapter.removeCodeBlocks(rawText);
      const parsed: Object = JSON.parse(text) as Object;
      if (typeof parsed === 'object' && parsed !== null && !Array.isArray(parsed)) {
        out.ok = true;
        out.json = parsed as Record<string, Object>;
      } else {
        out.ok = false;
        out.errorMessage = 'LLM response is not a JSON object';
        hilog.warn(DOMAIN, TAG,
          'runJson label=%{private}s parsed-not-object preview=%{private}s',
          requestLabel, MemoryLLMAdapter.preview(rawText));
      }
    } catch (error) {
      out.ok = false;
      out.errorMessage = error instanceof Error ? error.message : `${error}`;
      hilog.warn(DOMAIN, TAG, 'runJson failed label=%{private}s err=%{private}s preview=%{private}s',
        requestLabel, out.errorMessage, MemoryLLMAdapter.preview(out.rawText));
    }
    return out;
  }

  // 端 prompt 中常用 "{key}" 模板，做轻量替换。Python 那边用的是 .format(**kwargs)。
  static formatTemplate(template: string, vars: Record<string, string>): string {
    let out = template;
    const keys: string[] = Object.keys(vars);
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      const placeholder = `{${key}}`;
      // 全量替换，避免 String.replaceAll 在某些环境的兼容性
      out = out.split(placeholder).join(vars[key]);
    }
    return out;
  }

  // 与 Python remove_code_blocks 等价：剥离 ```[lang]\n...\n``` 包裹和 <think>...</think>。
  static removeCodeBlocks(content: string): string {
    if (!content || content.length <= 0) {
      return '';
    }
    const trimmed = content.trim();
    const fenceMatch = trimmed.match(/^```[a-zA-Z0-9]*\n([\s\S]*?)\n```$/);
    let body = fenceMatch ? fenceMatch[1].trim() : trimmed;
    body = body.replace(/<think>[\s\S]*?<\/think>/g, '').trim();
    return body;
  }

  private static preview(text: string): string {
    if (!text) {
      return '';
    }
    const single = text.replace(/\n/g, '\\n');
    return single.length > 200 ? single.substring(0, 200) : single;
  }
}
