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

import {
  AgentLiteDomNode,
  AgentLiteDomRect,
  AgentObservation,
  BrowserAgentAction
} from '../../core/AgentTypes';
import { HtmlParser, Node } from '../../core/HtmlParser';
import { BrowserAgentUtils } from '../../core/Utils';

export const SCROLL_TO_VIEW_ACTION_NAME: string = 'SCROLL_TO_VIEW';
export const SCROLL_TO_VIEW_ACTION_DESCRIPTION: string =
  '- SCROLL_TO_VIEW: Automatically scrolls a scrollable element to reveal a target option (e.g., a year, a city, etc.). This is usually more efficient than SCROLL.' +
  'Pass the exact option for the currently opened picker, not the complete range target. Check the text of the scrollable element for options.\n' +
  '  Format: SCROLL_TO_VIEW [scrollableElementID] [exactOption]';

class PickerScrollBox {
  left: number = 0;
  top: number = 0;
  right: number = 0;
  bottom: number = 0;
  width: number = 0;
  height: number = 0;
}

class PickerScrollPoint {
  node: AgentLiteDomNode = new AgentLiteDomNode();
  box: PickerScrollBox = new PickerScrollBox();
  label: string = '';
}

class PickerScrollSequence {
  axis: string = '';
  pitch: number = 0;
  points: PickerScrollPoint[] = [];
}

class PickerHtmlAlignment {
  entries: string[] = [];
  visibleStart: number = -1;
  orientation: number = 1;
}

export class PickerScrollPlan {
  action: BrowserAgentAction | null = null;
  distance: number = 0;
}

/**
 * Tool-local builder for SCROLL_TO_VIEW. It intentionally emits the existing SCROLL
 * runtime action so no shared action/transport surface is changed.
 */
export class PickerScrollToViewBuilder {
  private static readonly MIN_SEQUENCE_POINTS: number = 3;
  private static readonly MIN_EFFECTIVE_SCROLL_PX: number = 1;
  private static readonly SPACING_ABSOLUTE_TOLERANCE_PX: number = 3;
  private static readonly SPACING_RELATIVE_TOLERANCE: number = 0.12;
  private static readonly AXIS_SIZE_RELATIVE_TOLERANCE: number = 0.10;

  // Accept only an exactOption that belongs to the tool Exact Target.
  // Text: Exact Target "province: Guangdong" allows "Guangdong", rejects "Zhejiang".
  // Number: Exact Target "start date: 1980, end date: 2020" allows "1980", rejects "1970".
  static validateActionTarget(toolTarget: string, actionTarget: string): string | Error {
    const normalizedToolTarget = PickerScrollToViewBuilder.normalizeLabel(toolTarget);
    const normalizedActionTarget = PickerScrollToViewBuilder.normalizeLabel(actionTarget);
    if (normalizedActionTarget.length <= 0) {
      return new Error('SCROLL_TO_VIEW rejected: exactOption is empty.');
    }
    if (normalizedToolTarget.length <= 0) {
      return new Error('SCROLL_TO_VIEW rejected: the tool Exact Target is empty.');
    }
    const numericActionTarget = PickerScrollToViewBuilder.parseExactNumber(normalizedActionTarget);
    if (numericActionTarget !== null) {
      return PickerScrollToViewBuilder.matchNumericExactOption(
        normalizedToolTarget, numericActionTarget, actionTarget, normalizedActionTarget
      );
    }
    if (!normalizedToolTarget.includes(normalizedActionTarget)) {
      return new Error(
        `SCROLL_TO_VIEW rejected: exactOption "${actionTarget}" is not part of Exact Target.`
      );
    }
    return normalizedActionTarget;
  }

  static buildAction(
    target: string,
    containerId: number,
    observation: AgentObservation,
    reason: string = '精确滚动到目标选项'
  ): BrowserAgentAction | Error {
    const plan = PickerScrollToViewBuilder.buildPlan(target, containerId, observation, reason);
    if (plan instanceof Error) {
      return plan;
    }
    return plan.action ?? new Error('SCROLL_TO_VIEW rejected: the exact target is already visible; click it directly.');
  }

