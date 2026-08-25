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

#include "nweb_ai_inputmethod_controller.h"

#include "base/json/json_writer.h"
#include "nweb_errors.h"
#include "nweb_hilog.h"
#include "nweb_input_handler.h"
#include "nweb_inputmethod_handler.h"

namespace OHOS::NWeb {

namespace {
std::string BuildResultJson(int32_t code, const std::string& message) {
    base::Value::Dict result;
    result.Set("code", code);
    if (code == static_cast<int32_t>(WebCommandErrorCode::SUCCESS)) {
        result.Set("message", message);
    } else {
        result.Set("error", message);
    }
    std::string json;
    base::JSONWriter::Write(result, &json);
    return json;
}

constexpr int VK_BACK = 0x08;
constexpr int VK_TAB = 0x09;
constexpr int VK_RETURN = 0x0D;
constexpr int VK_SHIFT = 0x10;
constexpr int VK_CONTROL = 0x11;
constexpr int VK_MENU = 0x12;
constexpr int VK_CAPITAL = 0x14;
constexpr int VK_ESCAPE = 0x1B;
constexpr int VK_SPACE = 0x20;
constexpr int VK_PRIOR = 0x21;
constexpr int VK_NEXT = 0x22;
constexpr int VK_END = 0x23;
constexpr int VK_HOME = 0x24;
constexpr int VK_LEFT = 0x25;
constexpr int VK_UP = 0x26;
constexpr int VK_RIGHT = 0x27;
constexpr int VK_DOWN = 0x28;
constexpr int VK_INSERT = 0x2D;
constexpr int VK_DELETE = 0x2E;

constexpr int VK_F1 = 0x70;
constexpr int VK_F2 = 0x71;
constexpr int VK_F3 = 0x72;
constexpr int VK_F4 = 0x73;
constexpr int VK_F5 = 0x74;
constexpr int VK_F6 = 0x75;
constexpr int VK_F7 = 0x76;
constexpr int VK_F8 = 0x77;
constexpr int VK_F9 = 0x78;
constexpr int VK_F10 = 0x79;
constexpr int VK_F11 = 0x7A;
constexpr int VK_F12 = 0x7B;

constexpr int VK_LSHIFT= 0xA0;
constexpr int VK_RSHIFT = 0xA1;
constexpr int VK_LCONTROL = 0xA2;
constexpr int VK_RCONTROL = 0xA3;
constexpr int VK_LMENU = 0xA4;
constexpr int VK_RMENU = 0xA5;

constexpr int VK_OEM_1 = 0xBA;                            // ';:' for US
constexpr int VK_OEM_PLUS = 0xBB;                         // '+' any country
constexpr int VK_OEM_COMMA = 0xBC;                        // ',' any country
constexpr int VK_OEM_MINUS = 0xBD;                        // '-' any country
constexpr int VK_OEM_PERIOD = 0xBE;                       // '.' any country
constexpr int VK_OEM_2 = 0xBF;                            // '/?' for US
constexpr int VK_OEM_3 = 0xC0;                            // '`~' for US
constexpr int VK_OEM_4 = 0xDB;                            //  '[{' for US
constexpr int VK_OEM_5 = 0xDC;                            //  '\|' for US
constexpr int VK_OEM_6 = 0xDD;                            //  ']}' for US
constexpr int VK_OEM_7 = 0xDE;                            //  ''"' for US
}

NWebAIInputMethodController::NWebAIInputMethodController(std::shared_ptr<NWebInputHandler> inputHandler,
                                                        std::shared_ptr<NWebDelegateInterface> nwebDelegate,
                                                        CefRefPtr<NWebInputMethodHandler> inputMethodHandler)
    : inputHandler_(inputHandler), nwebDelegate_(nwebDelegate), inputMethodHandler_(inputMethodHandler) {}

std::string NWebAIInputMethodController::ExecuteJsonAction(const base::Value::Dict& jsonDict)
{
    std::string method;
    auto methodValue = jsonDict.Find("method");
    if (!methodValue || !methodValue->is_string()) {
        WVLOG_E("NWebAIInputMethodController: method not found");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_METHOD_NOT_FOUND), "method not found");
    }
    method = methodValue->GetString();
    WVLOG_I("NWebAIInputMethodController: method: %{public}s", method.c_str());

    if (method == "click") {
        return HandleClickOrFocus(jsonDict, "click");
    } else if (method == "focus") {
        return HandleClickOrFocus(jsonDict, "focus");
    } else if (method == "cursor_position") {
        return HandleCursorPosition(jsonDict);
    } else if (method == "type") {
        return HandleType(jsonDict);
    } else if (method == "send_keys") {
        return HandleSendKeys(jsonDict);
    }

    WVLOG_E("NWebAIInputMethodController: unknown method: %{public}s", method.c_str());
    return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_METHOD_UNKNOWN), "unknown method");
}

