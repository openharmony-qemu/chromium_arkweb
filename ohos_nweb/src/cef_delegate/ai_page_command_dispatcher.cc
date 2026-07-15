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

#include "ai_page_command_param_utils.h"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
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
#include "nweb_preference_delegate.h"
#include "nweb_delegate.h"
#include "third_party/blink/public/common/page/page_zoom.h"
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

using CommandHandler = AIPageCommandDispatcher::CommandHandler;
constexpr char kAnnotationCommandFailed[] = "annotation command failed";

void SetAnnotationListField(base::Value::Dict& output,
                            const base::Value::Dict& source,
                            const char* key) {
  const base::Value::List* list = source.FindList(key);
  output.Set(key, list ? list->Clone() : base::Value::List());
}

std::string BuildAnnotationCommandResultJson(const std::string& cef_result,
                                             bool include_details) {
  std::optional<base::Value> value =
      base::JSONReader::Read(cef_result, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!value || !value->is_dict()) {
    return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::FAILED),
                           kAnnotationCommandFailed);
  }

  const base::Value::Dict& cef_dict = value->GetDict();
  std::optional<bool> ok = cef_dict.FindBool("ok");
  if (!ok) {
    return BuildResultJson(static_cast<int32_t>(WebCommandErrorCode::FAILED),
                           kAnnotationCommandFailed);
  }

  if (!ok.value()) {
    const std::string* error = cef_dict.FindString("error");
    std::string message =
        (error && !error->empty()) ? *error : kAnnotationCommandFailed;
    WebCommandErrorCode code = message == "timed out"
                                   ? WebCommandErrorCode::WEB_EXECUTE_TIMEOUT
                                   : WebCommandErrorCode::FAILED;
    return BuildResultJson(static_cast<int32_t>(code), message);
  }

  base::Value::Dict output;
  output.Set("code", static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  output.Set("message", "success");
  if (include_details) {
    output.Set("renderedCount", cef_dict.FindInt("renderedCount").value_or(0));
    output.Set("truncated", cef_dict.FindBool("truncated").value_or(false));
    SetAnnotationListField(output, cef_dict, "missingElementIds");
    SetAnnotationListField(output, cef_dict, "invalidElementIds");
  }

  std::string json;
  base::JSONWriter::Write(output, &json);
  return json;
}

class AIPageAnnotationCommandCallbackImpl : public CefAIPageCommandCallback {
 public:
  AIPageAnnotationCommandCallbackImpl(
      AIPageCommandDispatcher::ResultCallback callback,
      bool include_details)
      : callback_(callback), include_details_(include_details) {}

  void OnResult(const CefString& result) override {
    if (callback_) {
      callback_->OnReceiveValue(BuildAnnotationCommandResultJson(
          result.ToString(), include_details_));
    }
  }

 private:
  AIPageCommandDispatcher::ResultCallback callback_;
  bool include_details_ = false;

  IMPLEMENT_REFCOUNTING(AIPageAnnotationCommandCallbackImpl);
};

void ResolveCommandResult(AIPageCommandDispatcher::ResultCallback callback,
                          WebCommandErrorCode code,
                          const std::string& message) {
  if (callback) {
    callback->OnReceiveValue(
        BuildResultJson(static_cast<int32_t>(code), message));
  }
}

std::optional<double> ReadNumberField(const base::Value::Dict& dict,
                                      const char* key) {
  const base::Value* value = dict.Find(key);
  if (!value || (!value->is_int() && !value->is_double())) {
    return std::nullopt;
  }
  return value->is_int() ? value->GetInt() : value->GetDouble();
}

void ExecuteScrollGestureAndReply(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& params,
    double x,
    double y,
    AIPageCommandDispatcher::ResultCallback callback) {
  if (!browser || !browser->GetHost()) {
    LOG(ERROR) << "[AIPageCmd] scroll: browser or host is null";
    ResolveCommandResult(callback, WebCommandErrorCode::WEB_NWEB_NULL,
                         "browser or host is null");
    return;
  }

  CefRefPtr<CefDictionaryValue> cdp =
      AIPageCommandParamUtils::BuildScrollGestureParams(params, x, y);
  int result = browser->GetHost()->ExecuteDevToolsMethod(
      0, "Input.synthesizeScrollGesture", cdp);
  if (result == 0) {
    LOG(ERROR) << "[AIPageCmd] scroll gesture CDP send failed";
    ResolveCommandResult(callback, WebCommandErrorCode::CDP_SEND_FAILED,
                         "SendDevToolsMessage failed");
    return;
  }
  ResolveCommandResult(callback, WebCommandErrorCode::SUCCESS, "success");
}