  static buildPlan(
    target: string,
    containerId: number,
    observation: AgentObservation,
    reason: string
  ): PickerScrollPlan | Error {
    // 1. Find the container element in the current LiteDOM.
    const container = PickerScrollToViewBuilder.findCurrentNodeById(observation.liteDomTree, containerId);
    if (!container) {
      return new Error(`SCROLL_TO_VIEW rejected: container element ${containerId} is not in the current LiteDOM.`);
    }
    
    // 2. Check if the container element is scrollable.
    if (!PickerScrollToViewBuilder.isScrollableNode(container)) {
      return new Error(`SCROLL_TO_VIEW rejected: element ${containerId} is not scrollable.`);
    }

    // 3. Calculate the bounding box of the container element.
    const containerBox = PickerScrollToViewBuilder.unionBox(container.rects);
    if (!containerBox || containerBox.width <= 0 || containerBox.height <= 0) {
      return new Error(`SCROLL_TO_VIEW rejected: scrollable element ${containerId} has no usable box.`);
    }

    // 4. Collect labeled descendant options, including off-screen ones.
    //    Example: visible years 2026/2025/2024 plus every outside descendant, such as 2024 to 1980.
    const candidates = PickerScrollToViewBuilder.collectDescendantPoints(observation, container, containerBox);
    // 5. Keep options whose center is inside the scroll container.
    //    Example: 2026/2025/2024 stay; 1980 below the viewport is dropped.
    const visible = PickerScrollToViewBuilder.filterVisiblePoints(candidates, containerBox);
    // 6. Find one evenly spaced option axis from the visible boxes.
    //    Example: 2026, 2025, 2024 at 50px pitch on the vertical axis.
    const sequence = PickerScrollToViewBuilder.findRegularSequence(visible);
    if (sequence instanceof Error) {
      return sequence;
    }

    const normalizedTarget = PickerScrollToViewBuilder.normalizeLabel(target);
    if (normalizedTarget.length <= 0) {
      return new Error('SCROLL_TO_VIEW rejected: Exact Target is empty.');
    }
    // 6b. Skip scrolling when the unique exact target is already on-screen and fully visible.
    //     Example: 2024 is the only match, it is in liteDomTree, and its box sits fully inside the container.
    const matches = PickerScrollToViewBuilder.collectExactMatches(candidates, normalizedTarget);
    if (matches.length === 1 && observation.liteDomTree.some((node: AgentLiteDomNode) =>
      PickerScrollToViewBuilder.sameNode(node, matches[0].node)) &&
      PickerScrollToViewBuilder.isFullyVisibleOption(matches[0], sequence, containerBox)) {
      return new PickerScrollPlan(); // Return an empty plan to indicate no scrolling is needed.
    }
    // 7. Resolve the target center.
    //    Example: guangdong at y=1010.
    const targetCenter = PickerScrollToViewBuilder.resolveTargetCenter(
      normalizedTarget, candidates, sequence, container, observation
    );
    if (targetCenter instanceof Error) {
      return targetCenter;
    }
    // 8. Compose the scroll action.
    //    Example: SCROLL [DOWN] [1010] [1.0].
    const action = PickerScrollToViewBuilder.composeScrollAction(
      container, containerBox, sequence, targetCenter, containerId, reason
    );
    if (action instanceof Error) {
      return action;
    }
    const plan = new PickerScrollPlan();
    plan.action = action;
    const visibleCenter = sequence.axis === 'vertical' ?
      (containerBox.top + containerBox.bottom) / 2 : (containerBox.left + containerBox.right) / 2;
    plan.distance = Math.abs(targetCenter - visibleCenter);
    return plan;
  }

  private static isFullyVisibleOption(
    point: PickerScrollPoint, sequence: PickerScrollSequence, containerBox: PickerScrollBox
  ): boolean {
    const box = point.box;
    const reference = sequence.points[0].box;
    return PickerScrollToViewBuilder.areAligned(sequence.points[0], point, sequence.axis) &&
      box.width >= reference.width - 1 && box.height >= reference.height - 1 &&
      box.top >= containerBox.top && box.bottom <= containerBox.bottom &&
      box.left >= containerBox.left && box.right <= containerBox.right;
  }

  private static matchNumericExactOption(
    normalizedToolTarget: string,
    numericActionTarget: number,
    actionTarget: string,
    normalizedActionTarget: string
  ): string | Error {
    const numericParts = normalizedToolTarget.match(/\d+(?:\.\d+)?/g) ?? [];
    for (let i = 0; i < numericParts.length; i++) {
      if (Number(numericParts[i]) === numericActionTarget) {
        return normalizedActionTarget;
      }
    }
    return new Error(
      `SCROLL_TO_VIEW rejected: exactOption "${actionTarget}" is not a numeric component of Exact Target.`
    );
  }

  private static composeScrollAction(
    container: AgentLiteDomNode,
    containerBox: PickerScrollBox,
    sequence: PickerScrollSequence,
    targetCenter: number,
    containerId: number,
    reason: string
  ): BrowserAgentAction | Error {
    // 9. Calculate the scroll direction and ratio.
    const vertical = sequence.axis === 'vertical';
    
    // Place the target at the center of the visible part of the picker.
    const visibleCenter = vertical ?
    (containerBox.top + containerBox.bottom) / 2 :
    (containerBox.left + containerBox.right) / 2;

    // Match the clientHeight/clientWidth used by the extension's SCROLL.
    const scrollViewportSize = vertical ?
      container.scrollViewportHeight :
      container.scrollViewportWidth;

    if (!Number.isFinite(scrollViewportSize) || scrollViewportSize <= 0) {
      return new Error(
        'SCROLL_TO_VIEW rejected: full scroll-container dimensions are unavailable. ' +
        'Use ordinary SCROLL on the same container.'
      );
    }

    const deltaPx = targetCenter - visibleCenter;
    if (!Number.isFinite(deltaPx)) {
      return new Error('SCROLL_TO_VIEW rejected: the computed distance is invalid.');
    }
    if (Math.abs(deltaPx) < PickerScrollToViewBuilder.MIN_EFFECTIVE_SCROLL_PX) {
      return new Error(
        'SCROLL_TO_VIEW rejected: the target is already centered; click it directly.'
      );
    }

    const ratio = Math.abs(deltaPx) / scrollViewportSize;
    if (!Number.isFinite(ratio) || ratio <= 0) {
      return new Error('SCROLL_TO_VIEW rejected: the computed scroll ratio is invalid.');
    }

    // 10. Create the scroll action.
    //    Example: SCROLL [DOWN] [1010] [1.0].
    return PickerScrollToViewBuilder.createScrollAction(
      PickerScrollToViewBuilder.resolveScrollDirection(vertical, deltaPx),
      containerId,
      ratio,
      container,
      reason
    );
  }

  private static resolveScrollDirection(vertical: boolean, deltaPx: number): string {
    if (vertical) {
      return deltaPx > 0 ? 'DOWN' : 'UP';
    }
    return deltaPx > 0 ? 'RIGHT' : 'LEFT';
  }

