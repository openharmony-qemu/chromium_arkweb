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

#include "base/ohos/ltpo/include/invisible_rendering_throttler.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"

namespace base {
namespace ohos {

static constexpr int64_t kNanosecondsPerSecond = 1000000000;

void InvisibleRenderingThrottler::InitConfigWithParams(int32_t timeout,
                                                         int32_t ratio) {
  config_.invisible_throttle_timeout = base::Milliseconds(timeout);
  config_.invisible_throttle_ratio = ratio;

  LOG(INFO) << "InvisibleRenderingThrottler Config init with params - "
            << "invisible_timeout_ms=" << config_.invisible_throttle_timeout.InMilliseconds()
            << ", invisible_ratio=" << config_.invisible_throttle_ratio;
}

void InvisibleRenderingThrottler::SetWebComponentVisible(int nweb_id, bool visible) {
  auto& info = throttle_info_map_[nweb_id];
  info.visible = visible;

  if (visible) {
    info.invisible_since = base::TimeTicks();
    info.state = RenderingThrottleState::NORMAL;
  } else if (info.invisible_since.is_null()) {
    info.invisible_since = base::TimeTicks::Now();
  }
  EvaluateThrottleState(nweb_id);
}

bool InvisibleRenderingThrottler::ShouldThrottleInvisible(
    int nweb_id, base::TimeDelta timeout) const {
  auto it = throttle_info_map_.find(nweb_id);
  if (it == throttle_info_map_.end()) {
    return false;
  }

  auto& info = it->second;
  if (info.visible) {
    return false;
  }
  base::TimeDelta invisible_duration = base::TimeTicks::Now() - info.invisible_since;
  return invisible_duration >= timeout;
}

int32_t InvisibleRenderingThrottler::ThrottleRatio() {
  if (throttle_info_map_.empty()) {
    LOG(DEBUG) << "ShouldThrottle: no web components";
    return 1;
  }

  // Check if all web components are in INVISIBLE_THROTTLED state
  for (const auto& pair : throttle_info_map_) {
    const auto& info = pair.second;
    if (info.state != RenderingThrottleState::INVISIBLE_THROTTLED) {
      LOG(DEBUG) << "ThrottleRatio: nweb_id " << pair.first
                 << " is not INVISIBLE_THROTTLED";
      return 1;
    }
  }

  LOG(DEBUG) << "ThrottleRatio: all components are INVISIBLE_THROTTLED";
  return config_.invisible_throttle_ratio;
}

void InvisibleRenderingThrottler::EvaluateThrottleState(int nweb_id) {
  // Update individual component states for tracking
  auto it = throttle_info_map_.find(nweb_id);
  if (it != throttle_info_map_.end()) {
    auto& info = it->second;
    bool should_throttle = ShouldThrottleInvisible(nweb_id, config_.invisible_throttle_timeout);
    RenderingThrottleState new_state = should_throttle ?
        RenderingThrottleState::INVISIBLE_THROTTLED : RenderingThrottleState::NORMAL;

    if (info.state != new_state) {
      info.state = new_state;
    }
  }
}

void InvisibleRenderingThrottler::EvaluateAllThrottleStates() {
  for (const auto& pair : throttle_info_map_) {
    EvaluateThrottleState(pair.first);
  }
}

}  // namespace ohos
}  // namespace base