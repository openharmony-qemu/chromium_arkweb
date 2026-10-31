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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/page/chrome_client_impl_ext.h"

#include "base/functional/bind.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"

class ChromeClientImplExtTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_ = helper_.Initialize(&web_frame_client_);
    main_frame_ = helper_.LocalMainFrame();
    chrome_client_impl_ =
        To<ChromeClientImpl>(&web_view_->GetPage()->GetChromeClient());
    ASSERT_TRUE(chrome_client_impl_);
    chrome_client_impl_ext_ = chrome_client_impl_->AsChromeClientImplExt();
    ASSERT_TRUE(chrome_client_impl_ext_);
  }

  test::TaskEnvironment task_environment_;
  ViewCreatingClient web_frame_client_;
  frame_test_helpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
  WebLocalFrame* main_frame_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
  ChromeClientImplExt* chrome_client_impl_ext_;
};

TEST_F(ChromeClientImplExtTest, SetScrollbarAvoidAreaTop) {
  ScopedPagePauser pauser;
  LocalFrame* frame = To<WebLocalFrameImpl>(main_frame_)->GetFrame();
  ASSERT_TRUE(frame);
  FrameLoadRequest request(frame->DomWindow(), ResourceRequest());
  request.SetNavigationPolicy(kNavigationPolicyNewForegroundTab);
  EXPECT_TRUE(chrome_client_impl_ext_->SetScrollbarAvoidAreaTop(0.2));

  EXPECT_TRUE(chrome_client_impl_ext_->GetWebView());
  EXPECT_TRUE(chrome_client_impl_ext_->GetWebView()->MainFrameImpl());
}

TEST_F(ChromeClientImplExtTest, SetScrollbarAvoidAreaBottom) {
  ScopedPagePauser pauser;
  LocalFrame* frame = To<WebLocalFrameImpl>(main_frame_)->GetFrame();
  ASSERT_TRUE(frame);
  FrameLoadRequest request(frame->DomWindow(), ResourceRequest());
  request.SetNavigationPolicy(kNavigationPolicyNewForegroundTab);
  EXPECT_TRUE(chrome_client_impl_ext_->SetScrollbarAvoidAreaBottom(0.2));

  EXPECT_TRUE(chrome_client_impl_ext_->GetWebView());
  EXPECT_TRUE(chrome_client_impl_ext_->GetWebView()->MainFrameImpl());
}

TEST_F(ChromeClientImplExtTest, GetVisibleRectToWeb) {
  ScopedPagePauser pauser;
  LocalFrame* frame = To<WebLocalFrameImpl>(main_frame_)->GetFrame();
  ASSERT_TRUE(frame);
  FrameLoadRequest request(frame->DomWindow(), ResourceRequest());
  request.SetNavigationPolicy(kNavigationPolicyNewForegroundTab);
  gfx::Rect rect = chrome_client_impl_ext_->GetVisibleRectToWeb(nullptr);
  EXPECT_EQ(rect.width(), 0);
}

TEST_F(ChromeClientImplExtTest, GetFoldStatus) {
  ScopedPagePauser pauser;
  LocalFrame* frame = To<WebLocalFrameImpl>(main_frame_)->GetFrame();
  ASSERT_TRUE(frame);
  FrameLoadRequest request(frame->DomWindow(), ResourceRequest());
  request.SetNavigationPolicy(kNavigationPolicyNewForegroundTab);
  EXPECT_EQ(chrome_client_impl_ext_->GetFoldStatus(nullptr), 0);
}

TEST_F(ChromeClientImplExtTest, CreateOverlayNullFrame) {
  int destroy_count = 0;
  chrome_client_impl_ext_->CreateOverlay(
      nullptr, SkBitmap(), gfx::Point(), {}, {},
      base::BindRepeating([](int* count) { ++*count; }, &destroy_count));
  EXPECT_EQ(destroy_count, 1);
}

TEST_F(ChromeClientImplExtTest, GetWordSelection) {
  ScopedPagePauser pauser;
  LocalFrame* frame = To<WebLocalFrameImpl>(main_frame_)->GetFrame();
  ASSERT_TRUE(frame);
  FrameLoadRequest request(frame->DomWindow(), ResourceRequest());
  request.SetNavigationPolicy(kNavigationPolicyNewForegroundTab);
  String text = String("Hello World");
  Vector<int8_t> result = chrome_client_impl_ext_->GetWordSelection(frame, text, 0);
  EXPECT_EQ(result.size(), 2u);
}

TEST_F(ChromeClientImplExtTest, GetWordSelectionNullText) {
  LocalFrame* frame = To<WebLocalFrameImpl>(main_frame_)->GetFrame();
  ASSERT_TRUE(frame);
  String null_text;
  Vector<int8_t> result =
      chrome_client_impl_ext_->GetWordSelection(frame, null_text, 0);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], -1);
  EXPECT_EQ(result[1], -1);
}

TEST_F(ChromeClientImplExtTest, GetWordSelectionNullFrame) {
  Vector<int8_t> result =
      chrome_client_impl_ext_->GetWordSelection(nullptr, String("Hello"), 0);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], -1);
  EXPECT_EQ(result[1], -1);
}

TEST_F(ChromeClientImplExtTest, GetVisibleRectToWebWithValidFrame) {
  ScopedPagePauser pauser;
  LocalFrame* frame = To<WebLocalFrameImpl>(main_frame_)->GetFrame();
  ASSERT_TRUE(frame);
  FrameLoadRequest request(frame->DomWindow(), ResourceRequest());
  request.SetNavigationPolicy(kNavigationPolicyNewForegroundTab);
  gfx::Rect rect = chrome_client_impl_ext_->GetVisibleRectToWeb(frame);
}
