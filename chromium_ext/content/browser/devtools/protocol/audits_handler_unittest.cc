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

#include "content/browser/devtools/protocol/audits_handler.h"

#include <memory>

#include "content/browser/devtools/protocol/audits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/chromium/protocol_traits.h"

namespace content::protocol {

class AuditsHandlerTest : public testing::Test {
 public:
  void SetUp() override { audits_handler_ = std::make_unique<AuditsHandler>(); }

  void TearDown() override { audits_handler_.reset(); }

 protected:
  std::unique_ptr<AuditsHandler> audits_handler_;
};

TEST_F(AuditsHandlerTest, EnableReturnsFallThrough) {
  DispatchResponse response = audits_handler_->Enable();
  EXPECT_EQ(crdtp::DispatchCode::FALL_THROUGH, response.Code());
}

TEST_F(AuditsHandlerTest, DoubleEnableReturnsFallThrough) {
  audits_handler_->Enable();
  DispatchResponse response = audits_handler_->Enable();
  EXPECT_EQ(crdtp::DispatchCode::FALL_THROUGH, response.Code());
}

TEST_F(AuditsHandlerTest, DisableReturnsFallThrough) {
  audits_handler_->Enable();
  DispatchResponse response = audits_handler_->Disable();
  EXPECT_EQ(crdtp::DispatchCode::FALL_THROUGH, response.Code());
}

TEST_F(AuditsHandlerTest, DisableWithoutEnable) {
  DispatchResponse response = audits_handler_->Disable();
  EXPECT_EQ(crdtp::DispatchCode::FALL_THROUGH, response.Code());
}

TEST_F(AuditsHandlerTest, OnIssueAddedNoOpWhenDisabled) {
  auto issue = Audits::InspectorIssue::Create()
                   .SetCode(Audits::InspectorIssueCodeEnum::MixedContentIssue)
                   .SetDetails(Audits::InspectorIssueDetails::Create().Build())
                   .Build();
  audits_handler_->OnIssueAdded(*issue);
}

TEST_F(AuditsHandlerTest, OnIssueAddedNoOpAfterDisable) {
  audits_handler_->Enable();
  audits_handler_->Disable();

  auto issue =
      Audits::InspectorIssue::Create()
          .SetCode(Audits::InspectorIssueCodeEnum::BlockedByResponseIssue)
          .SetDetails(Audits::InspectorIssueDetails::Create().Build())
          .Build();
  audits_handler_->OnIssueAdded(*issue);
}

TEST_F(AuditsHandlerTest, FreshHandlerDoesNotForwardIssues) {
  auto issue = Audits::InspectorIssue::Create()
                   .SetCode(Audits::InspectorIssueCodeEnum::HeavyAdIssue)
                   .SetDetails(Audits::InspectorIssueDetails::Create().Build())
                   .Build();
  audits_handler_->OnIssueAdded(*issue);
}

}  // namespace content::protocol
