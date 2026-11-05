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

#include <optional>
#include <string>

#include "arkweb/build/features/features.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Tests for the ArkWeb DFX-logging header validation helpers declared in
// net/http/http_util.h (implemented in
// arkweb/chromium_ext/net/http/http_util_for_include.cc, injected into
// net/http/http_util.cc).

class ArkWebHttpUtilTest : public testing::Test {
 public:
  ArkWebHttpUtilTest() = default;
  ~ArkWebHttpUtilTest() override = default;
};

TEST_F(ArkWebHttpUtilTest, IsValidHeaderValueTwoArg_Valid) {
  char invalid_char = 0;
  EXPECT_TRUE(
      HttpUtil::IsValidHeaderValue("text/html; charset=utf-8", &invalid_char));
}

TEST_F(ArkWebHttpUtilTest, IsValidHeaderValueTwoArg_ValidNullCharPtr) {
  EXPECT_TRUE(HttpUtil::IsValidHeaderValue("valid value", nullptr));
}

TEST_F(ArkWebHttpUtilTest, IsValidHeaderValueTwoArg_Nul) {
  std::string value = "foo";
  value.push_back('\0');
  value.append("bar");
  char invalid_char = 0;
  EXPECT_FALSE(HttpUtil::IsValidHeaderValue(value, &invalid_char));
  EXPECT_EQ(invalid_char, '\0');
}

TEST_F(ArkWebHttpUtilTest, IsValidHeaderValueTwoArg_Cr) {
  char invalid_char = 0;
  EXPECT_FALSE(HttpUtil::IsValidHeaderValue("foo\rbar", &invalid_char));
  EXPECT_EQ(invalid_char, '\r');
}

TEST_F(ArkWebHttpUtilTest, IsValidHeaderValueTwoArg_Lf) {
  char invalid_char = 0;
  EXPECT_FALSE(HttpUtil::IsValidHeaderValue("foo\nbar", &invalid_char));
  EXPECT_EQ(invalid_char, '\n');
}

TEST_F(ArkWebHttpUtilTest, IsValidHeaderValueTwoArg_InvalidNullCharPtr) {
  // A null invalid_char must not be dereferenced.
  EXPECT_FALSE(HttpUtil::IsValidHeaderValue("foo\rbar", nullptr));
}

TEST_F(ArkWebHttpUtilTest, EnsureValidHeaderValue_ValidNoKey) {
  // A valid value must not abort.
  EXPECT_NO_FATAL_FAILURE(
      HttpUtil::EnsureValidHeaderValue("Mozilla/5.0", "setUserAgent"));
}

TEST_F(ArkWebHttpUtilTest, EnsureValidHeaderValue_ValidWithKey) {
  EXPECT_NO_FATAL_FAILURE(HttpUtil::EnsureValidHeaderValue(
      "Mozilla/5.0", "setUserAgent", std::string_view("User-Agent")));
}

TEST_F(ArkWebHttpUtilTest, EnsureValidHeaderValue_InvalidNulDeath) {
  std::string value = "foo";
  value.push_back('\0');
  EXPECT_DEATH(HttpUtil::EnsureValidHeaderValue(value, "setUserAgent"), ".*");
}

TEST_F(ArkWebHttpUtilTest, EnsureValidHeaderValue_InvalidNulWithKeyDeath) {
  std::string value = "foo";
  value.push_back('\0');
  EXPECT_DEATH(HttpUtil::EnsureValidHeaderValue(value, "setUserAgent",
                                                std::string_view("User-Agent")),
               ".*");
}

TEST_F(ArkWebHttpUtilTest, EnsureValidHeaderValue_InvalidCrDeath) {
  EXPECT_DEATH(HttpUtil::EnsureValidHeaderValue("foo\rbar", "setUserAgent"),
               ".*");
}

TEST_F(ArkWebHttpUtilTest, EnsureValidHeaderValue_InvalidLfDeath) {
  EXPECT_DEATH(HttpUtil::EnsureValidHeaderValue("foo\nbar", "setUserAgent"),
               ".*");
}

TEST_F(ArkWebHttpUtilTest, EnsureValidHeaderKey_Valid) {
  EXPECT_NO_FATAL_FAILURE(
      HttpUtil::EnsureValidHeaderKey("User-Agent", "setUserAgent"));
}

TEST_F(ArkWebHttpUtilTest, EnsureValidHeaderKey_InvalidDeath) {
  // A space is not a valid header token character.
  EXPECT_DEATH(HttpUtil::EnsureValidHeaderKey("User Agent", "setUserAgent"),
               ".*");
}

}  // namespace net
