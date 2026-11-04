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

#ifndef BASE_OHOS_LTPO_INVISIBLE_RENDERING_THROTTLER_H_
#define BASE_OHOS_LTPO_INVISIBLE_RENDERING_THROTTLER_H_

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/ohos/ltpo/include/invisible_render_throttle_state.h"

namespace base {
namespace ohos {

constexpr int kDefaultInvisibleThrottleTimeoutMs = 5000;
constexpr int kDefaultInvisibleThrottleRatio = 10;
constexpr int kDefaultFrameRate = 60;
constexpr int kMinFrameRate = 30;
constexpr int kMaxFrameRate = 120;

struct ThrottleConfig {
  base::TimeDelta invisible_throttle_timeout = base::Milliseconds(kDefaultInvisibleThrottleTimeoutMs);
  int invisible_throttle_ratio = kDefaultInvisibleThrottleRatio;
};

// Tracks Web component visibility state and invisible rendering duration
class InvisibleRenderingThrottler {
 public:
  InvisibleRenderingThrottler() = default;
  ~InvisibleRenderingThrottler() = default;

  InvisibleRenderingThrottler(const InvisibleRenderingThrottler&) = delete;
  InvisibleRenderingThrottler& operator=(const InvisibleRenderingThrottler&) = delete;

  // Set configuration parameters
  void InitConfigWithParams(int32_t timeout, int32_t ratio);

  // Get current configuration
  const ThrottleConfig& GetConfig() const { return config_; }

  // Set Web component visibility (independent of active state)
  void SetWebComponentVisible(int nweb_id, bool visible);
  int32_t ThrottleRatio();
  void EvaluateAllThrottleStates();

 private:
 // Check if invisible duration exceeds timeout for throttling
  bool ShouldThrottleInvisible(int nweb_id, base::TimeDelta timeout) const;
  void EvaluateThrottleState(int nweb_id);


  ThrottleConfig config_;
  base::flat_map<int32_t, PerNWebThrottleInfo> throttle_info_map_;
};

}  // namespace ohos
}  // namespace base

#endif  // BASE_OHOS_LTPO_INVISIBLE_RENDERING_THROTTLER_H_