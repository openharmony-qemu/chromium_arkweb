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

import { AgentConstants, AgentLiteDomNode, AgentObservation } from './AgentTypes';
import { HtmlParser, Node } from './HtmlParser';
import { BrowserAgentUtf8TextService } from './Utf8TextService';
import { BrowserAgentUtils } from './Utils';
import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN = 0x6020;
const TAG = 'LiteDomDiffService';

/** 一条元素变更记录。kind 取 target / changed / appeared / disappeared。 */
class PageChangeEntry {
  kind: string = '';
  elementId: number = -1;
  node: AgentLiteDomNode | null = null;
  /** 与上一步动作目标的关联度，用于组内排序：越大越相关。 */
  relevance: number = 0;
  /** 是不是上一步动作的目标元素。标在条目上，省得单列一组时有时无。 */
  isActionTarget: boolean = false;
  /** 目标元素本次已经不在页面上。对话框、菜单、弹层被接受后关闭就是这种。 */
  targetGone: boolean = false;
  /** 子树是否按富文本规格输出（更深、保留 style）。只有富文本编辑的目标元素会置位。 */
  richSubtree: boolean = false;
  /** 元素周边的小树：祖先 / 自身 / 后代，每个节点只列变化了的属性。空串表示状态没变。 */
  stateTree: string = '';
  /**
   * litedom 字段的变化，例如 'text "1024" → "1025"'。
   */
  contentDelta: string = '';
}

/** 配对与分类的结果。matchedCurrent 一并带出来，后面定位动作目标要用。 */
class PageChangeGroups {
  appeared: PageChangeEntry[] = [];
  changed: PageChangeEntry[] = [];
  disappeared: PageChangeEntry[] = [];
  matchedCurrent: number[] = [];
}

/** 一组输出的配额：条数与字符数各自独立，互不挤占。 */
class GroupQuota {
  maxEntries: number = 0;
  maxChars: number = 0;
  withAttributes: boolean = true;
  withSubtree: number = 0;

  constructor(maxEntries: number, maxChars: number, withAttributes: boolean, withSubtree: number) {
    this.maxEntries = maxEntries;
    this.maxChars = maxChars;
    this.withAttributes = withAttributes;
    this.withSubtree = withSubtree;
  }
}

/**
 * 汇总「上一步动作之后页面发生了什么」，供 agent 判断动作是否生效、当前控件处于什么状态。
 *
 * 两个来源，覆盖互补的两类信息：
 * 1) litedom 前后比对 —— 交互元素的出现 / 消失 / 变化。变化元素额外从整页 HTML 解析树补出
 *    属性与浅层子树：litedom 只保留极少字段，而「已点赞 / 已展开 / 已选中」这类状态
 *    通常写在 class、aria-* 上，且经常挂在图标子节点而不是按钮自己身上。
 * 2) 整页可见文本前后比对 —— 新出现的文本行。校验错误、toast、空结果这类「页面在回话」
 *    的信息都在非交互节点上，litedom 根本不收录。
 *
 * 注意这不是 HTML 树 diff：「谁变了」由 litedom 的前后比对决定，HTML 只用来按 xpath
 * 查回单个元素的属性与子树。两份 HTML 都会解析（本次的 parseHtml 早就做过，上一份为了
 * 比对 class/aria 额外解析一次），但从不做树对树的比较。
 */
export class BrowserAgentLiteDomDiffService {
  private static readonly TARGET_QUOTA: GroupQuota = new GroupQuota(1, 1200, true, 1);
  // 子树只给最相关的第一条：实测一条带子树的记录就能吃掉这一组八成的字符配额，
  // 把后面的变更挤成 "...and 1 more"。广度（列出几个元素变了）比深度更重要。
  private static readonly CHANGED_QUOTA: GroupQuota = new GroupQuota(40, 8000, true, 40);
  // 新出现的元素要回答的是「它现在是什么状态」而不是「变了什么」，而状态往往在子孙上
  // （element-ui 级联框的选项，选中态写在后代的 el-checkbox is-checked 里）。所以前两条也给子树，
  // 字符配额相应放宽——否则模型只看得到容器的一串文本，完全不知道哪个选项被勾上了。
  private static readonly APPEARED_QUOTA: GroupQuota = new GroupQuota(40, 8000, true, 40);
  private static readonly DISAPPEARED_QUOTA: GroupQuota = new GroupQuota(30, 4000, true, 30);
  /** 新增文本行的条数与字符配额。 */
  private static readonly NEW_TEXT_MAX_LINES: number = 60;
  private static readonly NEW_TEXT_MAX_CHARS: number = 3000;
  /** 总字符安全网，设在各组配额之和以上，正常情况下由配额先生效。 */
  /**
   * 整段输出的总长度上限，目标是 prompt 的 10% 左右。
   * 锚点是 Utils 里的 MAX_TREE_TEXT_LENGTH = 20000——网页文本单块就能到两万字符，
   * 加上 litedom 树、系统提示与历史，整个 prompt 量级在四五万字符，取 4500。
   * 这个上限由各组共享：组按重要性依次追加，预算用完后面的组自然被挤掉，
   * 而不是把最后一条从中间截断。
   */
  private static readonly OUTPUT_MAX_CHARS: number = 4500;
  /** 给「新增文本」留的保底额度：校验提示 / toast 常常是唯一能说明动作失败的信号。 */
  private static readonly NEW_TEXT_RESERVE_CHARS: number = 800;

