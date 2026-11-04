/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "base/functional/bind.h"
#include "base/pending_task.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include <fuzzer/FuzzedDataProvider.h>
#include "base/test/bind.h"

#define private public
#include "arkweb/chromium_ext/gpu/ipc/service/gpu_hang_adapter.h"
#undef private

namespace gpu {

constexpr size_t kMaxInputSize = 4096;

base::Thread* g_thread = nullptr;
GpuHangAdapter* g_adapter_main = nullptr;
GpuHangAdapter* g_adapter_compositor = nullptr;

void InitEnvironment() {
  g_thread = new base::Thread("FuzzHangThread");
  CHECK(g_thread->Start());

  base::WaitableEvent event;
  g_thread->task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&event]() {
        g_adapter_main =
            GpuHangAdapter::CreateGpuHangAdapterForGpuMain().release();
        g_adapter_compositor =
            GpuHangAdapter::CreateGpuHangAdapterForCompositorGpu().release();
        g_adapter_main->checkHangWatch_ = true;
        g_adapter_main->hang_test_ = true;
        g_adapter_compositor->checkHangWatch_ = true;
        g_adapter_compositor->hang_test_ = true;
        event.Signal();
      }));
  event.Wait();
}

void FuzzAdapterMethods(GpuHangAdapter* adapter,
                        const uint8_t* data,
                        size_t size) {
  FuzzedDataProvider provider(data, size);
  int32_t iterations = provider.ConsumeIntegralInRange<int32_t>(1, 50);

  for (int32_t i = 0; i < iterations; ++i) {
    base::PendingTask dummy_task;
    adapter->WillProcessTask(dummy_task, false);
    adapter->DidProcessTask(dummy_task);
  }
}

}  // namespace gpu

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  gpu::InitEnvironment();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!data || size == 0)
    return 0;
  if (size > gpu::kMaxInputSize)
    return 0;

  FuzzedDataProvider provider(data, size);
  uint8_t scenario = provider.ConsumeIntegralInRange<uint8_t>(0, 7);

  auto remaining = provider.ConsumeRemainingBytes<uint8_t>();
  const uint8_t* rem_data = remaining.data();
  size_t rem_size = remaining.size();

  switch (scenario) {
    case 0:
      gpu::FuzzAdapterMethods(gpu::g_adapter_main, rem_data, rem_size);
      break;
    case 1:
      gpu::FuzzAdapterMethods(gpu::g_adapter_compositor, rem_data, rem_size);
      break;
    case 2: {
      // Enable hang_test path without triggering sleep.
      // WillProcessTask uses (test_count_++ % 1000 == 0); starting at 1
      // avoids sleep on the first call since 1 % 1000 != 0.
      // gpu::g_adapter_main->test_count_ = 1;
      // gpu::g_adapter_main->hang_test_ = true;
      gpu::FuzzAdapterMethods(gpu::g_adapter_main, rem_data, rem_size);
      // gpu::g_adapter_main->hang_test_ = false;
      break;
    }
    case 3: {
      // Create and destroy adapter on the worker thread to cover destructor.
      base::WaitableEvent event;
      gpu::g_thread->task_runner()->PostTask(
          FROM_HERE, base::BindLambdaForTesting([&event]() {
            auto adapter =
                gpu::GpuHangAdapter::CreateGpuHangAdapterForCompositorGpu();
            adapter->checkHangWatch_ = true;
            adapter->hang_test_ = true;
            base::PendingTask dummy_task;
            adapter->WillProcessTask(dummy_task, false);
            adapter->DidProcessTask(dummy_task);
            // adapter goes out of scope here, covering ~GpuHangAdapter().
            event.Signal();
          }));
      event.Wait();
      break;
    }
    case 4: {
      int32_t test_count = 999;
      gpu::SimulateHangForTesting(test_count, true, "gpu_main");
      break;
    }
    case 5: {
      gpu::LogGpuHungEvent(0, 1);
      break;
    }
    case 6: {
      gpu::ReportGpuFreeze();
      break;
    }
    case 7: {
      // Free functions
      std::string process_name = gpu::GetProcessName();
      (void)process_name;
      bool enabled = gpu::IsHangTestEnabled("gpu_main");
      (void)enabled;
      enabled = gpu::IsHangTestEnabled("compositor_gpu");
      (void)enabled;

      // int32_t test_count = 1;
      // gpu::SimulateHangForTesting(test_count, false, "gpu_main");
      break;
    }
    default:
      break;
  }

  return 0;
}

