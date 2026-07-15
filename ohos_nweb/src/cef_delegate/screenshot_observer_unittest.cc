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

#define private public
#include "cef_delegate/screenshot_observer.h"
#undef private

#include "cef_delegate/screenshot_capture_util.h"
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

class ScreenshotObserverTest : public Test {
 protected:
  void SetUp() override {
    mock_browser_ = new NiceMock<MockBrowserHost>();
    callback_ = std::make_shared<TestStringValueCallback>();
    ON_CALL(*mock_browser_, ExecuteDevToolsMethod(_, _, _))
        .WillByDefault(Return(0));
  }

  void TearDown() override {
    callback_.reset();
    mock_browser_ = nullptr;
  }

  CefRefPtr<ScreenshotObserver> CreateObserver() {
    return CefRefPtr<ScreenshotObserver>(
        new ScreenshotObserver(mock_browser_, callback_));
  }

  CefRefPtr<NiceMock<MockBrowserHost>> mock_browser_;
  std::shared_ptr<TestStringValueCallback> callback_;
};

// ===== Constructor =====

/**
 * @tc.name: Constructor_InitializesFields_0100
 * @tc.desc: Verify constructor stores browser and callback correctly.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, Constructor_InitializesFields_0100) {
  auto obs = CreateObserver();
  EXPECT_TRUE(obs);
  EXPECT_EQ(obs->browser_.get(), mock_browser_.get());
  EXPECT_EQ(obs->callback_.get(), callback_.get());
  EXPECT_TRUE(obs->xpath_.empty());
  EXPECT_TRUE(obs->search_id_.empty());
  EXPECT_EQ(obs->node_id_, 0);
  EXPECT_EQ(obs->resolved_node_id_, 0);
  EXPECT_EQ(obs->enable_msg_id_, 0);
  EXPECT_EQ(obs->get_document_msg_id_, 0);
  EXPECT_EQ(obs->perform_search_msg_id_, 0);
  EXPECT_EQ(obs->get_search_results_msg_id_, 0);
  EXPECT_EQ(obs->get_box_model_msg_id_, 0);
  EXPECT_EQ(obs->layout_metrics_msg_id_, 0);
  EXPECT_EQ(obs->screenshot_msg_id_, 0);
}

// ===== CaptureFullPage =====

/**
 * @tc.name: CaptureFullPage_NullBrowser_ErrorCode_0100
 * @tc.desc: CaptureFullPage with null browser returns WEB_NWEB_NULL.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, CaptureFullPage_NullBrowser_ErrorCode_0100) {
  CefRefPtr<ScreenshotObserver> obs(new ScreenshotObserver(nullptr, callback_));
  obs->CaptureFullPage();
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}

/**
 * @tc.name: CaptureFullPage_ObserverRegistrationFailed_ErrorCode_0100
 * @tc.desc: CaptureFullPage when AddDevToolsMessageObserver returns null.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CaptureFullPage_ObserverRegistrationFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(nullptr));

  auto obs = CreateObserver();
  obs->CaptureFullPage();
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SESSION_NOT_READY));
}

/**
 * @tc.name: CaptureFullPage_CDPSendFailed_ErrorCode_0100
 * @tc.desc: CaptureFullPage when Page.captureScreenshot returns 0.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, CaptureFullPage_CDPSendFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.captureScreenshot"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureFullPage();
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

/**
 * @tc.name: CaptureFullPage_Success_Async_0100
 * @tc.desc: CaptureFullPage sends Page.captureScreenshot and waits.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, CaptureFullPage_Success_Async_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.captureScreenshot"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->CaptureFullPage();
  EXPECT_EQ(obs->screenshot_msg_id_, 100);
  EXPECT_FALSE(callback_->called_);
}

// ===== CaptureElement =====

/**
 * @tc.name: CaptureElement_NullBrowser_ErrorCode_0100
 * @tc.desc: CaptureElement with null browser returns WEB_NWEB_NULL.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, CaptureElement_NullBrowser_ErrorCode_0100) {
  CefRefPtr<ScreenshotObserver> obs(
      new ScreenshotObserver(nullptr, callback_));
  obs->CaptureElement("//div");
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}

/**
 * @tc.name: CaptureElement_ObserverRegistrationFailed_ErrorCode_0100
 * @tc.desc: CaptureElement when AddDevToolsMessageObserver returns null.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CaptureElement_ObserverRegistrationFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(nullptr));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SESSION_NOT_READY));
}

/**
 * @tc.name: CaptureElement_DOMEnableSendFailed_ErrorCode_0100
 * @tc.desc: CaptureElement when DOM.enable returns 0.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CaptureElement_DOMEnableSendFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

/**
 * @tc.name: CaptureElement_Success_Async_0100
 * @tc.desc: CaptureElement sends DOM.enable and waits (async).
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, CaptureElement_Success_Async_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");
  EXPECT_EQ(obs->enable_msg_id_, 100);
  EXPECT_FALSE(callback_->called_);
}

// ===== CaptureElementByNodeId =====

/**
 * @tc.name: CaptureElementByNodeId_NullBrowser_ErrorCode_0100
 * @tc.desc: CaptureElementByNodeId with null browser returns WEB_NWEB_NULL.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CaptureElementByNodeId_NullBrowser_ErrorCode_0100) {
  CefRefPtr<ScreenshotObserver> obs(
      new ScreenshotObserver(nullptr, callback_));
  obs->CaptureElementByNodeId(123);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::WEB_NWEB_NULL));
}

/**
 * @tc.name: CaptureElementByNodeId_InvalidNodeId_ErrorCode_0100
 * @tc.desc: CaptureElementByNodeId with node_id=0 returns PARAM_INVALID.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CaptureElementByNodeId_InvalidNodeId_ErrorCode_0100) {
  auto obs = CreateObserver();
  obs->CaptureElementByNodeId(0);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}

/**
 * @tc.name: CaptureElementByNodeId_NegativeNodeId_ErrorCode_0200
 * @tc.desc: CaptureElementByNodeId with negative node_id returns PARAM_INVALID.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CaptureElementByNodeId_NegativeNodeId_ErrorCode_0200) {
  auto obs = CreateObserver();
  obs->CaptureElementByNodeId(-1);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID));
}

/**
 * @tc.name: CaptureElementByNodeId_ObserverRegistrationFailed_ErrorCode_0100
 * @tc.desc: CaptureElementByNodeId when AddDevToolsMessageObserver returns null.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CaptureElementByNodeId_ObserverRegistrationFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(nullptr));

  auto obs = CreateObserver();
  obs->CaptureElementByNodeId(123);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SESSION_NOT_READY));
}

/**
 * @tc.name: CaptureElementByNodeId_GetBoxModelSendFailed_ErrorCode_0100
 * @tc.desc: CaptureElementByNodeId when DOM.getBoxModel returns 0.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CaptureElementByNodeId_GetBoxModelSendFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElementByNodeId(123);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

/**
 * @tc.name: CaptureElementByNodeId_Success_Async_0100
 * @tc.desc: CaptureElementByNodeId sends DOM.getBoxModel and waits.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, CaptureElementByNodeId_Success_Async_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->CaptureElementByNodeId(123);
  EXPECT_EQ(obs->get_box_model_msg_id_, 100);
  EXPECT_FALSE(callback_->called_);
}

// ===== OnDevToolsMethodResult =====

/**
 * @tc.name: OnDevToolsMethodResult_MsgIdZero_Ignored_0100
 * @tc.desc: OnDevToolsMethodResult with message_id=0 returns early.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       OnDevToolsMethodResult_MsgIdZero_Ignored_0100) {
  auto obs = CreateObserver();
  obs->OnDevToolsMethodResult(nullptr, 0, true, nullptr, 0);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: OnDevToolsMethodResult_UnknownMsgId_Ignored_0100
 * @tc.desc: OnDevToolsMethodResult with unknown message_id is ignored.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       OnDevToolsMethodResult_UnknownMsgId_Ignored_0100) {
  auto obs = CreateObserver();
  obs->OnDevToolsMethodResult(nullptr, 99999, true, nullptr, 0);
  EXPECT_FALSE(callback_->called_);
}

// ===== HandleEnableResult =====

/**
 * @tc.name: HandleEnableResult_Failure_Finishes_0100
 * @tc.desc: DOM.enable failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, HandleEnableResult_Failure_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, false, response.data(),
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
TEST_F(ScreenshotObserverTest,
       HandleEnableResult_Success_SendsGetDocument_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, true, response.data(),
                              response.size());

  EXPECT_EQ(obs->get_document_msg_id_, 200);
  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: HandleEnableResult_GetDocumentSendFailed_ErrorCode_0100
 * @tc.desc: DOM.getDocument send failure triggers CDP_SEND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleEnableResult_GetDocumentSendFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

// ===== HandleGetDocumentResult =====

/**
 * @tc.name: HandleGetDocumentResult_Failure_Finishes_0100
 * @tc.desc: DOM.getDocument failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetDocumentResult_Failure_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleGetDocumentResult_InvalidJSON_Finishes_0100
 * @tc.desc: DOM.getDocument with invalid JSON triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetDocumentResult_InvalidJSON_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);

  std::string response = "not json";
  obs->OnDevToolsMethodResult(nullptr, 200, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleGetDocumentResult_MissingRoot_Finishes_0100
 * @tc.desc: DOM.getDocument missing root field triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetDocumentResult_MissingRoot_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);

  std::string response = R"({"other":"data"})";
  obs->OnDevToolsMethodResult(nullptr, 200, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleGetDocumentResult_InvalidNodeId_Finishes_0100
 * @tc.desc: DOM.getDocument with invalid nodeId triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetDocumentResult_InvalidNodeId_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);

  std::string response = R"({"root":{"nodeId":0}})";
  obs->OnDevToolsMethodResult(nullptr, 200, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleGetDocumentResult_PerformSearchSendFailed_ErrorCode_0100
 * @tc.desc: DOM.performSearch send failure triggers CDP_SEND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetDocumentResult_PerformSearchSendFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);

  std::string response = R"({"root":{"nodeId":1}})";
  obs->OnDevToolsMethodResult(nullptr, 200, true, response.data(), 21);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

// ===== HandlePerformSearchResult =====

/**
 * @tc.name: HandlePerformSearchResult_Failure_Finishes_0100
 * @tc.desc: DOM.performSearch failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandlePerformSearchResult_Failure_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 300, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandlePerformSearchResult_ZeroResults_Finishes_0100
 * @tc.desc: DOM.performSearch with zero results triggers element not found.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandlePerformSearchResult_ZeroResults_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);

  std::string response = R"({"searchId":"abc","resultCount":0})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandlePerformSearchResult_InvalidJSON_Finishes_0100
 * @tc.desc: DOM.performSearch with invalid JSON triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandlePerformSearchResult_InvalidJSON_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);

  std::string response = "not json";
  obs->OnDevToolsMethodResult(nullptr, 300, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandlePerformSearchResult_GetSearchResultsSendFailed_ErrorCode_0100
 * @tc.desc: DOM.getSearchResults send failure triggers CDP_SEND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandlePerformSearchResult_GetSearchResultsSendFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);

  std::string response = R"({"searchId":"abc","resultCount":1})";
  obs->OnDevToolsMethodResult(nullptr, 300, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

// ===== HandleGetSearchResultsResult =====

/**
 * @tc.name: HandleGetSearchResultsResult_Failure_Finishes_0100
 * @tc.desc: DOM.getSearchResults failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetSearchResultsResult_Failure_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 400, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleGetSearchResultsResult_EmptyNodeIds_Finishes_0100
 * @tc.desc: DOM.getSearchResults with empty nodeIds triggers element not found.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetSearchResultsResult_EmptyNodeIds_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);

  std::string response = R"({"nodeIds":[]})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleGetSearchResultsResult_InvalidJSON_Finishes_0100
 * @tc.desc: DOM.getSearchResults with invalid JSON triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetSearchResultsResult_InvalidJSON_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);

  std::string response = "not json";
  obs->OnDevToolsMethodResult(nullptr, 400, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleGetSearchResultsResult_InvalidNodeId_Finishes_0100
 * @tc.desc: DOM.getSearchResults with invalid nodeId triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetSearchResultsResult_InvalidNodeId_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);

  std::string response = R"({"nodeIds":[0]})";
  obs->OnDevToolsMethodResult(nullptr, 400, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

// ===== HandleGetBoxModelResult =====

/**
 * @tc.name: HandleGetBoxModelResult_Failure_Finishes_0100
 * @tc.desc: DOM.getBoxModel failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, HandleGetBoxModelResult_Failure_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(500));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);
  obs->OnDevToolsMethodResult(nullptr, 400, true, R"({"nodeIds":[123]})", 17);

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 500, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleGetBoxModelResult_InvalidJSON_Finishes_0100
 * @tc.desc: DOM.getBoxModel with invalid JSON triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetBoxModelResult_InvalidJSON_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(500));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);
  obs->OnDevToolsMethodResult(nullptr, 400, true, R"({"nodeIds":[123]})", 17);

  std::string response = "not json";
  obs->OnDevToolsMethodResult(nullptr, 500, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleGetBoxModelResult_MissingModel_Finishes_0100
 * @tc.desc: DOM.getBoxModel missing model field triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetBoxModelResult_MissingModel_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(500));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);
  obs->OnDevToolsMethodResult(nullptr, 400, true, R"({"nodeIds":[123]})", 17);

  std::string response = R"({"other":"data"})";
  obs->OnDevToolsMethodResult(nullptr, 500, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleGetBoxModelResult_InvalidBorder_Finishes_0100
 * @tc.desc: DOM.getBoxModel with invalid border data triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetBoxModelResult_InvalidBorder_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(500));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);
  obs->OnDevToolsMethodResult(nullptr, 400, true, R"({"nodeIds":[123]})", 17);

  std::string response = R"({"model":{"border":[1]}})";
  obs->OnDevToolsMethodResult(nullptr, 500, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleGetBoxModelResult_LayoutMetricsSendFailed_ErrorCode_0100
 * @tc.desc: Page.getLayoutMetrics send failure triggers CDP_SEND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleGetBoxModelResult_LayoutMetricsSendFailed_ErrorCode_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(500));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.getLayoutMetrics"), _))
      .WillOnce(Return(600));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);
  obs->OnDevToolsMethodResult(nullptr, 400, true, R"({"nodeIds":[123]})", 17);

  std::string response = R"({"model":{"border":[0,0,100,0,100,100,0,100]}})";
  obs->OnDevToolsMethodResult(nullptr, 500, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_SEND_FAILED));
}

// ===== HandleLayoutMetricsResult =====

/**
 * @tc.name: HandleLayoutMetricsResult_Failure_Finishes_0100
 * @tc.desc: Page.getLayoutMetrics failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleLayoutMetricsResult_Failure_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(500));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.getLayoutMetrics"), _))
      .WillOnce(Return(600));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);
  obs->OnDevToolsMethodResult(nullptr, 400, true, R"({"nodeIds":[123]})", 17);

  std::string response = R"({"model":{"border":[0,0,100,0,100,100,0,100]}})";
  obs->OnDevToolsMethodResult(nullptr, 500, true, response.data(),
                              response.size());

  obs->OnDevToolsMethodResult(nullptr, 600, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleLayoutMetricsResult_InvalidJSON_Finishes_0100
 * @tc.desc: Page.getLayoutMetrics with invalid JSON triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleLayoutMetricsResult_InvalidJSON_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(500));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.getLayoutMetrics"), _))
      .WillOnce(Return(600));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);
  obs->OnDevToolsMethodResult(nullptr, 400, true, R"({"nodeIds":[123]})", 17);

  std::string response = R"({"model":{"border":[0,0,100,0,100,100,0,100]}})";
  obs->OnDevToolsMethodResult(nullptr, 500, true, response.data(),
                              response.size());

  std::string metrics_response = "not json";
  obs->OnDevToolsMethodResult(nullptr, 600, true, metrics_response.data(),
                              metrics_response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleLayoutMetricsResult_MissingVisualViewport_Finishes_0100
 * @tc.desc: Page.getLayoutMetrics missing visualViewport triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleLayoutMetricsResult_MissingVisualViewport_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getDocument"), _))
      .WillOnce(Return(200));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.performSearch"), _))
      .WillOnce(Return(300));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getSearchResults"), _))
      .WillOnce(Return(400));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.getBoxModel"), _))
      .WillOnce(Return(500));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.getLayoutMetrics"), _))
      .WillOnce(Return(600));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");

  obs->OnDevToolsMethodResult(nullptr, 100, true, nullptr, 0);
  obs->OnDevToolsMethodResult(nullptr, 200, true, R"({"root":{"nodeId":1}})", 21);
  obs->OnDevToolsMethodResult(nullptr, 300, true, R"({"searchId":"abc","resultCount":1})", 34);
  obs->OnDevToolsMethodResult(nullptr, 400, true, R"({"nodeIds":[123]})", 17);

  std::string response = R"({"model":{"border":[0,0,100,0,100,100,0,100]}})";
  obs->OnDevToolsMethodResult(nullptr, 500, true, response.data(),
                              response.size());

  std::string metrics_response = R"({"other":"data"})";
  obs->OnDevToolsMethodResult(nullptr, 600, true, metrics_response.data(),
                              metrics_response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

// ===== HandleScreenshotResult =====

/**
 * @tc.name: HandleScreenshotResult_Failure_Finishes_0100
 * @tc.desc: Page.captureScreenshot failure triggers CDP_COMMAND_FAILED.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, HandleScreenshotResult_Failure_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.captureScreenshot"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->CaptureFullPage();

  std::string response = "{}";
  obs->OnDevToolsMethodResult(nullptr, 100, false, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

/**
 * @tc.name: HandleScreenshotResult_InvalidJSON_Finishes_0100
 * @tc.desc: Page.captureScreenshot with invalid JSON triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleScreenshotResult_InvalidJSON_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.captureScreenshot"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->CaptureFullPage();

  std::string response = "not json at all";
  obs->OnDevToolsMethodResult(nullptr, 100, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleScreenshotResult_MissingData_Finishes_0100
 * @tc.desc: Page.captureScreenshot missing data field triggers CDP_RESPONSE_ERROR.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       HandleScreenshotResult_MissingData_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.captureScreenshot"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->CaptureFullPage();

  std::string response = R"({"other":"data"})";
  obs->OnDevToolsMethodResult(nullptr, 100, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_RESPONSE_ERROR));
}

/**
 * @tc.name: HandleScreenshotResult_Success_Finishes_0100
 * @tc.desc: Page.captureScreenshot success returns SUCCESS.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, HandleScreenshotResult_Success_Finishes_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("Page.captureScreenshot"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->CaptureFullPage();

  std::string response =
      R"({"data":"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="})";
  obs->OnDevToolsMethodResult(nullptr, 100, true, response.data(),
                              response.size());
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

// ===== OnNativeCaptureComplete =====

/**
 * @tc.name: OnNativeCaptureComplete_Success_Finishes_0100
 * @tc.desc: OnNativeCaptureComplete with success returns SUCCESS with data.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       OnNativeCaptureComplete_Success_Finishes_0100) {
  auto obs = CreateObserver();

  CaptureResult result;
  result.success = true;
  result.base64_data = "test_base64_data";

  obs->OnNativeCaptureComplete(result);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

/**
 * @tc.name: OnNativeCaptureComplete_Failure_Finishes_0100
 * @tc.desc: OnNativeCaptureComplete with failure returns error code.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       OnNativeCaptureComplete_Failure_Finishes_0100) {
  auto obs = CreateObserver();

  CaptureResult result;
  result.success = false;
  result.error_code = WebCommandErrorCode::CDP_COMMAND_FAILED;
  result.error_message = "native capture failed";

  obs->OnNativeCaptureComplete(result);
  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::CDP_COMMAND_FAILED));
}

// ===== DiscardSearchResults =====

/**
 * @tc.name: DiscardSearchResults_CallsDiscardSearchResults_0100
 * @tc.desc: DiscardSearchResults sends DOM.discardSearchResults when search_id is set.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       DiscardSearchResults_CallsDiscardSearchResults_0100) {
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->search_id_ = "test_search_id";
  obs->DiscardSearchResults();

  EXPECT_EQ(obs->search_id_, "");
}

/**
 * @tc.name: DiscardSearchResults_EmptySearchId_NoCall_0100
 * @tc.desc: DiscardSearchResults does not call CDP when search_id is empty.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, DiscardSearchResults_EmptySearchId_NoCall_0100) {
  auto obs = CreateObserver();
  obs->search_id_ = "";

  obs->DiscardSearchResults();

  EXPECT_FALSE(callback_->called_);
}

// ===== CleanupDomDomain =====

/**
 * @tc.name: CleanupDomDomain_EmptyXpath_NoDOMDisable_0100
 * @tc.desc: CleanupDomDomain does not send DOM.disable when xpath is empty.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, CleanupDomDomain_EmptyXpath_NoDOMDisable_0100) {
  auto obs = CreateObserver();
  obs->xpath_ = "";

  obs->CleanupDomDomain();

  EXPECT_FALSE(callback_->called_);
}

/**
 * @tc.name: CleanupDomDomain_NonEmptyXpath_SendsDOMDisable_0100
 * @tc.desc: CleanupDomDomain sends DOM.disable when xpath is set.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CleanupDomDomain_NonEmptyXpath_SendsDOMDisable_0100) {
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(100));

  auto obs = CreateObserver();
  obs->xpath_ = "//div";
  obs->CleanupDomDomain();
}

/**
 * @tc.name: CleanupDomDomain_NonEmptyXpath_DiscardsSearchResults_0100
 * @tc.desc: CleanupDomDomain calls DiscardSearchResults first.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest,
       CleanupDomDomain_NonEmptyXpath_DiscardsSearchResults_0100) {
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.discardSearchResults"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(200));

  auto obs = CreateObserver();
  obs->xpath_ = "//div";
  obs->search_id_ = "test_search_id";
  obs->CleanupDomDomain();

  EXPECT_TRUE(obs->search_id_.empty());
}

// ===== FinishWithError =====

/**
 * @tc.name: FinishWithError_NullCallback_NoCrash_0100
 * @tc.desc: FinishWithError with null callback does not crash.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, FinishWithError_NullCallback_NoCrash_0100) {
  CefRefPtr<ScreenshotObserver> obs(
      new ScreenshotObserver(mock_browser_, nullptr));
  obs->FinishWithError(static_cast<int32_t>(WebCommandErrorCode::FAILED),
                       "test error");
}

/**
 * @tc.name: FinishWithError_ClearsRegistration_0100
 * @tc.desc: FinishWithError clears registration_.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, FinishWithError_ClearsRegistration_0100) {
  EXPECT_CALL(*mock_browser_, AddDevToolsMessageObserver(_))
      .WillOnce(Return(new FakeRegistration()));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.enable"), _))
      .WillOnce(Return(100));
  EXPECT_CALL(*mock_browser_,
              ExecuteDevToolsMethod(_, StrEq("DOM.disable"), _))
      .WillOnce(Return(0));

  auto obs = CreateObserver();
  obs->CaptureElement("//div");
  EXPECT_TRUE(obs->registration_);

  obs->FinishWithError(static_cast<int32_t>(WebCommandErrorCode::FAILED),
                       "test error");
  EXPECT_FALSE(obs->registration_);
}

// ===== FinishWithSuccess =====

/**
 * @tc.name: FinishWithSuccess_NullCallback_NoCrash_0100
 * @tc.desc: FinishWithSuccess with null callback does not crash.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, FinishWithSuccess_NullCallback_NoCrash_0100) {
  CefRefPtr<ScreenshotObserver> obs(
      new ScreenshotObserver(mock_browser_, nullptr));
  obs->FinishWithSuccess();
}

/**
 * @tc.name: FinishWithSuccess_SendsScreenshotData_0100
 * @tc.desc: FinishWithSuccess sends SUCCESS with screenshot data.
 * @tc.type: FUNC
 */
TEST_F(ScreenshotObserverTest, FinishWithSuccess_SendsScreenshotData_0100) {
  auto obs = CreateObserver();
  obs->screenshot_data_ = "test_base64_data";
  obs->FinishWithSuccess();

  EXPECT_TRUE(callback_->called_);
  EXPECT_EQ(ExtractCodeFromJson(callback_->value_),
            static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}
