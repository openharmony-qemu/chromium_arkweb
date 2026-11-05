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
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "url/url_util.h"

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
#include "arkweb/chromium_ext/content/public/browser/error_page_reload_reason.h"
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#include "arkweb/chromium_ext/net/base/navigation_info.h"
#include "arkweb/chromium_ext/net/base/request_attempt.h"
#include "arkweb/chromium_ext/net/base/web_navigation_info.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#endif

#if BUILDFLAG(ARKWEB_PRP_PRELOAD)
#include "net/base/network_anonymization_key.h"
#endif

namespace content {

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
class TestNavigationInfoContentBrowserClient : public TestContentBrowserClient {
 public:
  bool ShouldReportNewNavigationInfo() override { return true; }
};
#endif

class NavigationRequestUtilsTest : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    CreateNavigationHandle(GURL());
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override { RenderViewHostImplTestHarness::TearDown(); }

  void CreateNavigationHandle(const GURL& url) {
    auto common_navigation_params = blink::CreateCommonNavigationParams();
    common_navigation_params->initiator_origin =
        url::Origin::Create(GURL("https://initiator.domain.com"));
    common_navigation_params->url = url;

    auto commit_navigation_params = blink::CreateCommitNavigationParams();
    commit_navigation_params->frame_policy =
        main_test_rfh()->frame_tree_node()->pending_frame_policy();

    CreateNavigationHandleImpl(main_test_rfh()->frame_tree_node(),
                               std::move(common_navigation_params),
                               std::move(commit_navigation_params));
  }

  void CreateNavigationHandleImpl(
      FrameTreeNode* frame_tree_node,
      blink::mojom::CommonNavigationParamsPtr common_navigation_params,
      blink::mojom::CommitNavigationParamsPtr commit_navigation_params) {
    auto request = NavigationRequest::CreateBrowserInitiated(
        frame_tree_node, std::move(common_navigation_params),
        std::move(commit_navigation_params), false /* was_opener_suppressed */,
        std::string() /* extra_headers */, nullptr /* frame_entry */,
        nullptr /* entry */, false /* is_form_submission */,
        nullptr /* navigation_ui_data */, std::nullopt /* impression */,
        false /* is_pdf */);
    frame_tree_node->TakeNavigationRequest(std::move(request));

    GetNavigationRequest(frame_tree_node)->StartNavigation();
  }

  NavigationRequest* GetNavigationRequest(FrameTreeNode* frame_tree_node) {
    return frame_tree_node->navigation_request();
  }