  private static createScrollAction(
    direction: string,
    containerId: number,
    ratio: number,
    container: AgentLiteDomNode,
    reason: string
  ): BrowserAgentAction {
    const ratioText = PickerScrollToViewBuilder.formatRatio(ratio);
    const action = new BrowserAgentAction();
    action.action = `SCROLL [${direction}] [${containerId}] [${ratioText}]`;
    action.actionType = 'SCROLL';
    action.params = [direction, containerId.toString(), ratioText];
    action.element = containerId;
    action.requiresLoadingWait = true;
    action.reason = reason;
    return action;
  }

  private static findCurrentNodeById(nodes: AgentLiteDomNode[], id: number): AgentLiteDomNode | null {
    for (let i = 0; i < nodes.length; i++) {
      if (nodes[i].id === id) {
        return nodes[i];
      }
    }
    return null;
  }

  private static isScrollableNode(node: AgentLiteDomNode): boolean {
    return node.type.trim().toLowerCase() === 'scrollable' ||
      node.element.type.trim().toLowerCase() === 'scrollable';
  }

  // Collect labeled picker options under the container, including off-screen LiteDOM nodes.
  // Example: visible years 2026/2025/2024 plus every outsideDOM descendant, such as 2000 and 1980.
  private static collectDescendantPoints(
    observation: AgentObservation,
    container: AgentLiteDomNode,
    containerBox: PickerScrollBox
  ): PickerScrollPoint[] {
    const nodes: AgentLiteDomNode[] = [];
    // Collect LiteDOM plus nearby outsideDOM only. HTML is not used here: it has
    // text and XPath but no boxes, so it cannot contribute centers or pitch.
    // Example: outsideDOM may give 2000 a real rect; HTML "2000" cannot.
    PickerScrollToViewBuilder.appendUniqueNodes(nodes, observation.liteDomTree);
    PickerScrollToViewBuilder.appendUniqueNodes(nodes, observation.outsideDOMTree.above);
    PickerScrollToViewBuilder.appendUniqueNodes(nodes, observation.outsideDOMTree.below);
    PickerScrollToViewBuilder.appendUniqueNodes(nodes, observation.outsideDOMTree.left);
    PickerScrollToViewBuilder.appendUniqueNodes(nodes, observation.outsideDOMTree.right);

    // Select descendant nodes that are inside the scrollable container.
    const points: PickerScrollPoint[] = [];
    for (let i = 0; i < nodes.length; i++) {
      const point = PickerScrollToViewBuilder.toDescendantPoint(nodes[i], container, containerBox);
      if (point) {
        points.push(point);
      }
    }
    return points;
  }

  private static toDescendantPoint(
    node: AgentLiteDomNode,
    container: AgentLiteDomNode,
    containerBox: PickerScrollBox
  ): PickerScrollPoint | null {
    // Skip the container node itself and scrollable nodes.
    if (node.id === container.id || PickerScrollToViewBuilder.isScrollableNode(node)) {
      return null;
    }
    // Calculate the bounding box of the node.
    const box = PickerScrollToViewBuilder.unionBox(node.rects);
    // Check if the node is a descendant of the container.
    if (!box || !PickerScrollToViewBuilder.isDescendant(node, container, box, containerBox)) {
      return null;
    }
    const label = PickerScrollToViewBuilder.resolveNodeLabel(node);
    if (label.length <= 0) {
      return null;
    }
    const point = new PickerScrollPoint();
    point.node = node;
    point.box = box;
    point.label = label;
    return point;
  }

  private static appendUniqueNodes(target: AgentLiteDomNode[], source: AgentLiteDomNode[]): void {
    for (let i = 0; i < source.length; i++) {
      if (!PickerScrollToViewBuilder.containsNode(target, source[i])) {
        target.push(source[i]);
      }
    }
  }

  private static containsNode(nodes: AgentLiteDomNode[], candidate: AgentLiteDomNode): boolean {
    for (let i = 0; i < nodes.length; i++) {
      if (PickerScrollToViewBuilder.sameNode(nodes[i], candidate)) {
        return true;
      }
    }
    return false;
  }

  private static sameNode(left: AgentLiteDomNode, right: AgentLiteDomNode): boolean {
    if (left.xpath.length > 0 && right.xpath.length > 0) {
      return left.xpath === right.xpath;
    }
    if (left.selector.length > 0 && right.selector.length > 0) {
      return left.selector === right.selector;
    }
    return left.id === right.id;
  }

  private static isDescendant(
    node: AgentLiteDomNode,
    container: AgentLiteDomNode,
    nodeBox: PickerScrollBox,
    containerBox: PickerScrollBox
  ): boolean {
    // Check if the node is a descendant of the container.
    // 1. Check if the node is a descendant of the container by XPath.
    const containerXpath = container.xpath.trim();
    const nodeXpath = node.xpath.trim();
    if (containerXpath.length > 0 && nodeXpath.length > 0) {
      return nodeXpath.startsWith(`${containerXpath}/`);
    }
    // 2. Check if the node is a descendant of the container by center point.
    const centerX = (nodeBox.left + nodeBox.right) / 2;
    const centerY = (nodeBox.top + nodeBox.bottom) / 2;
    return centerX >= containerBox.left && centerX <= containerBox.right &&
      centerY >= containerBox.top && centerY <= containerBox.bottom;
  }

  // Keep only options whose center is inside the scroll container.
  // Example: 2026/2025/2024 stay; 1980 below the viewport is dropped.
  private static filterVisiblePoints(
    points: PickerScrollPoint[],
    containerBox: PickerScrollBox
  ): PickerScrollPoint[] {
    const visible: PickerScrollPoint[] = [];
    for (let i = 0; i < points.length; i++) {
      if (PickerScrollToViewBuilder.isPointInsideBox(points[i], containerBox)) {
        visible.push(points[i]);
      }
    }
    return visible;
  }

