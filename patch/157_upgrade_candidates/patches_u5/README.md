# 逐仓候选的 5 行上下文版本

本目录保存本轮保留的 28 份逐仓候选的 5 行上下文版本。先生成尚未开始应用的 `src`、`src/cef`、`src/v8`、`src/third_party/devtools-frontend/src` 四份，再生成已处理的 24 份。它们与上一级 `patches/` 中的同名文件表达**相同的历史 B₀ → 合成候选目标 tree 差异**，只把每个 hunk 的上下文从 Git 默认的 3 行增加到 5 行。原始候选不覆盖、不删除。`libaom` 和 `libvpx` 的原始候选经单仓来源审核判定为上游改动，因此没有生成 5 行版本；原文件若仍在 `patches/`，不表示它们重新进入迁移范围。

生成时从补丁生成阶段 `05-合成候选/patch_index.tsv` 读取各仓 `B0_sha` 与 `expected_tree`，把 Chromium baseline 的 Git 对象库作为只读 alternate，在 ArkWeb 144 的对应子仓执行等效于以下命令的对象差异导出（`<B0>`、`<目标 tree>` 分别替换为表中值）：

```bash
GIT_ALTERNATE_OBJECT_DIRECTORIES=<baseline 子仓 objects 路径> \
  git diff --no-ext-diff --no-textconv --binary --full-index -U5 \
  <B0> <目标 tree>
```

**校验**：用同一组对象和 `-U3` 重新导出的字节必须与原始 patch 完全一致；再将 `-U5` 补丁应用到以 B₀ 初始化的临时 Git index，并核对 `git write-tree` 等于原 `expected_tree`。28 仓均通过。逐仓原版与新版 SHA-256、大小和校验结果见 [`patch_index.tsv`](patch_index.tsv)。整个过程没有修改 157 目标仓的源码，也没有重新筛选 H、D。

增加上下文可以减少部分误匹配，但不能证明修改属于自研，也不能保证不会重复应用。已处理候选还需单独对照当时的 157 PR 与 reject；有 reject 的仓要据重放结果更新 reject 的上下文。本目录的历史 B₀ 校验只证明变更内容与原候选相同，**不代表已在 157 应用或编译通过**。
