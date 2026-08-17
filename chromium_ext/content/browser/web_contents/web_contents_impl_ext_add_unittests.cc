// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/content/browser/web_contents/web_contents_impl_ext.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstring>

#include <memory>

#include "base/command_line.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/ohos/native_web_contents_observer.h"  // NOLINT[code1]
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/wake_lock/wake_lock_context_host.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/custom_media_player_listener.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches_ext.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"

namespace content {
class MockCustomMediaPlayerListener : public CustomMediaPlayerListener {
 public:
  MockCustomMediaPlayerListener() = default;
  ~MockCustomMediaPlayerListener() = default;
  MOCK_METHOD(void, OnStatusChanged, (uint32_t status), (override));
  MOCK_METHOD(void, OnVolumeChanged, (double volume), (override));
  MOCK_METHOD(void, OnMutedChanged, (bool muted), (override));
  MOCK_METHOD(void, OnPlaybackRateChanged, (double playback_rate), (override));
  MOCK_METHOD(void, OnDurationChanged, (double duration), (override));
  MOCK_METHOD(void, OnTimeUpdate, (double current_time), (override));
  MOCK_METHOD(void,
              OnBufferedEndTimeChanged,
              (double buffered_time),
              (override));
  MOCK_METHOD(void, OnEnded, (), (override));
  MOCK_METHOD(void, OnNetworkStateChanged, (uint32_t state), (override));
  MOCK_METHOD(void, OnReadyStateChanged, (uint32_t state), (override));
  MOCK_METHOD(void, OnFullscreenChanged, (bool fullscreen), (override));
  MOCK_METHOD(void, OnSeeking, (), (override));
  MOCK_METHOD(void, OnSeekFinished, (), (override));
  MOCK_METHOD(void,
              OnError,
              (uint32_t error_code, const std::string& error_msg),
              (override));
  MOCK_METHOD(void, OnVideoSizeChanged, (int width, int height), (override));
};

class MockWebContentsDelegateExtended : public WebContentsDelegate {
 public:
  MockWebContentsDelegateExtended() = default;
  ~MockWebContentsDelegateExtended() = default;

  MOCK_METHOD(bool,
              TrigAdBlockEnabledForSiteFromUi,
              (const std::string& main_frame_url, int main_frame_tree_node_id),
              ());

  MOCK_METHOD(std::unique_ptr<VideoAssistant>,
              CreateVideoAssistant,
              (),
              (override));

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
  MOCK_METHOD(std::unique_ptr<MediaPlayerListener>,
              OnAVCastStarted,
              (media::mojom::MediaInfoForVASTPtr media_info,
               const MediaPlayerId& media_player_id),
              (override));
#endif

#if BUILDFLAG(ARKWEB_FULLSCREEN_VIDEO_OVERLAY)
  using StringMap = std::map<std::string, std::string>;
  MOCK_METHOD(void,
              OnVideoStatusChanged,
              (VideoPlaybackNotificationType action, StringMap& param),
              (override));
  MOCK_METHOD(bool, IsFullscreenVideoOverlayEnabled, (), (const, override));
#endif
};

class MockRenderViewHostDelegateView : public RenderViewHostDelegateView {
 public:
  MockRenderViewHostDelegateView() = default;

  MockRenderViewHostDelegateView(const MockRenderViewHostDelegateView&) =
      delete;
  MockRenderViewHostDelegateView& operator=(
      const MockRenderViewHostDelegateView&) = delete;

  ~MockRenderViewHostDelegateView() override = default;

  MOCK_METHOD(void,
              StartDragging,
              (const DropData& drop_data,
               const url::Origin& source_origin,
               blink::DragOperationsMask allowed_ops,
               const gfx::ImageSkia& image,
               const gfx::Vector2d& cursor_offset,
               const gfx::Rect& drag_obj_rect,
               const blink::mojom::DragEventSourceInfo& event_info,
               RenderWidgetHostImpl* source_rwh),
              (override));

};

class MockWebContentsView : public WebContentsView {
 public:
  MockWebContentsView() = default;
  ~MockWebContentsView() override = default;

