# 输入事件管线

本文记录键盘事件、文本输入、IME 合成在 ArkWeb 各层之间的传递规则与陷阱。具体接口签名以 `ohos_nweb/include/` 与 `ohos_interface/` 为准。

## 事件链路

```text
OHOS MMI 键值（如 KEYCODE_SPACE = 2050）
        ↓
arkweb/ohos_nweb/src/cef_delegate/nweb_input_delegate.cc
  keycodeConverter / keyScanCodeConverter 映射表
        ↓
ui::KeyboardCode（VKEY_SPACE = 0x20）
        ↓
arkweb/ohos_nweb/src/cef_delegate/nweb_event_handler.cc
  CreateCefKeyEvent / SendCefKeyEvent
        ↓
CefKeyEvent
        ↓
cef/libcef/browser/native/browser_platform_delegate_native_linux.cc
  TranslateUiKeyEvent
        ↓
ui::KeyEvent（含 DomCode、DomKey）
        ↓
ui/events/blink/web_input_event.cc
  MakeWebKeyboardEventFromUiEvent（lazy 调用 GetDomKey → ApplyLayout）
        ↓
blink::WebKeyboardEvent
        ↓
blink::KeyboardEvent（key、code、keyCode、charCode）
```

`event.key` 最终由 `ui::KeycodeConverter::DomKeyToKeyString(dom_key)` 决定（`third_party/blink/renderer/core/events/keyboard_event.cc:118-119`）。

## DomKey 的两个"空"枚举

| DomKey | 值 | 含义 | `DomKeyToKeyString` 返回 |
|---|---|---|---|
| `DomKey::NONE` | 0 | "未计算/错误"哨兵 | `""` |
| `DomKey::UNIDENTIFIED` | 0x0001 | W3C 标准"无法识别" | `"Unidentified"` |

两者**值不同、语义不同**，禁止混用。前者在 `ui/events/keycodes/dom/keycode_converter.cc:442-447` 既不是死键、不在命名表、也不是字符，因此返回空字符串。

## 已知缺陷位置

`cef/libcef/browser/native/browser_platform_delegate_native_linux.cc:289-296` 的 `TranslateUiKeyEvent`：

```cpp
#if BUILDFLAG(IS_OZONE_X11) || (BUILDFLAG(ARKWEB_INPUT_EVENTS) && !defined(COMPONENT_BUILD))
  ui::DomKey dom_key = ui::XKeySymToDomKey(keysym, character);
#else
  ui::DomKey dom_key = ui::DomKey::NONE;
#endif
```

对 `ARKWEB_INPUT_EVENTS && !COMPONENT_BUILD` 之外路径（COMPONENT_BUILD / OSR / 测试桩）显式设 `dom_key = DomKey::NONE`。配合 `arkweb/ohos_nweb/src/cef_delegate/nweb_event_handler.cc:356-361` 在 COMPONENT_BUILD 下未赋值的 `keyEvent.character`，整条链路可能让 `event.key === ""`。

`ui::KeyEvent::ApplyLayout`（`ui/events/event.cc:922-959`）在 `GetDomKey()` 调用时 lazy 兜底：`DomCodeToUsLayoutDomKey` 成功 → 正确字符；失败 → `DomKey::UNIDENTIFIED` → `"Unidentified"`。

## feature `kArkWebImeProcessKeyDomKeyFix` 门控

涉及文本输入或键盘事件处理（keydown / keyup / keypress、IME 合成、IPC 序列化 KeyboardEvent、表单 input 派发、Web ↔ Native 回写）的代码，必须根据 `base::Feature kArkWebImeProcessKeyDomKeyFix` 判断 DomKey / `event.key`：

- feature **开启**：用 `ui::XKeySymToDomKey(keysym, character)` 计算的 DomKey（空格 → `" "`、可打印字符 → 字面值、非可打印键 → W3C 命名）
- feature **关闭**：保留旧行为，消费方必须容错 `event.key === ""`（`DomKey::NONE`）与 `event.key === "Unidentified"`（`DomKey::UNIDENTIFIED`）两种异常值

修复落地时消除以下 `// FIXME`：
- `arkweb/ohos_nweb/src/cef_delegate/nweb_event_handler.cc:356, 382`
- `cef/libcef/browser/native/browser_platform_delegate_native_linux.cc:289` 的 `#else dom_key = DomKey::NONE` 短路

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

## 历史教训

COMPONENT_BUILD 模式下 `dom_key = NONE` 未兜底，曾导致 Web 端空格键 `event.key` 在测试 hap 中为空字符串，前端依赖 `key === " "` 的判断全部失效；同时部分 IME 路径下 fallback 到 `"Unidentified"`，让前端误判为"用户按了无法识别的键"。

## 相关文档

- [async-data-timing.md](async-data-timing.md)
- [chromium-ext-architecture.md](chromium-ext-architecture.md)
- [interface-boundary.md](interface-boundary.md)