  NavigationRequest* GetNavigationRequest() {
    return GetNavigationRequest(main_test_rfh()->frame_tree_node());
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

#if BUILDFLAG(ARKWEB_PRP_PRELOAD)
// GetNetworkContext returns nullptr for about:blank page
TEST_F(NavigationRequestUtilsTest, GetNetworkContext_BlankPage) {
  CreateNavigationHandle(GURL("about:blank"));
  NavigationRequestUtils utils(GetNavigationRequest());
  EXPECT_EQ(utils.GetNetworkContext(), nullptr);
}

// GetNetworkContext returns nullptr for subframe navigation
TEST_F(NavigationRequestUtilsTest, GetNetworkContext_SubFrame) {
  TestRenderFrameHost* child =
      contents()->GetPrimaryMainFrame()->AppendChild("child");
  ASSERT_NE(child, nullptr);
  ASSERT_TRUE(child->frame_tree_node() != nullptr);
  ASSERT_FALSE(child->frame_tree_node()->IsMainFrame());

  auto common_navigation_params = blink::CreateCommonNavigationParams();
  common_navigation_params->initiator_origin =
      url::Origin::Create(GURL("https://initiator.domain.com"));
  common_navigation_params->url = GURL("https://example.com/child");

  auto commit_navigation_params = blink::CreateCommitNavigationParams();
  commit_navigation_params->frame_policy =
      child->frame_tree_node()->pending_frame_policy();

  CreateNavigationHandleImpl(child->frame_tree_node(),
                             std::move(common_navigation_params),
                             std::move(commit_navigation_params));

  NavigationRequestUtils utils(GetNavigationRequest(child->frame_tree_node()));
  EXPECT_EQ(utils.GetNetworkContext(), nullptr);
}

// GetNetworkContext success path on main frame returns non-null
TEST_F(NavigationRequestUtilsTest, GetNetworkContext_MainFrame) {
  NavigationRequestUtils utils(GetNavigationRequest());
  EXPECT_NE(utils.GetNetworkContext(), nullptr);
}

// StartPage early-returns when GetNetworkContext is null (about:blank)
TEST_F(NavigationRequestUtilsTest, StartPage_NullNetworkContext) {
  CreateNavigationHandle(GURL("about:blank"));
  NavigationRequestUtils utils(GetNavigationRequest());
  // addr_web_handle_ is only assigned after the null-check in StartPage; the
  // member has no default initializer, so seed a sentinel to prove the
  // early-return path leaves it untouched.
  utils.addr_web_handle_ = 0xDEADBEEF;
  net::NetworkAnonymizationKey network_anonymization_key;
  utils.StartPage(network_anonymization_key, 100);
  EXPECT_EQ(utils.GetAddrWebHandle(), 0xDEADBEEFu);
}

// OnGetIsolation early-returns when origin starts with ORIGIN
TEST_F(NavigationRequestUtilsTest, OnGetIsolation_Canceled) {
  NavigationRequestUtils utils(GetNavigationRequest());
  utils.OnGetIsolation("origin.DEFAULT");
}

// OnGetIsolation early-returns when GetNetworkContext is null
TEST_F(NavigationRequestUtilsTest, OnGetIsolation_NullNetworkContext) {
  CreateNavigationHandle(GURL("about:blank"));
  NavigationRequestUtils utils(GetNavigationRequest());
  utils.OnGetIsolation("https://example.com/origin");
}
#endif

#if BUILDFLAG(ARKWEB_ADBLOCK)
// SetAdblockEnabledStatus with adblock disabled
TEST_F(NavigationRequestUtilsTest, SetAdblockEnabledStatus_Disabled) {
  NavigationRequestUtils utils(GetNavigationRequest());
  auto commit_params = blink::CreateCommitNavigationParams();
  main_test_rfh()->frame_tree_node()->set_adblock_enabled(false);
  utils.SetAdblockEnabledStatus(commit_params.get());
  EXPECT_FALSE(commit_params->site_adblock_enabled);
  EXPECT_FALSE(
      main_test_rfh()->frame_tree_node()->is_adblock_enabled_last_committed());
}

// SetAdblockEnabledStatus with adblock enabled
TEST_F(NavigationRequestUtilsTest, SetAdblockEnabledStatus_Enabled) {
  NavigationRequestUtils utils(GetNavigationRequest());
  auto commit_params = blink::CreateCommitNavigationParams();
  main_test_rfh()->frame_tree_node()->set_adblock_enabled(true);
  utils.SetAdblockEnabledStatus(commit_params.get());
  EXPECT_TRUE(commit_params->site_adblock_enabled);
  EXPECT_TRUE(
      main_test_rfh()->frame_tree_node()->is_adblock_enabled_last_committed());
}
#endif

#if BUILDFLAG(ARKWEB_EXT_UA)
// RemoveUserAgentHeaderForDevTools no-op when override is false
TEST_F(NavigationRequestUtilsTest,
       RemoveUserAgentHeaderForDevTools_NoOverride) {
  NavigationRequestUtils utils(GetNavigationRequest());
  // Should not crash and should not modify headers
  utils.RemoveUserAgentHeaderForDevTools(false);
}

// RemoveUserAgentHeaderForDevTools with override but no switch
TEST_F(NavigationRequestUtilsTest, RemoveUserAgentHeaderForDevTools_NoSwitch) {
  NavigationRequestUtils utils(GetNavigationRequest());
  utils.RemoveUserAgentHeaderForDevTools(true);
}

// RemoveUserAgentHeaderForDevTools with override and switch
TEST_F(NavigationRequestUtilsTest,
       RemoveUserAgentHeaderForDevTools_WithSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNwebExUa);
  NavigationRequestUtils utils(GetNavigationRequest());
  utils.RemoveUserAgentHeaderForDevTools(true);
}
#endif

#if BUILDFLAG(ARKWEB_READER_MODE)
// BeginNavigationImpl with no delegate is a no-op
TEST_F(NavigationRequestUtilsTest, BeginNavigationImpl_NoDelegate) {
  NavigationRequestUtils utils(GetNavigationRequest());
  bool should_override_url_loading = true;
  utils.BeginNavigationImpl(should_override_url_loading);
  // Without a delegate, the value must not be touched.
  EXPECT_TRUE(should_override_url_loading);
}
#endif

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
// PopulateNavigationInfo early-returns when nweb_ex is disabled
TEST_F(NavigationRequestUtilsTest, PopulateNavigationInfo_NoNwebEx) {
  NavigationRequestUtils utils(GetNavigationRequest());
  std::optional<network::URLLoaderCompletionStatus> status =
      network::URLLoaderCompletionStatus(net::OK);
  utils.PopulateNavigationInfo(status);
}

// PopulateNavigationInfo early-returns when ShouldReportNewNavigationInfo false
TEST_F(NavigationRequestUtilsTest, PopulateNavigationInfo_NoReport) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  NavigationRequestUtils utils(GetNavigationRequest());
  std::optional<network::URLLoaderCompletionStatus> status =
      network::URLLoaderCompletionStatus(net::OK);
  utils.PopulateNavigationInfo(status);
}

