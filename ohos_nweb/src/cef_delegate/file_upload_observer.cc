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

#include "file_upload_observer.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "cef/ohos_cef_ext/include/arkweb_browser_ext.h"
#include "include/cef_values.h"
#include "nweb_errors.h"
#include "nweb_value_callback.h"

namespace OHOS::NWeb {

FileUploadObserver::FileUploadObserver(
    CefRefPtr<ArkWebBrowserExt> browser, const std::string &xpath,
    const std::vector<std::string> &files,
    const std::vector<std::string> &warnings, ResultCallback callback)
    : browser_(browser), xpath_(xpath), files_(files), warnings_(warnings),
      callback_(callback) {}

void FileUploadObserver::Start() {
  registration_ = browser_->GetHost()->AddDevToolsMessageObserver(this);
  if (!registration_) {
    LOG(ERROR) << "[AIPageCmd] failed to add file upload observer";
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_SESSION_NOT_READY),
           "failed to add file upload observer");
    return;
  }

  enable_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.enable", CefDictionaryValue::Create());
  if (enable_msg_id_ == 0) {
    LOG(ERROR) << "[AIPageCmd] failed to send DOM.enable";
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
           "failed to send DOM.enable");
  }
}

void FileUploadObserver::StartByXpath() {
  CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
  params->SetString("query", xpath_);
  search_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.performSearch", params);
  if (search_msg_id_ == 0) {
    LOG(ERROR) << "[AIPageCmd] failed to send DOM.performSearch";
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
           "failed to send DOM.performSearch");
  }
}

void FileUploadObserver::OnDevToolsMethodResult(
    CefRefPtr<CefBrowser> browser, int message_id, bool success,
    const void *result, size_t result_size) {
  if (message_id == 0)
    return;
  if (message_id == enable_msg_id_) {
    HandleEnableResult(success, result, result_size);
  } else if (message_id == get_doc_msg_id_) {
    HandleGetDocResult(success, result, result_size);
  } else if (message_id == search_msg_id_) {
    HandleSearchResult(success, result, result_size);
  } else if (message_id == get_results_msg_id_) {
    HandleGetResultsResult(success, result, result_size);
  } else if (message_id == upload_msg_id_) {
    HandleUploadResult(success, result, result_size);
  }
}

void FileUploadObserver::HandleEnableResult(bool success,
                                                    const void *result,
                                                    size_t result_size) {
  std::string json(static_cast<const char *>(result), result_size);
  if (!success) {
    LOG(ERROR) << "[AIPageCmd] DOM.enable failed: " << json;
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
           "DOM.enable failed");
    return;
  }

  LOG(INFO) << "[AIPageCmd] DOM.enable success, sending DOM.getDocument";

  get_doc_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.getDocument", CefDictionaryValue::Create());
  if (get_doc_msg_id_ == 0) {
    LOG(ERROR) << "[AIPageCmd] failed to send DOM.getDocument";
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
           "failed to send DOM.getDocument");
  }
}

void FileUploadObserver::HandleGetDocResult(bool success,
                                                    const void *result,
                                                    size_t result_size) {
  std::string json(static_cast<const char *>(result), result_size);
  if (!success) {
    LOG(ERROR) << "[AIPageCmd] DOM.getDocument failed";
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
           "DOM.getDocument failed");
    return;
  }

  if (!xpath_.empty()) {
    StartByXpath();
  }
}

void FileUploadObserver::HandleSearchResult(bool success,
                                                    const void *result,
                                                    size_t result_size) {
  std::string json(static_cast<const char *>(result), result_size);
  if (!success) {
    LOG(ERROR) << "[AIPageCmd] DOM.performSearch failed: " << json;
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
           "DOM.performSearch failed");
    return;
  }
  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    LOG(ERROR) << "[AIPageCmd] invalid DOM.performSearch response";
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
           "invalid DOM.performSearch response");
    return;
  }

  const auto &dict = val->GetDict();
  const std::string *search_id = dict.FindString("searchId");
  auto result_count = dict.FindInt("resultCount");

  LOG(INFO) << "[AIPageCmd] DOM.performSearch resultCount="
            << (result_count.has_value() ? result_count.value() : 0);
  if (!search_id || !result_count.has_value() || result_count.value() == 0) {
    LOG(ERROR) << "[AIPageCmd] element not found by xpath";
    Finish(static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND),
           "element not found by xpath");
    return;
  }

  search_id_ = *search_id;
  SendGetSearchResults();
}

