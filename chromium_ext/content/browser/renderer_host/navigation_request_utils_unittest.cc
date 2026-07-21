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
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "arkweb/chromium_ext/content/browser/renderer_host/navigation_request_utils.h"
#include "base/test/scoped_command_line.h"
#include "content/public/common/content_switches.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "url/url_util.h"

namespace content {

class NavigationRequestUtilsTest : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    CreateNavigationHandle();
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override { RenderViewHostImplTestHarness::TearDown(); }

  void CreateNavigationHandle() {
    auto common_navigation_params = blink::CreateCommonNavigationParams();
    common_navigation_params->initiator_origin =
        url::Origin::Create(GURL("https://initiator.domain.com"));

    auto commit_navigation_params = blink::CreateCommitNavigationParams();
    commit_navigation_params->frame_policy =
        main_test_rfh()->frame_tree_node()->pending_frame_policy();

    auto request = NavigationRequest::CreateBrowserInitiated(
        main_test_rfh()->frame_tree_node(), std::move(common_navigation_params),
        std::move(commit_navigation_params), false /* was_opener_suppressed */,
        std::string() /* extra_headers */, nullptr /* frame_entry */,
        nullptr /* entry */, false /* is_form_submission */,
        nullptr /* navigation_ui_data */, std::nullopt /* impression */,
        false /* is_pdf */);
    main_test_rfh()->frame_tree_node()->TakeNavigationRequest(
        std::move(request));

    GetNavigationRequest()->StartNavigation();
  }

  NavigationRequest* GetNavigationRequest() {
    return main_test_rfh()->frame_tree_node()->navigation_request();
  }

  base::test::ScopedCommandLine scoped_command_line_;
};

// Constructor test
TEST_F(NavigationRequestUtilsTest, Constructor) {
  NavigationRequest* nav_req = GetNavigationRequest();
  ASSERT_NE(nav_req, nullptr);
  NavigationRequestUtils utils(nav_req);
  EXPECT_EQ(utils.nav_request_, nav_req);
}

#if BUILDFLAG(ARKWEB_NETWORK_LOAD)
// GetCustomScheme tests
TEST_F(NavigationRequestUtilsTest, GetCustomScheme_Empty) {
  NavigationRequestUtils utils(GetNavigationRequest());
  url::Origin http_origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_FALSE(utils.GetCustomScheme(http_origin));
}

TEST_F(NavigationRequestUtilsTest, GetCustomScheme_CustomScheme) {
  url::AddCustomScheme("test-custom-scheme");
  NavigationRequestUtils utils(GetNavigationRequest());
  url::Origin custom_origin =
      url::Origin::Create(GURL("test-custom-scheme://host"));
  EXPECT_TRUE(utils.GetCustomScheme(custom_origin));

  // http scheme should still return false
  url::Origin http_origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_FALSE(utils.GetCustomScheme(http_origin));
}
#endif

}  // namespace content