void HandleResolvedScrollRectResult(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& params,
    const std::string& rect_result,
    AIPageCommandDispatcher::ResultCallback callback) {
  std::optional<base::Value> value =
      base::JSONReader::Read(rect_result, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!value || !value->is_dict()) {
    ResolveCommandResult(callback, WebCommandErrorCode::ELEMENT_NOT_FOUND,
                         "element rect is not available");
    return;
  }

  const base::Value::Dict& result = value->GetDict();
  if (!result.FindBool("ok").value_or(false)) {
    ResolveCommandResult(callback, WebCommandErrorCode::ELEMENT_NOT_FOUND,
                         "element rect is not available");
    return;
  }
  if (result.FindBool("timedOut").value_or(false)) {
    ResolveCommandResult(callback, WebCommandErrorCode::WEB_EXECUTE_TIMEOUT,
                         "element rect resolve timeout");
    return;
  }
  const base::Value::List* invalid_ids = result.FindList("invalidElementIds");
  if (invalid_ids && !invalid_ids->empty()) {
    ResolveCommandResult(callback, WebCommandErrorCode::PARAM_INVALID,
                         "invalid id");
    return;
  }
  const base::Value::List* missing_ids = result.FindList("missingElementIds");
  if (missing_ids && !missing_ids->empty()) {
    ResolveCommandResult(callback, WebCommandErrorCode::ELEMENT_NOT_FOUND,
                         "element not found");
    return;
  }

  const base::Value::List* items = result.FindList("items");
  if (!items || items->empty() || !(*items)[0].is_dict()) {
    ResolveCommandResult(callback, WebCommandErrorCode::ELEMENT_NOT_FOUND,
                         "element rect is empty");
    return;
  }
  const base::Value::Dict& item = (*items)[0].GetDict();
  const base::Value::List* rects = item.FindList("rects");
  if (!rects || rects->empty() || !(*rects)[0].is_dict()) {
    ResolveCommandResult(callback, WebCommandErrorCode::ELEMENT_NOT_FOUND,
                         "element rect is empty");
    return;
  }
  const base::Value::Dict& rect = (*rects)[0].GetDict();
  std::optional<double> left = ReadNumberField(rect, "left");
  std::optional<double> top = ReadNumberField(rect, "top");
  std::optional<double> width = ReadNumberField(rect, "width");
  std::optional<double> height = ReadNumberField(rect, "height");
  if (!left || !top || !width || !height || *width <= 0 || *height <= 0) {
    ResolveCommandResult(callback, WebCommandErrorCode::ELEMENT_NOT_FOUND,
                         "element rect is empty");
    return;
  }

  ExecuteScrollGestureAndReply(browser, params, *left + *width / 2,
                               *top + *height / 2, callback);
}

class AIPageScrollRectCallbackImpl : public CefAIPageCommandCallback {
 public:
  AIPageScrollRectCallbackImpl(CefRefPtr<ArkWebBrowserExt> browser,
                               base::Value::Dict params,
                               AIPageCommandDispatcher::ResultCallback callback)
      : browser_(browser), params_(std::move(params)), callback_(callback) {}

  void OnResult(const CefString& result) override {
    HandleResolvedScrollRectResult(browser_, params_, result.ToString(),
                                   callback_);
  }

 private:
  CefRefPtr<ArkWebBrowserExt> browser_;
  base::Value::Dict params_;
  AIPageCommandDispatcher::ResultCallback callback_;

  IMPLEMENT_REFCOUNTING(AIPageScrollRectCallbackImpl);
};

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
      ResolveCommandResult(callback_, WebCommandErrorCode::PARAM_INVALID, "invalid param: nodeid, " + error_msg);
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

std::string BuildResultJson(int32_t code,
                            const std::string& message,
                            base::Value::Dict payload) {
  base::Value::Dict result;
  result.Set("code", code);
  result.Set("message", message);
  result.Set("result", std::move(payload));
  std::string json;
  base::JSONWriter::Write(result, &json);
  return json;
}

