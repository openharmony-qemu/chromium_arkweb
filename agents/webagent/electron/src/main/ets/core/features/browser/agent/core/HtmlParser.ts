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
import { parseDocument } from 'htmlparser2';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { Timer } from './Utils';
import { isNavigationUrlAllowed } from './AgentTypes';

const DOMAIN = 0x6020;
const TAG = 'HtmlParser';
/** DOM 树递归构建最大深度，超出部分截断以防栈溢出。 */
const MAX_DOM_TREE_DEPTH = 100;
/** HTML void/空元素，无闭合标签。 */
export const emptyTags = [
  'area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta', 'param', 'source', 'track', 'wbr',
];

function xorshift(x: number): number {
  let value = x | 0;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  return value | 0;
}

function hashSet(values: Iterable<number>): number {
  let sum = 1;
  for (const value of values) {
    sum += xorshift(value);
  }
  return sum;
}

function hashStr(input: string): number {
  let hash = 0;
  for (const char of input) {
    hash = (hash << 1) ^ char.charCodeAt(0);
  }
  return hash | 0;
}

type AttributeValue = string | string[];

interface ParsedDomNode {
  type?: string;
  name?: string;
  attribs?: Record<string, AttributeValue>;
  children?: ParsedDomNode[];
  data?: string;
}

interface PrintWholeTreeOptions {
  includeNoneText?: boolean;
  includeHref?: boolean;
  useIndent?: boolean;
}

export enum Location {
  inScreen = 'lite',
  above = 'above',
  below = 'below',
  left = 'left',
  right = 'right'
}

export class Node {
  public fa: Node | null;
  public tag_name: string;
  public attributes: Record<string, string>;
  public children: Node[];
  public text: string | null;
  public hash: number;
  public must_include_id: number;
  public location: Location;
  public idx = 0;
  public has_interested = false;
  public has_must_include = false;
  public can_fold = false;

  public constructor(
    fa: Node | null,
    tagName: string,
    attributes: Record<string, string> = {},
    children: Node[] = [],
    text: string | null = null,
    hash = 0,
    mustIncludeId = -1,
    location: Location = Location.inScreen,
  ) {
    this.fa = fa;
    this.tag_name = tagName;
    this.attributes = attributes;
    this.children = children;
    this.text = text;
    this.hash = hash;
    this.must_include_id = mustIncludeId;
    this.location = location;
  }
}

type AttributeGetter = (node: Node) => unknown;

export class HtmlParser {
  public base_url = '';
  public soup: ParsedDomNode;
  public only_visible: boolean;
  public must_include: string[];
  public must_include_locations: Location[];
  public control2element: Record<number, Node> = {};
  public root: Node | null;
  public global_node_idx = 0;
  public idx2node: Record<number, Node> = {};
  public idx2text: Record<number, string> = {};
  // 按“原始解析树位置”建立的 xpath -> 重建树 Node 映射。
  // make_tree 生成的 xpath 计数基于 element.children（含被过滤节点），与外部传入的完整 xpath 一致；
  // 而 Node.children 是过滤后的子节点，按位置遍历会错位，故用此映射替代按位置查找。
  public xpath2node: Record<string, Node> = {};

  private readonly CODE_PAT: RegExp;
  private static readonly HASH_DEPTH_NEAR_ROOT: number = 4;

  public constructor(
    html: string,
    onlyVisible = true,
    mustInclude: string[] = [],
    noFoldTag: string[] = [],
    mustIncludeLocations: Location[] = [],
  ) {
    this.CODE_PAT = new RegExp(
      String.raw`.{100,}(?=.*\{.*\}.*;)(?=.*\.\w+[\w-]*\s*\{)(?=.*(for\s*\(\s*var\s+i\s*=|document\.querySelectorAll))`,
      's',
    );
    const timer = new Timer();
    this.soup = parseDocument(html) as unknown as ParsedDomNode;
    timer.tik('HtmlParser parseDocument');
    this.only_visible = onlyVisible;
    this.must_include = mustInclude;
    this.must_include_locations = mustIncludeLocations;
    const htmlNode = this.findHtmlNode(this.soup);
    this.root = this.make_tree(htmlNode, null);
    timer.tik('HtmlParser make tree');
    this.preprocess(this.root, noFoldTag);
    timer.tik('HtmlParser preprocess');
    this.print_whole_tree();
  }