// PopulateNavigationInfo with no navigation_info in status
TEST_F(NavigationRequestUtilsTest, PopulateNavigationInfo_NoNavInfo) {
  TestNavigationInfoContentBrowserClient client;
  ScopedContentBrowserClientSetting setting(&client);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  NavigationRequestUtils utils(GetNavigationRequest());
  std::optional<network::URLLoaderCompletionStatus> status =
      network::URLLoaderCompletionStatus(net::OK);
  utils.PopulateNavigationInfo(status);
}

// PopulateNavigationInfo with empty request attempts
TEST_F(NavigationRequestUtilsTest, PopulateNavigationInfo_EmptyAttempts) {
  TestNavigationInfoContentBrowserClient client;
  ScopedContentBrowserClientSetting setting(&client);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  NavigationRequestUtils utils(GetNavigationRequest());
  network::URLLoaderCompletionStatus status(net::OK);
  status.navigation_info = net::NavigationInfo();
  utils.PopulateNavigationInfo(status);
}

// PopulateNavigationInfo success path with full navigation info
TEST_F(NavigationRequestUtilsTest, PopulateNavigationInfo_Success) {
  TestNavigationInfoContentBrowserClient client;
  ScopedContentBrowserClientSetting setting(&client);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  NavigationRequestUtils utils(GetNavigationRequest());
  network::URLLoaderCompletionStatus status(net::OK);
  status.navigation_info = net::NavigationInfo();
  status.navigation_info->request_url = "https://example.com/";
  status.navigation_info->request_uuid = "uuid-1";
  net::RequestAttempt attempt;
  attempt.request_trace_id = "trace-1";
  status.navigation_info->request_attempts.push_back(attempt);
  utils.PopulateNavigationInfo(status);
  EXPECT_FALSE(
      utils.GetWebNavigationInfo().navigation_info.request_url.empty());
}

// OnReportNewNavigationInfo early-returns when nweb_ex is disabled
TEST_F(NavigationRequestUtilsTest, OnReportNewNavigationInfo_NoNwebEx) {
  NavigationRequestUtils utils(GetNavigationRequest());
  utils.OnReportNewNavigationInfo("trace-id");
}

// OnReportNewNavigationInfo early-returns when ShouldReportNewNavigationInfo
// false
TEST_F(NavigationRequestUtilsTest, OnReportNewNavigationInfo_NoReport) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  NavigationRequestUtils utils(GetNavigationRequest());
  utils.OnReportNewNavigationInfo("trace-id");
}

// OnReportNewNavigationInfo resets when request_url is not HTTP/HTTPS
TEST_F(NavigationRequestUtilsTest, OnReportNewNavigationInfo_NonHttpUrl) {
  TestNavigationInfoContentBrowserClient client;
  ScopedContentBrowserClientSetting setting(&client);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  NavigationRequestUtils utils(GetNavigationRequest());
  utils.OnReportNewNavigationInfo("trace-id");
}

// OnReportNewNavigationInfo success path with HTTP URL
TEST_F(NavigationRequestUtilsTest, OnReportNewNavigationInfo_Success) {
  TestNavigationInfoContentBrowserClient client;
  ScopedContentBrowserClientSetting setting(&client);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  NavigationRequestUtils utils(GetNavigationRequest());
  utils.GetWebNavigationInfo().navigation_info.request_url =
      "https://example.com/";
  utils.OnReportNewNavigationInfo("trace-id");
}
#endif

}  // namespace content
