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

import { AgentBuildConfig } from '../../core/AgentBuildConfig';
import { AgentState } from '../../core/AgentState';
import { AgentToolCall, AgentToolQuery, AgentToolResult, BrowserAgentAction } from '../../core/AgentTypes';
import { AgentSiteTool } from '../../tools/AgentSiteTool';
import { AgentToolContext } from '../../ports/AgentContext';
import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN = 0x6020;
const TAG = 'BrowserAgentGraph';

export class WeiboAdvancedSearchArgs {
  start_time: string = '';
  end_time: string = '';
  keyword: string = '';
  followed: string = '';
}

export class WeiboSearchToolService extends AgentSiteTool {
  static readonly TOOL_ID: string = 'advancedSearch';
  static readonly TOOL_NAME: string = 'advancedSearch';
  static readonly TOOL_DESCRIPTION: string =
    'Use this tool to search information in weibo. Format: "tool": {"toolId":"advancedSearch","parameters":{"start_time":"YYYY-MM-DD","end_time":"YYYY-MM-DD","keyword":"keyword", "followed": "True"/"False"}} and keep "action" as an empty list.' +
    'When followed is True, it shows only posts posted by those who the user follows.' +
    'If the user dont ask time period, set the start_time to 01-01 of the last year and the end_time to the current date.';
  readonly toolId: string = WeiboSearchToolService.TOOL_ID;
  readonly displayName: string = WeiboSearchToolService.TOOL_NAME;
  readonly description: string = WeiboSearchToolService.TOOL_DESCRIPTION;
  readonly domain: string = 'weibo';

  supports(query: AgentToolQuery): boolean {
    return this.shouldEnable(query);
  }

  async invoke(call: AgentToolCall, state: AgentState, context: AgentToolContext): Promise<AgentToolResult> {
    void state;
    const nextResult = new AgentToolResult();
    nextResult.handled = true;
    const baseUrl = context.urlConfig.getSiteUrl('weibo_search');
    const url = WeiboSearchToolService.buildAdvancedSearchUrl(call.inputText, baseUrl);
    hilog.debug(DOMAIN, TAG, 'node execute tool advanced search url = %{private}s', url);
    
    if (url.length <= 0) {
      nextResult.success = false;
      nextResult.summary = 'Failed: advancedSearch requires valid start_time, end_time, keyword and followed.';
      return nextResult;
    }

    try {
      const action = this.buildNavigationAction(url);
      hilog.debug(DOMAIN, TAG, 'node execute tool action = %{private}s', action.action);
      await context.env.execute(action);
      if (action.requiresLoadingWait) {
        await context.env.waitForPageReady(url);
      }
    } catch (error) {
      nextResult.success = false;
      nextResult.summary = `advancedSearch navigation failed: ${error.message}`;
      return nextResult;
    }

    nextResult.success = true;
    nextResult.summary = `Navigated to Weibo advanced search: ${url}`;
    nextResult.resultPayloadText = url;
    return nextResult;
  }

  static buildAdvancedSearchUrl(inputText: string, baseUrl: string): string {
    const args = WeiboSearchToolService.parseArgs(inputText);
    const keyword = WeiboSearchToolService.normalizeKeyword(args.keyword);
    if (args.start_time.length <= 0 || args.end_time.length <= 0 || keyword.length <= 0) {
      return '';
    }
    const timescope = encodeURIComponent(`custom:${args.start_time}:${args.end_time}`);
    const url = baseUrl + '/weibo?q=' +
      WeiboSearchToolService.quotePlus(keyword) +
      '&atten=' + WeiboSearchToolService.toAttenValue(args.followed) +
      '&suball=1&timescope=' + timescope +
      '&Refer=g';
    if (!WeiboSearchToolService.isAllowedWeiboUrl(url)) {
      hilog.warn(DOMAIN, TAG, 'Weibo Search built url rejected by allowlist: %{private}s', url);
      return '';
    }
    return url;
  }

  private static readonly ALLOWED_WEIBO_HOST_SUFFIXES: string[] = ['weibo.com'];

  private static isAllowedWeiboUrl(url: string): boolean {
    if (url.length === 0) {
      return false;
    }
    const lower = url.toLowerCase();
    if (!lower.startsWith('http://') && !lower.startsWith('https://')) {
      return false;
    }
    const schemeEnd = lower.indexOf('://');
    const hostPart = url.substring(schemeEnd + 3);
    const hostEnd = hostPart.search(/[/?#]/);
    const hostWithPort = (hostEnd >= 0 ? hostPart.substring(0, hostEnd) : hostPart).toLowerCase();
    const portIndex = hostWithPort.indexOf(':');
    const host = portIndex >= 0 ? hostWithPort.substring(0, portIndex) : hostWithPort;
    if (host.length === 0) {
      return false;
    }
    return WeiboSearchToolService.ALLOWED_WEIBO_HOST_SUFFIXES.some(suffix =>
      host === suffix || host.endsWith('.' + suffix)
    );
  }

  private static parseArgs(inputText: string): WeiboAdvancedSearchArgs {
    const args = new WeiboAdvancedSearchArgs();
    if (inputText.trim().length <= 0) {
      return args;
    }
    try {
      const parsed = JSON.parse(inputText) as Record<string, Object>;
      args.start_time = WeiboSearchToolService.readString(parsed, 'start_time');
      args.end_time = WeiboSearchToolService.readString(parsed, 'end_time');
      args.keyword = WeiboSearchToolService.readString(parsed, 'keyword');
      args.followed = WeiboSearchToolService.readString(parsed, 'followed');
      if (args.followed !== 'True' && args.followed !== 'False') {
        throw new Error('The "followed" field must be a string of either "True" or "False"');
      }
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'Weibo Search parse args error: %{private}s', e.message);
      return args;
    }
    return args;
  }

  private static normalizeKeyword(keyword: string): string {
    return keyword.trim();
  }

  private static toUnixSeconds(dateText: string): number {
    const timestamp = Date.parse(`${dateText}T00:00:00Z`);
    return Number.isNaN(timestamp) ? 0 : Math.floor(timestamp / 1000);
  }

  private static quotePlus(value: string): string {
    return encodeURIComponent(value).replace(/%20/g, '+');
  }

  private static toAttenValue(value: string): string {
    const normalized = value.trim().toLowerCase();
    return normalized === 'true' ? '1' : '0';
  }

  private static readString(source: Record<string, Object>, key: string): string {
    const value = source[key];
    return typeof value === 'string' ? value : '';
  }

  private shouldEnable(query: AgentToolQuery): boolean {
    const currentUrl = query.currentUrl.trim().toLowerCase();
    const siteId = query.siteId.trim().toLowerCase();
    return siteId === 'weibo' ||
      currentUrl.indexOf('weibo.com') >= 0 ||
      currentUrl.indexOf('s.weibo.com') >= 0;
  }

  private buildNavigationAction(url: string): BrowserAgentAction {
    const action = new BrowserAgentAction();
    action.action = `NAVIGATION [${url}]`;
    action.actionType = 'NAVIGATION';
    action.params = [url];
    action.requiresLoadingWait = true;
    return action;
  }
}