  public set_base_url(url: string): void {
    this.base_url = url;
  }

  public setBaseUrl(url: string): void {
    this.set_base_url(url);
  }

  public getUrl(mustIncludeId: number): string {
    if (
      !(mustIncludeId in this.control2element) ||
      !('href' in this.control2element[mustIncludeId].attributes)
    ) {
      return '';
    }
    const href = this.control2element[mustIncludeId].attributes.href;
    if (!this.base_url) {
      return isNavigationUrlAllowed(href) ? href : '';
    }
    return this.resolveUrl(this.base_url, href);
  }

  public htmlLitedomNotMatch(): boolean {
    const ret = this.must_include.length > Object.keys(this.control2element).length;
    if (ret) {
      hilog.warn(DOMAIN, TAG, 'html inconsistent with litedom. litedom node = %{public}d, matched = %{public}d', this.must_include.length, Object.keys(this.control2element).length);
    }
    return ret;
  }

  public getUnmatchedLiteDom(): number[] {
    const unmatched: number[] = [];
    for (let i = 0; i < this.must_include.length; i += 1) {
      if (!(i in this.control2element)) {
        unmatched.push(i);
      }
    }
    if (unmatched.length > 0) {
      hilog.warn(
        DOMAIN, TAG,
        'getUnmatchedLiteDom unmatched = %{public}d / %{public}d',
        unmatched.length, this.must_include.length,
      );
    }
    return unmatched;
  }

  public filter_code(text: string | null | undefined): string {
    if (text && text.length < 2000 && text.length > 400) {
      hilog.debug(DOMAIN, TAG, 'filter code %{private}d, %{private}s', text.length, text);
    }
    if (!text || text.length > 2000) {
      return '';
    }
    const checkText = text.trim().slice(0, 400);
    if (this.CODE_PAT.test(checkText)) {
      return '';
    }
    return text;
  }

  public print_whole_tree(
    includeNoneText = false,
    includeHref = false,
    useIndent = true,
  ): string {
    this.global_node_idx = 0;
    this.idx2node = {};
    this.idx2text = {};
    const treeText = this.print_tree(this.root, useIndent ? 0 : -1, includeNoneText, includeHref);
    let result = treeText.join('\n');
    result = result.replace(
      ' <div>\n' +
      '  Agent 正在接管并操作页面\n' +
      '  <div>\n' +
      '   手动接管\n' +
      '   停止任务', '');
    return result;
  }

  public printWholeTree(options: PrintWholeTreeOptions = {}): string {
    return this.print_whole_tree(
      options.includeNoneText ?? false,
      options.includeHref ?? false,
      options.useIndent ?? true,
    );
  }

  public find_attribute_for_node(
    idx: number,
    attributes: Record<string, AttributeGetter>,
    depthLim = 6,
  ): [Record<string, unknown>, Node | null] {
    if (!(idx in this.idx2node)) {
      return [{}, null];
    }
    const node = this.idx2node[idx];
    return this.find_attribute_in_family(node, null, attributes, depthLim);
  }

  public findAttributeForNode(
    idx: number,
    attributes: Record<string, AttributeGetter>,
    depthLim = 6,
  ): [Record<string, unknown>, Node | null] {
    return this.find_attribute_for_node(idx, attributes, depthLim);
  }

