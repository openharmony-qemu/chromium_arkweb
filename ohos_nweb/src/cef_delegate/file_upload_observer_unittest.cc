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
#include "cef/include/cef_devtools_message_observer.h"
#include "cef/include/cef_registration.h"

// Access private members of FileUploadObserver for testing.
#define private public
#include "cef_delegate/file_upload_observer.h"
#undef private

#include "cef_delegate/test/mock_browser_host.h"
#include "nweb_errors.h"

using namespace testing;
using namespace OHOS::NWeb;
using OHOS::NWeb::test::MockBrowserHost;
using OHOS::NWeb::test::TestStringValueCallback;
using OHOS::NWeb::test::FakeRegistration;
using OHOS::NWeb::test::ExtractCodeFromJson;
using OHOS::NWeb::test::ExtractMessageFromJson;

// ===== Fixture =====

class FileUploadObserverTest : public Test {
 protected:
  void SetUp() override {
    mock_browser_ = new MockBrowserHost();
    callback_ = std::make_shared<TestStringValueCallback>();
  }

  void TearDown() override {
    callback_.reset();
    mock_browser_ = nullptr;
  }

  // Creates a FileUploadObserver, registers it via AddDevToolsMessageObserver,
  // and starts the CDP chain by issuing DOM.enable (returns msg_id=100).
  //
  // Lifecycle: the returned CefRefPtr keeps the observer alive.
  // Finish() internally creates a CefRefPtr self(this) which increments
  // the ref count; the outer CefRefPtr prevents premature deletion.
  CefRefPtr<FileUploadObserver> CreateStartedObserver(
      const std::string& xpath = "//input[@type='file']",
      const std::vector<std::string>& files = {"/tmp/a.txt",
                                                "/tmp/b.txt"}) {
    std::vector<std::string> no_warnings;
    auto obs = CefRefPtr<FileUploadObserver>(
        new FileUploadObserver(
            mock_browser_, xpath, files, no_warnings, callback_));

    EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
        .WillOnce(Return(new FakeRegistration()));
    EXPECT_CALL(*mock_browser_,
                ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
        .WillOnce(Return(100));

    obs->Start();
    EXPECT_EQ(obs->enable_msg_id_, 100);
    return obs;
  }

  CefRefPtr<MockBrowserHost> mock_browser_;
  std::shared_ptr<TestStringValueCallback> callback_;
};

// ===== Constructor =====

/**
 * @tc.name: Constructor_InitializesFields_0100
 * @tc.desc: Verify constructor stores all parameters correctly.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, Constructor_InitializesFields_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> warnings = {"warn1"};
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input", files, warnings, callback_));

  EXPECT_EQ(obs->xpath_, "//input");
  EXPECT_EQ(obs->files_.size(), 1u);
  EXPECT_EQ(obs->files_[0], "/tmp/a.txt");
  EXPECT_EQ(obs->warnings_.size(), 1u);
  EXPECT_EQ(obs->warnings_[0], "warn1");
  EXPECT_EQ(obs->callback_, callback_);
  EXPECT_EQ(obs->enable_msg_id_, 0);
  EXPECT_EQ(obs->get_doc_msg_id_, 0);
  EXPECT_EQ(obs->search_msg_id_, 0);
  EXPECT_EQ(obs->get_results_msg_id_, 0);
  EXPECT_EQ(obs->upload_msg_id_, 0);
  EXPECT_TRUE(obs->search_id_.empty());
}

// ===== Start =====

/**
 * @tc.name: Start_ObserverRegistrationFailed_ErrorCode_0100
 * @tc.desc: Start when AddDevToolsMessageObserver returns null reports
 *           CDP_SESSION_NOT_READY.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       Start_ObserverRegistrationFailed_ErrorCode_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input", files, no_warnings, callback_));

  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(CefRefPtr<CefRegistration>()));

  obs->Start();
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SESSION_NOT_READY));
}

/**
 * @tc.name: Start_DOMEnableSendFailed_ErrorCode_0100
 * @tc.desc: Start when ExecuteDevToolsMethod(DOM.enable) returns 0 reports
 *           CDP_SEND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       Start_DOMEnableSendFailed_ErrorCode_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input", files, no_warnings, callback_));

  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(0));

  obs->Start();
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

/**
 * @tc.name: Start_Success_SetsEnableMsgId_0100
 * @tc.desc: Start succeeds, enable_msg_id_ is set to non-zero.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, Start_Success_SetsEnableMsgId_0100) {
  auto obs = CreateStartedObserver();
  EXPECT_EQ(obs->enable_msg_id_, 100);
  // callback should NOT have been called yet (async chain)
  EXPECT_FALSE(callback_->called_);
}

// ===== OnDevToolsMethodResult routing =====

/**
 * @tc.name: OnDevToolsMethodResult_MsgIdZero_Ignored_0100
 * @tc.desc: message_id==0 is silently ignored.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       OnDevToolsMethodResult_MsgIdZero_Ignored_0100) {
  auto obs = CreateStartedObserver();
  obs->OnDevToolsMethodResult(nullptr, 0, true, nullptr, 0);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: OnDevToolsMethodResult_UnknownMsgId_Ignored_0100
 * @tc.desc: An unrecognised message_id is ignored.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       OnDevToolsMethodResult_UnknownMsgId_Ignored_0100) {
  auto obs = CreateStartedObserver();
  obs->OnDevToolsMethodResult(nullptr, 99999, true, nullptr, 0);
  EXPECT_FALSE(callback_->called_);
}

// ===== HandleEnableResult =====

/**
 * @tc.name: HandleEnableResult_Failure_Finishes_0100
 * @tc.desc: DOM.enable failure triggers Finish with CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleEnableResult_Failure_Finishes_0100) {
  auto obs = CreateStartedObserver();
  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, obs->enable_msg_id_,
                              false /* success */, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleEnableResult_Success_SendsGetDocument_0100
 * @tc.desc: DOM.enable success triggers DOM.getDocument.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleEnableResult_Success_SendsGetDocument_0100) {
  auto obs = CreateStartedObserver();

  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, obs->enable_msg_id_,
                              true /* success */, response.data(),
                              response.size());

  EXPECT_EQ(obs->get_doc_msg_id_, 200);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: HandleEnableResult_Success_GetDocSendFailed_0100
 * @tc.desc: DOM.enable success but DOM.getDocument send fails.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleEnableResult_Success_GetDocSendFailed_0100) {
  auto obs = CreateStartedObserver();

  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(0));

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, obs->enable_msg_id_,
                              true, response.data(), response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

