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

#include "ai_page_command_param_utils.h"

#include <array>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"

namespace OHOS::NWeb::AIPageCommandParamUtils {
namespace {

constexpr std::array<std::string_view, 4> kMouseEventTypes = {
    "mousePressed", "mouseReleased", "mouseMoved", "mouseWheel"};
constexpr std::array<std::string_view, 6> kMouseButtons = {
    "none", "left", "middle", "right", "back", "forward"};
constexpr std::array<std::string_view, 4> kKeyEventTypes = {
    "keyDown", "keyUp", "rawKeyDown", "char"};
constexpr std::array<std::string_view, 14> kSupportedInputTypes = {
    "default", "text", "password", "email", "url", "tel", "search",
    "number", "date", "datetime-local", "month", "range", "time",
    "week"};

CdpParamsResult MakeError(WebCommandErrorCode code, std::string message) {
  CdpParamsResult result;
  result.code = code;
  result.message = std::move(message);
  return result;
}

bool HasField(const base::Value::Dict& params, const char* key) {
  return params.Find(key) != nullptr;
}

bool IsPresentString(const base::Value::Dict& params,
                     const char* key,
                     std::string* value) {
  const base::Value* raw = params.Find(key);
  if (!raw || !raw->is_string()) {
    return false;
  }
  *value = raw->GetString();
  return true;
}

bool CopyStringField(const base::Value::Dict& params,
                     const char* key,
                     CefRefPtr<CefDictionaryValue> cdp,
                     std::string* error) {
  const base::Value* value = params.Find(key);
  if (!value) {
    return true;
  }
  if (!value->is_string()) {
    *error = std::string("invalid param: ") + key;
    return false;
  }
  cdp->SetString(key, value->GetString());
  return true;
}

bool CopyNumberField(const base::Value::Dict& params,
                     const char* key,
                     CefRefPtr<CefDictionaryValue> cdp,
                     std::string* error) {
  const base::Value* value = params.Find(key);
  if (!value) {
    return true;
  }
  if (!value->is_int() && !value->is_double()) {
    *error = std::string("invalid param: ") + key;
    return false;
  }
  cdp->SetDouble(key, value->is_int() ? value->GetInt() : value->GetDouble());
  return true;
}

bool CopyIntField(const base::Value::Dict& params,
                  const char* key,
                  CefRefPtr<CefDictionaryValue> cdp,
                  std::string* error) {
  const base::Value* value = params.Find(key);
  if (!value) {
    return true;
  }
  if (!value->is_int()) {
    *error = std::string("invalid param: ") + key;
    return false;
  }
  cdp->SetInt(key, value->GetInt());
  return true;
}

bool CopyBoolField(const base::Value::Dict& params,
                   const char* key,
                   CefRefPtr<CefDictionaryValue> cdp,
                   std::string* error) {
  const base::Value* value = params.Find(key);
  if (!value) {
    return true;
  }
  if (!value->is_bool()) {
    *error = std::string("invalid param: ") + key;
    return false;
  }
  cdp->SetBool(key, value->GetBool());
  return true;
}

bool CopyStringListField(const base::Value::Dict& params,
                         const char* key,
                         CefRefPtr<CefDictionaryValue> cdp,
                         std::string* error) {
  const base::Value* value = params.Find(key);
  if (!value) {
    return true;
  }
  if (!value->is_list()) {
    *error = std::string("invalid param: ") + key;
    return false;
  }
  CefRefPtr<CefListValue> list = CefListValue::Create();
  size_t index = 0;
  for (const auto& item : value->GetList()) {
    if (!item.is_string()) {
      *error = std::string("invalid param: ") + key;
      return false;
    }
    list->SetString(index++, item.GetString());
  }
  cdp->SetList(key, list);
  return true;
}

}  // namespace

ElementTarget ResolveIdOrXPathTarget(const base::Value::Dict& params) {
  const bool has_id = HasField(params, "id");
  const bool has_xpath = HasField(params, "xpath");
  if (has_id && has_xpath) {
    return {ElementTargetType::kConflict, std::string()};
  }

  std::string value;
  if (has_id) {
    if (!IsPresentString(params, "id", &value) || value.empty()) {
      return {ElementTargetType::kInvalid, std::string()};
    }
    return {ElementTargetType::kId, std::move(value)};
  }
  if (has_xpath) {
    if (!IsPresentString(params, "xpath", &value) || value.empty()) {
      return {ElementTargetType::kInvalid, std::string()};
    }
    return {ElementTargetType::kXPath, std::move(value)};
  }
  return {ElementTargetType::kMissing, std::string()};
}

ElementTarget ResolveScrollTarget(const base::Value::Dict& params) {
  const bool has_id = HasField(params, "id");
  if (has_id && (HasField(params, "x") || HasField(params, "y"))) {
    return {ElementTargetType::kConflict, std::string()};
  }
  if (has_id) {
    std::string value;
    if (!IsPresentString(params, "id", &value) || value.empty()) {
      return {ElementTargetType::kInvalid, std::string()};
    }
    return {ElementTargetType::kId, std::move(value)};
  }
  if (HasField(params, "x") || HasField(params, "y")) {
    return {ElementTargetType::kXPath, std::string()};
  }
  return {ElementTargetType::kMissing, std::string()};
}

CdpParamsResult BuildMouseEventParams(const base::Value::Dict& params) {
  const std::string* type = params.FindString("type");
  if (!type || type->empty()) {
    return MakeError(WebCommandErrorCode::PARAM_MISSING, "missing type");
  }
  if (!base::Contains(kMouseEventTypes, *type)) {
    return MakeError(WebCommandErrorCode::PARAM_INVALID,
                     "invalid mouse event type");
  }
  auto x = params.FindDouble("x");
  auto y = params.FindDouble("y");
  if (!x.has_value() || !y.has_value()) {
    return MakeError(WebCommandErrorCode::PARAM_MISSING, "missing x or y");
  }

  CefRefPtr<CefDictionaryValue> cdp = CefDictionaryValue::Create();
  cdp->SetString("type", *type);
  cdp->SetDouble("x", x.value());
  cdp->SetDouble("y", y.value());

  std::string error;
  const char* number_fields[] = {"timestamp", "force", "tangentialPressure",
                                 "tiltX", "tiltY", "deltaX", "deltaY"};
  for (const char* key : number_fields) {
    if (!CopyNumberField(params, key, cdp, &error)) {
      return MakeError(WebCommandErrorCode::PARAM_INVALID, error);
    }
  }
  const char* int_fields[] = {"modifiers", "buttons", "clickCount", "twist"};
  for (const char* key : int_fields) {
    if (!CopyIntField(params, key, cdp, &error)) {
      return MakeError(WebCommandErrorCode::PARAM_INVALID, error);
    }
  }
  if (const std::string* button = params.FindString("button")) {
    if (!base::Contains(kMouseButtons, *button)) {
      return MakeError(WebCommandErrorCode::PARAM_INVALID,
                       "invalid param: button");
    }
    cdp->SetString("button", *button);
  } else if (HasField(params, "button")) {
    return MakeError(WebCommandErrorCode::PARAM_INVALID,
                     "invalid param: button");
  }
  if (const std::string* pointer_type = params.FindString("pointerType")) {
    if (*pointer_type != "mouse") {
      return MakeError(WebCommandErrorCode::PARAM_INVALID,
                       "pointerType must be mouse");
    }
  } else if (HasField(params, "pointerType")) {
    return MakeError(WebCommandErrorCode::PARAM_INVALID,
                     "invalid param: pointerType");
  }
  cdp->SetString("pointerType", "mouse");

  CdpParamsResult result;
  result.params = cdp;
  return result;
}

CdpParamsResult BuildKeyEventParams(const base::Value::Dict& params) {
  const std::string* type = params.FindString("type");
  if (!type || type->empty()) {
    return MakeError(WebCommandErrorCode::PARAM_MISSING, "missing type");
  }
  if (!base::Contains(kKeyEventTypes, *type)) {
    return MakeError(WebCommandErrorCode::PARAM_INVALID,
                     "invalid key event type");
  }

  CefRefPtr<CefDictionaryValue> cdp = CefDictionaryValue::Create();
  cdp->SetString("type", *type);
  std::string error;
  const char* string_fields[] = {"text", "unmodifiedText", "key", "code"};
  for (const char* key : string_fields) {
    if (!CopyStringField(params, key, cdp, &error)) {
      return MakeError(WebCommandErrorCode::PARAM_INVALID, error);
    }
  }
  if (!CopyNumberField(params, "timestamp", cdp, &error)) {
    return MakeError(WebCommandErrorCode::PARAM_INVALID, error);
  }
  const char* int_fields[] = {"modifiers", "windowsVirtualKeyCode",
                              "nativeVirtualKeyCode", "location"};
  for (const char* key : int_fields) {
    if (!CopyIntField(params, key, cdp, &error)) {
      return MakeError(WebCommandErrorCode::PARAM_INVALID, error);
    }
  }
  const char* bool_fields[] = {"autoRepeat", "isKeypad", "isSystemKey",
                               "isForwarded"};
  for (const char* key : bool_fields) {
    if (!CopyBoolField(params, key, cdp, &error)) {
      return MakeError(WebCommandErrorCode::PARAM_INVALID, error);
    }
  }
  if (!CopyStringListField(params, "commands", cdp, &error)) {
    return MakeError(WebCommandErrorCode::PARAM_INVALID, error);
  }

  CdpParamsResult result;
  result.params = cdp;
  return result;
}

CefRefPtr<CefDictionaryValue> BuildScrollGestureParams(
    const base::Value::Dict& params,
    double x,
    double y) {
  CefRefPtr<CefDictionaryValue> cdp = CefDictionaryValue::Create();
  cdp->SetDouble("x", x);
  cdp->SetDouble("y", y);
  if (auto value = params.FindDouble("deltaX")) {
    cdp->SetDouble("xDistance", value.value());
  }
  if (auto value = params.FindDouble("deltaY")) {
    cdp->SetDouble("yDistance", value.value());
  }
  if (auto value = params.FindInt("speed")) {
    cdp->SetInt("speed", value.value());
  }
  return cdp;
}

bool IsSupportedInputType(const std::string& type) {
  return base::Contains(kSupportedInputTypes, type);
}

bool IsDefaultInputType(const std::string& type) {
  return type == "default";
}

}  // namespace OHOS::NWeb::AIPageCommandParamUtils