  public make_tree(
    element: ParsedDomNode | null,
    fa: Node | null,
    xpath = '/html',
    depth = 0,
  ): Node | null {
    if (!element) {
      return null;
    }

    if (depth > MAX_DOM_TREE_DEPTH) {
      return null;
    }

    const tagName = element.name;
    if (!tagName || tagName === 'script' || tagName === 'style') {
      return null;
    }
    if (tagName === 'iframe') {
      hilog.info(DOMAIN, TAG, 'found iframe, xpath = %{private}s', xpath);
    }

    const attributes = this.normalizeAttributes(element.attribs);
    if ((attributes['data-id'] ?? '').startsWith('temp-rect-id-')) {
      return null;
    }

    if (this.invisibleNotMust(xpath, attributes)) {
      return null;
    }

    const timeText = this.getTimeText(tagName, attributes);
    const mustIncludeId = this.must_include.indexOf(xpath);
    const location = mustIncludeId >= 0 && mustIncludeId < this.must_include_locations.length
      ? this.must_include_locations[mustIncludeId]
      : Location.inScreen;

    const node = new Node(
      fa,
      tagName,
      attributes,
      [],
      this.filter_code(timeText),
      0,
      mustIncludeId,
      location,
    );
    this.xpath2node[xpath] = node;
    if (mustIncludeId >= 0) {
      this.control2element[mustIncludeId] = node;
    }

    const childCount: Record<string, number> = {};
    for (const child of element.children ?? []) {
      if (this.isElementNode(child)) {
        childCount[child.name] = (childCount[child.name] ?? 0) + 1;
        const childNode = this.make_tree(
          child,
          node,
          `${xpath}/${child.name}[${childCount[child.name]}]`,
          depth + 1,
        );
        if (childNode) {
          node.children.push(childNode);
        }
      } else if (this.isTextNode(child) && !this.isCommentNode(child) && child.data.trim().length > 0) {
        node.children.push(
          new Node(
            node,
            'text',
            {},
            [],
            this.filter_code(child.data.trim()),
            hashStr('text'),
            -1,
            location,
          ),
        );
      }
    }

    node.hash = this.getNodeHash(attributes.class, tagName, node);
    return node;
  }

  public preprocess(node: Node | null, noFoldTag: string[] = []): void {
    if (!node) {
      return;
    }
    if (node.must_include_id >= 0) {
      node.has_must_include = true;
    }
    if (node.must_include_id >= 0 || (node.text && node.text.trim().length > 0)) {
      node.has_interested = true;
    }
    let count = 0;
    for (const child of node.children) {
      this.preprocess(child, noFoldTag);
      if (child.has_interested) {
        node.has_interested = true;
        count += 1;
      }
      if (child.has_must_include) {
        node.has_must_include = true;
      }
    }
    if (node.must_include_id >= 0) {
      node.can_fold = false;
    } else if ((count === 1 && !noFoldTag.includes(node.tag_name)) || node.tag_name === 'html') {
      node.can_fold = true;
    }
  }

  private buildOpenTag(node: Node, includeHref: boolean): string {
    const litedomId = node.must_include_id >= 0 ? ` ${node.location}dom=${node.must_include_id}` : '';
    const actionType = node.attributes['action-type']
      ? ` action-type="${node.attributes['action-type']}"`
      : '';
    if (node.tag_name === 'a' && includeHref) {
      return node.attributes.href
        ? `<a ${litedomId} has_href=True${actionType}>`
        : `<a ${litedomId}has_href=False${actionType}>`;
    }
    return `<${node.tag_name}${litedomId}${actionType}>`;
  }

