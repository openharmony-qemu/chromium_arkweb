/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "ndk_version.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api.h"

using ::testing::MockFunction;
using ::testing::Return;

namespace {

constexpr int TEST_API_VERSION_SMALL = 5;
constexpr int TEST_API_VERSION_LARGE = 10;
constexpr int TEST_API_VERSION_0 = 0;
constexpr int TEST_API_VERSION_NEGATIVE = -1;

struct GetParamApiVersionCase {
  int sdk_api_version;
  int expected_api_version;
};

struct CheckTargetApiVersionCase {
  int sdk_api_version;
  int target_api_version;
  bool expected_result;
};

}  // namespace

class NdkVersionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MockNdkApi::g_mock_OH_GetSdkApiVersion = [this]() {
      return mock_oh_get_sdk_api_version_.Call();
    };
  }

  void TearDown() override {
    MockNdkApi::g_mock_OH_GetSdkApiVersion = nullptr;
  }

  MockFunction<int(void)> mock_oh_get_sdk_api_version_;
};

class GetParamApiVersionTest
    : public NdkVersionTest,
      public ::testing::WithParamInterface<GetParamApiVersionCase> {};

TEST_P(GetParamApiVersionTest, ReturnsExpectedValueForEachSdkApiVersionBranch) {
  const auto& test_case = GetParam();
  EXPECT_CALL(mock_oh_get_sdk_api_version_, Call())
      .WillOnce(Return(test_case.sdk_api_version));

  EXPECT_EQ(test_case.expected_api_version, GetParamApiVersion());
}

INSTANTIATE_TEST_SUITE_P(
    NdkVersionBranches,
    GetParamApiVersionTest,
    ::testing::Values(
        GetParamApiVersionCase{TEST_API_VERSION_SMALL, TEST_API_VERSION_SMALL},
        GetParamApiVersionCase{TEST_API_VERSION_0, TEST_API_VERSION_0},
        GetParamApiVersionCase{TEST_API_VERSION_NEGATIVE, TEST_API_VERSION_0}));

class CheckTargetApiVersionTest
    : public NdkVersionTest,
      public ::testing::WithParamInterface<CheckTargetApiVersionCase> {};

TEST_P(CheckTargetApiVersionTest, CoversAllTargetComparisonBranches) {
  const auto& test_case = GetParam();
  EXPECT_CALL(mock_oh_get_sdk_api_version_, Call())
      .WillOnce(Return(test_case.sdk_api_version));

  EXPECT_EQ(test_case.expected_result,
            CheckTargetApiVersion(test_case.target_api_version));
}

INSTANTIATE_TEST_SUITE_P(
    NdkVersionBranches,
    CheckTargetApiVersionTest,
    ::testing::Values(
        CheckTargetApiVersionCase{TEST_API_VERSION_SMALL,
                                  TEST_API_VERSION_LARGE,
                                  true},
        CheckTargetApiVersionCase{TEST_API_VERSION_SMALL,
                                  TEST_API_VERSION_SMALL,
                                  false},
        CheckTargetApiVersionCase{TEST_API_VERSION_LARGE,
                                  TEST_API_VERSION_SMALL,
                                  false},
        CheckTargetApiVersionCase{TEST_API_VERSION_NEGATIVE,
                                  TEST_API_VERSION_SMALL,
                                  true}));
