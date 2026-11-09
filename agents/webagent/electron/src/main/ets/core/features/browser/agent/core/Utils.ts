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
import { HtmlParser, Location } from './HtmlParser';
import {
  AgentDecision,
  AgentLLMRequest,
  AgentLLMResponse,
  AgentLiteDomNode,
  AgentObservation,
  AgentTabInfo,
  AgentToolCall,
  BrowserAgentAction,
  AgentConstants,
  AgentLiteDomRect,
  AgentViewport
} from './AgentTypes';
import { resolveActionNeedWait } from '../prompts/Actions';
import { BrowserAgentLiteDomSummaryService } from './LiteDomSummaryService';
import { BrowserAgentObservationService } from './ObservationService';
import {BrowserAgentGraphContext, AgentToolContext} from '../ports/AgentContext';
import { vConfig } from '../VersionConfig';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { JSON } from '@kit.ArkTS';
import json from '@ohos.util.json';
import { BrowserAgentUtf8TextService } from './Utf8TextService';
import Image from '@ohos.multimedia.image';
import util from '@ohos.util';
import { AgentState } from './AgentState';

const DOMAIN = 0x6020;
const TAG = 'BrowserAgentGraph';

interface HtmlParseState {
  observation: AgentObservation | null;
  treeText: string;
  liteTreeText: string;
}

export class BrowserAgentTryLLM<T> {
  private readonly completeFn: (request: AgentLLMRequest) => Promise<AgentLLMResponse>;
  private readonly verifyFn: (response: AgentLLMResponse) => T | Promise<T>;
  private readonly maxAttempts: number;

  constructor(
    completeFn: (request: AgentLLMRequest) => Promise<AgentLLMResponse>,
    verifyFn: (response: AgentLLMResponse) => T | Promise<T>,
    maxAttempts: number = 3
  ) {
    this.completeFn = completeFn;
    this.verifyFn = verifyFn;
    this.maxAttempts = maxAttempts;
  }

  async run(
    request: AgentLLMRequest,
    onRetry?: (request: AgentLLMRequest, response: AgentLLMResponse, errorMessage: string) => void
  ): Promise<T> {
    const errors: string[] = [];
    let attempts = 0;
    for (let i = 0; i < this.maxAttempts; i++) {
      attempts += 1;
      let response: AgentLLMResponse;
      try {
// 模型也包到try里面。有可能HTTP错误: 400 [provider=dashscope_vision] raw={"error":{"message":"<400> InternalError.Algo.DataInspectionFailed: Input image data
// may contain inappropriate content."
        response = await this.completeFn(request);
      } catch (error) {
        const errorMessage = BrowserAgentUtils.stringifyError(error as Object);
        if (errorMessage.includes('timeout')) {
          i += 0; // timeout留一个可改接口
        }
        errors.push(errorMessage);
        hilog.error(DOMAIN, TAG, 'call LLM error: %{public}s, %{private}s', error.type, error.message);
        continue;
      }
      try {
        return await this.verifyFn(response);
      } catch (error) {
        const errorMessage = BrowserAgentUtils.stringifyError(error as Object);
        errors.push(errorMessage);
        this.applyRetryNotice(request, response, errorMessage);
        if (onRetry) {
          onRetry(request, response, errorMessage);
        }
      }
    }
    throw new Error(`Call LLM error after ${attempts} attempts: ${errors.join(' | ')}`);
  }

  private applyRetryNotice(request: AgentLLMRequest, response: AgentLLMResponse, errorMessage: string): void {
    const rawOutput = response.text.length > 0 ? response.text : response.rawText;
    request.userPrompt += `

# Important Notice: You were previously called with the same input in this step and you generated an output that triggered an error.
You are now called for a second time, please output carefully to avoid the previous error.
Your previous output (note that this is NOT the **previous step**. It is your llm's previous output in this step. Do not confuse them):
${rawOutput}
Error message: ${errorMessage}
`;
    request.messages = [];
  }
}

export class BrowserAgentUtils {
  private static readonly OCR_CONTENT_MARKER: string = '[OCR_CONTENT]';
  private static readonly HILOG_MAX_CONTENT_LENGTH: number = 3000;
  private static readonly MAX_TREE_TEXT_LENGTH: number = 20000;
  private static readonly SUPPORTED_SENSITIVE_OPERATIONS: ReadonlySet<string> = new Set([
    'download', 'upload', 'delete', 'submit', 'login', 'privacy', 'permission', 'payment'
  ]);
  private static readonly SUPPORTED_ACTIONS: string[] = [
    'CLICK',
    'HOVER',
    'NAVIGATION',
    'NAVIGATE_TO',
    'GO_BACK',
    'TAB_NEW',
    'TAB_CLOSE',
    'TAB_FOCUS',
    'SCREENSHOT',
    'SEARCH',
    'SEARCH_BY_QUERY',
    'FILL',
    'SELECT_TEXT',
    'SCROLL',
    'ARROW',
    'PRESS',
    'SELECT',
    'UPLOAD',
    'DIALOG',
    'USER_CONTROL',
    'ASK_USER',
    'WAIT',
    'FINISH',
    'CALCULATE',
    'RICH_TEXT',
    'GET_HREF',
    'DISPATCH'
  ];

  static attachLlmScreenshotFileName(request: AgentLLMRequest, screenshotPath: string): void {
    const fileName = BrowserAgentUtils.extractFileName(screenshotPath);
    if (fileName.length <= 0) {
      request.imageFileNames = [];
      return;
    }
    request.imageFileNames = [fileName];
  }

  static collectCombinedLiteNodes(observation: AgentObservation): AgentLiteDomNode[] {
    const combined: AgentLiteDomNode[] = observation.liteDomTree.slice();
    if (!vConfig.outside_element) {
      return combined;
    }
    const outsideSorted = BrowserAgentObservationService.flattenOutsideDOMTreeSortedById(observation.outsideDOMTree);
    for (let i = 0; i < outsideSorted.length; i++) {
      combined.push(outsideSorted[i]);
    }
    return combined;
  }

  private static resolveOutsideDirection(direction: string): Location {
    if (direction === 'above') {
      return Location.above;
    }
    if (direction === 'below') {
      return Location.below;
    }
    if (direction === 'left') {
      return Location.left;
    }
    if (direction === 'right') {
      return Location.right;
    }
    return Location.inScreen;
  }

  static buildParser(observation: AgentObservation | null, html: string): HtmlParser {
    if (!observation) {
      const parser = new HtmlParser(html, true, [], ['a'], []);
      parser.setBaseUrl('');
      return parser;
    }
    const liteDomLength = observation.liteDomTree.length;
    const combinedTree = BrowserAgentUtils.collectCombinedLiteNodes(observation);
    const xpaths = BrowserAgentUtils.collectXpaths(combinedTree);
    const locations: Location[] = [];
    for (let i = 0; i < combinedTree.length; i++) {
      const xpath = BrowserAgentUtils.readXpath(combinedTree[i]);
      if (xpath.length <= 0) {
        continue;
      }
      let loc = Location.inScreen;
      if (i >= liteDomLength) {
        const dir = observation.outsideDOMTree.idDirectionMap.get(String(combinedTree[i].id));
        loc = BrowserAgentUtils.resolveOutsideDirection(dir);
      }
      locations.push(loc);
    }
    const parser = new HtmlParser(html, true, xpaths, ['a'], locations);
    parser.setBaseUrl(observation.currentUrl);
    BrowserAgentUtils.patchSelectTexts(observation.liteDomTree, parser);
    BrowserAgentUtils.patchSelectTexts(
      BrowserAgentObservationService.flattenOutsideDOMTree(observation.outsideDOMTree), parser);
    return parser;
  }

