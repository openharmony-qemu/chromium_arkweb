// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// The class under test and several of its collaborators (content::RenderFrame,
// ChromeContentRendererClient) seal away members that the tests need to either
// access directly or override. `content::RenderFrame` in particular has a
// private constructor (friend-locked to RenderFrameImpl), which makes it
// impossible to subclass for mocking unless the access specifier is relaxed.
// The two macros below open up `private`/`protected` for the remainder of this
// translation unit so that:
//   * RenderFrame can be subclassed by FakeRenderFrameBase/MockRenderFrame,
//   * ChromeContentRendererClient::subresource_filter_user_ruleset_dealer_ and
//     chrome_observer_ can be inspected/manipulated from the tests.
// This is a well-known test-only idiom; it is not used in production code.
#define private public    // NOLINT(build/define_used)
#define protected public  // NOLINT(build/define_used)

#include "arkweb/chromium_ext/chrome/renderer/arkweb_chrome_content_renderer_client_ext.h"
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/range/range.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

// The subresource filter is the one collaborator that is both abstract and
// reasonably small, so it can be fully GMock-mocked. It is exercised in a
// standalone compile/behaviour check below; the production code paths that
// consume it (TriggerElementHidingInFrame/TriggerUserElementHidingInFrame)
// additionally require a live blink::WebLocalFrame/WebDocument which cannot be
// unit-tested here (see the GTEST_SKIP cases further down).
class MockWebDocumentSubresourceFilter
    : public blink::WebDocumentSubresourceFilter {
 public:
  ~MockWebDocumentSubresourceFilter() override = default;

  MOCK_METHOD(blink::WebDocumentSubresourceFilter::LoadPolicy,
              GetLoadPolicy,
              (const blink::WebURL&,
               network::mojom::RequestDestination,
               subresource_filter::ScopedRule*),
              (override));
  MOCK_METHOD(blink::WebDocumentSubresourceFilter::LoadPolicy,
              GetLoadPolicyForWebSocketConnect,
              (const blink::WebURL&),
              (override));
  MOCK_METHOD(blink::WebDocumentSubresourceFilter::LoadPolicy,
              GetLoadPolicyForWebTransportConnect,
              (const blink::WebURL&),
              (override));
  MOCK_METHOD(void, ClearStatistics, (), (override));
  MOCK_METHOD(std::unique_ptr<std::string>,
              GetElementHidingSelectors,
              (const blink::WebURL&, bool),
              (override));
  MOCK_METHOD(bool,
              HasGenericHideTypeOption,
              (const blink::WebURL&, const url::Origin&),
              (override));
  MOCK_METHOD(bool,
              HasElemHideTypeOption,
              (const blink::WebURL&, const url::Origin&),
              (override));
  MOCK_METHOD(bool,
              HasDocumentTypeOption,
              (const blink::WebURL&, const url::Origin&),
              (override));
  MOCK_METHOD(void,
              DidMatchCssRule,
              (const blink::WebURL&, const std::string&, bool),
              (override));
  MOCK_METHOD(void, SetDidFinishLoad, (bool), (override));
  MOCK_METHOD(bool, GetDidFinishLoad, (), (override));
  MOCK_METHOD(std::unique_ptr<std::vector<std::string>>,
              GetUserDomPathSelectors,
              (const blink::WebURL&, bool),
              (override));
  MOCK_METHOD(void, set_activation_state, (bool), (override));
  MOCK_METHOD(void, ReportDisallowedLoad, (), (override));
  MOCK_METHOD(bool, ShouldLogToConsole, (), (override));
};

// A concrete base that provides trivial, non-crashing implementations for every
// pure virtual on content::RenderFrame. This is required because RenderFrame
// declares ~40 pure virtuals with heavy signatures; mocking them all with
// MOCK_METHOD would be unwieldy and fragile. MockRenderFrame then GMock-mocks
// only the handful of methods the code under test actually exercises.
class FakeRenderFrameBase : public content::RenderFrame {
 public:
  FakeRenderFrameBase() = default;
  ~FakeRenderFrameBase() override = default;