  public print_tree(
    node: Node | null,
    indent: number, // <0表示不缩进
    includeNoneText: boolean,
    includeHref: boolean
  ): string[] {
    if (!node) {
      return [];
    }

    let texts: string[] = [];
    for (const child of node.children) {
      if (child.has_interested) {
        texts = texts.concat(
          this.print_tree(
            child,
            indent >= 0 ? indent + (node.can_fold ? 0 : 1) : indent,
            includeNoneText,
            includeHref,
          ),
        );
      }
    }

    const realIndent = Math.max(indent, 0);
    if (node.text && node.text.trim().length > 0) {
      let text = ' '.repeat(realIndent);
      text += node.text.trim().replace(/\n/g, '\\n');
      texts = [text, ...texts];
      this.idx2node[this.global_node_idx] = node;
      this.idx2text[this.global_node_idx] = node.text.trim();
      node.idx = this.global_node_idx;
      this.global_node_idx += 1;
    } else if (includeNoneText && !node.can_fold) {
      let text = ' '.repeat(realIndent);
      text += this.buildOpenTag(node, includeHref);
      if (emptyTags.includes(node.tag_name)) {
        if (texts.length > 0) {
          texts = [text, ...texts];
        } else {
          texts = [text];
        }
      } else {
        const closingTag = ' '.repeat(realIndent) + `</${node.tag_name}>`;
        if (texts.length > 1) {
          texts = [text, ...texts, closingTag];
        } else if (texts.length === 1) {
          texts = [text + texts[0].slice(realIndent) + ` </${node.tag_name}>`];
        } else {
          texts = [text + ` </${node.tag_name}>`];
        }
      }
    }
    return texts;
  }

  public printLiteDomTree(
    node: Node | null,
    indent: number,
    litedomParts: [string, string, string, string][],
  ): string[] {
    if (!node) {
      return [];
    }

    let texts: string[] = [];
    for (const child of node.children) {
      if (child.has_must_include) {
        texts = texts.concat(
          this.printLiteDomTree(
            child,
            indent >= 0 ? indent + (node.can_fold ? 0 : 1) : indent,
            litedomParts,
          ),
        );
      }
    }

    const realIndent = indent >= 0 ? indent : 0;
    if (node.can_fold) {
      return texts;
    }
    let text = ' '.repeat(realIndent);
    if (node.must_include_id >= 0) {
      const parts = litedomParts[node.must_include_id];
      const tag = parts[0];
      const attributes = parts[1];
      const textContent = parts[2];
      const kind = parts[3];
      const idAttr = `${kind}="${node.must_include_id}"`;
      const fullAttrs = attributes.length > 0 ? `${idAttr} ${attributes}` : idAttr;
      const openingTag = `<${tag} ${fullAttrs}>`;
      if (emptyTags.includes(tag)) {
        const middle = textContent.length > 0 ? ` ${textContent}` : '';
        text += `${openingTag}${middle}`;
        if (texts.length) {
          texts = [text, ...texts];
        } else {
          texts = [text];
        }
      } else {
        const closingTag = ' '.repeat(realIndent) + `</${tag}>`;
        const middle = textContent.length > 0 ? ` ${textContent} ` : ' ';
        text += `${openingTag}${middle}`;
        if (texts.length) {
          texts = [text, ...texts, closingTag];
        } else {
          texts = [text + `</${tag}>`];
        }
      }
    } else {
      const openingTag = ' '.repeat(realIndent) + `<${node.tag_name}>`;
      if (emptyTags.includes(node.tag_name)) {
        texts = [openingTag, ...texts];
      } else {
        const closingTag = ' '.repeat(realIndent) + `</${node.tag_name}>`;
        texts = [openingTag, ...texts, closingTag];
      }
    }
    return texts;
  }

