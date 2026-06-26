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

#include "screenshot_observer.h"

#include "ai_page_command_dispatcher.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "cef/ohos_cef_ext/include/arkweb_browser_ext.h"
#include "cef/include/cef_browser.h"
#include "include/cef_values.h"
#include "nweb_errors.h"

namespace OHOS::NWeb {

namespace {

std::string BuildResultJson(int32_t code, const std::string& message) {
  base::Value::Dict result;
  result.Set("code", code);
  result.Set("message", message);
  std::string json;
  base::JSONWriter::Write(result, &json);
  return json;
}

}  // namespace

ScreenshotObserver::ScreenshotObserver(
    CefRefPtr<ArkWebBrowserExt> browser,
    ResultCallback callback)
    : browser_(browser), callback_(callback) {}

void ScreenshotObserver::CaptureFullPage() {
  if (!browser_ || !browser_->GetHost()) {
    LOG(ERROR) << "[Screenshot] browser or host is null";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL),
                    "browser or host is null");
    return;
  }

  registration_ = browser_->GetHost()->AddDevToolsMessageObserver(this);
  if (!registration_) {
    LOG(ERROR) << "[Screenshot] failed to add observer";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SESSION_NOT_READY),
                    "failed to add observer");
    return;
  }

  CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
  params->SetString("format", "png");
  screenshot_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "Page.captureScreenshot", params);
  if (screenshot_msg_id_ == 0) {
    LOG(ERROR) << "[Screenshot] failed to send Page.captureScreenshot";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
                    "failed to send Page.captureScreenshot");
  }
}

void ScreenshotObserver::CaptureElement(const std::string& xpath) {
  LOG(DEBUG) << "[Screenshot] CaptureElement started, xpath=" << xpath;
  if (!browser_ || !browser_->GetHost()) {
    LOG(ERROR) << "[Screenshot] browser or host is null";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL),
                    "browser or host is null");
    return;
  }

  xpath_ = xpath;

  registration_ = browser_->GetHost()->AddDevToolsMessageObserver(this);
  if (!registration_) {
    LOG(ERROR) << "[Screenshot] failed to add observer";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SESSION_NOT_READY),
                    "failed to add observer");
    return;
  }

  // Enable DOM domain
  enable_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.enable", CefDictionaryValue::Create());
  if (enable_msg_id_ == 0) {
    LOG(ERROR) << "[Screenshot] failed to send DOM.enable";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
                    "failed to send DOM.enable");
  }
}

void ScreenshotObserver::CaptureElementByNodeId(int node_id) {
  LOG(DEBUG) << "[Screenshot] CaptureElementByNodeId started, node_id=" << node_id;

  if (!browser_ || !browser_->GetHost()) {
    LOG(ERROR) << "[Screenshot] browser or host is null";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL),
                    "browser or host is null");
    return;
  }

  if (node_id <= 0) {
    LOG(ERROR) << "[Screenshot] invalid nodeid";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID),
                    "invalid param: nodeid");
    return;
  }

  node_id_ = node_id;

  registration_ = browser_->GetHost()->AddDevToolsMessageObserver(this);
  if (!registration_) {
    LOG(ERROR) << "[Screenshot] failed to add observer";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SESSION_NOT_READY),
                    "failed to add observer");
    return;
  }

  // BackendNodeId doesn't require DOM.enable, directly get box model
  LOG(DEBUG) << "[Screenshot] sending DOM.getBoxModel with backendNodeId=" << node_id_;

  CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
  params->SetInt("backendNodeId", node_id_);

  get_box_model_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.getBoxModel", params);

  if (get_box_model_msg_id_ == 0) {
    LOG(ERROR) << "[Screenshot] failed to send DOM.getBoxModel";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
                    "failed to send DOM.getBoxModel");
  } else {
    LOG(DEBUG) << "[Screenshot] DOM.getBoxModel sent, msg_id=" << get_box_model_msg_id_;
  }
}

