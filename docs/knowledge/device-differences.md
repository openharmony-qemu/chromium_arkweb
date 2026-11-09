# 设备差异分支提示

本文用于需求分析、方案设计和开发时，从业务找到已有的差异化配置入口，再沿映射确认相关分支。同一设备形态内，也可能因产品配置、硬件能力或运行状态选择不同路径。

## 差异的识别线索

- 内核特性的差异可能来自 CCM 元数据和 `web_config.xml`，消费代码中未必出现设备名称。WebView 仓 `ohos_nweb/src/nweb_config_helper.cpp` 的 `configMap` 提供“XML 字段 → 命令行参数”映射，`ParseConfig()` 是配置加载入口。

## 按业务查找 configMap 映射

下表选取当前 `configMap` 中的业务检索入口，记录配置键与参数名；具体取值转换、参数增删和最终行为，可从对应 lambda、`ReadConfig()` / `ParseDeleteConfig()` 及参数引用处继续确认。登记了映射，不代表所有产品都会启用，也不能单凭声明确认最终消费路径。

| 业务检索词 | configMap 配置字段 | 对应命令行参数 |
|---|---|---|
| 绘制、截图、Canvas、图像共享的渲染后端 | `enableVulkanConfig/enableVulkan` | `--ohos-enable-vulkan` |
| DrDc、绘制调度 | `disableDrdcConfig/disableDrdc` | `--ohos-enable-drdc` |
| Renderer 数量、多 Web 实例 | `renderConfig/renderProcessCount` | `--renderer-process-limit=<值>` |
| Renderer 进程模式 | `settingConfig/enableSingleRenderProcess` | `--ohos-enable-single-render-process`（C++ 符号：`kIsMultiRenderProcess`） |
| GPU 进程模式 | `outOfProcessGPUConfig/enableOopGpu` | `--in-process-gpu` |
| 媒体后台播放、暂停 | `mediaConfig/backgroundMediaShouldSuspend` | `--disable-background-media-suspend` |
| 触摸事件 | `touchEventConfig/touchEventShouldRegister` | `--disable-touch-event-register` |
| 双击交互 | `settingConfig/enableDoubleTapForPlatform` | `--ohos-enable-double-tap-for-platform` |
| UA、页面兼容 | `userAgentConfig/userAgentValue` | `--ohos-user-agent-value=<值>` |
| 页面控件样式 | `settingConfig/disableMobileStyleSheet` | `--ohos-disable-mobile-style-sheet` |
| 渲染资源回收 | `settingConfig/enableDeleteUnusedResourcesDelay` | `--ohos-enable-delete-unused-resources-delay` |
| HTTP 缓存 | `settingConfig/enableHttpCacheSimple` | `--ohos-enable-http-cache-simple` |
| 打印 | `settingConfig/enablePrinting` | `--ohos-enable-printing` |

用映射出的参数字符串在相关内核仓中定位开关符号和引用。ArkWeb 扩展开关通常声明在 [content_switches_ext.h](../../chromium_ext/content/public/common/content_switches_ext.h)，定义在 [content_switches_ext.cc](../../chromium_ext/content/public/common/content_switches_ext.cc)。表中的 `--renderer-process-limit`、`--in-process-gpu` 复用 Chromium 仓 `content/public/common/content_switches.cc` 中的原生开关；`--disable-background-media-suspend` 复用 `media/base/media_switches.cc` 中的原生开关。Renderer 消费的命令行配置，可沿 Chromium 仓 `content/browser/renderer_host/render_process_host_impl.cc` 的 `PropagateBrowserCommandLineToRenderer()` 继续确认传递关系。

## configMap 之外的识别线索

- 配置也可能由 `nweb_config_helper.cpp` 的其他解析入口处理，或通过系统参数、硬件能力探测参与分支选择；未在 `configMap` 命中不等于没有差异。
- 对外接口可沿 `@syscap`、组件及产品的 SystemCapability 元数据定位适用范围。
- 历史设备类型、PC 模式、兼容类型和资源档位判断，可从 [sys_info_utils_ext.cc](../../chromium_ext/base/ohos/sys_info_utils_ext.cc) 的相关入口继续查找。
