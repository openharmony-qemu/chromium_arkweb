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
#include <string_view>
#include <utility>

#include "arkweb/build/features/features.h"
#include "base/datashare_uri_utils.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "cef/libcef/browser/frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "cef/ohos_cef_ext/include/arkweb_browser_ext.h"
#include "cef/ohos_cef_ext/include/arkweb_frame_ext.h"
#include "file_upload_observer.h"
#include "include/cef_values.h"
#include "nweb.h"
#include "nweb_errors.h"
#include "screenshot_observer.h"
#include "url/gurl.h"

namespace OHOS::NWeb {
namespace {

std::string BuildResultJson(int32_t code, const std::string& message);
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

void ResolveCommandResult(AIPageCommandDispatcher::ResultCallback callback,
                          WebCommandErrorCode code,
                          const std::string& message) {
  if (callback) {
    callback->OnReceiveValue(
        BuildResultJson(static_cast<int32_t>(code), message));
  }
}

class ValidateElementIdCallbackImpl : public CefAIPageCommandCallback {
 public:
  ValidateElementIdCallbackImpl(CefRefPtr<ScreenshotObserver> observer,
                                 int node_id,
                                 AIPageCommandDispatcher::ResultCallback callback)
      : observer_(observer), node_id_(node_id), callback_(callback) {}
 