// ===== HandleGetDocResult =====

/**
 * @tc.name: HandleGetDocResult_Failure_Finishes_0100
 * @tc.desc: DOM.getDocument failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleGetDocResult_Failure_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->get_doc_msg_id_ = 200;

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 200, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleGetDocResult_Success_StartsXPathSearch_0100
 * @tc.desc: DOM.getDocument success with non-empty xpath starts
 *           DOM.performSearch.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleGetDocResult_Success_StartsXPathSearch_0100) {
  auto obs = CreateStartedObserver();
  obs->get_doc_msg_id_ = 200;

  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 200, true, response.data(),
                              response.size());
  EXPECT_EQ(obs->search_msg_id_, 300);
  EXPECT_FALSE(callback_->called_);
}

// ===== HandleSearchResult =====

/**
 * @tc.name: HandleSearchResult_Failure_Finishes_0100
 * @tc.desc: DOM.performSearch failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleSearchResult_Failure_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->search_msg_id_ = 300;

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 300, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleSearchResult_InvalidJSON_Finishes_0100
 * @tc.desc: DOM.performSearch returns invalid JSON triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleSearchResult_InvalidJSON_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->search_msg_id_ = 300;

  std::string response = "not json at all";
  obs->OnDevToolsMethodResult(nullptr, 300, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleSearchResult_NoSearchId_Finishes_0100
 * @tc.desc: DOM.performSearch response missing searchId triggers
 *           ELEMENT_NOT_FOUND.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleSearchResult_NoSearchId_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->search_msg_id_ = 300;

  std::string response = R"({"resultCount":0})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

/**
 * @tc.name: HandleSearchResult_ZeroResultCount_Finishes_0100
 * @tc.desc: DOM.performSearch with resultCount=0 triggers ELEMENT_NOT_FOUND.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleSearchResult_ZeroResultCount_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->search_msg_id_ = 300;

  std::string response = R"({"searchId":"id1","resultCount":0})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

/**
 * @tc.name: HandleSearchResult_Success_SendsGetSearchResults_0100
 * @tc.desc: DOM.performSearch success with valid searchId sends
 *           DOM.getSearchResults.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleSearchResult_Success_SendsGetSearchResults_0100) {
  auto obs = CreateStartedObserver();
  obs->search_msg_id_ = 300;

  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));

  std::string response = R"({"searchId":"search123","resultCount":1})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, response.data(),
                              response.size());

  EXPECT_EQ(obs->search_id_, "search123");
  EXPECT_EQ(obs->get_results_msg_id_, 400);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: HandleSearchResult_GetSearchResultsSendFailed_0100
 * @tc.desc: DOM.performSearch succeeds but getSearchResults send fails.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleSearchResult_GetSearchResultsSendFailed_0100) {
  auto obs = CreateStartedObserver();
  obs->search_msg_id_ = 300;

  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(0));

  std::string response = R"({"searchId":"search123","resultCount":1})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, response.data(),
                              response.size());

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

// ===== HandleGetResultsResult =====

/**
 * @tc.name: HandleGetResultsResult_Failure_Finishes_0100
 * @tc.desc: DOM.getSearchResults failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleGetResultsResult_Failure_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->get_results_msg_id_ = 400;
  obs->search_id_ = "search123";

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 400, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleGetResultsResult_InvalidJSON_Finishes_0100
 * @tc.desc: Invalid JSON in getSearchResults triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleGetResultsResult_InvalidJSON_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->get_results_msg_id_ = 400;
  obs->search_id_ = "search123";

  std::string response = "bad json";
  obs->OnDevToolsMethodResult(nullptr, 400, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleGetResultsResult_EmptyNodeIds_Finishes_0100
 * @tc.desc: Empty nodeIds list triggers ELEMENT_NOT_FOUND.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleGetResultsResult_EmptyNodeIds_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->get_results_msg_id_ = 400;
  obs->search_id_ = "search123";

  std::string response = R"({"nodeIds":[]})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

/**
 * @tc.name: HandleGetResultsResult_NoNodeIdsKey_Finishes_0100
 * @tc.desc: Missing nodeIds key triggers ELEMENT_NOT_FOUND.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleGetResultsResult_NoNodeIdsKey_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->get_results_msg_id_ = 400;
  obs->search_id_ = "search123";

  std::string response = R"({"other":"data"})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

/**
 * @tc.name: HandleGetResultsResult_NonIntNodeId_Finishes_0100
 * @tc.desc: First nodeId is not int triggers ELEMENT_NOT_FOUND.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleGetResultsResult_NonIntNodeId_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->get_results_msg_id_ = 400;
  obs->search_id_ = "search123";

  std::string response = R"({"nodeIds":["not_int"]})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

/**
 * @tc.name: HandleGetResultsResult_Success_SendsSetFileInputFiles_0100
 * @tc.desc: Valid nodeId triggers DOM.discardSearchResults +
 *           DOM.setFileInputFiles.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleGetResultsResult_Success_SendsSetFileInputFiles_0100) {
  auto obs = CreateStartedObserver();
  obs->get_results_msg_id_ = 400;
  obs->search_id_ = "search123";

  // Expect discardSearchResults + setFileInputFiles
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.setFileInputFiles"), _))
      .WillOnce(Return(500));

  std::string response = R"({"nodeIds":[42]})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, response.data(),
                              response.size());

  EXPECT_EQ(obs->upload_msg_id_, 500);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: HandleGetResultsResult_SetFileInputFilesSendFailed_0100
 * @tc.desc: DOM.setFileInputFiles returns 0 triggers CDP_SEND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleGetResultsResult_SetFileInputFilesSendFailed_0100) {
  auto obs = CreateStartedObserver();
  obs->get_results_msg_id_ = 400;
  obs->search_id_ = "search123";

  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.setFileInputFiles"), _))
      .WillOnce(Return(0));

  std::string response = R"({"nodeIds":[42]})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, response.data(),
                              response.size());

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

// ===== HandleUploadResult =====

/**
 * @tc.name: HandleUploadResult_Failure_Finishes_0100
 * @tc.desc: DOM.setFileInputFiles failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleUploadResult_Failure_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->upload_msg_id_ = 500;

  std::string response = R"({"error":"something"})";
  obs->OnDevToolsMethodResult(nullptr, 500, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleUploadResult_Success_Finishes_0100
 * @tc.desc: DOM.setFileInputFiles success triggers Finish with SUCCESS code.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest,
       HandleUploadResult_Success_Finishes_0100) {
  auto obs = CreateStartedObserver();
  obs->upload_msg_id_ = 500;

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 500, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

// ===== Finish =====

/**
 * @tc.name: Finish_NullCallback_NoCrash_0100
 * @tc.desc: Finish with null callback does not crash and clears state.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, Finish_NullCallback_NoCrash_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input", files, no_warnings, nullptr));

  // Should not crash
  obs->Finish(0, "test");
}

/**
 * @tc.name: Finish_WithWarnings_AppendsToMessage_0100
 * @tc.desc: Finish appends warnings to the message field.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, Finish_WithWarnings_AppendsToMessage_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> warnings = {"file missing", "not readable"};
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input", files, warnings, callback_));

  obs->Finish(static_cast<int32_t>(WebCommandErrorCode::SUCCESS), "success");

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  std::string msg = ExtractMessageFromJson(callback_->value_);
  EXPECT_NE(msg.find("file missing"), std::string::npos);
  EXPECT_NE(msg.find("not readable"), std::string::npos);
  EXPECT_NE(msg.find("warnings:"), std::string::npos);
}

/**
 * @tc.name: Finish_NoWarnings_JustMessage_0100
 * @tc.desc: Finish without warnings contains only the base message.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, Finish_NoWarnings_JustMessage_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input", files, no_warnings, callback_));

  obs->Finish(static_cast<int32_t>(WebCommandErrorCode::SUCCESS), "success");

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractMessageFromJson(callback_->value_), "success");
}

/**
 * @tc.name: Finish_ClearsState_0100
 * @tc.desc: Finish sets callback_, browser_, registration_ to null.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, Finish_ClearsState_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input", files, no_warnings, callback_));

  obs->Finish(0, "test");

  EXPECT_EQ(obs->callback_, nullptr);
  EXPECT_EQ(obs->browser_, nullptr);
  EXPECT_EQ(obs->registration_, nullptr);
}

/**
 * @tc.name: Finish_CalledTwice_SecondCallNoCrash_0100
 * @tc.desc: Calling Finish twice does not crash (callback already null).
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, Finish_CalledTwice_SecondCallNoCrash_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input", files, no_warnings, callback_));

  obs->Finish(0, "first");
  EXPECT_TRUE(callback_->called_);

  // Second call — callback_ is already null inside observer, should not crash
  obs->Finish(0, "second");
}

// ===== Full CDP chain end-to-end =====

/**
 * @tc.name: FullCDPChain_Success_0100
 * @tc.desc: Simulate the complete CDP call chain: enable → getDoc → search →
 *           getResults → upload → success.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, FullCDPChain_Success_0100) {
  std::vector<std::string> files = {"/tmp/a.txt", "/tmp/b.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input[@type='file']", files, no_warnings, callback_));

  // Step 1: Start
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  obs->Start();

  // Step 2: HandleEnableResult → triggers getDocument
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  std::string enable_resp = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, true, enable_resp.data(),
                              enable_resp.size());

  // Step 3: HandleGetDocResult → triggers performSearch
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  std::string doc_resp = "{}";
  obs->OnDevToolsMethodResult(nullptr, 200, true, doc_resp.data(),
                              doc_resp.size());

  // Step 4: HandleSearchResult → triggers getSearchResults
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  std::string search_resp = R"({"searchId":"sid1","resultCount":1})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, search_resp.data(),
                              search_resp.size());

  // Step 5: HandleGetResultsResult → triggers discard + setFileInputFiles
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.setFileInputFiles"), _))
      .WillOnce(Return(500));
  std::string results_resp = R"({"nodeIds":[42]})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, results_resp.data(),
                              results_resp.size());

  // Step 6: HandleUploadResult → Finish with SUCCESS
  std::string upload_resp = "{}";
  obs->OnDevToolsMethodResult(nullptr, 500, true, upload_resp.data(),
                              upload_resp.size());

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ(ExtractMessageFromJson(callback_->value_), "success");
}

/**
 * @tc.name: FullCDPChain_EnableFails_0100
 * @tc.desc: Full chain where DOM.enable fails at the first step.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, FullCDPChain_EnableFails_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input[@type='file']", files, no_warnings, callback_));

  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  obs->Start();

  std::string resp = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, false, resp.data(), resp.size());

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: FullCDPChain_SearchNoResults_0100
 * @tc.desc: Full chain where performSearch returns 0 results.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, FullCDPChain_SearchNoResults_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input[@type='file']", files, no_warnings, callback_));

  // Start
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  obs->Start();

  // Enable success → getDocument
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  std::string resp1 = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, true, resp1.data(), resp1.size());

  // getDocument success → performSearch
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  std::string resp2 = "{}";
  obs->OnDevToolsMethodResult(nullptr, 200, true, resp2.data(), resp2.size());

  // performSearch returns 0 results → ELEMENT_NOT_FOUND
  std::string resp3 = R"({"searchId":"sid1","resultCount":0})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, resp3.data(), resp3.size());

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

/**
 * @tc.name: FullCDPChain_UploadFails_0100
 * @tc.desc: Full chain where setFileInputFiles returns failure.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, FullCDPChain_UploadFails_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> no_warnings;
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input[@type='file']", files, no_warnings, callback_));

  // Start
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  obs->Start();

  // Enable → getDocument
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  std::string r1 = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, true, r1.data(), r1.size());

  // getDocument → performSearch
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  std::string r2 = "{}";
  obs->OnDevToolsMethodResult(nullptr, 200, true, r2.data(), r2.size());

  // performSearch → getSearchResults
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  std::string r3 = R"({"searchId":"s1","resultCount":1})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, r3.data(), r3.size());

  // getSearchResults → discard + setFileInputFiles
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.setFileInputFiles"), _))
      .WillOnce(Return(500));
  std::string r4 = R"({"nodeIds":[42]})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, r4.data(), r4.size());

  // setFileInputFiles failure
  std::string r5 = R"({"error":"denied"})";
  obs->OnDevToolsMethodResult(nullptr, 500, false, r5.data(), r5.size());

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: FullCDPChain_WithWarnings_0100
 * @tc.desc: Full chain with warnings — Finish message includes warnings.
 * @tc.type: FUNC
 */