std::string NWebAIInputMethodController::HandleClickOrFocus(const base::Value::Dict& jsonDict, const std::string& method)
{
    auto* paramsValue = jsonDict.FindDict("params");
    if (!paramsValue) {
        WVLOG_E("NWebAIInputMethodController: params not found");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_PARAMS_MISSING), "click/focus params not found");
    }

    std::string xpath;
    auto xpathValue = paramsValue->FindString("XPath/nodeid");
    if (!xpathValue) {
        WVLOG_E("NWebAIInputMethodController: nodeid not found");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_NODEID_MISSING), "click/focus nodeid not found");
    }
    xpath = *xpathValue;
    
    if (xpath.empty()) {
        WVLOG_E("NWebAIInputMethodController: nodeid is empty");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_NODEID_EMPTY), "click/focus nodeid is empty");
    }
    WVLOG_I("NWebAIInputMethodController: %{public}s at %{private}s", method.c_str(), xpath.c_str());

    std::string jsStr;
    jsStr = CombineClickFocusJsString(method, xpath);
    if (!nwebDelegate_) {
        WVLOG_E("NWebAIInputMethodController: nwebDelegate_ is null");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_DELEGATE_NULL), "click/focus delegate not initialized");
    }
    bool isElement = nwebDelegate_->IsElementExist(xpath);
    if (!isElement) {
        WVLOG_E("NWebAIInputMethodController: element not exist, xpath: %{private}s", xpath.c_str());
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_CLICK_ELEMENT_NOT_FOUND), "click/focus element not exist");
    }
    nwebDelegate_->ExecuteJavaScript(jsStr);
    return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::SUCCESS), "success");
}

std::string NWebAIInputMethodController::CombineClickFocusJsString(const std::string& eventType, const std::string& xPath)
{
    // JSON-serialize xpath to safely embed in JS (handles all special chars including `, ${}, \, quotes)
    std::string jsonXPath;
    base::JSONWriter::Write(xPath, &jsonXPath);

    // Validate eventType against whitelist
    std::string safeEventType = (eventType == "click" || eventType == "focus") ? eventType : "click";

    std::ostringstream oss;
    oss << "(function() {"
        << "  try {"
        << "    const xpath = " << jsonXPath << ";"
        << "    const result = document.evaluate(xpath, document, null, "
        << "        XPathResult.FIRST_ORDERED_NODE_TYPE, null);"
        << "    const element = result.singleNodeValue;"
        << "    if (!element) {"
        << "      return JSON.stringify({code: -1, message: 'Element not found by XPath: ' + xpath});"
        << "    }"
        << "    if ('" << safeEventType << "' === 'click') {"
        << "      element.click();"
        << "      return JSON.stringify({code: 0, message: 'Click success'});"
        << "    } else if ('" << safeEventType << "' === 'focus') {"
        << "      element.focus();"
        << "      return JSON.stringify({code: 0, message: 'Focus success'});"
        << "    }"
        << "  } catch (e) {"
        << "    return JSON.stringify({code: -1, message: 'JavaScript error: ' + e.toString()});"
        << "  }"
        << "})();";

    return oss.str();
}

