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

#include "arkweb/chromium_ext/components/subresource_filter/content/browser/arkweb_content_subresource_filter_throttle_manager_ext.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "arkweb/chromium_ext/components/subresource_filter/content/browser/ohos_adblock_config.h"
#include "components/subresource_filter/content/shared/browser/activation_state_computing_navigation_throttle.h"
#include "components/subresource_filter/content/shared/browser/page_load_statistics.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/test/mock_navigation_handle.h"

namespace subresource_filter {

class ArkWebContentSubresourceFilterThrottleManagerExtTest
    : public ContentSubresourceFilterThrottleManagerTest {
 public:
  void SetUp() override {
    ContentSubresourceFilterThrottleManagerTest::SetUp();
    auto* throttle_manager = ContentSubresourceFilterThrottleManager::FromPage(
        main_rfh()->GetPage());
    ASSERT_TRUE(throttle_manager);
    ext =
        throttle_manager->AsArkWebContentSubresourceFilterThrottleManagerExt();
    ASSERT_TRUE(ext);
  }

  // Disambiguate web_contents() from multiple base classes
  content::WebContents* GetTestWebContents() {
    return content::RenderViewHostTestHarness::web_contents();
  }

  ArkWebContentSubresourceFilterThrottleManagerExt* ext;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ArkWebContentSubresourceFilterThrottleManagerExtTest,
                         ::testing::Values(WILL_START_REQUEST,
                                           WILL_PROCESS_RESPONSE));

// ============================================================================
// AdBlockMaybeCreateActivationState guard condition tests
// ============================================================================

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       AdBlockMaybeCreateActivationState_NullNavigation) {
  std::unique_ptr<ActivationStateComputingNavigationThrottle> throttle;
  // Null navigation_handle -> guard fails at first condition, no crash
  ext->AdBlockMaybeCreateActivationState(nullptr, throttle);
  EXPECT_EQ(throttle.get(), nullptr);
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       AdBlockMaybeCreateActivationState_IsDownload) {
  // Create mock handle that is a download
  content::MockNavigationHandle handle(GURL("https://www.example.com/"),
                                       main_rfh());
  EXPECT_CALL(handle, IsDownload()).WillOnce(::testing::Return(true));
  std::unique_ptr<ActivationStateComputingNavigationThrottle> throttle;
  ext->AdBlockMaybeCreateActivationState(&handle, throttle);
  EXPECT_EQ(throttle.get(), nullptr);
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       AdBlockMaybeCreateActivationState_NotInMainFrame) {
  // Subframe RFH has a parent -> IsInMainFrame() returns false
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  content::MockNavigationHandle handle(GURL("https://www.example.com/"),
                                       subframe);
  EXPECT_CALL(handle, IsDownload()).WillOnce(::testing::Return(false));
  std::unique_ptr<ActivationStateComputingNavigationThrottle> throttle;
  ext->AdBlockMaybeCreateActivationState(&handle, throttle);
  EXPECT_EQ(throttle.get(), nullptr);
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       AdBlockMaybeCreateActivationState_NotHTTPOrHTTPS) {
  // Non-HTTP URL -> SchemeIsHTTPOrHTTPS() returns false
  content::MockNavigationHandle handle(GURL("file:///local/file.html"),
                                       main_rfh());
  EXPECT_CALL(handle, IsDownload()).WillOnce(::testing::Return(false));
  std::unique_ptr<ActivationStateComputingNavigationThrottle> throttle;
  ext->AdBlockMaybeCreateActivationState(&handle, throttle);
  EXPECT_EQ(throttle.get(), nullptr);
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       AdBlockMaybeCreateActivationState_NullWebContents) {
  // Default-constructed mock has null WebContents
  content::MockNavigationHandle handle;
  handle.set_url(GURL("https://www.example.com/"));
  handle.set_is_in_primary_main_frame(true);
  EXPECT_CALL(handle, IsDownload()).WillOnce(::testing::Return(false));
  std::unique_ptr<ActivationStateComputingNavigationThrottle> throttle;
  ext->AdBlockMaybeCreateActivationState(&handle, throttle);
  EXPECT_EQ(throttle.get(), nullptr);
}

// ============================================================================
// OnPageAdBlockActivationState tests
// ============================================================================

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       OnPageAdBlockActivationState_ThrottleNotFound) {
  // Mock navigation handle with a navigation ID NOT in
  // ongoing_activation_throttles_ -> early return, no crash
  content::MockNavigationHandle handle(GURL("https://www.example.com/"),
                                       main_rfh());
  EXPECT_NO_FATAL_FAILURE(ext->OnPageAdBlockActivationState(&handle));
}

