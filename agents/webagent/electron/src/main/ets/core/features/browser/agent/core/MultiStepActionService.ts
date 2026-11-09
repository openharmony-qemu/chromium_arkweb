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

import { AgentLiteDomNode, AgentLiteDomRect, AgentDecision, AgentObservation } from './AgentTypes';
import { LiteDomTrie } from './LiteDomSummaryService';
import { BrowserAgentLiteDomSummaryService } from './LiteDomSummaryService';
import { AgentStateFactory } from './AgentStateFactory';
import { BrowserAgentUtils } from './Utils';
import { BrowserAgentAction } from '../core/AgentTypes';
import { vConfig } from '../VersionConfig';
import { BrowserAgentObservationService } from './ObservationService';
import { HtmlParser } from './HtmlParser';
import {hilog} from '@kit.PerformanceAnalysisKit';
const DOMAIN = 0x6020;
const TAG = 'MultiStepActionService';

export class MultiStepActionService {
  private static readonly MAX_SCROLL_VIEWPORT_RATIO: number = 2;
  private static readonly DOM_RECT_TOLERANCE_PX: number = 8;

  public static isQueuedActionStillValid(
    previousObservation: AgentObservation | null | undefined,
    currentObservation: AgentObservation | null | undefined,
    action: BrowserAgentAction,
    previousAction: BrowserAgentAction | null | undefined
  ): boolean {
    hilog.debug(DOMAIN, TAG, 'node reason action queue check target stable for action: %{private}s, previousAction: %{private}s', action.action, previousAction ? previousAction.action : 'null');
    if (!previousObservation || !currentObservation || MultiStepActionService.hasPageContextChanged(previousObservation, currentObservation)) {
      return false;
    }
    const preDom = previousObservation.liteDomTree;
    const nowDom = currentObservation.liteDomTree;
    // 上一步 action 的目标 id（仅当为非负整数时才用于「下方新增内容」判断，否则跳过）
    let previousId: number = -1;
    if (previousAction && typeof previousAction.element === 'number' && previousAction.element >= 0) {
      previousId = previousAction.element;
    }
    if (!['CLICK', 'SEARCH', 'FILL', 'SELECT', 'SELECT_TEXT', 'SCROLL'].includes(action.actionType)) {
      return false;
    }
    let id: number = -1;
    if (typeof action.element === 'number' && action.element >= 0) {
      id = action.element;
    }
    if (id >= 0) {
      if (id >= preDom.length || id >= nowDom.length) {
        return false;
      }
      if (!MultiStepActionService.isSameDomNode(preDom[id], nowDom[id])) { //防止目标变化
        return false;
      }
      if (MultiStepActionService.isBlockedBy(nowDom, id) > MultiStepActionService.isBlockedBy(preDom, id)) { //防止目标被新内容遮挡
        return false;
      }
      if (previousId >= 0 && MultiStepActionService.hasNewNodeBelowTarget(preDom, nowDom, previousId)) { //防止上一步目标下方出现新增内容（如下拉列表）
        return false;
      }
    }
    return true;
  }

  public static hasPageContextChanged(previous: AgentObservation, current: AgentObservation): boolean {
    if (previous.currentUrl !== current.currentUrl) {
      return true;
    }
    if (previous.siteId !== current.siteId) {
      return true;
    }
    if (previous.pageStateId !== current.pageStateId) {
      return true;
    }
    return false;
  }

  public static isSameDomNode(previous: AgentLiteDomNode, current: AgentLiteDomNode): boolean {
    if (!previous || !current) {
      return false;
    }
    return previous.xpath === current.xpath &&
      previous.text === current.text &&
      previous.type === current.type &&
      previous.element.tagName === current.element.tagName &&
      previous.element.ariaLabel === current.element.ariaLabel &&
      previous.element.dataId === current.element.dataId &&
      previous.element.title === current.element.title &&
      previous.element.value === current.element.value &&
    MultiStepActionService.isSameDomRect(previous, current);
  }