void ScreenshotObserver::OnDevToolsMethodResult(
    CefRefPtr<CefBrowser> browser,
    int message_id,
    bool success,
    const void* result,
    size_t result_size) {
  if (message_id == 0)
    return;

  LOG(DEBUG) << "[Screenshot] OnDevToolsMethodResult message_id=" << message_id
             << " success=" << success;

  if (message_id == enable_msg_id_) {
    HandleEnableResult(success, result, result_size);
  } else if (message_id == get_document_msg_id_) {
    HandleGetDocumentResult(success, result, result_size);
  } else if (message_id == perform_search_msg_id_) {
    HandlePerformSearchResult(success, result, result_size);
  } else if (message_id == get_search_results_msg_id_) {
    HandleGetSearchResultsResult(success, result, result_size);
  } else if (message_id == get_box_model_msg_id_) {
    HandleGetBoxModelResult(success, result, result_size);
  } else if (message_id == layout_metrics_msg_id_) {
    HandleLayoutMetricsResult(success, result, result_size);
  } else if (message_id == screenshot_msg_id_) {
    HandleScreenshotResult(success, result, result_size);
  } else {
    LOG(WARNING) << "[Screenshot] Unknown message_id=" << message_id;
  }
}

void ScreenshotObserver::HandleEnableResult(bool success,
                                            const void* result,
                                            size_t result_size) {
  LOG(DEBUG) << "[Screenshot] HandleEnableResult success=" << success;

  if (!success) {
    std::string json(static_cast<const char*>(result), result_size);
    LOG(ERROR) << "[Screenshot] DOM.enable failed: " << json;
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
                    "DOM.enable failed");
    return;
  }

  if (!xpath_.empty()) {
    // xpath: need to resolve element first
    LOG(DEBUG) << "[Screenshot] DOM.enable success, sending DOM.getDocument";

    // Get document to obtain root node ID
    get_document_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
        0, "DOM.getDocument", CefDictionaryValue::Create());

    if (get_document_msg_id_ == 0) {
      LOG(ERROR) << "[Screenshot] failed to send DOM.getDocument";
      FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
                      "failed to send DOM.getDocument");
    } else {
      LOG(DEBUG) << "[Screenshot] DOM.getDocument sent, msg_id=" << get_document_msg_id_;
    }
  } else {
    LOG(ERROR) << "[Screenshot] no xpath provided";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::PARAM_MISSING),
                    "no xpath provided");
  }
}

void ScreenshotObserver::HandleGetDocumentResult(bool success,
                                                 const void* result,
                                                 size_t result_size) {
  std::string json(static_cast<const char*>(result), result_size);
  LOG(DEBUG) << "[Screenshot] HandleGetDocumentResult success=" << success
             << ", result=" << json;
  if (!success) {
    LOG(ERROR) << "[Screenshot] DOM.getDocument failed: " << json;
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
                    "DOM.getDocument failed");
    return;
  }

  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    LOG(ERROR) << "[Screenshot] invalid DOM.getDocument response";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid DOM.getDocument response");
    return;
  }

  const auto& dict = val->GetDict();
  const base::Value::Dict* root = dict.FindDict("root");
  if (!root) {
    LOG(ERROR) << "[Screenshot] root field not found";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "root field not found");
    return;
  }

  auto node_id = root->FindInt("nodeId");
  if (!node_id.has_value() || node_id.value() == 0) {
    LOG(ERROR) << "[Screenshot] invalid root node ID";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid root node ID");
    return;
  }

  LOG(DEBUG) << "[Screenshot] got document root nodeId=" << node_id.value()
             << ", sending DOM.performSearch";

  CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
  params->SetString("query", xpath_);

  // Step 3: Perform search (supports xpath)
  perform_search_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.performSearch", params);

  if (perform_search_msg_id_ == 0) {
    LOG(ERROR) << "[Screenshot] failed to send DOM.performSearch";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
                    "failed to send DOM.performSearch");
  } else {
    LOG(DEBUG) << "[Screenshot] DOM.performSearch sent, msg_id=" << perform_search_msg_id_;
  }
}