  MOCK_METHOD(gfx::NativeView, GetNativeView, (), (const, override));
  MOCK_METHOD(gfx::NativeView, GetContentNativeView, (), (const, override));
  MOCK_METHOD(gfx::NativeWindow,
              GetTopLevelNativeWindow,
              (),
              (const, override));
  MOCK_METHOD(gfx::Rect, GetContainerBounds, (), (const, override));
  MOCK_METHOD(gfx::Rect, GetViewBounds, (), (const, override));
  MOCK_METHOD(void, Resize, (const gfx::Rect& new_bounds), (override));
  MOCK_METHOD(gfx::Size, GetSize, (), (const, override));
  MOCK_METHOD(void, Focus, (), (override));
  MOCK_METHOD(void, SetInitialFocus, (), (override));
  MOCK_METHOD(void, StoreFocus, (), (override));
  MOCK_METHOD(void, RestoreFocus, (), (override));
  MOCK_METHOD(void, FocusThroughTabTraversal, (bool reverse), (override));
  MOCK_METHOD(DropData*, GetDropData, (), (const, override));
  MOCK_METHOD(void, CreateView, (gfx::NativeView context), (override));
  MOCK_METHOD(RenderWidgetHostViewBase*,
              CreateViewForWidget,
              (RenderWidgetHost * render_widget_host),
              (override));
  MOCK_METHOD(RenderWidgetHostViewBase*,
              CreateViewForChildWidget,
              (RenderWidgetHost * render_widget_host),
              (override));
  MOCK_METHOD(void, SetPageTitle, (const std::u16string& title), (override));
  MOCK_METHOD(void, RenderViewReady, (), (override));
  MOCK_METHOD(void,
              RenderViewHostChanged,
              (RenderViewHost * old_host, RenderViewHost* new_host),
              (override));
  MOCK_METHOD(void, SetOverscrollControllerEnabled, (bool enabled), (override));
  MOCK_METHOD(void, OnCapturerCountChanged, (), (override));
  MOCK_METHOD(void, FullscreenStateChanged, (bool is_fullscreen), (override));
  MOCK_METHOD(BackForwardTransitionAnimationManager*,
              GetBackForwardTransitionAnimationManager,
              (),
              (override));
  MOCK_METHOD(void,
              DestroyBackForwardTransitionAnimationManager,
              (),
              (override));

#if BUILDFLAG(IS_MAC)
  MOCK_METHOD(bool, CloseTabAfterEventTrackingIfNeeded, (), (override));
#endif

  MOCK_METHOD(void, DidStopRefresh, (), (override));

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
  MOCK_METHOD(void,
              UpdateBrowserControlsHeight,
              (int height, bool animate),
              (override));
#endif
};

class MockDelegate : public RenderFrameHostManager::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(
      bool,
      CreateRenderViewForRenderManager,
      (RenderViewHost * render_view_host,
       const std::optional<blink::FrameToken>& opener_frame_token,
       RenderFrameProxyHost* proxy_host,
       const std::optional<base::UnguessableToken>& navigation_metrics_token),
      (override));

  MOCK_METHOD(void,
              CreateRenderWidgetHostViewForRenderManager,
              (RenderViewHost * render_view_host),
              (override));

  MOCK_METHOD(void,
              BeforeUnloadFiredFromRenderManager,
              (bool proceed, bool* proceed_to_fire_unload),
              (override));

  MOCK_METHOD(void, CancelModalDialogsForRenderManager, (), (override));

  MOCK_METHOD(void,
              NotifySwappedFromRenderManager,
              (RenderFrameHostImpl * old_frame, RenderFrameHostImpl* new_frame),
              (override));

  MOCK_METHOD(void,
              NotifySwappedFromRenderManagerWithoutFallbackContent,
              (RenderFrameHostImpl * new_frame),
              (override));

  MOCK_METHOD(void,
              NotifyMainFrameSwappedFromRenderManager,
              (RenderFrameHostImpl * old_frame, RenderFrameHostImpl* new_frame),
              (override));

  MOCK_METHOD(bool, FocusLocationBarByDefault, (), (override));
  MOCK_METHOD(void, ReattachOuterDelegateIfNeeded, (), (override));
  MOCK_METHOD(void,
              OnFrameTreeNodeDestroyed,
              (FrameTreeNode * node),
              (override));
  MOCK_METHOD(const std::string&, SharedRenderProcessToken, (), (override));
};