  private static isSameDomRect(previous: AgentLiteDomNode, current: AgentLiteDomNode): boolean {
    if (previous.rects.length <= 0 && current.rects.length <= 0) {
      return true;
    }
    if (previous.rects.length <= 0 || current.rects.length <= 0) {
      return false;
    }
    const previousRect = previous.rects[0];
    const currentRect = current.rects[0];
    return MultiStepActionService.isCloseNumber(previousRect.left, currentRect.left, MultiStepActionService.DOM_RECT_TOLERANCE_PX) &&
    MultiStepActionService.isCloseNumber(previousRect.top, currentRect.top, MultiStepActionService.DOM_RECT_TOLERANCE_PX) &&
    MultiStepActionService.isCloseNumber(previousRect.width, currentRect.width, MultiStepActionService.DOM_RECT_TOLERANCE_PX) &&
    MultiStepActionService.isCloseNumber(previousRect.height, currentRect.height, MultiStepActionService.DOM_RECT_TOLERANCE_PX);
  }

  private static isCloseNumber(left: number, right: number, tolerance: number): boolean {
    return Math.abs(left - right) <= tolerance;
  }

  private static isBlockedBy(nodes: AgentLiteDomNode[], id: number): number {
    if (nodes[id].rects.length !== 1) {
      return 0;
    }
    let cnt = 0;
    for (let i = 0; i < nodes.length; i++) {
      if (i !== id && MultiStepActionService.covered(nodes[id].rects[0], nodes[i].rects)) {
        cnt += 1;
      }
    }
    return cnt;
  }

  private static covered(rect1: AgentLiteDomRect, rect2: AgentLiteDomRect[]): boolean {
    for (let i = 0; i < rect2.length; i++) {
      if (MultiStepActionService.IOU(rect1, rect2[i]) > 0.5) {
        return true;
      }
    }
    return false;
  }

  private static IOU(rect1: AgentLiteDomRect, rect2: AgentLiteDomRect): number {
    const top = Math.max(rect1.top, rect2.top);
    const bottom = Math.min(rect1.bottom, rect2.bottom);
    const left = Math.max(rect1.left, rect2.left);
    const right = Math.min(rect1.right, rect2.right);
    const intersection = Math.max(bottom - top, 0) * Math.max(right - left, 0);
    const s1 = (rect1.right - rect1.left) * (rect1.bottom - rect1.top);
    const s2 = (rect2.right - rect2.left) * (rect2.bottom - rect2.top);
    return intersection / (s1 + s2 - intersection + 1e-8);
  }

  /**
   * 判断 currentDom 中是否存在「preDom 中不存在的」结点，且其 bbox 完全位于 preDom[preDomId] 的下方
   * （同时要求与目标在水平方向上有交集，避免把侧边栏等左右错位的新增内容误判为目标下方的新增）。
   * 用于 isQueuedActionStillValid：若上一步锁定的目标元素下方出现了新增内容，通常意味着页面已变化，
   * 之前推理出的动作不宜直接复用。
   */
  static hasNewNodeBelowTarget(
    preDom: AgentLiteDomNode[],
    currentDom: AgentLiteDomNode[],
    preDomId: number
  ): boolean {
    if (preDomId < 0 || preDomId >= preDom.length) {
      return false;
    }
    const targetBbox = MultiStepActionService.unionBbox(preDom[preDomId].rects);
    if (targetBbox === null) {
      return false;
    }
    const preTrie = new LiteDomTrie(preDom);
    for (let i = 0; i < currentDom.length; i++) {
      const cur = currentDom[i];
      const xpath = typeof cur.xpath === 'string' ? cur.xpath : '';
      if (xpath.length === 0) {
        continue;
      }
      if (preTrie.exists(xpath)) {
        continue;
      }
      hilog.debug(DOMAIN, TAG, 'found new dom element %{private}s', JSON.stringify(currentDom[i]));
      const curBbox = MultiStepActionService.unionBbox(cur.rects);
      if (curBbox === null) {
        continue;
      }
      hilog.debug(DOMAIN, TAG, 'pre box = %{private}s, cur box = %{private}s', JSON.stringify(targetBbox), JSON.stringify(curBbox));
      if (curBbox.top >= targetBbox.bottom &&
        MultiStepActionService.horizontallyOverlaps(curBbox, targetBbox)) {
        return true;
      }
    }
    return false;
  }

