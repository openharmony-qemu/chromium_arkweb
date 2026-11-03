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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "cef/include/cef_base.h"

#define private public
#include "cef_delegate/screenshot_capture_util.h"
#undef private

#include "nweb_errors.h"

using namespace testing;
using namespace OHOS::NWeb;

// ===== CaptureResult Tests =====

/**
 * @tc.name: CaptureResult_DefaultConstructor_0100
 * @tc.desc: Default constructor initializes success=false, code=SUCCESS.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_DefaultConstructor_0100) {
  CaptureResult result;
  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.base64_data.empty());
  EXPECT_EQ(result.error_code, WebCommandErrorCode::SUCCESS);
  EXPECT_TRUE(result.error_message.empty());
}

/**
 * @tc.name: CaptureResult_SuccessConstructor_0100
 * @tc.desc: Success constructor sets success=true and base64_data.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_SuccessConstructor_0100) {
  CaptureResult result(true, "test_data");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.base64_data, "test_data");
  EXPECT_EQ(result.error_code, WebCommandErrorCode::SUCCESS);
  EXPECT_TRUE(result.error_message.empty());
}

/**
 * @tc.name: CaptureResult_SuccessConstructor_EmptyData_0100
 * @tc.desc: Success constructor with empty data.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_SuccessConstructor_EmptyData_0100) {
  CaptureResult result(true, "");
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.base64_data.empty());
  EXPECT_EQ(result.error_code, WebCommandErrorCode::SUCCESS);
}

/**
 * @tc.name: CaptureResult_ErrorConstructor_0100
 * @tc.desc: Error constructor sets success=false and error code/message.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_ErrorConstructor_0100) {
  CaptureResult result(WebCommandErrorCode::FAILED, "test error");
  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.base64_data.empty());
  EXPECT_EQ(result.error_code, WebCommandErrorCode::FAILED);
  EXPECT_EQ(result.error_message, "test error");
}

/**
 * @tc.name: CaptureResult_ErrorConstructor_EmptyMessage_0100
 * @tc.desc: Error constructor with empty message.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_ErrorConstructor_EmptyMessage_0100) {
  CaptureResult result(WebCommandErrorCode::FAILED, "");
  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_code, WebCommandErrorCode::FAILED);
  EXPECT_TRUE(result.error_message.empty());
}

/**
 * @tc.name: CaptureResult_VariousErrorCodes_0100
 * @tc.desc: CaptureResult works with various valid WebCommandErrorCode values.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_VariousErrorCodes_0100) {
  CaptureResult r1(WebCommandErrorCode::WEB_NWEB_NULL, "");
  EXPECT_EQ(r1.error_code, WebCommandErrorCode::WEB_NWEB_NULL);

  CaptureResult r2(WebCommandErrorCode::PAGE_NOT_READY, "");
  EXPECT_EQ(r2.error_code, WebCommandErrorCode::PAGE_NOT_READY);

  CaptureResult r3(WebCommandErrorCode::ELEMENT_NOT_FOUND, "");
  EXPECT_EQ(r3.error_code, WebCommandErrorCode::ELEMENT_NOT_FOUND);

  CaptureResult r4(WebCommandErrorCode::FAILED, "");
  EXPECT_EQ(r4.error_code, WebCommandErrorCode::FAILED);

  CaptureResult r5(WebCommandErrorCode::CDP_SEND_FAILED, "");
  EXPECT_EQ(r5.error_code, WebCommandErrorCode::CDP_SEND_FAILED);

  CaptureResult r6(WebCommandErrorCode::CDP_SESSION_NOT_READY, "");
  EXPECT_EQ(r6.error_code, WebCommandErrorCode::CDP_SESSION_NOT_READY);

  CaptureResult r7(WebCommandErrorCode::CDP_COMMAND_FAILED, "");
  EXPECT_EQ(r7.error_code, WebCommandErrorCode::CDP_COMMAND_FAILED);

  CaptureResult r8(WebCommandErrorCode::CDP_RESPONSE_ERROR, "");
  EXPECT_EQ(r8.error_code, WebCommandErrorCode::CDP_RESPONSE_ERROR);
}

/**
 * @tc.name: CaptureResult_CopyConstruction_0100
 * @tc.desc: CaptureResult supports copy construction.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_CopyConstruction_0100) {
  CaptureResult original(true, "original_data");
  CaptureResult copy(original);
  EXPECT_EQ(copy.success, original.success);
  EXPECT_EQ(copy.base64_data, original.base64_data);
  EXPECT_EQ(copy.error_code, original.error_code);
  EXPECT_EQ(copy.error_message, original.error_message);
}

/**
 * @tc.name: CaptureResult_CopyConstruction_Error_0100
 * @tc.desc: CaptureResult copy construction with error result.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_CopyConstruction_Error_0100) {
  CaptureResult original(WebCommandErrorCode::FAILED, "error msg");
  CaptureResult copy(original);
  EXPECT_EQ(copy.success, original.success);
  EXPECT_EQ(copy.error_code, original.error_code);
  EXPECT_EQ(copy.error_message, original.error_message);
}

/**
 * @tc.name: CaptureResult_MoveConstruction_0100
 * @tc.desc: CaptureResult supports move construction.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_MoveConstruction_0100) {
  CaptureResult original(true, "original_data");
  CaptureResult moved(std::move(original));
  EXPECT_TRUE(moved.success);
  EXPECT_EQ(moved.base64_data, "original_data");
}

/**
 * @tc.name: CaptureResult_MoveConstruction_Error_0100
 * @tc.desc: CaptureResult move construction with error result.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_MoveConstruction_Error_0100) {
  CaptureResult original(WebCommandErrorCode::FAILED, "error msg");
  CaptureResult moved(std::move(original));
  EXPECT_FALSE(moved.success);
  EXPECT_EQ(moved.error_code, WebCommandErrorCode::FAILED);
  EXPECT_EQ(moved.error_message, "error msg");
}

/**
 * @tc.name: CaptureResult_CopyAssignment_0100
 * @tc.desc: CaptureResult supports copy assignment.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_CopyAssignment_0100) {
  CaptureResult result1;
  CaptureResult result2(true, "data2");
  result1 = result2;
  EXPECT_EQ(result1.success, result2.success);
  EXPECT_EQ(result1.base64_data, result2.base64_data);
  EXPECT_EQ(result1.error_code, result2.error_code);
}

/**
 * @tc.name: CaptureResult_CopyAssignment_Error_0100
 * @tc.desc: CaptureResult copy assignment with error result.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_CopyAssignment_Error_0100) {
  CaptureResult result1;
  CaptureResult result2(WebCommandErrorCode::FAILED, "error");
  result1 = result2;
  EXPECT_EQ(result1.error_code, result2.error_code);
  EXPECT_EQ(result1.error_message, result2.error_message);
}

/**
 * @tc.name: CaptureResult_MoveAssignment_0100
 * @tc.desc: CaptureResult supports move assignment.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_MoveAssignment_0100) {
  CaptureResult result1;
  CaptureResult result2(true, "data2");
  result1 = std::move(result2);
  EXPECT_TRUE(result1.success);
  EXPECT_EQ(result1.base64_data, "data2");
}

/**
 * @tc.name: CaptureResult_ErrorCodeAssignment_0100
 * @tc.desc: CaptureResult error_code field can be reassigned.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_ErrorCodeAssignment_0100) {
  CaptureResult result;
  EXPECT_EQ(result.error_code, WebCommandErrorCode::SUCCESS);

  result.error_code = WebCommandErrorCode::WEB_NWEB_NULL;
  EXPECT_EQ(result.error_code, WebCommandErrorCode::WEB_NWEB_NULL);

  result.error_code = WebCommandErrorCode::PAGE_NOT_READY;
  EXPECT_EQ(result.error_code, WebCommandErrorCode::PAGE_NOT_READY);

  result.error_code = WebCommandErrorCode::ELEMENT_NOT_FOUND;
  EXPECT_EQ(result.error_code, WebCommandErrorCode::ELEMENT_NOT_FOUND);

  result.error_code = WebCommandErrorCode::FAILED;
  EXPECT_EQ(result.error_code, WebCommandErrorCode::FAILED);
}

/**
 * @tc.name: CaptureResult_ErrorMessageAssignment_0100
 * @tc.desc: CaptureResult error_message field can be reassigned.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_ErrorMessageAssignment_0100) {
  CaptureResult result;
  result.error_message = "first error";
  EXPECT_EQ(result.error_message, "first error");

  result.error_message = "second error";
  EXPECT_EQ(result.error_message, "second error");
}

/**
 * @tc.name: CaptureResult_DataAssignment_0100
 * @tc.desc: CaptureResult base64_data field can be reassigned.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_DataAssignment_0100) {
  CaptureResult result;
  result.base64_data = "encoded_data";
  EXPECT_EQ(result.base64_data, "encoded_data");
}

/**
 * @tc.name: CaptureResult_SuccessFieldAssignment_0100
 * @tc.desc: CaptureResult success field can be reassigned.
 * @tc.type: FUNC
 */
TEST(CaptureResultTest, CaptureResult_SuccessFieldAssignment_0100) {
  CaptureResult result;
  EXPECT_FALSE(result.success);

  result.success = true;
  EXPECT_TRUE(result.success);

  result.success = false;
  EXPECT_FALSE(result.success);
}