  private static isPointInsideBox(point: PickerScrollPoint, containerBox: PickerScrollBox): boolean {
    const centerX = PickerScrollToViewBuilder.center(point, 'horizontal');
    const centerY = PickerScrollToViewBuilder.center(point, 'vertical');
    return centerX >= containerBox.left && centerX <= containerBox.right &&
      centerY >= containerBox.top && centerY <= containerBox.bottom;
  }

  // Pick one evenly spaced option axis. Need at least three aligned boxes.
  // Example: 2026, 2025, 2024 at 50px pitch on the vertical axis.
  private static findRegularSequence(
    points: PickerScrollPoint[]
  ): PickerScrollSequence | Error {
    if (points.length < PickerScrollToViewBuilder.MIN_SEQUENCE_POINTS) {
      return new Error('SCROLL_TO_VIEW rejected: fewer than three visible descendant option boxes were found.');
    }
    // 1. Find the best vertical sequence.
    const vertical = PickerScrollToViewBuilder.findBestAxisSequence(points, 'vertical');
    // 2. Find the best horizontal sequence.
    const horizontal = PickerScrollToViewBuilder.findBestAxisSequence(points, 'horizontal');
    // 3. Check if the sequences are valid.
    if (vertical.points.length < PickerScrollToViewBuilder.MIN_SEQUENCE_POINTS &&
      horizontal.points.length < PickerScrollToViewBuilder.MIN_SEQUENCE_POINTS) {
      return new Error('SCROLL_TO_VIEW rejected: descendant option coordinates are not an arithmetic sequence.');
    }
    // 4. Check if the sequences are ambiguous.
    if (vertical.points.length === horizontal.points.length &&
      vertical.points.length >= PickerScrollToViewBuilder.MIN_SEQUENCE_POINTS) {
      return new Error('SCROLL_TO_VIEW rejected: the ordered option axis is ambiguous.');
    }
    return vertical.points.length > horizontal.points.length ? vertical : horizontal;
  }

  private static findBestAxisSequence(
    source: PickerScrollPoint[],
    axis: string
  ): PickerScrollSequence {
    const points = source.slice();
    // Sort points by their center on the axis, vertically or horizontally.
    points.sort((left: PickerScrollPoint, right: PickerScrollPoint): number =>
      PickerScrollToViewBuilder.center(left, axis) - PickerScrollToViewBuilder.center(right, axis));
    const best = new PickerScrollSequence();
    best.axis = axis;
    // Try each pair as a pitch seed and grow the longest regular run. Need 3 points,
    // so the last start is length-3. Example: [2026, 2025, 2024] only starts at 2026.
    // The loop iterates from 0 to length-3, inclusive, to make the robustness of the algorithm.
    for (let start = 0; start + 2 < points.length; start++) {
      PickerScrollToViewBuilder.tryExtendSequence(points, start, axis, best);
    }
    return best;
  }

  // Try each pair as a pitch seed and grow the longest regular run. Need 3 points,
  // so the last start is length-3. Example: [2026, 2025, 2024] only starts at 2026.
  // The loop iterates from 0 to length-3, inclusive, to make the robustness of the algorithm.
  private static tryExtendSequence(
    points: PickerScrollPoint[],
    start: number,
    axis: string,
    best: PickerScrollSequence
  ): void {
    // Read the pitch between the first two points.
    const pitch = PickerScrollToViewBuilder.readPitch(points, start, axis);
    // If the pitch is invalid, skip this start.
    if (pitch === null) {
      return;
    }
    // Collect the longest regular run starting at this start.
    const run = PickerScrollToViewBuilder.collectRegularRun(points, start, pitch, axis);
    // If the run is longer than the best so far, update the best.
    if (run.length > best.points.length) {
      best.points = run;
      best.pitch = pitch;
    }
  }

  private static readPitch(
    points: PickerScrollPoint[],
    start: number,
    axis: string
  ): number | null {
    const pitch = PickerScrollToViewBuilder.center(points[start + 1], axis) -
      PickerScrollToViewBuilder.center(points[start], axis);
    if (!Number.isFinite(pitch) || pitch <= PickerScrollToViewBuilder.MIN_EFFECTIVE_SCROLL_PX ||
      !PickerScrollToViewBuilder.areAligned(points[start], points[start + 1], axis)) {
      return null;
    }
    return pitch;
  }

  // Collect the longest regular run starting at this start.
  private static collectRegularRun(
    points: PickerScrollPoint[],
    start: number,
    pitch: number,
    axis: string
  ): PickerScrollPoint[] {
    const run: PickerScrollPoint[] = [points[start], points[start + 1]];
    // Grow the run by adding points that continue the regular sequence.
    for (let i = start + 2; i < points.length; i++) {
      if (!PickerScrollToViewBuilder.continuesRegularRun(run, points[i], pitch, axis)) {
        break;
      }
      run.push(points[i]);
    }
    return run;
  }

  // Check if the candidate point continues the regular sequence.
  private static continuesRegularRun(
    run: PickerScrollPoint[],
    candidate: PickerScrollPoint,
    pitch: number,
    axis: string
  ): boolean {
    const previous = run[run.length - 1];
    const gap = PickerScrollToViewBuilder.center(candidate, axis) -
      PickerScrollToViewBuilder.center(previous, axis);
    // Check if the gap between the previous point and the candidate point is close to the pitch.
    // Also check if the candidate point is aligned with the first point of the run.
    return PickerScrollToViewBuilder.spacingMatches(gap, pitch) &&
      PickerScrollToViewBuilder.areAligned(run[0], candidate, axis);
  }