void ResolveNavigationSuccess(
    AIPageCommandDispatcher::ResultCallback callback) {
  ResolveCommandResult(callback, WebCommandErrorCode::SUCCESS, "success");
}

void ResolveNavigationSuccess(
    AIPageCommandDispatcher::ResultCallback callback,
    base::Value::Dict payload) {
  if (!callback) {
    return;
  }
  callback->OnReceiveValue(BuildResultJson(
      static_cast<int32_t>(WebCommandErrorCode::SUCCESS), "success",
      std::move(payload)));
}

void ResolveNavigationFailure(
    AIPageCommandDispatcher::ResultCallback callback,
    WebCommandErrorCode code,
    const std::string& message) {
  ResolveCommandResult(callback, code, message);
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
// Zoom factor range enforced by the spec AC-1.2/AC-1.3/AC-1.4.
constexpr double kZoomFactorMin = 0.25;
constexpr double kZoomFactorMax = 5.0;
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

  std::array<CommandEntry, 17> kCommandEntries = {{
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
      {"setZoomLevel", &AIPageCommandDispatcher::HandleSetZoomLevelCommand},
      {"getZoomLevel", &AIPageCommandDispatcher::HandleGetZoomLevelCommand},
      {"dispatchMouseEvent", &AIPageCommandDispatcher::HandleDispatchMouseEventCommand},
      {"dispatchKeyEvent", &AIPageCommandDispatcher::HandleDispatchKeyEventCommand},
      {"input", &AIPageCommandDispatcher::HandleInputCommand},
  }};

  auto entry = std::find_if(
      kCommandEntries.begin(), kCommandEntries.end(),
      [method](const CommandEntry& item) { return *method == item.method; });
  if (entry == kCommandEntries.end()) {
    ResolveEmptyResult(callback);
    return;
  }

  (this->*entry->handler)(browser, commandDict, callback);
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

bool IsValidAnnotationElementList(const base::Value::List& element_list) {
  if (element_list.empty()) {
    return false;
  }
  return std::all_of(element_list.begin(), element_list.end(),
                     [](const base::Value& item) {
                       return item.is_string() && !item.GetString().empty();
                     });
}

void AIPageCommandDispatcher::ReplyError(ResultCallback callback,
                                         WebCommandErrorCode code,
                                         const std::string &message) {
  if (callback) {
    callback->OnReceiveValue(
        BuildResultJson(static_cast<int32_t>(code), message));
  }
}

bool AIPageCommandDispatcher::ResolveZoomPref(
    bool& out_zoom_control_access) {
  out_zoom_control_access = true;
  NWebDelegate* delegate = delegate_.get();
  if (!delegate) {
    LOG(ERROR) << "[AIPageCmd] ResolveZoomPref: delegate is nullptr";
    return false;
  }
  NWebPreferenceDelegate* pref = delegate->GetPreferenceDelegate();
  if (!pref) {
    LOG(ERROR) << "[AIPageCmd] ResolveZoomPref: pref is nullptr";
    return false;
  }
  out_zoom_control_access = pref->IsZoomControlAccess();
  return true;
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
void AIPageCommandDispatcher::HandleScrollCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    LOG(ERROR) << "[AIPageCmd] scroll: invalid params JSON";
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "invalid params JSON");
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

  AIPageCommandParamUtils::ElementTarget target =
      AIPageCommandParamUtils::ResolveScrollTarget(*params);
  if (target.type == AIPageCommandParamUtils::ElementTargetType::kConflict) {
    ReplyError(callback, WebCommandErrorCode::PARAM_INVALID,
               "id is mutually exclusive with x/y");
    return;
  }
  if (target.type == AIPageCommandParamUtils::ElementTargetType::kInvalid) {
    ReplyError(callback, WebCommandErrorCode::PARAM_INVALID, "invalid id");
    return;
  }
  if (target.type == AIPageCommandParamUtils::ElementTargetType::kMissing) {
    LOG(ERROR) << "[AIPageCmd] scroll: missing id or x/y";
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING,
               "missing id or x/y");
    return;
  }

  if (target.type == AIPageCommandParamUtils::ElementTargetType::kId) {
#if !BUILDFLAG(ARKWEB_INPUT_EVENTS)
    ResolveEmptyResult(callback);
    return;
#else
    if (!browser || !browser->GetHost()) {
      LOG(ERROR) << "[AIPageCmd] scroll: browser or host is null";
      ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
                 "browser or host is null");
      return;
    }
    CefRefPtr<AIPageScrollRectCallbackImpl> rect_callback =
        new AIPageScrollRectCallbackImpl(browser, params->Clone(), callback);
    browser->GetHost()->GetAIPageElementRects({target.value}, 1,
                                              rect_callback);
    return;