  content::RenderFrame* GetMainRenderFrame() override { return nullptr; }
  content::RenderAccessibility* GetRenderAccessibility() override {
    return nullptr;
  }
  std::unique_ptr<content::AXTreeSnapshotter> CreateAXTreeSnapshotter(
      ui::AXMode) override {
    return nullptr;
  }
  blink::WebView* GetWebView() override { return nullptr; }
  const blink::WebView* GetWebView() const override { return nullptr; }
  blink::WebLocalFrame* GetWebFrame() override { return nullptr; }
  const blink::WebLocalFrame* GetWebFrame() const override { return nullptr; }
  const blink::web_pref::WebPreferences& GetBlinkPreferences() override {
    return blink_preferences_;
  }
  void ShowVirtualKeyboard() override {}
  void ExecuteJavaScript(const std::u16string&) override {}
  bool IsMainFrame() override { return false; }
  bool IsInFencedFrameTree() const override { return false; }
  bool IsHidden() override { return false; }
  void BindLocalInterface(const std::string&,
                          mojo::ScopedMessagePipeHandle) override {}
  const blink::BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker()
      override {
    return blink::GetEmptyBrowserInterfaceBroker();
  }
  blink::AssociatedInterfaceRegistry* GetAssociatedInterfaceRegistry()
      override {
    return nullptr;
  }
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override {
    return nullptr;
  }
  void SetSelectedText(const std::u16string&,
                       size_t,
                       const gfx::Range&) override {}
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel,
                           const std::string&) override {}
  bool IsPasting() override { return false; }
  void LoadHTMLStringForTesting(std::string_view,
                                const GURL&,
                                const std::string&,
                                const GURL&,
                                bool) override {}
  bool IsRequestingNavigation() override { return false; }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      blink::TaskType) override {
    return nullptr;
  }
  content::BindingsPolicySet GetEnabledBindings() override {
    return content::BindingsPolicySet();
  }
  void SetAccessibilityModeForTest(ui::AXMode) override {}
  const content::RenderFrameMediaPlaybackOptions&
  GetRenderFrameMediaPlaybackOptions() override {
    return media_playback_options_;
  }
  void SetRenderFrameMediaPlaybackOptions(
      const content::RenderFrameMediaPlaybackOptions&) override {}
  void SetAllowsCrossBrowsingInstanceFrameLookup() override {}
  gfx::RectF ElementBoundsInWindow(const blink::WebElement&) override {
    return gfx::RectF();
  }
  gfx::Rect ConvertViewportToWindow(const gfx::Rect&) override {
    return gfx::Rect();
  }
  float GetDeviceScaleFactor() override { return 1.0f; }
  blink::scheduler::WebAgentGroupScheduler& GetAgentGroupScheduler() override {
    return *agent_group_scheduler_;
  }
#if BUILDFLAG(ARKWEB_ADBLOCK)
  bool GetGlobalAdblockEnabled() override { return false; }
#endif
#if BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
  void OnDocumentEndReady() override {}
#endif
  void SetNewFeatureUsageCallback(
      content::RenderFrame::NewFeatureUsageCallback) override {}
  void SetSubresourceLoadCallback(
      content::RenderFrame::SubresourceLoadCallback) override {}
  void SetLoadFromMemoryCacheCallback(
      content::RenderFrame::LoadFromMemoryCacheCallback) override {}
  void SetDidStartResponseCallback(
      content::RenderFrame::DidStartResponseCallback) override {}
  void SetDidCompleteResponseCallback(
      content::RenderFrame::DidCompleteResponseCallback) override {}
  void SetDidCancelResponseCallback(
      content::RenderFrame::DidCancelResponseCallback) override {}

 private:
  blink::web_pref::WebPreferences blink_preferences_;
  content::RenderFrameMediaPlaybackOptions media_playback_options_;
  std::unique_ptr<blink::scheduler::WebAgentGroupScheduler>
      agent_group_scheduler_ =
          blink::scheduler::WebAgentGroupScheduler::CreateForTesting();
};

// MockRenderFrame exposes the few RenderFrame methods that the code under test
// actually depends on (web frame access, main-frame topology, adblock switch,
// task runner, weak pointer). Everything else is answered trivially by
// FakeRenderFrameBase.
class MockRenderFrame : public FakeRenderFrameBase {
 public:
  MockRenderFrame() = default;
  ~MockRenderFrame() override = default;

  MOCK_METHOD(blink::WebLocalFrame*, GetWebFrame, (), (override));
  MOCK_METHOD(bool, IsMainFrame, (), (override));
  MOCK_METHOD(content::RenderFrame*, GetMainRenderFrame, (), (override));
  MOCK_METHOD(scoped_refptr<base::SingleThreadTaskRunner>,
              GetTaskRunner,
              (blink::TaskType),
              (override));
#if BUILDFLAG(ARKWEB_ADBLOCK)
  MOCK_METHOD(bool, GetGlobalAdblockEnabled, (), (override));

