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

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/text_input_test_utils.h"
#include "gtest/gtest.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#define private public
#include "chrome/browser/password_manager/keyboard_suppressor_ohos.h"
#undef private

class KeyboardSuppressorOhosTest : public ::testing::Test {};

class KeyboardSuppressorOhosRenderFrameTest
    : public content::RenderViewHostTestHarness {
 public:
  KeyboardSuppressorOhosRenderFrameTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ASSERT_NE(autofill_client_injector_[web_contents()], nullptr);
    NavigateAndCommit(GURL("https://example.test/"));
  }

 protected:
  autofill::ContentAutofillDriver* GetDriver() {
    return autofill::ContentAutofillDriver::GetForRenderFrameHost(
        web_contents()->GetPrimaryMainFrame());
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
};

/**
 * @tc.name: KeyboardSuppressorOhosTest_Suppress_001.
 * @tc.desc: test of Suppress in KeyboardSuppressorOhosTest
 * @tc.type: FUNC.
 */
TEST_F(KeyboardSuppressorOhosTest, KeyboardSuppressorOhosTest_Suppress_001) {
  autofill::ContentAutofillClient* autofill_client = nullptr;
  base::TimeDelta timeout;
  KeyboardSuppressorOhos keyboardSuppressorOhosN(autofill_client, timeout);
  autofill::ContentAutofillDriver* driver = nullptr;
  keyboardSuppressorOhosN.Suppress(driver);
  EXPECT_EQ(keyboardSuppressorOhosN.suppressed_driver_, false);
}

/**
 * @tc.name: KeyboardSuppressorOhosTest_Unsuppress_001.
 * @tc.desc: test of Unsuppress in KeyboardSuppressorOhosTest
 * @tc.type: FUNC.
 */
TEST_F(KeyboardSuppressorOhosTest, KeyboardSuppressorOhosTest_Unsuppress_001) {
  autofill::ContentAutofillClient* autofill_client = nullptr;
  base::TimeDelta timeout;
  KeyboardSuppressorOhos keyboardSuppressorOhos(autofill_client, timeout);
  keyboardSuppressorOhos.Unsuppress();
  EXPECT_EQ(keyboardSuppressorOhos.suppressed_driver_, false);

  keyboardSuppressorOhos.suppressed_driver_ = true;
  keyboardSuppressorOhos.Unsuppress();
  EXPECT_EQ(keyboardSuppressorOhos.suppressed_driver_, false);
}

TEST_F(KeyboardSuppressorOhosTest, KeyboardSuppressorOhosTest_Suppress_WithDriverNotInited_001) {
  base::TimeDelta timeout = base::Milliseconds(100);
  KeyboardSuppressorOhos keyboardSuppressorOhos(nullptr, timeout);

  EXPECT_FALSE(keyboardSuppressorOhos.is_suppressor_inited_);
  EXPECT_FALSE(keyboardSuppressorOhos.suppressed_driver_);

  autofill::ContentAutofillDriver* driver = nullptr;
  keyboardSuppressorOhos.Suppress(driver);

  EXPECT_FALSE(keyboardSuppressorOhos.suppressed_driver_);
}

TEST_F(KeyboardSuppressorOhosTest, KeyboardSuppressorOhosTest_Unsuppress_AlreadyFalse_001) {
  base::TimeDelta timeout = base::Milliseconds(100);
  KeyboardSuppressorOhos keyboardSuppressorOhos(nullptr, timeout);

  EXPECT_FALSE(keyboardSuppressorOhos.suppressed_driver_);
  keyboardSuppressorOhos.Unsuppress();
  EXPECT_FALSE(keyboardSuppressorOhos.suppressed_driver_);
}

/**
 * @tc.name: KeyboardSuppressorOhosTest_Destructor_Unsuppress_001.
 * @tc.desc: test of destructor in KeyboardSuppressorOhosTest
 * @tc.type: FUNC.
 */