  private static buildLiteDomParts(observation: AgentObservation): [string, string, string, string][] {
    const liteDomLength = observation.liteDomTree.length;
    // 重建 combinedTree（patchSelectTexts 已填充 options，须在此之后取 parts）
    const combinedTreeForParts = BrowserAgentUtils.collectCombinedLiteNodes(observation);
    // litedomParts 索引须与 must_include 数组（collectXpaths 跳过空 xpath 后的序）对齐，
    // 这样 HtmlParser 中 node.must_include_id 才能正确反查到对应节点的 parts。
    // 第 4 项 kind 标识节点来源：liteDomTree -> 'liteDom'，outsideDOMTree -> '${dir}Dom'。
    const litedomParts: [string, string, string, string][] = [];
    for (let i = 0; i < combinedTreeForParts.length; i++) {
      const liteDomNode = combinedTreeForParts[i];
      const xpath = BrowserAgentUtils.readXpath(liteDomNode);
      if (xpath.length <= 0) {
        continue;
      }
      let kind = 'liteDom';
      if (i >= liteDomLength) {
        const direction = observation.outsideDOMTree.idDirectionMap.get(String(liteDomNode.id));
        // kind = '${Location}Dom'（inScreen='lite' → 'liteDom'，above → 'aboveDom'…）
        kind = `${BrowserAgentUtils.resolveOutsideDirection(direction)}Dom`;
      }
      const parts = BrowserAgentLiteDomSummaryService.buildLiteDomSummaryParts(liteDomNode);
      litedomParts.push([parts[0], parts[1], parts[2], kind]);
    }
    return litedomParts;
  }

  static parseHtml(state: HtmlParseState): HtmlParser {
    const html = BrowserAgentUtils.resolveHtml(state);
    const observation = state.observation;
    const parser = BrowserAgentUtils.buildParser(observation, html);
    if (!observation) {
      let treeText = parser.printWholeTree({
        includeNoneText: true,
        includeHref: false,
        useIndent: false,
      });
      if (BrowserAgentUtf8TextService.exceeds(treeText, BrowserAgentUtils.MAX_TREE_TEXT_LENGTH)) {
        hilog.warn(DOMAIN, TAG, 'html text exceeds length limit %{public}d / %{public}d', treeText.length, BrowserAgentUtils.MAX_TREE_TEXT_LENGTH);
        treeText = BrowserAgentUtf8TextService.truncateToChars(treeText,
          BrowserAgentUtils.MAX_TREE_TEXT_LENGTH) + ' ...(More content folded)';
      }
      state.treeText = treeText;
      state.liteTreeText = '';
      return parser;
    }
    if (!observation.hasOpenDialog) {
      observation.liteDomSummary = BrowserAgentLiteDomSummaryService.buildLiteDomSummary(
        observation.liteDomTree, AgentConstants.LITEDOM_MAX_SUMMARY_LENGTH);
      // select 补丁同样作用于 outsideDOMTree，须同步重建以保持与 tree 一致
      observation.outsideDomSummary = BrowserAgentLiteDomSummaryService.buildOutsideDomSummary(
        observation.outsideDOMTree, AgentConstants.LITEDOM_MAX_SUMMARY_LENGTH,
        observation.viewport.width, observation.viewport.height);
    }
    // HtmlParser 整树哈希（root.hash 聚合子节点），用于 handleBacklog 判断 scroll 前后 html 是否变化
    observation.htmlHash = parser.root !== null ? parser.root.hash : 0;
    let treeText = parser.printWholeTree({
      includeNoneText: true,
      includeHref: false,
      useIndent: false,
    });
    if (BrowserAgentUtf8TextService.exceeds(treeText, BrowserAgentUtils.MAX_TREE_TEXT_LENGTH)) {
      hilog.warn(DOMAIN, TAG, 'html text exceeds length limit %{public}d / %{public}d', treeText.length, BrowserAgentUtils.MAX_TREE_TEXT_LENGTH);
      treeText = BrowserAgentUtf8TextService.truncateToChars(treeText,
        BrowserAgentUtils.MAX_TREE_TEXT_LENGTH) + ' ...(More content folded)';
    }
    state.treeText = treeText;
    const litedomParts = BrowserAgentUtils.buildLiteDomParts(observation);
    let liteTreeText = parser.printLiteDomTree(parser.root, -1, litedomParts).join('\n');
    if (BrowserAgentUtf8TextService.exceeds(liteTreeText, BrowserAgentUtils.MAX_TREE_TEXT_LENGTH)) {
      hilog.warn(DOMAIN, TAG, 'lite tree text exceeds length limit %{public}d / %{public}d', liteTreeText.length, BrowserAgentUtils.MAX_TREE_TEXT_LENGTH);
      liteTreeText = BrowserAgentUtf8TextService.truncateToChars(liteTreeText,
        BrowserAgentUtils.MAX_TREE_TEXT_LENGTH) + ' ...(More content folded)';
    }
    state.liteTreeText = liteTreeText;
    return parser;
  }

  static checkActionParameter(
    decision: AgentDecision,
    liteDomTree: AgentLiteDomNode[],
    parser: HtmlParser
  ): string {
    void parser;
    if (decision.toolCall && BrowserAgentUtils.hasToolCall(decision.toolCall)) {
      return '';
    }

    const action = decision.browserAction;
    if (!action || action.action.trim().length <= 0) {
      throw new Error(
        `Parse action "${action}" error! Error: Empty action while no tool is called.`
      );
    }

    const normalizedAction = action.action.trim();
    const functionName = BrowserAgentUtils.readFunctionName(normalizedAction);
    const matches = BrowserAgentUtils.extractBracketParams(normalizedAction);
    const firstParam = BrowserAgentUtils.getFirstParameter(normalizedAction, matches);

    if (!BrowserAgentUtils.SUPPORTED_ACTIONS.includes(functionName)) {
      throw new Error(
        `Parse action "${normalizedAction}" error! Error: Predicted action name not in action space!`
      );
    }

    if (functionName === 'DIALOG') {
      const dialogAction = firstParam.trim().toLowerCase();
      if (dialogAction !== 'accept' && dialogAction !== 'dismiss') {
        throw new Error(
          `Parse action "${normalizedAction}" error! DIALOG requires accept or dismiss as first parameter.`
        );
      }
      return '';
    }

    if (BrowserAgentUtils.isElementAction(functionName)) {
      if (BrowserAgentUtils.isCoordinateFormat(firstParam)) {
        if (functionName !== 'CLICK') {
          throw new Error(
            `Parse action "${normalizedAction}" error! Coordinate format is only supported for CLICK action.`
          );
        }
        return '';
      }
      if (functionName === 'FILL' || functionName === 'RICH_TEXT') {
        BrowserAgentUtils.checkFillAction(functionName, matches, normalizedAction, liteDomTree);
        return '';
      }
      BrowserAgentUtils.assertElementIdExists(firstParam, liteDomTree, normalizedAction);
      BrowserAgentUtils.checkClickAction(functionName, matches, normalizedAction, liteDomTree);
      BrowserAgentUtils.checkSearchAction(functionName, matches, normalizedAction);
      BrowserAgentUtils.checkSelectTextAction(functionName, matches, normalizedAction);
      BrowserAgentUtils.checkUploadAction(functionName, matches, normalizedAction);
      BrowserAgentUtils.checkSelectAction(functionName, normalizedAction, firstParam, matches, liteDomTree);
      return '';
    }

    if (functionName === 'GET_HREF') {
      return BrowserAgentUtils.collectHref(normalizedAction, firstParam, liteDomTree, parser);
    }

    BrowserAgentUtils.checkNavigationAction(functionName, firstParam, normalizedAction);
    BrowserAgentUtils.checkScrollAction(functionName, normalizedAction, matches, liteDomTree);
    BrowserAgentUtils.normalizeDispatchAction(functionName, action, normalizedAction, matches, liteDomTree);
    return '';
  }