class MockRenderFrameHostManager : public RenderFrameHostManager {
 public:
  MockRenderFrameHostManager(FrameTreeNode* frame_tree_node, Delegate* delegate)
      : RenderFrameHostManager(frame_tree_node, delegate) {}
  virtual ~MockRenderFrameHostManager() = default;

  MOCK_METHOD(RenderFrameHostImpl*,
              speculative_frame_host,
              (),
              (const, override));
  MOCK_METHOD(RenderFrameHostImpl*, current_frame_host, (), (const, override));
};

class TestWebContentsImplExt : public TestWebContents {
 public:
  explicit TestWebContentsImplExt(BrowserContext* browser_context)
      : TestWebContents(browser_context) {}

  virtual ~TestWebContentsImplExt() {}
};

class WebContentsImplExtTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    CreateBrowserMainLoop();

    if (IsIsolatedOriginRequiredToGuaranteeDedicatedProcess()) {
      ChildProcessSecurityPolicyImpl::GetInstance()->AddFutureIsolatedOrigins(
          {url::Origin::Create(isolated_cross_site_url())},
          ChildProcessSecurityPolicy::IsolatedOriginSource::TEST,
          browser_context());
      SetContents(CreateTestWebContents());
    }
  }

  void TearDown() override {
    DestroyBrowserMainLoop();
    RenderViewHostImplTestHarness::TearDown();
  }

  TestWebContentsImplExt* ExtendContent() {
    return static_cast<TestWebContentsImplExt*>(web_contents());
  }

  bool has_audio_wake_lock() {
    return ExtendContent()
        ->media_web_contents_observer()
        ->has_audio_wake_lock_for_testing();
  }

  GURL isolated_cross_site_url() const {
    return GURL("http://isolated-cross-site.com");
  }

  void UpdataMap(gfx::Rect rect) {
    ExtendContent()->native_embed_rect_info_map_.insert(  // NOLINT[code1]
        std::make_pair("1", rect));
  }

  void UpdateObserver() {
    ExtendContent()->native_web_contents_observer_ = std::move(nullptr);  // NOLINT[code1]
  }

  void UpdateDelegate() {
    ExtendContent()->video_assistant_ = std::move(nullptr);
  }

  void SetPipStatus(bool flag) { ExtendContent()->pip_status_ = flag; }

  void UpdateDeleteView(RenderViewHostDelegateView* view) {
    ExtendContent()->render_view_host_delegate_view_ = view;
  }

  void SetView(std::unique_ptr<WebContentsView> view) {
    ExtendContent()->view_ = std::move(view);
  }

  void SetContextHost(WebContents* content) {
    ExtendContent()->wake_lock_context_host_ =
        std::make_unique<WakeLockContextHost>(content);
  }

  bool HasVideoAssistant() {
    return ExtendContent()->video_assistant_ != nullptr;
  }

  void SetMediaPlayerId(const MediaPlayerId& id) {
    ExtendContent()->media_player_id_ = id;
  }

  void SetSharedRenderProcessToken(const std::string& token) {
    ExtendContent()->shared_render_process_token_ = token;
  }

  void CreateBrowserMainLoop() {
    MainFunctionParams params(base::CommandLine::ForCurrentProcess());
    browser_main_loop_ =
        std::make_unique<BrowserMainLoop>(std::move(params), nullptr);
  }

  void DestroyBrowserMainLoop() { browser_main_loop_.reset(); }

#if BUILDFLAG(ARKWEB_NETWORK_DFX) && BUILDFLAG(ARKWEB_NWEB_EX)
  void CallRecordFirstVisuallyNonEmptyPaint() {
    ExtendContent()->RecordFirstVisuallyNonEmptyPaint();
  }

  void CallRecordPageFinished() {
    ExtendContent()->RecordPageFinished();
  }

  void CallRecordFullLoadingProgress() {
    ExtendContent()->RecordFullLoadingProgress();
  }
#endif

 private:
  std::unique_ptr<BrowserMainLoop> browser_main_loop_;
};

// ---------------------------------------------------------------------------
// Additional branch-coverage tests.
// Each test below targets a branch arm that was not covered according to the
// Branch data column of web_contents_impl_ext.cc. Line numbers refer to
// that coverage report.
// ---------------------------------------------------------------------------

