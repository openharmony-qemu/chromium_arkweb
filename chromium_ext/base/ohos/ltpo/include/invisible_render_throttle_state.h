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

#ifndef BASE_OHOS_LTPO_INVISIBLE_RENDER_THROTTLE_STATE_H_
#define BASE_OHOS_LTPO_INVISIBLE_RENDER_THROTTLE_STATE_H_

#include "base/containers/flat_map.h"
#include "base/time/time.h"

namespace base {
namespace ohos {

// Throttle state for Web page rendering
enum class RenderingThrottleState {
  // Normal rendering, no throttling
  NORMAL = 0,
  // Strong throttling after invisible timeout (e.g., 1fps)
  INVISIBLE_THROTTLED = 1,
};

// Per-NWeb throttle state tracking
struct PerNWebThrottleInfo {
  // Current throttle state
  RenderingThrottleState state = RenderingThrottleState::NORMAL;

  // Timestamp when entered invisible state
  base::TimeTicks invisible_since;

  // Whether Web component is visible (distinct from active state)
  bool visible;
};

}  // namespace ohos
}  // namespace base

#endif  // BASE_OHOS_LTPO_INVISIBLE_RENDER_THROTTLE_STATE_H_