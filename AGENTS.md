# ArkWeb Chromium 适配层指引

## 适用范围

本文件适用于 `chromium_arkweb` 仓库根目录；在 ArkWeb 联合工作区中，本仓通常挂载为 `src/arkweb/`。下文相对路径均以本仓库根目录为基准。

任务跨越 `chromium_src`、`chromium_cef` 或 `web_webview` 时，还必须读取对应仓库根目录的 `AGENTS.md`，并优先遵循更接近改动文件的指引。

## 项目定位

本仓库是 ArkWeb 对 Chromium 原生代码的解耦和适配层，对应 OpenHarmony 中的 Chromium 引擎部分。核心职责是将 Chromium 的侵入式修改隔离到独立目录，保持上游代码可同步更新。

仓库内代码最终编译为 **ArkWebCore.hap**（包名 `com.ohos.arkwebcore`），由 `openharmony/web_webview` 的 `ohos_nweb/` 加载使用。

## ArkWeb 多仓协作体系

本仓是 ArkWeb/WebView 子系统的成员之一。需求拆解时需跨多个仓库协作，以下列出核心关联仓库：

| 仓库名 | 定位 | 地址 |
|---|---|---|
| **arkui_ace_engine** | ArkUI 前端框架，承载 `<Web>` 组件的声明式 UI 层（ETS → WebPattern → WebDelegate） | https://gitcode.com/openharmony/arkui_ace_engine |
| **web_webview** | ArkWeb 系统服务主仓，定义 NWeb 接口 + Web 控件逻辑 + ohos_nweb 桥接层 | https://gitcode.com/openharmony/web_webview |
| **chromium_arkweb**（本仓） | Chromium 适配层，将 Chromium 侵入式修改隔离到独立目录，编译产出 ArkWebCore.hap | https://gitcode.com/openharmony-tpc/chromium_arkweb |
| **chromium_cef** | CEF 接口层，基于 Chromium Content API 封装，提供浏览器核心能力的 C/C++ 接口 | https://gitcode.com/openharmony-tpc/chromium_cef |
| **chromium_src** | Chromium 原始源码主仓，作为上游同步基线 | https://gitcode.com/openharmony-tpc/chromium_src |

**典型调用链（自上而下）：**
`arkui_ace_engine`（UI 层）→ `web_webview`（系统服务/NWeb）→ `chromium_arkweb`（适配层/CEF delegate）→ `chromium_cef`（CEF 接口）→ `chromium_src`（Chromium 内核）

**协作要点：**
- `arkui_ace_engine` 到 `web_webview` 通过 NWeb NAPI 接口通信（跨进程）。
- `web_webview` 的 `ohos_nweb/` 定义接口契约，本仓（`chromium_arkweb`）的 `ohos_nweb/cef_delegate` 实现这些接口。
- `glue/` 目录中的桥接契约从 `web_webview` 侧同步，本仓不承载业务行为。
- 需求拆解时沿调用链定位到具体仓库，在接口处核对契约即可，无需通读所有源码。

## 快速路由

- `chromium_ext/`：Chromium 侵入式修改的解耦层，按 Chromium 模块组织；核心注册文件是 `chromium_ext.gni`。
- `ohos_nweb/`：ArkWeb 侧 NWeb 接口实现层，通过 CEF delegate 将 Chromium 能力暴露给系统侧。
- `ohos_adapter_ndk/`：OpenHarmony 系统服务适配层，包含 adapter、mock 和 stub。
- `glue/`：从 WebView 侧同步的桥接契约，不承载业务行为。
- `build/`：构建脚本、GN 配置、特性开关和签名流程。
- `patch/`：对 Chromium 原始源码或第三方代码的最小化补丁。

构建入口是 `build/build.sh`。提交使用 `git commit -s`（DCO 签名）。

## 知识索引

稳定背景知识放在 `docs/knowledge/`。改动前先按目录和任务术语定位，再读取对应文档：

| 场景 | 先读 |
|---|---|
| 需求分析、方案设计或开发涉及渲染、媒体、输入、控件、页面适配或进程等业务行为，即使需求未提及设备 | [`docs/knowledge/device-differences.md`](docs/knowledge/device-differences.md)：业务配置映射与差异识别入口 |
| 顶层目录职责、模块边界、术语约定 | [`docs/knowledge/module-map.md`](docs/knowledge/module-map.md) |
| 修改 `chromium_ext/`，或任务涉及源码注册、特性开关、`BUILDFLAG` | [`docs/knowledge/chromium-ext-architecture.md`](docs/knowledge/chromium-ext-architecture.md) |
| 任务涉及构建入口、GN args、`coreut`、`allut`、`smokeut` | [`docs/knowledge/build-system.md`](docs/knowledge/build-system.md) |
| 修改 WebView、glue、NWeb、CEF delegate，或任务出现这些术语 | [`docs/knowledge/interface-boundary.md`](docs/knowledge/interface-boundary.md) |
| 修改 `ohos_adapter_ndk/`，或任务涉及系统服务适配、mock、stub | [`docs/knowledge/ohos-adapter-ndk.md`](docs/knowledge/ohos-adapter-ndk.md) |
| 修改 `patch/`、bridge、cpptoc、ctocpp、CEF wrapper，或任务涉及 UT/fuzzer | [`docs/knowledge/patch-and-test-workflow.md`](docs/knowledge/patch-and-test-workflow.md) |
| 键盘事件 / 文本输入 / IME / `event.key` / `kArkWebImeProcessKeyDomKeyFix` | [`docs/knowledge/input-event-pipeline.md`](docs/knowledge/input-event-pipeline.md) |
| 跨异步任务的数据生产-消费时序（剪贴板图片粘贴、文件下载、媒体编码等） | [`docs/knowledge/async-data-timing.md`](docs/knowledge/async-data-timing.md) |