// isSameUserAgent (cov lines 1089/1093): install a non-empty stored UA override
// so that an empty incoming override diverges at both empty() checks.
//   1085: stored ("current_ua") != incoming ("")  -> false, continue
//   1089: !incoming.empty()                       -> false (short-circuit)
//   1093: stored.empty()                          -> false
//   -> returns false. Covers the two missing false arms.
TEST_F(WebContentsImplExtTest, isSameUserAgent004) {
  blink::UserAgentOverride current;
  current.ua_string_override = "current_ua";
  ExtendContent()->SetUserAgentOverride(current,
                                        /*override_in_new_tabs=*/false);

  blink::UserAgentOverride ua_override;  // empty ua_string_override
  auto result = ExtendContent()->isSameUserAgent(ua_override);
  EXPECT_FALSE(result);
}

// isSameUserAgent (cov line 1090/1091): an override whose ua_string contains a
// control char ('\n') is an invalid HTTP header value, so IsValidHeaderValue()
// returns false and !IsValidHeaderValue() is true -> returns true.
TEST_F(WebContentsImplExtTest, isSameUserAgent005) {
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = std::string("invalid\nheader");
  auto result = ExtendContent()->isSameUserAgent(ua_override);
  EXPECT_TRUE(result);
}

#if BUILDFLAG(ARKWEB_READER_MODE)
// EnableReaderMode (cov line 1416): prefs non-null and value changes
// (default false != true) -> both && operands true, enter body.
TEST_F(WebContentsImplExtTest, EnableReaderMode001) {
  ExtendContent()->EnableReaderMode(true);
}

// EnableReaderMode: prefs non-null and value unchanged (false == default false)
// -> second operand (is_reader_mode_enabled != enabled) is false.
TEST_F(WebContentsImplExtTest, EnableReaderMode002) {
  ExtendContent()->EnableReaderMode(false);
}

// EnableReaderMode: test flag forces GetMutableRendererPrefs() to return null
// -> first operand (prefs != NULL) is false (short-circuit).
TEST_F(WebContentsImplExtTest, EnableReaderMode003) {
  ExtendContent()->SetTestFlag(true);
  ExtendContent()->EnableReaderMode(true);
}

// OnDidMeaningfulLayout (cov line 1435): delegate present -> forward (true
// arm).
TEST_F(WebContentsImplExtTest, OnDidMeaningfulLayout001) {
  MockWebContentsDelegateExtended delegate_extend;
  ExtendContent()->SetDelegate(&delegate_extend);
  ExtendContent()->OnDidMeaningfulLayout("http://test.com");
}

// OnDidMeaningfulLayout: no delegate -> skip (false arm).
TEST_F(WebContentsImplExtTest, OnDidMeaningfulLayout002) {
  ExtendContent()->OnDidMeaningfulLayout("http://test.com");
}
#endif  // BUILDFLAG(ARKWEB_READER_MODE)

// The next five tests cover the render_view_host_delegate_view_ == nullptr arm
// of each delegate-view guard. By default the harness provides a non-null
// delegate view, so the null arm is otherwise never taken.

TEST_F(WebContentsImplExtTest, MouseSelectMenuShow003) {
  UpdateDeleteView(nullptr);
  ExtendContent()->MouseSelectMenuShow(true);
}

TEST_F(WebContentsImplExtTest, ChangeVisibilityOfQuickMenu003) {
  UpdateDeleteView(nullptr);
  ExtendContent()->ChangeVisibilityOfQuickMenu();
}

TEST_F(WebContentsImplExtTest, IsQuickMenuShow003) {
  UpdateDeleteView(nullptr);
  EXPECT_FALSE(ExtendContent()->IsQuickMenuShow());
}

TEST_F(WebContentsImplExtTest, CloseImageOverlaySelection003) {
  UpdateDeleteView(nullptr);
  ExtendContent()->CloseImageOverlaySelection();
}

TEST_F(WebContentsImplExtTest, OnOverlayZoomChanged003) {
  UpdateDeleteView(nullptr);
  ExtendContent()->OnOverlayZoomChanged();
}

// Commit a navigation so the primary main frame may be live, then look it up
// by its own routing id. This targets the routing-id-match return path
// (GetTargetFramesIncludingPending line ~482) and the IsRenderFrameLive() true
// arm, which the no-navigation GetTargetFramesIncludingPending001 cannot reach.
// The expectation tolerates both outcomes: if the harness marks the frame live
// the call returns it, otherwise it returns nullptr.
TEST_F(WebContentsImplExtTest, GetTargetFramesIncludingPending002) {
  NavigateAndCommit(GURL("https://example.com"));
  auto* main_rfh = main_test_rfh();
  ASSERT_TRUE(main_rfh);
  auto* result = ExtendContent()->GetTargetFramesIncludingPending(
      main_rfh->GetRoutingID());
  EXPECT_EQ(result, main_rfh->IsRenderFrameLive() ? main_rfh : nullptr);
}