  base::WeakPtr<content::RenderFrame> GetRenderFrameWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }
#endif

#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
  MOCK_METHOD(gfx::Vector2dF, GetOverScrollOffset, (), (override));
#endif
#if BUILDFLAG(ARKWEB_PDF)
  MOCK_METHOD(void, OnPdfScrollAtBottom, (const std::string& url), (override));
  MOCK_METHOD(void,
              OnPdfLoadEvent,
              (int32_t result, const std::string& url),
              (override));
#endif  // BUILDFLAG(ARKWEB_PDF)
#if BUILDFLAG(ARKWEB_READER_MODE)
  MOCK_METHOD(bool, IsReaderModeEnabled, (), (override));
  MOCK_METHOD(bool, IsCustomReaderModeEnabled, (), (override));
  MOCK_METHOD(void,
              ReportDistillableResult,
              (const std::string& event_type, const std::string& value),
              (override));
#endif

  void InvalidateWeakPtrs() { weak_factory_.InvalidateWeakPtrs(); }

 private:
  base::WeakPtrFactory<MockRenderFrame> weak_factory_{this};
};

class ArkWebChromeContentRendererClientExtTest : public ::testing::Test {
 protected:
  // The ChromeContentRendererClient base constructor touches the sampling
  // profiler, so a task environment must be live before |client_| is created.
  base::test::TaskEnvironment task_environment_;

  void SetUp() override {
    blink::Platform::InitializeBlink();

    // Renderer-side code keys off the renderer process type; mirror the setup
    // used by the existing chrome_content_renderer_client_unittest.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kProcessType, switches::kRendererProcess);
    // Ensure a clean starting point for the NwebEx switch across tests.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kEnableNwebEx);
    client_ = std::make_unique<ArkWebChromeContentRendererClientExt>();
  }

  void TearDown() override {
    client_.reset();
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kEnableNwebEx);
  }

  void EnableNwebExSwitch() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableNwebEx);
  }

  ArkWebChromeContentRendererClientExt* client() { return client_.get(); }

 private:
  std::unique_ptr<ArkWebChromeContentRendererClientExt> client_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Construction / identity
// ---------------------------------------------------------------------------

// Verifies the constructor and destructor run cleanly and that the arkweb
// subtype is reachable through the polymorphic accessors.
TEST_F(ArkWebChromeContentRendererClientExtTest,
       AsArkWebChromeContentRendererClientExt_ReturnsThis) {
  EXPECT_EQ(client()->AsArkWebChromeContentRendererClientExt(), client());
  EXPECT_NE(client()->AsArkWebChromeContentRendererClientExt(), nullptr);
}

// ---------------------------------------------------------------------------
// RenderFrameCreatedContentSettings  [ARKWEB_NETWORK_BASE &&
// ARKWEB_EXT_EXCEPTION_LIST]
//   Branches:
//    A) switch present + observer non-null + content_settings non-null ->
//       calls SetContentSettingRules (see SKIP case: needs a live
//       ContentSettingsAgentImpl, whose constructor dereferences the render
//       frame and therefore cannot be unit-tested).
//    B) switch absent -> short-circuits, no call.
//    C) observer null -> short-circuits, no call.
//    D) content_settings null -> short-circuits, no call (needs a non-null
//       observer to be reachable; see SKIP case).
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_NETWORK_BASE) && BUILDFLAG(ARKWEB_EXT_EXCEPTION_LIST)
TEST_F(ArkWebChromeContentRendererClientExtTest,
       RenderFrameCreatedContentSettings_WhenSwitchAbsent_NoOp) {
  // Switch is absent (cleared in SetUp). The very first condition fails, so the
  // content_settings pointer is never dereferenced.
  client()->RenderFrameCreatedContentSettings(
      /*content_settings=*/nullptr, client());

  SUCCEED();
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       RenderFrameCreatedContentSettings_WhenObserverNull_NoOp) {
  EnableNwebExSwitch();
  // chrome_observer_ is null because RenderThreadStarted() was never called.
  EXPECT_EQ(client()->chrome_observer_.get(), nullptr);

  client()->RenderFrameCreatedContentSettings(
      /*content_settings=*/nullptr, client());

  SUCCEED();
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       RenderFrameCreatedContentSettings_WhenContentSettingsNull_NoOp) {
  EnableNwebExSwitch();
  // To genuinely exercise the content_settings-null short-circuit, the observer
  // must look non-null. Inject a dangling raw pointer (never dereferenced, as
  // the null content_settings check short-circuits first) and release it back
  // before destruction so no delete runs.
  client()->chrome_observer_.reset(
      reinterpret_cast<ChromeRenderThreadObserver*>(0x1000));
  client()->RenderFrameCreatedContentSettings(
      /*content_settings=*/nullptr, client());
  (void)client()->chrome_observer_.release();

  SUCCEED();
}