void ScreenshotObserver::HandlePerformSearchResult(bool success,
                                                   const void* result,
                                                   size_t result_size) {
  std::string json(static_cast<const char*>(result), result_size);
  LOG(DEBUG) << "[Screenshot] HandlePerformSearchResult success=" << success
             << ", result=" << json;

  if (!success) {
    LOG(ERROR) << "[Screenshot] DOM.performSearch failed: " << json;
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
                    "DOM.performSearch failed");
    return;
  }

  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    LOG(ERROR) << "[Screenshot] invalid DOM.performSearch response";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid DOM.performSearch response");
    return;
  }

  const auto& dict = val->GetDict();
  const std::string* search_id = dict.FindString("searchId");
  auto result_count = dict.FindInt("resultCount");

  if (!search_id || search_id->empty() || !result_count.has_value() ||
      result_count.value() <= 0) {
    LOG(ERROR) << "[Screenshot] element not found: " << xpath_;
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
                    "element not found");
    return;
  }

  search_id_ = *search_id;
  LOG(DEBUG) << "[Screenshot] performSearch found " << result_count.value()
             << " searchId=" << search_id_;

  CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
  params->SetString("searchId", search_id_);
  params->SetInt("fromIndex", 0);
  params->SetInt("toIndex", 1);

  LOG(DEBUG) << "[Screenshot] sending DOM.getSearchResults";

  get_search_results_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.getSearchResults", params);

  if (get_search_results_msg_id_ == 0) {
    LOG(ERROR) << "[Screenshot] failed to send DOM.getSearchResults";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
                    "failed to send DOM.getSearchResults");
  } else {
    LOG(DEBUG) << "[Screenshot] DOM.getSearchResults sent, msg_id="
               << get_search_results_msg_id_;
  }
}

void ScreenshotObserver::HandleGetSearchResultsResult(bool success,
                                                      const void* result,
                                                      size_t result_size) {
  std::string json(static_cast<const char*>(result), result_size);
  LOG(DEBUG) << "[Screenshot] HandleGetSearchResultsResult success=" << success
             << ", result=" << json;

  if (!success) {
    LOG(ERROR) << "[Screenshot] DOM.getSearchResults failed: " << json;
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
                    "DOM.getSearchResults failed");
    return;
  }

  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    LOG(ERROR) << "[Screenshot] invalid DOM.getSearchResults response";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid DOM.getSearchResults response");
    return;
  }

  const auto& dict = val->GetDict();
  const base::Value::List* node_ids = dict.FindList("nodeIds");
  if (!node_ids || node_ids->empty()) {
    LOG(ERROR) << "[Screenshot] element not found: " << xpath_;
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
                    "element not found");
    return;
  }

  auto node_id = (*node_ids)[0].GetIfInt();
  if (!node_id.has_value() || node_id.value() <= 0) {
    LOG(ERROR) << "[Screenshot] invalid nodeId from getSearchResults";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid nodeId from getSearchResults");
    return;
  }

  resolved_node_id_ = node_id.value();
  LOG(DEBUG) << "[Screenshot] found element node_id=" << resolved_node_id_;

  DiscardSearchResults();

  // Get box model to get element rect
  CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
  params->SetInt("nodeId", resolved_node_id_);
 
  LOG(DEBUG) << "[Screenshot] sending DOM.getBoxModel with nodeId=" << resolved_node_id_;
  get_box_model_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.getBoxModel", params);
  if (get_box_model_msg_id_ == 0) {
    LOG(ERROR) << "[Screenshot] failed to send DOM.getBoxModel";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
                    "failed to send DOM.getBoxModel");
  } else {
    LOG(DEBUG) << "[Screenshot] DOM.getBoxModel sent, msg_id=" << get_box_model_msg_id_;
  }
}

