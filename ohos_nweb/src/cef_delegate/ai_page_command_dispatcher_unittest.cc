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

#include <array>
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
#include "cef_delegate/nweb_preference_delegate.h"
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
    pref_delegate_ = std::make_shared<NWebPreferenceDelegate>();
    nweb_delegate_ = std::make_unique<NWebDelegate>(argc_, argv_.data());
    nweb_delegate_->preference_delegate_ = pref_delegate_;
    dispatcher_ = std::make_unique<AIPageCommandDispatcher>(
        nweb_delegate_->weak_factory_.GetWeakPtr());
  }

  void TearDown() override {
    dispatcher_.reset();
    callback_.reset();
    saved_observer_ = nullptr;
    mock_browser_ = nullptr;
    pref_delegate_.reset();
    nweb_delegate_.reset();
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

  static constexpr int argc_ = 1;
  std::array<const char*, 1> argv_ = {{"nweb_unittest"}};
  CefRefPtr<MockBrowserHost> mock_browser_;
  std::shared_ptr<TestStringValueCallback> callback_;
  CefRefPtr<CefDevToolsMessageObserver> saved_observer_;
  std::shared_ptr<NWebPreferenceDelegate> pref_delegate_;
  std::unique_ptr<NWebDelegate> nweb_delegate_;
  std::unique_ptr<AIPageCommandDispatcher> dispatcher_;
};

// ===== Dispatch routing tests =====

