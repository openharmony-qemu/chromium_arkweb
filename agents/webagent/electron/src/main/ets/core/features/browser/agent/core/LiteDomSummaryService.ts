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

import { AgentConstants, AgentLiteDomElement, AgentLiteDomNode, AgentOutsideDOMTree, OutsideDirection } from './AgentTypes';
import { BrowserAgentUtf8TextService } from './Utf8TextService';
import { emptyTags } from './HtmlParser';

export class BrowserAgentLiteDomSummaryService {
  private static readonly MAX_TEXT_LENGTH: number = 200;
  private static readonly MAX_VALUE_LENGTH: number = 100;
  private static readonly TRUNCATION_SUFFIX: string = '...[truncated]';

  static buildLiteDomSummary(items: AgentLiteDomNode[], maxLength: number): string {
    let summary = '';
    let chars = 0;
    for (let i = 0; i < items.length; i++) {
      const line = BrowserAgentLiteDomSummaryService.buildLiteDomSummaryLine(i, items[i]);
      const piece = i > 0 ? `\n${line}` : line;
      summary += piece;
      // 逐段累加码点数，避免每轮重新统计整段摘要
      chars += BrowserAgentUtf8TextService.countChars(piece);
      if (chars >= maxLength) {
        return `${BrowserAgentUtf8TextService.truncateToChars(summary, maxLength)} ...[truncated]`;
      }
    }
    return summary;
  }

  /**
   * 与 buildLiteDomSummary 等价，但行号使用 node.id 而非数组下标。
   * 供 outsideDOMTree 各桶使用——outside 节点的 id 是全局 offset 后的编号，
   * 需保留全局 id 以便 agent 据此引用元素。
   */
  static buildLiteDomSummaryByIds(items: AgentLiteDomNode[], maxLength: number): string {
    let summary = '';
    let chars = 0;
    for (let i = 0; i < items.length; i++) {
      const node = items[i];
      const line = BrowserAgentLiteDomSummaryService.buildLiteDomSummaryLine(node.id, node);
      const piece = i > 0 ? `\n${line}` : line;
      summary += piece;
      chars += BrowserAgentUtf8TextService.countChars(piece);
      if (chars >= maxLength) {
        return `${BrowserAgentUtf8TextService.truncateToChars(summary, maxLength)} ...[truncated]`;
      }
    }
    return summary;
  }