TEST_F(FileUploadObserverTest, FullCDPChain_WithWarnings_0100) {
  std::vector<std::string> files = {"/tmp/a.txt"};
  std::vector<std::string> warnings = {"path does not exist: /tmp/a.txt"};
  auto obs = CefRefPtr<FileUploadObserver>(
      new FileUploadObserver(
          mock_browser_, "//input[@type='file']", files, warnings, callback_));

  // Start
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  obs->Start();

  // Enable → getDocument
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  std::string r1 = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, true, r1.data(), r1.size());

  // getDocument → performSearch
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  std::string r2 = "{}";
  obs->OnDevToolsMethodResult(nullptr, 200, true, r2.data(), r2.size());

  // performSearch → getSearchResults
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  std::string r3 = R"({"searchId":"s1","resultCount":1})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, r3.data(), r3.size());

  // getSearchResults → discard + setFileInputFiles
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.setFileInputFiles"), _))
      .WillOnce(Return(500));
  std::string r4 = R"({"nodeIds":[1]})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, r4.data(), r4.size());

  // setFileInputFiles success
  std::string r5 = "{}";
  obs->OnDevToolsMethodResult(nullptr, 500, true, r5.data(), r5.size());

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  std::string msg = ExtractMessageFromJson(callback_->value_);
  EXPECT_NE(msg.find("warnings:"), std::string::npos);
  EXPECT_NE(msg.find("path does not exist"), std::string::npos);
}