std::string NWebAIInputMethodController::HandleCursorPosition(const base::Value::Dict& jsonDict)
{
    int x = 0, y = 0, width = 0, height = 0;
    if (inputMethodHandler_ && !inputMethodHandler_->IsAttached()) {
        WVLOG_E("NWebAIInputMethodController: input method not bound");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_METHOD_NOT_BOUND), "input method not bound");
    }
    if (inputMethodHandler_ && inputMethodHandler_->GetFocusRect(x, y, width, height)) {
      WVLOG_I("NWebAIInputMethodController:HandleCursorPosition, x=%{private}d, y=%{private}d", x, y);
    }
    base::Value::Dict resultDict;
    resultDict.Set("x", x);
    resultDict.Set("y", y);
    
    base::Value::Dict responseDict;
    responseDict.Set("result", std::move(resultDict));
    
    std::string jsonResult;
    base::JSONWriter::Write(responseDict, &jsonResult);
    return jsonResult;
}

std::string NWebAIInputMethodController::HandleType(const base::Value::Dict& jsonDict)
{
    auto* paramsValue = jsonDict.FindDict("params");
    if (!paramsValue) {
        WVLOG_E("NWebAIInputMethodController: params not found");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_PARAMS_MISSING), "type params not found");
    }
    
    std::string xpath;
    auto xpathValue = paramsValue->FindString("XPath/nodeid");
    if (!xpathValue) {
        WVLOG_E("NWebAIInputMethodController: nodeid not found");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_NODEID_MISSING), "type nodeid not found");
    }
    xpath = *xpathValue;
    
    auto textValue = paramsValue->FindString("text");
    if (!textValue) {
        WVLOG_E("NWebAIInputMethodController: type text not found");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_TEXT_MISSING), "type text not found");
    }
    std::string text = *textValue;
    
    int index = -1;
    auto indexValue = paramsValue->Find("index");
    if (indexValue && indexValue->is_int()) {
        index = indexValue->GetInt();
        if (index < -1) {
            index = -1;
        }
    }
    
    bool clear = false;
    auto clearValue = paramsValue->Find("clear");
    if (clearValue && clearValue->is_bool()) {
        clear = clearValue->GetBool();
    }
    
    WVLOG_I("NWebAIInputMethodController: type - xpath: %{private}s, text: %{private}s, index: %{public}d, clear: %{public}d",
            xpath.c_str(), text.c_str(), index, clear);
    
    if (xpath.empty()) {
        WVLOG_E("NWebAIInputMethodController: xpath is empty");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_XPATH_EMPTY), "type xpath is empty");
    }
    
    if (!nwebDelegate_) {
        WVLOG_E("NWebAIInputMethodController: nwebDelegate_ is null");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_DELEGATE_NULL), "type delegate not initialized");
    }
    
    bool isElement = nwebDelegate_->IsElementExist(xpath);
    if (!isElement) {
        WVLOG_E("NWebAIInputMethodController: element not exist, xpath: %{private}s", xpath.c_str());
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_ELEMENT_NOT_FOUND), "type element not exist");
    }
    
    std::string jsStr;
    if (clear) {
        jsStr = CombineInputModifyCommandJsString(xpath, text);
    } else {
        jsStr = CombineInputInsertCommandJsString(xpath, text, index);
    }
    
    nwebDelegate_->ExecuteJavaScript(jsStr);
    return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::SUCCESS), "success");
}