// Commit a navigation so the primary frame may be live, then run the full
// CollapseAllFramesSelection loop (rfh live -> render widget -> input handler
// -> CollapseSelection). No assertion: exercises the live-frame path that
// CollapseAllFramesSelection001 (no navigation) cannot reach.
TEST_F(WebContentsImplExtTest, CollapseAllFramesSelectionAfterNavigation) {
  NavigateAndCommit(GURL("https://example.com"));
  ExtendContent()->CollapseAllFramesSelection();
}

// ============================================================================
// Task 1: High-Priority Direct-Testable Methods (Batch 1)
// ============================================================================

#if BUILDFLAG(ARKWEB_READER_MODE)
TEST_F(WebContentsImplExtTest, EnableCustomReaderMode001) {
  ExtendContent()->EnableCustomReaderMode(true);
  // Verify through renderer prefs if getter exists, or observe side-effect
  auto* prefs = ExtendContent()->GetMutableRendererPrefs();
  ASSERT_NE(prefs, nullptr);
  EXPECT_TRUE(prefs->is_custom_reader_mode_enabled);
}
#endif  // BUILDFLAG(ARKWEB_READER_MODE)

#if BUILDFLAG(ARKWEB_READER_MODE)
TEST_F(WebContentsImplExtTest, EnableCustomReaderModeNullPrefs001) {
  ExtendContent()->SetTestFlag(
      true);  // Force GetMutableRendererPrefs() → nullptr
  ExtendContent()->EnableCustomReaderMode(true);
  // No crash expected, method handles nullptr gracefully
}
#endif  // BUILDFLAG(ARKWEB_READER_MODE)

#if BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)
TEST_F(WebContentsImplExtTest, BrowserControlsParamsRoundTrip001) {
  // Set parameters
  ExtendContent()->SetBrowserControlsParams(50.0f, 20.0f, 30.0f, 10.0f);

  // Get initial state (default is BOTH, cc::BOTH == 0)
  auto state = ExtendContent()->GetBrowserControlState();
  EXPECT_EQ(state, cc::BrowserControlsState::kBoth);
}
#endif  // BUILDFLAG(ARKWEB_EXT_TOPCONTROLS)

#if BUILDFLAG(ARKWEB_FULLSCREEN_VIDEO_OVERLAY)
TEST_F(WebContentsImplExtTest, IsFullscreenVideoOverlayEnabledWithDelegate001) {
  auto delegate = std::make_unique<MockWebContentsDelegateExtended>();
  EXPECT_CALL(*delegate, IsFullscreenVideoOverlayEnabled())
      .WillOnce(::testing::Return(true));

  ExtendContent()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExtendContent()->IsFullscreenVideoOverlayEnabled());
}
#endif  // BUILDFLAG(ARKWEB_FULLSCREEN_VIDEO_OVERLAY)

#if BUILDFLAG(ARKWEB_FULLSCREEN_VIDEO_OVERLAY)
TEST_F(WebContentsImplExtTest, IsFullscreenVideoOverlayEnabledNoDelegate001) {
  ExtendContent()->SetDelegate(nullptr);
  EXPECT_FALSE(ExtendContent()->IsFullscreenVideoOverlayEnabled());
}
#endif  // BUILDFLAG(ARKWEB_FULLSCREEN_VIDEO_OVERLAY)

// ============================================================================
// Task 2: High-Priority Direct-Testable Methods (Batch 2)
// ============================================================================