  private static getFirstParameter(normalizedAction: string, matches: string[]): string {
    return normalizedAction.split(/\s+/).length > 1 && matches.length > 0 ? matches[0] : '';
  }

  static updateTabContext(browserTabs: AgentTabInfo[], tabs: AgentTabInfo[]): AgentTabInfo[] {
    const nextTabs = BrowserAgentUtils.cloneTabs(tabs);
    const current = BrowserAgentUtils.findCurrentTab(browserTabs);
    if (!current) {
      return nextTabs;
    }

    if (current.title.length > 100) {
      current.title = `${current.title.substring(0, 100)}...`;
    }

    if (nextTabs.length <= 0 || nextTabs[nextTabs.length - 1].title !== current.title) {
      if (nextTabs.length >= 2 && nextTabs[nextTabs.length - 2].title === current.title) {
        nextTabs.pop();
        nextTabs[nextTabs.length - 1].active = true;
      } else {
        if (nextTabs.length > 0) {
          nextTabs[nextTabs.length - 1].active = false;
        }
        nextTabs.push(current);
      }
      return nextTabs;
    }

    return nextTabs;
  }

  static stringifyError(error: Object): string {
    if (error instanceof Error) {
      return error.message;
    }
    return `${error}`;
  }

  static removeOcrContentMarker(context: string): string {
    const marker = BrowserAgentUtils.OCR_CONTENT_MARKER;
    let sanitizedContext = context.split(marker).join('');

    // The beginning of the captured text may contain only the tail of the marker.
    for (let fragmentLength = marker.length - 1; fragmentLength > 0; fragmentLength--) {
      const markerTail = marker.substring(marker.length - fragmentLength);
      if (sanitizedContext.startsWith(markerTail)) {
        sanitizedContext = sanitizedContext.substring(fragmentLength);
        break;
      }
    }

    // The end of the captured text may contain only the head of the marker.
    for (let fragmentLength = marker.length - 1; fragmentLength > 0; fragmentLength--) {
      const markerHead = marker.substring(0, fragmentLength);
      if (sanitizedContext.endsWith(markerHead)) {
        sanitizedContext = sanitizedContext.substring(0, sanitizedContext.length - fragmentLength);
        break;
      }
    }

    return sanitizedContext;
  }

  static async base64ToPixelMap(base64: string): Promise<Image.PixelMap> {
    const base64Helper = new util.Base64Helper();
    const base64Text = base64.startsWith('data:') ?
      base64.substring(base64.indexOf(',') + 1) :
      base64;
    const decoded = base64Helper.decodeSync(base64Text);
    const bytes = new Uint8Array(decoded.length);
    bytes.set(decoded);
    const buffer = bytes.buffer;
    const imageSource = Image.createImageSource(buffer);
    try {
      return await imageSource.createPixelMap({
        desiredPixelFormat: Image.PixelMapFormat.RGBA_8888
      });
    } finally {
      await imageSource.release();
    }
  }

  static async base64ToPixelMaps(base64List: string[]): Promise<Image.PixelMap[]> {
    const pixelMaps: Image.PixelMap[] = [];
    for (let i = 0; i < base64List.length; i++) {
      const pixelMap = await BrowserAgentUtils.base64ToPixelMap(base64List[i]);
      pixelMaps.push(pixelMap);
    }
    return pixelMaps;
  }

  static debugLongText(domain: number, tag: string, label: string, content: string, isDebug: boolean = false): void {
    const normalizedLabel = label.length > 0 ? label : 'hilog';
    const normalizedContent = content.length > 0 ? content : '';
    if (normalizedContent.length <= BrowserAgentUtils.HILOG_MAX_CONTENT_LENGTH) {
      hilog.debug(domain, tag, '%{private}s %{private}s', normalizedLabel, normalizedContent);
      return;
    }
    const totalParts = Math.ceil(normalizedContent.length / BrowserAgentUtils.HILOG_MAX_CONTENT_LENGTH);
    for (let i = 0; i < totalParts; i++) {
      const start = i * BrowserAgentUtils.HILOG_MAX_CONTENT_LENGTH;
      const end = Math.min(start + BrowserAgentUtils.HILOG_MAX_CONTENT_LENGTH, normalizedContent.length);
      const chunk = normalizedContent.substring(start, end);
      hilog.debug(domain, tag, '%{private}s [%{private}d/%{private}d] %{private}s',
        normalizedLabel, i + 1, totalParts, chunk);
    }
  }

  private static resolveHtml(state: HtmlParseState): string {
    return state.observation ? state.observation.html : '';
  }

  static shouldNavigate(currentUrl: string, entryUrl: string): boolean {
    if (entryUrl.length <= 0) {
      return false;
    }
    if (currentUrl.length <= 0) {
      return true;
    }
    const currentHost = BrowserAgentUtils.extractHost(currentUrl);
    const entryHost = BrowserAgentUtils.extractHost(entryUrl);
    if (currentHost.length > 0 && currentHost === entryHost) {
      return false;
    }
    return !currentUrl.startsWith(entryUrl);
  }

  private static extractHost(url: string): string {
    let normalized = url.trim().toLowerCase();
    if (normalized.startsWith('https://')) {
      normalized = normalized.substring(8);
    } else if (normalized.startsWith('http://')) {
      normalized = normalized.substring(7);
    }
    const slashIndex = normalized.indexOf('/');
    if (slashIndex >= 0) {
      normalized = normalized.substring(0, slashIndex);
    }
    const queryIndex = normalized.indexOf('?');
    if (queryIndex >= 0) {
      normalized = normalized.substring(0, queryIndex);
    }
    const hashIndex = normalized.indexOf('#');
    if (hashIndex >= 0) {
      normalized = normalized.substring(0, hashIndex);
    }
    return normalized;
  }

  private static findCurrentTab(browserTabs: AgentTabInfo[]): AgentTabInfo | null {
    let activeCount = 0;
    let current: AgentTabInfo | null = null;
    for (let i = 0; i < browserTabs.length; i++) {
      if (!browserTabs[i].active) {
        continue;
      }
      activeCount += 1;
      current = BrowserAgentUtils.cloneTab(browserTabs[i]);
    }
    if (activeCount !== 1) {
      return null;
    }
    return current;
  }

  private static cloneTabs(tabs: AgentTabInfo[]): AgentTabInfo[] {
    const nextTabs: AgentTabInfo[] = [];
    for (let i = 0; i < tabs.length; i++) {
      nextTabs.push(BrowserAgentUtils.cloneTab(tabs[i]));
    }
    return nextTabs;
  }

  private static cloneTab(source: AgentTabInfo): AgentTabInfo {
    const tab = new AgentTabInfo();
    tab.tabId = source.tabId;
    tab.windowId = source.windowId;
    tab.title = source.title;
    tab.url = source.url;
    tab.active = source.active;
    return tab;
  }

  private static patchSelectTexts(items: AgentLiteDomNode[], parser: HtmlParser): void {
    for (let i = 0; i < items.length; i++) {
      const tagName = items[i].element.tagName.trim().toLowerCase();
      if (tagName !== 'select') {
        continue;
      }
      items[i].element.tagName = 'select';
      const xpath = BrowserAgentUtils.readXpath(items[i]);
      if (xpath.length <= 0) {
        hilog.warn(DOMAIN, TAG, 'get xpath for select dom failed: %{private}d', i);
        continue;
      }
      items[i].element.optionValues = [];
      const node = parser.getNodeFromXpath(xpath);
      if (node === null) {
        hilog.warn(DOMAIN, TAG, 'get node for select dom failed: %{private}d', i);
        continue;
      }
      for (const child of node.children) {
        const value:string = child.attributes.value ?? '';
        if (value.length) {
          items[i].element.optionValues.push(value);
          const text = parser.getText(child).filter(s => s).join(' ').trim().replace(/\s+/g, ' ');
          items[i].element.options.push(text);
        }
      }
    }
  }

