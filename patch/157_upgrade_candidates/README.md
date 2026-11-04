# ArkWeb 157 升级候选资料

这里保存 ArkWeb 144 相对 Chromium 144 的自研修改候选，供后续迁移到 Chromium 157。目录属于 `chromium_arkweb` 仓的 `patch/`，但其中的文件是**升级资料**，目前没有被 157 的构建流程应用。它们还不是通过 157 编译的正式补丁。

## 先理解四个代码点

多仓源码的每个子仓各有 Git 提交，因此用 [四份 XML](statistics/snapshots/) 固定所有子仓的提交点，而不是只记录一个顶层版本号：

| 简称 | 对应快照 | 作用 |
|---|---|---|
| B₀ | `chromium-baseline-prebulk-20260707-000000-generated-20261028-142311.xml` | 2026-07-07 大批漏洞回合之前的 Chromium 144 对比仓 |
| A₀ | `arkweb-144-prebulk-20260707-000000-generated-20261028-142311.xml` | 同一历史截点的 ArkWeb 144 |
| A₁ | `arkweb-144-master-20261027-192826.xml` | 2026-10-27 固定的 ArkWeb 144 最新状态，也是本仓 157 占位历史的来源 |
| B₁ | `chromium-baseline-144.0.7559.59-20261027-192943.xml` | 2026-10-27 固定的 Chromium 144 对比仓，用于最新差异和来源复核 |

`H` 指 B₀ 与 A₀ 的逐仓差异，覆盖漏洞批量回合之前的自研候选；其中少量差异其实是 Chromium 对比仓先合入上游代码形成的**反向差异**，已单列排除。`D` 指 A₀ 到 A₁ 的逐仓变化；这一时期混有大量上游漏洞回合，只挑出有 ArkWeb/OHOS 平台来源依据的修改及同一 PR 的功能配套。独立新增且整笔 PR 没有平台线索的修改暂留待审。

## 这里有什么

| 位置 | 来源 | 用途 |
|---|---|---|
| [`patches/`](patches/) | `ArkWeb_Helper/Knowledge/sujiahao/通用/需求/ArkWeb157尝鲜分支/01-补丁生成阶段/05-合成候选/patches/` | 30 份 H 加筛后 D 的逐仓候选补丁；每个文件对应一个多仓子仓，文件名不是本仓内源码路径 |
| [`patch_index.tsv`](patch_index.tsv) | 同上一级的 `patch_index.tsv` | 逐文件列出目标子仓 `repo_path`、历史基底 `B0_sha`、字节数、SHA-256 和历史基底上的验证结果；查补丁应先查此表 |
| [`statistics/`](statistics/) | 同一事务的固定快照、原始差异和筛选账本 | 解释哪些修改进入候选、哪些排除或待审，以及每项的原始 PR、代码位置和判断依据；逐文件来源见其中的 `source_index.tsv` |

候选补丁中 `a/`、`b/` 路径相对于 [`patch_index.tsv`](patch_index.tsv) 指定的**子仓根目录**。比如 `src/cef` 对应的补丁应在 CEF 子仓核对，不能在 `chromium_arkweb` 根目录直接执行 `git apply`。三个 ArkWeb 独有子仓以及共有仓内的整目录迁移项另记在 [`statistics/migration_items/`](statistics/migration_items/)；它们不在这 30 份文件级补丁中。

## 目前验证到哪一步

30 份补丁已在各自的历史 Chromium 基底 B₀ 的临时 index 中应用成功，结果 tree 与先应用 H、再应用筛后 D 一致。入库前逐份核对了文件字节数和 SHA-256；统计资料也通过 [`statistics/source_index.tsv`](statistics/source_index.tsv) 记录原文件位置和校验值。

`patches/src.patch` 约 22 MiB，超过 GitCode 单文件 10 MiB 限制，因此由本目录的 `.gitattributes` 使用 Git LFS 保存。检出本仓时需安装 Git LFS；若只看到 LFS 指针，执行 `git lfs pull` 后再按 `patch_index.tsv` 核对该文件的 SHA-256。

**尚未在 Chromium 157 上应用、解冲突或编译。**`statistics/` 中标为待审的项目仍可能改变候选范围；打 157 时应结合审核结论逐仓生成实际补丁，不能把这里的文件整体视为可直接执行的脚本。