  private static readonly MAX_ATTRIBUTES: number = 10;
  private static readonly MAX_VALUE_CHARS: number = 80;
  private static readonly STATE_VALUE_MAX_CHARS: number = 32;
  /** 小树里属性值的长度上限。class 要给前后全量，32 太短，但也不能让工具类站点无限铺开。 */
  private static readonly TREE_VALUE_MAX_CHARS: number = 200;
  /** 「文本被切碎」判据参与比较的行数上限，防止整页重绘时退化成平方级比较。 */
  private static readonly RESPLIT_MAX_LINES: number = 200;
  /** 长文本差异区段两侧保留的上下文字符数：只给差异本身，定位得到却读不懂。 */
  private static readonly REGION_CONTEXT_CHARS: number = 12;
  /** 状态比对往上看几层祖先：问卷/单选的选中态常挂在包裹的 label、li 上。 */
  private static readonly STATE_ANCESTOR_DEPTH: number = 2;
  /** 子树状态签名的深度：图标类状态挂在后代上，两层足够覆盖 <span><i> 这种嵌套。 */
  /** 变化小树往下探几层。富文本编辑器里一个链接常在 div > div > p > a，浅了够不到。 */
  private static readonly STATE_SUBTREE_DEPTH: number = 5;
  /**
   * subtree: 那段 HTML 骨架的深度，和变化小树分开。
   * 它只有 220 字符预算，深度换来的是广度——深 5 层只够钻进第一个孩子，
   * 而对「新出现了一个面板」这类条目，先看清有哪几项比看清第一项的内部更有用。
   */
  private static readonly SUBTREE_DEPTH: number = 2;
  private static readonly SUBTREE_MAX_CHARS: number = 220;
  /** React useId 生成的 ":r3:" 形态，每次渲染都变，不能当 key。 */
  private static readonly GENERATED_ID_PATTERN: RegExp = /^:[A-Za-z0-9]*:$/;
  /** 逐字保留的属性：能表达控件语义或状态。 */
  private static readonly KEEP_ATTRIBUTES: string[] = [
    'class', 'role', 'type', 'name', 'title', 'alt', 'placeholder',
    'id', 'href', 'checked', 'disabled', 'selected', 'open', 'value'
  ];
  /** 子树只留这几个：标签结构 + 类名 + 无障碍状态，足以看出图标换没换。 */
  private static readonly SUBTREE_KEEP_ATTRIBUTES: string[] = ['class', 'role'];
  /**
   * data-* 里的埋点键名特征，命中即丢——实测电商页面上这类占了属性体积的大头。
   * 必须按「以关键词开头的词段」匹配而不是子串：子串写法会把 data-state / data-status /
   * data-clickable 一起误杀，而 data-state 正是组件库最常用的状态属性。
   * stat / click 两个词歧义大，只认精确词段（stat / stats / statistic* / click / clicks / clicked）。
   */
  private static readonly TRACKING_KEY_PATTERN: RegExp =
    /(^|[-_])(?:(?:log|track|spm|report|exposure|point|monitor|trace|sensor|expo|pv|uv)[a-z]*|stats?|statistic[a-z]*|clicks?|clicked)([-_]|$)/i;
  private static readonly DATA_VALUE_MAX_CHARS: number = 32;
  /** xpath 后缀匹配取的段数：祖先层级平移时，尾部这几段仍然不变。 */
  private static readonly XPATH_SUFFIX_SEGMENTS: number = 4;
  /** 三轮唯一键配对用的键函数，按「假设强度」从强到弱排列，前一轮配上的不参与后一轮。 */
  private static readonly UNIQUE_KEY_PASSES: Array<(node: AgentLiteDomNode) => string> = [
    (node: AgentLiteDomNode): string => BrowserAgentLiteDomDiffService.dataIdKey(node),
    (node: AgentLiteDomNode): string => node.xpath,
    (node: AgentLiteDomNode): string => BrowserAgentLiteDomDiffService.xpathSuffixKey(node)
  ];
  /**
   * 视口移动类动作：litedom 只收视口内元素，滚动后集合几乎全换，逐元素比对是纯噪声
   * （页面没变，是视口动了）。这类动作只保留文本比对——treeText 取自整页 HTML，不受滚动影响。
   */
  private static readonly VIEWPORT_MOVE_ACTIONS: string[] = ['SCROLL', 'ARROW'];
  /**
   * 本来就不改变 DOM 的动作：SELECT_TEXT 只改选区（选区不是 DOM 状态），其余几个是纯读取。
   * 这类动作「无变化」是正常的，不能提示模型换方法——否则会让它放弃一个正确的动作去乱试。
   */
  /** 原生单选 / 复选：checked 是 property，不回写 attribute，前后 outerHTML 必然一模一样。 */
  private static readonly CHECKED_PROPERTY_TYPES: string[] = ['radio', 'checkbox'];
  /**
   * 额外参与比对的属性。我们本来就把它们打进 attrs，却从不比较，结果
   * 「提交按钮从 disabled 变可用」「details 展开」这类变化整个漏掉。
   * 这几个自身和祖先都比——状态写在祖先上很常见（<details open> 之于 <summary>）。
   */
  private static readonly DIFF_ATTRIBUTES: string[] =
    ['disabled', 'checked', 'selected', 'open', 'role', 'name'];
  /** 这几个 litedom 字段已经比过，HTML 侧再比一遍就是同一件事报两遍。 */
  private static readonly LITEDOM_COVERED_ATTRIBUTES: string[] =
    ['value', 'type', 'title', 'placeholder', 'aria-label'];
  /** 判定「整页重绘」的元素数下限：元素太少时比例不稳定。 */
  private static readonly LARGE_REPAINT_MIN_NODES: number = 8;
  private static readonly DOM_NEUTRAL_ACTIONS: string[] =
    ['SELECT_TEXT', 'WAIT', 'GET_HREF', 'CALCULATE', 'SCREENSHOT'];
  private static readonly SUBTREE_VOID_TAGS: string[] = ['br', 'hr', 'img', 'input', 'source', 'col', 'use', 'path'];
  /**
   * 富文本编辑动作之后，目标元素的子树要给得更深更全：加粗斜体嵌在 <strong> / <em> 里，
   * 颜色和字号写在 style 上，默认的深度 2 + 只留 class/role 根本够不到。
   * 只对「动作目标」这一条生效，所以放宽预算不会影响整体体积。
   */
  private static readonly RICH_TEXT_ACTION: string = 'RICH_TEXT';
  private static readonly RICH_SUBTREE_DEPTH: number = 12;
  private static readonly RICH_SUBTREE_MAX_CHARS: number = 2000;
  private static readonly RICH_SUBTREE_KEEP_ATTRIBUTES: string[] = ['class', 'role', 'style'];
  /**
   * treeText 里的结构标记，不是页面文案。这些标记必须剥掉：其中的 litedom 编号每次感知
   * 都重新编号，留着的话几乎每帧都会被当成「新出现的文本」——实测一个小红书页面 120 行
   * 新文本里绝大多数是这种垃圾，把真正的提示文案挤出了配额。
   * 见 stripTreeMarkup：剥标记而不是认整行，这样 print_tree 改格式时不用跟着追。
   */
  private static readonly TREE_OPEN_TAG_PATTERN: RegExp = /^<\/?[a-zA-Z][a-zA-Z0-9]*(\s[^<>]*)?>/;
  private static readonly TREE_CLOSE_TAG_PATTERN: RegExp = /<\/[a-zA-Z][a-zA-Z0-9]*>$/;
  private static readonly TREE_ACTION_TYPE_PATTERN: RegExp = /^action-type="[^"]*"/;

  /**
   * 构建页面变更摘要。无上一次感知、或发生跳转时返回空串。
   */
  public static buildDiff(
    previousObservation: AgentObservation | null,
    currentObservation: AgentObservation | null,
    parser: HtmlParser | null,
    previousTargetId: number = -1,
    previousActionType: string = ''
  ): string {
    const started = Date.now();
    const output = BrowserAgentLiteDomDiffService.computeDiff(
      previousObservation, currentObservation, parser, previousTargetId, previousActionType);
    // 这段是加进 prompt 的净增量，体积和耗时都要能在日志里直接看到。
    hilog.info(DOMAIN, TAG, 'page diff: chars=%{public}d tokens=%{public}d cost=%{public}dms',
      output.length, BrowserAgentLiteDomDiffService.estimateTokens(output), Date.now() - started);
    return output;
  }

  /** 粗估 token：中日韩字符约 1.5 个/token，其余约 4 个/token。只用于日志，不参与任何判断。 */
  private static estimateTokens(text: string): number {
    let wide = 0;
    for (const ch of text) {
      if (ch >= '\u2e80' && ch <= '\u9fff') {
        wide += 1;
      }
    }
    return Math.round(wide / 1.5 + (text.length - wide) / 4);
  }

  private static computeDiff(
    previousObservation: AgentObservation | null,
    currentObservation: AgentObservation | null,
    parser: HtmlParser | null,
    previousTargetId: number,
    previousActionType: string
  ): string {
    if (!previousObservation || !currentObservation) {
      return '';
    }
    // 跳转后整页换掉，逐元素比对没有意义。
    if (previousObservation.currentUrl !== currentObservation.currentUrl) {
      return 'Arrived at a new webpage (the url changed). Litedom comparison aborted.';
    }
    // litedom 的 8 个字段里没有 class / aria-*，而「已点赞 / 已选中 / 已展开」正写在这两处，
    // 且常常只有它们变（计数不刷新）。所以额外解析上一份 HTML——226KB 页面实测 8~25ms，
    // handleBacklog 早有同样的先例。
    // 必须和当前帧的 parser 用同一套参数（Utils.parseHtml 里是 onlyVisible=true +
    // collectCombinedLiteNodes 的 xpath 作 must_include，含 outsideDOMTree），否则两棵树口径不一致：onlyVisible 会丢掉带 display:none /
    // visibility:hidden 内联样式且不在 must_include 里的节点，上一帧若用 false 就成了当前帧的
    // 超集，文本差集「当前有而上一帧没有」永远算不出来。
    // 选择器正是重灾区：下拉 / 日历面板打开前就在 DOM 里，只是内联隐藏——面板标题这类
    // 非交互文本会被整组吃掉。
    const previousParser = previousObservation.html.length > 0
      ? new HtmlParser(previousObservation.html, true,
        BrowserAgentUtils.collectXpaths(
          BrowserAgentUtils.collectCombinedLiteNodes(previousObservation)), ['a'])
      : null;
    // 文本比对用「未截断」的整页文本：state.treeText 为了控制 prompt 体积截到 20000 字符，
    // 而 toast / 校验提示常被追加到 <body> 末尾，正好落在截断窗口之外。截断是为了限制
    // 送进 prompt 的量，不该限制比对的范围——输出条数另有配额兜着。printWholeTree 实测 1~2ms。
    const previousTreeText = BrowserAgentLiteDomDiffService.renderTreeText(previousParser);
    const currentTreeText = BrowserAgentLiteDomDiffService.renderTreeText(parser);

    if (BrowserAgentLiteDomDiffService.VIEWPORT_MOVE_ACTIONS.includes(previousActionType.trim().toUpperCase())) {
      return BrowserAgentLiteDomDiffService.buildViewportMoveResult(previousTreeText, currentTreeText);
    }

    const previousNodes = previousObservation.liteDomTree;
    const currentNodes = currentObservation.liteDomTree;
    const targetXpath = previousTargetId >= 0 && previousTargetId < previousNodes.length
      ? previousNodes[previousTargetId].xpath : '';
    const groups = BrowserAgentLiteDomDiffService.classifyChanges(
      previousNodes, currentNodes, targetXpath, previousParser, parser);
    const appeared = groups.appeared;
    const changed = groups.changed;
    const disappeared = groups.disappeared;

    // 目标元素若已落在 changed / appeared 里，就地打标记；否则单列一组（它一点没变时）。
    const target = BrowserAgentLiteDomDiffService.markOrExtractTarget(
      previousTargetId, currentNodes, groups.matchedCurrent, appeared, changed,
      disappeared, previousNodes, targetXpath);
    const newTextLines = BrowserAgentLiteDomDiffService.buildNewTextLines(previousTreeText, currentTreeText);
    if (previousActionType.trim().toUpperCase() === BrowserAgentLiteDomDiffService.RICH_TEXT_ACTION) {
      BrowserAgentLiteDomDiffService.enableRichSubtree(target, changed, appeared);
    }

    // 整页重绘：只压掉 appeared / disappeared 两组，changed 照常输出。
    // 那两组在重绘时确实只是把本帧 LiteDomTree 重抄一遍，但 changed 不是——
    // 「活下来且变了」的元素恰恰是重绘中唯一说明动作做了什么的信号，压掉就什么都不剩了。
    const largeRepaint = previousNodes.length >= BrowserAgentLiteDomDiffService.LARGE_REPAINT_MIN_NODES &&
      disappeared.length * 2 >= previousNodes.length && appeared.length * 2 >= currentNodes.length;

    const total = changed.length + (largeRepaint ? 0 : appeared.length + disappeared.length);
    hilog.info(DOMAIN, TAG,
      'page diff: appeared=%{public}d disappeared=%{public}d changed=%{public}d newText=%{public}d',
      appeared.length, disappeared.length, changed.length, newTextLines.length);
    const targetInputType = previousTargetId >= 0 && previousTargetId < previousNodes.length
      ? previousNodes[previousTargetId].element.type.trim().toLowerCase() : '';
    const noChangeNotice = BrowserAgentLiteDomDiffService.buildNoChangeNotice(
      previousActionType, currentObservation.tabStatus, target !== null, targetInputType);
    if (total <= 0 && newTextLines.length <= 0 && !target) {
      return noChangeNotice;
    }

    const blocks: string[] = [];
    if (total <= 0 && newTextLines.length <= 0) {
      blocks.push(noChangeNotice);
    }
    if (largeRepaint) {
      blocks.push(BrowserAgentLiteDomDiffService.buildLargeRepaintNotice(
        appeared.length, currentNodes.length, disappeared.length, previousNodes.length));
    }
    if (target) {
      BrowserAgentLiteDomDiffService.appendGroup(blocks, 'Action target', [target], parser,
        BrowserAgentLiteDomDiffService.TARGET_QUOTA, BrowserAgentLiteDomDiffService.remainingBudget(blocks));
    }
    // Changed 让出「新增文本」的保底额度，否则一页几十个变更会把 toast 挤掉。
    const textReserve = newTextLines.length > 0 ? BrowserAgentLiteDomDiffService.NEW_TEXT_RESERVE_CHARS : 0;
    BrowserAgentLiteDomDiffService.appendGroup(blocks, 'Changed', changed, parser,
      BrowserAgentLiteDomDiffService.CHANGED_QUOTA,
      BrowserAgentLiteDomDiffService.remainingBudget(blocks) - textReserve);
    BrowserAgentLiteDomDiffService.appendNewText(blocks, newTextLines,
      BrowserAgentLiteDomDiffService.remainingBudget(blocks));
    if (!largeRepaint) {
      BrowserAgentLiteDomDiffService.appendGroup(blocks, 'Appeared', appeared, parser,
        BrowserAgentLiteDomDiffService.APPEARED_QUOTA, BrowserAgentLiteDomDiffService.remainingBudget(blocks));
      // 消失的元素只在上一帧存在，必须拿上一帧的解析树去查，否则 xpath 会命中本帧的另一个元素。
      BrowserAgentLiteDomDiffService.appendGroup(blocks, 'Disappeared', disappeared, previousParser,
        BrowserAgentLiteDomDiffService.DISAPPEARED_QUOTA, BrowserAgentLiteDomDiffService.remainingBudget(blocks));
    }
    return blocks.join('\n');
  }

  /**
   * 视口移动后的输出。litedom 只收视口内的元素，滚一屏后集合几乎全换，逐元素比对是纯噪声
   * （页面没变，是视口动了）。treeText 取自整页 HTML 不受滚动影响，所以文本比对照常保留。
   */
  private static buildViewportMoveResult(previousTreeText: string, currentTreeText: string): string {
    const scrolledText = BrowserAgentLiteDomDiffService.buildNewTextLines(previousTreeText, currentTreeText);
    // 不能说「视口已经移动」：滚动失败（已经到底、容器不可滚、滚错了元素）时 HTML 同样不变，
    // 我们无从分辨。断言它移动了，模型就不会去重试或换策略。只说确实知道的——HTML 变没变。
    if (scrolledText.length <= 0) {
      return 'Interactive elements are not compared after a scroll (LiteDomTree only covers the visible ' +
        'area). The page HTML is unchanged, so nothing new was loaded; check the screenshot for whether ' +
        'the viewport actually moved.';
    }
    const blocks: string[] = [
      'Interactive elements are not compared after a scroll (LiteDomTree only covers the visible area). ' +
      'Text that was not in the page HTML before this action:'
    ];
    BrowserAgentLiteDomDiffService.appendNewText(blocks, scrolledText,
      BrowserAgentLiteDomDiffService.remainingBudget(blocks));
    return blocks.join('\n');
  }

  /**
   * 整页重绘的说明。appeared / disappeared 两组在重绘时只是把本帧 LiteDomTree 重抄一遍，
   * 那份数据模型下面本来就有；changed 则不同，仍然照常输出。
   * 注意调用处的判据落在「消失比例」而不是「新增比例」：弹出面板同样会凭空多出一堆元素，
   * 但不会让原有元素消失，而那恰恰是最该输出的场景。
   */
  private static buildLargeRepaintNotice(
    appearedCount: number,
    currentCount: number,
    disappearedCount: number,
    previousCount: number
  ): string {
    return `The page was largely replaced (${appearedCount} of ${currentCount} interactive elements ` +
      `are new, ${disappearedCount} of ${previousCount} previous ones are gone), so the previous ` +
      'action did take effect. Elements that only appeared or disappeared are not listed because they ' +
      'would just repeat the current page data below; elements that survived and changed are still listed.';
  }

  /**
   * 四轮配对 + 分三类 + 组内排序。
   * 配对与分类是同一件事的两半：分类结果完全由配对结果决定，拆开反而要来回传两个数组。
   */
  private static classifyChanges(
    previousNodes: AgentLiteDomNode[],
    currentNodes: AgentLiteDomNode[],
    targetXpath: string,
    previousParser: HtmlParser | null,
    parser: HtmlParser | null
  ): PageChangeGroups {
    const groups = new PageChangeGroups();
    const matchedPrevious: boolean[] = new Array<boolean>(previousNodes.length).fill(false);
    groups.matchedCurrent = new Array<number>(currentNodes.length).fill(-1);
    for (const keyOf of BrowserAgentLiteDomDiffService.UNIQUE_KEY_PASSES) {
      BrowserAgentLiteDomDiffService.matchByUniqueKey(
        previousNodes, currentNodes, matchedPrevious, groups.matchedCurrent, keyOf);
    }
    BrowserAgentLiteDomDiffService.matchByFingerprint(
      previousNodes, currentNodes, matchedPrevious, groups.matchedCurrent);

    for (let i = 0; i < currentNodes.length; i++) {
      const previousIndex = groups.matchedCurrent[i];
      if (previousIndex < 0) {
        groups.appeared.push(
          BrowserAgentLiteDomDiffService.buildEntry('appeared', i, currentNodes[i], targetXpath));
        continue;
      }
      const content = BrowserAgentLiteDomDiffService.contentDelta(previousNodes[previousIndex], currentNodes[i]);
      const tree = BrowserAgentLiteDomDiffService.buildStateTree(
        previousNodes[previousIndex], currentNodes[i], previousParser, parser);
      if (content.length > 0 || tree.length > 0) {
        const entry = BrowserAgentLiteDomDiffService.buildEntry('changed', i, currentNodes[i], targetXpath);
        entry.contentDelta = content;
        entry.stateTree = tree;
        groups.changed.push(entry);
      }
    }
    for (let i = 0; i < previousNodes.length; i++) {
      if (!matchedPrevious[i]) {
        groups.disappeared.push(
          BrowserAgentLiteDomDiffService.buildEntry('disappeared', -1, previousNodes[i], targetXpath));
      }
    }
    BrowserAgentLiteDomDiffService.sortByRelevance(groups.appeared);
    BrowserAgentLiteDomDiffService.sortByRelevance(groups.changed);
    BrowserAgentLiteDomDiffService.sortByRelevance(groups.disappeared);
    return groups;
  }

  /**
   * 「页面没有任何变化」的措辞。这句话必须区分三种截然不同的情况，否则会把模型推向错误行为：
   * 1) 该动作本来就不改 DOM（SELECT_TEXT 等）—— 无变化是正常的，不能当失败；
   * 2) 页面还在加载 —— 效果尚未呈现，应该等而不是换方法；
   * 3) 动作执行了但页面确实没反应 —— 可能是异步没返回，也可能推理错了。这两者单看一帧
   *    无法区分，所以让模型先等一拍、结合动作历史再下结论，而不是立刻改换目标。
   */
  private static buildNoChangeNotice(
    previousActionType: string,
    tabStatus: string,
    hasTarget: boolean,
    targetInputType: string
  ): string {
    const actionType = previousActionType.trim().toUpperCase();
    // 目标元素仍会带着当前属性列在下面，主消息不能说死，否则自相矛盾。
    const prefix = 'No interactive element appeared, disappeared or changed since the previous action, ' +
      'and no new text was rendered. ' +
      (hasTarget ? 'Still check the target element listed below: its attributes carry its current state. ' : '');
    // 原生 radio / checkbox 点中与否只体现在 property 上，HTML 里看不出来——必须说清楚，
    // 否则模型会把「HTML 没变」当成「没点上」，反复重试一个其实已经成功的动作。
    if (BrowserAgentLiteDomDiffService.CHECKED_PROPERTY_TYPES.includes(targetInputType)) {
      return `${prefix}Note that the previous action targeted a native ${targetInputType}: its checked ` +
        'state lives in a DOM property that is absent from the page HTML, so an unchanged page here does ' +
        'NOT mean the click missed. Confirm the selection from the screenshot instead.';
    }
    if (BrowserAgentLiteDomDiffService.DOM_NEUTRAL_ACTIONS.includes(actionType)) {
      return `${prefix}This is expected: ${actionType} does not change the page DOM by itself ` +
        '(a text selection, for example, is not part of the DOM). Judge its effect from the screenshot, ' +
        'and do NOT treat this as a failed action.';
    }
    if (tabStatus === AgentConstants.TAB_STATUS_LOADING) {
      return `${prefix}The tab is still loading, so the effect may simply not be rendered yet. ` +
        'Prefer WAIT and re-observe before concluding that the action failed.';
    }
    return `${prefix}This does not by itself mean the action was wrong — the page may still be waiting ` +
      'on an async update. If you have not waited since this action, WAIT once and re-observe; only if ' +
      'the same action produced no change more than once, switch to a different element or approach.';
  }

  /** 渲染整页可见文本，选项与 Utils.parseHtml 保持一致，重复调用是幂等的。 */
  private static renderTreeText(parser: HtmlParser | null): string {
    return parser
      ? parser.printWholeTree({includeNoneText: true, includeHref: false })
      : '';
  }

  /**
   * 整页可见文本的行级差集，输出本次新出现的文本行。
   * 文本行不需要身份匹配——只要判断「之前没有、现在有了」，因此不受 xpath 漂移影响。
   * 只报新增：消失的文本噪声大得多，且模型也无法据此行动。
   */
  private static buildNewTextLines(previousTreeText: string, currentTreeText: string): string[] {
    if (previousTreeText.length <= 0 || currentTreeText.length <= 0) {
      return [];
    }
    const remaining: Map<string, number> = new Map<string, number>();
    for (const line of BrowserAgentLiteDomDiffService.collectTextLines(previousTreeText)) {
      remaining.set(line, (remaining.get(line) ?? 0) + 1);
    }
    const newLines: string[] = [];
    for (const line of BrowserAgentLiteDomDiffService.collectTextLines(currentTreeText)) {
      const left = remaining.get(line) ?? 0;
      if (left > 0) {
        remaining.set(line, left - 1);
        continue;
      }
      newLines.push(line);
    }
    return BrowserAgentLiteDomDiffService.dropResplitText(newLines, remaining);
  }

  /**
   * 丢掉「原文被切碎」造成的假新增。
   *
   * 给正文里几个字加链接时，一个文本节点会被 <a> 切成三段：原来一行
   * 「欢迎访问我们的官网了解更多。」变成「欢迎访问我们的」「官网」「了解更多。」。
   * 三段都抵消不掉，于是全被报成「新出现的文本」——看着像冒出了新内容，实际一个字没多。
   * 判据是：这一行是某条消失了的行的子串。真正的新文案不会恰好是旧文案的一截。
   */
  private static dropResplitText(newLines: string[], remaining: Map<string, number>): string[] {
    // 这是 O(消失行 × 新增行) 的子串比较，而整页重绘时两边都可能上千行，
    // 且这一步跑在重绘守卫之前。行数一多，「文本被切碎」这个判据本身也没意义了，直接跳过。
    if (newLines.length <= 0 || newLines.length > BrowserAgentLiteDomDiffService.RESPLIT_MAX_LINES) {
      return newLines;
    }
    const vanished: string[] = [];
    remaining.forEach((count: number, line: string): void => {
      if (count > 0 && vanished.length < BrowserAgentLiteDomDiffService.RESPLIT_MAX_LINES) {
        vanished.push(line);
      }
    });
    if (vanished.length <= 0) {
      return newLines;
    }
    // 必须是「同一条消失的行里能找出至少两段新行」才算被切碎。只有一段是旧行子串时
    // 多半是文案真的换了（「手机号格式不正确，请重新输入」→「手机号格式不正确」），不能丢。
    const resplit: string[] = [];
    for (const old of vanished) {
      const pieces: string[] = [];
      for (const line of newLines) {
        if (old.length > line.length && old.includes(line)) {
          pieces.push(line);
        }
      }
      if (pieces.length < 2) {
        continue;
      }
      for (const piece of pieces) {
        if (!resplit.includes(piece)) {
          resplit.push(piece);
        }
      }
    }
    if (resplit.length <= 0) {
      return newLines;
    }
    const kept: string[] = [];
    for (const line of newLines) {
      if (!resplit.includes(line)) {
        kept.push(line);
      }
    }
    return kept;
  }

  /** 把 treeText 拆成「页面文案」行：剥掉结构标记，剩空的丢掉。 */
  private static collectTextLines(treeText: string): string[] {
    const lines: string[] = [];
    for (const raw of treeText.split('\n')) {
      const text = BrowserAgentLiteDomDiffService.stripTreeMarkup(raw);
      if (text.length > 1) {
        lines.push(text);
      }
    }
    return lines;
  }

  /**
   * 剥掉 print_tree 的结构标记，只留页面文案。
   *
   * 不去判断「整行是不是结构行」，而是把标记剥掉看还剩什么——剩空的就是纯结构行，
   * 剩文字的就是页面文案。这样写是因为结构行的形态会随 HtmlParser 演进而变：
   * 最早是 <div>，后来带上 element ID: 18，现在是 <div litedom=0> 文案 </div>
   * 且成对输出闭合标签。每变一次就去追一次正则，只会一次次漏。
   */
  private static stripTreeMarkup(line: string): string {
    let text = line.trim();
    let previous = '';
    while (previous !== text && text.length > 0) {
      previous = text;
      // 开头的 <tag ...> 或 </tag>
      text = text.replace(BrowserAgentLiteDomDiffService.TREE_OPEN_TAG_PATTERN, '').trim();
      // 结尾的 </tag>
      text = text.replace(BrowserAgentLiteDomDiffService.TREE_CLOSE_TAG_PATTERN, '').trim();
      // print_tree 会把 action-type 追加在标签之后，也是标记不是文案
      text = text.replace(BrowserAgentLiteDomDiffService.TREE_ACTION_TYPE_PATTERN, '').trim();
    }
    return text;
  }

  private static appendNewText(blocks: string[], newLines: string[], budget: number): void {
    if (newLines.length <= 0 || budget <= 0) {
      return;
    }
    const maxChars = BrowserAgentLiteDomDiffService.NEW_TEXT_MAX_CHARS < budget
      ? BrowserAgentLiteDomDiffService.NEW_TEXT_MAX_CHARS : budget;
    const lines: string[] = [];
    let used = 0;
    let shown = 0;
    for (let i = 0; i < newLines.length && shown < BrowserAgentLiteDomDiffService.NEW_TEXT_MAX_LINES; i++) {
      const text = BrowserAgentLiteDomDiffService.shorten(newLines[i]);
      if (used + text.length > maxChars) {
        break;
      }
      lines.push(`- ${text}`);
      used += text.length;
      shown += 1;
    }
    if (lines.length <= 0) {
      return;
    }
    if (newLines.length > shown) {
      lines.push(`- ...and ${newLines.length - shown} more new text lines`);
    }
    blocks.push(`### New text on the page (${newLines.length})\n${lines.join('\n')}`);
  }

  /**
   * 元素与动作目标的关联度。动作引发的变化绝大多数就发生在目标自身或其附近，
   * 用 xpath 前缀关系判断只需常数级字符串比较。
   */
  private static relevanceTo(xpath: string, targetXpath: string): number {
    if (targetXpath.length <= 0 || xpath.length <= 0) {
      return 0;
    }
    if (xpath === targetXpath) {
      return 100;
    }
    if (xpath.startsWith(`${targetXpath}/`)) {
      return 90;
    }
    if (targetXpath.startsWith(`${xpath}/`)) {
      return 80;
    }
    const left = xpath.split('/');
    const right = targetXpath.split('/');
    let common = 0;
    while (common < left.length && common < right.length && left[common] === right[common]) {
      common += 1;
    }
    return Math.min(70, common * 5);
  }

  /** 按相关性 → 文档顺序排序，保证配额优先留给与动作有关的元素。 */
  private static sortByRelevance(entries: PageChangeEntry[]): void {
    entries.sort((left: PageChangeEntry, right: PageChangeEntry): number => {
      if (left.relevance !== right.relevance) {
        return right.relevance - left.relevance;
      }
      return left.elementId - right.elementId;
    });
  }

  /**
   * 富文本编辑之后，把目标元素那一条的子树切到富文本规格。
   * 目标可能单列成一组，也可能已经落在 changed / appeared 里，三处都要找。
   */
  private static enableRichSubtree(
    target: PageChangeEntry | null,
    changed: PageChangeEntry[],
    appeared: PageChangeEntry[]
  ): void {
    if (target) {
      target.richSubtree = true;
      return;
    }
    for (const entry of changed.concat(appeared)) {
      if (entry.isActionTarget) {
        entry.richSubtree = true;
        return;
      }
    }
  }

  /**
   * 定位上一步动作目标在本次的对应元素。
   * 它已经落在 changed / appeared 里时就地打标记并返回 null——同一个元素不重复输出；
   * 只有当它一点变化都没有（不在任何组里）时，才单独返回一条供 target 组使用。
   */
  private static markOrExtractTarget(
    previousTargetId: number,
    currentNodes: AgentLiteDomNode[],
    matchedCurrent: number[],
    appeared: PageChangeEntry[],
    changed: PageChangeEntry[],
    disappeared: PageChangeEntry[],
    previousNodes: AgentLiteDomNode[],
    targetXpath: string
  ): PageChangeEntry | null {
    if (previousTargetId < 0) {
      return null;
    }
    for (let i = 0; i < currentNodes.length; i++) {
      if (matchedCurrent[i] !== previousTargetId) {
        continue;
      }
      for (const entry of appeared) {
        if (entry.elementId === i) {
          entry.isActionTarget = true;
          return null;
        }
      }
      for (const entry of changed) {
        if (entry.elementId === i) {
          entry.isActionTarget = true;
          return null;
        }
      }
      const standalone = BrowserAgentLiteDomDiffService.buildEntry('target', i, currentNodes[i], targetXpath);
      standalone.isActionTarget = true;
      return standalone;
    }
    // 本次找不到 —— 目标自己消失了。这在对话框 / 菜单 / 弹层里是常态，而且往往正是
    // 「动作被接受」的强信号，但原来一个字都没说：模型只看到「有几个东西消失了」，
    // 得自己回忆上一步点的是哪个。在消失组里把它标出来。
    if (previousTargetId < previousNodes.length) {
      const goneNode = previousNodes[previousTargetId];
      for (const entry of disappeared) {
        if (entry.node === goneNode) {
          entry.isActionTarget = true;
          entry.targetGone = true;
          return null;
        }
      }
    }
    return null;
  }

  /**
   * 唯一键配对：两侧各自建「键 -> 下标」索引，键在任一侧重复就整体丢弃，避免把不同元素配错。
   * 三轮共用这一套，区别只在 keyOf —— 每轮赌一个「什么东西不会变」的假设，从强赌到弱：
   * dataId（开发者给了稳定标识）→ xpath 精确（位置完全没动）→ xpath 尾部（相对近祖先没动）。
   */
  private static matchByUniqueKey(
    previousNodes: AgentLiteDomNode[],
    currentNodes: AgentLiteDomNode[],
    matchedPrevious: boolean[],
    matchedCurrent: number[],
    keyOf: (node: AgentLiteDomNode) => string
  ): void {
    const previousIndex = BrowserAgentLiteDomDiffService.indexUniqueKey(previousNodes, matchedPrevious, keyOf);
    const currentIndex = BrowserAgentLiteDomDiffService.indexUniqueKey(
      currentNodes, matchedCurrent.map((v) => v >= 0), keyOf);
    for (const entry of currentIndex.entries()) {
      const matchedIndex = previousIndex.get(entry[0]);
      if (matchedIndex === undefined || matchedPrevious[matchedIndex]) {
        continue;
      }
      matchedPrevious[matchedIndex] = true;
      matchedCurrent[entry[1]] = matchedIndex;
    }
  }

  /**
   * 第四轮：剩下的按语义指纹（标签 + 类型 + 文本 + 无障碍名）分组，组内按文档序两两配对。
   *
   * 这一轮不要求指纹唯一。真实页面上常有几十个长得一模一样的元素（京东每张商品卡里
   * 都有一个无文本的客服图标链接），任何基于内容的 key 都区分不了它们；但它们前后数量
   * 相等，按序配对等价于「整体对消」，正好避免误报成几十个消失 + 几十个出现。
   * 组内配错也没有可观测后果——它们在 litedom 层面本来就完全相同。
   */
  private static matchByFingerprint(
    previousNodes: AgentLiteDomNode[],
    currentNodes: AgentLiteDomNode[],
    matchedPrevious: boolean[],
    matchedCurrent: number[]
  ): void {
    const previousGroups = BrowserAgentLiteDomDiffService.groupByFingerprint(previousNodes, matchedPrevious);
    const currentGroups =
      BrowserAgentLiteDomDiffService.groupByFingerprint(currentNodes, matchedCurrent.map((v) => v >= 0));
    for (const entry of currentGroups.entries()) {
      const previousIndexes = previousGroups.get(entry[0]);
      if (previousIndexes === undefined) {
        continue;
      }
      const currentIndexes = entry[1];
      const pairs = Math.min(previousIndexes.length, currentIndexes.length);
      for (let i = 0; i < pairs; i++) {
        matchedPrevious[previousIndexes[i]] = true;
        matchedCurrent[currentIndexes[i]] = previousIndexes[i];
      }
    }
  }

  /** 按语义指纹把未配对元素分组，组内保持文档顺序。 */
  private static groupByFingerprint(
    nodes: AgentLiteDomNode[],
    alreadyMatched: boolean[]
  ): Map<string, number[]> {
    const groups: Map<string, number[]> = new Map<string, number[]>();
    for (let i = 0; i < nodes.length; i++) {
      if (alreadyMatched[i]) {
        continue;
      }
      const node = nodes[i];
      const fingerprint = [
        node.element.tagName.trim().toLowerCase(),
        node.element.type.trim(),
        node.text.trim().replace(/\s+/g, ' '),
        node.element.ariaLabel.trim(),
        node.element.title.trim(),
        node.element.placeholder.trim()
      ].join('|');
      const bucket = groups.get(fingerprint);
      if (bucket === undefined) {
        groups.set(fingerprint, [i]);
      } else {
        bucket.push(i);
      }
    }
    return groups;
  }

  /**
   * 建立「键 -> 下标」索引，只收未配对元素；键重复一律整体丢弃，避免把两个不同元素配错。
   * keyOf 返回空串表示该元素不参与本轮配对。
   */
  private static indexUniqueKey(
    nodes: AgentLiteDomNode[],
    alreadyMatched: boolean[],
    keyOf: (node: AgentLiteDomNode) => string
  ): Map<string, number> {
    const index: Map<string, number> = new Map<string, number>();
    const duplicated: Set<string> = new Set<string>();
    for (let i = 0; i < nodes.length; i++) {
      if (alreadyMatched[i]) {
        continue;
      }
      const key = keyOf(nodes[i]);
      if (key.length <= 0) {
        continue;
      }
      if (index.has(key)) {
        duplicated.add(key);
        continue;
      }
      index.set(key, i);
    }
    for (const key of duplicated) {
      index.delete(key);
    }
    return index;
  }

  /** dataId 键：空值与 React useId 形态不参与配对。 */
  private static dataIdKey(node: AgentLiteDomNode): string {
    const dataId = node.element.dataId.trim();
    if (dataId.length <= 1 || BrowserAgentLiteDomDiffService.GENERATED_ID_PATTERN.test(dataId)) {
      return '';
    }
    return dataId;
  }

  /** xpath 尾部键：段数不足时整条 xpath 即尾部，等价于精确匹配，无害。 */
  private static xpathSuffixKey(node: AgentLiteDomNode): string {
    if (node.xpath.length <= 0) {
      return '';
    }
    const segments = node.xpath.split('/');
    return segments.slice(
      Math.max(0, segments.length - BrowserAgentLiteDomDiffService.XPATH_SUFFIX_SEGMENTS)).join('/');
  }

  /**
   * 比较两个已配对元素的语义字段。
   * 刻意不比较 elementId（每次感知都会漂移）和几何（滚动即变），否则全是噪声。
   */


  /**
   * litedom 八个字段的变化，一律写成「旧 → 新」。
   *
   * 为什么给方向而不是只给现状：1024 → 1025 是点赞成功，1025 → 1024 是取消点赞，
   * 当前值一模一样而语义相反；value="" 也分不出是清空成功还是填入失败。
   *
   * 为什么不再单列一份字段名清单：「changed: text, state」这种摘要对模型没有信息量，
   * 后面括号里的具体值本身就说明了一切，两份并列只会让格式更难读。
   */
  private static contentDelta(previous: AgentLiteDomNode, current: AgentLiteDomNode): string {
    const parts: string[] = [];
    const a = previous.element;
    const b = current.element;
    BrowserAgentLiteDomDiffService.appendContentTransition('text', previous.text.trim(), current.text.trim(), parts);
    BrowserAgentLiteDomDiffService.appendContentTransition('value', a.value, b.value, parts);
    BrowserAgentLiteDomDiffService.appendContentTransition('tag', a.tagName, b.tagName, parts);
    BrowserAgentLiteDomDiffService.appendContentTransition('type', a.type, b.type, parts);
    BrowserAgentLiteDomDiffService.appendContentTransition('node type', previous.type, current.type, parts);
    BrowserAgentLiteDomDiffService.appendContentTransition('aria-label', a.ariaLabel, b.ariaLabel, parts);
    BrowserAgentLiteDomDiffService.appendContentTransition('title', a.title, b.title, parts);
    BrowserAgentLiteDomDiffService.appendContentTransition('placeholder', a.placeholder, b.placeholder, parts);
    BrowserAgentLiteDomDiffService.appendContentTransition(
      'options', a.options.join(' / '), b.options.join(' / '), parts);
    return parts.join('; ');
  }

  /** 生成单个字段的「旧 → 新」，过长时退化为字数对比。 */
  private static appendContentTransition(
    name: string,
    before: string,
    after: string,
    parts: string[]
  ): void {
    if (before === after) {
      return;
    }
    if (before.length <= BrowserAgentLiteDomDiffService.STATE_VALUE_MAX_CHARS &&
      after.length <= BrowserAgentLiteDomDiffService.STATE_VALUE_MAX_CHARS) {
      const left = before.length > 0 ? JSON.stringify(before) : '(empty)';
      const right = after.length > 0 ? JSON.stringify(after) : '(empty)';
      parts.push(`${name} ${left} → ${right}`);
      return;
    }
    // 长文本不给字数对比——大容器的 text 被 litedom 截到同一长度时，「2048 chars → 2048 chars」
    // 是一句完全没有信息量的话。改成剥掉公共前后缀，只给真正不同的那一段。
    const region = BrowserAgentLiteDomDiffService.differingRegion(before, after);
    parts.push(`${name} ${region}`);
  }

  /**
   * 两段长文本的差异区段：剥掉公共前缀与公共后缀，只保留真正不同的那一段，
   * 并带上两侧各 12 个字符的上下文——光给「"0" → "1"」定位得到却读不懂，
   * 带上下文才知道变的是「已选(0/25)」这个计数器。
   */
  private static differingRegion(before: string, after: string): string {
    let start = 0;
    while (start < before.length && start < after.length && before[start] === after[start]) {
      start += 1;
    }
    let endBefore = before.length;
    let endAfter = after.length;
    while (endBefore > start && endAfter > start && before[endBefore - 1] === after[endAfter - 1]) {
      endBefore -= 1;
      endAfter -= 1;
    }
    const left = BrowserAgentLiteDomDiffService.regionPiece(before, start, endBefore);
    const right = BrowserAgentLiteDomDiffService.regionPiece(after, start, endAfter);
    return `${left} → ${right}`;
  }

  /** 差异区段的一侧：前后各留一点上下文，超长截断，被剥掉的部分用省略号标出。 */
  private static regionPiece(source: string, start: number, end: number): string {
    const context = BrowserAgentLiteDomDiffService.REGION_CONTEXT_CHARS;
    const cap = BrowserAgentLiteDomDiffService.STATE_VALUE_MAX_CHARS;
    const from = start - context > 0 ? start - context : 0;
    const limit = end - start > cap ? start + cap : end;
    const to = limit + context < source.length ? limit + context : source.length;
    const piece = source.substring(from, to);
    if (piece.length <= 0) {
      return '(nothing)';
    }
    const head = from > 0 ? '...' : '';
    const tail = to < source.length ? '...' : '';
    return `"${head}${piece}${tail}"`;
  }

  /**
   * 元素周边的一棵小树：上溯 2 层祖先、下探 2 层后代，每个节点只列变化了的属性。
   * 没有变化的节点只有在它的后代有变化时才出现，用来表明位置。整棵树为空表示状态没变。
   *
   * 为什么不是扁平的一串「parent <li> class gained x, a descendant gained y」：那种写法
   * 说得出「变了什么」，说不出「在哪变的」——后代有好几层时根本分不清是哪一个节点。
   */
  private static buildStateTree(
    previous: AgentLiteDomNode,
    current: AgentLiteDomNode,
    previousParser: HtmlParser | null,
    currentParser: HtmlParser | null
  ): string {
    const previousNode = BrowserAgentLiteDomDiffService.resolveParsedNode(previous, previousParser);
    const currentNode = BrowserAgentLiteDomDiffService.resolveParsedNode(current, currentParser);
    if (!previousNode || !currentNode) {
      return '';
    }
    const selfLines: string[] = [];
    const selfChanged = BrowserAgentLiteDomDiffService.appendTreeLines(
      previousNode, currentNode, BrowserAgentLiteDomDiffService.STATE_SUBTREE_DEPTH, 0, true, selfLines);

    // 祖先只取变化了的，由外向内排。没变的祖先不占行——层级由缩进和 <-- this element 表明。
    const ancestors: string[] = [];
    let previousAncestor: Node | null = previousNode.fa;
    let currentAncestor: Node | null = currentNode.fa;
    let depth = 1;
    while (previousAncestor && currentAncestor &&
      depth <= BrowserAgentLiteDomDiffService.STATE_ANCESTOR_DEPTH) {
      const attributes = BrowserAgentLiteDomDiffService.nodeAttrDelta(previousAncestor, currentAncestor);
      if (attributes.length > 0) {
        ancestors.unshift(`<${previousAncestor.tag_name.toLowerCase()}>${attributes}`);
      }
      previousAncestor = previousAncestor.fa;
      currentAncestor = currentAncestor.fa;
      depth += 1;
    }
    if (!selfChanged && ancestors.length <= 0) {
      return '';
    }
    const lines: string[] = [];
    for (let i = 0; i < ancestors.length; i++) {
      lines.push(`${' '.repeat(i * 2)}${ancestors[i]}`);
    }
    for (const line of selfLines) {
      lines.push(`${' '.repeat(ancestors.length * 2)}${line}`);
    }
    return lines.join('\n');
  }

  /**
   * 递归打印一个节点。返回这棵子树里有没有变化——没有变化又不是元素自身的节点直接剪掉，
   * 否则一棵没动过的子树会把整个小树撑满。
   */
  private static appendTreeLines(
    previous: Node | null,
    current: Node | null,
    depthLeft: number,
    indent: number,
    isSelf: boolean,
    lines: string[]
  ): boolean {
    const node = current ?? previous;
    if (!node || node.tag_name === 'text') {
      return false;
    }
    let attributes = '';
    if (previous && current) {
      attributes = BrowserAgentLiteDomDiffService.nodeAttrDelta(previous, current);
    } else if (current) {
      attributes = ` added${BrowserAgentLiteDomDiffService.nodeAttrCurrent(current)}`;
    } else {
      attributes = ' removed';
    }
    const childLines: string[] = [];
    let childChanged = false;
    if (depthLeft > 0) {
      const previousChildren = previous ? previous.children : [];
      const currentChildren = current ? current.children : [];
      const count = previousChildren.length > currentChildren.length
        ? previousChildren.length : currentChildren.length;
      for (let i = 0; i < count; i++) {
        const previousChild = previousChildren[i] ?? null;
        const currentChild = currentChildren[i] ?? null;
        // 同一位置上标签都变了，多半是删了一个又插了一个，而不是同一个元素改了 class。
        // 按同一个元素去报会凭空造出「class "old-a" → "clearFont"」这种假变化。
        const samePair = !previousChild || !currentChild ||
          previousChild.tag_name === currentChild.tag_name;
        if (samePair) {
          childChanged = BrowserAgentLiteDomDiffService.appendTreeLines(
            previousChild, currentChild, depthLeft - 1, indent + 1, false, childLines) || childChanged;
          continue;
        }
        // 两边都要走一遍，不能用 || 短路，否则「新增」那条永远打不出来。
        const removed = BrowserAgentLiteDomDiffService.appendTreeLines(
          previousChild, null, depthLeft - 1, indent + 1, false, childLines);
        const added = BrowserAgentLiteDomDiffService.appendTreeLines(
          null, currentChild, depthLeft - 1, indent + 1, false, childLines);
        childChanged = childChanged || removed || added;
      }
    }
    const changed = attributes.length > 0 || childChanged;
    if (!changed && !isSelf) {
      return false;
    }
    const marker = isSelf ? '  <-- this element' : '';
    lines.push(`${' '.repeat(indent * 2)}<${node.tag_name.toLowerCase()}>${attributes}${marker}`);
    for (const line of childLines) {
      lines.push(line);
    }
    return changed;
  }

  /** 一个节点上变化了的属性。class 给前后全量，布尔属性说加上/去掉。 */
  private static nodeAttrDelta(previousNode: Node, currentNode: Node): string {
    const parts: string[] = [];
    const previousClass = (previousNode.attributes.class ?? '').trim();
    const currentClass = (currentNode.attributes.class ?? '').trim();
    if (previousClass !== currentClass) {
      parts.push(`class ${BrowserAgentLiteDomDiffService.treeValue(previousClass)} → ` +
        `${BrowserAgentLiteDomDiffService.treeValue(currentClass)}`);
    }
    for (const name of BrowserAgentLiteDomDiffService.stateAttributeNames(previousNode, currentNode, true)) {
      const before = BrowserAgentLiteDomDiffService.attributeValue(previousNode, name);
      const after = BrowserAgentLiteDomDiffService.attributeValue(currentNode, name);
      if (before === after) {
        continue;
      }
      // 布尔属性（disabled / open / selected）只有有无之分，说「加上了 / 去掉了」。
      if ((before === '(set)' || after === '(set)') && (before.length <= 0 || after.length <= 0)) {
        parts.push(`attribute "${name}" ${after.length > 0 ? 'added' : 'removed'}`);
        continue;
      }
      parts.push(`${name} ${BrowserAgentLiteDomDiffService.treeValue(before)} → ` +
        `${BrowserAgentLiteDomDiffService.treeValue(after)}`);
    }
    return parts.length > 0 ? ` ${parts.join(', ')}` : '';
  }

  /** 新出现的节点没有「之前」可比，改给当前的 class / role / aria-*。 */
  private static nodeAttrCurrent(node: Node): string {
    const parts: string[] = [];
    for (const name of Object.keys(node.attributes)) {
      // 和 attrs 一样设上限：电商站点插一张商品卡能带十几二十个属性，
      // 不封顶就会出现一行两百多字符、其中大半是 data-sku / aria-posinset 这类。
      if (parts.length >= BrowserAgentLiteDomDiffService.MAX_ATTRIBUTES) {
        parts.push('...');
        break;
      }
      const raw = (node.attributes[name] ?? '').trim();
      // style 只在「新增节点」上取。富文本的颜色、字号就写在这里——新插进来的
      // <span style="color: rgb(224,32,32)"> 不给 style 就只剩一个光秃秃的 <span>，
      // 完全看不出改了什么。而已存在节点的 style 变化几乎全是动画与布局
      //（实测滚动一屏，30 个节点里 14 个的 style 变了，没有一个是状态），
      // 所以 nodeAttrDelta 那边始终不比 style。
      const value = name.toLowerCase() === 'style'
        ? raw
        : BrowserAgentLiteDomDiffService.acceptAttribute(name, raw);
      if (value.length > 0) {
        parts.push(`${name.toLowerCase()}=${BrowserAgentLiteDomDiffService.treeValue(value)}`);
      }
    }
    const text = BrowserAgentLiteDomDiffService.directText(node);
    if (text.length > 0) {
      parts.push(`text=${BrowserAgentLiteDomDiffService.treeValue(text)}`);
    }
    return parts.length > 0 ? ` ${parts.join(' ')}` : '';
  }

  /**
   * 节点自己那一层的文本：自身的 text，没有就取直接子文本节点。
   * 新增节点全靠它认身份——插入的 <a href="..."> 光有 href 看不出链的是哪几个字。
   */
  private static directText(node: Node): string {
    const own = node.text ? node.text.trim() : '';
    if (own.length > 0) {
      return own;
    }
    let text = '';
    for (const child of node.children) {
      if (child.tag_name === 'text' && child.text) {
        text += child.text.trim();
      }
    }
    return text.trim();
  }

  /** 小树里的属性值：class 要给前后全量，所以用比别处更宽的上限。 */
  private static treeValue(value: string): string {
    if (value.length <= 0) {
      return '(none)';
    }
    return BrowserAgentUtf8TextService.exceeds(value, BrowserAgentLiteDomDiffService.TREE_VALUE_MAX_CHARS)
      ? `"${BrowserAgentUtf8TextService.truncateToChars(value,
        BrowserAgentLiteDomDiffService.TREE_VALUE_MAX_CHARS)}..."`
      : `"${value}"`;
  }

  /**
   * 可比对的属性名并集。aria-* 与 DIFF_ATTRIBUTES 自身和祖先都比；
   * href 与业务型 data-* 只比自身——祖先整块重渲染时这两类最容易带出噪声。
   */
  private static stateAttributeNames(previousNode: Node, currentNode: Node, wide: boolean): string[] {
    const names: string[] = [];
    const collect = (node: Node): void => {
      for (const name of Object.keys(node.attributes)) {
        const key = name.toLowerCase();
        if (names.includes(name) || key === 'class' ||
          BrowserAgentLiteDomDiffService.LITEDOM_COVERED_ATTRIBUTES.includes(key)) {
          continue;
        }
        if (key.startsWith('aria-')) {
          names.push(name);
        } else if (BrowserAgentLiteDomDiffService.DIFF_ATTRIBUTES.includes(key)) {
          names.push(name);
        } else if (wide && (key === 'href' ||
          (key.startsWith('data-') && !BrowserAgentLiteDomDiffService.TRACKING_KEY_PATTERN.test(key)))) {
          names.push(name);
        }
      }
    };
    collect(previousNode);
    collect(currentNode);
    names.sort();
    return names;
  }

  /** 属性的可比对值：缺失返回空串，有键无值（disabled / open）返回 '(set)'，href 去掉追踪 query。 */
  private static attributeValue(node: Node, name: string): string {
    const raw = node.attributes[name];
    if (typeof raw !== 'string') {
      return '';
    }
    const value = raw.trim();
    if (value.length <= 0) {
      return '(set)';
    }
    if (name.toLowerCase() === 'href') {
      const queryIndex = value.indexOf('?');
      return queryIndex >= 0 ? value.substring(0, queryIndex) : value;
    }
    return value;
  }

  private static buildEntry(
    kind: string,
    elementId: number,
    node: AgentLiteDomNode,
    targetXpath: string
  ): PageChangeEntry {
    const entry = new PageChangeEntry();
    entry.kind = kind;
    entry.elementId = elementId;
    entry.node = node;
    entry.relevance = BrowserAgentLiteDomDiffService.relevanceTo(node.xpath, targetXpath);
    return entry;
  }

  /** 还能用多少字符。各组按重要性依次追加，预算用完后面的组整组被挤掉。 */
  private static remainingBudget(blocks: string[]): number {
    let used = 0;
    for (const block of blocks) {
      used += block.length + 1;
    }
    return BrowserAgentLiteDomDiffService.OUTPUT_MAX_CHARS - used;
  }

  /** 按该组配额输出，同时受总预算约束。 */
  private static appendGroup(
    blocks: string[],
    title: string,
    entries: PageChangeEntry[],
    parser: HtmlParser | null,
    quota: GroupQuota,
    budget: number
  ): void {
    if (entries.length <= 0 || budget <= 0) {
      return;
    }
    const maxChars = quota.maxChars < budget ? quota.maxChars : budget;
    const lines: string[] = [];
    let used = 0;
    let shown = 0;
    for (let i = 0; i < entries.length && shown < quota.maxEntries; i++) {
      const withSubtree = shown < quota.withSubtree;
      const line = BrowserAgentLiteDomDiffService.formatEntry(
        entries[i], quota.withAttributes ? parser : null, withSubtree);
      if (line.length <= 0) {
        continue;
      }
      if (used + line.length > maxChars && shown > 0) {
        break;
      }
      lines.push(line);
      used += line.length;
      shown += 1;
    }
    if (entries.length > shown && shown > 0) {
      lines.push(`- ...and ${entries.length - shown} more`);
    }
    if (lines.length <= 0) {
      return;
    }
    blocks.push(`### ${title} (${entries.length})\n${lines.join('\n')}`);
  }

  private static formatEntry(entry: PageChangeEntry, parser: HtmlParser | null, withSubtree: boolean): string {
    const node = entry.node;
    if (!node) {
      return '';
    }
    const tagName = node.element.tagName.trim().length > 0 ? node.element.tagName.trim().toLowerCase() : 'unknown';
    // 消失的元素不给 elementId：那是上一次感知的编号，本次已失效。
    const parts: string[] = [BrowserAgentLiteDomDiffService.formatEntryHead(entry, tagName)];
    BrowserAgentLiteDomDiffService.pushTextAndContentDelta(node, entry, parts);
    BrowserAgentLiteDomDiffService.enrichWithParsedNode(entry, parser, withSubtree, parts);
    // 只剩一个 "- <a>" 的空壳条目零信息量，不如不占一行（消失组最容易出现这种）。
    // 但动作目标例外：它没有文本、属性又解析不到（shadow DOM、xpath 查不到）时同样是空壳，
    // 丢掉就把「你点的那个东西不见了」这条唯一的线索也丢了。
    if (parts.length <= 1 && entry.stateTree.length <= 0 && !entry.isActionTarget) {
      return '';
    }
    return BrowserAgentLiteDomDiffService.buildStateTreeSection(parts, entry);
  }

  /** 条目头：根据是否有可用 elementId 选择带编号或裸标签的格式，action 目标追加标记。 */
  private static formatEntryHead(entry: PageChangeEntry, tagName: string): string {
    const marker = entry.isActionTarget
      ? (entry.targetGone ? ' [action target, no longer on the page]' : ' [action target]')
      : '';
    return entry.elementId >= 0
      ? `- ${entry.elementId} <${tagName}>${marker}` : `- <${tagName}>${marker}`;
  }

  /** 文本与内容差异段：均非空才追加，保持顺序（text 在前，contentDelta 在后）。 */
  private static pushTextAndContentDelta(node: AgentLiteDomNode, entry: PageChangeEntry, parts: string[]): void {
    const text = BrowserAgentLiteDomDiffService.shorten(node.text);
    if (text.length > 0) {
      parts.push(`text=${JSON.stringify(text)}`);
    }
    if (entry.contentDelta.length > 0) {
      parts.push(`changed: ${entry.contentDelta}`);
    }
  }

  /**
   * 解析树增强段：拿到 parsedNode 后按序追加 attrs / ancestors / subtree。
   * ancestors 仅在没有 stateTree 时给（小树已含状态就不再重复祖先）；
   * subtree 仅在 withSubtree 且（无 stateTree 或为富文本目标）时给。
   */
  private static enrichWithParsedNode(
    entry: PageChangeEntry,
    parser: HtmlParser | null,
    withSubtree: boolean,
    parts: string[],
  ): void {
    const parsedNode = BrowserAgentLiteDomDiffService.resolveParsedNode(entry.node, parser);
    if (!parsedNode) {
      return;
    }
    const attributes = BrowserAgentLiteDomDiffService.formatAttributes(parsedNode);
    if (attributes.length > 0) {
      parts.push(`attrs: ${attributes}`);
    }
    const hasStateTree = entry.stateTree.length > 0;
    if (!hasStateTree) {
      // 没有小树可给的条目（appeared / disappeared / 一点没变的目标）退回给现状：
      // 祖先的当前属性 + 浅层子树。一个新出现的 <span class="el-checkbox__inner">
      // 自身没有子节点、状态在父节点上，不给祖先就完全看不出它到底勾没勾上。
      const ancestors = BrowserAgentLiteDomDiffService.ancestorState(parsedNode);
      if (ancestors.length > 0) {
        parts.push(`ancestors: ${ancestors}`);
      }
    }
    if (withSubtree && (!hasStateTree || entry.richSubtree)) {
      // 富文本编辑的目标要看正文结构（<strong> / style），小树只有属性不够，子树照给。
      const subtree = BrowserAgentLiteDomDiffService.formatSubtree(parsedNode, entry.richSubtree);
      if (subtree.length > 0) {
        parts.push(`subtree: ${subtree}`);
      }
    }
  }

  /** 拼接条目头部与可选的 stateTree（缩进 4 空格）。无 stateTree 时只返回头部行。 */
  private static buildStateTreeSection(parts: string[], entry: PageChangeEntry): string {
    const headLine = parts.join(' | ');
    if (entry.stateTree.length <= 0) {
      return headLine;
    }
    const indented: string[] = [headLine];
    for (const line of entry.stateTree.split('\n')) {
      indented.push(`    ${line}`);
    }
    return indented.join('\n');
  }

  /** 祖先（最多两层）的状态属性现状：class / role / aria-*，没有状态属性的祖先跳过。 */
  private static ancestorState(parsedNode: Node): string {
    const parts: string[] = [];
    let ancestor = parsedNode.fa;
    let depth = 1;
    while (ancestor && depth <= BrowserAgentLiteDomDiffService.STATE_ANCESTOR_DEPTH) {
      const pairs: string[] = [];
      const className = (ancestor.attributes.class ?? '').trim();
      if (className.length > 0) {
        pairs.push(`class="${BrowserAgentLiteDomDiffService.shorten(className)}"`);
      }
      for (const name of Object.keys(ancestor.attributes)) {
        const key = name.toLowerCase();
        const value = (ancestor.attributes[name] ?? '').trim();
        if ((key === 'role' || key.startsWith('aria-')) && value.length > 0) {
          pairs.push(`${key}="${BrowserAgentLiteDomDiffService.shorten(value)}"`);
        }
      }
      if (pairs.length > 0) {
        const label = depth === 1 ? 'parent' : 'ancestor';
        parts.push(`${label} <${ancestor.tag_name.toLowerCase()}> ${pairs.join(' ')}`);
      }
      ancestor = ancestor.fa;
      depth += 1;
    }
    return parts.join('; ');
  }

  /** 从整页 HTML 解析树按 xpath 取回该元素（litedom 只保留了极少字段）。 */
  private static resolveParsedNode(node: AgentLiteDomNode, parser: HtmlParser | null): Node | null {
    if (!parser || node.xpath.length <= 0 || node.xpath.includes('shadow-root')) {
      return null;
    }
    return parser.getNodeFromXpath(node.xpath);
  }

  /** 只输出能表达语义或状态的属性：埋点 data-*、style、src 这类是纯噪声，会吃满预算。 */
  private static formatAttributes(parsedNode: Node): string {
    const pairs: string[] = [];
    for (const name of Object.keys(parsedNode.attributes)) {
      if (pairs.length >= BrowserAgentLiteDomDiffService.MAX_ATTRIBUTES) {
        pairs.push('...');
        break;
      }
      const value = BrowserAgentLiteDomDiffService.acceptAttribute(name, parsedNode.attributes[name]);
      if (value.length <= 0) {
        continue;
      }
      pairs.push(`${name}=${JSON.stringify(BrowserAgentLiteDomDiffService.shorten(value))}`);
    }
    return pairs.join(', ');
  }

  /** 判定属性是否保留，并对 href 之类做净化；返回空串表示丢弃。 */
  private static acceptAttribute(name: string, rawValue: string): string {
    if (typeof rawValue !== 'string' || rawValue.trim().length <= 0) {
      return '';
    }
    const key = name.toLowerCase();
    const value = rawValue.trim();
    if (key.startsWith('aria-')) {
      return value;
    }
    if (key.startsWith('data-')) {
      // 埋点键名直接丢；业务型 data-* 通常很短（data-liked="1"、data-state="open"）。
      if (BrowserAgentLiteDomDiffService.TRACKING_KEY_PATTERN.test(key) ||
        value.length > BrowserAgentLiteDomDiffService.DATA_VALUE_MAX_CHARS) {
        return '';
      }
      return value;
    }
    if (!BrowserAgentLiteDomDiffService.KEEP_ATTRIBUTES.includes(key)) {
      return '';
    }
    if (key === 'href') {
      // 追踪参数全在 query 上，只留路径。
      const queryIndex = value.indexOf('?');
      return queryIndex >= 0 ? value.substring(0, queryIndex) : value;
    }
    return value;
  }

  /**
   * 输出浅层子树骨架。状态标记经常挂在图标子节点上（微博 woo-font--liked、
   * 编辑器按钮的 active 类），只看元素自身属性会整个漏掉。
   */
  private static formatSubtree(parsedNode: Node, rich: boolean): string {
    if (parsedNode.children.length <= 0) {
      return '';
    }
    const depth = rich
      ? BrowserAgentLiteDomDiffService.RICH_SUBTREE_DEPTH : BrowserAgentLiteDomDiffService.SUBTREE_DEPTH;
    const maxChars = rich
      ? BrowserAgentLiteDomDiffService.RICH_SUBTREE_MAX_CHARS : BrowserAgentLiteDomDiffService.SUBTREE_MAX_CHARS;
    let html = '';
    for (const child of parsedNode.children) {
      html += BrowserAgentLiteDomDiffService.subtreeHtml(child, depth, rich);
      if (html.length > maxChars) {
        return `${html.substring(0, maxChars)}...`;
      }
    }
    return html;
  }

  private static subtreeHtml(node: Node, depthLeft: number, rich: boolean): string {
    const text = node.text ? node.text.trim() : '';
    if (node.tag_name === 'text') {
      return BrowserAgentLiteDomDiffService.shorten(text);
    }
    const tagName = node.tag_name.toLowerCase();
    let attributes = '';
    for (const name of Object.keys(node.attributes)) {
      const value = node.attributes[name];
      const keepList = rich
        ? BrowserAgentLiteDomDiffService.RICH_SUBTREE_KEEP_ATTRIBUTES
        : BrowserAgentLiteDomDiffService.SUBTREE_KEEP_ATTRIBUTES;
      const keep = name.toLowerCase().startsWith('aria-') || keepList.includes(name);
      if (keep && value && value.trim().length > 0) {
        attributes += ` ${name}="${BrowserAgentLiteDomDiffService.shorten(value)}"`;
      }
    }
    const openTag = `<${tagName}${attributes}>`;
    if (BrowserAgentLiteDomDiffService.SUBTREE_VOID_TAGS.includes(tagName)) {
      return openTag;
    }
    if (depthLeft <= 0) {
      return `${openTag}...</${tagName}>`;
    }
    const maxChars = rich
      ? BrowserAgentLiteDomDiffService.RICH_SUBTREE_MAX_CHARS : BrowserAgentLiteDomDiffService.SUBTREE_MAX_CHARS;
    let inner = text;
    for (const child of node.children) {
      inner += BrowserAgentLiteDomDiffService.subtreeHtml(child, depthLeft - 1, rich);
      if (inner.length > maxChars) {
        break;
      }
    }
    return `${openTag}${inner}</${tagName}>`;
  }

  private static shorten(value: string): string {
    const normalized = value.trim().replace(/\s+/g, ' ');
    if (!BrowserAgentUtf8TextService.exceeds(normalized, BrowserAgentLiteDomDiffService.MAX_VALUE_CHARS)) {
      return normalized;
    }
    return `${BrowserAgentUtf8TextService.truncateToChars(normalized,
      BrowserAgentLiteDomDiffService.MAX_VALUE_CHARS)}...`;
  }
}