  void OnResult(const CefString& result) override {
    std::optional<base::Value> value =
        base::JSONReader::Read(result.ToString(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (!value || !value->is_dict()) {
      LOG(ERROR) << "[AIPageCmd] screenCapture: ValidateElementId invalid result";
      ResolveCommandResult(callback_, WebCommandErrorCode::FAILED, "validation failed");
      return;
    }
    const base::Value::Dict& dict = value->GetDict();
    std::optional<bool> ok = dict.FindBool("ok");
    if (!ok || !ok.value()) {
      const std::string* error = dict.FindString("error");
      std::string error_msg = error ? *error : "validation failed";
      LOG(ERROR) << "[AIPageCmd] screenCapture: ValidateElementId failed: " << error_msg;
      ResolveCommandResult(callback_, WebCommandErrorCode::PARAM_INVALID, "invalid param: nodeids, " + error_msg);
      return;
    }
    LOG(INFO) << "[AIPageCmd] screenCapture: ValidateElementId passed, node_id=" << node_id_;
    observer_->CaptureElementByNodeId(node_id_);
  }
 
 private:
  CefRefPtr<ScreenshotObserver> observer_;
  int node_id_;
  AIPageCommandDispatcher::ResultCallback callback_;
  IMPLEMENT_REFCOUNTING(ValidateElementIdCallbackImpl);
};

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

std::string WriteJson(base::Value::Dict value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

std::string WriteJsonString(const std::string& value) {
  std::string json;
  base::JSONWriter::Write(base::Value(value), &json);
  return json;
}

void ResolveNavigationOk(AIPageCommandDispatcher::ResultCallback callback) {
  if (!callback) {
    return;
  }
  callback->OnReceiveValue("{\"ok\":true}");
}

void ResolveNavigationOk(AIPageCommandDispatcher::ResultCallback callback,
                         base::Value::Dict payload) {
  if (!callback) {
    return;
  }
  callback->OnReceiveValue("{\"ok\":true,\"result\":" +
                           WriteJson(std::move(payload)) + "}");
}

void ResolveNavigationError(AIPageCommandDispatcher::ResultCallback callback,
                            const std::string& error) {
  if (!callback) {
    return;
  }
  callback->OnReceiveValue("{\"ok\":false,\"error\":" +
                           WriteJsonString(error) + "}");
}

bool IsAllowedNavigationUrl(const std::string& url) {
  GURL gurl(url);
  return gurl.is_valid() &&
         (gurl.SchemeIsHTTPOrHTTPS() || gurl.SchemeIsFile() ||
          gurl.SchemeIs("about"));
}

class AIPageNavigationEntryVisitor : public CefNavigationEntryVisitor {
 public:
  bool Visit(CefRefPtr<CefNavigationEntry> entry,
             bool current,
             int index,
             int /*total*/) override {
    if (current) {
      current_index_ = index;
    }

    base::Value::Dict item;
    item.Set("index", index);
    if (entry && entry->IsValid()) {
      item.Set("url", entry->GetURL().ToString());
      item.Set("title", entry->GetTitle().ToString());
    } else {
      item.Set("url", std::string());
      item.Set("title", std::string());
    }
    entries_.Append(std::move(item));
    return true;
  }

  base::Value::Dict TakeResult(CefRefPtr<ArkWebBrowserExt> browser) {
    base::Value::Dict result;
    result.Set("currentIndex", current_index_);
    result.Set("canGoBack", browser && browser->CanGoBack());
    result.Set("canGoForward", browser && browser->CanGoForward());
    result.Set("entries", std::move(entries_));
    return result;
  }

 private:
  int current_index_ = -1;
  base::Value::List entries_;

  IMPLEMENT_REFCOUNTING(AIPageNavigationEntryVisitor);
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

  std::array<CommandEntry, 12> kCommandEntries = {{
      {"getFullDom", &AIPageCommandDispatcher::HandleGetFullDomCommand},
      {"getLiteDom", &AIPageCommandDispatcher::HandleGetLiteDomCommand},
      {"scroll", &AIPageCommandDispatcher::HandleScrollCommand},
      {"select", &AIPageCommandDispatcher::HandleSelectCommand},
      {"uploadFile", &AIPageCommandDispatcher::HandleUploadFileCommand},
      {"screenCapture", &AIPageCommandDispatcher::HandleScreenCaptureCommand},
      {"addPageAnnotation", &AIPageCommandDispatcher::HandleAddPageAnnotationCommand},
      {"removePageAnnotation", &AIPageCommandDispatcher::HandleRemovePageAnnotationCommand},
      {"getUrlHistory", &AIPageCommandDispatcher::HandleGetUrlHistoryCommand},
      {"goBack", &AIPageCommandDispatcher::HandleGoBackCommand},
      {"goForward", &AIPageCommandDispatcher::HandleGoForwardCommand},
      {"navigate", &AIPageCommandDispatcher::HandleNavigateCommand},
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

struct ParsedElementId {
  std::string frame_token;
  std::string doc_token;
  int dom_node_id;
};

std::optional<ParsedElementId> ParseElementId(const std::string& node_id_str) {
  std::string_view element_id(node_id_str);

  size_t dom_separator = element_id.rfind('|');
  if (dom_separator == std::string_view::npos ||
      dom_separator + 1 >= element_id.size()) {
    return std::nullopt;
  }

  size_t document_separator = element_id.rfind('|', dom_separator - 1);
  if (document_separator == std::string_view::npos || document_separator == 0 ||
      document_separator + 1 == dom_separator) {
    return std::nullopt;
  }

  int dom_node_id = 0;
  if (!base::StringToInt(std::string(element_id.substr(dom_separator + 1)), &dom_node_id) ||
      dom_node_id <= 0) {
    return std::nullopt;
  }

  ParsedElementId parsed;
  parsed.frame_token = std::string(element_id.substr(0, document_separator));
  parsed.doc_token = std::string(element_id.substr(
      document_separator + 1, dom_separator - document_separator - 1));
  parsed.dom_node_id = dom_node_id;
  return parsed;
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
  const std::string *xpath = nullptr;

  if (params) {
    xpath = params->FindString("xpath");
    const std::string *node_id_str = params->FindString("nodeid");

    if (node_id_str && !node_id_str->empty()) {
      auto element_id = ParseElementId(*node_id_str);
      if (!element_id) {
        LOG(ERROR) << "[AIPageCmd] screenCapture: invalid nodeid format: " << *node_id_str;
        ReplyError(callback, WebCommandErrorCode::PARAM_INVALID, "invalid param: nodeid");
        return;
      }

      LOG(INFO) << "[AIPageCmd] screenCapture: parsed nodeid=" << element_id->dom_node_id
                << " from " << *node_id_str;
 
      browser->GetHost()->ValidateElementId(
          *node_id_str,
          new ValidateElementIdCallbackImpl(observer, element_id->dom_node_id, callback));
      return;
    }
  }

  if (xpath && !xpath->empty()) {
    LOG(INFO) << "[AIPageCmd] screenCapture: using xpath=" << *xpath;
    observer->CaptureElement(*xpath);
  } else {
    LOG(INFO) << "[AIPageCmd] screenCapture: full page capture";
    observer->CaptureFullPage();
  }
}

void AIPageCommandDispatcher::HandleAddPageAnnotationCommand(
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
  browser->GetMainFrame()->AsArkWebFrame()->AddPageAnnotation(
      BuildParamsJson(command), cefCallback);
}

void AIPageCommandDispatcher::HandleRemovePageAnnotationCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict&,
    ResultCallback callback) {
  if (browser == nullptr || browser->GetMainFrame() == nullptr ||
      browser->GetMainFrame()->AsArkWebFrame() == nullptr) {
    ResolveEmptyResult(callback);
    return;
  }

  CefRefPtr<AIPageCommandCallbackImpl> cefCallback =
      new AIPageCommandCallbackImpl(callback);
  browser->GetMainFrame()->AsArkWebFrame()->RemovePageAnnotation(cefCallback);
}

void AIPageCommandDispatcher::HandleGetUrlHistoryCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict&,
    ResultCallback callback) {
  if (browser == nullptr || browser->GetHost() == nullptr) {
    ResolveNavigationError(callback, "browser host is not available");
    return;
  }

  CefRefPtr<AIPageNavigationEntryVisitor> visitor =
      new AIPageNavigationEntryVisitor();
  browser->GetHost()->GetNavigationEntries(visitor, false);
  ResolveNavigationOk(callback, visitor->TakeResult(browser));
}

void AIPageCommandDispatcher::HandleGoBackCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict&,
    ResultCallback callback) {
  if (browser == nullptr) {
    ResolveNavigationError(callback, "browser is not available");
    return;
  }
  if (!browser->CanGoBack()) {
    ResolveNavigationError(callback, "Cannot go back");
    return;
  }

  browser->GoBack();
  ResolveNavigationOk(callback);
}

void AIPageCommandDispatcher::HandleGoForwardCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict&,
    ResultCallback callback) {
  if (browser == nullptr) {
    ResolveNavigationError(callback, "browser is not available");
    return;
  }
  if (!browser->CanGoForward()) {
    ResolveNavigationError(callback, "Cannot go forward");
    return;
  }

  browser->GoForward();
  ResolveNavigationOk(callback);
}

void AIPageCommandDispatcher::HandleNavigateCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  const base::Value::Dict* params = command.FindDict("params");
  const std::string* url = params ? params->FindString("url") : nullptr;
  if (!url || url->empty()) {
    ResolveNavigationError(callback, "params.url is required");
    return;
  }
  if (!IsAllowedNavigationUrl(*url)) {
    ResolveNavigationError(callback, "params.url is invalid");
    return;
  }

  if (browser == nullptr || browser->GetMainFrame() == nullptr) {
    ResolveNavigationError(callback, "main frame is not available");
    return;
  }

  browser->GetMainFrame()->LoadURL(CefString(*url));
  ResolveNavigationOk(callback);
}

}  // namespace OHOS::NWeb