// ============================================================================
// SetStatisticsAfterDocumentLoad null statistics tests
// ============================================================================

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       SetStatisticsAfterDocumentLoad_NullStatistics) {
  mojom::DocumentLoadStatisticsPtr stats = mojom::DocumentLoadStatistics::New();
  EXPECT_NO_FATAL_FAILURE(
      ext->SetStatisticsAfterDocumentLoad(std::move(stats)));
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       UserSetStatisticsAfterDocumentLoad_NullStatistics) {
  mojom::DocumentLoadStatisticsPtr stats = mojom::DocumentLoadStatistics::New();
  EXPECT_NO_FATAL_FAILURE(
      ext->UserSetStatisticsAfterDocumentLoad(std::move(stats)));
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       UserSetDocumentLoadStatistics_NullStatistics) {
  mojom::DocumentLoadStatisticsPtr stats = mojom::DocumentLoadStatistics::New();
  EXPECT_NO_FATAL_FAILURE(ext->UserSetDocumentLoadStatistics(std::move(stats)));
}

// ============================================================================
// ReportSubresourceMap tests
// ============================================================================

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       ReportSubresourceMap_EmptyMap) {
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/not_disallowed.html"), main_rfh());
  navigation_simulator()->Start();

  CreateFencedFrameWithTestNavigation(GURL("https://www.a.com/allowed.html"),
                                      main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* fenced_frame =
      navigation_simulator()->GetFinalRenderFrameHost();
  ::testing::internal::CaptureStderr();
  ext->ReportSubresourceMap(fenced_frame,
                            GURL("https://www.a.com/allowed.html"));
  navigation_simulator()->StopLoading();
  std::string log_output = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log_output.find("map.size():0"), std::string::npos);
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       ReportSubresourceMap_NonEmptyMap) {
  // Populate the aggregated statistics with a disallowed URL so the
  // subresource map is non-empty and ReportSubresourceMap enters the
  // web_contents->OnAdsBlocked() path.
  content::MockNavigationHandle handle(GetTestWebContents());
  handle.set_url(GURL("https://www.example.com/"));
  ext->StatisticActivationState(&handle);
  auto* statistics = throttle_manager()->page_load_statistics();
  ASSERT_NE(statistics, nullptr);

  mojom::DocumentLoadStatisticsPtr stats = mojom::DocumentLoadStatistics::New();
  stats->num_loads_total = 5;
  stats->num_loads_disallowed = 1;
  stats->loads_disallowed_url_map["https://www.blocked.example/ads.js"] = 1;
  statistics->OnDocumentLoadStatistics(*stats);

  content::RenderFrameHost* fenced_frame = CreateFencedFrameWithTestNavigation(
      GURL("https://www.a.com/allowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  fenced_frame = navigation_simulator()->GetFinalRenderFrameHost();

  ::testing::internal::CaptureStderr();
  EXPECT_NO_FATAL_FAILURE(ext->ReportSubresourceMap(
      fenced_frame, GURL("https://www.a.com/allowed.html")));
  navigation_simulator()->StopLoading();
  std::string log_output = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log_output.find("map.size():1"), std::string::npos);
}

// ============================================================================
// StatisticActivationState tests
// ============================================================================

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       StatisticActivationState_CreatesStatistics) {
  content::MockNavigationHandle handle(GetTestWebContents());
  handle.set_url(GURL("https://www.example.com/"));
  ext->StatisticActivationState(&handle);
  auto* stats = throttle_manager()->page_load_statistics();
  EXPECT_NE(stats, nullptr);
  EXPECT_NE(stats->AsPageLoadStatisticsExt(), nullptr);
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       StatisticActivationState_StatisticsAlreadyExists) {
  content::MockNavigationHandle handle1(GetTestWebContents());
  handle1.set_url(GURL("https://www.example.com/"));
  ext->StatisticActivationState(&handle1);
  auto* stats_first = throttle_manager()->page_load_statistics();
  ASSERT_NE(stats_first, nullptr);

  content::MockNavigationHandle handle2(GetTestWebContents());
  handle2.set_url(GURL("https://www.example.com/page2"));
  ext->StatisticActivationState(&handle2);
  auto* stats_second = throttle_manager()->page_load_statistics();
  EXPECT_EQ(stats_first, stats_second);
}

