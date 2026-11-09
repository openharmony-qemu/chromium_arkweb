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

import { AgentConstants, AgentLiteDomNode, AgentLiteDomRect, AgentObservation, AgentOutsideDOMTree, OutsideDirection } from './AgentTypes';
import { BrowserAgentDialogObservationService } from './DialogObservationService';
import { BrowserAgentLiteDomSummaryService } from './LiteDomSummaryService';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { vConfig } from '../VersionConfig';

const DOMAIN = 0x6020;
const TAG = 'ObservationService';

interface AgentLiteDomRectLike {
  left: number;
  top: number;
  right: number;
  bottom: number;
  width: number;
  height: number;
}

interface AgentLiteDomElementLike {
  tagName: string;
  type: string;
  ariaLabel: string;
  dataId: string;
  title: string;
  value?: string;
  placeholder?: string;
  options: string[];
  optionValues?: string[];
}

interface AgentLiteDomNodeLike {
  id?: number;
  text: string;
  include: boolean;
  area: number;
  type?: string;
  selector?: string;
  element: AgentLiteDomElementLike;
  rects: AgentLiteDomRectLike[];
  xpath?: string;
  scrollViewportWidth?: number;
  scrollViewportHeight?: number;
}

interface AgentPerceptionLike {
  currentUrl: string;
  currentTitle?: string;
  originalScreenshot: string;
  annotatedScreenshot: string;
  viewport?: {
    width: number;
    height: number;
  };
  liteDomTree: AgentLiteDomNodeLike[];
  nearbyLiteDomTree?: AgentLiteDomNodeLike[];
  liteDomSummary: string;
  hasOpenDialog?: boolean;
  dialogState?: Record<string, Object> | null;
}

export class BrowserAgentObservationService {
  static fromPerception(perception: AgentPerceptionLike): AgentObservation {
    const observation = new AgentObservation();
    observation.currentUrl = perception.currentUrl;
    observation.currentTitle = typeof perception.currentTitle === 'string' ? perception.currentTitle : '';
    observation.originalScreenshot = perception.originalScreenshot;
    observation.annotatedScreenshot = perception.annotatedScreenshot;
    observation.viewport.width = perception.viewport ? perception.viewport.width : 0;
    observation.viewport.height = perception.viewport ? perception.viewport.height : 0;
    observation.liteDomTree = BrowserAgentObservationService.cloneLiteDomTree(perception.liteDomTree);
    // 为 liteDomTree 节点分配连续 id (0..N-1)，作为 nearby 节点 id 的 offset 基准
    for (let i = 0; i < observation.liteDomTree.length; i++) {
      observation.liteDomTree[i].id = i;
    }
    if (vConfig.outside_element) {
      observation.outsideDOMTree = BrowserAgentObservationService.buildOutsideDOMTree(
        perception.nearbyLiteDomTree ?? [],
        observation.liteDomTree.length,
        observation.viewport.width,
        observation.viewport.height);
    }
    observation.hasOpenDialog = perception.hasOpenDialog === true;
    observation.dialogState = perception.dialogState ?? null;
    observation.liteDomSummary = observation.hasOpenDialog ?
      BrowserAgentDialogObservationService.build(observation.dialogState) :
      (observation.liteDomTree.length > 0 ?
        BrowserAgentLiteDomSummaryService.buildLiteDomSummary(
          observation.liteDomTree,
          AgentConstants.LITEDOM_MAX_SUMMARY_LENGTH) :
        perception.liteDomSummary);
    if (vConfig.outside_element) {
      observation.outsideDomSummary = observation.hasOpenDialog ? '' :
        BrowserAgentLiteDomSummaryService.buildOutsideDomSummary(
          observation.outsideDOMTree,
          AgentConstants.LITEDOM_MAX_SUMMARY_LENGTH,
          observation.viewport.width,
          observation.viewport.height);
    }
    return observation;
  }

  static cloneLiteDomTree(liteDomTree: AgentLiteDomNodeLike[]): AgentLiteDomNode[] {
    if (liteDomTree.length <= 0) {
      return [];
    }
    const nextLiteDomTree: AgentLiteDomNode[] = [];
    for (let i = 0; i < liteDomTree.length; i++) {
      nextLiteDomTree.push(BrowserAgentObservationService.cloneLiteDomNode(liteDomTree[i]));
    }
    return nextLiteDomTree;
  }