void ScreenshotObserver::HandleGetBoxModelResult(bool success,
                                                 const void* result,
                                                 size_t result_size) {
  std::string json(static_cast<const char*>(result), result_size);
  LOG(DEBUG) << "[Screenshot] HandleGetBoxModelResult success=" << success
             << ", result=" << json;
  if (!success) {
    LOG(ERROR) << "[Screenshot] DOM.getBoxModel failed: " << json;
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
                    "element not found");
    return;
  }

  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    LOG(ERROR) << "[Screenshot] invalid DOM.getBoxModel response";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid DOM.getBoxModel response");
    return;
  }

  const auto& dict = val->GetDict();
  const base::Value::Dict* model = dict.FindDict("model");
  if (!model) {
    LOG(ERROR) << "[Screenshot] invalid box model data";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid box model data");
    return;
  }

  // Extract quad coordinates: [x1, y1, x2, y2, x3, y3, x4, y4, ...]
  // For rect, we use top-left (x1, y1) and bottom-right (x3, y3)
  const base::Value::List* border = model->FindList("border");
  if (!border || border->size() < 8) {
    LOG(ERROR) << "[Screenshot] invalid border data";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid border data");
    return;
  }

  auto x0 = (*border)[0].GetIfDouble();
  auto y0 = (*border)[1].GetIfDouble();
  auto x2 = (*border)[4].GetIfDouble();
  auto y2 = (*border)[5].GetIfDouble();

  if (!x0 || !y0 || !x2 || !y2) {
    LOG(ERROR) << "[Screenshot] invalid coordinate values";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid coordinate values");
    return;
  }

  // Store element coordinates temporarily (relative to document)
  temp_element_x_ = *x0;
  temp_element_y_ = *y0;
  temp_element_width_ = *x2 - *x0;
  temp_element_height_ = *y2 - *y0;

  LOG(DEBUG) << "[Screenshot] element rect (document coords): x=" << temp_element_x_
            << " y=" << temp_element_y_ << " width=" << temp_element_width_
            << " height=" << temp_element_height_;

  // Get layout metrics to calculate viewport coordinates
  layout_metrics_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "Page.getLayoutMetrics", CefDictionaryValue::Create());

  if (layout_metrics_msg_id_ == 0) {
    LOG(ERROR) << "[Screenshot] failed to send Page.getLayoutMetrics";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
                    "failed to send Page.getLayoutMetrics");
  } else {
    LOG(DEBUG) << "[Screenshot] Page.getLayoutMetrics sent, msg_id=" << layout_metrics_msg_id_;
  }
}

void ScreenshotObserver::HandleLayoutMetricsResult(bool success,
                                                   const void* result,
                                                   size_t result_size) {
  std::string json(static_cast<const char*>(result), result_size);
  LOG(DEBUG) << "[Screenshot] HandleLayoutMetricsResult success=" << success
            << ", result=" << json;

  if (!success) {
    LOG(ERROR) << "[Screenshot] Page.getLayoutMetrics failed: " << json;
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
                    "Page.getLayoutMetrics failed");
    return;
  }

  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    LOG(ERROR) << "[Screenshot] invalid Page.getLayoutMetrics response";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid Page.getLayoutMetrics response");
    return;
  }

  const auto& dict = val->GetDict();

  // Extract scroll offset and viewport size from layoutViewport
  // layoutViewport represents the main scrollable area
  const base::Value::Dict* visual_viewport = dict.FindDict("visualViewport");
  if (!visual_viewport) {
    LOG(ERROR) << "[Screenshot] visualViewport not found in response";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "visualViewport not found");
    return;
  }

  scroll_x_ = visual_viewport->FindDouble("pageX").value_or(0);
  scroll_y_ = visual_viewport->FindDouble("pageY").value_or(0);

  // Convert document coordinates to viewport coordinates
  double viewport_x = temp_element_x_ + scroll_x_;
  double viewport_y = temp_element_y_ + scroll_y_;

  LOG(DEBUG) << "[Screenshot] element rect (viewport coords): x=" << viewport_x
             << " y=" << viewport_y << " width=" << temp_element_width_
             << " height=" << temp_element_height_;

  // Capture screenshot with corrected clip coordinates
  CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
  params->SetString("format", "png");

  CefRefPtr<CefDictionaryValue> clip = CefDictionaryValue::Create();
  clip->SetDouble("x", viewport_x);
  clip->SetDouble("y", viewport_y);
  clip->SetDouble("width", temp_element_width_);
  clip->SetDouble("height", temp_element_height_);
  clip->SetDouble("scale", 1);
  params->SetDictionary("clip", clip);

  LOG(DEBUG) << "[Screenshot] sending Page.captureScreenshot with viewport clip";

  screenshot_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "Page.captureScreenshot", params);

  if (screenshot_msg_id_ == 0) {
    LOG(ERROR) << "[Screenshot] failed to send Page.captureScreenshot";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
                    "failed to send Page.captureScreenshot");
  } else {
    LOG(DEBUG) << "[Screenshot] Page.captureScreenshot sent, msg_id=" << screenshot_msg_id_;
  }
}

