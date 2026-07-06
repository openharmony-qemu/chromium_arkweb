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

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.mojom.h"
#include "content/common/frame_messages.mojom-forward.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"

namespace content {

extern void CommitNavigationExt(const std::string& effective_scheme,
    ContentBrowserClient::NonNetworkURLLoaderFactoryMap& non_network_factories,
    BrowserContext* browser_context);

class RenderFrameHostImplForIncludeTest : public RenderFrameHostImplTest {
public:
  void SetFrame(mojo::AssociatedRemote<mojom::Frame>& frame, RenderFrameHostImpl* rfh) {
    rfh->frame_ = std::move(frame);
  }

  RenderFrameHostImpl::PageCloseState GetPageCloseState(RenderFrameHostImpl* rfh) {
    return rfh->page_close_state_;
  }

  void SetDelegate(RenderFrameHostImpl* rfh, RenderFrameHostDelegate* delegate) {
    rfh->delegate_ = delegate;
  }

  void TestOnPdfLoadEvent(RenderFrameHostImpl* rfh, int32_t result, const std::string& url) {
    rfh->OnPdfLoadEvent(result, url);
  }

  void TestOnPdfScrollAtBottom(RenderFrameHostImpl* rfh, const std::string& url) {
    rfh->OnPdfScrollAtBottom(url);
  }

  const RenderFrameHostImpl::PageCloseState expect_page_close_state =
      RenderFrameHostImpl::PageCloseState::kNotClosing;
};

TEST_F(RenderFrameHostImplForIncludeTest, SendAccessibilityEvent_BasicCall) {
  int64_t accessibilityId = 123;
  int32_t eventType = 1;
  std::string argument = "test argument";

  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->SendAccessibilityEvent(accessibilityId, eventType, argument));
}

TEST_F(RenderFrameHostImplForIncludeTest, GetCreateNewWindow_NoDelegate) {
  GURL target_url = GURL("https://parent.example.test/");
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_POPUP;
  bool allow_popup = true;
  auto features = blink::mojom::WindowFeatures::New();
  content::mojom::FrameHost::GetCreateNewWindowCallback callback = base::DoNothing();
  
  RenderFrameHostImpl* rfh = main_test_rfh();
  auto delegate = rfh->delegate();
  SetDelegate(rfh, nullptr);
  ASSERT_FALSE(rfh->delegate());
  rfh->GetCreateNewWindow(target_url, disposition, allow_popup,
                          std::move(features), std::move(callback));
  SetDelegate(rfh, delegate);
  ASSERT_TRUE(rfh->delegate());
}

TEST_F(RenderFrameHostImplForIncludeTest, GetCreateNewWindow_AllowPopupTrue) {
  GURL target_url = GURL("https://parent.example.test/");
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_POPUP;
  bool allow_popup = true;
  auto features = blink::mojom::WindowFeatures::New();
  content::mojom::FrameHost::GetCreateNewWindowCallback callback = base::DoNothing();
  
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(
      rfh->GetCreateNewWindow(target_url, disposition, allow_popup,
                              std::move(features), std::move(callback)));
}

TEST_F(RenderFrameHostImplForIncludeTest, GetCreateNewWindow_AllowPopupFalse_HasTransientActivation) {
  GURL target_url = GURL("https://parent.example.test/");
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_POPUP;
  bool allow_popup = false;
  auto features = blink::mojom::WindowFeatures::New();
  content::mojom::FrameHost::GetCreateNewWindowCallback callback = base::DoNothing();
  
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(
      rfh->GetCreateNewWindow(target_url, disposition, allow_popup,
                              std::move(features), std::move(callback)));
}

TEST_F(RenderFrameHostImplForIncludeTest, GetCreateNewWindow_AllowPopupFalse_NoTransientActivation) {
  GURL target_url = GURL("https://parent.example.test/");
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_POPUP;
  bool allow_popup = false;
  auto features = blink::mojom::WindowFeatures::New();
  content::mojom::FrameHost::GetCreateNewWindowCallback callback = base::DoNothing();

  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(
      rfh->GetCreateNewWindow(target_url, disposition, allow_popup,
                              std::move(features), std::move(callback)));
}