  // Same column (vertical) or same row (horizontal), and similar box size.
  // Example: year cells 40px tall at x=170 align; a 40px city cell at x=360 does not.
  private static areAligned(
    anchor: PickerScrollPoint,
    candidate: PickerScrollPoint,
    axis: string
  ): boolean {
    const vertical = axis === 'vertical';
    const anchorAxisSize = vertical ? anchor.box.height : anchor.box.width;
    const candidateAxisSize = vertical ? candidate.box.height : candidate.box.width;
    const maxAxisSize = Math.max(anchorAxisSize, candidateAxisSize);
    // 1. Size: heights must be close on a vertical wheel (widths on a horizontal strip).
    //    40px vs 42px ok; 40px vs 80px (header) rejected (tolerance 10%).
    if (maxAxisSize <= 0 ||
      Math.abs(anchorAxisSize - candidateAxisSize) / maxAxisSize >
      PickerScrollToViewBuilder.AXIS_SIZE_RELATIVE_TOLERANCE) {
      return false;
    }
    const anchorCrossCenter = vertical ?
      (anchor.box.left + anchor.box.right) / 2 :
      (anchor.box.top + anchor.box.bottom) / 2;
    const candidateCrossCenter = vertical ?
      (candidate.box.left + candidate.box.right) / 2 :
      (candidate.box.top + candidate.box.bottom) / 2;
    // 2. Cross-axis is strict: same column (x) or same row (y), 3px only for LiteDOM rounding.
    //    x=170 vs x=172 ok; x=170 vs x=175 is a different column.
    return Math.abs(anchorCrossCenter - candidateCrossCenter) <=
      PickerScrollToViewBuilder.SPACING_ABSOLUTE_TOLERANCE_PX;
  }

  // Check if the actual spacing matches the expected spacing.
  private static spacingMatches(actual: number, expected: number): boolean {
    const tolerance = Math.max(
      PickerScrollToViewBuilder.SPACING_ABSOLUTE_TOLERANCE_PX,
      Math.abs(expected) * PickerScrollToViewBuilder.SPACING_RELATIVE_TOLERANCE
    );
    return actual > 0 && Math.abs(actual - expected) <= tolerance;
  }

  // Visible boxes give axis and pitch, but the exact option is often off-screen and
  // has no painted cell. This function proves one axis coordinate for that option so
  // composeScrollAction can emit one SCROLL. Example: visible 2026/2025/2024, target 1980.
  // Input:  target = "guangdong"
  // Output: the center of the target option. Example: guangdong at y=1010.
  private static resolveTargetCenter(
    target: string,
    candidates: PickerScrollPoint[],
    sequence: PickerScrollSequence,
    container: AgentLiteDomNode,
    observation: AgentObservation
  ): number | Error {
    // 1. Direct LiteDOM box for the target, including nearby outsideDOM. Example: 1980 still has a rect.
    const directMatches = PickerScrollToViewBuilder.collectExactMatches(candidates, target);
    if (directMatches.length > 1) {
      return new Error('SCROLL_TO_VIEW rejected: multiple LiteDOM options exactly match the target.');
    }
    if (directMatches.length === 1) {
      const point = directMatches[0];
      const reference = sequence.points[0].box;
      // Use the painted LiteDOM center only when the box is a full option cell.
      // Example: 1980 below the viewport still has a complete 50x40 rect → scroll to its center.
      // A clipped cell (peeking at the container edge) is shorter/narrower, so its center
      // shifts toward the visible fragment and would undershoot. Fall through to HTML
      // row projection to recover the unclipped row center.
      // Example: 2024 clipped to 50x12 at the bottom edge → do not use that box.
      if (point.box.width >= reference.width - 1 && point.box.height >= reference.height - 1) {
        return PickerScrollToViewBuilder.resolveDirectMatchCenter(point, sequence);
      }
    }

    // 2. HTML row list has no boxes; project by index. Example: zeta is 3 rows after delta.
    // can not find from the LiteDOM and outsideDOM, use this method.
    const htmlCenter = PickerScrollToViewBuilder.resolveHtmlTargetCenter(target, sequence, container, observation);
    if (!(htmlCenter instanceof Error)) {
      return htmlCenter;
    }
    return new Error(
      `SCROLL_TO_VIEW rejected: target position is not provable from LiteDOM boxes, ` +
      `or an unambiguous ordered HTML list. HTML check: ${htmlCenter.message}`
    );
  }

  private static collectExactMatches(candidates: PickerScrollPoint[], target: string): PickerScrollPoint[] {
    const matches: PickerScrollPoint[] = [];
    for (let i = 0; i < candidates.length; i++) {
      if (candidates[i].label === target) {
        matches.push(candidates[i]);
      }
    }
    return matches;
  }

  // Use the unique LiteDOM box for this label. Example: outsideDOM "zhejiang" at y=1010.
  private static resolveDirectMatchCenter(
    point: PickerScrollPoint,
    sequence: PickerScrollSequence
  ): number | Error {
    // Same column/row as the visible run. Example: year x=170, not a city at x=360.
    if (!PickerScrollToViewBuilder.areAligned(sequence.points[0], point, sequence.axis)) {
      return new Error('SCROLL_TO_VIEW rejected: the target LiteDOM box is not aligned with the option sequence.');
    }
    // Center must sit on N * pitch from the first visible cell. Example: (1010-130)/50 is an integer.
    if (!PickerScrollToViewBuilder.isOnSequenceGrid(point, sequence)) {
      return new Error('SCROLL_TO_VIEW rejected: the target LiteDOM box is not on the option coordinate sequence.');
    }
    return PickerScrollToViewBuilder.center(point, sequence.axis);
  }