  static cloneOutsideDOMTree(tree: AgentOutsideDOMTree): AgentOutsideDOMTree {
    const next = new AgentOutsideDOMTree();
    next.above = BrowserAgentObservationService.cloneLiteDomTree(tree.above);
    next.below = BrowserAgentObservationService.cloneLiteDomTree(tree.below);
    next.left = BrowserAgentObservationService.cloneLiteDomTree(tree.left);
    next.right = BrowserAgentObservationService.cloneLiteDomTree(tree.right);
    BrowserAgentObservationService.populateIdDirectionMap(next);
    return next;
  }

  static flattenOutsideDOMTree(tree: AgentOutsideDOMTree): AgentLiteDomNode[] {
    const flat: AgentLiteDomNode[] = [];
    for (let i = 0; i < tree.above.length; i++) {
      flat.push(tree.above[i]);
    }
    for (let i = 0; i < tree.below.length; i++) {
      flat.push(tree.below[i]);
    }
    for (let i = 0; i < tree.left.length; i++) {
      flat.push(tree.left[i]);
    }
    for (let i = 0; i < tree.right.length; i++) {
      flat.push(tree.right[i]);
    }
    return flat;
  }

  /**
   * 摊平 outsideDOMTree 并按 node.id 升序排序。
   * 用于构造 combinedTree（liteDomTree ++ 排序后的 outside 节点），
   * 使 combinedTree[id] 对全局 id（含 outside offset 后的 id）都成立。
   */
  static flattenOutsideDOMTreeSortedById(tree: AgentOutsideDOMTree): AgentLiteDomNode[] {
    const flat = BrowserAgentObservationService.flattenOutsideDOMTree(tree);
    if (flat.length <= 1) {
      return flat;
    }
    flat.sort((a: AgentLiteDomNode, b: AgentLiteDomNode): number => a.id - b.id);
    return flat;
  }

  /**
   * 按 id 在 outsideDOMTree 四个桶里查找节点。命中返回节点，否则 null。
   * 节点数通常很少，线性扫描足够；idDirectionMap 可先确认 id 属于 outside。
   */
  static findOutsideNodeById(tree: AgentOutsideDOMTree, id: number): AgentLiteDomNode | null {
    for (let i = 0; i < tree.above.length; i++) {
      if (tree.above[i].id === id) {
        return tree.above[i];
      }
    }
    for (let i = 0; i < tree.below.length; i++) {
      if (tree.below[i].id === id) {
        return tree.below[i];
      }
    }
    for (let i = 0; i < tree.left.length; i++) {
      if (tree.left[i].id === id) {
        return tree.left[i];
      }
    }
    for (let i = 0; i < tree.right.length; i++) {
      if (tree.right[i].id === id) {
        return tree.right[i];
      }
    }
    return null;
  }

  private static buildOutsideDOMTree(
    nearbyTree: AgentLiteDomNodeLike[],
    idOffset: number,
    viewportWidth: number,
    viewportHeight: number
  ): AgentOutsideDOMTree {
    const tree = new AgentOutsideDOMTree();
    if (nearbyTree.length <= 0) {
      return tree;
    }
    if (viewportWidth <= 0 || viewportHeight <= 0) {
      hilog.warn(DOMAIN, TAG, 'viewport missing, classify all nearby nodes as below');
      for (let i = 0; i < nearbyTree.length; i++) {
        const cloned = BrowserAgentObservationService.cloneLiteDomNode(nearbyTree[i]);
        cloned.id = idOffset + i;
        tree.below.push(cloned);
        tree.idDirectionMap.set(String(cloned.id), 'below');
      }
      return tree;
    }
    for (let i = 0; i < nearbyTree.length; i++) {
      const direction = BrowserAgentObservationService.classifyOutsideDirection(
        nearbyTree[i], viewportWidth, viewportHeight);
      const cloned = BrowserAgentObservationService.cloneLiteDomNode(nearbyTree[i]);
      cloned.id = idOffset + i;
      switch (direction) {
        case 'above':
          tree.above.push(cloned);
          break;
        case 'below':
          tree.below.push(cloned);
          break;
        case 'left':
          tree.left.push(cloned);
          break;
        case 'right':
          tree.right.push(cloned);
          break;
        default:
          hilog.warn(DOMAIN, TAG, 'litedom direction not recognized: %{public}s', direction);
          break;
      }
      tree.idDirectionMap.set(String(cloned.id), direction);
    }
    return tree;
  }

  static populateIdDirectionMap(tree: AgentOutsideDOMTree): void {
    if (tree.idDirectionMap.size > 0) {
      tree.idDirectionMap.clear();
    }
    for (let i = 0; i < tree.above.length; i++) {
      tree.idDirectionMap.set(String(tree.above[i].id), 'above');
    }
    for (let i = 0; i < tree.below.length; i++) {
      tree.idDirectionMap.set(String(tree.below[i].id), 'below');
    }
    for (let i = 0; i < tree.left.length; i++) {
      tree.idDirectionMap.set(String(tree.left[i].id), 'left');
    }
    for (let i = 0; i < tree.right.length; i++) {
      tree.idDirectionMap.set(String(tree.right[i].id), 'right');
    }
  }