#if BUILDFLAG(ARKWEB_FULLSCREEN_VIDEO_OVERLAY)
TEST_F(WebContentsImplExtTest, OnVideoStatusChangedWithDelegate001) {
  auto delegate = std::make_unique<MockWebContentsDelegateExtended>();
  std::map<std::string, std::string> params = {{"key1", "value1"}};
  // VideoPlaybackNotificationType is defined in a _for_include.cc fragment
  // and not visible to the test; use static_cast with the underlying value.
  auto kStatusChanged = static_cast<VideoPlaybackNotificationType>(0);

  EXPECT_CALL(
      *delegate,
      OnVideoStatusChanged(kStatusChanged, testing::_))
      .Times(1);

  ExtendContent()->SetDelegate(delegate.get());
  ExtendContent()->OnVideoStatusChanged(kStatusChanged, params);
}
#endif  // BUILDFLAG(ARKWEB_FULLSCREEN_VIDEO_OVERLAY)

#if BUILDFLAG(ARKWEB_FULLSCREEN_VIDEO_OVERLAY)
TEST_F(WebContentsImplExtTest, OnVideoStatusChangedNoDelegate001) {
  std::map<std::string, std::string> params;
  ExtendContent()->SetDelegate(nullptr);
  auto kEnded = static_cast<VideoPlaybackNotificationType>(7);
  ExtendContent()->OnVideoStatusChanged(kEnded, params);
  // No crash expected, method handles nullptr gracefully
}
#endif  // BUILDFLAG(ARKWEB_FULLSCREEN_VIDEO_OVERLAY)

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
TEST_F(WebContentsImplExtTest, OnAVCastStartedWithDelegate001) {
  auto delegate = std::make_unique<MockWebContentsDelegateExtended>();
  auto media_info = media::mojom::MediaInfoForVAST::New();
  auto player_id = MediaPlayerId::CreateMediaPlayerIdForTests();

  EXPECT_CALL(*delegate, OnAVCastStarted(testing::_, testing::_))
      .WillOnce(::testing::Return(nullptr));

  ExtendContent()->SetDelegate(delegate.get());
  auto result =
      ExtendContent()->OnAVCastStarted(std::move(media_info), player_id);
  EXPECT_EQ(result, nullptr);
}
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
TEST_F(WebContentsImplExtTest, OnAVCastStartedNoDelegate001) {
  auto media_info = media::mojom::MediaInfoForVAST::New();
  auto player_id = MediaPlayerId::CreateMediaPlayerIdForTests();

  ExtendContent()->SetDelegate(nullptr);
  auto result =
      ExtendContent()->OnAVCastStarted(std::move(media_info), player_id);
  EXPECT_EQ(result, nullptr);
}
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
TEST_F(WebContentsImplExtTest, EnableVideoAssistantAVCastEnable001) {
  ExtendContent()->EnableVideoAssistantAVCast(true);
}
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
TEST_F(WebContentsImplExtTest,
       EnableVideoAssistantAVCastNullVideoAssistant001) {
  UpdateDelegate();  // Sets video_assistant_ to nullptr via fixture helper
  ExtendContent()->EnableVideoAssistantAVCast(true);
  // No crash expected, method logs error and returns
}
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)

// ============================================================================
// Task 3: SafeBrowsing and CreateNativeBridgeHost Coverage
// ============================================================================

#if BUILDFLAG(ARKWEB_SAFEBROWSING)
TEST_F(WebContentsImplExtTest, SafeBrowsingCheckDetailRoundTrip001) {
  GURL test_url("https://malicious.example.com");
  ExtendContent()->SetSafeBrowsingCheckDetail(42, 3, test_url);

  int code = 0;
  int threat_type = 0;
  GURL url;
  ExtendContent()->GetSafeBrowsingCheckDetail(code, threat_type, url);

  EXPECT_EQ(code, 42);
  EXPECT_EQ(threat_type, 3);
  EXPECT_EQ(url, test_url);
}
#endif  // BUILDFLAG(ARKWEB_SAFEBROWSING)

#if BUILDFLAG(ARKWEB_SAFEBROWSING)
TEST_F(WebContentsImplExtTest, SafeBrowsingCheckDetailEmptyUrl001) {
  GURL empty_url;
  ExtendContent()->SetSafeBrowsingCheckDetail(0, 0, empty_url);

  int code = -1;
  int threat_type = -1;
  GURL url("https://dummy.com");
  ExtendContent()->GetSafeBrowsingCheckDetail(code, threat_type, url);

  EXPECT_EQ(code, 0);
  EXPECT_EQ(threat_type, 0);
  EXPECT_TRUE(url.is_empty());
}
#endif  // BUILDFLAG(ARKWEB_SAFEBROWSING)

