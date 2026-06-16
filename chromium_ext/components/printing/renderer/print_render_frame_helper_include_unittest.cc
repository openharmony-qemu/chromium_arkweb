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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ARKWEB_PRINT)
#define private public
#include "components/printing/renderer/print_render_frame_helper.h"
#undef private
#endif

#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace printing {

#if BUILDFLAG(ARKWEB_PRINT)

class MockPrintRenderFrameHelperDelegate
    : public PrintRenderFrameHelper::Delegate {
 public:
  MOCK_METHOD(blink::WebElement, GetPdfElement,
              (blink::WebLocalFrame* frame), (override));
  MOCK_METHOD(bool, IsPrintPreviewEnabled, (), (override));
  MOCK_METHOD(bool, IsScriptedPrintEnabled, (), (override));
  MOCK_METHOD(bool, ShouldGenerateTaggedPDF, (), (override));
  MOCK_METHOD(bool, OverridePrint,
              (blink::WebLocalFrame* frame), (override));
};

class ClosuresForMojoResponseArkWebPrintTest : public testing::Test {
 protected:
  void SetUp() override {
    closures_ = base::MakeRefCounted<ClosuresForMojoResponse>();
  }

  scoped_refptr<ClosuresForMojoResponse> closures_;
};

TEST_F(ClosuresForMojoResponseArkWebPrintTest, SetAndRunQuitClosure) {
  bool callback_called = false;
  closures_->SetPrintRequestedPreviewQuitClosure(
      base::BindOnce([](bool* flag) { *flag = true; },
                     &callback_called));

  closures_->RunPrintRequestedPreviewQuitClosure();

  EXPECT_TRUE(callback_called);
}

TEST_F(ClosuresForMojoResponseArkWebPrintTest, SetQuitClosure_AfterConsumed) {
  bool first_called = false;
  bool second_called = false;

  closures_->SetPrintRequestedPreviewQuitClosure(
      base::BindOnce([](bool* flag) { *flag = true; },
                     &first_called));

  closures_->RunPrintRequestedPreviewQuitClosure();
  EXPECT_TRUE(first_called);

  closures_->SetPrintRequestedPreviewQuitClosure(
      base::BindOnce([](bool* flag) { *flag = true; },
                     &second_called));

  closures_->RunPrintRequestedPreviewQuitClosure();
  EXPECT_TRUE(second_called);
}

TEST_F(ClosuresForMojoResponseArkWebPrintTest, QuitClosure_NullAfterRun) {
  closures_->SetPrintRequestedPreviewQuitClosure(base::BindOnce([]() {}));

  EXPECT_TRUE(closures_->print_requested_preview_quit_closure_);

  closures_->RunPrintRequestedPreviewQuitClosure();

  EXPECT_FALSE(closures_->print_requested_preview_quit_closure_);
}

#endif  // BUILDFLAG(ARKWEB_PRINT)

}  // namespace printing
