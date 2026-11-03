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

#ifndef NWEB_AI_INPUTMETHOD_CONTROLLER_H
#define NWEB_AI_INPUTMETHOD_CONTROLLER_H

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "cef/include/cef_base.h"
#include "nweb.h"

namespace OHOS::NWeb {

class NWebInputHandler;
class NWebDelegateInterface;
class NWebInputMethodHandler;

class NWebAIInputMethodController {
public:
    NWebAIInputMethodController(std::shared_ptr<NWebInputHandler> inputHandler,
                     std::shared_ptr<NWebDelegateInterface> nwebDelegate,
                     CefRefPtr<NWebInputMethodHandler> inputMethodHandler);
    ~NWebAIInputMethodController() = default;

    std::string ExecuteJsonAction(const base::Value::Dict& jsonDict);

private:
    std::string HandleClickOrFocus(const base::Value::Dict& jsonDict, const std::string& method);
    std::string HandleCursorPosition(const base::Value::Dict& jsonDict);
    std::string HandleType(const base::Value::Dict& jsonDict);
    std::string HandleSendKeys(const base::Value::Dict& jsonDict);
    std::string CombineClickFocusJsString(const std::string& eventType, const std::string& xPath);
    std::string CombineInputInsertCommandJsString(const std::string& xPath,
                                                  const std::string& content,
                                                  int32_t index);
    std::string CombineInputModifyCommandJsString(const std::string& xPath,
                                                  const std::string& content);

    bool NeedShiftForMainKey(const std::string& mainKey);
    void EnsureShiftInModifiers(std::vector<int32_t>& modifierKeyCodes);
    void SendCombinedKeyEvent(int32_t mainKeycode, const std::vector<int32_t>& modifierKeyCodes);

    int32_t GetVKeyFromWebKey(const std::string& key);
    int32_t GetOhosKeyCodeFromVKey(int vkey);
    int32_t GetMainKeyCode(const std::string& mainKey);

    std::shared_ptr<NWebInputHandler> inputHandler_;
    std::shared_ptr<NWebDelegateInterface> nwebDelegate_;
    CefRefPtr<NWebInputMethodHandler> inputMethodHandler_;
    std::vector<std::string> SplitKeys(const std::string& keys);
    std::vector<int32_t> BuildModifierKeyCodes(const std::vector<std::string>& modifierKeys);
};

}  // namespace OHOS::NWeb

#endif  // NWEB_AI_INPUTMETHOD_CONTROLLER_H
