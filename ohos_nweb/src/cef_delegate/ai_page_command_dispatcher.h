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

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cef/include/cef_base.h"

class ArkWebBrowserExt;
class CefDictionaryValue;

namespace OHOS::NWeb {

class NWebDelegate;
class NWebPreferenceDelegate;
class NWebStringValueCallback;

enum class WebCommandErrorCode;

class AIPageCommandDispatcher {
 public:
  using ResultCallback = std::shared_ptr<NWebStringValueCallback>;
  using CommandHandler =
      void (AIPageCommandDispatcher::*)(CefRefPtr<ArkWebBrowserExt> browser,
                                        const base::Value::Dict& command,
                                        ResultCallback callback);

  explicit AIPageCommandDispatcher(base::WeakPtr<NWebDelegate> delegate)
      : delegate_(std::move(delegate)) {}
  void Dispatch(const std::string& command,
                CefRefPtr<ArkWebBrowserExt> browser,
                ResultCallback callback);

  void HandleScreenCaptureCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                  const base::Value::Dict& command,
                                  ResultCallback callback);

 private:
  void ResolveEmptyResult(ResultCallback callback);
  void HandleGetFullDomCommand(CefRefPtr<ArkWebBrowserExt> browser,
                               const base::Value::Dict& command,
                               ResultCallback callback);
  void HandleGetLiteDomCommand(CefRefPtr<ArkWebBrowserExt> browser,
                               const base::Value::Dict& command,
                               ResultCallback callback);
  void HandleScrollCommand(CefRefPtr<ArkWebBrowserExt> browser,
                           const base::Value::Dict& command,
                           ResultCallback callback);
  void HandleSelectCommand(CefRefPtr<ArkWebBrowserExt> browser,
                           const base::Value::Dict& command,
                           ResultCallback callback);
  void HandleUploadFileCommand(CefRefPtr<ArkWebBrowserExt> browser,
                               const base::Value::Dict& command,
                               ResultCallback callback);
  void HandleSetZoomLevelCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                 const base::Value::Dict& command,
                                 ResultCallback callback);
  void HandleGetZoomLevelCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                 const base::Value::Dict& command,
                                 ResultCallback callback);

  bool ResolveZoomPref(bool& out_zoom_control_access);

  // Returns true on success (writes normalized factor to out_zoom_factor);
  // otherwise calls ReplyError and returns false.
  bool ValidateZoomLevelParam(const base::Value::Dict& params,
                              double& out_zoom_factor,
                              ResultCallback callback);

  // Returns true when zoom may proceed; otherwise calls ReplyError and returns
  // false.
  bool CheckZoomPreconditions(ResultCallback callback);

  void ReplyZoomLevel(ResultCallback callback, double zoom_factor);

  // --- Helpers ---
  void ReplyError(ResultCallback callback,
                  WebCommandErrorCode code,
                  const std::string& message);

  // Pure: builds CDP params for Input.synthesizeScrollGesture, returns nullptr
  // if x/y missing
  CefRefPtr<CefDictionaryValue> BuildScrollParams(
      const base::Value::Dict& params);

  // Select / list helpers
  std::vector<int32_t> ParseIntList(const base::Value::Dict& dict,
                                    const std::string& key);
  std::vector<std::string> ParseStringList(const base::Value::Dict& dict,
                                           const std::string& key);
  void ExecuteSelectAndReply(CefRefPtr<ArkWebBrowserExt> browser,
                             const std::string& xpath,
                             const std::vector<int32_t>& indexes,
                             const std::vector<std::string>& values,
                             ResultCallback callback);

  // Upload helpers
  bool ValidateFilePaths(std::vector<std::string>& files,
                         std::vector<std::string>& warnings,
                         ResultCallback callback);
  void DispatchUploadTarget(CefRefPtr<ArkWebBrowserExt> browser,
                            const base::Value::Dict& dict,
                            const std::vector<std::string>& files,
                            const std::vector<std::string>& warnings,
                            ResultCallback callback);

  base::WeakPtr<NWebDelegate> delegate_;
};

}  // namespace OHOS::NWeb

#endif  // OHOS_NWEB_SRC_CEF_DELEGATE_AI_PAGE_COMMAND_DISPATCHER_H_