// The "all conditions met -> calls SetContentSettingRules" branch requires a
// real content_settings::ContentSettingsAgentImpl. Its constructor dereferences
// the render frame (to obtain a WebLocalFrame and install the content settings
// client), which is not feasible without a live RenderFrameImpl. This must be
// covered by a ChromeRenderViewTest integration test.
TEST_F(ArkWebChromeContentRendererClientExtTest,
       DISABLED_RenderFrameCreatedContentSettings_AllConditionsMet_CallsRules) {
  GTEST_SKIP() << "Requires a live ContentSettingsAgentImpl/observer; cover "
                  "via ChromeRenderViewTest.";
}
#endif

// ---------------------------------------------------------------------------
// PostIOThreadCreated  [ARKWEB_NOTIFICATION]
//   Branch A: kEnableNwebEx absent -> disables Notifications + PushMessaging.
//   Branch B: kEnableNwebEx present -> leaves them untouched.
//   The base ChromeContentRendererClient::PostIOThreadCreated posts a profiler
//   task; we feed it a TestSimpleTaskRunner so that task is never run.
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_NOTIFICATION)
TEST_F(ArkWebChromeContentRendererClientExtTest,
       PostIOThreadCreated_WhenSwitchAbsent_DisablesNotificationsAndPush) {
  // Switch absent (cleared in SetUp). Pre-enable the features to guarantee a
  // deterministic before-state regardless of prior tests.
  blink::WebRuntimeFeatures::EnableNotifications(true);
  blink::WebRuntimeFeatures::EnablePushMessaging(true);
  ASSERT_TRUE(blink::WebRuntimeFeatures::IsNotificationsEnabled());
  ASSERT_TRUE(blink::WebRuntimeFeatures::IsPushMessagingEnabled());

  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  client()->PostIOThreadCreated(runner.get());

  EXPECT_FALSE(blink::WebRuntimeFeatures::IsNotificationsEnabled());
  EXPECT_FALSE(blink::WebRuntimeFeatures::IsPushMessagingEnabled());
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       PostIOThreadCreated_WhenSwitchPresent_KeepsFeaturesEnabled) {
  EnableNwebExSwitch();
  blink::WebRuntimeFeatures::EnableNotifications(true);
  blink::WebRuntimeFeatures::EnablePushMessaging(true);

  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  client()->PostIOThreadCreated(runner.get());

  // The switch being present must skip the two Disable calls.
  EXPECT_TRUE(blink::WebRuntimeFeatures::IsNotificationsEnabled());
  EXPECT_TRUE(blink::WebRuntimeFeatures::IsPushMessagingEnabled());
}
#endif