编辑前应明确本次任务所属模块、已读取的知识文档，以及发现的接口、生成物和构建约束。

## 核心规则

- chromium_ext 中的代码按 Chromium 模块组织，每个子目录对应 Chromium 源码中的一个模块；目录结构可通过 `find chromium_ext -maxdepth 1` 获取。
- 新增扩展文件必须在 `chromium_ext.gni` 中注册，否则不会参与编译。
- 补丁（`patch/`）应最小化，仅包含无法通过 chromium_ext 解耦的必要修改。
- `ohos_nweb/cef_delegate` 中的 delegate 类实现 `ohos_interface` 定义的 NWeb 接口；新增接口能力需先确认接口定义归属。
- OpenHarmony 系统服务调用优先放入 `ohos_adapter_ndk/`，不要直接散落到 NWeb 或 Chromium 扩展逻辑中。
- 特性开关在 `build/features/features.gni` 中声明，通过 `arkweb_<feature>` 命名，并经 `BUILD.gn` 导出到 `BUILDFLAG(...)`。
- 不要手改 bridge、cpptoc、ctocpp 或 CEF wrapper 的生成物，先找生成输入。
- 不要绕过 `build/build.sh` 所调用的 `prepare.sh` 和 CEF translator；否则 bridge/CEF 生成物可能不是最新。
- 修改 NWeb/glue/CEF 接口时必须核对接口两端；如果接口定义归属或兼容性影响不明确，先向 Code Owner 确认，不要自行假定。

### 高风险变更边界

以下变更在动手前必须明确影响范围、上下游契约和验证方案；信息不足时先向对应仓库 Code Owner 确认：

- 修改安全、权限、认证、信任边界、证书校验或访问控制行为。
- 修改 IPC/CDP 等协议语义、消息结构、错误码、序列化格式或持久化数据格式。
- 修改已有 Public API 的签名、语义、生命周期或废弃策略；破坏性变更必须评估版本兼容方案，不能只修改单侧实现。
- 新增、删除或升级第三方依赖，或修改 License、NOTICE 及许可证相关配置。
- 修改 `web_config.xml`、PARAM 等配置的默认值、访问权限或持久化行为。

不得通过新增直接依赖绕过 Mojo、NWeb、CEF delegate、glue 或 adapter 层。不得为了使功能或测试通过而移除、放宽或短路已有权限检查、调用方身份检查、证书校验或兼容性检查。

- Code Owner：@ringking0

## 验证闭环

### 最低检查

每次代码改动至少完成：

1. 在受影响的 Git 项目内检查改动范围，确认没有修改生成物或无关文件。
2. 对受影响目标执行一次编译验证。
3. 运行与改动直接相关的最小单测；没有对应测试时说明原因。
4. 对 C/C++、GN 等受支持文件执行 `git cl format --dry-run`；若当前项目或环境不支持该命令，报告未执行原因，不得声称格式检查通过。
5. 根据下表执行任务特定验证。

### 按变更类型验证

| 变更类型 | 最低验证 |
|---|---|
| `chromium_ext/` 内部实现 | 受影响目标编译 + 最近的相关 UT |
| 新增 Chromium 扩展源文件 | 编译 + 确认已在 `chromium_ext.gni` 注册 |
| NWeb/glue/CEF 接口 | 接口定义端和实现端编译 + 重新生成 bridge/glue + 相关 UT |
| WebView Public API | WebView 全量单测 + 受影响的 NAPI/ANI/CJ/Native 绑定编译 + 兼容性评估 |
| adapter 或 OH 系统服务调用 | 受影响目标编译 + adapter mock/stub 单测 |
| patch 或 Chromium 原文件 | 受影响目标编译 + 对应功能测试；确认补丁最小且无法通过 `chromium_ext/` 解耦 |
| PARAM/XML/持久化配置 | 配置加载测试 + 默认值、权限和旧版本兼容检查 |
| 指针所有权或生命周期 | 常规构建之外，使用构建脚本的 `-use-ptr-check` |
| Blink GC 对象或成员 | 常规构建之外，使用构建脚本的 `-use-blink-gc` |
| 安全边界或不可信输入 | 相关功能测试；存在可用目标时增加 `-asan` 或对应 fuzzer 验证 |

### 构建和测试入口

- 从本仓库根目录使用 `./build/build.sh -t <target> <product>`；target 和 product 必须从脚本 `usage()` 或最近的 `BUILD.gn` 获取。
- 单测根据影响范围选择脚本已有的 `coreut`、`allut`、`smokeut` 或具体 UT target。
- fuzzer 使用构建脚本的 `-fuzzer` 路径生成配置，再按脚本输出的 GN 查询结果选择具体 target。
- WebView 侧构建和测试遵循 `web_webview` 仓库根目录 `AGENTS.md` 中的 OpenHarmony 构建命令，不得把本仓库构建命令套用到 WebView 仓库。
- Chromium 和 CEF 修改还必须遵循对应仓库根目录 `AGENTS.md` 中的构建要求。

### 完成标准

只有满足以下条件才能声明完成：

- 受影响目标编译成功，且没有由本次变更新增的警告。
- 相关测试通过；新增测试已被对应聚合 UT target 覆盖。
- 格式检查和适用的静态检查通过。
- 接口定义、实现、生成物和多语言绑定保持一致。
- Public API、安全、权限、协议或持久化变更已有兼容性结论和确认记录。
- 最终报告列出改动范围、实际运行的命令及结果、未执行检查、受阻原因和剩余风险。

若受环境、依赖、设备或耗时限制无法运行验证，不得声称验证通过；应提供建议的复现命令，并把该项标记为未验证。
