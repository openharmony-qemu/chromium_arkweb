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

#include "content/browser/devtools/protocol/browser_handler.h"

#include <memory>

#include "content/browser/devtools/protocol/browser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/chromium/protocol_traits.h"

namespace content::protocol {

class BrowserHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    browser_handler_ = std::make_unique<BrowserHandler>(false);
  }

  void TearDown() override { browser_handler_.reset(); }

 protected:
  std::unique_ptr<BrowserHandler> browser_handler_;
};

TEST_F(BrowserHandlerTest, GetVersionReturnsSuccess) {
  std::string protocol_version;
  std::string product;
  std::string revision;
  std::string user_agent;
  std::string js_version;
  Response response = browser_handler_->GetVersion(
      &protocol_version, &product, &revision, &user_agent, &js_version);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  EXPECT_FALSE(protocol_version.empty());
  EXPECT_FALSE(js_version.empty());
}

TEST_F(BrowserHandlerTest, DisableReturnsSuccess) {
  Response response = browser_handler_->Disable();
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(BrowserHandlerTest, DoSetDownloadBehaviorNotAllowed) {
  Response response = browser_handler_->DoSetDownloadBehavior(
      Browser::SetDownloadBehavior::BehaviorEnum::Deny,
      /*browser_context=*/nullptr,
      /*download_path=*/std::nullopt);
  EXPECT_EQ(crdtp::DispatchCode::SERVER_ERROR, response.Code());
  EXPECT_EQ("Not allowed", response.Message());
}

TEST_F(BrowserHandlerTest, GetBrowserCommandLineWithoutAutomation) {
  std::unique_ptr<protocol::Array<std::string>> arguments;
  Response response = browser_handler_->GetBrowserCommandLine(&arguments);
  EXPECT_EQ(crdtp::DispatchCode::SERVER_ERROR, response.Code());
}

TEST_F(BrowserHandlerTest, GetHistogramNotFound) {
  std::unique_ptr<Browser::Histogram> histogram;
  Response response =
      browser_handler_->GetHistogram("NonExistent.Histogram",
                                     /*in_delta=*/std::nullopt, &histogram);
  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(BrowserHandlerTest, GetHistogramsReturnsSuccess) {
  std::unique_ptr<Array<Browser::Histogram>> histograms;
  Response response = browser_handler_->GetHistograms(
      /*in_query=*/std::nullopt, /*in_delta=*/std::nullopt, &histograms);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  EXPECT_NE(nullptr, histograms);
}

TEST_F(BrowserHandlerTest, DoSetDownloadBehaviorAllowWithoutPath) {
  auto handler = std::make_unique<BrowserHandler>(true);
  Response response = handler->DoSetDownloadBehavior(
      Browser::SetDownloadBehavior::BehaviorEnum::Allow,
      /*browser_context=*/nullptr,
      /*download_path=*/std::nullopt);
  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
  EXPECT_EQ("downloadPath not provided", response.Message());
}

TEST_F(BrowserHandlerTest, DoSetDownloadBehaviorAllowAndNameWithoutPath) {
  auto handler = std::make_unique<BrowserHandler>(true);
  Response response = handler->DoSetDownloadBehavior(
      Browser::SetDownloadBehavior::BehaviorEnum::AllowAndName,
      /*browser_context=*/nullptr,
      /*download_path=*/std::nullopt);
  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
  EXPECT_EQ("downloadPath not provided", response.Message());
}

TEST_F(BrowserHandlerTest, DoSetDownloadBehaviorNoManagerDelegate) {
  auto handler = std::make_unique<BrowserHandler>(true);
  Response response = handler->DoSetDownloadBehavior(
      Browser::SetDownloadBehavior::BehaviorEnum::Deny,
      /*browser_context=*/nullptr,
      /*download_path=*/std::string("/data/local/tmp"));
  EXPECT_EQ(crdtp::DispatchCode::SERVER_ERROR, response.Code());
}

}  // namespace content::protocol
