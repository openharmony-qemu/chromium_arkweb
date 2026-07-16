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

#include "cef_delegate/ai_page_command_param_utils.h"

#include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace OHOS::NWeb::AIPageCommandParamUtils {
namespace {

TEST(AIPageCommandParamUtilsTest, ResolveIdOrXPathTargetRejectsConflict) {
  base::Value::Dict params;
  params.Set("id", "frame|document|42");
  params.Set("xpath", "//select");

  ElementTarget target = ResolveIdOrXPathTarget(params);

  EXPECT_EQ(ElementTargetType::kConflict, target.type);
}

TEST(AIPageCommandParamUtilsTest, ResolveElementTargetsRejectInvalidIdType) {
  base::Value::Dict params;
  params.Set("id", 42);

  ElementTarget element_target = ResolveIdOrXPathTarget(params);
  ElementTarget scroll_target = ResolveScrollTarget(params);

  EXPECT_EQ(ElementTargetType::kInvalid, element_target.type);
  EXPECT_EQ(ElementTargetType::kInvalid, scroll_target.type);
}

TEST(AIPageCommandParamUtilsTest, ResolveScrollTargetRejectsIdAndCoordinates) {
  base::Value::Dict params;
  params.Set("id", "frame|document|42");
  params.Set("x", 10);

  ElementTarget target = ResolveScrollTarget(params);

  EXPECT_EQ(ElementTargetType::kConflict, target.type);
}

TEST(AIPageCommandParamUtilsTest, BuildMouseEventParamsDefaultsPointerType) {
  base::Value::Dict params;
  params.Set("type", "mouseWheel");
  params.Set("x", 500);
  params.Set("y", 300);
  params.Set("deltaX", 0);
  params.Set("deltaY", 100);
  params.Set("unknown", "ignored");

  CdpParamsResult result = BuildMouseEventParams(params);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(std::string("mouseWheel"), result.params->GetString("type").ToString());
  EXPECT_EQ(std::string("mouse"), result.params->GetString("pointerType").ToString());
  EXPECT_FALSE(result.params->HasKey("unknown"));
}

TEST(AIPageCommandParamUtilsTest, BuildMouseEventParamsRejectsPenPointer) {
  base::Value::Dict params;
  params.Set("type", "mouseMoved");
  params.Set("x", 10);
  params.Set("y", 20);
  params.Set("pointerType", "pen");

  CdpParamsResult result = BuildMouseEventParams(params);

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(WebCommandErrorCode::PARAM_INVALID, result.code);
}

TEST(AIPageCommandParamUtilsTest,
     BuildMouseEventParamsRejectsInvalidRequiredFieldTypes) {
  base::Value::Dict invalid_type_params;
  invalid_type_params.Set("type", 1);
  invalid_type_params.Set("x", 10);
  invalid_type_params.Set("y", 20);

  CdpParamsResult invalid_type_result =
      BuildMouseEventParams(invalid_type_params);

  EXPECT_FALSE(invalid_type_result.ok());
  EXPECT_EQ(WebCommandErrorCode::PARAM_INVALID, invalid_type_result.code);
  EXPECT_EQ("invalid param: type", invalid_type_result.message);

  base::Value::Dict invalid_x_params;
  invalid_x_params.Set("type", "mouseMoved");
  invalid_x_params.Set("x", "bad");
  invalid_x_params.Set("y", 20);

  CdpParamsResult invalid_x_result = BuildMouseEventParams(invalid_x_params);

  EXPECT_FALSE(invalid_x_result.ok());
  EXPECT_EQ(WebCommandErrorCode::PARAM_INVALID, invalid_x_result.code);
  EXPECT_EQ("invalid param: x", invalid_x_result.message);

  base::Value::Dict invalid_y_params;
  invalid_y_params.Set("type", "mouseMoved");
  invalid_y_params.Set("x", 10);
  invalid_y_params.Set("y", "bad");

  CdpParamsResult invalid_y_result = BuildMouseEventParams(invalid_y_params);

  EXPECT_FALSE(invalid_y_result.ok());
  EXPECT_EQ(WebCommandErrorCode::PARAM_INVALID, invalid_y_result.code);
  EXPECT_EQ("invalid param: y", invalid_y_result.message);
}

TEST(AIPageCommandParamUtilsTest, BuildKeyEventParamsRejectsInvalidType) {
  base::Value::Dict params;
  params.Set("type", "press");

  CdpParamsResult result = BuildKeyEventParams(params);

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(WebCommandErrorCode::PARAM_INVALID, result.code);
}

TEST(AIPageCommandParamUtilsTest, BuildKeyEventParamsRejectsInvalidTypeField) {
  base::Value::Dict params;
  params.Set("type", 1);

  CdpParamsResult result = BuildKeyEventParams(params);

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(WebCommandErrorCode::PARAM_INVALID, result.code);
  EXPECT_EQ("invalid param: type", result.message);
}

TEST(AIPageCommandParamUtilsTest, InputTypeSpec) {
  EXPECT_TRUE(IsSupportedInputType("default"));
  EXPECT_TRUE(IsSupportedInputType("datetime-local"));
  EXPECT_FALSE(IsSupportedInputType("datetime"));
  EXPECT_FALSE(IsSupportedInputType("checkbox"));
}

}  // namespace
}  // namespace OHOS::NWeb::AIPageCommandParamUtils