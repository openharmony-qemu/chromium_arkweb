# OpenCode Agent Skills

本目录承载 ArkWeb 仓库的 OpenCode 代理技能（Agent Skills），以及管理脚本
`opencode_setup.py`。每个技能是一个独立子目录，根目录含 `SKILL.md`（带 YAML
frontmatter 的 `name` 与 `description`）。

`opencode_setup.py` 用于把技能安装到 OpenCode 的工作区，并管理其启用/禁用/询问
状态。它不依赖任何外部 CLI——OpenCode 通过文件发现技能、通过 `opencode.json` 的
`permission` 块控制状态，因此本脚本直接操作文件与配置。

参考文档：<https://opencode.ai/docs/zh-cn/skills/>

## 命令

```bash
python3 arkweb/agents/skills/opencode_setup.py <command> [names...] [--global]
```

| 命令 | 说明 |
|------|------|
| `list` | 列出所有可用与已安装的技能及其状态 |
| `install <names>` | 把技能以符号链接安装到目标作用域（默认项目级） |
| `uninstall <names>` | 移除已安装的符号链接（不会删除真实目录） |
| `enable <names>` | 启用：在配置中写入 `permission: allow` |
| `disable <names>` | 禁用：在配置中写入 `permission: deny` |
| `ask <names>` | 加载前需用户确认：写入 `permission: ask` |

`--global` 作用于全局作用域（`~/.config/opencode`）而非项目作用域。

## 作用域与位置

| 作用域 | 安装目录 | 配置文件 |
|--------|----------|----------|
| 项目级（默认） | `<src>/.opencode/skills/<name>/` | `<src>/.opencode/opencode.json` |
| 全局（`--global`） | `~/.config/opencode/skills/<name>/` | `~/.config/opencode/opencode.json` |

- 项目根取外层 Chromium `src` 根（当 arkweb 以 `src/arkweb` 形式挂载于其中时，
  通过父目录的 `DEPS`/`.gn` 标记识别）；arkweb 独立检出时回退到 arkweb 根。
- 项目级配置写在 `.opencode/opencode.json`（而非 `src` 根的 `opencode.json`），
  OpenCode 会读取该位置，且其优先级高于根 `opencode.json`，从而与
  `.opencode/skills/` 同目录集中管理。
- `install` 创建符号链接而非拷贝，技能随本地 sync 自动保持最新。
- `enable`/`disable`/`ask` 按名称匹配权限（OpenCode 支持通配符），因此不强制要求
  先安装；对未知名称会告警但仍写入，便于预配置。

## 可用技能来源

`list`/`install` 扫描两个仓库的技能源目录：

1. 本仓（ArkWeb）：`arkweb/agents/skills/`
2. 外层 Chromium src：`<src>/agents/skills/`（含 `histograms`、`fuzzing` 等社区技能）

同名技能以 ArkWeb 版本优先。arkweb 独立检出（父目录无 Chromium 标记）时只扫描
ArkWeb 自身。

## `list` 输出列

| 列 | 含义 |
|----|------|
| SKILL | 技能名 |
| AVAILABLE | 是否在源目录中可安装（yes/no） |
| PROJECT | 项目作用域状态：`on`=启用 `off`=禁用 `ask`=确认 `no`=未安装 |
| GLOBAL | 全局作用域状态（同上） |
| SOURCE | 源技能路径（`//` 表示项目根下，`~` 表示家目录下） |
| DESTINATION | 安装目录（已安装则显示实际位置，未安装则显示默认项目安装目标） |

## 示例

```bash
# 查看全部技能
python3 arkweb/agents/skills/opencode_setup.py list

# 安装到项目（src/.opencode/skills）
python3 arkweb/agents/skills/opencode_setup.py install histograms

# 安装到全局
python3 arkweb/agents/skills/opencode_setup.py install histograms --global

# 项目级禁用 / 全局级改为询问
python3 arkweb/agents/skills/opencode_setup.py disable histograms
python3 arkweb/agents/skills/opencode_setup.py ask histograms --global

# 卸载
python3 arkweb/agents/skills/opencode_setup.py uninstall histograms
```

## 测试

```bash
python3 -m unittest opencode_setup_unittest
```

测试位于 `opencode_setup_unittest.py`（使用 `pyfakefs`），覆盖 JSONC 解析、配置
读写、双源发现、安装/卸载、权限写入与表格输出。
