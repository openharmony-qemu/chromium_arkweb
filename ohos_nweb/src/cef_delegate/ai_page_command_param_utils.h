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

#ifndef OHOS_NWEB_SRC_CEF_DELEGATE_AI_PAGE_COMMAND_PARAM_UTILS_H_
#define OHOS_NWEB_SRC_CEF_DELEGATE_AI_PAGE_COMMAND_PARAM_UTILS_H_

#include <string>

#include "base/values.h"
#include "include/cef_values.h"
#include "nweb_errors.h"

namespace OHOS::NWeb::AIPageCommandParamUtils {

enum class ElementTargetType {
  kMissing,
  kId,
  kXPath,
  kConflict,
  kInvalid,
};

struct ElementTarget {
  ElementTargetType type = ElementTargetType::kMissing;
  std::string value;
};

struct CdpParamsResult {
  CefRefPtr<CefDictionaryValue> params;
  WebCommandErrorCode code = WebCommandErrorCode::SUCCESS;
  std::string message;

  bool ok() const { return params.get() != nullptr; }
};

ElementTarget ResolveIdOrXPathTarget(const base::Value::Dict& params);
ElementTarget ResolveScrollTarget(const base::Value::Dict& params);

CdpParamsResult BuildMouseEventParams(const base::Value::Dict& params);
CdpParamsResult BuildKeyEventParams(const base::Value::Dict& params);
CefRefPtr<CefDictionaryValue> BuildScrollGestureParams(
    const base::Value::Dict& params,
    double x,
    double y);

bool IsSupportedInputType(const std::string& type);
bool IsDefaultInputType(const std::string& type);

}  // namespace OHOS::NWeb::AIPageCommandParamUtils

#endif  // OHOS_NWEB_SRC_CEF_DELEGATE_AI_PAGE_COMMAND_PARAM_UTILS_H_