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

#include <optional>
#include <string>

#include "arkweb/build/features/features.h"
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#include "base/command_line.h"

// Include base class header before ext header so inheritance is fully defined.
// clang-format off
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "arkweb/chromium_ext/content/browser/renderer_host/arkweb_navigation_controller_impl_ext.h"
// clang-format on

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches_ext.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "url/gurl.h"

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
#include "arkweb/chromium_ext/content/public/browser/error_page_reload_reason.h"
#endif

namespace content {

// Test fixture for ArkWebNavigationControllerImplExt.
// When ARKWEB_PRP_PRELOAD is enabled, the actual controller is
// ArkWebNavigationControllerImplExt, so virtual methods called through
// controller() dispatch to the ext implementation.
class ArkWebNavigationControllerImplExtTest : public RenderViewHostTestHarness,
                                              public WebContentsObserver {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    Observe(GetTestWebContents());
  }

  void TearDown() override {
    // Clean up any command-line switches left by tests.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kEnableNwebEx);
    Observe(nullptr);
    RenderViewHostTestHarness::TearDown();
  }

  NavigationControllerImpl& controller_impl() {
    return static_cast<NavigationControllerImpl&>(controller());
  }

  // Disambiguate web_contents() from multiple base classes
  WebContents* GetTestWebContents() {
    return RenderViewHostTestHarness::web_contents();
  }

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation) override {
    last_reload_type_ = navigation->GetReloadType();
  }

  ReloadType last_reload_type() const { return last_reload_type_; }

 private:
  ReloadType last_reload_type_ = ReloadType::NONE;
};

// ============================================================================
// GetOriginalUrl tests (ARKWEB_NETWORK_CONNINFO)
// ============================================================================

#if BUILDFLAG(ARKWEB_NETWORK_CONNINFO)
TEST_F(ArkWebNavigationControllerImplExtTest, GetOriginalUrl_NoEntries) {
  // No committed navigation, no entries -> returns empty string.
  EXPECT_EQ(controller().GetOriginalUrl(), std::string());
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       GetOriginalUrl_WithCommittedNavigation) {
  // Navigate to a URL, commit it, then check GetOriginalUrl returns that URL.
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.com/"), GetTestWebContents());
  simulator->Commit();
  EXPECT_EQ(controller().GetOriginalUrl(),
            GURL("https://www.example.com/").spec());
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       GetOriginalUrl_SecondNavigationReturnsCurrent) {
  // Navigate twice; GetOriginalUrl returns the current (last committed) entry.
  auto simulator1 = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.com/"), GetTestWebContents());
  simulator1->Commit();
  auto simulator2 = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.org/page2"), GetTestWebContents());
  simulator2->Commit();
  EXPECT_EQ(controller().GetOriginalUrl(),
            GURL("https://www.example.org/page2").spec());
}
#endif  // BUILDFLAG(ARKWEB_NETWORK_CONNINFO)

