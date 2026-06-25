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

#include <unistd.h>

#include <algorithm>
#include <array>
#include <optional>

#include "arkweb/build/features/features.h"
#include "base/datashare_uri_utils.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "cef/ohos_cef_ext/include/arkweb_browser_ext.h"
#include "cef/ohos_cef_ext/include/arkweb_frame_ext.h"
#include "file_upload_observer.h"
#include "include/cef_values.h"
#include "nweb.h"
#include "nweb_errors.h"
#include "screenshot_observer.h"

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

std::string BuildResultJson(int32_t code, const std::string &message) {
  base::Value::Dict result;
  result.Set("code", code);
  result.Set("message", message);
  std::string json;
  base::JSONWriter::Write(result, &json);
  return json;
}
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

  std::array<CommandEntry, 6> kCommandEntries = {{
      {"getFullDom", &AIPageCommandDispatcher::HandleGetFullDomCommand},
      {"getLiteDom", &AIPageCommandDispatcher::HandleGetLiteDomCommand},
      {"scroll", &AIPageCommandDispatcher::HandleScrollCommand},
      {"select", &AIPageCommandDispatcher::HandleSelectCommand},
      {"uploadFile", &AIPageCommandDispatcher::HandleUploadFileCommand},
      {"screenCapture", &AIPageCommandDispatcher::HandleScreenCaptureCommand},
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

// --- Private helpers ---

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

void AIPageCommandDispatcher::ReplyError(ResultCallback callback,
                                         WebCommandErrorCode code,
                                         const std::string &message) {
  if (callback) {
    callback->OnReceiveValue(
        BuildResultJson(static_cast<int32_t>(code), message));
  }
}

// --- Existing DOM handlers (unchanged from eb6ea2df) ---

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

// --- Scroll ---
CefRefPtr<CefDictionaryValue> AIPageCommandDispatcher::BuildScrollParams(
    const base::Value::Dict& params) {
  auto x = params.FindDouble("x");
  auto y = params.FindDouble("y");
  if (!x.has_value() || !y.has_value()) {
    return nullptr;
  }

  CefRefPtr<CefDictionaryValue> cdp = CefDictionaryValue::Create();
  cdp->SetDouble("x", x.value());
  cdp->SetDouble("y", y.value());
  if (auto v = params.FindDouble("deltaX")) {
    cdp->SetDouble("xDistance", v.value());
  }
  if (auto v = params.FindDouble("deltaY")) {
    cdp->SetDouble("yDistance", v.value());
  }
  if (auto v = params.FindInt("speed")) {
    cdp->SetInt("speed", v.value());
  }
  return cdp;
}

void AIPageCommandDispatcher::HandleScrollCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  if (!browser || !browser->GetHost()) {
    LOG(ERROR) << "[AIPageCmd] browser or host is null";
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }

  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    LOG(ERROR) << "[AIPageCmd] scroll: invalid params JSON";
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "invalid params JSON");
    return;
  }

  // Required params must exist
  if (!params->Find("x") || !params->Find("y")) {
    LOG(ERROR) << "[AIPageCmd] scroll: missing x or y";
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING, "missing x or y");
    return;
  }

  // Numeric params: present but wrong type → PARAM_INVALID with param name
  for (const char* key : {"x", "y", "deltaX", "deltaY"}) {
    const base::Value* v = params->Find(key);
    if (v && !v->is_int() && !v->is_double()) {
      LOG(ERROR) << "[AIPageCmd] scroll: invalid param " << key;
      ReplyError(callback, WebCommandErrorCode::PARAM_INVALID,
                 std::string("invalid param: ") + key);
      return;
    }
  }
  const base::Value* v = params->Find("speed");
  if (v && (!v->is_int() || v->GetInt() == 0)) {
    LOG(ERROR) << "[AIPageCmd] scroll: invalid param speed";
    ReplyError(callback, WebCommandErrorCode::PARAM_INVALID,
               "invalid param: speed");
    return;
  }

  CefRefPtr<CefDictionaryValue> cdp = BuildScrollParams(*params);
  int result = browser->GetHost()->ExecuteDevToolsMethod(
      0, "Input.synthesizeScrollGesture", cdp);
  if (result == 0) {
    LOG(ERROR) << "[AIPageCmd] scroll gesture CDP send failed";
    ReplyError(callback, WebCommandErrorCode::CDP_SEND_FAILED,
               "SendDevToolsMessage failed");
    return;
  }
  ReplyError(callback, WebCommandErrorCode::SUCCESS, "success");
}

// --- Select ---

void AIPageCommandDispatcher::HandleSelectCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
#if !BUILDFLAG(ARKWEB_INPUT_EVENTS)
  ResolveEmptyResult(callback);
  return;
#else
  if (!browser || !browser->GetHost()) {
    LOG(ERROR) << "[AIPageCmd] browser or host is null";
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }

  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    LOG(ERROR) << "[AIPageCmd] select: invalid params JSON";
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "invalid params JSON");
    return;
  }

  const std::string* xpath = params->FindString("xpath");
  if (!xpath || xpath->empty()) {
    LOG(ERROR) << "[AIPageCmd] select: missing xpath";
    ReplyError(callback, WebCommandErrorCode::JSON_INVALID_XPATH,
               "missing or empty xpath");
    return;
  }

  std::vector<int32_t> indexes = ParseIntList(*params, "indexes");
  std::vector<std::string> values = ParseStringList(*params, "values");
  if (indexes.empty() && values.empty()) {
    LOG(ERROR) << "[AIPageCmd] select: both indexes and values empty";
    ReplyError(callback, WebCommandErrorCode::JSON_INVALID_SELECT_OPTIONS,
               "both indexes and values are empty");
    return;
  }

  ExecuteSelectAndReply(browser, *xpath, indexes, values, callback);