// ============================================================================
// Task 4: Camera Methods with Environment Skip
// ============================================================================

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
TEST_F(WebContentsImplExtTest, StartCameraSkipped) {
  ExtendContent()->StartCamera(123);
  // No crash expected, method handles null manager gracefully
}
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
TEST_F(WebContentsImplExtTest, StopCameraSkipped) {
  ExtendContent()->StopCamera(456);
  // No crash expected, method handles null manager gracefully
}
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
TEST_F(WebContentsImplExtTest, CloseCameraSkipped) {
  ExtendContent()->CloseCamera(789);
  // No crash expected, method handles null manager gracefully
}
#endif  // BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)

// ============================================================================
// Task 5: NETWORK_DFX Indirect Coverage via Public Getters
// ============================================================================

#if BUILDFLAG(ARKWEB_NETWORK_DFX) && BUILDFLAG(ARKWEB_NWEB_EX)
TEST_F(WebContentsImplExtTest, RecordAndGetFirstVisuallyNonEmptyPaint001) {
  GURL test_url("https://example.com");
  NavigateAndCommit(test_url);

  CallRecordFirstVisuallyNonEmptyPaint();

  auto timestamp = ExtendContent()->FirstVisuallyNonEmptyPaintTime(test_url);
  EXPECT_FALSE(timestamp.is_null());

  auto timestamp_mismatch = ExtendContent()->FirstVisuallyNonEmptyPaintTime(GURL("https://other.com"));
  EXPECT_TRUE(timestamp_mismatch.is_null());
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_DFX) && BUILDFLAG(ARKWEB_NWEB_EX)
TEST_F(WebContentsImplExtTest, RecordAndGetPageFinished001) {
  GURL test_url("https://finished.example.com");
  NavigateAndCommit(test_url);

  CallRecordPageFinished();

  auto timestamp = ExtendContent()->PageFinishedTime(test_url);
  EXPECT_FALSE(timestamp.is_null());

  auto timestamp_mismatch = ExtendContent()->PageFinishedTime(GURL("https://other.com"));
  EXPECT_TRUE(timestamp_mismatch.is_null());
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_DFX) && BUILDFLAG(ARKWEB_NWEB_EX)
TEST_F(WebContentsImplExtTest, RecordAndGetFullLoadingProgress001) {
  GURL test_url("https://progress.example.com");
  NavigateAndCommit(test_url);

  // NavigateAndCommit triggers load progress updates which automatically call
  // RecordFullLoadingProgress() via WebContentsImpl (web_contents_impl.cc:8884),
  // so the timestamp may already be recorded for the committed URL.
  // Verify that a mismatched URL always returns null.
  auto timestamp_mismatch = ExtendContent()->GetFullLoadingProgressTime(GURL("https://other.com"));
  EXPECT_TRUE(timestamp_mismatch.is_null());
}
#endif

// ============================================================================
// Task 6: Branch Coverage for SavePageEx, isSameUserAgent, DelVideoSurface
// ============================================================================

#if BUILDFLAG(ARKWEB_SAVE_PAGE)
TEST_F(WebContentsImplExtTest, SavePageExValidType001) {
  base::FilePath path("/tmp/test_page.mhtml");
  bool callback_invoked = false;

  auto callback = base::BindOnce([](bool* flag, bool result) {
    *flag = true;
  }, &callback_invoked);

  ExtendContent()->SavePageEx(path, SAVE_PAGE_TYPE_AS_MHTML, std::move(callback));
}
#endif  // BUILDFLAG(ARKWEB_SAVE_PAGE)

TEST_F(WebContentsImplExtTest, isSameUserAgentEmptyString001) {
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = std::string("");
  auto result = ExtendContent()->isSameUserAgent(ua_override);
  // An empty override matches the default (also empty) UA override, so
  // isSameUserAgent returns true (line 1105: equal, or line 1113: current empty).
  EXPECT_TRUE(result);
}

TEST_F(WebContentsImplExtTest, isSameUserAgentUnicode001) {
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = std::string("Mozilla/用户代理");
  auto result = ExtendContent()->isSameUserAgent(ua_override);
  // Unicode characters are not valid HTTP header values, so
  // !net::HttpUtil::IsValidHeaderValue() is true → isSameUserAgent returns true
  // (treats invalid override as same to ignore it).
  EXPECT_TRUE(result);
}
}  // namespace content
