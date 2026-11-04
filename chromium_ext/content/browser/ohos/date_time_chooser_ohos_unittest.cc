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
#include "content/browser/ohos/date_time_chooser_ohos.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/system/functions.h"

namespace content {
class DateTimeChooserOHOSTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    chooser_ = std::make_unique<DateTimeChooserOHOS>(web_contents());
  }

  void TearDown() override {
    chooser_.reset();
    RenderViewHostTestHarness::TearDown();
  }


 protected:
  std::unique_ptr<DateTimeChooserOHOS> chooser_;
};

// RAII type that intercepts mojom::ReportBadMessage() calls.
class BadMessageHelper {
 public:
  BadMessageHelper() { mojo::SetDefaultProcessErrorHandler(callback_.Get()); }
  ~BadMessageHelper() {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  // This callback is invoked when a bad message arrives.
  base::MockRepeatingCallback<void(const std::string&)>& callback() {
    return callback_;
  }

 private:
  base::MockRepeatingCallback<void(const std::string&)> callback_;
  // mojo::ReportBadMessage() needs a MessageDispatchContext.
  mojo::Message dummy_message_{0, 0, 0, 0, nullptr};
  mojo::internal::MessageDispatchContext context_{&dummy_message_};
};

TEST_F(DateTimeChooserOHOSTest, NotifyResult_CallbackIsNull) {
  testing::internal::CaptureStderr();
  chooser_->NotifyResult(true, 123.45);
  std::string err_log = testing::internal::GetCapturedStderr();
  EXPECT_NE(err_log.find("DateTimeCallback is null"), std::string::npos);
}

TEST_F(DateTimeChooserOHOSTest, OpenDateTimeDialog) {
  BadMessageHelper bad_message_helper;
  EXPECT_CALL(bad_message_helper.callback(),
              Run("DateTimeChooserOHOS: Previous picker's binding isn't closed."));
  mojo::Remote<blink::mojom::DateTimeChooser> date_time_chooser_remote;
  chooser_->OnDateTimeChooserReceiver(
      date_time_chooser_remote.BindNewPipeAndPassReceiver());
  auto value = blink::mojom::DateTimeDialogValue::New();
  base::test::TestFuture<bool, double> future;
  chooser_->OpenDateTimeDialog(std::move(value), future.GetCallback());
  chooser_->OpenDateTimeDialog(std::move(value), future.GetCallback());
}

TEST_F(DateTimeChooserOHOSTest, NotifyResult_Success) {
  mojo::Remote<blink::mojom::DateTimeChooser> date_time_chooser_remote;
  chooser_->OnDateTimeChooserReceiver(
      date_time_chooser_remote.BindNewPipeAndPassReceiver());
  auto value = blink::mojom::DateTimeDialogValue::New();
  base::test::TestFuture<bool, double> future;
  chooser_->OpenDateTimeDialog(std::move(value), future.GetCallback());
  chooser_->NotifyResult(true, 123.45);
  EXPECT_TRUE(future.Wait());
  auto [success, dialog_value] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(dialog_value, 123.45);
}
}  // namespace content