  public find_attribute_in_family(
    node: Node | null,
    requester: Node | null,
    attributes: Record<string, AttributeGetter>,
    depthLim = 5,
  ): [Record<string, unknown>, Node | null] {
    if (!node || node.children.length >= 10) {
      return [{}, requester];
    }

    if (this.hasTwins(node, requester, depthLim)) { //不记得当时为啥要写这个逻辑了。这个函数是提取出来的，函数名只是根据逻辑命名的。
      return [{}, requester];
    }

    if (depthLim <= 0) {
      return [{}, node];
    }

    const ret: Record<string, unknown> = {};
    const missing: Record<string, AttributeGetter> = {};
    let ancestor: Node | null = null;

    for (const attr of Object.keys(attributes)) {
      ret[attr] = null;
      const value = attributes[attr](node);
      if (value !== null && value !== undefined) {
        ret[attr] = value;
      } else {
        missing[attr] = attributes[attr];
      }
    }

    for (const child of node.children) {
      if (Object.keys(missing).length === 0) {
        break;
      }
      if (child === requester) {
        continue;
      }
      const [childRet] = this.find_attribute_in_family(child, node, missing, depthLim + 1);
      this.applyAttributes(childRet, ret, missing);
    }

    if (node.fa === null) {
      return [ret, node];
    }

    if (node.fa !== requester) {
      const usedDepth = node.fa.children.length > 1 ? 1 : 0;
      const [faRet, nextAncestor] = this.find_attribute_in_family(
        node.fa,
        node,
        missing,
        depthLim - usedDepth,
      );
      ancestor = nextAncestor;
      this.applyAttributes(faRet, ret, missing);
    }

    return [ret, ancestor];
  }

  public findAttributeInFamily(
    node: Node | null,
    requester: Node | null,
    attributes: Record<string, AttributeGetter>,
    depthLim = 5,
  ): [Record<string, unknown>, Node | null] {
    return this.find_attribute_in_family(node, requester, attributes, depthLim);
  }

  public getText(node: Node | null): Array<string | null> {
    if (!node) {
      return [];
    }
    let text: Array<string | null> = node.text !== null ? [node.text] : [];
    for (const child of node.children) {
      text = text.concat(this.getText(child));
    }
    return text;
  }

  public get_node_children(xpathStr: string): string[] {
    const node = this.getNodeFromXpath(xpathStr);
    if (!node) {
      return [];
    }
    const texts = this.get_children_text(node);
    const result: string[] = [];
    for (const item of texts) {
      if (item !== null && item !== node.text) {
        result.push(item);
      }
    }
    return result;
  }

  public getNodeChildren(xpathStr: string): string[] {
    return this.get_node_children(xpathStr);
  }

  public isValidXpath(xpathStr: string): boolean {
    if (!xpathStr.startsWith('/html')) {
      return false;
    }
    const parts = xpathStr.split('/').slice(2);
    if (parts.length === 0) {
      return true;
    }
    for (const part of parts) {
      const match = part.match(/^([a-zA-Z][a-zA-Z0-9]*)\[(\d+)\]$/);
      if (!match) {
        return false;
      }
      const index = Number(match[2]);
      if (index < 1) {
        return false;
      }
    }
    return true;
  }

  public getNodeFromXpath(xpathStr: string): Node | null {
    if (!this.isValidXpath(xpathStr)) {
      hilog.error(DOMAIN, TAG, 'getNodeFromXpath failed. Found invalid xpath: %{private}s', xpathStr);
      return null;
    }
    // 用 make_tree 期间记录的“原始位置 xpath”映射查找。
    // 重建树会过滤不可见/script/style 等节点，导致按 Node.children 位置计数与原始 xpath 错位，
    // 故不能在重建树上按 [k] 遍历；映射未命中说明目标节点已被过滤，应返回 null。
    const node = this.xpath2node[xpathStr];
    if (!node) {
      hilog.warn(DOMAIN, TAG, 'getNodeFromXpath miss (node filtered out): %{private}s', xpathStr);
      return null;
    }
    return node;
  }

