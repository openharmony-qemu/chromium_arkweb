# 输入事件管线

本文记录键盘事件、文本输入、IME 合成在 ArkWeb 各层之间的传递规则与约束。具体接口签名以 `ohos_nweb/include/` 与 `ohos_interface/` 为准。

## 事件链路

```text
OHOS MMI 键值（如 KEYCODE_SPACE = 2050）
        ↓
NWebInputDelegate
  keycodeConverter / keyScanCodeConverter 映射表
        ↓
ui::KeyboardCode（VKEY_SPACE = 0x20）
        ↓
NWebEventHandler
  CreateCefKeyEvent / SendCefKeyEvent
        ↓
CefKeyEvent
        ↓
BrowserPlatformDelegateNativeLinux::TranslateUiKeyEvent
        ↓
ui::KeyEvent（含 DomCode、DomKey）
        ↓
MakeWebKeyboardEventFromUiEvent（lazy 调用 GetDomKey → ApplyLayout）
        ↓
blink::WebKeyboardEvent
        ↓
blink::KeyboardEvent（key、code、keyCode、charCode）
```

## DomKey 必须正确计算

`event.key` 最终由 `ui::KeycodeConverter::DomKeyToKeyString(dom_key)` 决定。DomKey 有两个容易混淆的"空"枚举：

| DomKey | 值 | 含义 | `DomKeyToKeyString` 返回 |
|---|---|---|---|
| `DomKey::NONE` | 0 | "未计算/错误"哨兵 | `""` |
| `DomKey::UNIDENTIFIED` | 0x0001 | W3C 标准"无法识别" | `"Unidentified"` |

**规则：**

1. **禁止把 `DomKey::NONE` 当作合法值传递给下游。** 它是哨兵，不是"空格"也不是"无法识别"。任何代码路径如果可能产生 `DomKey::NONE`，必须在传递到 `blink::KeyboardEvent` 之前替换为正确计算值或 `DomKey::UNIDENTIFIED`。
2. **`DomKey::NONE` 与 `DomKey::UNIDENTIFIED` 语义不同，禁止混用。** 前者表示"还没算过"，后者表示"算过了但无法识别"。`ui::KeycodeConverter` 对前者返回空字符串，对后者返回 `"Unidentified"`。
3. **`ui::KeyEvent::ApplyLayout` 是 lazy 兜底。** 只有在 `GetDomKey()` 被调用时才计算：`DomCodeToUsLayoutDomKey` 成功 → 正确字符；失败 → `DomKey::UNIDENTIFIED`。如果 DomKey 在此之前被显式设为 `DomKey::NONE`，lazy 兜底不会触发。

## 条件编译分支必须全覆盖

`BrowserPlatformDelegateNativeLinux::TranslateUiKeyEvent` 使用条件编译选择 DomKey 计算方式：

```cpp
#if BUILDFLAG(IS_OZONE_X11) || (BUILDFLAG(ARKWEB_INPUT_EVENTS) && !defined(COMPONENT_BUILD))
  ui::DomKey dom_key = ui::XKeySymToDomKey(keysym, character);
#else
  ui::DomKey dom_key = ui::DomKey::NONE;  // ← 留意：这里产生了哨兵值
#endif
```

**规则：**

1. **新增 `BUILDFLAG` 分支时，必须确保每个分支都能产生合法 DomKey。** 如果某个分支无法计算 DomKey，应走 `ApplyLayout` 兜底，而不是直接赋 `DomKey::NONE`。
2. **修改 `NWebEventHandler` 中 `CefKeyEvent` 构造逻辑时，必须确保 `character` 字段在所有构建模式下都被赋值。** 未赋值的 `character` 配合 `DomKey::NONE` 会让整条链路产出 `event.key === ""`。
3. **COMPONENT_BUILD / OSR / 测试桩路径不是"可以不管"的路径。** 这些路径下产生的异常 `event.key` 值会被测试 hap 和前端代码消费，导致功能失效。

## feature `kArkWebImeProcessKeyDomKeyFix` 门控

涉及文本输入或键盘事件处理（keydown / keyup / keypress、IME 合成、IPC 序列化 KeyboardEvent、表单 input 派发、Web ↔ Native 回写）的代码，必须根据 `base::Feature kArkWebImeProcessKeyDomKeyFix` 判断 DomKey / `event.key`：

- feature **开启**：用 `ui::XKeySymToDomKey(keysym, character)` 计算的 DomKey（空格 → `" "`、可打印字符 → 字面值、非可打印键 → W3C 命名）
- feature **关闭**：保留旧行为，消费方必须容错 `event.key === ""`（`DomKey::NONE`）与 `event.key === "Unidentified"`（`DomKey::UNIDENTIFIED`）两种异常值

修复落地时消除以下 `// FIXME`：
- `NWebEventHandler` 中 `keyEvent.character` 未赋值处
- `BrowserPlatformDelegateNativeLinux::TranslateUiKeyEvent` 的 `#else dom_key = DomKey::NONE` 短路

## 前端代码建议

由于三种值（`" "`、`""`、`"Unidentified"`）在历史/兼容路径下都可能出现，对空格等可打印键的判断不能依赖单一 `event.key`：

```js
// 最稳：用 code（物理位置，与 dom_key 无关）
if (event.code === 'Space') { ... }

// 次稳：用 keyCode（直接来自 VKEY_SPACE = 0x20）
if (event.keyCode === 32) { ... }

// 用 key 但兼容异常值
if (event.key === ' ' || event.key === '' || event.key === 'Unidentified') {
  if (event.code === 'Space') { ... }
}
```

## 相关文档

- [async-data-timing.md](async-data-timing.md)
- [chromium-ext-architecture.md](chromium-ext-architecture.md)
- [interface-boundary.md](interface-boundary.md)
