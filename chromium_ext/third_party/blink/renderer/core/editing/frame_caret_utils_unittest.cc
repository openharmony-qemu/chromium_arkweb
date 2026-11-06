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

#include "arkweb/build/features/features.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include "arkweb/chromium_ext/third_party/blink/renderer/core/editing/frame_caret_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {
class WebViewChromeClient : public EmptyChromeClient {
 public:
  explicit WebViewChromeClient(WebViewImpl* web_view) : web_view_(web_view) {}
  WebViewImpl* GetWebView() const override { return web_view_; }

 private:
  WebViewImpl* web_view_;
};

class FrameCaretUtilsNoWebViewTest : public PageTestBase {};

TEST_F(FrameCaretUtilsNoWebViewTest,
       GetTouchHandleStateFromWeb_NoWebView_ReturnsFalse) {
  FrameCaretUtils utils(nullptr);
  EXPECT_FALSE(utils.GetTouchHandleStateFromWeb(&GetFrameView()));
}

class FrameCaretUtilsWebViewTest : public PageTestBase {
 public:
  void SetUp() override {
    web_view_ = helper_.InitializeAndLoad("about:blank");
    web_view_chrome_client_ =
        MakeGarbageCollected<WebViewChromeClient>(web_view_);
    SetupPageWithClients(web_view_chrome_client_);
  }
  void TearDown() override {
    helper_.Reset();
    PageTestBase::TearDown();
  }

  frame_test_helpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
  Persistent<WebViewChromeClient> web_view_chrome_client_;
};

TEST_F(FrameCaretUtilsWebViewTest,
       GetTouchHandleStateFromWeb_NoTouchHandle_ReturnsFalse) {
  FrameCaretUtils utils(nullptr);
  EXPECT_FALSE(utils.GetTouchHandleStateFromWeb(&GetFrameView()));
}

TEST_F(FrameCaretUtilsWebViewTest,
       GetTouchHandleStateFromWeb_TouchHandleExist_ReturnsTrue) {
  web_view_->GetSettings()->SetTouchHandleExistState(true);
  FrameCaretUtils utils(nullptr);
  EXPECT_TRUE(utils.GetTouchHandleStateFromWeb(&GetFrameView()));
}

}  // namespace

}  // namespace blink