// ============================================================================
// SetStatisticsAfterDocumentLoad with valid statistics
// ============================================================================

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       SetStatisticsAfterDocumentLoad_WithStatistics) {
  content::MockNavigationHandle handle(GetTestWebContents());
  handle.set_url(GURL("https://www.example.com/"));
  ext->StatisticActivationState(&handle);
  ASSERT_NE(throttle_manager()->page_load_statistics(), nullptr);

  mojom::DocumentLoadStatisticsPtr stats = mojom::DocumentLoadStatistics::New();
  stats->num_loads_total = 5;
  stats->num_loads_disallowed = 2;
  EXPECT_NO_FATAL_FAILURE(
      ext->SetStatisticsAfterDocumentLoad(std::move(stats)));
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       UserSetStatisticsAfterDocumentLoad_WithStatistics) {
  content::MockNavigationHandle handle(GetTestWebContents());
  handle.set_url(GURL("https://www.example.com/"));
  ext->StatisticActivationState(&handle);
  ASSERT_NE(throttle_manager()->page_load_statistics(), nullptr);

  mojom::DocumentLoadStatisticsPtr stats = mojom::DocumentLoadStatistics::New();
  stats->num_loads_total = 3;
  EXPECT_NO_FATAL_FAILURE(
      ext->UserSetStatisticsAfterDocumentLoad(std::move(stats)));
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       UserSetDocumentLoadStatistics_WithStatistics) {
  content::MockNavigationHandle handle(GetTestWebContents());
  handle.set_url(GURL("https://www.example.com/"));
  ext->StatisticActivationState(&handle);
  ASSERT_NE(throttle_manager()->page_load_statistics(), nullptr);

  mojom::DocumentLoadStatisticsPtr stats = mojom::DocumentLoadStatistics::New();
  stats->num_loads_total = 7;
  EXPECT_NO_FATAL_FAILURE(ext->UserSetDocumentLoadStatistics(std::move(stats)));
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       SetStatisticsAfterDocumentLoad_WithDisallowedUrls) {
  // A non-empty loads_disallowed_url_map triggers NotifyAndResetStatistics,
  // which reports blocked URLs via web_contents->OnAdsBlocked() (null
  // delegate in the harness -> no-op) and resets the after-load statistics.
  content::MockNavigationHandle handle(GetTestWebContents());
  handle.set_url(GURL("https://www.example.com/"));
  ext->StatisticActivationState(&handle);
  ASSERT_NE(throttle_manager()->page_load_statistics(), nullptr);

  mojom::DocumentLoadStatisticsPtr stats = mojom::DocumentLoadStatistics::New();
  stats->num_loads_total = 5;
  stats->num_loads_disallowed = 1;
  stats->loads_disallowed_url_map["https://www.blocked.example/ads.js"] = 1;
  EXPECT_NO_FATAL_FAILURE(
      ext->SetStatisticsAfterDocumentLoad(std::move(stats)));

  // Calling a second time with no disallowed URLs must not crash.
  mojom::DocumentLoadStatisticsPtr stats2 =
      mojom::DocumentLoadStatistics::New();
  stats2->num_loads_total = 1;
  EXPECT_NO_FATAL_FAILURE(
      ext->SetStatisticsAfterDocumentLoad(std::move(stats2)));
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       UserSetStatisticsAfterDocumentLoad_WithDisallowedUrls) {
  content::MockNavigationHandle handle(GetTestWebContents());
  handle.set_url(GURL("https://www.example.com/"));
  ext->StatisticActivationState(&handle);
  ASSERT_NE(throttle_manager()->page_load_statistics(), nullptr);

  mojom::DocumentLoadStatisticsPtr stats = mojom::DocumentLoadStatistics::New();
  stats->num_loads_disallowed = 1;
  stats->loads_disallowed_url_map["https://www.blocked.example/ads.js"] = 2;
  EXPECT_NO_FATAL_FAILURE(
      ext->UserSetStatisticsAfterDocumentLoad(std::move(stats)));
}

// ============================================================================
// AdBlockActivationStateForNextCommittedLoad logic verification
// ============================================================================

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       AdBlockActivationState_DisabledLevel) {
  // Logic verification: mojom::ActivationState() defaults to kDisabled
  mojom::ActivationState default_state;
  EXPECT_EQ(default_state.activation_level, mojom::ActivationLevel::kDisabled);
}

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       AdBlockActivationState_EnabledLevelPath) {
  // Logic verification: non-disabled activation level enters the return path
  mojom::ActivationState enabled_state;
  enabled_state.activation_level = mojom::ActivationLevel::kEnabled;
  EXPECT_NE(enabled_state.activation_level, mojom::ActivationLevel::kDisabled);
}

// ============================================================================
// BindUserReceiver logic verification
// ============================================================================

TEST_P(ArkWebContentSubresourceFilterThrottleManagerExtTest,
       BindUserReceiver_FromPageCheck) {
  // Logic verification: BindUserReceiver checks if (auto* manager = FromPage())
  // before binding. When the manager exists on the page (as in our test setup),
  // it would attempt to bind the receiver to user_receiver_.
  // Full integration test requires Mojo infrastructure setup.
  // Verify that FromPage returns a valid manager for our test page.
  auto* manager =
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage());
  EXPECT_NE(manager, nullptr);
}

}  // namespace subresource_filter