TEST_F(RenderFrameHostImplForIncludeTest, GenerateCodeCache_OptionsConversion) {
  const std::string test_url = "https://example.com/script.js";
  const std::string test_script = "console.log('Hello World');";

  std::map<std::string, std::string> response_headers = {
      {"Content-Type", "application/javascript"},
      {"Cache-Control", "max-age=3600"}
  };
  auto cache_options = std::make_shared<oh_code_cache::CacheOptions>(response_headers);

  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->GenerateCodeCache(test_url, test_script, cache_options, base::DoNothing()));
}

TEST_F(RenderFrameHostImplForIncludeTest, GenerateCodeCache_NoResponseHeaders) {
  const std::string test_url = "https://example.com/script.js";
  const std::string test_script = "console.log('Hello World');";

  std::map<std::string, std::string> empty_headers;
  auto cache_options = std::make_shared<oh_code_cache::CacheOptions>(empty_headers);

  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->GenerateCodeCache(test_url, test_script, cache_options, base::DoNothing()));
}

TEST_F(RenderFrameHostImplForIncludeTest, OnClearContextMenu_ReadyToBeDeleted) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  rfh->SetLifecycleState(RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted);
  EXPECT_TRUE(rfh->IsInactiveAndDisallowActivation(DisallowActivationReasonId::kShowContextMenu));
  rfh->OnClearContextMenu();
}

TEST_F(RenderFrameHostImplForIncludeTest, OnClearContextMenu_Active) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  rfh->SetLifecycleState(RenderFrameHostImpl::LifecycleStateImpl::kActive);
  EXPECT_FALSE(rfh->IsInactiveAndDisallowActivation(DisallowActivationReasonId::kShowContextMenu));
  rfh->OnClearContextMenu();
}

TEST_F(RenderFrameHostImplForIncludeTest, UpdateAdBlockEnabledToRender_SiteAdblockEnableTrue) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  bool site_adblock_enabled = true;
  ASSERT_NO_FATAL_FAILURE(rfh->UpdateAdBlockEnabledToRender(site_adblock_enabled));
}

TEST_F(RenderFrameHostImplForIncludeTest, UpdateAdBlockEnabledToRender_SiteAdblockEnableFalse) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  bool site_adblock_enabled = false;
  ASSERT_NO_FATAL_FAILURE(rfh->UpdateAdBlockEnabledToRender(site_adblock_enabled));
}

TEST_F(RenderFrameHostImplForIncludeTest, MouseSelectMenuShow_WithDelegate) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->MouseSelectMenuShow(true));
  ASSERT_NO_FATAL_FAILURE(rfh->MouseSelectMenuShow(false));
}

TEST_F(RenderFrameHostImplForIncludeTest, MouseSelectMenuShow_NoDelegate) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  auto delegate = rfh->delegate();
  SetDelegate(rfh, nullptr);
  ASSERT_FALSE(rfh->delegate());
  rfh->MouseSelectMenuShow(true);
  SetDelegate(rfh, delegate);
  ASSERT_TRUE(rfh->delegate());
}

TEST_F(RenderFrameHostImplForIncludeTest, ChangeVisibilityOfQuickMenu_WithDelegate) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->ChangeVisibilityOfQuickMenu());
}

TEST_F(RenderFrameHostImplForIncludeTest, ChangeVisibilityOfQuickMenu_NoDelegate) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  auto delegate = rfh->delegate();
  SetDelegate(rfh, nullptr);
  ASSERT_FALSE(rfh->delegate());
  rfh->ChangeVisibilityOfQuickMenu();
  SetDelegate(rfh, delegate);
  ASSERT_TRUE(rfh->delegate());
}

TEST_F(RenderFrameHostImplForIncludeTest, CloseImageOverlaySelection_WithDelegate) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->CloseImageOverlaySelection());
}

TEST_F(RenderFrameHostImplForIncludeTest, CloseImageOverlaySelection_NoDelegate) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  auto delegate = rfh->delegate();
  SetDelegate(rfh, nullptr);
  ASSERT_FALSE(rfh->delegate());
  rfh->CloseImageOverlaySelection();
  SetDelegate(rfh, delegate);
  ASSERT_TRUE(rfh->delegate());
}

