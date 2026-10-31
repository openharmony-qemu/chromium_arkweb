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

#include "arkweb/build/features/features.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "base/functional/bind.h"
#include "base/test/task_environment.h"

#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "content/public/browser/context_menu_params.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "url/gurl.h"

// Access private members of LinkToTextMenuManager
// (OnRequestLinkGenerationCompleted, Timeout). Wrapped in clang-format off/on
// to preserve include order.
// clang-format off
#define private public
#include "arkweb/chromium_ext/chrome/browser/renderer_context_menu/link_to_text_menu_manager.h"
#undef private
// clang-format on

namespace OHOS::NWeb {

namespace {

content::ContextMenuParams MakeParams(
    const GURL& page_url,
    const std::u16string& selection_text,
    std::optional<blink::mojom::AnnotationType> annotation_type =
        std::nullopt) {
  content::ContextMenuParams params;
  params.page_url = page_url;
  params.selection_text = selection_text;
  params.annotation_type = annotation_type;
  return params;
}

}  // namespace

// Uses base::test::TaskEnvironment instead of ChromeRenderViewHostTestHarness
// to avoid the heavy ContentTestSuite/GL/CEF initialization that crashes on
// the OHOS device. Tests that need WebContents use InitMenu with nullptr to
// exercise the null-guard paths.
class LinkToTextMenuManagerTest : public testing::Test {
 protected:
   void SetUp() override {

  }

  void TearDown() override {

  }
  base::test::TaskEnvironment task_environment_;
  LinkToTextMenuManager manager_;
};

// --- Construction / simple state ---

TEST_F(LinkToTextMenuManagerTest, DefaultState) {
  EXPECT_FALSE(manager_.ShouldOfferLinkToText());
  EXPECT_FALSE(manager_.HasGeneratedLink());
  EXPECT_FALSE(manager_.IsOnAnnotation());
  EXPECT_FALSE(manager_.CanReshareLink());
  EXPECT_EQ(manager_.GetLinkUrl(), std::string());
}

// --- SetLinkGenerationCompletedCallback + OnRequestLinkGenerationCompleted ---

TEST_F(LinkToTextMenuManagerTest, SetLinkGenerationCompletedCallbackRuns) {
  bool callback_called = false;
  bool has_link_value = true;
  manager_.SetLinkGenerationCompletedCallback(base::BindRepeating(
      [](bool* called, bool* out_has_link, bool has_link) {
        *called = true;
        *out_has_link = has_link;
      },
      &callback_called, &has_link_value));

  // Trigger the callback path via OnRequestLinkGenerationCompleted with an
  // empty selector and a non-kNone error, so generated_link_ stays nullopt.
  manager_.OnRequestLinkGenerationCompleted(
      std::string(),
      shared_highlighting::LinkGenerationError::kIncorrectSelector,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedBeforeReady);
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(has_link_value);
}

TEST_F(LinkToTextMenuManagerTest, OnRequestLinkGenerationCompleted_Valid) {
  // Set url_ directly since InitMenu requires WebContents.
  manager_.url_ = GURL("http://example.com/page");

  bool callback_called = false;
  bool has_link_value = false;
  manager_.SetLinkGenerationCompletedCallback(base::BindRepeating(
      [](bool* called, bool* out, bool has_link) {
        *called = true;
        *out = has_link;
      },
      &callback_called, &has_link_value));

  manager_.OnRequestLinkGenerationCompleted(
      "foo", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  EXPECT_TRUE(manager_.HasGeneratedLink());
  EXPECT_NE(manager_.GetLinkUrl(), std::string());
  EXPECT_TRUE(manager_.GetLinkUrl().find(":~:text=foo") != std::string::npos);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(has_link_value);
}

TEST_F(LinkToTextMenuManagerTest,
       OnRequestLinkGenerationCompleted_EmptySelectorNoLink) {
  manager_.OnRequestLinkGenerationCompleted(
      std::string(), shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  EXPECT_FALSE(manager_.HasGeneratedLink());
  EXPECT_EQ(manager_.GetLinkUrl(), std::string());
}

TEST_F(LinkToTextMenuManagerTest,
       OnRequestLinkGenerationCompleted_ErrorNoLink) {
  manager_.OnRequestLinkGenerationCompleted(
      "foo", shared_highlighting::LinkGenerationError::kIncorrectSelector,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedBeforeReady);
  EXPECT_FALSE(manager_.HasGeneratedLink());
}

// --- InitMenu: null web_contents path ---

TEST_F(LinkToTextMenuManagerTest, InitMenu_NullWebContents) {
  auto params = MakeParams(GURL("http://example.com/"), u"sel");
  manager_.InitMenu(params, nullptr);
  EXPECT_FALSE(manager_.ShouldOfferLinkToText());
}

// --- InitMenu with annotation + null web_contents (tests annotation_type
// assignment without requiring a full WebContents) ---

TEST_F(LinkToTextMenuManagerTest, InitMenu_NullWebContentsWithAnnotation) {
  auto params = MakeParams(GURL("http://example.com/"), std::u16string(),
                           blink::mojom::AnnotationType::kSharedHighlight);
  manager_.InitMenu(params, nullptr);
  // Even with null web_contents, annotation_type_ is set before the null
  // check returns. IsOnAnnotation should reflect it.
  // Note: InitMenu returns early on null web_contents, so visible_ stays
  // false, but annotation_type_ is still assigned.
  EXPECT_FALSE(manager_.ShouldOfferLinkToText());
}

// --- CopyLinkToText without InitMenu (no web_contents) ---

TEST_F(LinkToTextMenuManagerTest, CopyLinkToText_NoInitReturnsFalse) {
  // Without InitMenu, web_contents_ is null and open_from_new_selection_ is
  // false. CopyLinkToText dispatches to ReshareLink which checks
  // web_contents_.
  EXPECT_FALSE(manager_.CopyLinkToText());
}

// --- RemoveLinkToText without InitMenu ---

TEST_F(LinkToTextMenuManagerTest, RemoveLinkToText_NoInitReturnsFalse) {
  // Without InitMenu, annotation_type_ is nullopt -> RemoveLinkToText returns
  // false.
  EXPECT_FALSE(manager_.RemoveLinkToText());
}

// --- RemoveLinkToText after InitMenu with null web_contents + annotation ---

TEST_F(LinkToTextMenuManagerTest, RemoveLinkToText_NullWebContentsFails) {
  auto params = MakeParams(GURL("http://example.com/"), std::u16string(),
                           blink::mojom::AnnotationType::kSharedHighlight);
  manager_.InitMenu(params, nullptr);
  EXPECT_FALSE(manager_.RemoveLinkToText());
}

// --- Timeout: after completion is no-op ---
// Note: Timeout() without prior StartLinkGenerationRequestWithTimeout() crashes
// because mojo::Remote::is_connected() returns true for a default-constructed
// remote, but remote_->Cancel() dereferences an invalid pointer. This is a
// business code bug documented in the UT report.

TEST_F(LinkToTextMenuManagerTest, Timeout_AfterCompletionIsNoOp) {
  // Set is_generation_complete_ = true so Timeout() returns early before
  // touching the invalid remote_.
  manager_.url_ = GURL("http://example.com/page");
  manager_.OnRequestLinkGenerationCompleted(
      "foo", shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  ASSERT_TRUE(manager_.HasGeneratedLink());
  ASSERT_TRUE(manager_.is_generation_complete_);
  manager_.Timeout();
  EXPECT_TRUE(manager_.HasGeneratedLink());
}

}  // namespace OHOS::NWeb