  private static isOnSequenceGrid(point: PickerScrollPoint, sequence: PickerScrollSequence): boolean {
    const directCenter = PickerScrollToViewBuilder.center(point, sequence.axis);
    const gridOffset =
      (directCenter - PickerScrollToViewBuilder.center(sequence.points[0], sequence.axis)) / sequence.pitch;
    const nearestGridOffset = Math.round(gridOffset);
    const gridError = Math.abs(gridOffset - nearestGridOffset) * sequence.pitch;
    const gridTolerance = Math.max(
      PickerScrollToViewBuilder.SPACING_ABSOLUTE_TOLERANCE_PX,
      sequence.pitch * PickerScrollToViewBuilder.SPACING_RELATIVE_TOLERANCE
    );
    return Number.isFinite(gridOffset) && gridError <= gridTolerance;
  }

  // Project a missing numeric option from the visible value step. No LiteDOM box required.
  // Example: visible 2026/2025/2024 (step -1, pitch 50) → 1980 is 46 steps → y = center(2026) + 46*50.
  private static resolveNumericTargetCenter(
    target: string,
    sequence: PickerScrollSequence
  ): number | Error {
    const targetValue = PickerScrollToViewBuilder.parseExactNumber(target);
    if (targetValue === null) {
      return new Error('target is not a plain number.');
    }
    const values = PickerScrollToViewBuilder.collectPlainNumbers(sequence.points);
    if (values instanceof Error) {
      return values;
    }
    // Value step from the first two visible labels. Example: 2025 - 2026 = -1.
    const valueStep = values[1] - values[0];
    if (!Number.isFinite(valueStep) || Math.abs(valueStep) <= Number.EPSILON) {
      return new Error('visible numeric values have no non-zero step.');
    }
    if (!PickerScrollToViewBuilder.hasConstantStep(values, valueStep)) {
      return new Error('visible numeric values are not an arithmetic sequence.');
    }
    // Target must land on that step. Example: (1980-2026)/(-1) = 46; 1979.5 would fail.
    const targetOffset = (targetValue - values[0]) / valueStep;
    const roundedOffset = Math.round(targetOffset);
    if (!Number.isFinite(targetOffset) || Math.abs(targetOffset - roundedOffset) > 1e-6) {
      return new Error('the numeric target does not land on the visible value step.');
    }
    return PickerScrollToViewBuilder.center(sequence.points[0], sequence.axis) +
      roundedOffset * sequence.pitch;
  }

  private static collectPlainNumbers(points: PickerScrollPoint[]): number[] | Error {
    const values: number[] = [];
    for (let i = 0; i < points.length; i++) {
      const value = PickerScrollToViewBuilder.parseExactNumber(points[i].label);
      if (value === null) {
        return new Error('visible option labels are not all plain numbers.');
      }
      values.push(value);
    }
    return values;
  }

  private static hasConstantStep(values: number[], valueStep: number): boolean {
    const valueTolerance = Math.max(1e-9, Math.abs(valueStep) * 1e-6);
    for (let i = 2; i < values.length; i++) {
      if (Math.abs((values[i] - values[i - 1]) - valueStep) > valueTolerance) {
        return false;
      }
    }
    return true;
  }

  // Project a missing text (or number) option from the HTML row list. HTML has no boxes;
  // XPath only joins visible LiteDOM cells to rows, then index * pitch gives the center.
  // Example: visible beta/gamma/delta align to HTML; zeta is 3 rows after delta.
  // can not find from the LiteDOM and outsideDOM, and not a number, use this method.
  private static resolveHtmlTargetCenter(
    target: string,
    sequence: PickerScrollSequence,
    container: AgentLiteDomNode,
    observation: AgentObservation
  ): number | Error {
    if (observation.html.trim().length <= 0 || container.xpath.trim().length <= 0) {
      return new Error('HTML or the container XPath is unavailable.');
    }
    const parser = PickerScrollToViewBuilder.parseHtmlDocument(observation.html);
    if (parser instanceof Error) {
      return parser;
    }
    const containerNode = parser.getNodeFromXpath(container.xpath);
    if (!containerNode) {
      return new Error('the scroll container XPath is absent from HTML.');
    }
    // Map each visible option XPath into HTML. Example: /div[1]/div[2] → the "beta" node.
    const visibleNodes = PickerScrollToViewBuilder.collectVisibleHtmlNodes(sequence, parser, containerNode);
    if (visibleNodes instanceof Error) {
      return visibleNodes;
    }
    // Shared parent of those rows, still under the scroller. Example: the <div> that wraps all options.
    const common = PickerScrollToViewBuilder.findNearestCommonAncestor(visibleNodes, containerNode);
    if (!common) {
      return new Error('visible option HTML nodes have no usable common ancestor.');
    }
    // One child per visible option, same tag. Example: three sibling <div> rows, not one shared cell.
    const branches = PickerScrollToViewBuilder.collectHtmlBranches(visibleNodes, common);
    if (branches instanceof Error) {
      return branches;
    }
    // Read every row of that tag and lock the unique place the visible labels fit.
    const alignment = PickerScrollToViewBuilder.alignHtmlEntries(
      PickerScrollToViewBuilder.collectHtmlRowEntries(common, branches[0].tag_name),
      sequence.points
    );
    if (alignment instanceof Error) {
      return alignment;
    }
    return PickerScrollToViewBuilder.projectHtmlTargetCenter(target, alignment, sequence);
  }

  private static parseHtmlDocument(html: string): HtmlParser | Error {
    try {
      return new HtmlParser(html, false);
    } catch (error) {
      return new Error(`HTML parsing failed: ${BrowserAgentUtils.stringifyError(error as Object)}`);
    }
  }