TEST_F(RenderFrameHostImplForIncludeTest, IsJsDialogShowOrBeforeUnloadTimedOut_NoDelegate) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  auto delegate = rfh->delegate();
  SetDelegate(rfh, nullptr);
  ASSERT_FALSE(rfh->delegate());
  EXPECT_FALSE(rfh->IsJsDialogShowOrBeforeUnloadTimedOut());
  SetDelegate(rfh, delegate);
  ASSERT_TRUE(rfh->delegate());
}

TEST_F(RenderFrameHostImplForIncludeTest, IsJsDialogShowOrBeforeUnloadTimedOut_WithDelegate) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  rfh->IsJsDialogShowOrBeforeUnloadTimedOut();
  EXPECT_TRUE(GetPageCloseState(rfh) == expect_page_close_state);
}

TEST_F(RenderFrameHostImplForIncludeTest, CommitNavigationExt_WithResourcesScheme) {
  const std::string effective_scheme = url::kResourcesScheme;
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;
  
  EXPECT_TRUE(non_network_factories.empty());
  std::unique_ptr<TestBrowserContext> browser_context = std::make_unique<TestBrowserContext>();
  CommitNavigationExt(effective_scheme, non_network_factories, browser_context.get());
  
  EXPECT_EQ(non_network_factories.size(), 1u);
  EXPECT_TRUE(non_network_factories.find(url::kResourcesScheme) != non_network_factories.end());
  EXPECT_TRUE(non_network_factories[url::kResourcesScheme].is_valid());
}

TEST_F(RenderFrameHostImplForIncludeTest, CommitNavigationExt_WithNonResourcesScheme) {
  const std::string effective_scheme = "http";
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;

  std::unique_ptr<TestBrowserContext> browser_context = std::make_unique<TestBrowserContext>();
  CommitNavigationExt(effective_scheme, non_network_factories, browser_context.get());

  EXPECT_TRUE(non_network_factories.empty());
}

TEST_F(RenderFrameHostImplForIncludeTest, GetWorldId_EmptyName) {
  const std::string empty_world_name = "";
  int32_t world_id = 0;
    
  RenderFrameHostImpl* rfh = main_test_rfh();
  bool result = rfh->GetWorldId(empty_world_name, &world_id);
  
  EXPECT_FALSE(result);
}

TEST_F(RenderFrameHostImplForIncludeTest, GetWorldId_NewName) {
  const std::string world_name = "test_world";
  int32_t world_id = 0;
  
  RenderFrameHostImpl* rfh = main_test_rfh();
  bool result = rfh->GetWorldId(world_name, &world_id);
  
  EXPECT_TRUE(result);
}

TEST_F(RenderFrameHostImplForIncludeTest, GetWorldId_ExistingName) {
  const std::string world_name = "test_world";
  int32_t first_world_id = 0;
  int32_t second_world_id = 0;

  RenderFrameHostImpl* rfh = main_test_rfh();
  rfh->GetWorldId(world_name, &first_world_id);
  bool result = rfh->GetWorldId(world_name, &second_world_id);
  
  EXPECT_TRUE(result);
  EXPECT_EQ(first_world_id, second_world_id);
}

TEST_F(RenderFrameHostImplForIncludeTest, GetWorldId_IncrementalNames) {
  const std::string world_name1 = "test_world_1";
  const std::string world_name2 = "test_world_2";
  const std::string world_name3 = "test_world_3";
  int32_t world_id1 = 0;
  int32_t world_id2 = 0;
  int32_t world_id3 = 0;

  RenderFrameHostImpl* rfh = main_test_rfh();
  rfh->GetWorldId(world_name1, &world_id1);
  
  rfh->GetWorldId(world_name2, &world_id2);
  
  rfh->GetWorldId(world_name3, &world_id3);
  
  EXPECT_EQ(world_id2, world_id1 + 1);
  EXPECT_EQ(world_id3, world_id2 + 1);
}

