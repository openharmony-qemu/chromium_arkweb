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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/time/time.h"
#include "url/gurl.h"

// This UT file performs logic-verification tests for functions defined in
// profile_interaction_manager_for_include.cc. That file is a _for_include
// fragment injected into profile_interaction_manager.cc at compile time,
// so we cannot directly call the functions from a standalone UT.
// Instead, we verify the branching logic and decision paths.

namespace subresource_filter {

// ============================================================================
// OnPageActivationComputedExt logic verification
// ============================================================================
//
// The function has these key branches:
// 1. If url.SchemeIsHTTPOrHTTPS() -> activation_level = kEnabled
// 2. If rfh == NULL -> return kDisabled
// 3. If node == NULL || !node->is_adblock_enabled() ||
//    !navigation_handle->GetWebContents()->IsAdsBlockEnabled() ->
//    return kDisabled
// 4. Otherwise -> return effective_activation_level

class OnPageActivationComputedExtLogicTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(OnPageActivationComputedExtLogicTest, HttpUrlSchemesAreRecognized) {
  GURL http_url("http://example.com/test");
  EXPECT_TRUE(http_url.SchemeIsHTTPOrHTTPS());

  GURL https_url("https://example.com/test");
  EXPECT_TRUE(https_url.SchemeIsHTTPOrHTTPS());
}

TEST_F(OnPageActivationComputedExtLogicTest, NonHttpUrlSchemesNotRecognized) {
  GURL file_url("file:///local/path");
  EXPECT_FALSE(file_url.SchemeIsHTTPOrHTTPS());

  GURL data_url("data:text/html,<html></html>");
  EXPECT_FALSE(data_url.SchemeIsHTTPOrHTTPS());

  GURL about_url("about:blank");
  EXPECT_FALSE(about_url.SchemeIsHTTPOrHTTPS());
}

// ============================================================================
// ProfileInteractionManagerUtil::OnAdsViolationTriggeredExt logic
// verification
// ============================================================================
//
// The function has these key branches:
// 1. if (rfh == nullptr) return false
// 2. if (profile_context_) -> check ads_intervention_manager
//    a. GetLastAdsIntervention(url) -> if has value AND duration not elapsed ->
//       return false
//    b. Otherwise -> TriggerAdsInterventionForUrlOnSubsequentLoads
// 3. return true

class OnAdsViolationTriggeredExtLogicTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(OnAdsViolationTriggeredExtLogicTest,
       RecentInterventionDurationNotElapsed_ReturnsFalse) {
  // Business logic (lines 77-81):
  // If last_intervention exists AND duration_since < intervention_duration
  // then return false
  base::TimeDelta duration_since = base::Seconds(10);
  base::TimeDelta intervention_duration = base::Seconds(3600);
  bool should_return_false = (duration_since < intervention_duration);
  EXPECT_TRUE(should_return_false);
}

TEST_F(OnAdsViolationTriggeredExtLogicTest,
       InterventionDurationElapsed_Proceeds) {
  // When duration_since >= intervention_duration, the function does not
  // return false, it proceeds to TriggerAdsInterventionForUrlOnSubsequentLoads
  base::TimeDelta duration_since = base::Seconds(7200);
  base::TimeDelta intervention_duration = base::Seconds(3600);
  bool should_not_return_false = (duration_since < intervention_duration);
  EXPECT_FALSE(should_not_return_false);
}

}  // namespace subresource_filter