  private static collectVisibleHtmlNodes(
    sequence: PickerScrollSequence,
    parser: HtmlParser,
    containerNode: Node
  ): Node[] | Error {
    const visibleNodes: Node[] = [];
    for (let i = 0; i < sequence.points.length; i++) {
      const node = parser.getNodeFromXpath(sequence.points[i].node.xpath);
      if (!node || !PickerScrollToViewBuilder.isHtmlAncestor(containerNode, node)) {
        return new Error('a visible option XPath cannot be aligned to the HTML container.');
      }
      visibleNodes.push(node);
    }
    return visibleNodes;
  }

  // Map each visible HTML node to its row under the shared parent. Example: span "beta"
  // climbs to sibling <div> rows [row0, row1, row2], not to the container itself.
  private static collectHtmlBranches(visibleNodes: Node[], common: Node): Node[] | Error {
    const branches: Node[] = [];
    for (let i = 0; i < visibleNodes.length; i++) {
      // get the direct child of the common node.
      const branch = PickerScrollToViewBuilder.directChildUnder(common, visibleNodes[i]);
      if (!branch) {
        return new Error('visible option HTML nodes do not map to repeated row branches.');
      }
      // Each visible option must own its own row. Reject if two labels climb to the
      // same child, e.g. <li>beta gamma</li> where both texts share one <li>.
      if (PickerScrollToViewBuilder.containsBranch(branches, branch)) {
        return new Error('multiple visible options map to the same HTML row.');
      }
      branches.push(branch);
    }
    // Rows must be the same tag so later we can collect every sibling of that tag.
    // Example: all <div>, not a mix of <div> and <span>.
    if (!PickerScrollToViewBuilder.shareRowTag(branches, branches[0].tag_name)) {
      return new Error('visible option HTML rows do not share one structure.');
    }
    return branches;
  }

  private static containsBranch(branches: Node[], branch: Node): boolean {
    for (let i = 0; i < branches.length; i++) {
      if (branches[i] === branch) {
        return true;
      }
    }
    return false;
  }

  private static shareRowTag(branches: Node[], rowTag: string): boolean {
    for (let i = 1; i < branches.length; i++) {
      if (branches[i].tag_name !== rowTag) {
        return false;
      }
    }
    return true;
  }

  // Collect every sibling of the row tag, then collect the text of each.
  // Example: <div>alpha beta gamma delta zeta</div> → ["alpha", "beta", "gamma", "delta", "zeta"].
  private static collectHtmlRowEntries(common: Node, rowTag: string): string[] {
    const entries: string[] = [];
    for (let i = 0; i < common.children.length; i++) {
      if (common.children[i].tag_name !== rowTag) {
        continue;
      }
      const text = PickerScrollToViewBuilder.collectHtmlText(common.children[i]);
      if (text.length > 0) {
        entries.push(text);
      }
    }
    return entries;
  }

  // Turn the HTML row of `target` into one axis coordinate using the visible pitch.
  // Input:  target = "zeta"
  //         alignment = { entries: [alpha, beta, gamma, delta, zeta], visibleStart: 1, orientation: +1 }
  //         sequence.points[0] = beta at y=130, pitch = 50
  // Output: y = 130 + ((4 - 1) * +1) * 50 = 280
  //         or Error if target is missing or duplicated in entries.
  private static projectHtmlTargetCenter(
    target: string,
    alignment: PickerHtmlAlignment,
    sequence: PickerScrollSequence
  ): number | Error {
    const targetIndex = PickerScrollToViewBuilder.findUniqueTargetIndex(alignment.entries, target);
    if (targetIndex instanceof Error) {
      return targetIndex;
    }
    // Row distance in HTML, flipped if the visible run is opposite the HTML order.
    const offset = (targetIndex - alignment.visibleStart) * alignment.orientation;
    return PickerScrollToViewBuilder.center(sequence.points[0], sequence.axis) + offset * sequence.pitch;
  }

  private static findUniqueTargetIndex(entries: string[], target: string): number | Error {
    let targetIndex = -1;
    for (let i = 0; i < entries.length; i++) {
      if (entries[i] !== target) {
        continue;
      }
      if (targetIndex >= 0) {
        return new Error('the exact target occurs more than once in the ordered HTML rows.');
      }
      targetIndex = i;
    }
    if (targetIndex < 0) {
      return new Error('the exact target is absent from the ordered HTML rows.');
    }
    return targetIndex;
  }

  // Find the unique place the visible LiteDOM labels sit in the HTML row list.
  // Input:  entries = HTML row texts, e.g. [alpha, beta, gamma, delta, zeta]
  //         points  = visible LiteDOM options, e.g. [beta, gamma, delta]
  // Output: { entries, visibleStart: 1, orientation: +1 }
  //         or Error if the window is shorter, missing, or not unique.
  //         orientation -1 when HTML is 1980…2026 but visible cells are 2026, 2025, 2024.
  private static alignHtmlEntries(
    entries: string[],
    points: PickerScrollPoint[]
  ): PickerHtmlAlignment | Error {
    if (entries.length < points.length) {
      return new Error('the HTML row list is shorter than the visible option sequence.');
    }
    const result = new PickerHtmlAlignment();
    let matchCount = 0;
    // Try downward (+1) then upward (-1). Keep the alignment only if exactly one fits.
    for (let orientationIndex = 0; orientationIndex < 2; orientationIndex++) {
      matchCount += PickerScrollToViewBuilder.collectOrientationMatches(
        entries, points, orientationIndex === 0 ? 1 : -1, result
      );
    }
    if (matchCount !== 1) {
      return new Error('visible LiteDOM labels do not have one unique ordered alignment in HTML.');
    }
    return result;
  }