/**
 * @tc.name: Dispatch_InvalidJSON_EmptyResult_0100
 * @tc.desc: Dispatch with invalid JSON returns empty result.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_InvalidJSON_EmptyResult_0100) {
  dispatcher_->Dispatch("not json", mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(callback_->value_, "");
}

/**
 * @tc.name: Dispatch_MissingMethod_EmptyResult_0100
 * @tc.desc: Dispatch with missing method field returns empty result.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_MissingMethod_EmptyResult_0100) {
  dispatcher_->Dispatch("{}", mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(callback_->value_, "");
}

/**
 * @tc.name: Dispatch_EmptyMethod_EmptyResult_0100
 * @tc.desc: Dispatch with empty method field returns empty result.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, Dispatch_EmptyMethod_EmptyResult_0100) {
  dispatcher_->Dispatch(
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
  dispatcher_->Dispatch(
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
  dispatcher_->Dispatch(cmd, nullptr, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
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
  auto result = dispatcher_->ParseIntList(dict, "indexes");
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
  auto result = dispatcher_->ParseIntList(dict, "nonexistent");
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
  auto result = dispatcher_->ParseIntList(dict, "indexes");
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
  auto result = dispatcher_->ParseStringList(dict, "values");
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
  auto result = dispatcher_->ParseStringList(dict, "nonexistent");
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
  auto result = dispatcher_->ParseStringList(dict, "values");
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
  dispatcher_->Dispatch(cmd, nullptr, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, nullptr, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->ReplyError(nullptr,
                                      WebCommandErrorCode::FAILED, "test");
}

/**
 * @tc.name: ReplyError_ValidCallback_JsonResult_0100
 * @tc.desc: ReplyError produces valid JSON with code and message.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ReplyError_ValidCallback_JsonResult_0100) {
  dispatcher_->ReplyError(
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
  bool result = dispatcher_->ValidateFilePaths(files, warnings, cb);
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
  dispatcher_->Dispatch(cmd, nullptr, callback_);
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

  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
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
  dispatcher_->Dispatch(cmd, nullptr, nullptr);
}

/**
 * @tc.name: ResolveEmptyResult_NullCallback_NoCrash_0100
 * @tc.desc: ResolveEmptyResult with null callback does not crash.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ResolveEmptyResult_NullCallback_NoCrash_0100) {
  dispatcher_->ResolveEmptyResult(nullptr);
}

/**
 * @tc.name: ResolveEmptyResult_ValidCallback_0100
 * @tc.desc: ResolveEmptyResult with valid callback sends empty string.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ResolveEmptyResult_ValidCallback_0100) {
  dispatcher_->ResolveEmptyResult(callback_);
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

// ===== setZoomLevel / getZoomLevel command tests =====
/**
 * @tc.name: ValidateZoomLevelParam_DoubleValue_ReturnsTrue_0100
 * @tc.desc: ValidateZoomLevelParam accepts double zoomLevel in [0.25, 5.0].
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_DoubleValue_ReturnsTrue_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", 1.5);
  double factor = 0.0;
  EXPECT_TRUE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_NEAR(factor, 1.5, 0.001);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: ValidateZoomLevelParam_IntValue_NormalizesToDouble_0100
 * @tc.desc: ValidateZoomLevelParam accepts int zoomLevel and widens to double.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_IntValue_NormalizesToDouble_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", 2);
  double factor = 0.0;
  EXPECT_TRUE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_NEAR(factor, 2.0, 0.001);
}

/**
 * @tc.name: ValidateZoomLevelParam_Boundary_Low_ReturnsTrue_0100
 * @tc.desc: ValidateZoomLevelParam accepts 0.25 (inclusive lower bound).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_Boundary_Low_ReturnsTrue_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", 0.25);
  double factor = 99.0;
  EXPECT_TRUE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_NEAR(factor, 0.25, 0.001);
}

/**
 * @tc.name: ValidateZoomLevelParam_Boundary_High_ReturnsTrue_0100
 * @tc.desc: ValidateZoomLevelParam accepts 5.0 (inclusive upper bound).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_Boundary_High_ReturnsTrue_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", 5.0);
  double factor = 0.0;
  EXPECT_TRUE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_NEAR(factor, 5.0, 0.001);
}

/**
 * @tc.name: ValidateZoomLevelParam_OutOfRange_Low_ReturnsFalse_0100
 * @tc.desc: ValidateZoomLevelParam rejects 0.1 (< 0.25) with ZOOM_LEVEL_OUT_OF_RANGE.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_OutOfRange_Low_ReturnsFalse_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", 0.1);
  double factor = 0.0;
  EXPECT_FALSE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ZOOM_LEVEL_OUT_OF_RANGE));
}

/**
 * @tc.name: ValidateZoomLevelParam_OutOfRange_High_ReturnsFalse_0100
 * @tc.desc: ValidateZoomLevelParam rejects 10.0 (> 5.0) with ZOOM_LEVEL_OUT_OF_RANGE.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_OutOfRange_High_ReturnsFalse_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", 10.0);
  double factor = 0.0;
  EXPECT_FALSE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ZOOM_LEVEL_OUT_OF_RANGE));
}

/**
 * @tc.name: ValidateZoomLevelParam_Negative_ReturnsFalse_0100
 * @tc.desc: ValidateZoomLevelParam rejects negative with ZOOM_LEVEL_INVALID.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_Negative_ReturnsFalse_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", -1.0);
  double factor = 0.0;
  EXPECT_FALSE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ZOOM_LEVEL_INVALID));
}

/**
 * @tc.name: ValidateZoomLevelParam_Zero_ReturnsFalse_0100
 * @tc.desc: Zero must be rejected as INVALID, not as OUT_OF_RANGE.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_Zero_ReturnsFalse_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", 0.0);
  double factor = 0.0;
  EXPECT_FALSE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ZOOM_LEVEL_INVALID));
}

/**
 * @tc.name: ValidateZoomLevelParam_Missing_ReturnsFalse_0100
 * @tc.desc: ValidateZoomLevelParam rejects missing zoomLevel with PARAM_MISSING.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_Missing_ReturnsFalse_0100) {
  base::Value::Dict params;
  params.Set("other", 1.0);
  double factor = 0.0;
  EXPECT_FALSE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_MISSING));
}

/**
 * @tc.name: ValidateZoomLevelParam_NonNumeric_ReturnsFalse_0100
 * @tc.desc: ValidateZoomLevelParam rejects string zoomLevel with PARAM_MISSING.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       ValidateZoomLevelParam_NonNumeric_ReturnsFalse_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", "big");
  double factor = 0.0;
  EXPECT_FALSE(dispatcher_->ValidateZoomLevelParam(
      params, factor, callback_));
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_MISSING));
}

// ===== CheckZoomPreconditions tests (direct) =====

/**
 * @tc.name: CheckZoomPreconditions_Allowed_ReturnsTrue_0100
 * @tc.desc: CheckZoomPreconditions returns true when zoomControlAccess=true.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       CheckZoomPreconditions_Allowed_ReturnsTrue_0100) {
  pref_delegate_->PutZoomControlAccess(true);
  EXPECT_TRUE(dispatcher_->CheckZoomPreconditions(callback_));
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: CheckZoomPreconditions_ZoomDisabled_ReturnsFalse_0100
 * @tc.desc: CheckZoomPreconditions returns ZOOM_DISABLED when zoomControlAccess=false.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       CheckZoomPreconditions_ZoomDisabled_ReturnsFalse_0100) {
  pref_delegate_->PutZoomControlAccess(false);
  EXPECT_FALSE(dispatcher_->CheckZoomPreconditions(callback_));
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ZOOM_DISABLED));
}

/**
 * @tc.name: CheckZoomPreconditions_NullPref_DefaultsAllowZoom_0100
 * @tc.desc: CheckZoomPreconditions with null pref falls back to defaults (zoom proceeds).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       CheckZoomPreconditions_NullPref_DefaultsAllowZoom_0100) {
  auto no_pref_dispatcher =
      std::make_unique<AIPageCommandDispatcher>(base::WeakPtr<NWebDelegate>{});
  EXPECT_TRUE(no_pref_dispatcher->CheckZoomPreconditions(callback_));
  EXPECT_FALSE(callback_->called_);
}

// ===== ReplyZoomLevel tests (direct) =====

/**
 * @tc.name: ReplyZoomLevel_BuildsFlatJson_0100
 * @tc.desc: ReplyZoomLevel emits {code, message, zoomLevel} with no data wrapper.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ReplyZoomLevel_BuildsFlatJson_0100) {
  dispatcher_->ReplyZoomLevel(callback_, 1.5);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  auto val = base::JSONReader::Read(callback_->value_,
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(val && val->is_dict());
  EXPECT_FALSE(val->GetDict().FindDict("data"));
  auto zoom = val->GetDict().FindDouble("zoomLevel");
  ASSERT_TRUE(zoom.has_value());
  EXPECT_NEAR(*zoom, 1.5, 0.001);
}

/**
 * @tc.name: ReplyZoomLevel_NullCallback_NoOp_0100
 * @tc.desc: ReplyZoomLevel tolerates null callback without crashing.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ReplyZoomLevel_NullCallback_NoOp_0100) {
  dispatcher_->ReplyZoomLevel(nullptr, 1.0);
}

// ===== setZoomLevel happy path / range (Dispatch-driven) =====

/**
 * @tc.name: SetZoomLevel_Success_Normal_0100
 * @tc.desc: setZoomLevel(1.5) returns SUCCESS and forwards factor→level
 *           to CEF Host (AC-1.1).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_Success_Normal_0100) {
  EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).WillOnce(Return());

  base::Value::Dict params;
  params.Set("zoomLevel", 1.5);
  std::string cmd = BuildCommand("setZoomLevel", std::move(params));
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

/**
 * @tc.name: SetZoomLevel_Success_NonStandardStep_0100
 * @tc.desc: setZoomLevel(1.3) (non-preset step) succeeds (AC-4.1).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_Success_NonStandardStep_0100) {
  EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).WillOnce(Return());

  base::Value::Dict params;
  params.Set("zoomLevel", 1.3);
  std::string cmd = BuildCommand("setZoomLevel", std::move(params));
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

/**
 * @tc.name: SetZoomLevel_Success_Boundary_0100
 * @tc.desc: setZoomLevel at boundary 0.25 and 5.0 succeeds (AC-1.2, AC-1.3).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_Success_Boundary_0100) {
  for (double factor : {0.25, 5.0}) {
    EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).WillOnce(Return());
    auto cb = std::make_shared<TestStringValueCallback>();
    base::Value::Dict params;
    params.Set("zoomLevel", factor);
    std::string cmd = BuildCommand("setZoomLevel", std::move(params));
    dispatcher_->Dispatch(cmd, mock_browser_, cb);
    EXPECT_TRUE(cb->called_);
    EXPECT_EQ(ExtractCodeFromJson(cb->value_),
              static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
    testing::Mock::VerifyAndClearExpectations(mock_browser_.get());
  }
}

/**
 * @tc.name: SetZoomLevel_OutOfRange_ErrorCode_0100
 * @tc.desc: setZoomLevel(0.1) and setZoomLevel(10.0) return
 *           ZOOM_LEVEL_OUT_OF_RANGE(480) without invoking host (AC-1.4).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_OutOfRange_ErrorCode_0100) {
  for (double factor : {0.1, 10.0}) {
    EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).Times(0);
    auto cb = std::make_shared<TestStringValueCallback>();
    base::Value::Dict params;
    params.Set("zoomLevel", factor);
    std::string cmd = BuildCommand("setZoomLevel", std::move(params));
    dispatcher_->Dispatch(cmd, mock_browser_, cb);
    EXPECT_TRUE(cb->called_);
    EXPECT_EQ(ExtractCodeFromJson(cb->value_),
              static_cast<int32_t>(WebCommandErrorCode::ZOOM_LEVEL_OUT_OF_RANGE));
    testing::Mock::VerifyAndClearExpectations(mock_browser_.get());
  }
}

/**
 * @tc.name: SetZoomLevel_InvalidValue_ErrorCode_0100
 * @tc.desc: setZoomLevel with negative / zero returns
 *           ZOOM_LEVEL_INVALID(481) without invoking host (AC-1.5).
 *           NaN/Infinity 在 ArkWeb JSON 接口层即被拒绝返回 17100024，
 *           不会到达 dispatcher；UT 不直接覆盖（详见 spec AC-1.5 注）。
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_InvalidValue_ErrorCode_0100) {
  for (double factor : {-1.0, 0.0}) {
    EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).Times(0);
    auto cb = std::make_shared<TestStringValueCallback>();
    base::Value::Dict params;
    params.Set("zoomLevel", factor);
    std::string cmd = BuildCommand("setZoomLevel", std::move(params));
    dispatcher_->Dispatch(cmd, mock_browser_, cb);
    EXPECT_TRUE(cb->called_);
    EXPECT_EQ(ExtractCodeFromJson(cb->value_),
              static_cast<int32_t>(WebCommandErrorCode::ZOOM_LEVEL_INVALID));
    testing::Mock::VerifyAndClearExpectations(mock_browser_.get());
  }
}

/**
 * @tc.name: SetZoomLevel_NonNumericType_ErrorCode_0100
 * @tc.desc: setZoomLevel with string zoomLevel returns PARAM_MISSING(391) (AC-1.5b).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_NonNumericType_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).Times(0);
  base::Value::Dict params;
  params.Set("zoomLevel", "big");
  std::string cmd = BuildCommand("setZoomLevel", std::move(params));
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_MISSING));
}

/**
 * @tc.name: SetZoomLevel_MissingZoomLevel_ErrorCode_0100
 * @tc.desc: setZoomLevel with missing zoomLevel field returns
 *           PARAM_MISSING(391) (AC-1.6).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_MissingZoomLevel_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).Times(0);
  base::Value::Dict params;
  params.Set("other", 1.0);
  std::string cmd = BuildCommand("setZoomLevel", std::move(params));
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_MISSING));
}

/**
 * @tc.name: SetZoomLevel_Disabled_ErrorCode_0100
 * @tc.desc: setZoomLevel when zoomControlAccess=false returns
 *           ZOOM_DISABLED(482) (AC-1.8). Host not invoked.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_Disabled_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).Times(0);
  base::Value::Dict params;
  params.Set("zoomLevel", 1.5);
  std::string cmd = BuildCommand("setZoomLevel", std::move(params));
  pref_delegate_->PutZoomControlAccess(false);
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ZOOM_DISABLED));
}

/**
 * @tc.name: SetZoomLevel_NullBrowser_ErrorCode_0100
 * @tc.desc: setZoomLevel with null browser returns WEB_NWEB_NULL(132) (AC-1.9).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_NullBrowser_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("zoomLevel", 1.5);
  std::string cmd = BuildCommand("setZoomLevel", std::move(params));
  dispatcher_->Dispatch(cmd, nullptr, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}

/**
 * @tc.name: SetZoomLevel_ParamsNotDict_ErrorCode_0100
 * @tc.desc: setZoomLevel with non-dict params ("1.5" string) returns
 *           JSON_IS_INVALID(110) (AC-1.10). Host not invoked.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, SetZoomLevel_ParamsNotDict_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).Times(0);
  std::string cmd =
      R"({"method":"setZoomLevel","params":"1.5"})";
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::JSON_IS_INVALID));
}

// ===== getZoomLevel happy path =====

/**
 * @tc.name: GetZoomLevel_Success_ReturnsZoomLevel_0100
 * @tc.desc: getZoomLevel returns code=SUCCESS with top-level zoomLevel (AC-2.1).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, GetZoomLevel_Success_ReturnsZoomLevel_0100) {
  // CEF returns a zoom level; the handler converts to factor via
  // blink::ZoomLevelToZoomFactor. Use level 0.0 (factor 1.0) for stability.
  EXPECT_CALL(*mock_browser_, GetZoomLevel()).WillOnce(Return(0.0));

  std::string cmd = BuildCommand("getZoomLevel");
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  // zoomLevel sits at top level (no data wrapper).
  auto val =
      base::JSONReader::Read(callback_->value_, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(val && val->is_dict());
  EXPECT_FALSE(val->GetDict().FindDict("data"));
  auto zoom = val->GetDict().FindDouble("zoomLevel");
  ASSERT_TRUE(zoom.has_value());
  EXPECT_NEAR(*zoom, 1.0, 0.001);
}

/**
 * @tc.name: GetZoomLevel_SetThenGet_0100
 * @tc.desc: After set(2.0), getZoomLevel returns ~2.0 (factor↔level round-trip).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, GetZoomLevel_SetThenGet_0100) {
  double stored_level;
  EXPECT_CALL(*mock_browser_, SetZoomLevel(_))
      .WillOnce(testing::SaveArg<0>(&stored_level));
  base::Value::Dict set_params;
  set_params.Set("zoomLevel", 2.0);
  std::string set_cmd = BuildCommand("setZoomLevel", std::move(set_params));
  dispatcher_->Dispatch(set_cmd, mock_browser_, callback_);

  testing::Mock::VerifyAndClearExpectations(mock_browser_.get());
  auto cb2 = std::make_shared<TestStringValueCallback>();
  EXPECT_CALL(*mock_browser_, GetZoomLevel()).WillOnce(Return(stored_level));
  std::string get_cmd = BuildCommand("getZoomLevel");
  dispatcher_->Dispatch(get_cmd, mock_browser_, cb2);

  auto val =
      base::JSONReader::Read(cb2->value_, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(val && val->is_dict());
  EXPECT_FALSE(val->GetDict().FindDict("data"));
  auto zoom = val->GetDict().FindDouble("zoomLevel");
  ASSERT_TRUE(zoom.has_value());
  EXPECT_NEAR(*zoom, 2.0, 0.001);
}

/**
 * @tc.name: GetZoomLevel_ParamsVariants_0100
 * @tc.desc: getZoomLevel ignores params entirely; any shape returns SUCCESS (AC-2.4).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, GetZoomLevel_ParamsVariants_0100) {
  const std::vector<std::string> forms = {
      R"({"method":"getZoomLevel"})",               // no params
      R"({"method":"getZoomLevel","params":{}})",   // empty dict
      R"({"method":"getZoomLevel","params":null})", // null
      R"({"method":"getZoomLevel","params":"x"})",  // string
      R"({"method":"getZoomLevel","params":[1]})",  // list
      R"({"method":"getZoomLevel","params":42})",   // number
  };
  for (const auto& raw : forms) {
    EXPECT_CALL(*mock_browser_, GetZoomLevel()).WillOnce(Return(0.0));
    auto cb = std::make_shared<TestStringValueCallback>();
    dispatcher_->Dispatch(raw, mock_browser_, cb);
    EXPECT_TRUE(cb->called_) << "raw: " << raw;
    EXPECT_EQ(ExtractCodeFromJson(cb->value_),
              static_cast<int32_t>(WebCommandErrorCode::SUCCESS))
        << "raw: " << raw;
    testing::Mock::VerifyAndClearExpectations(mock_browser_.get());
  }
}

/**
 * @tc.name: GetZoomLevel_NullBrowser_ErrorCode_0100
 * @tc.desc: getZoomLevel with null browser returns WEB_NWEB_NULL(132) (AC-2.5).
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, GetZoomLevel_NullBrowser_ErrorCode_0100) {
  std::string cmd = BuildCommand("getZoomLevel");
  dispatcher_->Dispatch(cmd, nullptr, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}

// ===== default-path compatibility =====

/**
 * @tc.name: Dispatch_NoProvider_DefaultsAllowZoom_0100
 * @tc.desc: With no NWebPreferenceDelegate wired (null pref_), defaults
 *           (zoom_control_access=true) let zoom proceed.
 *           Existing non-zoom commands remain unaffected by the accessor wiring.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest,
       Dispatch_NoProvider_DefaultsAllowZoom_0100) {
  // Dispatcher with no pref — defaults allow zoom.
  auto no_pref_dispatcher =
      std::make_unique<AIPageCommandDispatcher>(base::WeakPtr<NWebDelegate>{});
  EXPECT_CALL(*mock_browser_, SetZoomLevel(_)).WillOnce(Return());
  base::Value::Dict params;
  params.Set("zoomLevel", 1.5);
  std::string cmd = BuildCommand("setZoomLevel", std::move(params));
  no_pref_dispatcher->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  // scroll still returns WEB_NWEB_NULL with null browser.
  testing::Mock::VerifyAndClearExpectations(mock_browser_.get());
  auto cb2 = std::make_shared<TestStringValueCallback>();
  base::Value::Dict scroll_params;
  scroll_params.Set("x", 100.0);
  scroll_params.Set("y", 200.0);
  std::string cmd2 = BuildCommand("scroll", std::move(scroll_params));
  no_pref_dispatcher->Dispatch(cmd2, nullptr, cb2);
  EXPECT_TRUE(cb2->called_);
  EXPECT_EQ(ExtractCodeFromJson(cb2->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}

// ===== ScreenCapture command tests =====
 
/**
 * @tc.name: ScreenCapture_NullBrowser_ErrorCode_0100
 * @tc.desc: ScreenCapture with null browser returns WEB_NWEB_NULL error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_NullBrowser_ErrorCode_0100) {
  base::Value::Dict params;
  params.Set("node_id", 1);
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  dispatcher_->Dispatch(cmd, nullptr, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}
 
/**
 * @tc.name: ScreenCapture_InvalidParamType_ErrorCode_0100
 * @tc.desc: ScreenCapture with non-double x param returns PARAM_INVALID error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_InvalidParamType_ErrorCode_0100) {
  base::Value::Dict params;
  base::Value::Dict clip;
  clip.Set("x", "not_a_number");
  clip.Set("y", 200.0);
  clip.Set("width", 100.0);
  clip.Set("height", 100.0);
  clip.Set("scale", 1.0);
  params.Set("clip", std::move(clip));
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}
 
/**
 * @tc.name: ScreenCapture_InvalidParamType_ErrorCode_0100
 * @tc.desc: ScreenCapture with missing x param returns PARAM_INVALID error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_MissingParam_ErrorCode_0100) {
  base::Value::Dict params;
  base::Value::Dict clip;
  clip.Set("y", 200.0);
  clip.Set("width", 100.0);
  clip.Set("height", 100.0);
  clip.Set("scale", 1.0);
  params.Set("clip", std::move(clip));
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}
 
/**
 * @tc.name: ScreenCapture_ObserverRegistrationFailed_0100
 * @tc.desc: ScreenCapture AddDevToolsMessageObserver returns null reports CDP_SESSION_NOT_READY error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_ObserverRegistrationFailed_0100) {
  base::Value::Dict params;
  base::Value::Dict clip;
  clip.Set("x", 200.0);
  clip.Set("y", 200.0);
  clip.Set("width", 100.0);
  clip.Set("height", 100.0);
  clip.Set("scale", 1.0);
  params.Set("clip", std::move(clip));
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SESSION_NOT_READY));
}
 
/**
 * @tc.name: ScreenCapture_CDPFailed_ErrorCode_0100
 * @tc.desc: ScreenCapture when CDP returns 0 returns CDP_SEND_FAILED error.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_CDPFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_, ExecuteDevToolsMethod(_, _, _))
      .WillOnce(Return(0));
 
  base::Value::Dict params;
  base::Value::Dict clip;
  clip.Set("x", 200.0);
  clip.Set("y", 200.0);
  clip.Set("width", 100.0);
  clip.Set("height", 100.0);
  clip.Set("scale", 1.0);
  params.Set("clip", std::move(clip));
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}
 
/**
 * @tc.name: ScreenCapture_Success_0100
 * @tc.desc: ScreenCapture with valid params and successful CDP.
 * @tc.type: FUNC
 */
TEST_F(AIPageCommandDispatcherTest, ScreenCapture_Success_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_, ExecuteDevToolsMethod(_, _, _))
      .WillOnce(Return(1));
 
  base::Value::Dict params;
  base::Value::Dict clip;
  clip.Set("x", 200.0);
  clip.Set("y", 200.0);
  clip.Set("width", 100.0);
  clip.Set("height", 100.0);
  clip.Set("scale", 1.0);
  params.Set("clip", std::move(clip));
  std::string cmd = BuildCommand("screenCapture", std::move(params));
  dispatcher_->Dispatch(cmd, mock_browser_, callback_);
  EXPECT_FALSE(callback_->called_);
}