  /**
   * 沿 xpath 路径判断目标元素在 prev/curr 两棵树间是否"左下区域稳定"：
   *  - xpath 在 curr 树存在；
   *  - 目标节点本身子树哈希（node.hash 聚合全部后代）与 prev 对应节点相同；
   *  - strict: 路径上每个祖先的所有"左侧孩子"（实际 children 索引 < 路径孩子）的哈希都与 prev 相同。
   *  - unstrict: 路径上每个祖先的所有"左侧孩子" 在最多往下深度为hashDepth的子树（忽略超过hashDepth的子树部分）与prev相同。
   * 右侧孩子与目标之下的右下区域可自由变化（适配 scroll 后动态加载新内容、目标本身不变的场景）。
   * 递归沿路径下降时同步判定，任一层不满足即返回 false。
   */
  public static isTargetStableAlongPath(
    prevParser: HtmlParser,
    currParser: HtmlParser,
    xpathStr: string,
    strict: boolean
  ): boolean {
    if (!prevParser.root || !currParser.root) {
      return false;
    }
    const segments = HtmlParser.parseXpathSegments(xpathStr);
    hilog.debug(DOMAIN, TAG, 'isTargetStableAlongPath xpath = %{public}s', JSON.stringify(segments));
    if (segments.length <= 0) {
      // 无路径段（如 xpath=/html）→ 退化为整树根哈希比较
      return prevParser.root.hash === currParser.root.hash;
    }
    return HtmlParser.walkPathStable(prevParser.root, currParser.root, segments, 0, strict ? -1 : HtmlParser.HASH_DEPTH_NEAR_ROOT);
  }

  private static parseXpathSegments(xpathStr: string): ReadonlyArray<readonly [string, number]> {
    return xpathStr.split('/').slice(2).map((part: string): readonly [string, number] => {
      const bracketIdx = part.indexOf('[');
      if (bracketIdx < 0) {
        return [part, 1] as const;
      }
      const tag = part.substring(0, bracketIdx);
      // 去掉末尾 ']'
      const idxStr = part.substring(bracketIdx + 1, part.length - 1);
      let k = Number(idxStr);
      if (Number.isNaN(k) || k < 1) {
        k = 1;
      }
      return [tag, k] as const;
    });
  }

  private static walkPathStable(
    prevNode: Node,
    currNode: Node,
    segments: ReadonlyArray<readonly [string, number]>,
    depth: number,
    hashDepth: number
  ): boolean {
    hilog.debug(DOMAIN, TAG, 'walk path stable entering %{private}s', currNode.tag_name);
    if (depth === segments.length) {
      // 到达目标节点：比子树哈希（node.hash 已聚合全部后代）
      return prevNode.hash === currNode.hash;
    }
    const tag = segments[depth][0];
    const k = segments[depth][1];
    // 在 prev/curr 各自找第 k 个同名孩子，记录其实际 children 索引
    let prevActual = -1;
    let prevCount = 0;
    for (let i = 0; i < prevNode.children.length; i++) {
      if (prevNode.children[i].tag_name === tag) {
        prevCount += 1;
        if (prevCount === k) {
          prevActual = i;
          break;
        }
      }
    }
    let currActual = -1;
    let currCount = 0;
    for (let i = 0; i < currNode.children.length; i++) {
      if (currNode.children[i].tag_name === tag) {
        currCount += 1;
        if (currCount === k) {
          currActual = i;
          break;
        }
      }
    }
    if (prevActual < 0 || currActual < 0) {
      hilog.warn(DOMAIN, TAG, 'walkPathStable failed: xpath not exist in tree');
      return false; // xpath 在某棵树不存在
    }
    if (prevActual !== currActual) {
      hilog.warn(DOMAIN, TAG, 'walkPathStable failed: left children number not identical, pre = %{private}d, cur = %{private}d', prevActual, currActual);
      for (let i = 0; i <= prevActual; i += 1) {
        hilog.debug(DOMAIN, TAG, 'pre %{private}d = %{private}s', i, prevNode.children[i].tag_name);
      }
      for (let i = 0; i <= currActual; i += 1) {
        hilog.debug(DOMAIN, TAG, 'cur %{private}d = %{private}s', i, currNode.children[i].tag_name);
      }
      return false; // 路径孩子位置漂移 → 左侧区域不同
    }
    // 所有左侧孩子（实际索引 < 路径索引）的哈希须与 prev 相同
    for (let i = 0; i < currActual; i++) {
      if (i >= prevNode.children.length) {
        return false;
      }
      if (hashDepth < 0 && prevNode.children[i].hash !== currNode.children[i].hash ||
          hashDepth >= 0 && HtmlParser.getHashNearRoot(prevNode.children[i], hashDepth) !== HtmlParser.getHashNearRoot(currNode.children[i], hashDepth)) {
        hilog.warn(DOMAIN, TAG, 'walkPathStable failed: left children %{public}d not identical', i);
        for (let j = 0; j <= i; j++) {
          hilog.debug(DOMAIN, TAG, 'left children %{public}d = %{private}s', j, prevNode.children[j].tag_name);
        }
        return false;
      }
    }
    return HtmlParser.walkPathStable(
      prevNode.children[prevActual],
      currNode.children[currActual],
      segments,
      depth + 1,
      hashDepth
    );
  }