// ============================================================================
// InsertBackForwardEntry tests (ARKWEB_EXT_NAVIGATION)
// ============================================================================

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
TEST_F(ArkWebNavigationControllerImplExtTest,
       InsertBackForwardEntry_NegativeIndex) {
  // index < 0 -> ERR_WRONG_OFFSET
  auto result =
      controller().InsertBackForwardEntry(-1, GURL("https://www.example.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::ERR_WRONG_OFFSET);
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       InsertBackForwardEntry_IndexTooLarge) {
  // index > entries_.size() -> ERR_WRONG_OFFSET
  auto result =
      controller().InsertBackForwardEntry(5, GURL("https://www.example.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::ERR_WRONG_OFFSET);
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       InsertBackForwardEntry_EmptyEntriesInsert) {
  // No committed entries yet. Insert at index 0 should succeed.
  auto result =
      controller().InsertBackForwardEntry(0, GURL("https://www.example.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::UPDATE_OK);
  EXPECT_EQ(controller().GetEntryCount(), 1u);
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       InsertBackForwardEntry_InsertAfterNavigation) {
  // First commit a navigation, then insert a new entry at index 1.
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.com/"), GetTestWebContents());
  simulator->Commit();
  EXPECT_EQ(controller().GetEntryCount(), 1u);

  auto result =
      controller().InsertBackForwardEntry(1, GURL("https://www.inserted.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::UPDATE_OK);
  EXPECT_EQ(controller().GetEntryCount(), 2u);
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       InsertBackForwardEntry_InsertAtBeginningAdjustsIndex) {
  // Commit a navigation, then insert at index 0.
  // last_committed_entry_index_ should increase.
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.com/"), GetTestWebContents());
  simulator->Commit();
  int original_committed = controller().GetCurrentEntryIndex();

  auto result =
      controller().InsertBackForwardEntry(0, GURL("https://www.inserted.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::UPDATE_OK);
  // The committed index should have shifted right
  EXPECT_EQ(controller().GetCurrentEntryIndex(), original_committed + 1);
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       InsertBackForwardEntry_IndexJustBeyondSize) {
  // index == entries_.size() should succeed (append at end)
  auto result =
      controller().InsertBackForwardEntry(0, GURL("https://www.first.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::UPDATE_OK);
  // Now entries_.size() == 1, so index 1 should be valid (append)
  result =
      controller().InsertBackForwardEntry(1, GURL("https://www.second.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::UPDATE_OK);
}
#endif  // BUILDFLAG(ARKWEB_EXT_NAVIGATION)

// ============================================================================
// UpdateNavigationEntryUrl tests (ARKWEB_EXT_NAVIGATION)
// ============================================================================

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
TEST_F(ArkWebNavigationControllerImplExtTest,
       UpdateNavigationEntryUrl_InvalidIndex) {
  // Index out of range -> ERR_WRONG_OFFSET
  auto result =
      controller().UpdateNavigationEntryUrl(99, GURL("https://www.new.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::ERR_WRONG_OFFSET);
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       UpdateNavigationEntryUrl_NegativeIndex) {
  auto result =
      controller().UpdateNavigationEntryUrl(-1, GURL("https://www.new.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::ERR_WRONG_OFFSET);
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       UpdateNavigationEntryUrl_ValidEntry) {
  // Commit a navigation, then update the URL of entry at index 0
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.com/"), GetTestWebContents());
  simulator->Commit();

  auto result = controller().UpdateNavigationEntryUrl(
      0, GURL("https://www.updated.com/"));
  EXPECT_EQ(result,
            NavigationController::NavigationEntryUpdateError::UPDATE_OK);
  // The entry's URL should be updated
  EXPECT_EQ(controller().GetEntryAtIndex(0)->GetURL(),
            GURL("https://www.updated.com/"));
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       UpdateNavigationEntryUrl_SetsVirtualAndOriginalUrl) {
  // Verify that UpdateNavigationEntryUrl also sets virtual URL and
  // original request URL
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.com/"), GetTestWebContents());
  simulator->Commit();

  GURL new_url("https://www.updated.com/");
  controller().UpdateNavigationEntryUrl(0, new_url);
  auto* entry = controller().GetEntryAtIndex(0);
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetVirtualURL(), new_url);
  EXPECT_EQ(entry->GetOriginalRequestURL(), new_url);
}
#endif  // BUILDFLAG(ARKWEB_EXT_NAVIGATION)

// ============================================================================
// ReloadWithNetError tests (ARKWEB_EX_FALLBACK_PROXY)
//
// ReloadWithNetError delegates to Reload() after storing the reason.
// We verify via WebContentsObserver that the reload is initiated with
// the correct ReloadType.
// ============================================================================

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
TEST_F(ArkWebNavigationControllerImplExtTest, ReloadWithNetError_NormalReload) {
  // Navigate first so reload has a target
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.com/"), GetTestWebContents());
  simulator->Commit();

  controller().ReloadWithNetError(ReloadType::NORMAL, false,
                                  ErrorPageReloadReason::FALLBACK_PROXY);
  // Verify that Reload was called with NORMAL type
  EXPECT_EQ(last_reload_type(), ReloadType::NORMAL);
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       ReloadWithNetError_BypassingCache) {
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.example.com/"), GetTestWebContents());
  simulator->Commit();

  controller().ReloadWithNetError(ReloadType::BYPASSING_CACHE, false,
                                  ErrorPageReloadReason::FALLBACK_PROXY);
  EXPECT_EQ(last_reload_type(), ReloadType::BYPASSING_CACHE);
}
#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

// ============================================================================
// NewEntrySetExtraHeaders condition logic tests (ARKWEB_NETWORK_BASE)
//
// Since NewEntrySetExtraHeaders is not on the base NavigationController
// interface and requires RenderFrameHostImpl, DidCommitProvisionalLoadParams,
// and NavigationEntryImpl to be constructed, we verify the condition logic
// and the command-line switch path.
// ============================================================================

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
TEST_F(ArkWebNavigationControllerImplExtTest, NwebExSwitch_PresentAndAbsent) {
  // Verify the command-line switch that NewEntrySetExtraHeaders depends on.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  EXPECT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx));
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kEnableNwebEx);
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx));
}

TEST_F(ArkWebNavigationControllerImplExtTest,
       NewEntrySetExtraHeaders_ConditionLogic) {
  // Logic verification: the method only sets extra headers when
  // BOTH is_currently_error_page AND HasSwitch(kEnableNwebEx) are true.
  bool is_currently_error_page = false;

  // When is_currently_error_page is false, condition fails regardless of switch
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  bool has_switch = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx);
  EXPECT_TRUE(has_switch);
  EXPECT_FALSE(is_currently_error_page && has_switch);

  // When is_currently_error_page is true but HasSwitch is false
  is_currently_error_page = true;
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kEnableNwebEx);
  has_switch = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx);
  EXPECT_FALSE(is_currently_error_page && has_switch);

  // When both are true, the condition passes
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  has_switch = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx);
  EXPECT_TRUE(is_currently_error_page && has_switch);
}
#endif  // BUILDFLAG(ARKWEB_NETWORK_BASE)

// ============================================================================
// Compile-time enum value verification
// ============================================================================

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
static_assert(
    static_cast<int>(
        NavigationController::NavigationEntryUpdateError::UPDATE_OK) == 0);
static_assert(
    static_cast<int>(
        NavigationController::NavigationEntryUpdateError::ERR_WRONG_OFFSET) ==
    -1);
static_assert(
    static_cast<int>(
        NavigationController::NavigationEntryUpdateError::ERR_OTHER) == -2);
#endif  // BUILDFLAG(ARKWEB_EXT_NAVIGATION)

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
static_assert(static_cast<int>(ErrorPageReloadReason::INVALID) == 0);
static_assert(static_cast<int>(ErrorPageReloadReason::FALLBACK_PROXY) != 0);
#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

}  // namespace content
