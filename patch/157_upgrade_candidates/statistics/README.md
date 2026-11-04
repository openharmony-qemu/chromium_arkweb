# 候选补丁的来源与未收入项统计

这份目录回答两个问题：`patches/` 的 30 份候选从哪些固定代码点产生，以及**没有收入候选的修改在哪里、因为什么被排除或暂缓**。资料来自 `ArkWeb_Helper/Knowledge/sujiahao/通用/需求/ArkWeb157尝鲜分支/01-补丁生成阶段/`；[`source_index.tsv`](source_index.tsv) 对本目录每个复制文件逐项记录原路径、归档与原文件各自的字节数和 SHA-256，以及必要的整理动作。原始账本中的相对链接仍指向 Knowledge 目录结构；在本仓阅读时优先使用下面的导航。

`web_webview` 是独立的 ArkWeb 自有子仓，按 A₁ XML 的固定提交整仓跟踪。因此这里的 D 逐文件审核不处理它；原始 D 的 WebView 补丁及索引行也已移除。固定 XML 与 [`migration_items/`](migration_items/) 中的整仓记录仍保留该提交点。

## 阅读顺序

1. 先看 [`snapshots/`](snapshots/) 的四份 XML。它们固定多仓各子仓的 Git SHA；主目录 [README](../README.md) 解释 B₀、A₀、A₁、B₁。
2. 查某个文件是否进入候选：在 [`patch_index.tsv`](../patch_index.tsv) 找对应子仓的补丁，再到 [`D_review/4010项D阶段覆盖账本.tsv`](D_review/4010项D阶段覆盖账本.tsv) 查 D 阶段文件去向。H 阶段例外查 `H_exclusions/`。
3. 查没有收入候选的具体改动：D 的原始代码差异在 [`D_review/raw_diff/`](D_review/raw_diff/)，原始 patch 文件名及两端 SHA 在其 `index.tsv`；D 账本中的 `D_patch_location` 可定位到原始代码行。H 的 81 项表则给出固定 SHA、对应 PR 和取回命令。
4. 需要判断排除是否正确时，按下面的“排除原因”及逐项账本复核，不把“未收入候选”直接当成“永不迁移”。

## 固定输入：`snapshots/`

| 文件 | 原来源 | 作用 |
|---|---|---|
| `arkweb-144-prebulk-20260707-000000-generated-20261028-142311.xml` | Knowledge `01-固定快照/` | 历史 ArkWeb A₀；生成 H 的 ArkWeb 端、D 的起点 |
| `chromium-baseline-prebulk-20260707-000000-generated-20261028-142311.xml` | 同上 | 历史 Chromium B₀；生成 H 的 Chromium 端，也是 30 份候选的验证基底 |
| `arkweb-144-master-20261027-192826.xml` | 同上 | 最新 ArkWeb A₁；生成 D 的终点及本次 157 占位的来源 |
| `chromium-baseline-144.0.7559.59-20261027-192943.xml` | 同上 | 最新 Chromium B₁；供最新来源复核，不是 30 份候选的直接应用基底 |

## H 排除：`H_exclusions/`

H 是历史 Chromium B₀ 到历史 ArkWeb A₀ 的文件差异。原则上纳入自研候选，但其中 **81 个文件级差异**与 baseline 已合入的上游提交构成精确反向关系，故只排除这些差异的 H 自研来源；同文件其他修改及 D 修改不因此一并排除。

| 文件 | 原来源 | 看什么 |
|---|---|---|
| `81项反向来源.tsv` | Knowledge `02-原始差异/H-20260707-before-bulk/来源排除/` | 每项的子仓、文件、历史 blob 与排除决定；先从这里确认是否属于 81 项 |
| `81项提交依据.tsv` | 同上 | 对应 baseline 提交说明、两端精确反向匹配的证据 |
| `81项反向差异PR追溯.tsv` | 同上 | 原始 H patch 位置、PR 链接、合入 SHA、取回该文件修改的命令；复核具体代码用此表 |
| `CEF-3664-14项核对.md` | 同上 | 81 项中的 14 项 CEF 历史反向差异及 141 到 144 的来源链；14 项已包含在 81 项里 |
| `构建工具链差异待确认.md` | Knowledge `02-原始差异/H-20260707-before-bulk/` | sysroot 和 `depot_tools` 的大块差异为何暂不当成业务源码补丁，待构建阶段确认哪些文件确需迁移 |

历史 H 原始 patch 仍保存在上述 Knowledge 目录；它含大量工具链文件。这里的 81 项表保留了复核代码所需的固定提交、PR 和取回命令。

## D 排除与待审：`D_review/`

