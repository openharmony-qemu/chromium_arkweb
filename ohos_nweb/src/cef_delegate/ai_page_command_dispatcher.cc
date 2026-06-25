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

#include "ai_page_command_dispatcher.h"

#include <algorithm>
#include <array>
#include <optional>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "cef/ohos_cef_ext/include/arkweb_browser_ext.h"
#include "cef/ohos_cef_ext/include/arkweb_frame_ext.h"
#include "nweb.h"

namespace OHOS::NWeb {
namespace {

class AIPageCommandCallbackImpl : public CefAIPageCommandCallback {
 public:
  explicit AIPageCommandCallbackImpl(
      AIPageCommandDispatcher::ResultCallback callback)
      : callback_(callback) {}
 
  void OnResult(const CefString& result) override {
    if (callback_) {
      callback_->OnReceiveValue(result.ToString());
    }
  }
 
 private:
  AIPageCommandDispatcher::ResultCallback callback_;
 
  IMPLEMENT_REFCOUNTING(AIPageCommandCallbackImpl);
};

using CommandHandler = void (*)(CefRefPtr<ArkWebBrowserExt> browser,
                                const base::Value::Dict& command,
                                AIPageCommandDispatcher::ResultCallback callback);

struct CommandEntry {
  const char* method;
  CommandHandler handler;
};

}  // namespace

void AIPageCommandDispatcher::Dispatch(const std::string& command,
                                       CefRefPtr<ArkWebBrowserExt> browser,
                                       ResultCallback callback) {
  std::optional<base::Value> commandValue =
    base::JSONReader::Read(command, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!commandValue || !commandValue->is_dict()) {
    ResolveEmptyResult(callback);
    return;
  }

  const base::Value::Dict& commandDict = commandValue->GetDict();
  const std::string* method = commandDict.FindString("method");
  if (method == nullptr || method->empty()) {
    ResolveEmptyResult(callback);
    return;
  }

  static constexpr std::array<CommandEntry, 2> kCommandEntries = {{
      {"getFullDom", &AIPageCommandDispatcher::HandleGetFullDomCommand},
      {"getLiteDom", &AIPageCommandDispatcher::HandleGetLiteDomCommand},
  }};

  auto entry = std::find_if(
      kCommandEntries.begin(), kCommandEntries.end(),
      [method](const CommandEntry& item) { return *method == item.method; });
  if (entry == kCommandEntries.end()) {
    ResolveEmptyResult(callback);
    return;
  }

  entry->handler(browser, commandDict, callback);
}

void AIPageCommandDispatcher::ResolveEmptyResult(ResultCallback callback) {
  if (callback) {
    callback->OnReceiveValue("");
  }
}

std::string BuildParamsJson(const base::Value::Dict& command) {
  std::string paramsJson = "{}";
  if (const base::Value::Dict* params = command.FindDict("params")) {
    base::JSONWriter::Write(*params, &paramsJson);
  }
  return paramsJson;
}

void AIPageCommandDispatcher::HandleGetFullDomCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  if (browser == nullptr || browser->GetMainFrame() == nullptr ||
      browser->GetMainFrame()->AsArkWebFrame() == nullptr) {
    ResolveEmptyResult(callback);
    return;
  }
 
  CefRefPtr<AIPageCommandCallbackImpl> cefCallback =
      new AIPageCommandCallbackImpl(callback);
  browser->GetMainFrame()->AsArkWebFrame()->GetFullDom(BuildParamsJson(command),
                                                       cefCallback);
}

void AIPageCommandDispatcher::HandleGetLiteDomCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  if (browser == nullptr || browser->GetMainFrame() == nullptr ||
      browser->GetMainFrame()->AsArkWebFrame() == nullptr) {
    ResolveEmptyResult(callback);
    return;
  }
 
  CefRefPtr<AIPageCommandCallbackImpl> cefCallback =
      new AIPageCommandCallbackImpl(callback);
  browser->GetMainFrame()->AsArkWebFrame()->GetLiteDom(BuildParamsJson(command),
                                                       cefCallback);
}

}  // namespace OHOS::NWeb