#endif
}

std::vector<int32_t> AIPageCommandDispatcher::ParseIntList(
    const base::Value::Dict& dict,
    const std::string& key) {
  std::vector<int32_t> result;
  const base::Value::List* list = dict.FindList(key);
  if (!list) {
    return result;
  }
  result.reserve(list->size());
  for (const auto& item : *list) {
    if (item.is_int()) {
      result.push_back(static_cast<int32_t>(item.GetInt()));
    }
  }
  return result;
}

std::vector<std::string> AIPageCommandDispatcher::ParseStringList(
    const base::Value::Dict& dict,
    const std::string& key) {
  std::vector<std::string> result;
  const base::Value::List* list = dict.FindList(key);
  if (!list) {
    return result;
  }
  result.reserve(list->size());
  for (const auto& item : *list) {
    if (item.is_string()) {
      result.push_back(item.GetString());
    }
  }
  return result;
}

void AIPageCommandDispatcher::ExecuteSelectAndReply(
    CefRefPtr<ArkWebBrowserExt> browser,
    const std::string& xpath,
    const std::vector<int32_t>& indexes,
    const std::vector<std::string>& values,
    ResultCallback callback) {
  int32_t result =
      browser->GetHost()->ExecuteSelectCommand(xpath, indexes, values);
  if (callback) {
    callback->OnReceiveValue(BuildResultJson(
        result, result == static_cast<int32_t>(WebCommandErrorCode::SUCCESS)
                    ? "success"
                    : "select command failed"));
  }
}

// --- Upload file ---

void AIPageCommandDispatcher::HandleUploadFileCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  if (!browser || !browser->GetHost()) {
    LOG(ERROR) << "[AIPageCmd] browser or host is null";
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }

  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    LOG(ERROR) << "[AIPageCmd] uploadFile: invalid params JSON";
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "invalid params JSON");
    return;
  }

  auto files = ParseStringList(*params, "files");
  if (files.empty()) {
    LOG(ERROR) << "[AIPageCmd] uploadFile: files list is empty";
    ReplyError(callback, WebCommandErrorCode::FILE_LIST_EMPTY,
               "files list is empty");
    return;
  }

  std::vector<std::string> warnings;
  if (!ValidateFilePaths(files, warnings, callback)) {
    return;
  }

  DispatchUploadTarget(browser, *params, files, warnings, callback);
}

bool AIPageCommandDispatcher::ValidateFilePaths(
    std::vector<std::string>& files,
    std::vector<std::string>& warnings,
    ResultCallback callback) {
  for (auto& path : files) {
    if (path.empty()) {
      LOG(ERROR) << "[AIPageCmd] uploadFile: empty file path";
      ReplyError(callback, WebCommandErrorCode::FILE_PATH_EMPTY,
                 "empty file path");
      return false;
    }
    std::string original_path = path;
    std::string real_path = base::GetRealPath(base::FilePath(path));
    if (real_path.empty()) {
      LOG(ERROR) << "[AIPageCmd] uploadFile: failed to resolve path: "
                 << original_path;
      ReplyError(callback, WebCommandErrorCode::FILE_NOT_FOUND,
                 "failed to resolve file path: " + original_path);
      return false;
    }
    path = std::move(real_path);

    if (!base::PathExists(base::FilePath(path))) {
      LOG(WARNING) << "[AIPageCmd] uploadFile: path does not exist: "
                   << original_path;
      warnings.push_back(original_path + ": path does not exist");
    } else if (!base::PathIsReadable(base::FilePath(path))) {
      LOG(WARNING) << "[AIPageCmd] uploadFile: path not readable: "
                   << original_path;
      warnings.push_back(original_path + ": path not readable");
    }
  }
  return true;
}

void AIPageCommandDispatcher::DispatchUploadTarget(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& dict,
    const std::vector<std::string>& files,
    const std::vector<std::string>& warnings,
    ResultCallback callback) {
  const std::string* xpath = dict.FindString("xpath");
  if (!xpath || xpath->empty()) {
    LOG(ERROR) << "[AIPageCmd] uploadFile: xpath is required";
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING,
               "xpath is required");
    return;
  }

  auto handler = new FileUploadObserver(browser, *xpath, files,
                                                warnings, callback);
  handler->Start();
}

void AIPageCommandDispatcher::HandleScreenCaptureCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict &command,
    ResultCallback callback) {
  if (!browser || !browser->GetHost()) {
    LOG(ERROR) << "[AIPageCmd] browser or host is null";
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }

  CefRefPtr<ScreenshotObserver> observer =
         new ScreenshotObserver(browser, callback);

  // Check for capture target parameters
  LOG(DEBUG) << "[AIPageCmd]" << " screenCapture: command=" << BuildParamsJson(command);
  const base::Value::Dict *params = command.FindDict("params");
  const std::string *selector = nullptr;
  std::optional<int> node_id;
  if (params) {
    selector = params->FindString("selector");
    node_id = params->FindInt("node_id");
  }
  if (node_id.has_value() && node_id.value() > 0) {
    LOG(DEBUG) << "[AIPageCmd] screenCapture: using node_id=" << node_id.value();
    observer->CaptureElementByNodeId(node_id.value());
  } else if (selector && !selector->empty()) {
    LOG(DEBUG) << "[AIPageCmd] screenCapture: using selector=" << *selector;
    observer->CaptureElement(*selector);
  } else {
    LOG(DEBUG) << "[AIPageCmd] screenCapture: full page capture";
    observer->CaptureFullPage();
  }
}

}  // namespace OHOS::NWeb