  /** 判断两个包围盒在水平方向上是否有交集（仅边相切不算）。 */
  private static horizontallyOverlaps(a: AgentLiteDomRect, b: AgentLiteDomRect): boolean {
    return a.left < b.right && a.right > b.left;
  }

  /** 计算一组 rect 的并集包围盒；rects 为空时返回 null。 */
  private static unionBbox(rects: AgentLiteDomRect[]): AgentLiteDomRect | null {
    if (!Array.isArray(rects) || rects.length === 0) {
      return null;
    }
    const first = rects[0];
    let left = first.left;
    let top = first.top;
    let right = first.right;
    let bottom = first.bottom;
    for (let i = 1; i < rects.length; i++) {
      const r = rects[i];
      if (r.left < left) {
        left = r.left;
      }
      if (r.top < top) {
        top = r.top;
      }
      if (r.right > right) {
        right = r.right;
      }
      if (r.bottom > bottom) {
        bottom = r.bottom;
      }
    }
    const bbox = new AgentLiteDomRect();
    bbox.left = left;
    bbox.top = top;
    bbox.right = right;
    bbox.bottom = bottom;
    return bbox;
  }

  /**
   * 若 decision.browserAction 目标是 outside 元素，则替换为 scroll-into-view 动作序列。
   * 垂直方向 scroll 放 browserAction，水平方向 scroll 放 actionQueue（各带一次感知，最后回 LLM 推理）。
   * 返回非空 hint 字符串表示已替换；空串表示无需替换。
   */
  static maybeDeferOutsideElementAction(
    decision: AgentDecision,
    observation: AgentObservation,
    combinedTree: AgentLiteDomNode[]
  ): string {
    const action = decision.browserAction;
    if (!action || !vConfig.outside_element) {
      return '';
    }
    const liteCount = observation.liteDomTree.length;
    if (liteCount <= 0) {
      return '';
    }
    const targetId = BrowserAgentUtils.resolveActionTargetElementId(action.action);
    if (targetId < liteCount) {
      // 非 outside 元素（或非元素动作 -1），无需替换
      return '';
    }
    if (targetId >= combinedTree.length) {
      // 真越界，checkActionParameter 已应抛错；此处兜底不替换
      return '';
    }
    const node = BrowserAgentObservationService.findOutsideNodeById(observation.outsideDOMTree, targetId);
    if (!node) {
      return '';
    }
    const viewportW = observation.viewport.width;
    const viewportH = observation.viewport.height;
    if (viewportW <= 0 || viewportH <= 0) {
      return '';
    }

    const scrolls = MultiStepActionService.buildScrollIntoViewActions(node, viewportW, viewportH);
    if (scrolls.length <= 0) {
      return '';
    }

    const originalActionText = action.action;
    const elementDesc = MultiStepActionService.describeLiteDomNodeForHint(node);
    const scrollTexts = scrolls.map((s: BrowserAgentAction): string => s.action).join('; ');

    // 垂直 scroll 作 browserAction；水平 scroll（若有）入 actionQueue 队首
    decision.browserAction = scrolls[0];
    decision.browserAction.reason = `auto-deferred: scroll off-screen element (id=${targetId}) into view`;

    // SELECT/SELECT_TEXT 的 option 解析依赖 liteDomTree 数字索引（adaptSelectAction 会 clobber element），
    // 不走 deferred-selector，scroll 后由 LLM 重新推理；其余元素动作可压入 deferred（element=CSS selector）。
    const actionType = action.actionType;
    const hasStableSelector = typeof node.selector === 'string' && node.selector.length > 0;
    // path-stability 精查需要 xpath；无 xpath 则只能依赖整树哈希快路径
    const hasXpath = typeof node.xpath === 'string' && node.xpath.length > 0;
    const selectorBasedDeferred = hasStableSelector && hasXpath && actionType !== 'SELECT' && actionType !== 'SELECT_TEXT';

    const queue: BrowserAgentAction[] = [];
    if (scrolls.length > 1) {
      queue.push(scrolls[1]);
    }
    if (selectorBasedDeferred) {
      // 把原动作压入队列：element=CSS selector（transport 发 params.selector），xpath=节点 xpath（handleBacklog 精查用）
      const deferred = AgentStateFactory.cloneAction(action);
      deferred.element = node.selector;
      deferred.xpath = node.xpath;
      deferred.reason = decision.explanation; // 使用explanation而不是reason
      queue.push(deferred);
    }
    // 丢弃 LLM 原后续队列（依赖 outside 动作成功，已失效）
    decision.actionQueue = queue;

    hilog.debug(DOMAIN, TAG,
      'deferred outside-element action %{private}s -> scroll(s): %{private}s, queued deferred=%{private}s',
      originalActionText, scrollTexts, selectorBasedDeferred ? node.selector : '(none, re-derive)');

    if (selectorBasedDeferred) {
      return `Deferred original action \`${originalActionText}\` targeting an off-screen element ` +
        `(${elementDesc}, id=${targetId}); executed ${scrollTexts} to bring it into view. `;
    }
    return `Deferred original action \`${originalActionText}\` targeting an off-screen element ` +
      `(${elementDesc}, id=${targetId}); executed ${scrollTexts} to bring it into view. ` +
      `Re-derive the intended action against the freshly perceived tree.`;
  }

