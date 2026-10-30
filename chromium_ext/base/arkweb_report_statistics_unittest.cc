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

#include "arkweb/chromium_ext/base/arkweb_report_statistics.h"

#include <string>

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace ohos {

namespace {
// Global flag captured by the report callback. Must be reset before each test.
bool g_callback_called = false;
std::string g_callback_content;

void OnReportStatisticLog(const char* content) {
  g_callback_called = true;
  if (content != nullptr) {
    g_callback_content = content;
  }
}
}  // namespace

class ReportStatisticsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    g_callback_called = false;
    g_callback_content.clear();
    // Reset the callback to avoid leaking state across tests.
    SetOnReportStatisticLogCallback(nullptr);
  }

  void TearDown() override {
    SetOnReportStatisticLogCallback(nullptr);
    g_callback_called = false;
    g_callback_content.clear();
  }

  base::test::TaskEnvironment task_environment_;  // Must outlive run loops.
};

// NOTE: This test MUST stay before any test that calls
// SetReportStatisticTaskRunner(): the ReportStatistic singleton's task_runner_
// is set permanently once initialized, and there is no reset API. gtest runs
// tests in declaration order within a suite.
TEST_F(ReportStatisticsTest, Statistics_NoTaskRunnerNoCrash) {
  // task_runner_ is still null here (no SetReportStatisticTaskRunner call yet):
  // ReportStatisticLog returns early without posting or invoking the callback.
  SetOnReportStatisticLogCallback(&OnReportStatisticLog);
  OperationStatistics::Statistics(REGION_CHINA, PLATFORM_OPERATION_ANALYSIS,
                                  OperationStatistics::GROUP_BECE, "event_id",
                                  OperationStatistics::DEFAULT_DATA_VERSION,
                                  "content", false, true, false,
                                  REPORT_IMMEDIATELY);

  EXPECT_FALSE(g_callback_called);
}

TEST_F(ReportStatisticsTest, Statistics_WritesJsonAndCallsCallback) {
  // Bind the singleton's task_runner_ to this test's main thread.
  SetReportStatisticTaskRunner();
  SetOnReportStatisticLogCallback(&OnReportStatisticLog);

  OperationStatistics::Statistics(REGION_CHINA, PLATFORM_OPERATION_ANALYSIS,
                                  OperationStatistics::GROUP_BECE, "event_id",
                                  OperationStatistics::DEFAULT_DATA_VERSION,
                                  "content", false, true, false,
                                  REPORT_IMMEDIATELY);

  EXPECT_TRUE(g_callback_called);
  // The JSON payload should contain the event id.
  EXPECT_NE(g_callback_content.find("event_id"), std::string::npos);
  EXPECT_NE(g_callback_content.find("content"), std::string::npos);
}

TEST_F(ReportStatisticsTest, Statistics_NoCallbackNoCrash) {
  // Re-bind the singleton's task_runner_ to this test's main thread so we do
  // not touch a runner belonging to an earlier (destroyed) TaskEnvironment.
  SetReportStatisticTaskRunner();
  // Callback is not set: the else-if branch is not taken.
  OperationStatistics::Statistics(REGION_CHINA, PLATFORM_OPERATION_ANALYSIS,
                                  OperationStatistics::GROUP_BECE, "event_id",
                                  OperationStatistics::DEFAULT_DATA_VERSION,
                                  "content", false, true, false,
                                  REPORT_IMMEDIATELY);

  EXPECT_FALSE(g_callback_called);
}

TEST_F(ReportStatisticsTest, Statistics_ConstantsAreValid) {
  EXPECT_STREQ(OperationStatistics::GROUP_BECE, "BECE");
  EXPECT_STREQ(OperationStatistics::GROUP_BWCE, "BWCE");
  EXPECT_EQ(OperationStatistics::DEFAULT_DATA_VERSION, 1);
}

TEST_F(ReportStatisticsTest, Statistics_FromOtherThreadPostsBack) {
  // Re-bind the singleton's task_runner_ to THIS test's main thread. Without
  // this, task_runner_ would point at a TaskEnvironment from an earlier test
  // that has already been destroyed.
  SetReportStatisticTaskRunner();
  // Calling Statistics() from another thread makes !BelongsToCurrentThread()
  // true, so ReportStatisticLog posts the task back to the bound runner
  // instead of invoking the callback inline.
  SetOnReportStatisticLogCallback(&OnReportStatisticLog);

  base::Thread worker("stat_worker");
  ASSERT_TRUE(worker.Start());
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  worker.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WaitableEvent* done) {
                       OperationStatistics::Statistics(
                           REGION_CHINA, PLATFORM_OPERATION_ANALYSIS,
                           OperationStatistics::GROUP_BECE, "event_id",
                           OperationStatistics::DEFAULT_DATA_VERSION, "content",
                           false, true, false, REPORT_IMMEDIATELY);
                       done->Signal();
                     },
                     &done));
  done.Wait();

  // The posted task runs on the test thread (the bound runner). Pump it.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(g_callback_called);
}

}  // namespace ohos
}  // namespace base