#endif
  }

  auto x = params->FindDouble("x");
  auto y = params->FindDouble("y");
  if (!x.has_value() || !y.has_value()) {
    LOG(ERROR) << "[AIPageCmd] scroll: missing x or y";
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING, "missing x or y");
    return;
  }
  ExecuteScrollGestureAndReply(browser, *params, x.value(), y.value(), callback);
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
  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    LOG(ERROR) << "[AIPageCmd] select: invalid params JSON";
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "invalid params JSON");
    return;
  }

  AIPageCommandParamUtils::ElementTarget target =
      AIPageCommandParamUtils::ResolveIdOrXPathTarget(*params);
  if (target.type == AIPageCommandParamUtils::ElementTargetType::kConflict) {
    ReplyError(callback, WebCommandErrorCode::PARAM_INVALID,
               "id and xpath are mutually exclusive");
    return;
  }
  if (target.type == AIPageCommandParamUtils::ElementTargetType::kMissing) {
    LOG(ERROR) << "[AIPageCmd] select: missing xpath";
    ReplyError(callback, WebCommandErrorCode::JSON_INVALID_XPATH,
               "missing or empty xpath");
    return;
  }
  if (target.type == AIPageCommandParamUtils::ElementTargetType::kInvalid) {
    ReplyError(callback, WebCommandErrorCode::PARAM_INVALID,
               "invalid id or xpath");
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

  if (!browser || !browser->GetHost()) {
    LOG(ERROR) << "[AIPageCmd] select: browser or host is null";
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }

  if (target.type == AIPageCommandParamUtils::ElementTargetType::kId) {
    ExecuteSelectByIdAndReply(browser, target.value, indexes, values, callback);
    return;
  }
  ExecuteSelectAndReply(browser, target.value, indexes, values, callback);
#endif
}

void AIPageCommandDispatcher::HandleDispatchMouseEventCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "invalid params JSON");
    return;
  }
  AIPageCommandParamUtils::CdpParamsResult cdp_result =
      AIPageCommandParamUtils::BuildMouseEventParams(*params);
  if (!cdp_result.ok()) {
    ReplyError(callback, cdp_result.code, cdp_result.message);
    return;
  }
  if (!browser || !browser->GetHost()) {
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }
  int result = browser->GetHost()->ExecuteDevToolsMethod(
      0, "Input.dispatchMouseEvent", cdp_result.params);
  if (result == 0) {
    ReplyError(callback, WebCommandErrorCode::CDP_SEND_FAILED,
               "SendDevToolsMessage failed");
    return;
  }
  ReplyError(callback, WebCommandErrorCode::SUCCESS, "success");
}

void AIPageCommandDispatcher::HandleDispatchKeyEventCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "invalid params JSON");
    return;
  }
  AIPageCommandParamUtils::CdpParamsResult cdp_result =
      AIPageCommandParamUtils::BuildKeyEventParams(*params);
  if (!cdp_result.ok()) {
    ReplyError(callback, cdp_result.code, cdp_result.message);
    return;
  }
  if (!browser || !browser->GetHost()) {
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }
  int result = browser->GetHost()->ExecuteDevToolsMethod(
      0, "Input.dispatchKeyEvent", cdp_result.params);
  if (result == 0) {
    ReplyError(callback, WebCommandErrorCode::CDP_SEND_FAILED,
               "SendDevToolsMessage failed");
    return;
  }
  ReplyError(callback, WebCommandErrorCode::SUCCESS, "success");
}

void AIPageCommandDispatcher::HandleInputCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
#if !BUILDFLAG(ARKWEB_INPUT_EVENTS)
  ResolveEmptyResult(callback);
  return;
