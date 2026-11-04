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

#include "content/browser/devtools/protocol/emulation_handler.h"

#include <memory>

#include "content/browser/devtools/protocol/emulation.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/chromium/protocol_traits.h"

namespace content::protocol {

class EmulationHandlerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    emulation_handler_ = std::make_unique<EmulationHandler>();
  }

  void TearDown() override {
    emulation_handler_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<EmulationHandler> emulation_handler_;
};

TEST_F(EmulationHandlerTest, ClearDeviceMetricsOverrideNoHost) {
  Response response = emulation_handler_->ClearDeviceMetricsOverride();
  EXPECT_EQ(crdtp::DispatchCode::SERVER_ERROR, response.Code());
  EXPECT_EQ("Can't find the associated web contents", response.Message());
}

TEST_F(EmulationHandlerTest, ClearDeviceMetricsOverrideNotEnabled) {
  emulation_handler_->SetRenderer(0, contents()->GetPrimaryMainFrame());
  Response response = emulation_handler_->ClearDeviceMetricsOverride();
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(EmulationHandlerTest, DisableReturnsSuccessWithHost) {
  emulation_handler_->SetRenderer(0, contents()->GetPrimaryMainFrame());
  Response response = emulation_handler_->Disable();
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(EmulationHandlerTest, CanEmulateReturnsSuccess) {
  bool result = false;
  Response response = emulation_handler_->CanEmulate(&result);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(result);
#endif
}

TEST_F(EmulationHandlerTest, CanEmulateWithHost) {
  emulation_handler_->SetRenderer(0, contents()->GetPrimaryMainFrame());
  bool result = false;
  Response response = emulation_handler_->CanEmulate(&result);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(EmulationHandlerTest, DeviceEmulationNotEnabledByDefault) {
  EXPECT_FALSE(emulation_handler_->device_emulation_enabled());
}

TEST_F(EmulationHandlerTest, SetFocusEmulationEnabledNoOpAndDisable) {
  emulation_handler_->SetRenderer(0, contents()->GetPrimaryMainFrame());
  Response response = emulation_handler_->SetFocusEmulationEnabled(false);
  EXPECT_EQ(crdtp::DispatchCode::FALL_THROUGH, response.Code());

  Response disable_response = emulation_handler_->Disable();
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, disable_response.Code());
}

TEST_F(EmulationHandlerTest, DisableWithoutHost) {
  Response response = emulation_handler_->Disable();
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(EmulationHandlerTest, ClearIdleOverrideNoHost) {
  Response response = emulation_handler_->ClearIdleOverride();
  EXPECT_EQ(crdtp::DispatchCode::INTERNAL_ERROR, response.Code());
}

TEST_F(EmulationHandlerTest, SetIdleOverrideNoHost) {
  Response response = emulation_handler_->SetIdleOverride(true, true);
  EXPECT_EQ(crdtp::DispatchCode::INTERNAL_ERROR, response.Code());
}

}  // namespace content::protocol
