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

#ifndef OHOS_NWEB_SRC_CEF_DELEGATE_FILE_UPLOAD_OBSERVER_H_
#define OHOS_NWEB_SRC_CEF_DELEGATE_FILE_UPLOAD_OBSERVER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cef/include/cef_base.h"
#include "cef/include/cef_devtools_message_observer.h"
#include "cef/include/cef_registration.h"

class ArkWebBrowserExt;

namespace OHOS::NWeb {

class NWebStringValueCallback;

// Resolves xpath to nodeId via chained CDP calls (DOM.performSearch ->
// DOM.getSearchResults), then sends DOM.setFileInputFiles.
class FileUploadObserver : public CefDevToolsMessageObserver {
 public:
  using ResultCallback = std::shared_ptr<NWebStringValueCallback>;

  // XPath mode: resolves xpath to nodeId via CDP chain, then uploads.
  FileUploadObserver(CefRefPtr<ArkWebBrowserExt> browser,
                             const std::string &xpath,
                             const std::vector<std::string> &files,
                             const std::vector<std::string> &warnings,
                             ResultCallback callback);

  // Registers observer and starts the CDP call chain.
  void Start();

 private:
  // CefDevToolsMessageObserver:
  void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser, int message_id,
                              bool success, const void *result,
                              size_t result_size) override;

  void HandleEnableResult(bool success, const void *result, size_t result_size);
  void HandleGetDocResult(bool success, const void *result, size_t result_size);
  void HandleSearchResult(bool success, const void *result, size_t result_size);
  void HandleGetResultsResult(bool success, const void *result,
                              size_t result_size);
  void HandleUploadResult(bool success, const void *result, size_t result_size);

  void StartByXpath();
  void SendGetSearchResults();
  void SendSetFileInputFiles(int node_id);

  void Finish(int32_t code, const std::string &message);

  CefRefPtr<ArkWebBrowserExt> browser_;
  std::string xpath_;
  std::vector<std::string> files_;
  std::vector<std::string> warnings_;
  ResultCallback callback_;
  CefRefPtr<CefRegistration> registration_;

  int enable_msg_id_ = 0;
  int get_doc_msg_id_ = 0;
  int search_msg_id_ = 0;
  int get_results_msg_id_ = 0;
  int upload_msg_id_ = 0;
  std::string search_id_;

  IMPLEMENT_REFCOUNTING(FileUploadObserver);
};

} // namespace OHOS::NWeb

#endif // OHOS_NWEB_SRC_CEF_DELEGATE_FILE_UPLOAD_OBSERVER_H_