#else
  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "invalid params JSON");
    return;
  }

  AIPageCommandParamUtils::ElementTarget target =
      AIPageCommandParamUtils::ResolveIdOrXPathTarget(*params);
  if (target.type == AIPageCommandParamUtils::ElementTargetType::kConflict) {
    ReplyError(callback, WebCommandErrorCode::PARAM_INVALID,
               "id and xpath are mutually exclusive");
    return;
  }
  if (target.type == AIPageCommandParamUtils::ElementTargetType::kMissing) {
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING,
               "id or xpath is required");
    return;
  }
  if (target.type == AIPageCommandParamUtils::ElementTargetType::kInvalid) {
    ReplyError(callback, WebCommandErrorCode::PARAM_INVALID,
               "invalid id or xpath");
    return;
  }

  const base::Value* input_type_value = params->Find("type");
  if (!input_type_value) {
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING, "missing type");
    return;
  }
  if (!input_type_value->is_string()) {
    ReplyError(callback, WebCommandErrorCode::PARAM_INVALID,
               "invalid param: type");
    return;
  }
  const std::string& input_type = input_type_value->GetString();
  if (input_type.empty()) {
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING, "missing type");
    return;
  }
  if (!AIPageCommandParamUtils::IsSupportedInputType(input_type)) {
    ReplyError(callback, WebCommandErrorCode::INPUT_TYPE_INVALID,
               "unsupported input type");
    return;
  }
  const base::Value* value = params->Find("value");
  if (!value) {
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING, "missing value");
    return;
  }
  if (!value->is_string()) {
    ReplyError(callback, WebCommandErrorCode::JSON_INVALID_INPUT_VALUE,
               "invalid input value");
    return;
  }

  if (!browser || !browser->GetHost()) {
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }

  if (target.type == AIPageCommandParamUtils::ElementTargetType::kId) {
    ExecuteAIPageInputByIdAndReply(browser, target.value, value->GetString(),
                                   input_type, callback);
    return;
  }
  ExecuteAIPageInputByXPathAndReply(browser, target.value, value->GetString(),
                                    input_type, callback);
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

void AIPageCommandDispatcher::ExecuteSelectByIdAndReply(
    CefRefPtr<ArkWebBrowserExt> browser,
    const std::string& id,
    const std::vector<int32_t>& indexes,
    const std::vector<std::string>& values,
    ResultCallback callback) {
  int32_t result =
      browser->GetHost()->ExecuteSelectCommandById(id, indexes, values);
  ResolveCommandResult(
      callback, static_cast<WebCommandErrorCode>(result),
      result == static_cast<int32_t>(WebCommandErrorCode::SUCCESS)
          ? "success"
          : "select command failed");
}

void AIPageCommandDispatcher::ExecuteAIPageInputByXPathAndReply(
    CefRefPtr<ArkWebBrowserExt> browser,
    const std::string& xpath,
    const std::string& value,
    const std::string& input_type,
    ResultCallback callback) {
  int32_t result = browser->GetHost()->ExecuteAIPageInputCommandByXPath(
      xpath, value, input_type);
  ResolveCommandResult(
      callback, static_cast<WebCommandErrorCode>(result),
      result == static_cast<int32_t>(WebCommandErrorCode::SUCCESS)
          ? "success"
          : "input command failed");
}

void AIPageCommandDispatcher::ExecuteAIPageInputByIdAndReply(
    CefRefPtr<ArkWebBrowserExt> browser,
    const std::string& id,
    const std::string& value,
    const std::string& input_type,
    ResultCallback callback) {
  int32_t result =
      browser->GetHost()->ExecuteAIPageInputCommandById(id, value, input_type);
  ResolveCommandResult(
      callback, static_cast<WebCommandErrorCode>(result),
      result == static_cast<int32_t>(WebCommandErrorCode::SUCCESS)
          ? "success"
          : "input command failed");
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
  const base::Value::Dict *clip = nullptr;

  if (params) {
    xpath = params->FindString("xpath");
    const std::string *node_id_str = params->FindString("nodeid");
    clip = params->FindDict("clip");

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
  } else if (clip) {
    LOG(INFO) << "[AIPageCmd] screenCapture: capture by area";
    observer->CapturePageByArea(*clip);
  } else {
    LOG(INFO) << "[AIPageCmd] screenCapture: full page capture";
    observer->CaptureFullPage();
  }
}