TEST_F(RenderFrameHostImplForIncludeTest, ExecuteJavaScriptInFrames_WithWorldName) {
  const std::string world_name = "test_world";
  int32_t world_id = 0;

  RenderFrameHostImpl* rfh = main_test_rfh();
  bool result = rfh->GetWorldId(world_name, &world_id);
  EXPECT_TRUE(result);

  std::u16string javascript = u"alert('Hello World');";
  bool recursive = true;
  std::string worldName = "test_world";
  RenderFrameHost::JavaScriptResultCallback callback = base::DoNothing();

  rfh->ExecuteJavaScriptInFrames(javascript, recursive, worldName, std::move(callback));
}

TEST_F(RenderFrameHostImplForIncludeTest, ExecuteJavaScriptInFrames_WithoutWorldName) {
  std::u16string javascript = u"alert('Hello World');";
  bool recursive = true;
  std::string empty_world_name = "";
  RenderFrameHost::JavaScriptResultCallback callback = base::DoNothing();

  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(
      rfh->ExecuteJavaScriptInFrames(javascript, recursive, empty_world_name, std::move(callback)));
}

TEST_F(RenderFrameHostImplForIncludeTest, ExecuteJavaScriptInFrames_NonRecursive) {
  std::u16string javascript = u"alert('Hello World');";
  bool recursive = false;
  std::string empty_world_name = "";
  RenderFrameHost::JavaScriptResultCallback callback = base::DoNothing();

  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(
      rfh->ExecuteJavaScriptInFrames(javascript, recursive, empty_world_name, std::move(callback)));
}

TEST_F(RenderFrameHostImplForIncludeTest, OnPdfScrollAtBottom_WithDelegate) {
  std::string url = "https://example.com/document.pdf";
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_TRUE(rfh->delegate());
  TestOnPdfScrollAtBottom(rfh, url);
}

TEST_F(RenderFrameHostImplForIncludeTest, OnPdfScrollAtBottom_NoDelegate) {
  std::string url = "https://example.com/document.pdf";
  RenderFrameHostImpl* rfh = main_test_rfh();
  auto delegate = rfh->delegate();
  SetDelegate(rfh, nullptr);
  ASSERT_FALSE(rfh->delegate());
  TestOnPdfScrollAtBottom(rfh, url);
  SetDelegate(rfh, delegate);
  ASSERT_TRUE(rfh->delegate());
}

TEST_F(RenderFrameHostImplForIncludeTest, OnPdfLoadEvent_WithDelegate) {
  std::string url = "https://example.com/document.pdf";
  int32_t result = 0;
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_TRUE(rfh->delegate());
  TestOnPdfLoadEvent(rfh, result, url);
}

TEST_F(RenderFrameHostImplForIncludeTest, OnPdfLoadEvent_NoDelegate) {
  std::string url = "https://example.com/document.pdf";
  int32_t result = 0;
  RenderFrameHostImpl* rfh = main_test_rfh();
  auto delegate = rfh->delegate();
  SetDelegate(rfh, nullptr);
  ASSERT_FALSE(rfh->delegate());
  TestOnPdfLoadEvent(rfh, result, url);
  SetDelegate(rfh, delegate);
  ASSERT_TRUE(rfh->delegate());
}

TEST_F(RenderFrameHostImplForIncludeTest, OnDocumentEndReady) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->OnDocumentEndReady());
}

#if BUILDFLAG(ARKWEB_FLING)
TEST_F(RenderFrameHostImplForIncludeTest, OnNestedScroll_BasicCall) {
  float delta_x = 10.0f;
  float delta_y = 10.0f;

  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->OnNestedScroll(delta_x, delta_y));
}

TEST_F(RenderFrameHostImplForIncludeTest, OnNestedFling_BasicCall) {
  float velocity_x = 10.0f;
  float velocity_y = 10.0f;

  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->OnNestedFling(velocity_x, velocity_y));
}
#endif  // BUILDFLAG(ARKWEB_FLING)

#if BUILDFLAG(ARKWEB_MENU)
TEST_F(RenderFrameHostImplForIncludeTest, HideQuickMenu_WithDelegateNotShowing) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->HideQuickMenu());
}

TEST_F(RenderFrameHostImplForIncludeTest, HideQuickMenu_NoDelegate) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  auto delegate = rfh->delegate();
  SetDelegate(rfh, nullptr);
  ASSERT_FALSE(rfh->delegate());
  rfh->HideQuickMenu();
  SetDelegate(rfh, delegate);
  ASSERT_TRUE(rfh->delegate());
}
#endif  // BUILDFLAG(ARKWEB_MENU)