// ---------------------------------------------------------------------------
// GetAdBlockEnabledByFrame  [ARKWEB_ADBLOCK]
//   Branches reachable without a live blink::WebLocalFrame:
//    1. null weak ptr                                   -> false
//    2. global adblock disabled                         -> false
//    3. main frame, null web frame, null main render    -> false (falls through
//                                                         to subframe path)
//    4. subframe, null main render frame                -> false
//    5. subframe, main render frame, null main web frame-> false
//   Branches that need WebLocalFrame::GetAdBlockEnabled (live frame) -> SKIP:
//    6. main frame + non-null web frame                 -> GetAdBlockEnabled()
//    7. subframe + main render + non-null main web frame-> GetAdBlockEnabled()
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_ADBLOCK)
TEST_F(ArkWebChromeContentRendererClientExtTest,
       GetAdBlockEnabledByFrame_WhenWeakPtrNull_ReturnsFalse) {
  EXPECT_FALSE(client()->GetAdBlockEnabledByFrame(
      base::WeakPtr<content::RenderFrame>()));
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       GetAdBlockEnabledByFrame_WhenGlobalAdblockDisabled_ReturnsFalse) {
  NiceMock<MockRenderFrame> frame;
  ON_CALL(frame, GetGlobalAdblockEnabled).WillByDefault(Return(false));

  EXPECT_FALSE(
      client()->GetAdBlockEnabledByFrame(frame.GetRenderFrameWeakPtr()));
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       GetAdBlockEnabledByFrame_MainFrameNullWebFrameFallsThroughToSubframe) {
  NiceMock<MockRenderFrame> frame;
  ON_CALL(frame, GetGlobalAdblockEnabled).WillByDefault(Return(true));
  ON_CALL(frame, IsMainFrame).WillByDefault(Return(true));
  ON_CALL(frame, GetWebFrame).WillByDefault(Return(nullptr));
  ON_CALL(frame, GetMainRenderFrame).WillByDefault(Return(nullptr));

  // Main frame but null web frame: the main-frame block does not return, and
  // the subframe path then finds a null main render frame.
  EXPECT_FALSE(
      client()->GetAdBlockEnabledByFrame(frame.GetRenderFrameWeakPtr()));
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       GetAdBlockEnabledByFrame_SubframeNullMainRenderFrame_ReturnsFalse) {
  NiceMock<MockRenderFrame> frame;
  ON_CALL(frame, GetGlobalAdblockEnabled).WillByDefault(Return(true));
  ON_CALL(frame, IsMainFrame).WillByDefault(Return(false));
  ON_CALL(frame, GetMainRenderFrame).WillByDefault(Return(nullptr));

  EXPECT_FALSE(
      client()->GetAdBlockEnabledByFrame(frame.GetRenderFrameWeakPtr()));
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       GetAdBlockEnabledByFrame_SubframeMainWebFrameNull_ReturnsFalse) {
  NiceMock<MockRenderFrame> frame;
  NiceMock<MockRenderFrame> main_frame;
  ON_CALL(frame, GetGlobalAdblockEnabled).WillByDefault(Return(true));
  ON_CALL(frame, IsMainFrame).WillByDefault(Return(false));
  ON_CALL(frame, GetMainRenderFrame).WillByDefault(Return(&main_frame));
  ON_CALL(main_frame, GetWebFrame).WillByDefault(Return(nullptr));

  EXPECT_FALSE(
      client()->GetAdBlockEnabledByFrame(frame.GetRenderFrameWeakPtr()));
}

TEST_F(
    ArkWebChromeContentRendererClientExtTest,
    DISABLED_GetAdBlockEnabledByFrame_MainFrameWebFrame_ReadsAdBlockEnabled) {
  GTEST_SKIP()
      << "Needs a live blink::WebLocalFrame to return GetAdBlockEnabled"
      << "; cover via ChromeRenderViewTest.";
}

TEST_F(
    ArkWebChromeContentRendererClientExtTest,
    DISABLED_GetAdBlockEnabledByFrame_SubframeMainWebFrame_ReadsAdBlockEnabled) {
  GTEST_SKIP()
      << "Needs a live blink::WebLocalFrame to return GetAdBlockEnabled"
      << "; cover via ChromeRenderViewTest.";
}
#endif