void ScreenshotObserver::HandleScreenshotResult(bool success,
                                                const void* result,
                                                size_t result_size) {
  if (!success || !result || result_size == 0) {
    LOG(ERROR) << "[Screenshot] Page.captureScreenshot failed";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
                    "Page.captureScreenshot failed");
    return;
  }

  std::string json(static_cast<const char*>(result), result_size);
  LOG(DEBUG) << "[Screenshot] HandleScreenshotResult success=" << success
             << ", result=" << json;
  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    LOG(ERROR) << "[Screenshot] invalid screenshot response";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "invalid screenshot response");
    return;
  }

  const auto& dict = val->GetDict();
  const std::string* data = dict.FindString("data");
  if (!data) {
    LOG(ERROR) << "[Screenshot] screenshot response missing data field";
    FinishWithError(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
                    "screenshot response missing data field");
    return;
  }

  LOG(DEBUG) << "[Screenshot] capture success, data length=" << data->length();
  screenshot_data_ = *data;

  // Success - will clean up DOM domain in FinishWithSuccess
  FinishWithSuccess();
}

void ScreenshotObserver::DiscardSearchResults() {
  if (!search_id_.empty() && browser_ && browser_->GetHost()) {
    LOG(DEBUG) << "[Screenshot] Sending DOM.discardSearchResults for searchId=" << search_id_;
    CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
    params->SetString("searchId", search_id_);
    int discard_msg_id = browser_->GetHost()->ExecuteDevToolsMethod(
        0, "DOM.discardSearchResults", params);
    if (discard_msg_id == 0) {
      LOG(WARNING) << "[Screenshot] failed to send DOM.discardSearchResults";
    } else {
      LOG(DEBUG) << "[Screenshot] DOM.discardSearchResults sent, msg_id=" << discard_msg_id;
    }
    search_id_.clear();
  }
}

void ScreenshotObserver::CleanupDomDomain() {
  DiscardSearchResults();

  if (!xpath_.empty() && browser_ && browser_->GetHost()) {
    LOG(DEBUG) << "[Screenshot] Sending DOM.disable to clean up DOM domain";
    int disable_msg_id = browser_->GetHost()->ExecuteDevToolsMethod(
        0, "DOM.disable", CefDictionaryValue::Create());

    if (disable_msg_id == 0) {
      LOG(WARNING) << "[Screenshot] failed to send DOM.disable";
    } else {
      LOG(DEBUG) << "[Screenshot] DOM.disable sent, msg_id=" << disable_msg_id;
    }
  }
}

void ScreenshotObserver::FinishWithError(int32_t code,
                                                 const std::string& message) {
  // Clean up DOM domain before finishing
  CleanupDomDomain();

  if (callback_) {
    callback_->OnReceiveValue(BuildResultJson(code, message));
  }
  if (registration_) {
    registration_ = nullptr;
  }
}

void ScreenshotObserver::FinishWithSuccess() {
  // Clean up DOM domain before finishing
  CleanupDomDomain();

  if (callback_) {
    callback_->OnReceiveValue(screenshot_data_);
  }
  if (registration_) {
    registration_ = nullptr;
  }
}

}  // namespace OHOS::NWeb
