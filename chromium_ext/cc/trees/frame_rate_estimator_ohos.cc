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

#include "arkweb/chromium_ext/cc/trees/frame_rate_estimator_ohos.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "base/trace_event/trace_event.h"


namespace cc {
namespace {
#if BUILDFLAG(ARKWEB_THROTTLE_FRAME)
constexpr auto kInputPriorityDelay = base::Milliseconds(250);
constexpr size_t kThresholdOfNoFrameProducedToThrottle = 4u;

}  // namespace

FrameRateEstimator::FrameRateEstimator(base::SequencedTaskRunner* task_runner)
    : notifier_(
          task_runner,
          base::BindRepeating(&FrameRateEstimator::OnExitInputPriorityMode,
                              base::Unretained(this)),
          kInputPriorityDelay) {}

FrameRateEstimator::~FrameRateEstimator() = default;

void FrameRateEstimator::DidNotProduceFrameWithReason(FrameSkippedReason reason) {
    if (reason == FrameSkippedReason::kNoDamage) {
        ++num_no_damage_did_not_produce_frame_;
        if (num_no_damage_did_not_produce_frame_ > kThresholdOfNoFrameProducedToThrottle &&
            !begin_frame_throttle_mode_) {
            begin_frame_throttle_mode_ = true;
        }
    } else {
        ClearBeginFrameThrottleSettings();
    }
}

void FrameRateEstimator::ClearBeginFrameThrottleSettings() {
    num_no_damage_did_not_produce_frame_ = 0u;
    begin_frame_throttle_mode_ = false;
}

base::TimeDelta FrameRateEstimator::GetPreferredInterval() const {
  if (input_priority_mode_) {
    return viz::BeginFrameArgs::MinInterval();
  }

  if (begin_frame_throttle_mode_ &&
      num_no_damage_did_not_produce_frame_ > kThresholdOfNoFrameProducedToThrottle) {
    return viz::BeginFrameArgs::DefaultInterval() * 2;
  }

  return viz::BeginFrameArgs::MinInterval();
}

void FrameRateEstimator::NotifyInputEvent() {
  input_priority_mode_ = true;
  notifier_.Schedule();
}

void FrameRateEstimator::OnExitInputPriorityMode() {
  input_priority_mode_ = false;
}

#endif
}  // namespace cc