std::string NWebAIInputMethodController::CombineInputInsertCommandJsString(const std::string& xPath,
                                                                  const std::string& content,
                                                                  int32_t index)
{
    // JSON-serialize to safely embed in JS (handles all special chars including quotes, backticks, ${}, \)
    std::string jsonXPath;
    base::JSONWriter::Write(xPath, &jsonXPath);
    std::string jsonContent;
    base::JSONWriter::Write(content, &jsonContent);

    std::ostringstream oss;
    oss << "(function() {"
        << "  try {"
        << "    const xpath = " << jsonXPath << ";"
        << "    const result = document.evaluate(xpath, document, null, "
        << "        XPathResult.FIRST_ORDERED_NODE_TYPE, null);"
        << "    const el = result.singleNodeValue;"
        << "    if (!el) {"
        << "      return JSON.stringify({status: 'error', reason: 'XPath not found: ' + xpath});"
        << "    }"
        << "    if (!el || (!['INPUT', 'TEXTAREA'].includes(el.tagName) && !el.isContentEditable)) {"
        << "      return JSON.stringify({status: 'error', reason: 'Element is not editable'});"
        << "    }"
        << "    const isInput = el.tagName === 'INPUT' || el.tagName === 'TEXTAREA';"
        << "    const dispatchInput = () => {"
        << "      el.dispatchEvent(new InputEvent('input', { bubbles: true, composed: true }));"
        << "      el.dispatchEvent(new Event('change', { bubbles: true }));"
        << "    };"
        << "    const content = " << jsonContent << ";"
        << "    let insertIndex = " << index << ";"
        << "    if (isInput) {"
        << "      const val = el.value;"
        << "      if (insertIndex < 0) insertIndex = val.length;"
        << "      insertIndex = Math.min(insertIndex, val.length);"
        << "      el.value = val.slice(0, insertIndex) + content + val.slice(insertIndex);"
        << "      el.setSelectionRange(insertIndex + content.length, insertIndex + content.length);"
        << "    } else {"
        << "      const text = el.innerText;"
        << "      if (insertIndex < 0) insertIndex = text.length;"
        << "      insertIndex = Math.min(insertIndex, text.length);"
        << "      el.innerText = text.slice(0, insertIndex) + content + text.slice(insertIndex);"
        << "      const textNode = el.firstChild;"
        << "      if (textNode && textNode.nodeType === 3) {"
        << "        const range = document.createRange();"
        << "        const selection = window.getSelection();"
        << "        range.setStart(textNode, Math.min(insertIndex + content.length, el.innerText.length));"
        << "        range.collapse(true);"
        << "        selection.removeAllRanges();"
        << "        selection.addRange(range);"
        << "      }"
        << "    }"
        << "    dispatchInput();"
        << "    return JSON.stringify({status: 'success', action: 'inputInsert'});"
        << "  } catch (e) {"
        << "    return JSON.stringify({status: 'fail', reason: e.toString()});"
        << "  }"
        << "})();";

    return oss.str();
}

std::string NWebAIInputMethodController::CombineInputModifyCommandJsString(const std::string& xPath,
                                                                  const std::string& content)
{
    // JSON-serialize to safely embed in JS (handles all special chars including quotes, backticks, ${}, \)
    std::string jsonXPath;
    base::JSONWriter::Write(xPath, &jsonXPath);
    std::string jsonContent;
    base::JSONWriter::Write(content, &jsonContent);

    std::ostringstream oss;
    oss << "(function() {"
        << "  try {"
        << "    const xpath = " << jsonXPath << ";"
        << "    const result = document.evaluate(xpath, document, null, "
        << "        XPathResult.FIRST_ORDERED_NODE_TYPE, null);"
        << "    const el = result.singleNodeValue;"
        << "    if (!el) {"
        << "      return JSON.stringify({status: 'error', reason: 'XPath not found: ' + xpath});"
        << "    }"
        << "    if (!el || (!['INPUT', 'TEXTAREA'].includes(el.tagName) && !el.isContentEditable)) {"
        << "      return JSON.stringify({status: 'error', reason: 'Element is not editable'});"
        << "    }"
        << "    const isInput = el.tagName === 'INPUT' || el.tagName === 'TEXTAREA';"
        << "    const dispatchInput = () => {"
        << "      el.dispatchEvent(new InputEvent('input', { bubbles: true, composed: true }));"
        << "      el.dispatchEvent(new Event('change', { bubbles: true }));"
        << "    };"
        << "    const content = " << jsonContent << ";"
        << "    if (isInput) {"
        << "      el.value = content;"
        << "    } else {"
        << "      el.innerText = content;"
        << "      const range = document.createRange();"
        << "      const sel = window.getSelection();"
        << "      range.selectNodeContents(el);"
        << "      range.collapse(false);"
        << "      sel.removeAllRanges();"
        << "      sel.addRange(range);"
        << "    }"
        << "    dispatchInput();"
        << "    return JSON.stringify({status: 'success', action: 'inputModify'});"
        << "  } catch (e) {"
        << "    return JSON.stringify({status: 'fail', reason: e.toString()});"
        << "  }"
        << "})();";

    return oss.str();
}