  /**
   * 构造 outsideDOMTree 的摘要：收集所有 outside 元素，按"距视口边的最大偏移距离"升序排序，
   * 优先塞距离小的元素，直到累计字符超过 maxLength。
   *
   * 距离定义：对每个元素取 4 个方向"超出视口"的偏移量的最大值：
   *   aboveAmount = minTop < 0 ? -minTop : 0
   *   belowAmount = maxBottom > viewportH ? maxBottom - viewportH : 0
   *   leftAmount  = minLeft < 0 ? -minLeft : 0
   *   rightAmount = maxRight > viewportW ? maxRight - viewportW : 0
   *   distance = max(aboveAmount, belowAmount, leftAmount, rightAmount)
   * 距离越小 = 越接近视口边 = 优先塞入。
   *
   * 输出格式：连续同方向元素合并到一个 <{direction}Dom> 哨兵 section 内（与原格式兼容）；
   * 因按距离排序，不同方向会交错，故可能产生多个同方向的不连续 section。
   */
  static buildOutsideDomSummary(
    tree: AgentOutsideDOMTree,
    maxLength: number,
    viewportWidth: number,
    viewportHeight: number
  ): string {
    // 收集所有 outside 元素 + 方向 + 距离
    interface OutsideEntry {
      node: AgentLiteDomNode;
      direction: OutsideDirection;
      distance: number;
    }
    const entries: OutsideEntry[] = [];
    const directions: OutsideDirection[] = ['above', 'below', 'left', 'right'];
    for (let d = 0; d < directions.length; d++) {
      const direction = directions[d];
      const bucket = BrowserAgentLiteDomSummaryService.selectOutsideBucket(tree, direction);
      for (let i = 0; i < bucket.length; i++) {
        const node = bucket[i];
        const distance = BrowserAgentLiteDomSummaryService.computeOutsideDistance(
          node, viewportWidth, viewportHeight);
        entries.push({ node, direction, distance });
      }
    }
    if (entries.length <= 0) {
      return '';
    }
    // 按距离升序排序，距离小的优先入选，直到累计 chars 超过 maxLength
    entries.sort((a: OutsideEntry, b: OutsideEntry): number => a.distance - b.distance);

    const selected: OutsideEntry[] = [];
    let chars = 0;
    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i];
      const line = BrowserAgentLiteDomSummaryService.buildLiteDomSummaryLine(entry.node.id, entry.node);
      if (line.length <= 0) {
        continue;
      }
      const piece = selected.length > 0 ? `\n${line}` : line;
      const pieceChars = BrowserAgentUtf8TextService.countChars(piece);
      // 哨兵开销粗略估算：每个 section 约 30 chars（<belowDom>\n...\n</belowDom>）
      // 简单起见，按全局累计 chars 截断，最后由各 section flush 时统一处理。
      if (chars + pieceChars >= maxLength) {
        break;
      }
      selected.push(entry);
      chars += pieceChars;
    }
    if (selected.length <= 0) {
      return '';
    }

    // 入选元素按 id 升序排序（summary 内部按元素编号顺序，与距离排序无关）
    selected.sort((a: OutsideEntry, b: OutsideEntry): number => a.node.id - b.node.id);

    // 按 above/below/left/right 固定方向序，连续同方向合并 section
    let summary = '';
    let totalChars = 0;
    let currentDirection: OutsideDirection | null = null;
    let currentBody = '';
    let currentBodyChars = 0;

    const flushSection = (): void => {
      if (currentDirection === null || currentBody.length <= 0) {
        return;
      }
      const section = `<${currentDirection}Dom>\n${currentBody}\n</${currentDirection}Dom>`;
      const piece = summary.length > 0 ? `\n${section}` : section;
      summary += piece;
      totalChars += BrowserAgentUtf8TextService.countChars(piece);
    };

    for (let i = 0; i < selected.length; i++) {
      const entry = selected[i];
      const line = BrowserAgentLiteDomSummaryService.buildLiteDomSummaryLine(entry.node.id, entry.node);
      if (line.length <= 0) {
        continue;
      }
      if (currentDirection !== null && currentDirection !== entry.direction) {
        flushSection();
        currentBody = '';
        currentBodyChars = 0;
      }
      currentDirection = entry.direction;
      const piece = currentBody.length > 0 ? `\n${line}` : line;
      currentBody += piece;
      currentBodyChars += BrowserAgentUtf8TextService.countChars(piece);
    }
    flushSection();
    return summary;
  }

  /**
   * 计算节点距视口边的最大偏移距离（取 4 个方向超出视口量的最大值）。
   * 节点完全在视口内时返回 0（不应出现在 outside 树中，防御处理）。
   */
  private static computeOutsideDistance(
    node: AgentLiteDomNode,
    viewportWidth: number,
    viewportHeight: number
  ): number {
    if (node.rects.length <= 0) {
      return Number.MAX_SAFE_INTEGER; // 无 rect 信息，排到最后
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
    const aboveAmount = minTop < 0 ? -minTop : 0;
    const belowAmount = maxBottom > viewportHeight ? maxBottom - viewportHeight : 0;
    const leftAmount = minLeft < 0 ? -minLeft : 0;
    const rightAmount = maxRight > viewportWidth ? maxRight - viewportWidth : 0;
    return Math.max(aboveAmount, Math.max(belowAmount, Math.max(leftAmount, rightAmount)));
  }

  private static selectOutsideBucket(tree: AgentOutsideDOMTree, direction: OutsideDirection): AgentLiteDomNode[] {
    if (direction === 'above') {
      return tree.above;
    }
    if (direction === 'below') {
      return tree.below;
    }
    if (direction === 'left') {
      return tree.left;
    }
    return tree.right;
  }

  static buildLiteDomSummaryParts(node: AgentLiteDomNode): [string, string, string] {
    const attributes: string[] = [];
    const tagName = BrowserAgentLiteDomSummaryService.resolveSummaryTagName(node.element.tagName);
    const type = BrowserAgentLiteDomSummaryService.resolveNodeType(node);
    if (type === 'scrollable') {
      attributes.push('scrollable=true');
    } else if (type === 'inputtable') {
      attributes.push('inputtable=true');
    }
    if (tagName === 'input' && node.element.type?.trim().length > 0) {
      if (node.element.type.trim().toLowerCase() === 'file') {
        attributes.push('uploadable=true');
      }
      attributes.push(`type=${node.element.type.trim().toLowerCase()}`);
    }
    const text = BrowserAgentLiteDomSummaryService.truncateSummaryValue(
      BrowserAgentLiteDomSummaryService.normalizeSummaryValue(node.text)
    );
    const value = BrowserAgentLiteDomSummaryService.resolveSummaryValue(node.element);
    if (value.length > 0 && value !== text) {
      attributes.push(`value=${JSON.stringify(value)}`);
    }
    const title = BrowserAgentLiteDomSummaryService.resolveSummaryTitle(node.element);
    if (title.length > 0 && title !== text) {
      attributes.push(`title=${JSON.stringify(
        BrowserAgentUtf8TextService.truncateToChars(title, AgentConstants.LITEDOM_MAX_TITLE_LENGTH))}`);
    }
    const placeholder = BrowserAgentLiteDomSummaryService.normalizeSummaryValue(node.element.placeholder);
    if (placeholder.length > 0) {
      attributes.push(`placeholder=${JSON.stringify(placeholder)}`);
    }
    if (node.element.options.length > 0) {
      const boundedOptions: string[] = [];
      for (let k = 0; k < node.element.options.length && k < AgentConstants.LITEDOM_MAX_OPTIONS; k++) {
        boundedOptions.push(BrowserAgentLiteDomSummaryService.sanitizeDomValue(node.element.options[k],
          BrowserAgentLiteDomSummaryService.MAX_TEXT_LENGTH));
      }
      attributes.push(`option=${JSON.stringify(boundedOptions)}`);
    }
    return [tagName, attributes.join(' '), text];
  }

  static buildLiteDomSummaryLine(index: number, node: AgentLiteDomNode): string {
    const [tag, attributes, text] = BrowserAgentLiteDomSummaryService.buildLiteDomSummaryParts(node);
    const openingTag = attributes.length > 0 ? `<${tag} ${attributes}>` : `<${tag}>`;
    if (emptyTags.includes(tag)) {
      const middle = text.length > 0 ? ` ${text}` : '';
      return `${index}. ${openingTag}${middle}`;
    }
    const closingTag = `</${tag}>`;
    const middle = text.length > 0 ? ` ${text} ` : ' ';
    return `${index}. ${openingTag}${middle}${closingTag}`;
  }

  private static resolveNodeType(node: AgentLiteDomNode): string {
    return node.type.length > 0 ? node.type : node.element.type;
  }

  private static resolveSummaryValue(element: AgentLiteDomElement): string {
    const type = element.type?.trim().toLowerCase();
    if (type === 'password' || type === 'hidden') {
      return '';
    }
    return BrowserAgentLiteDomSummaryService.truncateSummaryValue(
      BrowserAgentLiteDomSummaryService.normalizeSummaryValue(element.value),
      BrowserAgentLiteDomSummaryService.MAX_VALUE_LENGTH);
  }

  private static resolveSummaryTitle(element: AgentLiteDomElement): string {
    const title = BrowserAgentLiteDomSummaryService.normalizeSummaryValue(element.title);
    if (title.length > 0) {
      return title;
    }
    return BrowserAgentLiteDomSummaryService.normalizeSummaryValue(element.ariaLabel);
  }

  private static resolveSummaryTagName(tagName: string): string {
    const normalized = tagName.trim().toLowerCase();
    return normalized.length > 0 ? normalized : 'unknown';
  }

  private static normalizeSummaryValue(value: string): string {
    return BrowserAgentLiteDomSummaryService.sanitizeDomValue(value, BrowserAgentLiteDomSummaryService.MAX_TEXT_LENGTH);
  }

  private static truncateSummaryValue(value: string, maxLength: number = BrowserAgentLiteDomSummaryService.MAX_TEXT_LENGTH): string {
    return BrowserAgentLiteDomSummaryService.sanitizeDomValue(value, maxLength);
  }

  /** DOM 值净化：trim + 压缩空白 + 截断（DOM 摘要专用，不改变元素定位所需文本内容） */
  private static sanitizeDomValue(value: string, maxLength: number): string {
    const normalized = value.trim().replace(/\s+/g, ' ');
    if (!BrowserAgentUtf8TextService.exceeds(normalized, maxLength)) {
      return normalized;
    }
    const bounded = BrowserAgentUtf8TextService.truncateToChars(normalized, maxLength);
    return `${bounded}${BrowserAgentLiteDomSummaryService.TRUNCATION_SUFFIX}`;
  }
}