void AIPageCommandDispatcher::HandleAddPageAnnotationCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "params is invalid");
    return;
  }

  const base::Value* element_list_value = params->Find("elementList");
  if (!element_list_value) {
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING,
               "missing elementList");
    return;
  }

  if (!element_list_value->is_list() ||
      !IsValidAnnotationElementList(element_list_value->GetList())) {
    ReplyError(callback, WebCommandErrorCode::PARAM_INVALID,
               "invalid elementList");
    return;
  }

  if (!browser) {
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or frame is null");
    return;
  }
  auto main_frame = browser->GetMainFrame();
  if (!main_frame) {
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or frame is null");
    return;
  }
  auto arkweb_frame = main_frame->AsArkWebFrame();
  if (!arkweb_frame) {
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or frame is null");
    return;
  }

  CefRefPtr<AIPageAnnotationCommandCallbackImpl> cefCallback =
      new AIPageAnnotationCommandCallbackImpl(callback, true);
  arkweb_frame->AddPageAnnotation(BuildParamsJson(command), cefCallback);
}

void AIPageCommandDispatcher::HandleRemovePageAnnotationCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict&,
    ResultCallback callback) {
  if (!browser) {
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or frame is null");
    return;
  }
  auto main_frame = browser->GetMainFrame();
  if (!main_frame) {
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or frame is null");
    return;
  }
  auto arkweb_frame = main_frame->AsArkWebFrame();
  if (!arkweb_frame) {
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or frame is null");
    return;
  }

  CefRefPtr<AIPageAnnotationCommandCallbackImpl> cefCallback =
      new AIPageAnnotationCommandCallbackImpl(callback, false);
  arkweb_frame->RemovePageAnnotation(cefCallback);
}

void AIPageCommandDispatcher::HandleGetUrlHistoryCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict&,
    ResultCallback callback) {
  if (browser == nullptr || browser->GetHost() == nullptr) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::WEB_NWEB_NULL,
                              "browser host is not available");
    return;
  }

  CefRefPtr<AIPageNavigationEntryVisitor> visitor =
      new AIPageNavigationEntryVisitor();
  browser->GetHost()->GetNavigationEntries(visitor, false);
  ResolveNavigationSuccess(callback, visitor->TakeResult(browser));
}

void AIPageCommandDispatcher::HandleGoBackCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict&,
    ResultCallback callback) {
  if (browser == nullptr) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::WEB_NWEB_NULL,
                              "browser is not available");
    return;
  }
  if (!browser->CanGoBack()) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::FAILED,
                              "Cannot go back");
    return;
  }

  browser->GoBack();
  ResolveNavigationSuccess(callback);
}

void AIPageCommandDispatcher::HandleGoForwardCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict&,
    ResultCallback callback) {
  if (browser == nullptr) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::WEB_NWEB_NULL,
                              "browser is not available");
    return;
  }
  if (!browser->CanGoForward()) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::FAILED,
                              "Cannot go forward");
    return;
  }

  browser->GoForward();
  ResolveNavigationSuccess(callback);
}

void AIPageCommandDispatcher::HandleNavigateCommand(
    CefRefPtr<ArkWebBrowserExt> browser,
    const base::Value::Dict& command,
    ResultCallback callback) {
  const base::Value::Dict* params = command.FindDict("params");
  const base::Value* url_value = params ? params->Find("url") : nullptr;
  if (!url_value) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::PARAM_MISSING,
                             "params.url is required");
    return;
  }
  if (!url_value->is_string()) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::PARAM_INVALID,
                             "params.url is invalid");
    return;
  }
  const std::string& url = url_value->GetString();
  if (url.empty()) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::PARAM_MISSING,
                              "params.url is required");
    return;
  }
  if (!IsAllowedNavigationUrl(url)) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::PARAM_INVALID,
                              "params.url is invalid");
    return;
  }

  if (browser == nullptr) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::WEB_NWEB_NULL,
                              "browser is not available");
    return;
  }
  if (browser->GetMainFrame() == nullptr) {
    ResolveNavigationFailure(callback, WebCommandErrorCode::PAGE_NOT_READY,
                              "main frame is not available");
    return;
  }

  browser->GetMainFrame()->LoadURL(CefString(url));
  ResolveNavigationSuccess(callback);
}

// --- Zoom level (setZoomLevel / getZoomLevel) ---