  static collectXpaths(items: AgentLiteDomNode[]): string[] {
    const xpaths: string[] = [];
    for (let i = 0; i < items.length; i++) {
      const xpath = BrowserAgentUtils.readXpath(items[i]);
      if (xpath.length > 0) {
        xpaths.push(xpath);
      }
    }
    return xpaths;
  }

  private static readXpath(node: AgentLiteDomNode): string {
    return typeof node.xpath === 'string' ? node.xpath : '';
  }

  private static hasToolCall(toolCall: AgentToolCall): boolean {
    const toolId = toolCall.toolId.trim().toLowerCase();
    const toolName = toolCall.toolName.trim().toLowerCase();
    return toolId.length > 0 || toolName.length > 0;
  }

  private static readFunctionName(actionText: string): string {
    const segments = actionText.split(/\s+/);
    return segments.length > 0 ? segments[0] : '';
  }

  static extractBracketParams(actionText: string): string[] {
    const normalizedAction = actionText.trim();
    const functionName = BrowserAgentUtils.readFunctionName(normalizedAction);
    if (functionName === 'FILL' || functionName === 'RICH_TEXT') {
      return BrowserAgentUtils.extractFillLikeBracketParams(normalizedAction);
    }
    return BrowserAgentUtils.extractBracketParamsRecursive(normalizedAction);
  }

  private static extractBracketParamsRecursive(text: string): string[] {
    const params: string[] = [];
    let depth = 0;
    let start = -1;
    for (let i = 0; i < text.length; i++) {
      if (text[i] === '[') {
        if (depth === 0) {
          start = i;
        }
        depth++;
      } else if (text[i] === ']' && depth > 0) {
        depth--;
        if (depth === 0 && start >= 0) {
          params.push(text.substring(start + 1, i));
          start = -1;
        }
      }
    }
    hilog.debug(DOMAIN, TAG, 'extracted brackets: %{private}s', JSON.stringify(params));
    return params;
  }

  /**
   * FILL/RICH_TEXT: [mode] [elementId] [value...] where value may contain ']' characters.
   * Value spans from the third '[' to the last ']' in the action string.
   */
  static extractFillLikeBracketParams(actionText: string): string[] {
    const trimmed = actionText.trim();
    const firstSpace = trimmed.indexOf(' ');
    if (firstSpace < 0) {
      return [];
    }
    let scanIndex = firstSpace + 1;
    while (scanIndex < trimmed.length && trimmed.charAt(scanIndex) === ' ') {
      scanIndex += 1;
    }

    const modeSegment = BrowserAgentUtils.readNextBracketSegment(trimmed, scanIndex);
    if (!modeSegment) {
      return [];
    }
    scanIndex = modeSegment.nextIndex;

    const elementSegment = BrowserAgentUtils.readNextBracketSegment(trimmed, scanIndex);
    if (!elementSegment) {
      return [modeSegment.value];
    }
    scanIndex = elementSegment.nextIndex;

    const valueOpen = trimmed.indexOf('[', scanIndex);
    if (valueOpen < 0) {
      return [modeSegment.value, elementSegment.value];
    }
    const valueClose = trimmed.lastIndexOf(']');
    if (valueClose <= valueOpen) {
      return [modeSegment.value, elementSegment.value];
    }

    let value = trimmed.substring(valueOpen + 1, valueClose);
    const trailing = trimmed.substring(valueClose + 1).trim();
    if (trailing.length > 0) {
      value = value.length > 0 ? `${value}${trailing}` : trailing;
    }
    return [modeSegment.value, elementSegment.value, value];
  }

  private static readNextBracketSegment(
    text: string,
    startIndex: number
  ): { value: string; nextIndex: number } | null {
    let index = startIndex;
    while (index < text.length && text.charAt(index) === ' ') {
      index += 1;
    }
    if (text.charAt(index) !== '[') {
      return null;
    }
    const close = text.indexOf(']', index + 1);
    if (close < 0) {
      return null;
    }
    return {
      value: text.substring(index + 1, close),
      nextIndex: close + 1
    };
  }

  private static isElementAction(functionName: string): boolean {
    return [
      'CLICK',
      'HOVER',
      'SEARCH',
      'FILL',
      'SELECT_TEXT',
      'SELECT',
      'UPLOAD',
      'SEARCH_BY_QUERY',
      'RICH_TEXT',
    ].includes(functionName);
  }

  private static isDirection(value: string): boolean {
    const normalized = value.toLowerCase();
    return normalized === 'up' || normalized === 'down' || normalized === 'left' || normalized === 'right';
  }

  private static assertElementIdExists(
    elementIdText: string,
    liteDomTree: AgentLiteDomNode[],
    actionText: string
  ): void {
    const elementIndex = Number(elementIdText);
    if (Number.isNaN(elementIndex) || elementIndex < 0 || elementIndex >= liteDomTree.length) {
      throw new Error(
        `Parse action "${actionText}" error! The Predicted elementID "${elementIdText}" does not exist! ` +
        'You must refer to an element in the LiteDOMTree or OutsideDOMTree. ' +
        'Off-screen elements are auto-scrolled into view before the action is re-derived on the next perception.'
      );
    }
  }

  /**
   * 从动作文本解析其目标 elementId。
   * 命中数字 elementId 返回该 id；非元素动作 / 坐标格式 / 无 id 返回 -1。
   * 与 checkActionParameter 内各 check* 助手的 id 提取保持一致：
   *   - CLICK+coordinate → -1
   *   - FILL / RICH_TEXT → matches[1]（matches[0] 是 mode）
   *   - 其余 element 动作 → matches[0]
   */
  static resolveActionTargetElementId(actionText: string): number {
    const normalized = actionText.trim();
    const functionName = BrowserAgentUtils.readFunctionName(normalized);
    if (!BrowserAgentUtils.isElementAction(functionName)) {
      return -1;
    }
    const matches = BrowserAgentUtils.extractBracketParams(normalized);
    const firstParam = matches.length > 0 ? matches[0] : '';
    if (BrowserAgentUtils.isCoordinateFormat(firstParam)) {
      return -1;
    }
    if (functionName === 'FILL' || functionName === 'RICH_TEXT') {
      if (matches.length < 2) {
        return -1;
      }
      const id = Number(matches[1]);
      return Number.isNaN(id) ? -1 : id;
    }
    if (matches.length < 1) {
      return -1;
    }
    const id = Number(matches[0]);
    return Number.isNaN(id) ? -1 : id;
  }

  private static checkSelectAction(
    functionName: string,
    actionText: string,
    firstParam: string,
    params: string[],
    liteDomTree: AgentLiteDomNode[]
  ): void {
    if (functionName !== 'SELECT') {
      return;
    }
    BrowserAgentUtils.assertElementIdExists(firstParam, liteDomTree, actionText);
    const elementIndex = Number(firstParam);
    if (liteDomTree[elementIndex].element.tagName !== 'select') {
      throw new Error(
        `Parse action "${actionText}" error! Predicted element is not a SELECT element. ` +
        'Use select only on elements with a SELECT tag.'
      );
    }
    const option = params.length > 1 ? params[1] : '';
    const options = BrowserAgentUtils.parseSelectOptions(liteDomTree[elementIndex]);
    hilog.debug(DOMAIN, TAG, 'node reason check action selection options = %{private}s, option = %{private}s', JSON.stringify(options), option);
    if (!options.includes(option)) {
      throw new Error(
        `Parse action "${actionText}" error! Predicted element does not have the option of "${option}". ` +
        'Please select a valid option or consider other actions.'
      );
    }
  }