D 是历史 ArkWeb A₀ 到最新 ArkWeb A₁ 的变化，共 **4,010 个差异文件**。先排除明确的上游漏洞或社区回合 PR，再检查其余修改的 ArkWeb/OHOS 平台宏、平台作用范围及同 PR 功能配套。以下资料按“总去向 → 具体代码 → PR 来源”使用：

| 文件或目录 | 原来源 | 看什么 |
|---|---|---|
| `4010项D阶段覆盖账本.tsv` | Knowledge `03-D候选筛选/03-候选输出/D-来源分流/` | **当前总去向**：逐文件记录状态、原因、上游 PR 和自研 PR；588 个文件级候选、2,845 个明确上游来源、26 个汇总 PR 的上游安全子 PR、6 个已复核同 PR 误报、545 个待审 |
| `待复核177笔非正向宏PR.tsv` | 同上 | 545 个待审文件关联的 177 笔无正向平台宏 PR，含 PR 和原始 D patch 位置；它们没有静默丢弃 |
| `全量PR宏与来源索引.tsv` | Knowledge `03-D候选筛选/02-归属核对/D-全量PR配套复核/` | 1,568 笔期间 PR 的合入 SHA、来源标签及平台宏线索，供核对总账本的 PR 判定 |
| `上游排除项自研适配风险账本.tsv` | 同上 | 被排除的上游 PR 与 D 文件关联：逐项给 PR、SHA、改动摘录、原始 patch 位置和取回命令；用于发现上游回合中可能夹带的 ArkWeb 适配 |
| `上游排除适配风险摘要.json` | 同上 | 上述风险账本的数量摘要；优先查看有平台宏或平台文字的关联，不直接据此认定自研 |
| `全部文件复核.tsv` | Knowledge `03-D候选筛选/02-归属核对/D-人工确认表/` | 4,010 个文件的文件名、D 原始位置、简短代码片段和本地提交/PR 线索；属于较早的人工复核底表，**最终去向以覆盖账本为准** |
| `9269子PR逐文件归属.tsv` | Knowledge `03-D候选筛选/02-归属核对/D-PR配套补漏/` | 汇总 PR `chromium_src!9269` 中安全子 PR 和自研子 PR 的逐文件拆分，避免按汇总 PR 整批收入或剔除 |
| `D普通上游排除PR逐项复核.md` | Knowledge `复核附表/` | 1,133 笔普通上游排除 PR 的人工复核表，留有结论栏；原始文件改动仍由上游风险账本定位 |
| `raw_diff/index.tsv` 与 `raw_diff/*.patch` | Knowledge `02-原始差异/D-20260707-to-20261027/` | A₀ 到 A₁ 的 **17 份逐仓原始 diff**，包含后来被剔除的改动；`index.tsv` 记录子仓、两端 SHA、文件名和 SHA-256。原件有一行 WebView，归档时按整仓跟踪原则移除；其余行与原件一致。这些文件是核对原始代码的证据，不能整批打到 157 |

上表中的“上游排除”是**来源判断**：该 PR 主要是上游修复，生成 D 候选时不整批收入；可能夹带的自研适配已保留风险位置，留给审核或 157 应用阶段处理。“待审”表示现有宏与 PR 证据不足，不等于已判定为上游。

## 整仓和整目录项：`migration_items/`

三个 ArkWeb 独有子仓 `chromium_arkweb`、`web_webview`、`third_party_bounds_checking_function` 的固定提交在 A₁ XML 中。共有仓内部的 ArkWeb/OHOS/EXT 专属目录另行记录，不从 30 份文件级候选中截取一部分代替整目录。

| 文件 | 原来源 | 看什么 |
|---|---|---|
| `folder_migration_items.tsv` | Knowledge `04-迁移项/整目录迁移候选/` | 18 个整目录项的子仓、目录、最新树 SHA、文件数及候选或待确认状态 |
| `named_directories_review.tsv` | 同上 | 168 个按名称扫描过的目录及 baseline 存在性，说明为何选出上述整目录项 |

## 文件完整性

[`source_index.tsv`](source_index.tsv) 为本目录 **37 个来源文件**逐项保存原始工作区路径，以及原文件和归档文件各自的字节数、SHA-256。除 `D_review/raw_diff/index.tsv` 按上述规则移除 WebView 行外，其余 36 个文件与来源逐字节相同。30 份合成候选补丁的来源与 SHA-256 在主目录 [`patch_index.tsv`](../patch_index.tsv)。这些索引用于证明本次入库材料与固定的 Knowledge 产物之间的关系；审核结论变化后应生成新版本，不覆写旧证据。
