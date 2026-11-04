// Copyright 2026 Huawei Device Co., Ltd.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/features.h"
// clang-format off
#define private public
#include "base/ohos/ltpo/include/invisible_rendering_throttler.h"
#undef private
// clang-format on
#include "build/build_config.h"
#if BUILDFLAG(ARKWEB_SLIDE_LTPO)

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace ohos {

class InvisibleRenderingThrottlerTest : public testing::Test {
 protected:
  InvisibleRenderingThrottler throttler_;
};

// ---- InitConfigWithParams ----

TEST_F(InvisibleRenderingThrottlerTest, InitConfigWithParams_SetsConfig) {
  throttler_.InitConfigWithParams(3000, 5);
  EXPECT_EQ(throttler_.config_.invisible_throttle_timeout.InMilliseconds(),
            3000);
  EXPECT_EQ(throttler_.config_.invisible_throttle_ratio, 5);
}

// ---- SetWebComponentVisible ----

TEST_F(InvisibleRenderingThrottlerTest,
       SetWebComponentVisible_True_ResetsStateAndTimestamp) {
  // First make component invisible to set invisible_since
  throttler_.SetWebComponentVisible(1, false);
  EXPECT_FALSE(throttler_.throttle_info_map_[1].visible);
  EXPECT_FALSE(throttler_.throttle_info_map_[1].invisible_since.is_null());

  // Now set visible=true: should reset invisible_since and set state to NORMAL
  throttler_.SetWebComponentVisible(1, true);
  EXPECT_TRUE(throttler_.throttle_info_map_[1].visible);
  EXPECT_TRUE(throttler_.throttle_info_map_[1].invisible_since.is_null());
  EXPECT_EQ(throttler_.throttle_info_map_[1].state,
            RenderingThrottleState::NORMAL);
}

TEST_F(InvisibleRenderingThrottlerTest,
       SetWebComponentVisible_False_SetsInvisibleSince) {
  // First time invisible: invisible_since should be set
  throttler_.SetWebComponentVisible(1, false);
  EXPECT_FALSE(throttler_.throttle_info_map_[1].visible);
  EXPECT_FALSE(throttler_.throttle_info_map_[1].invisible_since.is_null());
}

TEST_F(InvisibleRenderingThrottlerTest,
       SetWebComponentVisible_FalseAgain_KeepsInvisibleSince) {
  // Set invisible first time
  throttler_.SetWebComponentVisible(1, false);
  auto first_invisible_since = throttler_.throttle_info_map_[1].invisible_since;

  // Set invisible again: invisible_since should NOT change
  throttler_.SetWebComponentVisible(1, false);
  EXPECT_EQ(throttler_.throttle_info_map_[1].invisible_since,
            first_invisible_since);
}

// ---- ShouldThrottleInvisible ----

TEST_F(InvisibleRenderingThrottlerTest,
       ShouldThrottleInvisible_NotInMap_ReturnsFalse) {
  EXPECT_FALSE(
      throttler_.ShouldThrottleInvisible(999, base::Milliseconds(5000)));
}

TEST_F(InvisibleRenderingThrottlerTest,
       ShouldThrottleInvisible_Visible_ReturnsFalse) {
  throttler_.SetWebComponentVisible(1, true);
  EXPECT_FALSE(throttler_.ShouldThrottleInvisible(1, base::Milliseconds(5000)));
}

TEST_F(InvisibleRenderingThrottlerTest,
       ShouldThrottleInvisible_DurationExceedsTimeout_ReturnsTrue) {
  // Set invisible with a very short timeout (0ms) so duration always exceeds
  throttler_.SetWebComponentVisible(1, false);
  EXPECT_TRUE(throttler_.ShouldThrottleInvisible(1, base::Milliseconds(0)));
}

TEST_F(InvisibleRenderingThrottlerTest,
       ShouldThrottleInvisible_DurationLessThanTimeout_ReturnsFalse) {
  // Set invisible just now with a very long timeout
  throttler_.SetWebComponentVisible(1, false);
  EXPECT_FALSE(
      throttler_.ShouldThrottleInvisible(1, base::Milliseconds(3600000)));
}

// ---- ThrottleRatio ----

TEST_F(InvisibleRenderingThrottlerTest, ThrottleRatio_EmptyMap_ReturnsOne) {
  EXPECT_EQ(throttler_.ThrottleRatio(), 1);
}

TEST_F(InvisibleRenderingThrottlerTest,
       ThrottleRatio_NotAllThrottled_ReturnsOne) {
  throttler_.SetWebComponentVisible(1, true);
  // Component is visible → state is NORMAL → not all INVISIBLE_THROTTLED
  EXPECT_EQ(throttler_.ThrottleRatio(), 1);
}

TEST_F(InvisibleRenderingThrottlerTest,
       ThrottleRatio_AllThrottled_ReturnsConfigRatio) {
  throttler_.InitConfigWithParams(0, 7);
  // Set invisible → duration exceeds 0ms timeout → INVISIBLE_THROTTLED
  throttler_.SetWebComponentVisible(1, false);
  // Force evaluation: after SetWebComponentVisible, EvaluateThrottleState
  // is already called. With timeout=0ms, ShouldThrottleInvisible returns true,
  // so state should be INVISIBLE_THROTTLED.
  EXPECT_EQ(throttler_.ThrottleRatio(), 7);
}

// ---- EvaluateThrottleState ----

TEST_F(InvisibleRenderingThrottlerTest, EvaluateThrottleState_NotInMap_NoOp) {
  // Should not crash or add entry
  throttler_.EvaluateThrottleState(999);
  EXPECT_EQ(throttler_.throttle_info_map_.size(), 0u);
}

TEST_F(InvisibleRenderingThrottlerTest,
       EvaluateThrottleState_ShouldThrottle_UpdatesState) {
  throttler_.InitConfigWithParams(0, 5);
  throttler_.SetWebComponentVisible(1, false);
  // With timeout=0ms, ShouldThrottleInvisible returns true
  EXPECT_EQ(throttler_.throttle_info_map_[1].state,
            RenderingThrottleState::INVISIBLE_THROTTLED);
}

TEST_F(InvisibleRenderingThrottlerTest,
       EvaluateThrottleState_ShouldNotThrottle_KeepsNormal) {
  throttler_.InitConfigWithParams(3600000, 5);
  throttler_.SetWebComponentVisible(1, false);
  // With very long timeout, ShouldThrottleInvisible returns false
  EXPECT_EQ(throttler_.throttle_info_map_[1].state,
            RenderingThrottleState::NORMAL);
}

TEST_F(InvisibleRenderingThrottlerTest,
       EvaluateThrottleState_StateUnchanged_NoUpdate) {
  // Set visible → state is NORMAL
  throttler_.SetWebComponentVisible(1, true);
  EXPECT_EQ(throttler_.throttle_info_map_[1].state,
            RenderingThrottleState::NORMAL);
  // Evaluate again with long timeout → still NORMAL, state unchanged path
  throttler_.EvaluateThrottleState(1);
  EXPECT_EQ(throttler_.throttle_info_map_[1].state,
            RenderingThrottleState::NORMAL);
}

// ---- EvaluateAllThrottleStates ----

TEST_F(InvisibleRenderingThrottlerTest,
       EvaluateAllThrottleStates_UpdatesAllComponents) {
  throttler_.InitConfigWithParams(0, 5);
  // Add two invisible components
  throttler_.SetWebComponentVisible(1, false);
  throttler_.SetWebComponentVisible(2, false);
  // Both should already be INVISIBLE_THROTTLED from SetWebComponentVisible
  // Now make one visible and re-evaluate all
  throttler_.SetWebComponentVisible(1, true);
  throttler_.EvaluateAllThrottleStates();
  EXPECT_EQ(throttler_.throttle_info_map_[1].state,
            RenderingThrottleState::NORMAL);
  EXPECT_EQ(throttler_.throttle_info_map_[2].state,
            RenderingThrottleState::INVISIBLE_THROTTLED);
}

// ---- GetConfig ----

TEST_F(InvisibleRenderingThrottlerTest, GetConfig_DefaultValues) {
  const auto& config = throttler_.GetConfig();
  EXPECT_EQ(config.invisible_throttle_timeout.InMilliseconds(), 5000);
  EXPECT_EQ(config.invisible_throttle_ratio, 10);
}

TEST_F(InvisibleRenderingThrottlerTest, GetConfig_AfterInit) {
  throttler_.InitConfigWithParams(2000, 3);
  const auto& config = throttler_.GetConfig();
  EXPECT_EQ(config.invisible_throttle_timeout.InMilliseconds(), 2000);
  EXPECT_EQ(config.invisible_throttle_ratio, 3);
}

// ---- Multiple components mixed states ----

TEST_F(InvisibleRenderingThrottlerTest, ThrottleRatio_MixedStates_ReturnsOne) {
  throttler_.InitConfigWithParams(0, 7);
  // One visible (NORMAL), one invisible (INVISIBLE_THROTTLED)
  throttler_.SetWebComponentVisible(1, true);
  throttler_.SetWebComponentVisible(2, false);
  // Not all INVISIBLE_THROTTLED → return 1
  EXPECT_EQ(throttler_.ThrottleRatio(), 1);
}

TEST_F(InvisibleRenderingThrottlerTest,
       ThrottleRatio_AllInvisibleAllThrottled_ReturnsRatio) {
  throttler_.InitConfigWithParams(0, 7);
  throttler_.SetWebComponentVisible(1, false);
  throttler_.SetWebComponentVisible(2, false);
  // All INVISIBLE_THROTTLED → return config ratio
  EXPECT_EQ(throttler_.ThrottleRatio(), 7);
}

// ---- State transition: visible→invisible→throttled ----

TEST_F(InvisibleRenderingThrottlerTest,
       StateTransition_VisibleToInvisibleToThrottled) {
  throttler_.InitConfigWithParams(0, 5);
  // Start visible
  throttler_.SetWebComponentVisible(1, true);
  EXPECT_EQ(throttler_.throttle_info_map_[1].state,
            RenderingThrottleState::NORMAL);
  EXPECT_TRUE(throttler_.throttle_info_map_[1].visible);

  // Go invisible → throttled (timeout=0ms)
  throttler_.SetWebComponentVisible(1, false);
  EXPECT_EQ(throttler_.throttle_info_map_[1].state,
            RenderingThrottleState::INVISIBLE_THROTTLED);
  EXPECT_FALSE(throttler_.throttle_info_map_[1].visible);

  // Go visible again → back to NORMAL
  throttler_.SetWebComponentVisible(1, true);
  EXPECT_EQ(throttler_.throttle_info_map_[1].state,
            RenderingThrottleState::NORMAL);
  EXPECT_TRUE(throttler_.throttle_info_map_[1].visible);
}

}  // namespace ohos
}  // namespace base

#endif  // BUILDFLAG(ARKWEB_SLIDE_LTPO)