// ---------------------------------------------------------------------------
// TriggerElementHidingInFrame  [ARKWEB_ADBLOCK, ARKWEB_LOGGER_REPORT]
//   Reachable branches:
//    1. null weak ptr          -> LOG ERROR + return
//    2. null web frame         -> LOG ERROR + return
//   The remaining branches (null filter, adblock disabled, scheme, $document,
//   $elemhide, $generichide, selectors empty/non-empty + InsertStyleSheet) all
//   require a live blink::WebLocalFrame (GetDocumentLoader/GetDocument) and a
//   real blink::WebDocument (whose Url()/InsertStyleSheet() dereference an
//   internal Document*). These are covered by integration tests; see SKIPs.
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_ADBLOCK)
TEST_F(ArkWebChromeContentRendererClientExtTest,
       TriggerElementHidingInFrame_WhenWeakPtrNull_NoCrash) {
  client()->TriggerElementHidingInFrame(base::WeakPtr<content::RenderFrame>());
  SUCCEED();
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       TriggerElementHidingInFrame_WhenWebFrameNull_NoCrash) {
  NiceMock<MockRenderFrame> frame;
  ON_CALL(frame, GetWebFrame).WillByDefault(Return(nullptr));

  client()->TriggerElementHidingInFrame(frame.GetRenderFrameWeakPtr());
  SUCCEED();
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       DISABLED_TriggerElementHidingInFrame_NullFilter_ReturnsEarly) {
  GTEST_SKIP() << "Needs a live WebLocalFrame::GetDocumentLoader(); cover via "
                  "ChromeRenderViewTest.";
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       DISABLED_TriggerElementHidingInFrame_AdblockDisabled_ReturnsEarly) {
  GTEST_SKIP() << "Needs a live WebLocalFrame to reach the filter/adblock "
                  "checks; cover via ChromeRenderViewTest.";
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       DISABLED_TriggerElementHidingInFrame_NonHttpScheme_ReturnsEarly) {
  GTEST_SKIP() << "Needs a live blink::WebDocument to inspect Url(); cover via "
                  "ChromeRenderViewTest.";
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       DISABLED_TriggerElementHidingInFrame_DocumentOption_ReturnsEarly) {
  GTEST_SKIP() << "Needs a live WebLocalFrame::GetHasDocumentTypeOption(); "
                  "cover via ChromeRenderViewTest.";
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       DISABLED_TriggerElementHidingInFrame_ElemHideOption_ReturnsEarly) {
  GTEST_SKIP() << "Needs a live WebLocalFrame::GetHasElemHideTypeOption(); "
                  "cover via ChromeRenderViewTest.";
}

TEST_F(
    ArkWebChromeContentRendererClientExtTest,
    DISABLED_TriggerElementHidingInFrame_GenericHideOption_LogsAndContinues) {
  GTEST_SKIP() << "Needs a live WebLocalFrame::GetHasGenericHideTypeOption(); "
                  "cover via ChromeRenderViewTest.";
}

TEST_F(
    ArkWebChromeContentRendererClientExtTest,
    DISABLED_TriggerElementHidingInFrame_NonEmptySelectors_InsertsStyleSheet) {
  GTEST_SKIP() << "Needs a live WebDocument::InsertStyleSheet(); cover via "
                  "ChromeRenderViewTest.";
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       DISABLED_TriggerElementHidingInFrame_EmptySelectors_ResetsAndReturns) {
  GTEST_SKIP() << "Needs a live WebLocalFrame/WebDocument; cover via "
                  "ChromeRenderViewTest.";
}
#endif

// ---------------------------------------------------------------------------
// TriggerUserElementHidingInFrame  [ARKWEB_ADBLOCK, ARKWEB_LOGGER_REPORT]
//   Reachable branches:
//    1. null weak ptr -> LOG ERROR + return
//    2. null web frame -> return
//   Remaining branches (scheme, null user filter, selectors empty/non-empty +
//   InsertStyleSheet) need a live WebLocalFrame/WebDocument -> SKIP.
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_ADBLOCK)
TEST_F(ArkWebChromeContentRendererClientExtTest,
       TriggerUserElementHidingInFrame_WhenWeakPtrNull_NoCrash) {
  client()->TriggerUserElementHidingInFrame(
      base::WeakPtr<content::RenderFrame>());
  SUCCEED();
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       TriggerUserElementHidingInFrame_WhenWebFrameNull_NoCrash) {
  NiceMock<MockRenderFrame> frame;
  ON_CALL(frame, GetWebFrame).WillByDefault(Return(nullptr));

  client()->TriggerUserElementHidingInFrame(frame.GetRenderFrameWeakPtr());
  SUCCEED();
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       DISABLED_TriggerUserElementHidingInFrame_NonHttpScheme_ReturnsEarly) {
  GTEST_SKIP()
      << "Needs a live WebDocument::Url(); cover via ChromeRenderViewTest.";
}

TEST_F(ArkWebChromeContentRendererClientExtTest,
       DISABLED_TriggerUserElementHidingInFrame_NullUserFilter_ReturnsEarly) {
  GTEST_SKIP()
      << "Needs a live WebLocalFrame::GetDocumentLoader()/"
         "GetWebUserSubresourceFilter(); cover via ChromeRenderViewTest.";
}

TEST_F(
    ArkWebChromeContentRendererClientExtTest,
    DISABLED_TriggerUserElementHidingInFrame_NonEmptySelectors_InsertsStyleSheet) {
  GTEST_SKIP() << "Needs a live WebDocument::InsertStyleSheet(); cover via "
                  "ChromeRenderViewTest.";
}