std::string NWebAIInputMethodController::HandleSendKeys(const base::Value::Dict& jsonDict)
{
    auto* paramsValue = jsonDict.FindDict("params");
    if (!paramsValue) {
        WVLOG_E("NWebAIInputMethodController: params not found");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_SEND_KEYS_PARAMS_MISSING), "send_keys params not found");
    }
    
    auto keysValue = paramsValue->FindString("key");
    if (!keysValue) {
        WVLOG_E("NWebAIInputMethodController: send_keys keys not found");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_SEND_KEYS_KEY_MISSING), "send_keys keys not found");
    }

    std::string keys = *keysValue;
    WVLOG_I("NWebAIInputMethodController: send_keys: %{private}s", keys.c_str());

    if (!inputHandler_) {
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::SUCCESS), "success");
    }

    // Split keys and separate main key from modifiers
    std::vector<std::string> keyParts = SplitKeys(keys);
    if (keyParts.empty()) {
        WVLOG_E("NWebAIInputMethodController: no valid keys found");
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_SEND_KEYS_NO_VALID_KEYS), "send_keys no valid keys");
    }

    std::string mainKey = keyParts.back();
    keyParts.pop_back();
    
    // Build modifier key codes
    std::vector<int32_t> modifierKeyCodes = BuildModifierKeyCodes(keyParts);
    
    // Handle Shift for uppercase main key
    if (NeedShiftForMainKey(mainKey)) {
        EnsureShiftInModifiers(modifierKeyCodes);
    }
    
    // Get main key code and send event
    int32_t mainKeycode = GetMainKeyCode(mainKey);
    if (mainKeycode == 0) {
        WVLOG_E("NWebAIInputMethodController: unknown main key: %{public}s", mainKey.c_str());
        return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::INPUT_SEND_KEYS_UNKNOWN_KEY), "send_keys unknown main key");
    }
    
    SendCombinedKeyEvent(mainKeycode, modifierKeyCodes);
    return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::SUCCESS), "success");
}

std::vector<std::string> NWebAIInputMethodController::SplitKeys(const std::string& keys)
{
    std::vector<std::string> keyParts;
    size_t pos = 0;
    
    while (pos < keys.length()) {
        size_t plusPos = keys.find('+', pos);
        std::string keyStr;
        
        if (plusPos == std::string::npos) {
            keyStr = keys.substr(pos);
            pos = keys.length();
        } else {
            keyStr = keys.substr(pos, plusPos - pos);
            pos = plusPos + 1;
        }
        keyParts.push_back(keyStr);
    }
    
    return keyParts;
}