void FileUploadObserver::SendGetSearchResults() {
  CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
  params->SetString("searchId", search_id_);
  params->SetInt("fromIndex", 0);
  params->SetInt("toIndex", 1);
  get_results_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.getSearchResults", params);
  if (get_results_msg_id_ == 0) {
    LOG(ERROR) << "[AIPageCmd] failed to send DOM.getSearchResults";
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
           "failed to send DOM.getSearchResults");
  }
}

void FileUploadObserver::HandleGetResultsResult(bool success,
                                                        const void *result,
                                                        size_t result_size) {
  std::string json(static_cast<const char *>(result), result_size);
  if (!success) {
    LOG(ERROR) << "[AIPageCmd] DOM.getSearchResults failed: " << json;
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
           "DOM.getSearchResults failed");
    return;
  }

  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    LOG(ERROR) << "[AIPageCmd] invalid DOM.getSearchResults response";
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR),
           "invalid DOM.getSearchResults response");
    return;
  }

  const auto &dict = val->GetDict();
  const base::Value::List *node_ids = dict.FindList("nodeIds");
  if (!node_ids || node_ids->empty() || !node_ids->front().is_int()) {
    LOG(ERROR) << "[AIPageCmd] no node found for xpath";
    Finish(static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND),
           "no node found for xpath");
    return;
  }

  int node_id = node_ids->front().GetInt();
  LOG(INFO) << "[AIPageCmd] DOM.getSearchResults nodeId=" << node_id;

  CefRefPtr<CefDictionaryValue> discard = CefDictionaryValue::Create();
  discard->SetString("searchId", search_id_);
  browser_->GetHost()->ExecuteDevToolsMethod(0, "DOM.discardSearchResults",
                                             discard);

  SendSetFileInputFiles(node_id);
}

void FileUploadObserver::SendSetFileInputFiles(int node_id) {
  CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
  CefRefPtr<CefListValue> cef_files = CefListValue::Create();
  for (size_t i = 0; i < files_.size(); ++i) {
    cef_files->SetString(i, files_[i]);
  }
  CefRefPtr<CefValue> files_value = CefValue::Create();
  files_value->SetList(cef_files);
  params->SetValue("files", files_value);
  params->SetInt("nodeId", node_id);

  LOG(INFO) << "[AIPageCmd] DOM.setFileInputFiles nodeId=" << node_id;
  upload_msg_id_ = browser_->GetHost()->ExecuteDevToolsMethod(
      0, "DOM.setFileInputFiles", params);
  if (upload_msg_id_ == 0) {
    LOG(ERROR) << "[AIPageCmd] failed to send DOM.setFileInputFiles";
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED),
           "failed to send DOM.setFileInputFiles");
  }
}

void FileUploadObserver::HandleUploadResult(bool success,
                                                    const void *result,
                                                    size_t result_size) {
  std::string json(static_cast<const char *>(result), result_size);
  if (!success) {
    LOG(ERROR) << "[AIPageCmd] DOM.setFileInputFiles failed: " << json;
    Finish(static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED),
           "DOM.setFileInputFiles failed");
    return;
  }
  Finish(static_cast<int32_t>(WebCommandErrorCode::SUCCESS), "success");
}

void FileUploadObserver::Finish(int32_t code,
                                        const std::string &message) {
  CefRefPtr<CefDevToolsMessageObserver> self(this);
  if (!callback_) {
    callback_ = nullptr;
    browser_ = nullptr;
    registration_ = nullptr;
    return;
  }

  std::string final_message = message;
  if (!warnings_.empty()) {
    final_message += "; warnings: ";
    for (size_t i = 0; i < warnings_.size(); ++i) {
      if (i > 0)
        final_message += ", ";
      final_message += warnings_[i];
    }
  }

  base::Value::Dict result;
  result.Set("code", code);
  result.Set("message", final_message);
  std::string json;
  base::JSONWriter::Write(result, &json);
  callback_->OnReceiveValue(json);
  callback_ = nullptr;
  browser_ = nullptr;
  registration_ = nullptr;
}

}  // namespace OHOS::NWeb