  static getSelectAction(actionIn: BrowserAgentAction, params: string [], liteDom: AgentLiteDomNode[]): BrowserAgentAction {
    const element = liteDom[Number(params[0])].element;
    for (let i = 0; i < element.options.length; i++) {
      if (element.options[i] === params[1]) {
        const action = new BrowserAgentAction();
        action.action = `SELECT [${params[0]}] [${element.optionValues[i]}]`;
        action.actionType = 'SELECT';
        action.element = Number(params[0]);
        action.params = [element.optionValues[i]];
        action.requiresLoadingWait = actionIn.requiresLoadingWait;
        action.reason = actionIn.reason;
        return action;
      }
    }
    return actionIn; //不应该出现
  }
  static adaptSelectAction(actionIn: BrowserAgentAction, liteDom: AgentLiteDomNode[]): BrowserAgentAction {
    const functionName = BrowserAgentUtils.readFunctionName(actionIn.action);
    const params = BrowserAgentUtils.extractBracketParams(actionIn.action);
    if (functionName === 'SELECT') { //select需要把option text换成value
      return BrowserAgentUtils.getSelectAction(actionIn, params, liteDom);
    }
    if (functionName === 'SELECT_TEXT') {
      return BrowserAgentUtils.getSelectTextAction(actionIn, params);
    }
    return actionIn;
  }
  static getSelectTextAction(actionIn: BrowserAgentAction, params: string[]): BrowserAgentAction {
    const action = new BrowserAgentAction();
    action.action = actionIn.action;
    action.actionType = 'SELECT_TEXT';
    action.element = Number(params[0]);
    action.params = [params[1], params[2]];
    action.requiresLoadingWait = actionIn.requiresLoadingWait;
    action.reason = actionIn.reason;
    return action;
  }

  private static parseSelectOptions(node: AgentLiteDomNode): string[] {
    if (node.element.options && node.element.options.length > 0) {
      return node.element.options;
    }
    return [];
  }

  private static checkClickAction(functionName: string, matches: string[], normalizedAction: string, liteDomTree: AgentLiteDomNode[]): void {
    if (functionName !== 'CLICK') {
      return;
    }
    if (matches.length < 1) {
      throw new Error( `Parse action "${normalizedAction}" error! Missing target elementId. Please follow the action format strictly! Check if you have added the square bracket correctly.`);
    }
    if (liteDomTree[Number(matches[0])].type === 'scrollable') {
      throw new Error( `Parse action "${normalizedAction}" error! The target element ${matches[0]} is scrollable but not clickable. Try to scroll it to reveal the target you want.`);
    }
  }

  private static checkFillAction(functionName: string, matches: string[], normalizedAction: string, liteDomTree: AgentLiteDomNode[]): void {
    const fillMode = matches.length > 0 ? matches[0].toUpperCase() : '';
    if (!['REPLACE', 'APPEND', 'PREPEND', 'INPUT'].includes(fillMode)) {
      throw new Error(`Parse action "${normalizedAction}" error! ${functionName} requires INPUT, REPLACE, PREPEND or APPEND as first parameter!`);
    }
    if (matches.length < 2 || matches[1].length === 0) {
      throw new Error(`Parse action "${normalizedAction}" error! ${functionName} requires elementId as second parameter!`);
    }
    BrowserAgentUtils.assertElementIdExists(matches[1], liteDomTree, normalizedAction);
    const node = liteDomTree[Number(matches[1])];
    if (node.type !== 'inputtable') {
      throw new Error(`Parse action "${normalizedAction}" error! The target element ${matches[1]} is not inputtable. Check if there is nearby or parent element of the target that is inputtable.`);
    }
    if (matches.length < 3) {
      throw new Error(`Parse action "${normalizedAction}" error! ${functionName} requires text as third parameter!`);
    }
    if (node.element.tagName === 'input' && node.element.type === 'date') {
      const dateValue = matches[2].trim();
      if (!/^\d{4}-\d{2}-\d{2}$/.test(dateValue)) {
        throw new Error(
          `Parse action "${normalizedAction}" error! date input requires value in YYYY-MM-DD format, got "${matches[2]}".`
        );
      }
      const parts = dateValue.split('-');
      const month = Number(parts[1]);
      const day = Number(parts[2]);
      if (month < 1 || month > 12 || day < 1 || day > 31) {
        throw new Error(
          `Parse action "${normalizedAction}" error! date input has invalid month or day in "${matches[2]}".`
        );
      }
    }
  }

  private static checkSearchAction(functionName: string, matches: string[], normalizedAction: string): void {
    if (functionName !== 'SEARCH') {
      return;
    }
    if (matches.length < 2 || matches[1].length === 0) {
      throw new Error( `Parse action "${normalizedAction}" error! The second parameter (text) is empty! Please follow the action format strictly! Check if you have added the square bracket correctly.`);
    }
  }

  private static checkUploadAction(functionName: string, matches: string[], normalizedAction: string): void {
    if (functionName !== 'UPLOAD') {
      return;
    }
    if (matches.length < 2) {
      throw new Error(`Parse action "${normalizedAction}" error! UPLOAD requires at least 2 parameters: elementId and file path!`);
    }
    if (matches[1].length === 0) {
      throw new Error(`Parse action "${normalizedAction}" error! The file path is empty!`);
    }
    for (let i = 1; i < matches.length; i++) {
      const filePath = matches[i].trim();
      if (!BrowserAgentUtils.isAbsoluteLocalFilePath(filePath)) {
        throw new Error(
          `Parse action "${normalizedAction}" error! UPLOAD requires an absolute local file path explicitly provided by the user.`
        );
      }
    }
  }
  private static isAbsoluteLocalFilePath(filePath: string): boolean {
    return filePath.startsWith('/') || /^[A-Za-z]:[\\/]/.test(filePath) || filePath.startsWith('\\\\');
  }

  private static extractFileName(path: string): string {
    const normalized = path.replace(/\\/g, '/').trim();
    const index = normalized.lastIndexOf('/');
    return index >= 0 ? normalized.substring(index + 1) : normalized;
  }

  private static checkSelectTextAction(functionName: string, matches: string[], normalizedAction: string): void {
    if (functionName !== 'SELECT_TEXT') {
      return;
    }
    if (matches.length < 3) {
      throw new Error(`Parse action "${normalizedAction}" error! SELECT_TEXT requires 3 parameters: elementId, substring, and occurrence index!`);
    }
    if (matches[1].length === 0) {
      throw new Error(`Parse action "${normalizedAction}" error! The substring parameter is empty!`);
    }
    const occurrenceStr = matches[2].trim();
    if (!/^\d+$/.test(occurrenceStr)) {
      throw new Error(`Parse action "${normalizedAction}" error! The occurrence index must be a positive integer!`);
    }
    const occurrence = Number(occurrenceStr);
    if (occurrence < 1) {
      throw new Error(`Parse action "${normalizedAction}" error! The occurrence index must be at least 1!`);
    }
  }
  private static checkNavigationAction(functionName: string, firstParam: string, normalizedAction: string): void {
    if (functionName !== 'NAVIGATION' && functionName !== 'NAVIGATE_TO') {
      return;
    }
    if (firstParam === '') {
      throw new Error(
        `Parse action "${normalizedAction}" error! ${functionName} requires a url parameter in square brackets.`
      );
    }
  }

  private static checkScrollAction(
    functionName: string,
    actionText: string,
    params: string[],
    liteDomTree: AgentLiteDomNode[]
  ): void {
    if (functionName !== 'SCROLL') {
      return;
    }
    if (params.length <= 0) {
      throw new Error(`Parse action "${actionText}" error! Scroll action requires a direction parameter.`);
    }
    const direction = params[0].trim().toLowerCase();
    if (!BrowserAgentUtils.isDirection(direction)) {
      throw new Error(
        `Parse action "${actionText}" error! Unsupported scroll direction "${params[0]}".`
      );
    }
    if (params.length > 3) {
      throw new Error(`Parse action "${actionText}" error! Scroll action accepts at most direction, target, and viewport ratio.`);
    }
    if (params.length <= 1) {
      return;
    }
    const scrollTargetText = params[1].trim();
    if (BrowserAgentUtils.isWindowScrollTarget(scrollTargetText)) {
      if (params.length > 2) {
        BrowserAgentUtils.checkScrollRatio(actionText, params[2]);
      }
      return;
    }
    BrowserAgentUtils.assertElementIdExists(scrollTargetText, liteDomTree, actionText);
    if (params.length > 2) {
      BrowserAgentUtils.checkScrollRatio(actionText, params[2]);
    }
  }