bool AIPageCommandDispatcher::ValidateZoomLevelParam(
    const base::Value::Dict& params, double& out_zoom_factor,
    ResultCallback callback) {
  const base::Value* zoom_value = params.Find("zoomLevel");
  if (!zoom_value || (!zoom_value->is_double() && !zoom_value->is_int())) {
    LOG(ERROR) << "[AIPageCmd] setZoomLevel: missing or non-numeric zoomLevel";
    ReplyError(callback, WebCommandErrorCode::PARAM_MISSING,
               "param missing: zoomLevel");
    return false;
  }

  out_zoom_factor = zoom_value->is_int()
                        ? static_cast<double>(zoom_value->GetInt())
                        : zoom_value->GetDouble();
  // 0.0 must be rejected here as INVALID, not as OUT_OF_RANGE.
  if (std::isnan(out_zoom_factor) || out_zoom_factor <= 0.0) {
    LOG(ERROR) << "[AIPageCmd] setZoomLevel: invalid zoomLevel value";
    ReplyError(callback, WebCommandErrorCode::ZOOM_LEVEL_INVALID,
               "zoom level invalid");
    return false;
  }

  if (out_zoom_factor < kZoomFactorMin || out_zoom_factor > kZoomFactorMax) {
    LOG(ERROR) << "[AIPageCmd] setZoomLevel: zoomLevel out of range: "
               << out_zoom_factor;
    ReplyError(callback, WebCommandErrorCode::ZOOM_LEVEL_OUT_OF_RANGE,
               "zoom level out of range");
    return false;
  }

  return true;
}

bool AIPageCommandDispatcher::CheckZoomPreconditions(
    ResultCallback callback) {
  bool zoom_control_access = true;
  ResolveZoomPref(zoom_control_access);

  if (!zoom_control_access) {
    LOG(ERROR) << "[AIPageCmd] setZoomLevel: zoom control disabled";
    ReplyError(callback, WebCommandErrorCode::ZOOM_DISABLED,
               "zoom control is disabled");
    return false;
  }

  return true;
}

void AIPageCommandDispatcher::HandleSetZoomLevelCommand(
    CefRefPtr<ArkWebBrowserExt> browser, const base::Value::Dict& command,
    ResultCallback callback) {
  if (!browser || !browser->GetHost()) {
    LOG(ERROR) << "[AIPageCmd] setZoomLevel: browser or host is null";
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }

  const base::Value::Dict* params = command.FindDict("params");
  if (!params) {
    LOG(ERROR) << "[AIPageCmd] setZoomLevel: invalid params JSON";
    ReplyError(callback, WebCommandErrorCode::JSON_IS_INVALID,
               "invalid params JSON");
    return;
  }

  double zoom_factor = 0.0;
  if (!ValidateZoomLevelParam(*params, zoom_factor, callback)) {
    return;
  }

  if (!CheckZoomPreconditions(callback)) {
    return;
  }

  double cef_zoom_level = blink::ZoomFactorToZoomLevel(zoom_factor);
  LOG(INFO) << "[AIPageCmd] setZoomLevel: factor=" << zoom_factor
            << " level=" << cef_zoom_level;
  browser->GetHost()->SetZoomLevel(cef_zoom_level);

  if (callback) {
    callback->OnReceiveValue(BuildResultJson(
        static_cast<int32_t>(WebCommandErrorCode::SUCCESS), "success"));
  }
}

void AIPageCommandDispatcher::ReplyZoomLevel(ResultCallback callback,
                                             double zoom_factor) {
  if (!callback) {
    return;
  }
  base::Value::Dict result;
  result.Set("code", static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  result.Set("message", "success");
  result.Set("zoomLevel", zoom_factor);
  std::string json;
  base::JSONWriter::Write(result, &json);
  callback->OnReceiveValue(json);
}

void AIPageCommandDispatcher::HandleGetZoomLevelCommand(
    CefRefPtr<ArkWebBrowserExt> browser, const base::Value::Dict& command,
    ResultCallback callback) {
  if (!browser || !browser->GetHost()) {
    LOG(ERROR) << "[AIPageCmd] getZoomLevel: browser or host is null";
    ReplyError(callback, WebCommandErrorCode::WEB_NWEB_NULL,
               "browser or host is null");
    return;
  }

  double cef_zoom_level = browser->GetHost()->GetZoomLevel();
  double zoom_factor = blink::ZoomLevelToZoomFactor(cef_zoom_level);
  LOG(INFO) << "[AIPageCmd] getZoomLevel: level=" << cef_zoom_level
            << " factor=" << zoom_factor;

  ReplyZoomLevel(callback, zoom_factor);
}

}  // namespace OHOS::NWeb