  private static getHashNearRoot(node: Node, hashDepth: number): number {
    let hash = 0;
    if (node.attributes.class) {
      hash = hashStr(node.attributes.class);
    } else {
      hash = hashStr(node.tag_name);
    }
    if (hashDepth <= 0 || !node.children.length) {
      return hash;
    }
    hash += hashSet(node.children.map((child) => HtmlParser.getHashNearRoot(child, hashDepth - 1)));
    return hash;
  }

  public get_children_text(node: Node): Array<string | null> {
    let texts: Array<string | null> = [node.text];
    for (const child of node.children) {
      texts = texts.concat(this.get_children_text(child));
    }
    return texts;
  }

  public getChildrenText(node: Node): Array<string | null> {
    return this.get_children_text(node);
  }

  private invisibleNotMust(xpath: string, attributes: Record<string, string>): boolean {
    if (!this.must_include.includes(xpath) &&
        this.only_visible &&
        'style' in attributes &&
        (attributes.style.includes('display: none') ||
          attributes.style.includes('visibility: hidden'))
        ) {
      return true;
    }
    return false;
  }

  private getTimeText(tagName:string, attributes: Record<string, string>): string | null {
    if (tagName === 'time') {
      if (attributes.datetime) {
        return `time: ${attributes.datetime}`;
      } else if (attributes.title) {
        return `time: ${attributes.title}`;
      }
    }
    return null;
  }

  private getNodeHash(cls:string, tagName:string, node:Node): number {
    let hash = 0;
    if (cls) {
      hash = hashStr(cls);
    } else {
      hash = hashStr(tagName);
    }
    hash += hashSet(node.children.map((child) => child.hash));
    return hash;
  }

  private normalizeAttributes(
    attributes?: Record<string, AttributeValue>,
  ): Record<string, string> {
    const normalized: Record<string, string> = {};
    if (!attributes) {
      return normalized;
    }
    for (const [key, value] of Object.entries(attributes)) {
      normalized[key] = Array.isArray(value) ? value.join(' ') : String(value);
    }
    return normalized;
  }

  private hasTwins(node: Node, requester: Node, depthLim: number): boolean {
    if (node.fa !== requester && requester) {
      for (const child of node.children) {
        if (child !== requester && child.hash === requester.hash && !(depthLim >= 3 && node.children.length <= 4)) {
          return true;
        }
      }
    }
    return false;
  }

  private applyAttributes(
    source: Record<string, unknown>,
    target: Record<string, unknown>,
    missing: Record<string, AttributeGetter>
  ): void {
    for (const attr of Object.keys(source)) {
      if (source[attr] !== null && source[attr] !== undefined) {
        target[attr] = source[attr];
        Reflect.deleteProperty(missing, attr);
      }
    }
  }

  private findHtmlNode(document: ParsedDomNode): ParsedDomNode | null {
    const queue = [...(document.children ?? [])];
    while (queue.length > 0) {
      const node = queue.shift()!;
      if (this.isElementNode(node) && node.name === 'html') {
        return node;
      }
      queue.push(...(node.children ?? []));
    }
    return null;
  }