TEST_F(KeyboardSuppressorOhosTest, KeyboardSuppressorOhosTest_Destructor_Unsuppress_001) {
  base::TimeDelta timeout = base::Milliseconds(100);
  KeyboardSuppressorOhos* keyboardSuppressorOhos = new KeyboardSuppressorOhos(nullptr, timeout);
  keyboardSuppressorOhos->suppressed_driver_ = true;
  delete keyboardSuppressorOhos;
}

TEST_F(KeyboardSuppressorOhosRenderFrameTest,
       KeyboardSuppressorOhosTest_SuppressAndTimeout_001) {
  const base::TimeDelta timeout = base::Milliseconds(100);
  KeyboardSuppressorOhos keyboard_suppressor(nullptr, timeout);
  autofill::ContentAutofillDriver* driver = GetDriver();
  ASSERT_NE(driver, nullptr);

  keyboard_suppressor.Suppress(driver);

  EXPECT_TRUE(keyboard_suppressor.is_suppressor_inited_);
  EXPECT_TRUE(keyboard_suppressor.suppressed_driver_);
  EXPECT_TRUE(keyboard_suppressor.unsuppress_timer_.IsRunning());

  task_environment()->FastForwardBy(timeout);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(keyboard_suppressor.suppressed_driver_);
  EXPECT_FALSE(keyboard_suppressor.unsuppress_timer_.IsRunning());
}

TEST_F(KeyboardSuppressorOhosRenderFrameTest,
       KeyboardSuppressorOhosTest_SuppressRestartsTimer_001) {
  const base::TimeDelta timeout = base::Milliseconds(100);
  KeyboardSuppressorOhos keyboard_suppressor(nullptr, timeout);
  autofill::ContentAutofillDriver* driver = GetDriver();
  ASSERT_NE(driver, nullptr);

  keyboard_suppressor.Suppress(driver);
  task_environment()->FastForwardBy(base::Milliseconds(60));
  keyboard_suppressor.Suppress(driver);
  task_environment()->FastForwardBy(base::Milliseconds(60));

  EXPECT_TRUE(keyboard_suppressor.suppressed_driver_);
  EXPECT_TRUE(keyboard_suppressor.unsuppress_timer_.IsRunning());

  task_environment()->FastForwardBy(base::Milliseconds(40));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(keyboard_suppressor.suppressed_driver_);
  EXPECT_FALSE(keyboard_suppressor.unsuppress_timer_.IsRunning());
}

TEST_F(KeyboardSuppressorOhosRenderFrameTest,
       InitRegistersCallbackAndCallbackTracksSuppression) {
  content::RenderFrameHost* render_frame_host =
      web_contents()->GetPrimaryMainFrame();
  ASSERT_NE(render_frame_host, nullptr);
  content::RenderWidgetHost* render_widget_host =
      render_frame_host->GetRenderWidgetHost();
  ASSERT_NE(render_widget_host, nullptr);

  autofill::ContentAutofillDriver* driver = GetDriver();
  ASSERT_NE(driver, nullptr);

  {
    KeyboardSuppressorOhos keyboard_suppressor(nullptr,
                                                base::Milliseconds(100));
    keyboard_suppressor.InitSuppressor(*driver);
    ASSERT_TRUE(keyboard_suppressor.is_suppressor_inited_);

    content::SendTextInputStateChangedToWidget(
        render_widget_host, ui::mojom::TextInputState::New());
    EXPECT_FALSE(keyboard_suppressor.suppressed_driver_);

    keyboard_suppressor.Suppress(driver);
    EXPECT_TRUE(keyboard_suppressor.suppressed_driver_);
    content::SendTextInputStateChangedToWidget(
        render_widget_host, ui::mojom::TextInputState::New());
    keyboard_suppressor.Unsuppress();
    EXPECT_FALSE(keyboard_suppressor.suppressed_driver_);
  }

  // The registered callback owns a WeakPtr. It must remain safe after the
  // suppressor is destroyed and cover the null-self branch.
  content::SendTextInputStateChangedToWidget(render_widget_host,
                                             ui::mojom::TextInputState::New());
}