std::vector<int32_t> NWebAIInputMethodController::BuildModifierKeyCodes(
    const std::vector<std::string>& modifierKeys)
{
    std::vector<int32_t> modifierKeyCodes;
    
    for (const auto& keyStr : modifierKeys) {
        if (keyStr == "Shift") {
            modifierKeyCodes.push_back(GetOhosKeyCodeFromVKey(VK_SHIFT));
            continue;
        }
        if (keyStr == "Ctrl" || keyStr == "Control") {
            modifierKeyCodes.push_back(GetOhosKeyCodeFromVKey(VK_CONTROL));
            continue;
        }
        if (keyStr == "Alt") {
            modifierKeyCodes.push_back(GetOhosKeyCodeFromVKey(VK_MENU));
            continue;
        }
        
        // Regular keys as modifiers (uncommon, but supported)
        int32_t vkey = GetVKeyFromWebKey(keyStr);
        int32_t keycode = GetOhosKeyCodeFromVKey(vkey);
        if (keycode != 0) {
            modifierKeyCodes.push_back(keycode);
        }
    }
    
    return modifierKeyCodes;
}

bool NWebAIInputMethodController::NeedShiftForMainKey(const std::string& mainKey)
{
    return (mainKey.length() == 1 && 
            mainKey[0] >= 'A' && 
            mainKey[0] <= 'Z');
}

void NWebAIInputMethodController::EnsureShiftInModifiers(std::vector<int32_t>& modifierKeyCodes)
{
    int32_t shiftKeyCode = GetOhosKeyCodeFromVKey(VK_SHIFT);
    
    // Check if Shift is already in modifiers
    for (int32_t modifier : modifierKeyCodes) {
        if (modifier == shiftKeyCode) {
            return;
        }
    }
    // Add Shift if not present
    modifierKeyCodes.push_back(shiftKeyCode);
}

int32_t NWebAIInputMethodController::GetMainKeyCode(const std::string& mainKey)
{
    int32_t vkey = GetVKeyFromWebKey(mainKey);
    int32_t keycode = GetOhosKeyCodeFromVKey(vkey);
    
    WVLOG_I("NWebAIInputMethodController: send_keys - main key: %{private}s, "
            "keycode: %{public}d", mainKey.c_str(), keycode);
    
    return keycode;
}

void NWebAIInputMethodController::SendCombinedKeyEvent(
    int32_t mainKeycode, 
    const std::vector<int32_t>& modifierKeyCodes)
{
    WVLOG_I("NWebAIInputMethodController: send_keys - modifiers count: %{public}zu", 
            modifierKeyCodes.size());
    
    // Send key press with modifiers
    inputHandler_->WebSendKeyEvent(mainKeycode, 0, modifierKeyCodes);
    
    // Send key release with modifiers
    inputHandler_->WebSendKeyEvent(mainKeycode, 1, modifierKeyCodes);
}