  private static isWindowScrollTarget(value: string): boolean {
    return value.trim().toLowerCase() === 'window';
  }

  private static readonly MAX_SCROLL_VIEWPORT_RATIO = 3;

  private static checkScrollRatio(actionText: string, value: string): void {
    const normalized = value.trim().toLowerCase();
    if (normalized === 'page') {
      return;
    }
    if (!/^\d*\.?\d+$/.test(normalized)) {
      throw new Error(
        `Parse action "${actionText}" error! Scroll viewport ratio must be a number in (0, ${BrowserAgentUtils.MAX_SCROLL_VIEWPORT_RATIO}], e.g. 0.5, 1, or 2. Omit for default page scroll.`
      );
    }
    const numeric = Number(normalized);
    if (!Number.isFinite(numeric) || numeric <= 0 || numeric > BrowserAgentUtils.MAX_SCROLL_VIEWPORT_RATIO) {
      throw new Error(
        `Parse action "${actionText}" error! Scroll viewport ratio must be greater than 0 and at most ${BrowserAgentUtils.MAX_SCROLL_VIEWPORT_RATIO}.`
      );
    }
  }

  private static isNumericElementId(value: string): boolean {
    return /^\d+$/.test(value.trim());
  }

  private static isSelectTargetReference(value: string): boolean {
    const normalized = value.trim();
    if (normalized.length <= 0 || /\s/.test(normalized)) {
      return false;
    }
    if (/^[A-Za-z_][A-Za-z0-9_-]*$/.test(normalized)) {
      return true;
    }
    return normalized.startsWith('#') ||
    normalized.startsWith('.') ||
    normalized.startsWith('[') ||
    normalized.startsWith('select');
  }

  private static htmlContainsText(parser: HtmlParser, expected: string): boolean {
    const normalizedExpected = expected.trim().toLowerCase();
    if (normalizedExpected.length <= 0) {
      return false;
    }
    const treeText = parser.printWholeTree({
      includeNoneText: true,
      includeHref: false,
      useIndent: false,
    }).toLowerCase();
    return treeText.includes(normalizedExpected);
  }

  private static calculateExpression(actionText: string, expression: string): string {
    //calculate问题，如何安全计算？
    throw new Error(
      `The calculate action is dangerous. Try search for the expression on baidu`
    );
  }

  private static collectHref(
    actionText: string,
    firstParam: string,
    liteDomTree: AgentLiteDomNode[],
    parser: HtmlParser
  ): string {
    try {
      const ids = firstParam.split(/\s+/).filter((item) => item.length > 0);
      const ret: string[] = [];
      for (let i = 0; i < ids.length; i++) {
        const idText = ids[i];
        if (!/^\d+$/.test(idText)) {
          throw new Error(`Element ID ${idText} is not a number.`);
        }
        const elementIndex = Number(idText);
        if (elementIndex < 0 || elementIndex >= liteDomTree.length) {
          throw new Error(`Element ID ${idText} is out of range.`);
        }
        const url = parser.getUrl(elementIndex);
        if (url.length > 0) {
          ret.push(`url of element ${BrowserAgentUtils.describeNode(liteDomTree[elementIndex])}: ${url}`);
        } else {
          ret.push(`Get href of element ${idText} failed: Does not have href attribute.`);
        }
      }
      return JSON.stringify(ret);
    } catch (error) {
      throw new Error(`Parse action "${actionText}" error! ${error.message}`);
    }
  }

  private static normalizeDispatchAction(
    functionName: string,
    action: BrowserAgentAction,
    actionText: string,
    params: string[],
    liteDomTree: AgentLiteDomNode[]
  ): void {
    if (functionName !== 'DISPATCH') {
      return;
    }
    try {
      if (params.length !== 2) {
        throw new Error(`Expected two parameters: task, elements. Got ${params.length} parameters`);
      }
      const task = params[0];
      const normalizedIds = params[1].includes(',')
        ? params[1].replace(/,/g, ' ')
        : params[1];
      BrowserAgentUtils.applyActionMutation(action, `DISPATCH [${task}] [${normalizedIds}]`);
      const ids = normalizedIds.split(/\s+/).filter((item) => item.length > 0);
      if (ids.length <= 0) {
        throw new Error('No target elements');
      }
      for (let i = 0; i < ids.length; i++) {
        const idText = ids[i];
        if (!/^\d+$/.test(idText)) {
          throw new Error(`Element ID ${idText} is not a number.`);
        }
        const elementIndex = Number(idText);
        if (elementIndex < 0 || elementIndex >= liteDomTree.length) {
          throw new Error(`Element ID ${idText} is out of range.`);
        }
      }
    } catch (error) {
      throw new Error(`Parse action "${actionText}" error! ${error}`);
    }
  }

  private static applyActionMutation(action: BrowserAgentAction, actionText: string): void {
    action.action = actionText;
    action.requiresLoadingWait = BrowserAgentUtils.resolveActionRequiresLoadingWait(actionText);
  }

  private static resolveActionRequiresLoadingWait(actionText: string): boolean {
    const actionName = actionText.trim().split(/\s+/)[0];
    return resolveActionNeedWait(actionName);
  }

  private static describeNode(node: AgentLiteDomNode): string {
    return `{text: ${node.text}, tagName: ${node.element.tagName}}`;
  }

  static parseQueryResult(payloadText: string): { status: string, hasUserControlled: boolean } { //专门查询query action的返回值
    if (payloadText.length <= 0) {
      return { status: '', hasUserControlled: false };
    }
    try {
      const raw = JSON.parse(payloadText) as Record<string, Object>;
      const controlStatus = raw.controlStatus as Record<string, Object>;
      const status = controlStatus.status as string;
      const hasUserControlled = (controlStatus.humanControlTakenOver ?? false) as boolean;
      return { status: status, hasUserControlled: hasUserControlled };
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parseQueryResult error: %{public}s. Using empty string', e.message);
      return { status: '', hasUserControlled: false };
    }
  }

