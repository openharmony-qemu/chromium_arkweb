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

#ifndef OHOS_NWEB_SRC_CEF_DELEGATE_AI_PAGE_COMMAND_DISPATCHER_H_
#define OHOS_NWEB_SRC_CEF_DELEGATE_AI_PAGE_COMMAND_DISPATCHER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "cef/include/cef_base.h"

class ArkWebBrowserExt;
class CefDictionaryValue;

namespace OHOS::NWeb {

class NWebStringValueCallback;

enum class WebCommandErrorCode;

class AIPageCommandDispatcher {
 public:
  using ResultCallback = std::shared_ptr<NWebStringValueCallback>;

  static void Dispatch(const std::string& command,
                       CefRefPtr<ArkWebBrowserExt> browser,
                       ResultCallback callback);

  static void HandleScreenCaptureCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                  const base::Value::Dict& command,
                                  ResultCallback callback);

 private:
  static void ResolveEmptyResult(ResultCallback callback);
  static void HandleGetFullDomCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                      const base::Value::Dict& command,
                                      ResultCallback callback);
  static void HandleGetLiteDomCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                      const base::Value::Dict& command,
                                      ResultCallback callback);
  static void HandleScrollCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                  const base::Value::Dict& command,
                                  ResultCallback callback);
  static void HandleSelectCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                  const base::Value::Dict& command,
                                  ResultCallback callback);
  static void HandleUploadFileCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                      const base::Value::Dict& command,
                                      ResultCallback callback);
  static void HandleAddPageAnnotationCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                             const base::Value::Dict& command,
                                             ResultCallback callback);
  static void HandleRemovePageAnnotationCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                                const base::Value::Dict& command,
                                                ResultCallback callback);

  // --- Helpers ---
  static void ReplyError(ResultCallback callback,
                         WebCommandErrorCode code,
                         const std::string& message);

  // Pure: builds CDP params for Input.synthesizeScrollGesture, returns nullptr
  // if x/y missing
  static CefRefPtr<CefDictionaryValue> BuildScrollParams(
      const base::Value::Dict& params);

  // Select / list helpers
  static std::vector<int32_t> ParseIntList(const base::Value::Dict& dict,
                                           const std::string& key);
  static std::vector<std::string> ParseStringList(const base::Value::Dict& dict,
                                                  const std::string& key);
  static void ExecuteSelectAndReply(CefRefPtr<ArkWebBrowserExt> browser,
                                    const std::string& xpath,
                                    const std::vector<int32_t>& indexes,
                                    const std::vector<std::string>& values,
                                    ResultCallback callback);

  // Upload helpers
  static bool ValidateFilePaths(std::vector<std::string>& files,
                                std::vector<std::string>& warnings,
                                ResultCallback callback);
  static void DispatchUploadTarget(CefRefPtr<ArkWebBrowserExt> browser,
                                   const base::Value::Dict& dict,
                                   const std::vector<std::string>& files,
                                   const std::vector<std::string>& warnings,
                                   ResultCallback callback);
};

}  // namespace OHOS::NWeb

#endif  // OHOS_NWEB_SRC_CEF_DELEGATE_AI_PAGE_COMMAND_DISPATCHER_H_