  /**
   * 按 rects 相对 viewport 的位置分类 nearby 节点。
   *
   * 视口坐标系：可见区域为 [0, viewportWidth) × [0, viewportHeight)。
   * - above: 节点位于视口上方（已向上滚出可视区）
   * - below: 节点位于视口下方（已向下滚出可视区）
   * - left/right: 节点位于视口左/右
   *
   * 判定策略：先算节点在四个方向“超出视口的长度”，取最大者；
   * 上下与左右出现冲突（含平局）时，按用户要求“以上下为优先”归到垂直方向。
   * 若节点完全在视口内（nearby 不应出现这种情况，防御处理），按中心点所在象限归类。
   */
  private static classifyOutsideDirection(
    node: AgentLiteDomNodeLike,
    viewportWidth: number,
    viewportHeight: number
  ): OutsideDirection {
    if (node.rects.length <= 0) {
      return 'below';
    }
    const minTop = Math.min(...node.rects.map(r => r.top));
    const maxBottom = Math.max(...node.rects.map(r => r.bottom));
    const minLeft = Math.min(...node.rects.map(r => r.left));
    const maxRight = Math.max(...node.rects.map(r => r.right));

    const aboveAmount = minTop < 0 ? -minTop : 0;
    const belowAmount = maxBottom > viewportHeight ? maxBottom - viewportHeight : 0;
    const leftAmount = minLeft < 0 ? -minLeft : 0;
    const rightAmount = maxRight > viewportWidth ? maxRight - viewportWidth : 0;

    const maxVertical = aboveAmount >= belowAmount ? aboveAmount : belowAmount;
    const maxHorizontal = leftAmount >= rightAmount ? leftAmount : rightAmount;

    if (maxVertical > 0 || maxHorizontal > 0) {
      // 上下与左右冲突时（含平局）优先归到垂直方向
      if (maxVertical >= maxHorizontal) {
        return aboveAmount > belowAmount ? 'above' : 'below';
      }
      return leftAmount > rightAmount ? 'left' : 'right';
    }

    // 节点完全在视口内：按中心点象限归类，仍以垂直优先
    const centerY = (minTop + maxBottom) / 2;
    if (centerY < viewportHeight / 2) {
      return 'above';
    }
    if (centerY > viewportHeight / 2) {
      return 'below';
    }
    const centerX = (minLeft + maxRight) / 2;
    return centerX <= viewportWidth / 2 ? 'left' : 'right';
  }

  private static cloneLiteDomNode(node: AgentLiteDomNodeLike): AgentLiteDomNode {
    const nextNode = new AgentLiteDomNode();
    nextNode.id = typeof node.id === 'number' ? node.id : 0;
    nextNode.text = node.text;
    nextNode.include = node.include;
    nextNode.area = node.area;
    nextNode.scrollViewportWidth = typeof node.scrollViewportWidth === 'number' ? node.scrollViewportWidth : 0;
    nextNode.scrollViewportHeight = typeof node.scrollViewportHeight === 'number' ? node.scrollViewportHeight : 0;
    nextNode.type = typeof node.type === 'string' ? node.type : '';
    nextNode.selector = typeof node.selector === 'string' ? node.selector : '';
    nextNode.element.tagName = node.element.tagName;
    nextNode.element.type = node.element.type;
    nextNode.element.ariaLabel = node.element.ariaLabel;
    nextNode.element.dataId = node.element.dataId;
    nextNode.element.title = node.element.title;
    nextNode.element.value = typeof node.element.value === 'string' ? node.element.value : '';
    nextNode.element.placeholder = typeof node.element.placeholder === 'string' ? node.element.placeholder : '';
    nextNode.element.options = node.element.options.slice();
    nextNode.element.optionValues = node.element.optionValues?.slice();
    nextNode.xpath = node.xpath;
    for (let i = 0; i < node.rects.length; i++) {
      const rect = node.rects[i];
      const nextRect = new AgentLiteDomRect();
      nextRect.left = rect.left;
      nextRect.top = rect.top;
      nextRect.right = rect.right;
      nextRect.bottom = rect.bottom;
      nextRect.width = rect.width;
      nextRect.height = rect.height;
      nextNode.rects.push(nextRect);
    }
    return nextNode;
  }
}
