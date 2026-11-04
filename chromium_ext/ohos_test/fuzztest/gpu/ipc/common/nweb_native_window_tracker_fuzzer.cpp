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

#include "arkweb/chromium_ext/gpu/ipc/common/nweb_native_window_tracker.h"
#include "base/command_line.h"

#include <climits>
#include <cstdint>
#include <memory>

namespace {

constexpr size_t MAX_ITERATIONS = 50;
constexpr int32_t MAX_WINDOW_ID = 10000;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
  }
};

void FuzzAddNativeWindow(FuzzedDataProvider* fdp) {
  NWebNativeWindowTracker* tracker = NWebNativeWindowTracker::GetInstance();
  if (!tracker) {
    return;
  }

  size_t num_adds = fdp->ConsumeIntegralInRange<size_t>(1, MAX_ITERATIONS);
  for (size_t i = 0; i < num_adds; ++i) {
    uintptr_t ptr_value = fdp->ConsumeIntegral<uintptr_t>();
    void* native_window = reinterpret_cast<void*>(ptr_value);

    int32_t window_id = tracker->AddNativeWindow(native_window);
    (void)window_id;
  }
}

void FuzzGetNativeWindow(FuzzedDataProvider* fdp) {
  NWebNativeWindowTracker* tracker = NWebNativeWindowTracker::GetInstance();
  if (!tracker) {
    return;
  }

  uintptr_t ptr_value = fdp->ConsumeIntegral<uintptr_t>();
  void* native_window = reinterpret_cast<void*>(ptr_value);

  int32_t window_id = tracker->AddNativeWindow(native_window);

  size_t num_gets = fdp->ConsumeIntegralInRange<size_t>(1, 20);
  for (size_t i = 0; i < num_gets; ++i) {
    int32_t test_id = fdp->ConsumeBool() ? window_id : 
                      fdp->ConsumeIntegralInRange<int32_t>(INT32_MIN, MAX_WINDOW_ID);
    bool is_ref = fdp->ConsumeBool();

    void* result = tracker->GetNativeWindow(test_id, is_ref);
    (void)result;
  }
}

void FuzzDestroyNativeWindow(FuzzedDataProvider* fdp) {
  NWebNativeWindowTracker* tracker = NWebNativeWindowTracker::GetInstance();
  if (!tracker) {
    return;
  }

  uintptr_t ptr_value1 = fdp->ConsumeIntegral<uintptr_t>();
  uintptr_t ptr_value2 = fdp->ConsumeIntegral<uintptr_t>();
  uintptr_t ptr_value3 = fdp->ConsumeIntegral<uintptr_t>();

  void* window1 = reinterpret_cast<void*>(ptr_value1);
  void* window2 = reinterpret_cast<void*>(ptr_value2);
  void* window3 = reinterpret_cast<void*>(ptr_value3);

  int32_t id1 = tracker->AddNativeWindow(window1);
  int32_t id2 = tracker->AddNativeWindow(window2);
  int32_t id3 = tracker->AddNativeWindow(window3);

  tracker->DestroyNativeWindow(id1);

  tracker->DestroyNativeWindow(id2);

  tracker->DestroyNativeWindow(id3);

  tracker->DestroyNativeWindow(fdp->ConsumeIntegral<int32_t>());
}

void FuzzCheckNativeWindow(FuzzedDataProvider* fdp) {
  NWebNativeWindowTracker* tracker = NWebNativeWindowTracker::GetInstance();
  if (!tracker) {
    return;
  }

  uintptr_t ptr_value = fdp->ConsumeIntegral<uintptr_t>();
  void* native_window = reinterpret_cast<void*>(ptr_value);

  tracker->AddNativeWindow(native_window);

  size_t num_checks = fdp->ConsumeIntegralInRange<size_t>(1, 20);
  for (size_t i = 0; i < num_checks; ++i) {
    uintptr_t check_ptr = fdp->ConsumeBool() ? ptr_value : 
                          fdp->ConsumeIntegral<uintptr_t>();
    void* check_window = reinterpret_cast<void*>(check_ptr);

    bool exists = tracker->CheckNativeWindow(check_window);
    (void)exists;
  }
}

