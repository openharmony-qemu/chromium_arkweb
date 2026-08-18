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

#include <map>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#if BUILDFLAG(ARKWEB_ADBLOCK)
#include "content/public/browser/web_contents.h"
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)
#define private public
#include "arkweb/chromium_ext/components/subresource_filter/content/shared/browser/page_load_statistics_ext.h"
#undef private

namespace subresource_filter {
#if BUILDFLAG(ARKWEB_ADBLOCK)
class PageLoadStatisticsExtTest : public content::RenderViewHostTestHarness {
 public:
  PageLoadStatisticsExtTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }

  void CheckStatistics(const mojom::DocumentLoadStatistics& statistics,
                       const mojom::DocumentLoadStatistics& expect_statistics) {
    EXPECT_EQ(expect_statistics.num_loads_total, statistics.num_loads_total);
    EXPECT_EQ(expect_statistics.num_loads_evaluated,
              statistics.num_loads_evaluated);
    EXPECT_EQ(expect_statistics.num_loads_matching_rules,
              statistics.num_loads_matching_rules);
    EXPECT_EQ(expect_statistics.num_loads_disallowed,
              statistics.num_loads_disallowed);
    EXPECT_EQ(expect_statistics.loads_disallowed_url_map.size(),
              statistics.loads_disallowed_url_map.size());
  }

  mojom::ActivationState activation_state_;
};

TEST_F(PageLoadStatisticsExtTest, SetReportedSetsFlag) {
  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());
  EXPECT_TRUE(stats.IsFirstReport());
  stats.SetReported();
  EXPECT_FALSE(stats.IsFirstReport());
}

TEST_F(PageLoadStatisticsExtTest, ClearStatisticsResetsFields) {
  mojom::DocumentLoadStatistics statistics;
  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());
  statistics.num_loads_total = 10;
  statistics.num_loads_evaluated = 20;
  statistics.num_loads_matching_rules = 30;
  statistics.num_loads_disallowed = 40;
  statistics.loads_disallowed_url_map = {{"url", 1}};
  stats.ClearStatistics(statistics);
  CheckStatistics(statistics, mojom::DocumentLoadStatistics());
  EXPECT_TRUE(statistics.loads_disallowed_url_map.empty());
}

TEST_F(PageLoadStatisticsExtTest, OnStatisticsAfterDocumentLoad_NoWebContents) {
  mojom::DocumentLoadStatistics statistics;
  PageLoadStatisticsExt stats(activation_state_, nullptr);
  statistics.num_loads_total = 10;
  statistics.num_loads_evaluated = 20;
  statistics.num_loads_matching_rules = 30;
  statistics.num_loads_disallowed = 40;
  statistics.loads_disallowed_url_map = {{"url", 1}};
  stats.OnStatisticsAfterDocumentLoad(statistics);
  // no webcontents, do not clear
  auto& agg = stats.aggregated_document_after_load_statistics_;
  CheckStatistics(agg, statistics);

  // no url map, do notify
  statistics.num_loads_disallowed = 50;
  statistics.loads_disallowed_url_map.clear();
  stats.ClearStatistics(agg);
  stats.OnStatisticsAfterDocumentLoad(statistics);
  CheckStatistics(agg, statistics);
  EXPECT_TRUE(agg.loads_disallowed_url_map.empty());
}

TEST_F(PageLoadStatisticsExtTest, OnUserDocumentLoadStatistics) {
  mojom::DocumentLoadStatistics statistics;
  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());
  statistics.num_loads_total = 10;
  statistics.num_loads_evaluated = 20;
  statistics.num_loads_matching_rules = 30;
  statistics.num_loads_disallowed = 40;
  statistics.loads_disallowed_url_map = {{"url", 1}};
  stats.OnUserDocumentLoadStatistics(statistics);
  auto agg = stats.getUserAggregatedDocumentStatistics();
  CheckStatistics(agg, statistics);
  EXPECT_FALSE(agg.loads_disallowed_url_map.empty());
}

