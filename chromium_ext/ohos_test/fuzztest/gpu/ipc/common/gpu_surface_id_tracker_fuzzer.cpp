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

#include "arkweb/chromium_ext/gpu/ipc/common/gpu_surface_id_tracker.h"
#include "base/command_line.h"
#include "gpu/ipc/common/surface_handle.h"

#include <climits>
#include <cstdint>
#include <memory>
#include <string>

namespace {

constexpr size_t MAX_ITERATIONS = 50;
constexpr size_t MAX_STRING_LENGTH = 256;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
  }
};

void FuzzAddSurfaceForNativeWidget(FuzzedDataProvider* fdp) {
  gpu::GpuSurfaceIdTracker* tracker = gpu::GpuSurfaceIdTracker::GetInstance();
  if (!tracker) {
    return;
  }

  size_t num_adds = fdp->ConsumeIntegralInRange<size_t>(1, MAX_ITERATIONS);
  for (size_t i = 0; i < num_adds; ++i) {
    gfx::AcceleratedWidget widget = 
        static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());
    
    std::string surface_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);

    gpu::GpuSurfaceIdTracker::SurfaceRecord record(widget, surface_id);
    int surface_handle = tracker->AddSurfaceForNativeWidget(std::move(record));
    (void)surface_handle;
  }
}

void FuzzIsValidSurfaceHandle(FuzzedDataProvider* fdp) {
  gpu::GpuSurfaceIdTracker* tracker = gpu::GpuSurfaceIdTracker::GetInstance();
  if (!tracker) {
    return;
  }

  gfx::AcceleratedWidget widget = 
      static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());
  std::string surface_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);

  gpu::GpuSurfaceIdTracker::SurfaceRecord record(widget, surface_id);
  gpu::SurfaceHandle valid_handle = tracker->AddSurfaceForNativeWidget(std::move(record));

  size_t num_checks = fdp->ConsumeIntegralInRange<size_t>(1, 20);
  for (size_t i = 0; i < num_checks; ++i) {
    gpu::SurfaceHandle test_handle = fdp->ConsumeBool() ? valid_handle :
                           fdp->ConsumeIntegral<gpu::SurfaceHandle>();

    bool is_valid = tracker->IsValidSurfaceHandle(test_handle);
    (void)is_valid;
  }
}

void FuzzRemoveSurface(FuzzedDataProvider* fdp) {
  gpu::GpuSurfaceIdTracker* tracker = gpu::GpuSurfaceIdTracker::GetInstance();
  if (!tracker) {
    return;
  }

  gfx::AcceleratedWidget widget1 = 
      static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());
  gfx::AcceleratedWidget widget2 = 
      static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());
  gfx::AcceleratedWidget widget3 = 
      static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());

  std::string surface_id1 = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string surface_id2 = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  std::string surface_id3 = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);

  gpu::GpuSurfaceIdTracker::SurfaceRecord record1(widget1, surface_id1);
  gpu::GpuSurfaceIdTracker::SurfaceRecord record2(widget2, surface_id2);
  gpu::GpuSurfaceIdTracker::SurfaceRecord record3(widget3, surface_id3);

  gpu::SurfaceHandle handle1 = tracker->AddSurfaceForNativeWidget(std::move(record1));
  gpu::SurfaceHandle handle2 = tracker->AddSurfaceForNativeWidget(std::move(record2));
  gpu::SurfaceHandle handle3 = tracker->AddSurfaceForNativeWidget(std::move(record3));

  tracker->RemoveSurface(handle1);

  tracker->RemoveSurface(handle2);

  tracker->RemoveSurface(handle3);
}

void FuzzAcquireNativeImageSurfaceId(FuzzedDataProvider* fdp) {
  gpu::GpuSurfaceIdTracker* tracker = gpu::GpuSurfaceIdTracker::GetInstance();
  if (!tracker) {
    return;
  }

  gfx::AcceleratedWidget widget = 
      static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());
  std::string surface_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);

  gpu::GpuSurfaceIdTracker::SurfaceRecord record(widget, surface_id);
  gpu::SurfaceHandle handle = tracker->AddSurfaceForNativeWidget(std::move(record));

  size_t num_acquires = fdp->ConsumeIntegralInRange<size_t>(1, 20);
  for (size_t i = 0; i < num_acquires; ++i) {
    gpu::SurfaceHandle test_handle = fdp->ConsumeBool() ? handle :
                           fdp->ConsumeIntegral<gpu::SurfaceHandle>();

    std::string acquired_id = tracker->AcquireNativeImageSurfaceId(test_handle);
    (void)acquired_id;
  }
}

void FuzzGetSurfaceCount(FuzzedDataProvider* fdp) {
  gpu::GpuSurfaceIdTracker* tracker = gpu::GpuSurfaceIdTracker::GetInstance();
  if (!tracker) {
    return;
  }

  size_t initial_count = tracker->GetSurfaceCount();
  (void)initial_count;

  size_t num_adds = fdp->ConsumeIntegralInRange<size_t>(1, 10);
  for (size_t i = 0; i < num_adds; ++i) {
    gfx::AcceleratedWidget widget = 
        static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());
    std::string surface_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);

    gpu::GpuSurfaceIdTracker::SurfaceRecord record(widget, surface_id);
    tracker->AddSurfaceForNativeWidget(std::move(record));
  }

  size_t after_add_count = tracker->GetSurfaceCount();
  (void)after_add_count;
}

