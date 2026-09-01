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
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "cef/include/cef_base.h"

// Access private members of ai_page_command_dispatcher for testing.
// Only include the dispatcher header under the macro; other headers
// are included after #undef to avoid polluting their privates.
#define private public
#include "cef_delegate/ai_page_command_dispatcher.h"
#include "cef_delegate/nweb_delegate.h"
#undef private

#include "cef_delegate/file_upload_observer.h"
#include "cef_delegate/test/mock_browser_host.h"
#include "cef/include/cef_browser.h"
#include "include/cef_values.h"
#include "nweb_errors.h"
#include "nweb_value_callback.h"

using namespace testing;
using namespace OHOS::NWeb;
using OHOS::NWeb::test::MockBrowserHost;
using OHOS::NWeb::test::TestStringValueCallback;
using OHOS::NWeb::test::FakeRegistration;
using OHOS::NWeb::test::ExtractCodeFromJson;
using OHOS::NWeb::test::ExtractMessageFromJson;

// ===== Test Fixture =====

class AIPageCommandDispatcherTest : public Test {
 protected:
  void SetUp() override {
    mock_browser_ = new MockBrowserHost();
    callback_ = std::make_shared<TestStringValueCallback>();
  }

  void TearDown() override {
    callback_.reset();
    saved_observer_ = nullptr;
    mock_browser_ = nullptr;
  }

  static std::string BuildCommand(const std::string& method,
                                  base::Value::Dict params = {}) {
    base::Value::Dict cmd;
    cmd.Set("method", method);
    if (!params.empty()) {
      cmd.Set("params", std::move(params));
    }
    std::string json;
    base::JSONWriter::Write(cmd, &json);
    return json;
  }

  CefRefPtr<MockBrowserHost> mock_browser_;
  std::shared_ptr<TestStringValueCallback> callback_;
  CefRefPtr<CefDevToolsMessageObserver> saved_observer_;
};

// ===== Dispatch routing tests =====

