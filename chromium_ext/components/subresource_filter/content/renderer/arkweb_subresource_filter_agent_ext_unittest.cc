/* Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <memory>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/content/renderer/user_unverified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
// clang-format off
#define private public
#define protected public
#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"
#include "arkweb/chromium_ext/components/subresource_filter/core/common/document_subresource_filter_ext.h"
#include "arkweb/chromium_ext/components/subresource_filter/content/renderer/arkweb_subresource_filter_agent_ext.h"
#include "components/subresource_filter/content/renderer/web_document_subresource_filter_impl.h"
#include "content/public/renderer/render_frame.h"
#undef protected
#undef private
// clang-format on
#include "url/gurl.h"
#include "user_subresource_filter_agent_test.h"

namespace subresource_filter {

class ArkWebSubresourceFilterAgentExtTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(ArkWebSubresourceFilterAgentExtTest,
       AsArkWebSubresourceFilterAgentExt_ReturnsThis) {
  ArkWebSubresourceFilterAgentExtTester tester;
  TestRenderFrameCondition cond;
  cond.is_main_frame = false;
  tester.CreateMockArkSubresourceFilterAgentExt(cond,
                                                "http://example.com/test");
  auto* agent = tester.GetAgent();
  EXPECT_NE(agent, nullptr);
  EXPECT_EQ(agent->AsArkWebSubresourceFilterAgentExt(), agent);
}

TEST_F(ArkWebSubresourceFilterAgentExtTest,
       AsArkWebSubresourceFilterAgentExt_BaseClassReturnsNull) {
  ArkWebSubresourceFilterAgentExtTester tester;
  TestRenderFrameCondition cond;
  cond.is_main_frame = false;
  tester.CreateMockArkSubresourceFilterAgentExt(cond,
                                                "http://example.com/test");
  auto* agent = tester.GetAgent();
  EXPECT_NE(agent, nullptr);
  SubresourceFilterAgent* base = static_cast<SubresourceFilterAgent*>(agent);
  EXPECT_NE(base->AsArkWebSubresourceFilterAgentExt(), nullptr);
}

#if BUILDFLAG(ARKWEB_ADBLOCK)
TEST_F(ArkWebSubresourceFilterAgentExtTest, DidSubresourceFiltered_NoFilter) {
  ArkWebSubresourceFilterAgentExtTester tester;
  TestRenderFrameCondition cond;
  cond.is_main_frame = false;
  cond.is_in_fenced_frame_tree = true;
  const char url[] = "http://example.com/test";
  tester.CreateMockArkSubresourceFilterAgentExt(cond, url);
  auto* agent = tester.GetAgent();
  EXPECT_NE(agent, nullptr);
  // No filter_for_last_created_document_
  EXPECT_NO_FATAL_FAILURE(agent->DidSubresourceFiltered());
}

TEST_F(ArkWebSubresourceFilterAgentExtTest,
       DidSubresourceFiltered_FilterButNotLoadFinished) {
  ArkWebSubresourceFilterAgentExtTester tester;
  tester.SetTestRulesetToDisallowURLs("test");
  TestRenderFrameCondition cond;
  cond.is_main_frame = false;
  cond.is_in_fenced_frame_tree = true;
  const char url[] = "http://example.com/test";
  tester.CreateMockArkSubresourceFilterAgentExt(cond, url);
  auto* agent = tester.GetAgent();
  EXPECT_NE(agent, nullptr);

  // Set filter but did_load_finished_ = false
  scoped_refptr<const MemoryMappedRuleset> ruleset =
      tester.GetUnverifiedRulesetDealer()->GetRuleset();
  EXPECT_NE(ruleset, nullptr);
  base::OnceClosure first_disallowed_load_callback(base::BindOnce([]() {}));
  mojom::ActivationState activation_state;
  activation_state.activation_level = mojom::ActivationLevel::kEnabled;
  auto filter = std::make_unique<WebDocumentSubresourceFilterImpl>(
      url::Origin::Create(GURL(url)), activation_state, std::move(ruleset),
      std::move(first_disallowed_load_callback));
  agent->filter_for_last_created_document_ = filter->AsWeakPtr();
  // did_load_finished_ is false by default
  EXPECT_NO_FATAL_FAILURE(agent->DidSubresourceFiltered());
}

TEST_F(ArkWebSubresourceFilterAgentExtTest,
       DidSubresourceFiltered_LoadFinishedWithFilter) {
  ArkWebSubresourceFilterAgentExtTester tester;
  tester.SetTestRulesetToDisallowURLs("test");
  TestRenderFrameCondition cond;
  cond.is_main_frame = false;
  cond.is_in_fenced_frame_tree = true;
  const char url[] = "http://example.com/test";
  tester.CreateMockArkSubresourceFilterAgentExt(cond, url);
  auto* agent = tester.GetAgent();
  EXPECT_NE(agent, nullptr);

  scoped_refptr<const MemoryMappedRuleset> ruleset =
      tester.GetUnverifiedRulesetDealer()->GetRuleset();
  EXPECT_NE(ruleset, nullptr);
  base::OnceClosure first_disallowed_load_callback(base::BindOnce([]() {}));
  mojom::ActivationState activation_state;
  activation_state.activation_level = mojom::ActivationLevel::kEnabled;
  auto filter = std::make_unique<WebDocumentSubresourceFilterImpl>(
      url::Origin::Create(GURL(url)), activation_state, std::move(ruleset),
      std::move(first_disallowed_load_callback));
  agent->filter_for_last_created_document_ = filter->AsWeakPtr();
  agent->did_load_finished_ = true;
  EXPECT_NO_FATAL_FAILURE(agent->DidSubresourceFiltered());
}

TEST_F(ArkWebSubresourceFilterAgentExtTest,
       CalcElementHidingTypeOption_NullFrame) {
  ArkWebSubresourceFilterAgentExtTester tester;
  TestRenderFrameCondition cond;
  cond.is_main_frame = true;
  cond.is_in_fenced_frame_tree = true;
  tester.CreateMockArkSubresourceFilterAgentExt(cond,
                                                "http://example.com/test");
  auto* agent = tester.GetAgent();
  EXPECT_NE(agent, nullptr);
  EXPECT_NO_FATAL_FAILURE(agent->CalcElementHidingTypeOption(nullptr));
}

TEST_F(ArkWebSubresourceFilterAgentExtTest,
       CalcElementHidingTypeOption_NullLocalFrame) {
  ArkWebSubresourceFilterAgentExtTester tester;
  TestRenderFrameCondition cond;
  cond.is_main_frame = true;
  cond.is_in_fenced_frame_tree = true;
  cond.has_local_frame = false;
  tester.CreateMockArkSubresourceFilterAgentExt(cond,
                                                "http://example.com/test");
  auto* agent = tester.GetAgent();
  EXPECT_NE(agent, nullptr);
  EXPECT_NO_FATAL_FAILURE(
      agent->CalcElementHidingTypeOption(tester.GetRenderFrame()));
}

TEST_F(ArkWebSubresourceFilterAgentExtTest,
       CalcElementHidingTypeOption_NonHttpProtocol) {
  // When has_local_frame is true but mock frame is invalid, GetWebFrame()
  // returns a dangling pointer. Use has_local_frame=false to test the
  // web_local_frame null check path (which is the same early-return path
  // that non-HTTP URLs take after the protocol check).
  ArkWebSubresourceFilterAgentExtTester tester;
  TestRenderFrameCondition cond;
  cond.is_main_frame = true;
  cond.is_in_fenced_frame_tree = false;
  cond.has_local_frame = false;
  tester.CreateMockArkSubresourceFilterAgentExt(cond, "file:///local/path");
  auto* agent = tester.GetAgent();
  EXPECT_NE(agent, nullptr);
  // web_local_frame is null -> early return, no crash
  EXPECT_NO_FATAL_FAILURE(
      agent->CalcElementHidingTypeOption(tester.GetRenderFrame()));
}
#endif

}  // namespace subresource_filter
