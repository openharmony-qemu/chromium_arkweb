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

// 这个文件负责 browser 功能 中 Rednote Page State Resolver Service 的核心服务。
import {
  BrowserPageState,
  BrowserPageStateResolver,
  BrowserRuntimeState
} from '../../../shared/domain/BrowserAutomationTypes';

export class RednotePageStateResolverService {
  public static readonly SITE_ID: string = 'rednote';
  public static readonly RESOLVER_ID: string = 'rednote_url';
  public static readonly STATE_HOME: string = 'home';
  public static readonly STATE_SEARCH_RESULT: string = 'search_result';
  public static readonly STATE_NOTE_DETAIL: string = 'note_detail';
  public static readonly STATE_UNKNOWN: string = 'unknown';
  private static readonly DOMAIN_PATTERN: string = 'xiaohongshu.com';

  static getResolver(): BrowserPageStateResolver {
    const resolver = new BrowserPageStateResolver();
    resolver.resolverId = RednotePageStateResolverService.RESOLVER_ID;
    resolver.siteId = RednotePageStateResolverService.SITE_ID;
    resolver.displayName = '小红书 URL 状态识别';
    return resolver;
  }

  static resolve(runtime: BrowserRuntimeState): BrowserPageState | null {
    if (!runtime.currentPerception) {
      return null;
    }
    const currentUrl = runtime.currentPerception.currentUrl;
    if (currentUrl.indexOf(RednotePageStateResolverService.DOMAIN_PATTERN) < 0) {
      return null;
    }

    if (currentUrl.indexOf('/search_result') >= 0) {
      return RednotePageStateResolverService.buildState(
        RednotePageStateResolverService.STATE_SEARCH_RESULT,
        '小红书搜索结果页',
        currentUrl
      );
    }
    if (currentUrl.indexOf('/discovery/item') >= 0 || currentUrl.indexOf('/explore/') >= 0) {
      return RednotePageStateResolverService.buildState(
        RednotePageStateResolverService.STATE_NOTE_DETAIL,
        '小红书笔记详情页',
        currentUrl
      );
    }
    if (currentUrl.indexOf('/explore') >= 0 || currentUrl.indexOf('xiaohongshu.com') >= 0) {
      return RednotePageStateResolverService.buildState(
        RednotePageStateResolverService.STATE_HOME,
        '小红书首页',
        currentUrl
      );
    }
    return RednotePageStateResolverService.buildState(
      RednotePageStateResolverService.STATE_UNKNOWN,
      '小红书未知页面',
      currentUrl
    );
  }

  private static buildState(stateId: string, displayName: string, currentUrl: string): BrowserPageState {
    const state = new BrowserPageState();
    state.siteId = RednotePageStateResolverService.SITE_ID;
    state.stateId = stateId;
    state.displayName = displayName;
    state.currentUrl = currentUrl;
    state.matchedBy = 'url';
    return state;
  }
}