  static resolveErrorType(error: Object): { errorType: string; errorMessage: string } {
    const message = BrowserAgentUtils.stringifyError(error);
    if (message.length <= 0) {
      return { errorType: '', errorMessage: '' };
    }
    try {
      const parsed = JSON.parse(message) as Record<string, Object>;
      const errorType = typeof parsed.errorType === 'string' ? parsed.errorType as string : '';
      const errorMessage = typeof parsed.errorMessage === 'string' ? parsed.errorMessage as string : '';
      return { errorType, errorMessage };
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'resolveErrorType error: %{public}s', e.message);
      return { errorType: '', errorMessage: message };
    }
  }

  static formatSensitiveActionReason(errorMessage: string): string {
    const normalized = errorMessage.trim();
    return BrowserAgentUtils.SUPPORTED_SENSITIVE_OPERATIONS.has(normalized) ? normalized : '';
  }

  static isRecoverableActionError(error: Object): boolean {
    const resolved = BrowserAgentUtils.resolveErrorType(error);
    if (resolved.errorType === 'ElementNotFound' || resolved.errorType === 'ElementNotInteractable') {
      return true;
    }
    const lowered = resolved.errorMessage.toLowerCase();
    return lowered.includes('not scrollable') ||
      lowered.includes('element not found') || lowered.includes('no elementmap cached');
  }

  static formatActionFailureForAgent(error: Object, action: BrowserAgentAction | null): string {
    const resolved = BrowserAgentUtils.resolveErrorType(error);
    const actionText = action !== null ? action.action : 'unknown action';
    const detail = resolved.errorMessage.length > 0 ?
      resolved.errorMessage :
      BrowserAgentUtils.stringifyError(error);
    if (detail.includes('failed to focus on input box')) {
      return `The action "${actionText}" failed and was not executed: ${detail}. ` +
        'Try other actions, or try to click on the target element **twice** in a row to focus on it before fill.';
    }
    return `The action "${actionText}" failed and was not executed: ${detail}. ` +
      'The page may have changed or the element id may be stale or wrong.';
  }

  static async contextInteract<T>(
    context: BrowserAgentGraphContext | AgentToolContext,
    f: () => Promise<T>
  ): Promise<[T, boolean]> {
    //用于在不涉及agent流程变动（图流动变动、state内容变动）的情况下，通过重试解决perceive或execute时遇到用户主动接管问题
    //返回的boolean表示是否中途检测到用户接管
    //目前如果用户在action execute之后，perceive之前接管，接管结束后不会告知agent，后续可能要改
    let error: unknown;
    for (let i = 0; i < 3; i++) {
      try {
        const ret = await f();
        return [ret, false];
      } catch (e) {
        error = e;
        const errorType = BrowserAgentUtils.resolveErrorType(e as Object).errorType;
        context.events.onChunk(`node contextInteract failed ${i}/3: ${errorType}`, false);
        if ([AgentConstants.ACTION_NOT_SUPPORTED_MESSAGE, AgentConstants.GROUP_CLOSED].includes(errorType)) {
          throw e;
        }
        if (errorType === AgentConstants.IDLE_MESSAGE) {
          throw e;
        }
        if (errorType === AgentConstants.ACTION_BUSY_MESSAGE) {
          return BrowserAgentUtils.waitForActionBusy(context, f);
        }
        if (BrowserAgentUtils.hasUserIntervened(errorType)) {
          // 用户接管就不再过3次尝试了
          const ret = await BrowserAgentUtils.waitForUserControl(context, f);
          return [ret, true];
        }
        if (context.workspace.isDebug()) {
          context.workspace.appendText('error.log', 'contextInteract ' + e.message + '\n');
        }
      }
    }
    throw error;
  }

  /**
   * 扩展忙碌时仅重试当前只读交互，等待执行槽释放后返回最新结果。
   */
  private static async waitForActionBusy<T>(
    context: BrowserAgentGraphContext | AgentToolContext,
    f: () => Promise<T>,
  ): Promise<[T, boolean]> {
    context.events.onChunk('浏览器扩展仍在执行上一个动作，等待执行槽释放', false);
    const startedAt = Date.now();
    while (Date.now() - startedAt < AgentConstants.ACTION_BUSY_WAIT_TIMEOUT) {
      await new Promise<void>((resolve) =>
        setTimeout(resolve, AgentConstants.ACTION_BUSY_RETRY_INTERVAL)
      );
      try {
        const result = await f();
        return [result, false];
      } catch (error) {
        const errorType = BrowserAgentUtils.resolveErrorType(error as Object).errorType;
        if (errorType === AgentConstants.ACTION_BUSY_MESSAGE) {
          continue;
        }
        if (errorType === AgentConstants.BROWSER_MESSAGE_USER_CONTROLLING) {
          const result = await BrowserAgentUtils.waitForUserControl(context, f);
          return [result, true];
        }
        throw error;
      }
    }
    throw new Error(JSON.stringify({
      errorType: AgentConstants.ACTION_BUSY_MESSAGE,
      errorMessage: 'browser extension remained busy while waiting to re-observe'
    }));
  }

  private static async waitForUserControl<T>(
    context: BrowserAgentGraphContext | AgentToolContext,
    f: () => Promise<T>,
  ): Promise<T> {
    context.events.onChunk(`检测到用户主动接管，agent等待`, false);
    context.events.onSwitchControl?.('user');
    const startTime = Date.now();
    while (Date.now() - startTime < AgentConstants.USER_COMPLETE_TIMEOUT) {
      try {
        const ret = await f();
        context.events.onChunk(`用户接管完成，agent继续`, false);
        context.events.onSwitchControl?.('agent');
        return ret;
      } catch (e) {
        const errorType = BrowserAgentUtils.resolveErrorType(e as Object).errorType;
        if (!BrowserAgentUtils.hasUserIntervened(errorType)) {
          throw e;
        }
        hilog.warn(DOMAIN, TAG, 'waitForUserControl caught error: %{public}s', e.message);
      }
      await new Promise<void>((resolve) => setTimeout(resolve, 500));
    }
    context.events.onChunk(`agent等待时间内（1小时内）用户未完成接管，任务终止`, false);
    hilog.info(DOMAIN, TAG, 'agent等待时间内（1小时内）用户未完成接管，任务终止');
    throw new Error('agent等待时间内（1小时内）用户未完成接管，任务终止');
  }

  public static withTimeout<T>(promise: Promise<T>, ms: number): Promise<T> {
    const timeout = new Promise<never>((_, reject) =>
    setTimeout(() => reject(new Error('Timeout')), ms)
    );
    return Promise.race([promise, timeout]);
  }

  public static hasUserIntervened(errorType: string): boolean {
    return [AgentConstants.BROWSER_MESSAGE_USER_CONTROLLING, AgentConstants.BROWSER_MESSAGE_USER_HAS_CONTROLLED].includes(errorType);
  }

  public static markIfUserControlled(state: AgentState, controlled: boolean): void {
    if (controlled) {
      state.previousReturnValue =
        'The user took over the browser in the previous step, and the execution of the previous ' +
        'action / tool was interrupted. The user has now finished his/her operation and gave the ' +
        'browser back to you. The webpage may have changed compared to the last time you saw it ' +
        'due to user control. Please try to continue the task';
      state.actionQueue = []; // 清空actionQueue
    }
  }

  static isCoordinateFormat(text: string): boolean {
    return text.toUpperCase().startsWith('COORDINATE');
  }

  /**
   * 根据BrowserAgentAction中的坐标（[0,1000]范围），计算其可能点击了哪些liteDom控件。
   *
   * 原则：
   * 1. 坐标中心必须位于控件的bbox中（命中才纳入候选）。
   * 2. 按照坐标与bbox中心的距离升序排序。
   * 3. 若某控件有多个bbox，以其各bbox与坐标距离的最小值作为排序依据。
   *
   * 返回命中控件的索引数组（按距离升序），坐标未命中任何控件时返回空数组。
   */
  static findHitLiteDomNodes(
    action: BrowserAgentAction,
    liteDomTree: AgentLiteDomNode[],
    viewport: AgentViewport
  ): number[] {
    if (!action.point || viewport.width <= 0 || viewport.height <= 0) {
      return [];
    }

    const SCALE_RANGE = 1000;
    const px = action.point.x * viewport.width / SCALE_RANGE;
    const py = action.point.y * viewport.height / SCALE_RANGE;

    const hits: LiteDomHit[] = [];
    for (let i = 0; i < liteDomTree.length; i++) {
      const node = liteDomTree[i];
      let minDistance = -1;
      for (let j = 0; j < node.rects.length; j++) {
        const rect = node.rects[j];
        if (px < rect.left || px > rect.right || py < rect.top || py > rect.bottom) {
          continue;
        }
        const centerX = (rect.left + rect.right) / 2;
        const centerY = (rect.top + rect.bottom) / 2;
        const dx = px - centerX;
        const dy = py - centerY;
        const distance = Math.sqrt(dx * dx + dy * dy);
        if (minDistance < 0 || distance < minDistance) {
          minDistance = distance;
        }
      }
      if (minDistance >= 0) {
        const hit = new LiteDomHit();
        hit.index = i;
        hit.distance = minDistance;
        hits.push(hit);
      }
    }

    hits.sort((a: LiteDomHit, b: LiteDomHit): number => a.distance - b.distance);

    const result: number[] = [];
    for (let i = 0; i < hits.length; i++) {
      result.push(hits[i].index);
    }
    return result;
  }

  private filterObservationDom(observation: AgentObservation, relevantIds: number[]): void {
    const idSet = new Set(relevantIds);
    // Always keep scrollable elements as they may be needed for navigation
    const filteredTree: AgentLiteDomNode[] = [];
    for (let i = 0; i < observation.liteDomTree.length; i++) {
      if (idSet.has(i) || observation.liteDomTree[i].type === 'scrollable') {
        filteredTree.push(observation.liteDomTree[i]);
      }
    }
    // Rebuild liteDomSummary from filtered tree — keep original IDs in summary
    const summaryLines: string[] = [];
    for (let i = 0; i < observation.liteDomTree.length; i++) {
      if (idSet.has(i) || observation.liteDomTree[i].type === 'scrollable') {
        const node = observation.liteDomTree[i];
        const tagName = node.element.tagName.length > 0 ? node.element.tagName.toLowerCase() : 'unknown';
        const attrs: string[] = [];
        if (node.type === 'scrollable') {
          attrs.push('scrollable=true');
        }
        const text = node.text.trim().replace(/\s+/g, ' ');
        if (text.length > 0) {
          attrs.push(`text=${text.length > 200 ? text.substring(0, 200) + '...' : text}`);
        }
        const elementType = node.element.type.trim().toLowerCase();
        const value = node.element.value.trim().replace(/\s+/g, ' ');
        if (elementType !== 'password' && elementType !== 'hidden' && value.length > 0 && value !== text) {
          const promptValue = value.length > 200 ? value.substring(0, 200) + '...' : value;
          attrs.push(`value=${JSON.stringify(promptValue)}`);
        }
        const title = node.element.title.trim().length > 0 ? node.element.title.trim() : node.element.ariaLabel.trim();
        if (title.length > 0 && title !== text) {
          attrs.push(`title=${JSON.stringify(title)}`);
        }
        if (node.element.options.length > 0) {
          attrs.push(`option=${JSON.stringify(node.element.options)}`);
        }
        summaryLines.push(attrs.length > 0 ? `${i}: <${tagName}> ${attrs.join(', ')}` : `${i}: <${tagName}>`);
      }
    }
    observation.liteDomSummary = summaryLines.join('\n');
  }

  public static parseTracePayload(tracePayloadText: string): Object[] {
    if (tracePayloadText.length <= 0) {
      return [];
    }
    try {
      const parsed = JSON.parse(tracePayloadText);
      if (!Array.isArray(parsed)) {
        return [];
      }
      const entries: Object[] = [];
      for (let i = 0; i < parsed.length; i++) {
        if (typeof parsed[i] === 'object' && parsed[i] !== null) {
          entries.push(parsed[i] as Object);
        }
      }
      return entries;
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'parseTracePayload error: %{private}s', e.message);
      return [];
    }
  }

  public static buildTabFocusAction(tabId: number): BrowserAgentAction {
    const action = new BrowserAgentAction();
    action.action = `TAB_FOCUS [${Math.floor(tabId)}]`;
    action.actionType = 'TAB_FOCUS';
    action.element = null;
    action.params = [String(Math.floor(tabId))];
    action.requiresLoadingWait = false;
    action.reason = '切回起始标签页';
    return action;
  }

  public static buildGoBackToStartAction(): BrowserAgentAction {
    const action = new BrowserAgentAction();
    action.action = 'go_back_to_start';
    action.actionType = 'GO_BACK_TO_START';
    action.element = null;
    action.params = [];
    action.requiresLoadingWait = true;
    action.reason = '清理标签页';
    return action;
  }

  public static buildQueryAction(): BrowserAgentAction {
    const action = new BrowserAgentAction();
    action.action = 'QUERY';
    action.actionType = 'QUERY';
    action.requiresLoadingWait = false;
    return action;
  }

  public static buildAgentControlAction(): BrowserAgentAction {
    const action = new BrowserAgentAction();
    action.action = 'AGENT_CONTROL';
    action.actionType = 'AGENT_CONTROL';
    action.requiresLoadingWait = false;
    action.reason = '恢复agent操控';
    return action;
  }

  public static buildGoBackAction(): BrowserAgentAction {
    const action = new BrowserAgentAction();
    action.action = 'GO_BACK';
    action.actionType = 'GO_BACK';
    action.requiresLoadingWait = true;
    action.reason = '清理标签页';
    return action;
  }

  public static buildTabCloseAction(reason: string): BrowserAgentAction {
    const action = new BrowserAgentAction();
    action.action = 'TAB_CLOSE';
    action.actionType = 'TAB_CLOSE';
    action.element = null;
    action.params = [];
    action.requiresLoadingWait = false;
    action.reason = reason;
    return action;
  }

  public static buildScrollIntoViewActions(
    node: AgentLiteDomNode,
    viewportWidth: number,
    viewportHeight: number
  ): BrowserAgentAction[] {
    if (node.rects.length <= 0) {
      return [];
    }
    const minTop = Math.min(...node.rects.map(r => r.top));
    const maxBottom = Math.max(...node.rects.map(r => r.bottom));
    const minLeft = Math.min(...node.rects.map(r => r.left));
    const maxRight = Math.max(...node.rects.map(r => r.right));
    const MAX_RATIO = BrowserAgentUtils.MAX_SCROLL_VIEWPORT_RATIO;
    const scrolls: BrowserAgentAction[] = [];

    // 居中策略：把元素中线对齐到视口中线。ratio = (元素中线 - 视口中线) / 视口尺寸。
    // 垂直：元素整体在视口上方（maxBottom <= 0）或下方（minTop >= viewportH）才发 scroll
    const centerV = (minTop + maxBottom) / 2;
    const halfH = viewportHeight / 2;
    if (maxBottom <= 0) {
      const ratio = Math.min(MAX_RATIO, Math.max(0, (halfH - centerV) / viewportHeight));
      scrolls.push(BrowserAgentUtils.buildWindowScroll('UP', ratio));
    } else if (minTop >= viewportHeight) {
      const ratio = Math.min(MAX_RATIO, Math.max(0, (centerV - halfH) / viewportHeight));
      scrolls.push(BrowserAgentUtils.buildWindowScroll('DOWN', ratio));
    }
    // 水平
    const centerH = (minLeft + maxRight) / 2;
    const halfW = viewportWidth / 2;
    if (maxRight <= 0) {
      const ratio = Math.min(MAX_RATIO, Math.max(0, (halfW - centerH) / viewportWidth));
      scrolls.push(BrowserAgentUtils.buildWindowScroll('LEFT', ratio));
    } else if (minLeft >= viewportWidth) {
      const ratio = Math.min(MAX_RATIO, Math.max(0, (centerH - halfW) / viewportWidth));
      scrolls.push(BrowserAgentUtils.buildWindowScroll('RIGHT', ratio));
    }
    return scrolls;
  }

  public static buildWindowScroll(direction: string, ratio: number): BrowserAgentAction {
    const ratioText = (Math.round(ratio * 100) / 100).toString();
    const action = new BrowserAgentAction();
    action.action = `SCROLL [${direction}] [window] [${ratioText}]`;
    action.actionType = 'SCROLL';
    action.element = null;
    action.params = [direction, 'window', ratioText];
    action.requiresLoadingWait = true;
    action.reason = '自动将目标元素滚动至视野内';
    return action;
  }
}

class LiteDomHit {
  public index: number = 0;
  public distance: number = 0;
}

export class Timer {
  private start: number;
  constructor() {
    this.start = Date.now();
  }
  tik(tag: string): number {
    const t = Date.now() - this.start;
    this.start = Date.now();
    hilog.debug(DOMAIN, TAG, 'Time %{public}s: %{public}d', tag, t);
    return t;
  }
}