TEST_F(
    ArkWebChromeContentRendererClientExtTest,
    DISABLED_TriggerUserElementHidingInFrame_EmptySelectors_ResetsAndReturns) {
  GTEST_SKIP() << "Needs a live WebLocalFrame/WebDocument; cover via "
                  "ChromeRenderViewTest.";
}
#endif

// ---------------------------------------------------------------------------
// RenderFrameCreateSubresourceFilterAgent  [ARKWEB_ADBLOCK]
//   Branch A: subresource_filter_user_ruleset_dealer_ null -> no agent created.
//   Branch B: non-null dealer -> constructs UserSubresourceFilterAgent +
//   Initialize(). Requires a real UserUnverifiedRulesetDealer and live frame ->
//   SKIP.
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_ADBLOCK)
TEST_F(ArkWebChromeContentRendererClientExtTest,
       RenderFrameCreateSubresourceFilterAgent_WhenDealerNull_NoOp) {
  // The dealer is null because RenderThreadStarted() was never invoked.
  EXPECT_FALSE(client()->subresource_filter_user_ruleset_dealer_);

  client()->RenderFrameCreateSubresourceFilterAgent(
      /*render_frame=*/nullptr, client());
  SUCCEED();
}

TEST_F(
    ArkWebChromeContentRendererClientExtTest,
    DISABLED_RenderFrameCreateSubresourceFilterAgent_WhenDealerNonNull_CreatesAgent) {
  GTEST_SKIP() << "Needs a real UserUnverifiedRulesetDealer + live RenderFrame "
                  "to construct UserSubresourceFilterAgent; cover via "
                  "ChromeRenderViewTest.";
}
#endif

// ---------------------------------------------------------------------------
// RenderFrameCreateSubresourceFilterAgentTriggerHide  [ARKWEB_ADBLOCK]
//   Posts TriggerElementHidingInFrame + TriggerUserElementHidingInFrame onto
//   the DOMManipulation task runner. Verify exactly two tasks are posted and
//   that the weak pointer is captured correctly.
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_ADBLOCK)
TEST_F(ArkWebChromeContentRendererClientExtTest,
       RenderFrameCreateSubresourceFilterAgentTriggerHide_PostsTwoTasks) {
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  NiceMock<MockRenderFrame> frame;
  ON_CALL(frame, GetTaskRunner).WillByDefault(Return(runner));

  client()->RenderFrameCreateSubresourceFilterAgentTriggerHide(&frame);

  EXPECT_EQ(runner->NumPendingTasks(), 2u);

  // Running the queued tasks must not crash: the captured weak ptr is still
  // valid at this point, so Trigger* will call GetWebFrame() (returns nullptr
  // by default) and bail out at the "!web_frame" guard.
  runner->RunPendingTasks();
  EXPECT_EQ(runner->NumPendingTasks(), 0u);
}

TEST_F(
    ArkWebChromeContentRendererClientExtTest,
    RenderFrameCreateSubresourceFilterAgentTriggerHide_DeadWeakPtrSkipsWork) {
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  NiceMock<MockRenderFrame> frame;
  ON_CALL(frame, GetTaskRunner).WillByDefault(Return(runner));

  client()->RenderFrameCreateSubresourceFilterAgentTriggerHide(&frame);
  ASSERT_EQ(runner->NumPendingTasks(), 2u);

  // The render frame may be torn down before the posted tasks execute; the weak
  // ptr becomes null and both Trigger* helpers must bail out at the very first
  // guard without touching any collaborator.
  frame.InvalidateWeakPtrs();
  runner->RunPendingTasks();
  EXPECT_EQ(runner->NumPendingTasks(), 0u);
}
#endif

// ---------------------------------------------------------------------------
// Mock sanity check: the GMock subresource filter compiles and is callable.
// ---------------------------------------------------------------------------

TEST_F(ArkWebChromeContentRendererClientExtTest,
       MockWebDocumentSubresourceFilter_IsUsable) {
  MockWebDocumentSubresourceFilter filter;
  EXPECT_CALL(filter, GetElementHidingSelectors(_, _))
      .WillOnce(
          Return(std::make_unique<std::string>("body { display: none }")));

  auto result = filter.GetElementHidingSelectors(blink::WebURL(), true);
  EXPECT_NE(result, nullptr);
  EXPECT_FALSE(result->empty());
}
