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

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>

#include "base/command_line.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/vector2d.h"

#if BUILDFLAG(IS_ARKWEB)
#define private public
#include "arkweb/chromium_ext/ui/gl/events/native_scroller_ohos.h"
#undef private
#endif

#if BUILDFLAG(IS_ARKWEB)

namespace arkweb {

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
  }
};

}  // namespace arkweb

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static arkweb::Environment env;

  FuzzedDataProvider fdp(data, size);

  auto scroller = std::make_unique<ui::NativeScrollerOhos>();

  int selector = fdp.ConsumeIntegralInRange<int>(0, 10);

  switch (selector) {
    case 0: {
      scroller->curr_y_ = 50.0;
      scroller->curr_velocity_y_ = 0.5;
      scroller->final_y_ = 100.0;
      scroller->velocity_threshold_ = 10.0;
      bool should_abort = scroller->ShouldAbortAnimation();
      (void)should_abort;
      break;
    }
    case 1: {
      scroller->curr_y_ = 100.0;
      scroller->curr_velocity_y_ = 0.5;
      scroller->final_y_ = 100.0;
      scroller->velocity_threshold_ = 1.0;
      bool should_abort1 = scroller->ShouldAbortAnimation();
      (void)should_abort1;
      scroller->AbortAnimation();
      bool is_finished = scroller->finisheed_;
      (void)is_finished;
      break;
    }
    case 2: {
      scroller->curr_y_ = 0.0;
      scroller->curr_velocity_y_ = 100.0;
      scroller->final_y_ = 100.0;
      scroller->velocity_threshold_ = 1.0;
      bool should_abort = scroller->ShouldAbortAnimation();
      (void)should_abort;
      scroller->AbortAnimation();
      break;
    }
    case 3: {
      scroller->curr_y_ = 100.0;
      scroller->curr_velocity_y_ = 100.0;
      scroller->final_y_ = 100.0;
      scroller->velocity_threshold_ = 200.0;
      bool should_abort = scroller->ShouldAbortAnimation();
      (void)should_abort;
      scroller->AbortAnimation();
      break;
    }
    case 4: {
      scroller->Fling(0, 0, 0.0f, 5000.0f, INT_MIN, INT_MAX, INT_MIN, INT_MAX,
                       base::TimeTicks(),
                       ui::NativeScrollerOhos::GestureDevice::kTouchscreen);
      for (int i = 0; i < 5; ++i) {
        gfx::Vector2dF offset, velocity;
        base::TimeTicks time = base::TimeTicks() + base::Milliseconds(fdp.ConsumeIntegralInRange<int>(0, 500));
        scroller->ComputeScrollOffset(time, &offset, &velocity);
      }
      scroller->AbortAnimation();
      gfx::Vector2dF offset, velocity;
      scroller->ComputeScrollOffset(base::TimeTicks() + base::Milliseconds(1000), &offset, &velocity);
      break;
    }
    case 5: {
      scroller->Fling(0, 100.0f, 0.0f, 5000.0f, INT_MIN, INT_MAX, INT_MIN, INT_MAX,
                       base::TimeTicks(),
                       ui::NativeScrollerOhos::GestureDevice::kTouchscreen);
      float time1 = fdp.ConsumeFloatingPointInRange<float>(0.0f, 2.0f);
      float time2 = fdp.ConsumeFloatingPointInRange<float>(0.0f, 2.0f);
      double pos1 = scroller->GetPosition(time1);
      double vel1 = scroller->GetVelocity(time1);
      double pos2 = scroller->GetPosition(time2);
      double vel2 = scroller->GetVelocity(time2);
      (void)pos1; (void)vel1; (void)pos2; (void)vel2;
      break;
    }
    case 6: {
      scroller->Fling(0, 0, 0.0f, 5000.0f, INT_MIN, INT_MAX, INT_MIN, INT_MAX,
                       base::TimeTicks(),
                       ui::NativeScrollerOhos::GestureDevice::kTouchpad);
      gfx::Vector2dF offset, velocity;
      scroller->ComputeScrollOffset(base::TimeTicks() + base::Milliseconds(100), &offset, &velocity);
      break;
    }
    case 7: {
      scroller->Fling(0, 50.0f, 0.0f, 0.0f, INT_MIN, INT_MAX, INT_MIN, INT_MAX,
                       base::TimeTicks(),
                       ui::NativeScrollerOhos::GestureDevice::kTouchscreen);
      gfx::Vector2dF offset, velocity;
      scroller->ComputeScrollOffset(base::TimeTicks() + base::Milliseconds(100), &offset, &velocity);
      break;
    }
    case 8: {
      scroller->Fling(0, 100.0f, 0.0f, -5000.0f, INT_MIN, INT_MAX, INT_MIN, INT_MAX,
                       base::TimeTicks(),
                       ui::NativeScrollerOhos::GestureDevice::kTouchscreen);
      float time1 = fdp.ConsumeFloatingPointInRange<float>(0.0f, 2.0f);
      float time2 = fdp.ConsumeFloatingPointInRange<float>(0.0f, 2.0f);
      double pos1 = scroller->GetPosition(time1);
      double vel1 = scroller->GetVelocity(time1);
      double pos2 = scroller->GetPosition(time2);
      double vel2 = scroller->GetVelocity(time2);
      (void)pos1; (void)vel1; (void)pos2; (void)vel2;
      break;
    }
    case 9: {
      scroller->Fling(0, 0, 0.0f, 5000.0f, INT_MIN, INT_MAX, INT_MIN, INT_MAX,
                       base::TimeTicks(),
                       ui::NativeScrollerOhos::GestureDevice::kTouchscreen);
      gfx::Vector2dF offset, velocity;
      scroller->ComputeScrollOffset(base::TimeTicks() - base::Milliseconds(100), &offset, &velocity);
      break;
    }
    case 10: {
      scroller->curr_y_ = 0.0;
      scroller->curr_velocity_y_ = 100.0;
      scroller->final_y_ = 100.0;
      scroller->velocity_threshold_ = 1.0;
      scroller->ShouldAbortAnimation();
      scroller->AbortAnimation();
      gfx::Vector2dF offset, velocity;
      scroller->ComputeScrollOffset(base::TimeTicks() + base::Milliseconds(100), &offset, &velocity);
      break;
    }
  }

  return 0;
}

#else

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return 0;
}

#endif