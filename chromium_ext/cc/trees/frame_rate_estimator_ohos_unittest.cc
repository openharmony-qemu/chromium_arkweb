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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/trees/frame_rate_estimator_ohos.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {
#if BUILDFLAG(ARKWEB_THROTTLE_FRAME)
class FrameRateEstimatorTest : public testing::Test {
 public:
  FrameRateEstimatorTest() = default;
  ~FrameRateEstimatorTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features*/ {features::
                                   kThrottleFrameRateOnManyDidNotProduceFrame},
        /* disabled_features*/ {});
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    estimator_ = std::make_unique<FrameRateEstimator>(task_runner_.get());
  }

  void TearDown() override {
    estimator_.reset();
    task_runner_.reset();
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SimpleTestTickClock test_tick_clock_;
  std::unique_ptr<FrameRateEstimator> estimator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FrameRateEstimatorTest, FrameHistoryUsed) {
  EXPECT_NE(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
  base::TimeTicks time;
  for (int i = 0; i < 10; ++i) {
    estimator_->ClearBeginFrameThrottleSettings();
    time += viz::BeginFrameArgs::DefaultInterval();
  }
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());

  estimator_->ClearBeginFrameThrottleSettings();
  EXPECT_NE(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

TEST_F(FrameRateEstimatorTest, InputPriorityMode) {
  estimator_->NotifyInputEvent();
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());

  task_runner_->RunUntilIdle();
  EXPECT_NE(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

TEST_F(FrameRateEstimatorTest, RafAtHalfFps) {
  // Recorded rAF intervals at 30 fps.
  const base::TimeDelta kIntervals[] = {
      base::Microseconds(33425), base::Microseconds(33298),
      base::Microseconds(33396), base::Microseconds(33339),
      base::Microseconds(33431), base::Microseconds(33320),
      base::Microseconds(33364), base::Microseconds(33360)};
  const base::TimeDelta kIntervalForHalfFps =
      viz::BeginFrameArgs::DefaultInterval() * 2;
  base::TimeTicks time;
  for (size_t i = 0; i <= std::size(kIntervals); ++i) {
    estimator_->ClearBeginFrameThrottleSettings();
    EXPECT_EQ(kIntervalForHalfFps, estimator_->GetPreferredInterval());
    if (i < std::size(kIntervals))
      time += kIntervals[i];
  }
}

TEST_F(FrameRateEstimatorTest, ThrottleWhenManyDidNotProduceFrame) {
  estimator_->ClearBeginFrameThrottleSettings();
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());

  EXPECT_EQ(estimator_->GetPreferredInterval(), viz::BeginFrameArgs::MinInterval());
  // Stop throttling if draw.
  estimator_->ClearBeginFrameThrottleSettings();
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}

TEST_F(FrameRateEstimatorTest, ManyDidNotProduceFrameWhenInput) {
  estimator_->ClearBeginFrameThrottleSettings();
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());

  EXPECT_EQ(estimator_->GetPreferredInterval(), viz::BeginFrameArgs::MinInterval());
  // No throttlling if input event happens.
  estimator_->NotifyInputEvent();
  EXPECT_EQ(estimator_->GetPreferredInterval(),
            viz::BeginFrameArgs::MinInterval());
}
#endif
}  // namespace
}  // namespace cc
