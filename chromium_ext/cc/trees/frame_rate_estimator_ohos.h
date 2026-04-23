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

#ifndef CC_TREES_FRAME_RATE_ESTIMATOR_OHOS_H_
#define CC_TREES_FRAME_RATE_ESTIMATOR_OHOS_H_

#include <cstdint>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/base/delayed_unique_notifier.h"
#include "cc/cc_export.h"
#if BUILDFLAG(ARKWEB_THROTTLE_FRAME)
#include "cc/scheduler/scheduler.h"
#endif
namespace cc {
#if BUILDFLAG(ARKWEB_THROTTLE_FRAME)
class CC_EXPORT FrameRateEstimator {
 public:
  explicit FrameRateEstimator(base::SequencedTaskRunner* task_runner);
  ~FrameRateEstimator();

  void ClearBeginFrameThrottleSettings();
  void NotifyInputEvent();
  base::TimeDelta GetPreferredInterval() const;
  bool input_priority_mode() const {
    return input_priority_mode_;
  }
  bool GetBeginFrameThrottleMode() const {
    return !input_priority_mode_ && begin_frame_throttle_mode_;
  }
  void DidNotProduceFrameWithReason(FrameSkippedReason reason);

 private:
  void OnExitInputPriorityMode();

  // Number of "did not produce frame" since the last draw.
  uint64_t num_did_not_produce_frame_since_last_draw_ = 0;

  // Number of "did not produce frame" caused by no damage continuously.
  uint64_t num_no_damage_did_not_produce_frame_ = 0;

  bool begin_frame_throttle_mode_ = false;

  uint64_t num_of_consecutive_frames_with_min_delta_ = 0u;

  // We conservatively switch to high frame rate after an input event to lower
  // the input latency for a minimum duration. This tracks when we are in this
  // mode.
  bool input_priority_mode_ = false;
  DelayedUniqueNotifier notifier_;
};
#endif
}  // namespace cc

#endif  // CC_TREES_FRAME_RATE_ESTIMATOR_H_