void FuzzMixedOperations(FuzzedDataProvider* fdp) {
  gpu::GpuSurfaceIdTracker* tracker = gpu::GpuSurfaceIdTracker::GetInstance();
  if (!tracker) {
    return;
  }

  size_t operations = fdp->ConsumeIntegralInRange<size_t>(10, MAX_ITERATIONS);

  for (size_t i = 0; i < operations; ++i) {
    uint8_t op_type = fdp->ConsumeIntegralInRange<uint8_t>(0, 4);

    switch (op_type) {
      case 0: {
        gfx::AcceleratedWidget widget = 
            static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());
        std::string surface_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
        gpu::GpuSurfaceIdTracker::SurfaceRecord record(widget, surface_id);
        int handle = tracker->AddSurfaceForNativeWidget(std::move(record));
        (void)handle;
        break;
      }
      case 1: {
        gpu::SurfaceHandle handle = fdp->ConsumeIntegral<gpu::SurfaceHandle>();
        bool is_valid = tracker->IsValidSurfaceHandle(handle);
        (void)is_valid;
        break;
      }
      case 2: {
        gpu::SurfaceHandle handle = fdp->ConsumeIntegral<gpu::SurfaceHandle>();
        tracker->RemoveSurface(handle);
        break;
      }
      case 3: {
        gpu::SurfaceHandle handle = fdp->ConsumeIntegral<gpu::SurfaceHandle>();
        std::string surface_id = tracker->AcquireNativeImageSurfaceId(handle);
        (void)surface_id;
        break;
      }
      case 4: {
        std::size_t count = tracker->GetSurfaceCount();
        (void)count;
        break;
      }
    }
  }
}

void FuzzBoundaryValues(FuzzedDataProvider* fdp) {
  gpu::GpuSurfaceIdTracker* tracker = gpu::GpuSurfaceIdTracker::GetInstance();
  if (!tracker) {
    return;
  }

  gfx::AcceleratedWidget widget_null = gfx::kNullAcceleratedWidget;
  gfx::AcceleratedWidget widget_max = 
      static_cast<gfx::AcceleratedWidget>(INTPTR_MAX);

  std::string empty_id = "";
  std::string long_id(MAX_STRING_LENGTH, 'a');

  gpu::GpuSurfaceIdTracker::SurfaceRecord record1(widget_null, empty_id);
  tracker->AddSurfaceForNativeWidget(std::move(record1));

  gpu::GpuSurfaceIdTracker::SurfaceRecord record2(widget_max, long_id);
  tracker->AddSurfaceForNativeWidget(std::move(record2));

  tracker->IsValidSurfaceHandle(INT32_MIN);
  tracker->IsValidSurfaceHandle(INT32_MAX);
  tracker->IsValidSurfaceHandle(0);
  tracker->IsValidSurfaceHandle(-1);

  tracker->AcquireNativeImageSurfaceId(INT32_MIN);
  tracker->AcquireNativeImageSurfaceId(INT32_MAX);
  tracker->AcquireNativeImageSurfaceId(0);

  std::size_t count = tracker->GetSurfaceCount();
  (void)count;
}

void FuzzInvalidInputs(FuzzedDataProvider* fdp) {
  gpu::GpuSurfaceIdTracker* tracker = gpu::GpuSurfaceIdTracker::GetInstance();
  if (!tracker) {
    return;
  }

  tracker->IsValidSurfaceHandle(fdp->ConsumeIntegral<gpu::SurfaceHandle>());

  tracker->AcquireNativeImageSurfaceId(fdp->ConsumeIntegral<gpu::SurfaceHandle>());

  tracker->GetSurfaceCount();

  gfx::AcceleratedWidget widget = 
      static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());
  std::string surface_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  gpu::GpuSurfaceIdTracker::SurfaceRecord record(widget, surface_id);
  gpu::SurfaceHandle handle = tracker->AddSurfaceForNativeWidget(std::move(record));

  tracker->RemoveSurface(handle);
}

void FuzzStressTest(FuzzedDataProvider* fdp) {
  gpu::GpuSurfaceIdTracker* tracker = gpu::GpuSurfaceIdTracker::GetInstance();
  if (!tracker) {
    return;
  }

  size_t iterations = fdp->ConsumeIntegralInRange<size_t>(50, 200);

  for (size_t i = 0; i < iterations; ++i) {
    gfx::AcceleratedWidget widget = 
        static_cast<gfx::AcceleratedWidget>(fdp->ConsumeIntegral<intptr_t>());
    std::string surface_id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);

    gpu::GpuSurfaceIdTracker::SurfaceRecord record(widget, surface_id);
    gpu::SurfaceHandle handle = tracker->AddSurfaceForNativeWidget(std::move(record));

    bool is_valid = tracker->IsValidSurfaceHandle(handle);

    std::string acquired_id = tracker->AcquireNativeImageSurfaceId(handle);

    tracker->RemoveSurface(handle);

    tracker->IsValidSurfaceHandle(handle);

    (void)is_valid;
    (void)acquired_id;
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static Environment env;

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 7);

  switch (selector) {
    case 0:
      FuzzAddSurfaceForNativeWidget(&fdp);
      break;
    case 1:
      FuzzIsValidSurfaceHandle(&fdp);
      break;
    case 2:
      FuzzRemoveSurface(&fdp);
      break;
    case 3:
      FuzzAcquireNativeImageSurfaceId(&fdp);
      break;
    case 4:
      FuzzGetSurfaceCount(&fdp);
      break;
    case 5:
      FuzzMixedOperations(&fdp);
      break;
    case 6:
      FuzzBoundaryValues(&fdp);
      break;
    case 7:
      FuzzStressTest(&fdp);
      break;
    default:
      FuzzAddSurfaceForNativeWidget(&fdp);
      break;
  }

  return 0;
}