  private static collectOrientationMatches(
    entries: string[],
    points: PickerScrollPoint[],
    orientation: number,
    result: PickerHtmlAlignment
  ): number {
    let matchCount = 0;
    for (let start = 0; start < entries.length; start++) {
      if (!PickerScrollToViewBuilder.entriesMatchAt(entries, points, start, orientation)) {
        continue;
      }
      matchCount++;
      result.entries = entries;
      result.visibleStart = start;
      result.orientation = orientation;
    }
    return matchCount;
  }

  private static entriesMatchAt(
    entries: string[],
    points: PickerScrollPoint[],
    start: number,
    orientation: number
  ): boolean {
    for (let i = 0; i < points.length; i++) {
      const entryIndex = start + orientation * i;
      if (entryIndex < 0 || entryIndex >= entries.length || entries[entryIndex] !== points[i].label) {
        return false;
      }
    }
    return true;
  }

  private static findNearestCommonAncestor(nodes: Node[], floor: Node): Node | null {
    if (nodes.length <= 0) {
      return null;
    }
    let candidate: Node | null = nodes[0];
    while (candidate) {
      if (PickerScrollToViewBuilder.isCommonAncestor(candidate, nodes, floor)) {
        return candidate;
      }
      if (candidate === floor) {
        break;
      }
      candidate = candidate.fa;
    }
    return null;
  }

  private static isCommonAncestor(candidate: Node, nodes: Node[], floor: Node): boolean {
    if (!PickerScrollToViewBuilder.isHtmlAncestor(floor, candidate)) {
      return false;
    }
    for (let i = 1; i < nodes.length; i++) {
      if (!PickerScrollToViewBuilder.isHtmlAncestor(candidate, nodes[i])) {
        return false;
      }
    }
    return true;
  }

  private static isHtmlAncestor(ancestor: Node, node: Node): boolean {
    let current: Node | null = node;
    while (current) {
      if (current === ancestor) {
        return true;
      }
      current = current.fa;
    }
    return false;
  }

  // Walk up from node until the parent is ancestor; that node is the option row.
  // Example: span "beta" → row0, because row0.fa is the list wrapper.
  // fa is the HTML parent. If node is not under ancestor, return null.
  private static directChildUnder(ancestor: Node, node: Node): Node | null {
    let current: Node | null = node;
    while (current && current.fa !== ancestor) {
      current = current.fa;
    }
    return current && current.fa === ancestor ? current : null;
  }

  private static collectHtmlText(node: Node): string {
    const parts: string[] = [];
    PickerScrollToViewBuilder.appendHtmlText(parts, node);
    return PickerScrollToViewBuilder.normalizeLabel(parts.join(' '));
  }

  private static appendHtmlText(parts: string[], node: Node): void {
    if (node.text && node.text.trim().length > 0) {
      parts.push(node.text);
    }
    const value = node.attributes.value;
    if (parts.length <= 0 && value && value.trim().length > 0) {
      parts.push(value);
    }
    for (let i = 0; i < node.children.length; i++) {
      PickerScrollToViewBuilder.appendHtmlText(parts, node.children[i]);
    }
  }

  private static resolveNodeLabel(node: AgentLiteDomNode): string {
    const values: string[] = [
      node.text,
      node.element.value,
      node.element.ariaLabel,
      node.element.title
    ];
    // Try each value in order, and return the first non-empty normalized string.
    // node.text is the most accurate text.
    for (let i = 0; i < values.length; i++) {
      const normalized = PickerScrollToViewBuilder.normalizeLabel(values[i]);
      if (normalized.length > 0) {
        return normalized;
      }
    }
    return '';
  }

  // Collapse any whitespace run to a single space
  // Trim the ends, and lowercase the result.
  private static normalizeLabel(value: string): string {
    return value.replace(/\s+/g, ' ').trim().toLowerCase();
  }

  private static parseExactNumber(value: string): number | null {
    if (!/^[+-]?(?:\d+\.?\d*|\.\d+)$/.test(value)) {
      return null;
    }
    const numeric = Number(value);
    return Number.isFinite(numeric) ? numeric : null;
  }

  private static center(point: PickerScrollPoint, axis: string): number {
    return axis === 'vertical' ?
      (point.box.top + point.box.bottom) / 2 :
      (point.box.left + point.box.right) / 2;
  }

  // Fold a node's rect list into one bounding box. One LiteDOM node can paint as several fragments.
  // example: rects [(10,20)-(50,40), (10,40)-(40,60)] become (10,20)-(50,60).
  private static unionBox(rects: AgentLiteDomRect[]): PickerScrollBox | null {
    if (rects.length <= 0) {
      return null;
    }
    const box = new PickerScrollBox();
    box.left = rects[0].left;
    box.top = rects[0].top;
    box.right = rects[0].right;
    box.bottom = rects[0].bottom;
    for (let i = 1; i < rects.length; i++) {
      box.left = Math.min(box.left, rects[i].left);
      box.top = Math.min(box.top, rects[i].top);
      box.right = Math.max(box.right, rects[i].right);
      box.bottom = Math.max(box.bottom, rects[i].bottom);
    }
    box.width = box.right - box.left;
    box.height = box.bottom - box.top;
    if (!PickerScrollToViewBuilder.hasFiniteBox(box)) {
      return null;
    }
    return box;
  }

  private static hasFiniteBox(box: PickerScrollBox): boolean {
    return [box.left, box.top, box.right, box.bottom, box.width, box.height].every(
      (value: number): boolean => Number.isFinite(value));
  }

  private static formatRatio(ratio: number): string {
    return (Math.round(ratio * 10000) / 10000).toString();
  }
}