  private isElementNode(node: ParsedDomNode | null | undefined): node is ParsedDomNode & { name: string } {
    return Boolean(node && typeof node.name === 'string' && node.name.length > 0);
  }

  private isTextNode(node: ParsedDomNode | null | undefined): node is ParsedDomNode & { data: string } {
    return Boolean(node && typeof node.data === 'string');
  }

  private isCommentNode(node: ParsedDomNode | null | undefined): boolean {
    return node?.type === 'comment';
  }

  private resolveUrl(baseUrl: string, href: string): string {
    if (href.length <= 0) {
      return '';
    }
    if (this.hasScheme(href)) {
      return isNavigationUrlAllowed(href) ? href : '';
    }
    if (href.startsWith('//')) {
      const scheme = this.readScheme(baseUrl);
      const resolved = scheme.length > 0 ? `${scheme}:${href}` : href;
      return isNavigationUrlAllowed(resolved) ? resolved : '';
    }

    const normalizedBase = this.stripFragmentAndQuery(baseUrl);
    if (href.startsWith('#') || href.startsWith('?')) {
      return `${normalizedBase}${href}`;
    }

    const origin = this.readOrigin(normalizedBase);
    if (href.startsWith('/')) {
      const resolved = origin.length > 0 ? `${origin}${href}` : href;
      return isNavigationUrlAllowed(resolved) ? resolved : '';
    }

    const baseDir = this.readBaseDirectory(normalizedBase);
    if (baseDir.length <= 0) {
      return isNavigationUrlAllowed(href) ? href : '';
    }
    const resolved = this.joinPath(baseDir, href);
    return isNavigationUrlAllowed(resolved) ? resolved : '';
  }

  private hasScheme(text: string): boolean {
    return /^[A-Za-z][A-Za-z0-9+.-]*:/.test(text);
  }

  private readScheme(url: string): string {
    const schemeEnd = url.indexOf(':');
    if (schemeEnd <= 0) {
      return '';
    }
    return url.substring(0, schemeEnd);
  }

  private stripFragmentAndQuery(url: string): string {
    let end = url.length;
    const hashIndex = url.indexOf('#');
    if (hashIndex >= 0 && hashIndex < end) {
      end = hashIndex;
    }
    const queryIndex = url.indexOf('?');
    if (queryIndex >= 0 && queryIndex < end) {
      end = queryIndex;
    }
    return url.substring(0, end);
  }

  private readOrigin(url: string): string {
    const schemeIndex = url.indexOf('://');
    if (schemeIndex < 0) {
      return '';
    }
    const pathStart = url.indexOf('/', schemeIndex + 3);
    if (pathStart < 0) {
      return url;
    }
    return url.substring(0, pathStart);
  }

  private readBaseDirectory(url: string): string {
    const lastSlash = url.lastIndexOf('/');
    const schemeIndex = url.indexOf('://');
    if (lastSlash < 0) {
      return url;
    }
    if (schemeIndex >= 0 && lastSlash <= schemeIndex + 2) {
      return url;
    }
    return url.substring(0, lastSlash);
  }

  private joinPath(baseDir: string, relativePath: string): string {
    const baseOrigin = this.readOrigin(baseDir);
    let path = baseOrigin.length > 0 ? baseDir.substring(baseOrigin.length) : baseDir;
    if (!path.startsWith('/')) {
      path = `/${path}`;
    }

    const segments = path.split('/');
    if (segments.length > 0 && segments[segments.length - 1].length === 0) {
      segments.pop();
    }

    const relatives = relativePath.split('/');
    for (let i = 0; i < relatives.length; i++) {
      const segment = relatives[i];
      if (segment.length === 0 || segment === '.') {
        continue;
      }
      if (segment === '..') {
        if (segments.length > 1) {
          segments.pop();
        }
        continue;
      }
      segments.push(segment);
    }

    const normalizedPath = segments.join('/');
    if (baseOrigin.length > 0) {
      return `${baseOrigin}${normalizedPath}`;
    }
    return normalizedPath;
  }
}
