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

#include <memory>
#include <string>

#include "base/values.h"
#include "cef/include/cef_base.h"

class ArkWebBrowserExt;

namespace OHOS::NWeb {

class NWebStringValueCallback;

class AIPageCommandDispatcher {
 public:
  using ResultCallback = std::shared_ptr<NWebStringValueCallback>;

  static void Dispatch(const std::string& command,
                       CefRefPtr<ArkWebBrowserExt> browser,
                       ResultCallback callback);

 private:
  static void ResolveEmptyResult(ResultCallback callback);
  static void HandleGetFullDomCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                      const base::Value::Dict& command,
                                      ResultCallback callback);
  static void HandleGetLiteDomCommand(CefRefPtr<ArkWebBrowserExt> browser,
                                      const base::Value::Dict& command,
                                      ResultCallback callback);
};

}  // namespace OHOS::NWeb

#endif  // OHOS_NWEB_SRC_CEF_DELEGATE_AI_PAGE_COMMAND_DISPATCHER_H_