TEST_F(PageLoadStatisticsExtTest,
       OnUserStatisticsAfterDocumentLoad_NoWebContents) {
  mojom::DocumentLoadStatistics statistics;
  PageLoadStatisticsExt stats(activation_state_, nullptr);
  statistics.num_loads_total = 10;
  statistics.num_loads_evaluated = 20;
  statistics.num_loads_matching_rules = 30;
  statistics.num_loads_disallowed = 40;
  statistics.loads_disallowed_url_map = {{"url", 1}};
  stats.OnUserStatisticsAfterDocumentLoad(statistics);
  // no webcontents
  auto& agg = stats.user_aggregated_document_after_load_statistics_;
  CheckStatistics(agg, statistics);
  EXPECT_FALSE(agg.loads_disallowed_url_map.empty());

  // no url map, do notify
  statistics.num_loads_disallowed = 50;
  statistics.loads_disallowed_url_map.clear();
  stats.ClearStatistics(agg);
  stats.OnUserStatisticsAfterDocumentLoad(statistics);
  // no webcontents
  CheckStatistics(agg, statistics);
}

TEST_F(PageLoadStatisticsExtTest,
       OnUserStatisticsAfterDocumentLoad_HasWebContents) {
  mojom::DocumentLoadStatistics statistics;
  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());
  statistics.num_loads_total = 10;
  statistics.num_loads_evaluated = 20;
  statistics.num_loads_matching_rules = 30;
  statistics.num_loads_disallowed = 40;
  statistics.loads_disallowed_url_map = {{"url", 1}};
  EXPECT_TRUE(stats.IsFirstReport());
  stats.OnUserStatisticsAfterDocumentLoad(statistics);
  EXPECT_FALSE(stats.IsFirstReport());
  auto& agg = stats.user_aggregated_document_after_load_statistics_;
  CheckStatistics(agg, mojom::DocumentLoadStatistics());
}

TEST_F(PageLoadStatisticsExtTest,
       OnStatisticsAfterDocumentLoad_HasWebContents) {
  mojom::DocumentLoadStatistics statistics;
  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());
  statistics.num_loads_total = 10;
  statistics.num_loads_evaluated = 20;
  statistics.num_loads_matching_rules = 30;
  statistics.num_loads_disallowed = 40;
  statistics.loads_disallowed_url_map = {{"url", 1}};
  EXPECT_TRUE(stats.IsFirstReport());
  stats.OnStatisticsAfterDocumentLoad(statistics);
  EXPECT_FALSE(stats.IsFirstReport());
  // set reported after notify
  stats.OnStatisticsAfterDocumentLoad(statistics);
  EXPECT_FALSE(stats.IsFirstReport());
  auto& agg = stats.aggregated_document_after_load_statistics_;
  CheckStatistics(agg, mojom::DocumentLoadStatistics());
}

TEST_F(PageLoadStatisticsExtTest,
       OnStatisticsAfterDocumentLoad_AccumulatesFields) {
  mojom::DocumentLoadStatistics statistics;
  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());

  // First call with URL map - triggers NotifyAndResetStatistics (clears)
  statistics.num_loads_total = 5;
  statistics.num_loads_evaluated = 8;
  statistics.num_loads_matching_rules = 3;
  statistics.num_loads_disallowed = 2;
  statistics.loads_disallowed_url_map = {{"url1", 1}};
  stats.OnStatisticsAfterDocumentLoad(statistics);
  // After notify+clear, the aggregated stats are reset
  auto& agg = stats.aggregated_document_after_load_statistics_;
  EXPECT_EQ(agg.num_loads_total, 0);
  EXPECT_EQ(agg.num_loads_disallowed, 0);
}