TEST_F(RenderFrameHostImplForIncludeTest, GetAllImage_WithFrame) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->GetAllImage(1, "https://example.com/img.png", base::DoNothing()));
}

TEST_F(RenderFrameHostImplForIncludeTest, GetImageByXPath_WithFrame) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->GetImageByXPath(1, "/html/body/img[1]", base::DoNothing()));
}

#if BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
TEST_F(RenderFrameHostImplForIncludeTest, GetImageFromCache_BasicCall) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->GetImageFromCache("https://example.com/icon.png", base::DoNothing()));
}
#endif  // BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)

#if BUILDFLAG(ARKWEB_PDF)
TEST_F(RenderFrameHostImplForIncludeTest, SetIsPDF_True) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->SetIsPDF(true));
}

TEST_F(RenderFrameHostImplForIncludeTest, SetIsPDF_False) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->SetIsPDF(false));
}
#endif  // BUILDFLAG(ARKWEB_PDF)

#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
TEST_F(RenderFrameHostImplForIncludeTest, OnOverScrollOffsetChanged_BasicCall) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->OnOverScrollOffsetChanged(10.0f, 20.0f));
}
#endif  // BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
TEST_F(RenderFrameHostImplForIncludeTest, SetBlankScreenDetectionConfig_Enable) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  std::vector<double> timing = {1.0, 2.0, 3.0};
  std::vector<int32_t> methods = {1, 2};
  ASSERT_NO_FATAL_FAILURE(rfh->SetBlankScreenDetectionConfig(true, timing, methods, 5));
}

TEST_F(RenderFrameHostImplForIncludeTest, SetBlankScreenDetectionConfig_Disable) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  std::vector<double> timing;
  std::vector<int32_t> methods;
  ASSERT_NO_FATAL_FAILURE(rfh->SetBlankScreenDetectionConfig(false, timing, methods, 0));
}

TEST_F(RenderFrameHostImplForIncludeTest, DetectBlankScreen_Disabled) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->DetectBlankScreen("https://example.com"));
}

TEST_F(RenderFrameHostImplForIncludeTest, DetectBlankScreen_Enabled) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  std::vector<double> timing = {1.0};
  std::vector<int32_t> methods = {1};
  rfh->SetBlankScreenDetectionConfig(true, timing, methods, 5);
  ASSERT_NO_FATAL_FAILURE(rfh->DetectBlankScreen("https://example.com"));
}

TEST_F(RenderFrameHostImplForIncludeTest, OnDetectedBlankScreen_BasicCall) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->OnDetectedBlankScreen("https://example.com", 1, 0));
}

TEST_F(RenderFrameHostImplForIncludeTest, OnDetectedBlankScreen_Enabled) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  std::vector<double> timing = {1.0};
  std::vector<int32_t> methods = {1};
  rfh->SetBlankScreenDetectionConfig(true, timing, methods, 5);
  ASSERT_NO_FATAL_FAILURE(rfh->OnDetectedBlankScreen("https://example.com", 1, 0));
}
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)

#if BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)
TEST_F(RenderFrameHostImplForIncludeTest, OnFirstScreenPaint_BasicCall) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->OnFirstScreenPaint("https://example.com", 1000, 2000));
}
#endif  // BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)

#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
TEST_F(RenderFrameHostImplForIncludeTest, NotifyContextMenuWillShow_BasicCall) {
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->NotifyContextMenuWillShow());
}
#endif  // BUILDFLAG(ARKWEB_EXT_FREE_COPY)

#if BUILDFLAG(ARKWEB_ACCESSIBILITY)
TEST_F(RenderFrameHostImplForIncludeTest, SendAccessibilityEvent_SecondCall) {
  int64_t accessibilityId = 456;
  int32_t eventType = 2;
  std::string argument = "second test";
  RenderFrameHostImpl* rfh = main_test_rfh();
  ASSERT_NO_FATAL_FAILURE(rfh->SendAccessibilityEvent(accessibilityId, eventType, argument));
}
#endif  // BUILDFLAG(ARKWEB_ACCESSIBILITY)

}  // namespace content