class TrieNode {
  children: Map<string, TrieNode> = new Map<string, TrieNode>();
  terminal: boolean = false;
}

/**
 * 基于 litedom 结点 xpath 构建的 trie。
 * 根到某结点的路径（每条边以一个 xpath 段为单位，如 "div[3]"）即对应结点的 xpath。
 * 实际 litedom 结点只是 html 的一部分，不会因为 "div[3]" 而补出 [1]、[2] 等兄弟段。
 */
export class LiteDomTrie {
  private root: TrieNode = new TrieNode();

  constructor(items: AgentLiteDomNode[] = []) {
    this.build(items);
  }

  /** 以一个 litedom 数组（重新）构建 trie。 */
  build(items: AgentLiteDomNode[]): void {
    this.root = new TrieNode();
    for (let i = 0; i < items.length; i++) {
      this.insertXpath(items[i].xpath);
    }
  }

  /** 查询给定 xpath 是否作为某个已插入的 litedom 结点存在（精确匹配 terminal 结点）。 */
  exists(xpath: string): boolean {
    const segments = LiteDomTrie.splitXpath(xpath);
    let node: TrieNode = this.root;
    for (let i = 0; i < segments.length; i++) {
      const child = node.children.get(segments[i]);
      if (child === undefined) {
        return false;
      }
      node = child;
    }
    return node.terminal;
  }

  private insertXpath(xpath: string): void {
    const segments = LiteDomTrie.splitXpath(xpath);
    if (segments.length === 0) {
      return;
    }
    let node: TrieNode = this.root;
    for (let i = 0; i < segments.length; i++) {
      const seg = segments[i];
      let child = node.children.get(seg);
      if (child === undefined) {
        child = new TrieNode();
        node.children.set(seg, child);
      }
      node = child;
    }
    node.terminal = true;
  }

  private static splitXpath(xpath: string): string[] {
    if (typeof xpath !== 'string' || xpath.length === 0) {
      return [];
    }
    const segments: string[] = [];
    const parts = xpath.split('/');
    for (let i = 0; i < parts.length; i++) {
      const part = parts[i];
      if (part.length > 0) {
        segments.push(part);
      }
    }
    return segments;
  }
}