void FuzzMixedOperations(FuzzedDataProvider* fdp) {
  NWebNativeWindowTracker* tracker = NWebNativeWindowTracker::GetInstance();
  if (!tracker) {
    return;
  }

  size_t operations = fdp->ConsumeIntegralInRange<size_t>(10, MAX_ITERATIONS);

  for (size_t i = 0; i < operations; ++i) {
    uint8_t op_type = fdp->ConsumeIntegralInRange<uint8_t>(0, 3);

    switch (op_type) {
      case 0: {
        uintptr_t ptr_value = fdp->ConsumeIntegral<uintptr_t>();
        void* native_window = reinterpret_cast<void*>(ptr_value);
        int32_t window_id = tracker->AddNativeWindow(native_window);
        (void)window_id;
        break;
      }
      case 1: {
        int32_t window_id = fdp->ConsumeIntegral<int32_t>();
        bool is_ref = fdp->ConsumeBool();
        void* result = tracker->GetNativeWindow(window_id, is_ref);
        (void)result;
        break;
      }
      case 2: {
        int32_t window_id = fdp->ConsumeIntegral<int32_t>();
        tracker->DestroyNativeWindow(window_id);
        break;
      }
      case 3: {
        uintptr_t ptr_value = fdp->ConsumeIntegral<uintptr_t>();
        void* native_window = reinterpret_cast<void*>(ptr_value);
        bool exists = tracker->CheckNativeWindow(native_window);
        (void)exists;
        break;
      }
    }
  }
}

void FuzzBoundaryValues(FuzzedDataProvider* fdp) {
  NWebNativeWindowTracker* tracker = NWebNativeWindowTracker::GetInstance();
  if (!tracker) {
    return;
  }

  tracker->AddNativeWindow(nullptr);

  tracker->AddNativeWindow(reinterpret_cast<void*>(UINTPTR_MAX));

  tracker->GetNativeWindow(INT32_MIN, fdp->ConsumeBool());
  tracker->GetNativeWindow(INT32_MAX, fdp->ConsumeBool());
  tracker->GetNativeWindow(0, fdp->ConsumeBool());
  tracker->GetNativeWindow(-1, fdp->ConsumeBool());

  tracker->DestroyNativeWindow(INT32_MIN);
  tracker->DestroyNativeWindow(INT32_MAX);
  tracker->DestroyNativeWindow(0);
  tracker->DestroyNativeWindow(-1);

  tracker->CheckNativeWindow(nullptr);
  tracker->CheckNativeWindow(reinterpret_cast<void*>(UINTPTR_MAX));
}

void FuzzInvalidInputs(FuzzedDataProvider* fdp) {
  NWebNativeWindowTracker* tracker = NWebNativeWindowTracker::GetInstance();
  if (!tracker) {
    return;
  }

  tracker->GetNativeWindow(fdp->ConsumeIntegral<int32_t>(), fdp->ConsumeBool());

  tracker->DestroyNativeWindow(fdp->ConsumeIntegral<int32_t>());

  tracker->CheckNativeWindow(reinterpret_cast<void*>(fdp->ConsumeIntegral<uintptr_t>()));

  uintptr_t ptr_value = fdp->ConsumeIntegral<uintptr_t>();
  tracker->AddNativeWindow(reinterpret_cast<void*>(ptr_value));
  tracker->DestroyNativeWindow(fdp->ConsumeIntegral<int32_t>());
}

void FuzzStressTest(FuzzedDataProvider* fdp) {
  NWebNativeWindowTracker* tracker = NWebNativeWindowTracker::GetInstance();
  if (!tracker) {
    return;
  }

  size_t iterations = fdp->ConsumeIntegralInRange<size_t>(50, 200);

  for (size_t i = 0; i < iterations; ++i) {
    uintptr_t ptr_value = fdp->ConsumeIntegral<uintptr_t>();
    void* native_window = reinterpret_cast<void*>(ptr_value);

    int32_t window_id = tracker->AddNativeWindow(native_window);

    void* retrieved = tracker->GetNativeWindow(window_id, fdp->ConsumeBool());

    bool exists = tracker->CheckNativeWindow(retrieved);

    tracker->DestroyNativeWindow(window_id);

    tracker->GetNativeWindow(window_id, fdp->ConsumeBool());

    (void)exists;
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static Environment env;

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 5);

  switch (selector) {
    case 0:
      FuzzAddNativeWindow(&fdp);
      break;
    case 1:
      FuzzDestroyNativeWindow(&fdp);
      break;
    case 2:
      FuzzCheckNativeWindow(&fdp);
      break;
    case 3:
      FuzzMixedOperations(&fdp);
      break;
    case 4:
      FuzzBoundaryValues(&fdp);
      break;
    case 5:
      FuzzInvalidInputs(&fdp);
      break;
    default:
      FuzzAddNativeWindow(&fdp);
      break;
  }

  return 0;
}