int32_t NWebAIInputMethodController::GetVKeyFromWebKey(const std::string& key)
{
    if (key == "Enter" || key == "Return") {
        return VK_RETURN;
    }
    if (key == "Tab") {
        return VK_TAB;
    }
    if (key == "CapsLock") {
        return VK_CAPITAL;
    }
    if (key == "Escape" || key == "Esc") {
        return VK_ESCAPE;
    }
    if (key == "Space") {
        return VK_SPACE;
    }
    if (key == "Backspace") {
        return VK_BACK;
    }
    if (key == "Delete" || key == "Del") {
        return VK_DELETE;
    }
    if (key == "Insert") {
        return VK_INSERT;
    }
    if (key == "ArrowUp" || key == "Up") {
        return VK_UP;
    }
    if (key == "ArrowDown" || key == "Down") {
        return VK_DOWN;
    }
    if (key == "ArrowLeft" || key == "Left") {
        return VK_LEFT;
    }
    if (key == "ArrowRight" || key == "Right") {
        return VK_RIGHT;
    }
    if (key == "Control" || key == "Ctrl") {
        return VK_CONTROL;
    }
    if (key == "Shift") {
        return VK_SHIFT;
    }
    if (key == "Alt") {
        return VK_MENU;
    }
    if (key == "PageUp") {
        return VK_PRIOR;
    }
    if (key == "PageDown") {
        return VK_NEXT;
    }
    if (key == "End") {
        return VK_END;
    }
    if (key == "Home") {
        return VK_HOME;
    }
    if (key == "F1") {
        return VK_F1;
    }
    if (key == "F2") {
        return VK_F2;
    }
    if (key == "F3") {
        return VK_F3;
    }
    if (key == "F4") {
        return VK_F4;
    }
    if (key == "F5") {
        return VK_F5;
    }
    if (key == "F6") {
        return VK_F6;
    }
    if (key == "F7") {
        return VK_F7;
    }
    if (key == "F8") {
        return VK_F8;
    }
    if (key == "F9") {
        return VK_F9;
    }
    if (key == "F10") {
        return VK_F10;
    }
    if (key == "F11") {
        return VK_F11;
    }
    if (key == "F12") {
        return VK_F12;
    }

    if (key.length() == 1) {
        char c = key[0];
        if (c >= 'a' && c <= 'z') {
            return 'A' + (c - 'a');
        }
        if (c >= 'A' && c <= 'Z') {
            return c;
        }
        if (c >= '0' && c <= '9') {
            return c;
        }
    }

    if (key == "`") {
        return VK_OEM_3;
    }
    if (key == "-") {
        return VK_OEM_MINUS;
    }
    if (key == "=") {
        return VK_OEM_PLUS;
    }
    if (key == "[") {
        return VK_OEM_4;
    }
    if (key == "]") {
        return VK_OEM_6;
    }
    if (key == "\\") {
        return VK_OEM_5;
    }
    if (key == ";") {
        return VK_OEM_1;
    }
    if (key == "'") {
        return VK_OEM_7;
    }
    if (key == ",") {
        return VK_OEM_COMMA;
    }
    if (key == ".") {
        return VK_OEM_PERIOD;
    }
    if (key == "/") {
        return VK_OEM_2;
    }
    return 0;
}

int32_t NWebAIInputMethodController::GetOhosKeyCodeFromVKey(int vkey)
{
    if (vkey >= 'A' && vkey <= 'Z') {
        return 1952 + vkey;
    }
    if (vkey >= '0' && vkey <= '9') {
        return 1952 + vkey;
    }
    if (vkey >= VK_F1 && vkey <= VK_F12) {
        return 2090 + (vkey - VK_F1);
    }
    switch (vkey) {
        case VK_RETURN:
            return 2054;
        case VK_TAB:
            return 2049;
        case VK_ESCAPE:
            return 2070;
        case VK_CAPITAL:
            return 2074;
        case VK_SPACE:
            return 2050;
        case VK_BACK:
            return 2055;
        case VK_DELETE:
            return 2071;
        case VK_UP:
            return 2012;
        case VK_DOWN:
            return 2013;
        case VK_LEFT:
            return 2014;
        case VK_RIGHT:
            return 2015;
        case VK_CONTROL:
            return 2072;
        case VK_SHIFT:
            return 2047;
        case VK_MENU:
            return 2045;
        case VK_INSERT:
            return 2083;
        case VK_PRIOR:
            return 2068;
        case VK_NEXT:
            return 2069;
        case VK_END:
            return 2082;
        case VK_HOME:
            return 2081;
        case VK_OEM_3:
            return 2056;
        case VK_OEM_MINUS:
            return 2057;
        case VK_OEM_PLUS:
            return 2058;
        case VK_OEM_4:
            return 2059;
        case VK_OEM_6:
            return 2060;
        case VK_OEM_5:
            return 2061;
        case VK_OEM_1:
            return 2062;
        case VK_OEM_7:
            return 2063;
        case VK_OEM_COMMA:
            return 2043;
        case VK_OEM_PERIOD:
            return 2044;
        case VK_OEM_2:
            return 2064;
        default:
            return 0;
    }
}


}  // namespace OHOS::NWeb