/**
 * @tc.name: Dispatch_InvalidJSON_EmptyResult_0100
 * @tc.desc: Dispatch with invalid JSON returns empty result.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_InvalidJSON_EmptyResult_0100) {
  AIPageCommandDispatcher::Dispatch("not json", mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(callback_->value_, "");
}

/**
 * @tc.name: Dispatch_MissingMethod_EmptyResult_0100
 * @tc.desc: Dispatch with missing method field returns empty result.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_MissingMethod_EmptyResult_0100) {
  AIPageCommandDispatcher::Dispatch("{}", mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(callback_->value_, "");
}

/**
 * @tc.name: Dispatch_EmptyMethod_EmptyResult_0100
 * @tc.desc: Dispatch with empty method field returns empty result.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_EmptyMethod_EmptyResult_0100) {
  AIPageCommandDispatcher::Dispatch(
      R"({"method":""})", mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(callback_->value_, "");
}

/**
 * @tc.name: Dispatch_UnknownMethod_EmptyResult_0100
 * @tc.desc: Dispatch with unknown method returns empty result.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_UnknownMethod_EmptyResult_0100) {
  AIPageCommandDispatcher::Dispatch(
      R"({"method":"nonexistent"})", mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(callback_->value_, "");
}

// ===== Scroll command tests =====

/**
 * @tc.name: Scroll_NullBrowser_ErrorCode_0100
 * @tc.desc: Scroll with null browser returns WEB_NWEB_NULL error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Scroll_NullBrowser_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("x", 100.0);
  params.Set("y", 200.0);
  std::string cmd = BuildCommand("scroll", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, nullptr, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}

/**
 * @tc.name: Scroll_MissingParams_ErrorCode_0100
 * @tc.desc: Scroll with missing params returns JSON_IS_INVALID error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Scroll_MissingParams_ErrorCode_0100) {
  std::string cmd = R"({"method":"scroll"})";
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::JSON_IS_INVALID));
}

/**
 * @tc.name: Scroll_MissingXY_ErrorCode_0100
 * @tc.desc: Scroll with missing x/y params returns PARAM_MISSING error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Scroll_MissingXY_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("deltaX", 10.0);
  std::string cmd = BuildCommand("scroll", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_MISSING));
}

/**
 * @tc.name: Scroll_InvalidParamType_ErrorCode_0100
 * @tc.desc: Scroll with non-numeric x param returns PARAM_INVALID error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Scroll_InvalidParamType_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("x", "not_a_number");
  params.Set("y", 200.0);
  std::string cmd = BuildCommand("scroll", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}

/**
 * @tc.name: Scroll_InvalidSpeedType_ErrorCode_0100
 * @tc.desc: Scroll with non-integer speed returns PARAM_INVALID error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Scroll_InvalidSpeedType_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("x", 100.0);
  params.Set("y", 200.0);
  params.Set("speed", "fast");
  std::string cmd = BuildCommand("scroll", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}

/**
 * @tc.name: Scroll_ZeroSpeed_ErrorCode_0100
 * @tc.desc: Scroll with speed=0 returns PARAM_INVALID error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Scroll_ZeroSpeed_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("x", 100.0);
  params.Set("y", 200.0);
  params.Set("speed", 0);
  std::string cmd = BuildCommand("scroll", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}

/**
 * @tc.name: Scroll_CDPFailed_ErrorCode_0100
 * @tc.desc: Scroll when CDP returns 0 returns CDP_SEND_FAILED error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Scroll_CDPFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, ExecuteDevToolsMethod(_, _, _))
      .WillOnce(Return(0));

  base::Value::Dict params;
  params.Set("x", 100.0);
  params.Set("y", 200.0);
  std::string cmd = BuildCommand("scroll", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

/**
 * @tc.name: Scroll_Success_0100
 * @tc.desc: Scroll with valid params and successful CDP returns SUCCESS.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Scroll_Success_0100) {
  EXPECT_CALL(*mock_browser_, ExecuteDevToolsMethod(
                                  _, StrEq("Input.synthesizeScrollGesture"), _))
      .WillOnce(Return(42));

  base::Value::Dict params;
  params.Set("x", 100.0);
  params.Set("y", 200.0);
  params.Set("deltaX", 50.0);
  params.Set("deltaY", -30.0);
  params.Set("speed", 800);
  std::string cmd = BuildCommand("scroll", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

// ===== BuildScrollParams tests =====

/**
 * @tc.name: BuildScrollParams_MissingXY_ReturnsNull_0100
 * @tc.desc: BuildScrollParams with missing x/y returns nullptr.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, BuildScrollParams_MissingXY_ReturnsNull_0100) {
  base::Value::Dict params;
  params.Set("x", 100.0);
  auto result = AIPageCommandDispatcher::BuildScrollParams(params);
  EXPECT_FALSE(result);
}

/**
 * @tc.name: BuildScrollParams_AllFields_0100
 * @tc.desc: BuildScrollParams with all fields populates CDP params.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, BuildScrollParams_AllFields_0100) {
  base::Value::Dict params;
  params.Set("x", 10.0);
  params.Set("y", 20.0);
  params.Set("deltaX", 5.0);
  params.Set("deltaY", -5.0);
  params.Set("speed", 500);
  auto result = AIPageCommandDispatcher::BuildScrollParams(params);
  ASSERT_TRUE(result);
  EXPECT_DOUBLE_EQ(result->GetDouble("x"), 10.0);
  EXPECT_DOUBLE_EQ(result->GetDouble("y"), 20.0);
  EXPECT_DOUBLE_EQ(result->GetDouble("xDistance"), 5.0);
  EXPECT_DOUBLE_EQ(result->GetDouble("yDistance"), -5.0);
  EXPECT_EQ(result->GetInt("speed"), 500);
}

/**
 * @tc.name: BuildScrollParams_OnlyXY_0100
 * @tc.desc: BuildScrollParams with only x/y, optional fields absent.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, BuildScrollParams_OnlyXY_0100) {
  base::Value::Dict params;
  params.Set("x", 50.0);
  params.Set("y", 60.0);
  auto result = AIPageCommandDispatcher::BuildScrollParams(params);
  ASSERT_TRUE(result);
  EXPECT_DOUBLE_EQ(result->GetDouble("x"), 50.0);
  EXPECT_DOUBLE_EQ(result->GetDouble("y"), 60.0);
  EXPECT_FALSE(result->HasKey("xDistance"));
  EXPECT_FALSE(result->HasKey("yDistance"));
  EXPECT_FALSE(result->HasKey("speed"));
}

// ===== ParseIntList / ParseStringList tests =====

/**
 * @tc.name: ParseIntList_ValidList_0100
 * @tc.desc: ParseIntList extracts integers from a valid list.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ParseIntList_ValidList_0100) {
  base::Value::Dict dict;
  base::Value::List list;
  list.Append(1);
  list.Append(2);
  list.Append(3);
  dict.Set("indexes", std::move(list));
  auto result = AIPageCommandDispatcher::ParseIntList(dict, "indexes");
  EXPECT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], 1);
  EXPECT_EQ(result[1], 2);
  EXPECT_EQ(result[2], 3);
}

/**
 * @tc.name: ParseIntList_MissingKey_ReturnsEmpty_0100
 * @tc.desc: ParseIntList with missing key returns empty vector.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ParseIntList_MissingKey_ReturnsEmpty_0100) {
  base::Value::Dict dict;
  auto result = AIPageCommandDispatcher::ParseIntList(dict, "nonexistent");
  EXPECT_TRUE(result.empty());
}

/**
 * @tc.name: ParseIntList_NonIntItems_Skipped_0100
 * @tc.desc: ParseIntList skips non-integer items in the list.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ParseIntList_NonIntItems_Skipped_0100) {
  base::Value::Dict dict;
  base::Value::List list;
  list.Append(1);
  list.Append("two");
  list.Append(3);
  dict.Set("indexes", std::move(list));
  auto result = AIPageCommandDispatcher::ParseIntList(dict, "indexes");
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], 1);
  EXPECT_EQ(result[1], 3);
}

/**
 * @tc.name: ParseStringList_ValidList_0100
 * @tc.desc: ParseStringList extracts strings from a valid list.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ParseStringList_ValidList_0100) {
  base::Value::Dict dict;
  base::Value::List list;
  list.Append("hello");
  list.Append("world");
  dict.Set("values", std::move(list));
  auto result = AIPageCommandDispatcher::ParseStringList(dict, "values");
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "hello");
  EXPECT_EQ(result[1], "world");
}

/**
 * @tc.name: ParseStringList_MissingKey_ReturnsEmpty_0100
 * @tc.desc: ParseStringList with missing key returns empty vector.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ParseStringList_MissingKey_ReturnsEmpty_0100) {
  base::Value::Dict dict;
  auto result = AIPageCommandDispatcher::ParseStringList(dict, "nonexistent");
  EXPECT_TRUE(result.empty());
}

/**
 * @tc.name: ParseStringList_NonStringItems_Skipped_0100
 * @tc.desc: ParseStringList skips non-string items in the list.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ParseStringList_NonStringItems_Skipped_0100) {
  base::Value::Dict dict;
  base::Value::List list;
  list.Append("hello");
  list.Append(42);
  list.Append("world");
  dict.Set("values", std::move(list));
  auto result = AIPageCommandDispatcher::ParseStringList(dict, "values");
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "hello");
  EXPECT_EQ(result[1], "world");
}

// ===== Select command tests =====

/**
 * @tc.name: Select_NullBrowser_ErrorCode_0100
 * @tc.desc: Select with null browser returns WEB_NWEB_NULL error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Select_NullBrowser_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("xpath", "//select");
  base::Value::List idx_list;
  idx_list.Append(0);
  params.Set("indexes", std::move(idx_list));
  std::string cmd = BuildCommand("select", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, nullptr, callback_);
  EXPECT_TRUE(callback_->called_);
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
#else
  EXPECT_EQ(callback_->value_, "");
#endif
}

/**
 * @tc.name: Select_MissingParams_ErrorCode_0100
 * @tc.desc: Select with missing params returns JSON_IS_INVALID error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Select_MissingParams_ErrorCode_0100) {
  std::string cmd = R"({"method":"select"})";
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::JSON_IS_INVALID));
#endif
}

/**
 * @tc.name: Select_EmptyXpath_ErrorCode_0100
 * @tc.desc: Select with empty xpath returns JSON_INVALID_XPATH error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Select_EmptyXpath_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("xpath", "");
  base::Value::List idx_list;
  idx_list.Append(0);
  params.Set("indexes", std::move(idx_list));
  std::string cmd = BuildCommand("select", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_XPATH));
#endif
}

/**
 * @tc.name: Select_EmptyIndexesAndValues_ErrorCode_0100
 * @tc.desc: Select with both empty indexes and values returns
 *           JSON_INVALID_SELECT_OPTIONS error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Select_EmptyIndexesAndValues_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("xpath", "//select[@id='color']");
  std::string cmd = BuildCommand("select", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_SELECT_OPTIONS));
#endif
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
/**
 * @tc.name: Select_Success_WithIndexes_0100
 * @tc.desc: Select with valid indexes calls ExecuteSelectCommand.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Select_Success_WithIndexes_0100) {
  EXPECT_CALL(*mock_browser_, ExecuteSelectCommand(_, _, _))
      .WillOnce(Return(static_cast<int32_t>(WebCommandErrorCode::SUCCESS)));

  base::Value::Dict params;
  params.Set("xpath", "//select[@id='color']");
  base::Value::List idx_list;
  idx_list.Append(0);
  idx_list.Append(1);
  params.Set("indexes", std::move(idx_list));
  std::string cmd = BuildCommand("select", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

/**
 * @tc.name: Select_Success_WithValues_0100
 * @tc.desc: Select with valid values calls ExecuteSelectCommand.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Select_Success_WithValues_0100) {
  EXPECT_CALL(*mock_browser_, ExecuteSelectCommand(_, _, _))
      .WillOnce(Return(static_cast<int32_t>(WebCommandErrorCode::SUCCESS)));

  base::Value::Dict params;
  params.Set("xpath", "//select[@id='color']");
  base::Value::List val_list;
  val_list.Append("red");
  val_list.Append("blue");
  params.Set("values", std::move(val_list));
  std::string cmd = BuildCommand("select", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

/**
 * @tc.name: Select_ExecuteFails_ErrorCode_0100
 * @tc.desc: Select when ExecuteSelectCommand fails returns error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Select_ExecuteFails_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, ExecuteSelectCommand(_, _, _))
      .WillOnce(Return(static_cast<int32_t>(WebCommandErrorCode::FAILED)));

  base::Value::Dict params;
  params.Set("xpath", "//select");
  base::Value::List val_list;
  val_list.Append("red");
  params.Set("values", std::move(val_list));
  std::string cmd = BuildCommand("select", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_NE(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

// ===== Upload file command tests =====

/**
 * @tc.name: UploadFile_NullBrowser_ErrorCode_0100
 * @tc.desc: UploadFile with null browser returns WEB_NWEB_NULL error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, UploadFile_NullBrowser_ErrorCode_0100) {
  base::Value::Dict params;
  base::Value::List files;
  files.Append("/tmp/test.txt");
  params.Set("files", std::move(files));
  std::string cmd = BuildCommand("uploadFile", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, nullptr, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}

/**
 * @tc.name: UploadFile_MissingParams_ErrorCode_0100
 * @tc.desc: UploadFile with missing params returns JSON_IS_INVALID error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, UploadFile_MissingParams_ErrorCode_0100) {
  std::string cmd = R"({"method":"uploadFile"})";
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::JSON_IS_INVALID));
}

/**
 * @tc.name: UploadFile_EmptyFilesList_ErrorCode_0100
 * @tc.desc: UploadFile with empty files list returns FILE_LIST_EMPTY error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, UploadFile_EmptyFilesList_ErrorCode_0100) {
  base::Value::Dict params;
  base::Value::List files;
  params.Set("files", std::move(files));
  std::string cmd = BuildCommand("uploadFile", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::FILE_LIST_EMPTY));
}

/**
 * @tc.name: UploadFile_EmptyPathInList_ErrorCode_0100
 * @tc.desc: UploadFile with empty path in files list returns FILE_PATH_EMPTY.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, UploadFile_EmptyPathInList_ErrorCode_0100) {
  base::Value::Dict params;
  base::Value::List files;
  files.Append("");
  params.Set("files", std::move(files));
  std::string cmd = BuildCommand("uploadFile", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::FILE_PATH_EMPTY));
}

/**
 * @tc.name: UploadFile_MissingXpath_ErrorCode_0100
 * @tc.desc: UploadFile with missing xpath returns PARAM_MISSING.
 *          Uses /proc/self/exe as a guaranteed-existing file to pass
 *          ValidateFilePaths without depending on test data files.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, UploadFile_MissingXpath_ErrorCode_0100) {
  base::Value::Dict params;
  base::Value::List files;
  files.Append("/proc/self/exe");
  params.Set("files", std::move(files));
  std::string cmd = BuildCommand("uploadFile", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_MISSING));
}

/**
 * @tc.name: UploadFile_EmptyXpath_ErrorCode_0100
 * @tc.desc: UploadFile with empty xpath returns PARAM_MISSING.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, UploadFile_EmptyXpath_ErrorCode_0100) {
  base::Value::Dict params;
  base::Value::List files;
  files.Append("/proc/self/exe");
  params.Set("files", std::move(files));
  params.Set("xpath", "");
  std::string cmd = BuildCommand("uploadFile", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_MISSING));
}

// ===== ReplyError tests =====

/**
 * @tc.name: ReplyError_NullCallback_NoCrash_0100
 * @tc.desc: ReplyError with null callback does not crash.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ReplyError_NullCallback_NoCrash_0100) {
  AIPageCommandDispatcher::ReplyError(nullptr,
                                      WebCommandErrorCode::FAILED, "test");
}

/**
 * @tc.name: ReplyError_ValidCallback_JsonResult_0100
 * @tc.desc: ReplyError produces valid JSON with code and message.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ReplyError_ValidCallback_JsonResult_0100) {
  AIPageCommandDispatcher::ReplyError(
      callback_, WebCommandErrorCode::CDP_SEND_FAILED, "send failed");
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
  EXPECT_EQ(ExtractMessageFromJson(callback_->value_), "send failed");
}

// ===== Error code range tests =====

/**
 * @tc.name: ErrorCodes_CDPRange_0100
 * @tc.desc: Verify CDP error codes are in the 350-369 range.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ErrorCodes_CDPRange_0100) {
  EXPECT_GE(static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED), 350);
  EXPECT_LE(static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR), 369);
}

/**
 * @tc.name: ErrorCodes_FileRange_0100
 * @tc.desc: Verify file upload error codes are in the 370-389 range.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ErrorCodes_FileRange_0100) {
  EXPECT_GE(static_cast<int32_t>(WebCommandErrorCode::FILE_NOT_FOUND), 370);
  EXPECT_LE(static_cast<int32_t>(WebCommandErrorCode::FILE_LIST_EMPTY), 389);
}

/**
 * @tc.name: ErrorCodes_DispatchRange_0100
 * @tc.desc: Verify command dispatch error codes are in the 390-399 range.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ErrorCodes_DispatchRange_0100) {
  EXPECT_GE(static_cast<int32_t>(WebCommandErrorCode::UNSUPPORTED_COMMAND), 390);
  EXPECT_LE(static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID), 399);
}

// ===== ValidateFilePaths tests =====

/**
 * @tc.name: ValidateFilePaths_EmptyPath_ReturnsFalse_0100
 * @tc.desc: ValidateFilePaths with empty path returns false and sets
 *           FILE_PATH_EMPTY.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ValidateFilePaths_EmptyPath_ReturnsFalse_0100) {
  std::vector<std::string> files = {""};
  std::vector<std::string> warnings;
  auto cb = std::make_shared<TestStringValueCallback>();
  bool result = AIPageCommandDispatcher::ValidateFilePaths(files, warnings, cb);
  EXPECT_FALSE(result);
  EXPECT_TRUE(cb->called_);
  EXPECT_EQ(ExtractCodeFromJson(cb->value_),
            static_cast<int32_t>(WebCommandErrorCode::FILE_PATH_EMPTY));
}

// ===== Dispatch routing tests for scroll/select/uploadFile =====

/**
 * @tc.name: Dispatch_ScrollRoutes_0100
 * @tc.desc: Verify "scroll" method routes to HandleScrollCommand.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_ScrollRoutes_0100) {
  base::Value::Dict params;
  params.Set("x", 100.0);
  params.Set("y", 200.0);
  std::string cmd = BuildCommand("scroll", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, nullptr, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_NE(callback_->value_, "");
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}

/**
 * @tc.name: Dispatch_UploadFileRoutes_0100
 * @tc.desc: Verify "uploadFile" routes through to observer creation by
 *           checking AddDevToolsMessageObserver is invoked.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_UploadFileRoutes_0100) {
  base::Value::Dict params;
  base::Value::List files;
  files.Append("/proc/self/exe");
  params.Set("files", std::move(files));
  params.Set("xpath", "//input[@type='file']");
  std::string cmd = BuildCommand("uploadFile", std::move(params));

  // Save the observer so it stays alive via saved_observer_ (the production
  // code creates the observer with raw new, and AddDevToolsMessageObserver
  // keeps it alive in real CEF. The mock does not, so we must hold the ref.)
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(DoAll(SaveArg<0>(&saved_observer_),
                      Return(new FakeRegistration())));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(1));

  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
}

// ===== Null callback safety =====

/**
 * @tc.name: Dispatch_NullCallback_NoCrash_0100
 * @tc.desc: Dispatch with null callback does not crash.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_NullCallback_NoCrash_0100) {
  base::Value::Dict params;
  params.Set("x", 100.0);
  params.Set("y", 200.0);
  std::string cmd = BuildCommand("scroll", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, nullptr, nullptr);
}

/**
 * @tc.name: ResolveEmptyResult_NullCallback_NoCrash_0100
 * @tc.desc: ResolveEmptyResult with null callback does not crash.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ResolveEmptyResult_NullCallback_NoCrash_0100) {
  AIPageCommandDispatcher::ResolveEmptyResult(nullptr);
}

/**
 * @tc.name: ResolveEmptyResult_ValidCallback_0100
 * @tc.desc: ResolveEmptyResult with valid callback sends empty string.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ResolveEmptyResult_ValidCallback_0100) {
  AIPageCommandDispatcher::ResolveEmptyResult(callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(callback_->value_, "");
}

// ===== FileUploadObserver construction test =====

/**
 * @tc.name: FileUploadObserver_Construct_0100
 * @tc.desc: FileUploadObserver can be constructed without crash.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, FileUploadObserver_Construct_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> warnings;
  auto cb = std::make_shared<TestStringValueCallback>();
  auto observer = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input[@type='file']", files, warnings, cb));
  EXPECT_TRUE(observer);
}

/**
 * @tc.name: ScreenCapture_NodeIdValidationSuccess_0100
 * @tc.desc: ValidateElementIdCallbackImpl OnResult with ok=true triggers CaptureElementByNodeId.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_NodeIdValidationSuccess_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_, ExecuteDevToolsMethod(_, _, _))
      .WillOnce(Return(1));

  CefRefPtr<CefAIPageCommandCallback> capturedCallback;
  EXPECT_CALL(*mock_browser_, ValidateElementId(_, _))
      .WillOnce(SaveArg<1>(&capturedCallback));

  base::Value::Dict params;
  params.Set("nodeid", "frame|doc|123");
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);

  ASSERT_TRUE(capturedCallback);
  capturedCallback->OnResult(R"({"ok":true})");
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: ScreenCapture_NodeIdValidationFailed_0100
 * @tc.desc: ValidateElementIdCallbackImpl OnResult with ok=false returns PARAM_INVALID.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_NodeIdValidationFailed_0100) {
  CefRefPtr<CefAIPageCommandCallback> capturedCallback;
  EXPECT_CALL(*mock_browser_, ValidateElementId(_, _))
      .WillOnce(SaveArg<1>(&capturedCallback));

  base::Value::Dict params;
  params.Set("nodeid", "frame|doc|123");
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);

  ASSERT_TRUE(capturedCallback);
  capturedCallback->OnResult(R"({"ok":false,"error":"element not found"})");
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}

/**
 * @tc.name: ScreenCapture_NodeIdValidationInvalidJson_0100
 * @tc.desc: ValidateElementIdCallbackImpl OnResult with invalid JSON returns FAILED.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_NodeIdValidationInvalidJson_0100) {
  CefRefPtr<CefAIPageCommandCallback> capturedCallback;
  EXPECT_CALL(*mock_browser_, ValidateElementId(_, _))
      .WillOnce(SaveArg<1>(&capturedCallback));

  base::Value::Dict params;
  params.Set("nodeid", "frame|doc|123");
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);

  ASSERT_TRUE(capturedCallback);
  capturedCallback->OnResult("not json at all");
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::FAILED));
}

/**
 * @tc.name: ScreenCapture_Xpath_Success_0100
 * @tc.desc: ScreenCapture with xpath parameter calls CaptureElement.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_Xpath_Success_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_, ExecuteDevToolsMethod(_, _, _))
      .WillOnce(Return(1));

  base::Value::Dict params;
  params.Set("xpath", "//div[@id='container']");
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: ScreenCapture_FullPage_Success_0100
 * @tc.desc: ScreenCapture with no xpath triggers full page capture.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_FullPage_Success_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_, ExecuteDevToolsMethod(_, _, _))
      .WillOnce(Return(1));

  base::Value::Dict params;
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: ScreenCapture_InvalidNodeId_ErrorCode_0100
 * @tc.desc: ScreenCapture with invalid nodeid format returns PARAM_INVALID error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_InvalidNodeId_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("nodeid", "not_a_valid_node_id_format");
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}

/**
 * @tc.name: ScreenCapture_ValidNodeId_TriggersValidation_0100
 * @tc.desc: ScreenCapture with valid nodeid format triggers ValidateElementId.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_ValidNodeId_TriggersValidation_0100) {
  EXPECT_CALL(*mock_browser_, ValidateElementId(_, _));

  base::Value::Dict params;
  params.Set("nodeid", "frame|doc|123");
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: ScreenCapture_NodeIdMissingDomSeparator_ErrorCode_0100
 * @tc.desc: ScreenCapture with nodeid missing dom separator returns PARAM_INVALID.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_NodeIdMissingDomSeparator_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("nodeid", "frame_doc_123");
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}

/**
 * @tc.name: ScreenCapture_NodeIdInvalidDomNodeId_ErrorCode_0100
 * @tc.desc: ScreenCapture with nodeid having non-numeric dom_node_id returns PARAM_INVALID.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_NodeIdInvalidDomNodeId_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("nodeid", "frame|doc|notanumber");
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}

/**
 * @tc.name: ScreenCapture_NodeIdZeroDomNodeId_ErrorCode_0100
 * @tc.desc: ScreenCapture with nodeid having zero dom_node_id returns PARAM_INVALID.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_NodeIdZeroDomNodeId_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("nodeid", "frame|doc|0");
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  AIPageCommandDispatcher::Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}
