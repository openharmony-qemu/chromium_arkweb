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

#if BUILDFLAG(ARKWEB_DFX_LOGGING)
#include "base/logging.h"
#endif

namespace {
#if BUILDFLAG(ARKWEB_DFX_LOGGING)
std::string GetEscapeString(char c) {
  switch (c) {
    case '\0':
      return "\\0 (NUL)";
    case '\r':
      return "\\r (CR)";
    case '\n':
      return "\\n (LF)";
    default:
      return std::string(1, c);
  }
}
#endif
}  // namespace

namespace net {
#if BUILDFLAG(ARKWEB_DFX_LOGGING)
bool HttpUtil::IsValidHeaderValue(std::string_view value, char* invalid_char) {
  // Validates header value by disallowing NUL, CR and LF.
  // If invalid_char is not null, stores the first invalid character found.
  // Returns true if the value is valid, false otherwise.
  for (char c : value) {
    if (c == '\0' || c == '\r' || c == '\n') {
      if (invalid_char) {
        *invalid_char = c;
      }
      return false;
    }
  }
  return true;
}

void HttpUtil::EnsureValidHeaderValue(std::string_view value,
                                      std::string_view operation_name,
                                      std::optional<std::string_view> key) {
  char invalid_char = 0;
  if (!IsValidHeaderValue(value, &invalid_char)) {
    std::string msg = base::StrCat(
        {operation_name, ": ",
         key.has_value()
             ? base::StrCat({"Invalid header: ", key.value(), ", value "})
             : "Invalid value, ",
         "contains invalid char: ", GetEscapeString(invalid_char), "."});
    ARKWEB_SET_FATAL_AND_ABORT(msg);
  }
}

void HttpUtil::EnsureValidHeaderKey(std::string_view key,
                                    std::string_view operation_name) {
  if (!IsValidHeaderName(key)) {
    std::string msg =
        base::StrCat({operation_name, ": ", key, ", has invalid key name."});
    ARKWEB_SET_FATAL_AND_ABORT(msg);
  }
}
#endif
}  // namespace net