TEST_F(PageLoadStatisticsExtTest, OnUserDocumentLoadStatistics_Accumulates) {
  mojom::DocumentLoadStatistics statistics;
  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());

  statistics.num_loads_total = 10;
  statistics.num_loads_evaluated = 20;
  statistics.num_loads_matching_rules = 30;
  statistics.num_loads_disallowed = 40;
  statistics.loads_disallowed_url_map = {{"url", 1}};
  stats.OnUserDocumentLoadStatistics(statistics);

  statistics.num_loads_total = 5;
  statistics.num_loads_evaluated = 10;
  statistics.num_loads_matching_rules = 15;
  statistics.num_loads_disallowed = 20;
  statistics.loads_disallowed_url_map = {{"url2", 2}};
  stats.OnUserDocumentLoadStatistics(statistics);

  auto agg = stats.getUserAggregatedDocumentStatistics();
  EXPECT_EQ(agg.num_loads_total, 15);       // 10 + 5
  EXPECT_EQ(agg.num_loads_evaluated, 30);   // 20 + 10
  EXPECT_EQ(agg.num_loads_disallowed, 60);  // 40 + 20
}

TEST_F(PageLoadStatisticsExtTest,
       getAggregatedDocumentStatistics_InitiallyEmpty) {
  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());

  auto agg = stats.getAggregatedDocumentStatistics();
  EXPECT_EQ(agg.num_loads_total, 0);
  EXPECT_EQ(agg.num_loads_evaluated, 0);
  EXPECT_EQ(agg.num_loads_matching_rules, 0);
  EXPECT_EQ(agg.num_loads_disallowed, 0);
  EXPECT_TRUE(agg.loads_disallowed_url_map.empty());
}

TEST_F(PageLoadStatisticsExtTest, SetReported_TwiceStaysFalse) {
  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());
  EXPECT_TRUE(stats.IsFirstReport());
  stats.SetReported();
  EXPECT_FALSE(stats.IsFirstReport());
  stats.SetReported();
  EXPECT_FALSE(stats.IsFirstReport());
}

TEST_F(PageLoadStatisticsExtTest, ClearStatistics_ClearsAllFields) {
  mojom::DocumentLoadStatistics statistics;
  statistics.num_loads_total = 100;
  statistics.num_loads_evaluated = 200;
  statistics.num_loads_matching_rules = 300;
  statistics.num_loads_disallowed = 400;
  statistics.loads_disallowed_url_map = {{"url1", 1}, {"url2", 2}};

  auto web_contents = CreateTestWebContents();
  PageLoadStatisticsExt stats(activation_state_, web_contents.get());
  stats.ClearStatistics(statistics);

  EXPECT_EQ(statistics.num_loads_total, 0);
  EXPECT_EQ(statistics.num_loads_evaluated, 0);
  EXPECT_EQ(statistics.num_loads_matching_rules, 0);
  EXPECT_EQ(statistics.num_loads_disallowed, 0);
  EXPECT_TRUE(statistics.loads_disallowed_url_map.empty());
}

TEST_F(PageLoadStatisticsExtTest, NotifyAndResetStatistics_NoWebContents) {
  PageLoadStatisticsExt stats(activation_state_, nullptr);
  // Set some data in aggregated_document_after_load_statistics_
  stats.aggregated_document_after_load_statistics_.num_loads_total = 10;
  stats.aggregated_document_after_load_statistics_.loads_disallowed_url_map = {
      {"url", 1}};
  // No crash without webcontents
  EXPECT_NO_FATAL_FAILURE(stats.NotifyAndResetStatistics());
  // web_contents_ is null so data is not cleared
  EXPECT_EQ(stats.aggregated_document_after_load_statistics_.num_loads_total,
            10);
}

TEST_F(PageLoadStatisticsExtTest, UserNotifyAndResetStatistics_NoWebContents) {
  PageLoadStatisticsExt stats(activation_state_, nullptr);
  stats.user_aggregated_document_after_load_statistics_.num_loads_total = 10;
  stats.user_aggregated_document_after_load_statistics_
      .loads_disallowed_url_map = {{"url", 1}};
  // No crash without webcontents
  EXPECT_NO_FATAL_FAILURE(stats.UserNotifyAndResetStatistics());
  // web_contents_ is null so data is not cleared
  EXPECT_EQ(
      stats.user_aggregated_document_after_load_statistics_.num_loads_total,
      10);
}
#endif
}  // namespace subresource_filter
