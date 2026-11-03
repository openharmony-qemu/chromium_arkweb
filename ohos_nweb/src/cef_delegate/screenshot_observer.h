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

#ifndef OHOS_NWEB_SRC_CEF_DELEGATE_SCREENSHOT_OBSERVER_H_
#define OHOS_NWEB_SRC_CEF_DELEGATE_SCREENSHOT_OBSERVER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cef/include/cef_base.h"
#include "cef/include/cef_devtools_message_observer.h"
#include "cef/include/cef_registration.h"
#include "include/cef_values.h"
#include "ohos_nweb/include/nweb_value_callback.h"

class ArkWebBrowserExt;

namespace OHOS::NWeb {

struct CaptureResult;

// class NWebStringValueCallback;

// Executes Page.captureScreenshot and extracts base64 image data from response
// Supports both full-page and element-specific capture via CDP chain
class ScreenshotObserver : public CefDevToolsMessageObserver {
 public:
  // using ResultCallback = AIPageCommandDispatcher::ResultCallback;
  using ResultCallback = std::shared_ptr<NWebStringValueCallback>;

  ScreenshotObserver(CefRefPtr<ArkWebBrowserExt> browser,
                     ResultCallback callback);

  // Capture full page (no element lookup needed)
  void CaptureFullPage();

  // Capture specific element by xpath
  // Executes: DOM.enable -> DOM.getDocument -> DOM.performSearch -> DOM.getSearchResults -> DOM.discardSearchResults -> DOM.getBoxModel -> Page.getLayoutMetrics -> ScreenshotCaptureUtil::CaptureWithClip -> DOM.disable
  void CaptureElement(const std::string& xpath);

  // Capture specific element by node ID
  // Executes: DOM.getBoxModel(node_id) -> Page.getLayoutMetrics -> ScreenshotCaptureUtil::CaptureWithClip
  void CaptureElementByNodeId(int node_id);

  // Capture page of a specified area
  void CapturePageByArea(const base::Value::Dict& clip);
 
 private:
  void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser, int message_id,
                              bool success, const void *result,
                              size_t result_size) override;

  void HandleEnableResult(bool success, const void *result, size_t result_size);
  void HandleGetDocumentResult(bool success, const void *result, size_t result_size);
  void HandlePerformSearchResult(bool success, const void *result, size_t result_size);
  void HandleGetSearchResultsResult(bool success, const void *result, size_t result_size);
  void HandleGetBoxModelResult(bool success, const void *result, size_t result_size);
  void HandleLayoutMetricsResult(bool success, const void *result, size_t result_size);
  void HandleScreenshotResult(bool success, const void *result, size_t result_size);
  void OnNativeCaptureComplete(CaptureResult result);

  void DiscardSearchResults();
  void CleanupDomDomain();
  void FinishWithError(int32_t code, const std::string &message);
  void FinishWithSuccess();

  // builds CDP params for Page.captureScreenshot clip.
  CefRefPtr<CefDictionaryValue> BuildAreaParams(const base::Value::Dict& clip);

  std::string screenshot_data_;
  CefRefPtr<ArkWebBrowserExt> browser_;
  ResultCallback callback_;
  CefRefPtr<CefRegistration> registration_;

  std::string xpath_;
  std::string search_id_;
  int node_id_ = 0;
  int resolved_node_id_ = 0;

  int enable_msg_id_ = 0;
  int get_document_msg_id_ = 0;
  int perform_search_msg_id_ = 0;
  int get_search_results_msg_id_ = 0;
  int get_box_model_msg_id_ = 0;
  int layout_metrics_msg_id_ = 0;
  int screenshot_msg_id_ = 0;
  // Temporary storage for element coordinates before layout metrics
  double temp_element_x_ = 0;
  double temp_element_y_ = 0;
  double temp_element_width_ = 0;
  double temp_element_height_ = 0;
  // Scroll offset from Page.getLayoutMetrics
  double scroll_x_ = 0;
  double scroll_y_ = 0;

  IMPLEMENT_REFCOUNTING(ScreenshotObserver);
  base::WeakPtrFactory<ScreenshotObserver> weak_factory_{this};
};

}  // namespace OHOS::NWeb

#endif  // OHOS_NWEB_SRC_CEF_DELEGATE_SCREENSHOT_OBSERVER_H_