  static buildScrollIntoViewActions(
    node: AgentLiteDomNode,
    viewportWidth: number,
    viewportHeight: number
  ): BrowserAgentAction[] {
    if (node.rects.length <= 0) {
      return [];
    }
    let minTop = node.rects[0].top;
    let maxBottom = node.rects[0].bottom;
    let minLeft = node.rects[0].left;
    let maxRight = node.rects[0].right;
    for (let i = 1; i < node.rects.length; i++) {
      const r = node.rects[i];
      minTop = Math.min(minTop, r.top);
      maxBottom = Math.max(maxBottom, r.bottom);
      minLeft = Math.min(minLeft, r.left);
      maxRight = Math.max(maxRight, r.right);
    }
    const MAX_RATIO = MultiStepActionService.MAX_SCROLL_VIEWPORT_RATIO;
    const scrolls: BrowserAgentAction[] = [];

    // 居中策略：把元素中线对齐到视口中线。ratio = (元素中线 - 视口中线) / 视口尺寸。
    // 垂直：元素整体在视口上方（maxBottom <= 0）或下方（minTop >= viewportH）才发 scroll
    const centerV = (minTop + maxBottom) / 2;
    const halfH = viewportHeight / 2;
    if (maxBottom <= 0) {
      const ratio = Math.min(MAX_RATIO, Math.max(0, (halfH - centerV) / viewportHeight));
      scrolls.push(MultiStepActionService.buildWindowScroll('UP', ratio));
    } else if (minTop >= viewportHeight) {
      const ratio = Math.min(MAX_RATIO, Math.max(0, (centerV - halfH) / viewportHeight));
      scrolls.push(MultiStepActionService.buildWindowScroll('DOWN', ratio));
    }
    // 水平
    const centerH = (minLeft + maxRight) / 2;
    const halfW = viewportWidth / 2;
    if (maxRight <= 0) {
      const ratio = Math.min(MAX_RATIO, Math.max(0, (halfW - centerH) / viewportWidth));
      scrolls.push(MultiStepActionService.buildWindowScroll('LEFT', ratio));
    } else if (minLeft >= viewportWidth) {
      const ratio = Math.min(MAX_RATIO, Math.max(0, (centerH - halfW) / viewportWidth));
      scrolls.push(MultiStepActionService.buildWindowScroll('RIGHT', ratio));
    }
    return scrolls;
  }

