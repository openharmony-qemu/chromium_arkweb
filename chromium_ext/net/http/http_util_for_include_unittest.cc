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

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "arkweb/build/features/features.h"
#include "net/http/http_util.h"

#if BUILDFLAG(ARKWEB_DFX_LOGGING)

namespace net {

// ---------------------------------------------------------------------------
// Unit tests for http_util_for_include.cc - the ArkWeb DFX header-value
// validation helpers injected into net/http/http_util.cc.
//
// The abort paths (EnsureValidHeaderValue/EnsureValidHeaderKey on invalid
// input) call the [[noreturn]] SetFatalMessageAndAbort, so they cannot be
// exercised without killing the test process; the valid-input happy paths are
// covered here.
//
// Branch coverage:
//   IsValidHeaderValue(value, invalid_char):
//     [A] valid value                        -> true
//     [B] NUL character                      -> false, writes '\0'
//     [C] CR character                       -> false, writes '\r'
//     [D] LF character                       -> false, writes '\n'
//     [E] invalid char with null out-param   -> false, no write
//     [F] value with a later valid prefix then NUL -> false
//   EnsureValidHeaderValue (valid input):    [G] does not abort
//   EnsureValidHeaderKey (valid key):        [H] does not abort
// ---------------------------------------------------------------------------

TEST(HttpUtilForIncludeTest, IsValidHeaderValue_Valid_ReturnsTrue) {
  char invalid_char = 0;
  EXPECT_TRUE(HttpUtil::IsValidHeaderValue("text/html; charset=utf-8",
                                           &invalid_char));
  EXPECT_EQ(invalid_char, 0);
}

TEST(HttpUtilForIncludeTest, IsValidHeaderValue_Nul_WritesInvalidChar) {
  std::string value("ab\0cd", 5);
  char invalid_char = 0;
  EXPECT_FALSE(HttpUtil::IsValidHeaderValue(value, &invalid_char));
  EXPECT_EQ(invalid_char, '\0');
}

TEST(HttpUtilForIncludeTest, IsValidHeaderValue_Cr_WritesInvalidChar) {
  std::string value("line\rbreak");
  char invalid_char = 0;
  EXPECT_FALSE(HttpUtil::IsValidHeaderValue(value, &invalid_char));
  EXPECT_EQ(invalid_char, '\r');
}

TEST(HttpUtilForIncludeTest, IsValidHeaderValue_Lf_WritesInvalidChar) {
  std::string value("line\nbreak");
  char invalid_char = 0;
  EXPECT_FALSE(HttpUtil::IsValidHeaderValue(value, &invalid_char));
  EXPECT_EQ(invalid_char, '\n');
}

TEST(HttpUtilForIncludeTest, IsValidHeaderValue_InvalidChar_NullOutParam) {
  std::string value("bad\nvalue");
  // With a null out-param the invalid char is not written but the value is
  // still rejected.
  EXPECT_FALSE(HttpUtil::IsValidHeaderValue(value, nullptr));
}

TEST(HttpUtilForIncludeTest, IsValidHeaderValue_ValidPrefixThenNul_ReturnsFalse) {
  std::string value("ok-prefix\0suffix", 13);
  char invalid_char = 0;
  EXPECT_FALSE(HttpUtil::IsValidHeaderValue(value, &invalid_char));
  EXPECT_EQ(invalid_char, '\0');
}

TEST(HttpUtilForIncludeTest, EnsureValidHeaderValue_ValidInput_NoAbort) {
  EXPECT_NO_FATAL_FAILURE(HttpUtil::EnsureValidHeaderValue(
      "application/json", "SetHeader", std::string_view("X-Custom")));
}

TEST(HttpUtilForIncludeTest, EnsureValidHeaderKey_ValidKey_NoAbort) {
  EXPECT_NO_FATAL_FAILURE(
      HttpUtil::EnsureValidHeaderKey("X-Custom-Key", "SetHeader"));
}

}  // namespace net

#endif  // BUILDFLAG(ARKWEB_DFX_LOGGING)