  static buildWindowScroll(direction: string, ratio: number): BrowserAgentAction {
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

  static describeLiteDomNodeForHint(node: AgentLiteDomNode): string {
    return BrowserAgentLiteDomSummaryService.buildLiteDomSummaryLine(node.id, node);
  }

  /**
   * 在 liteDomTree 中查找 xpath 精确匹配的节点索引。线性扫描，树通常很小。
   * 未找到返回 -1。仅匹配非空 xpath。
   */
  static findLiteDomIndexByXpath(tree: AgentLiteDomNode[], xpath: string): number {
    if (xpath.length <= 0) {
      return -1;
    }
    for (let i = 0; i < tree.length; i++) {
      if (tree[i].xpath === xpath) {
        return i;
      }
    }
    return -1;
  }

  /**
   * 整树 htmlHash 不等时，沿 deferred 动作的目标 xpath 路径精查"左下区域"稳定性：
   * 用 queueOriginObservation 的原始 html 重建 prevParser（only_visible=false，
   * must_include/noFoldTag 不影响树结构与哈希，故与 curr parser 可比），
   * 再调用 HtmlParser.isTargetStableAlongPath 比对沿路径子树与祖先左侧孩子哈希。
   * 命中且稳定返回 true；xpath/prevHtml 缺失或不可重建返回 false。
   */
  static isDeferredTargetStableAlongXpath(
    queuedAction: BrowserAgentAction,
    originObs: AgentObservation | null | undefined,
    currParser: HtmlParser
  ): boolean {
    const xpath = queuedAction.xpath;
    const prevHtml = originObs && originObs.html ? originObs.html : '';
    if (xpath.length > 0 && prevHtml.length > 0 && currParser) {
      const prevParser = new HtmlParser(prevHtml, false, [], ['a']);
      return HtmlParser.isTargetStableAlongPath(prevParser, currParser, xpath, false);
    }
    return false;
  }

  /**
   * 把 deferred 动作文本里 element-id 槽位（如 CLICK [100]）替换为当前 liteDomTree 里 xpath 对应的新序号。
   * 仅做文本级 token 替换：按动作类型定位 element 参数在括号序列中的位置（CLICK/HOVER/...=0, FILL/RICH_TEXT=1）。
   * 命中返回重写后的 action 文本；xpath 缺失/未找到/动作类型不支持返回 null（调用方回退到原文本）。
   */
  static rewriteDeferredActionElementId(
    queuedAction: BrowserAgentAction,
    currentTree: AgentLiteDomNode[]
  ): string | null {
    const xpath = queuedAction.xpath;
    if (xpath.length <= 0) {
      return null;
    }
    const newId = MultiStepActionService.findLiteDomIndexByXpath(currentTree, xpath);
    if (newId < 0) {
      return null;
    }
    const actionText = queuedAction.action;
    const functionName = actionText.trim().split(/\s+/)[0].toUpperCase();
    const elementParamIndex = MultiStepActionService.resolveElementParamIndex(functionName);
    if (elementParamIndex < 0) {
      return null;
    }
    const params = BrowserAgentUtils.extractBracketParams(actionText);
    if (params.length <= elementParamIndex) {
      return null;
    }
    if (BrowserAgentUtils.isCoordinateFormat(params[elementParamIndex])) {
      return null; // 坐标格式不替换
    }
    // 重建：保留空白与括号序列，仅替换 element-id token
    let paramIdx = 0;
    let result = '';
    let i = 0;
    while (i < actionText.length) {
      const ch = actionText.charAt(i);
      if (ch === '[') {
        const close = actionText.indexOf(']', i);
        if (close < 0) {
          break; // 括号不匹配，原样保留剩余
        }
        const inner = actionText.substring(i + 1, close);
        if (paramIdx === elementParamIndex) {
          result += `[${String(newId)}]`;
        } else {
          result += `[${inner}]`;
        }
        paramIdx += 1;
        i = close + 1;
      } else {
        result += ch;
        i += 1;
      }
    }
    return result;
  }

  static resolveElementParamIndex(actionName: string): number {
    switch (actionName) {
      case 'CLICK':
      case 'HOVER':
      case 'SEARCH':
      case 'SEARCH_BY_QUERY':
      case 'SELECT_TEXT':
      case 'SELECT':
      case 'UPLOAD':
        return 0;
      case 'FILL':
      case 'RICH_TEXT':
        return 1;
      default:
        return -1;
    }
  }
}
