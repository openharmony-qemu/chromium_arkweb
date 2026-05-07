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

#include "arkweb/chromium_ext/gpu/vulkan/ohos/vulkan_implementation_ohos.h"
#include "base/command_line.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/ipc/common/nweb_native_window_tracker.h"

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

void FuzzInitializeVulkanInstance(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  size_t num_inits = fdp->ConsumeIntegralInRange<size_t>(1, 10);
  for (size_t i = 0; i < num_inits; ++i) {
    bool using_surface = fdp->ConsumeBool();
    bool result = impl->InitializeVulkanInstance(using_surface);
    (void)result;
  }
}

void FuzzGetVulkanInstance(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(true);
  if (!init_result) {
    return;
  }

  size_t num_gets = fdp->ConsumeIntegralInRange<size_t>(1, 20);
  for (size_t i = 0; i < num_gets; ++i) {
    gpu::VulkanInstance* instance = impl->GetVulkanInstance();
    (void)instance;
  }
}

void FuzzCreateViewSurface(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(true);
  if (!init_result) {
    return;
  }

  gfx::AcceleratedWidget window = fdp->ConsumeIntegral<gfx::AcceleratedWidget>();
  auto surface = impl->CreateViewSurface(window);
  (void)surface;
}

void FuzzGetPhysicalDevicePresentationSupport(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(true);
  if (!init_result) {
    return;
  }

  VkPhysicalDevice device = reinterpret_cast<VkPhysicalDevice>(fdp->ConsumeIntegral<uint64_t>());
  uint32_t queue_family_index = fdp->ConsumeIntegral<uint32_t>();
  std::vector<VkQueueFamilyProperties> queue_family_properties;

  size_t num_props = fdp->ConsumeIntegralInRange<size_t>(0, 10);
  queue_family_properties.reserve(num_props);
  for (size_t i = 0; i < num_props; ++i) {
    VkQueueFamilyProperties props = {};
    props.queueFlags = fdp->ConsumeIntegral<uint32_t>();
    props.queueCount = fdp->ConsumeIntegral<uint32_t>();
    props.timestampValidBits = fdp->ConsumeIntegral<uint32_t>();
    impl->GetPhysicalDevicePresentationSupport(
        device, queue_family_properties, queue_family_index);
  }
}

void FuzzGetRequiredDeviceExtensions(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(fdp->ConsumeBool());
  if (!init_result) {
    return;
  }

  size_t num_calls = fdp->ConsumeIntegralInRange<size_t>(1, 10);
  for (size_t i = 0; i < num_calls; ++i) {
    std::vector<const char*> extensions = impl->GetRequiredDeviceExtensions();
    (void)extensions;
  }
}

void FuzzGetOptionalDeviceExtensions(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(fdp->ConsumeBool());
  if (!init_result) {
    return;
  }

  size_t num_calls = fdp->ConsumeIntegralInRange<size_t>(1, 10);
  for (size_t i = 0; i < num_calls; ++i) {
    std::vector<const char*> extensions = impl->GetOptionalDeviceExtensions();
    (void)extensions;
  }
}

void FuzzGetExternalSemaphoreHandleType(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(true);
  if (!init_result) {
    return;
  }

  size_t num_calls = fdp->ConsumeIntegralInRange<size_t>(1, 10);
  for (size_t i = 0; i < num_calls; ++i) {
    VkExternalSemaphoreHandleTypeFlagBits handle_type =
        impl->GetExternalSemaphoreHandleType();
    (void)handle_type;
  }
}

void FuzzCanImportGpuMemoryBuffer(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(true);
  if (!init_result) {
    return;
  }

  gfx::GpuMemoryBufferType memory_buffer_type =
      static_cast<gfx::GpuMemoryBufferType>(
          fdp->ConsumeIntegralInRange<int32_t>(0, 10));

  bool can_import = impl->CanImportGpuMemoryBuffer(nullptr, memory_buffer_type);
  (void)can_import;
}

void FuzzMixedOperations(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(true);
  if (!init_result) {
    return;
  }

  size_t operations = fdp->ConsumeIntegralInRange<size_t>(10, MAX_ITERATIONS);

  for (size_t i = 0; i < operations; ++i) {
    uint8_t op_type = fdp->ConsumeIntegralInRange<uint8_t>(0, 5);

    switch (op_type) {
      case 0: {
        gpu::VulkanInstance* instance = impl->GetVulkanInstance();
        (void)instance;
        break;
      }
      case 1: {
        gfx::AcceleratedWidget window =
            fdp->ConsumeIntegral<gfx::AcceleratedWidget>();
        auto surface = impl->CreateViewSurface(window);
        break;
      }
      case 2: {
        std::vector<const char*> extensions = impl->GetRequiredDeviceExtensions();
        break;
      }
      case 3: {
        std::vector<const char*> extensions = impl->GetOptionalDeviceExtensions();
        break;
      }
      case 4: {
        VkExternalSemaphoreHandleTypeFlagBits handle_type =
            impl->GetExternalSemaphoreHandleType();
        break;
      }
      case 5: {
        gfx::GpuMemoryBufferType memory_buffer_type =
            static_cast<gfx::GpuMemoryBufferType>(
                fdp->ConsumeIntegralInRange<int32_t>(0, 10));
        bool can_import = impl->CanImportGpuMemoryBuffer(
            nullptr, memory_buffer_type);
        break;
      }
    }
  }
}

void FuzzBoundaryValues(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  impl->InitializeVulkanInstance(true);
  impl->InitializeVulkanInstance(false);

  gfx::AcceleratedWidget window_min = 0;
  gfx::AcceleratedWidget window_max = UINTPTR_MAX;

  auto surface1 = impl->CreateViewSurface(window_min);
  auto surface2 = impl->CreateViewSurface(window_max);

  VkPhysicalDevice device = 0;
  uint32_t queue_family_index = 0;
  std::vector<VkQueueFamilyProperties> empty_props;
  bool result = impl->GetPhysicalDevicePresentationSupport(
      device, empty_props, queue_family_index);
  (void)result;

  gfx::GpuMemoryBufferType buffer_type =
      static_cast<gfx::GpuMemoryBufferType>(INT32_MIN);
  bool can_import = impl->CanImportGpuMemoryBuffer(nullptr, buffer_type);
  (void)can_import;
}

void FuzzInvalidInputs(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(true);
  if (!init_result) {
    return;
  }

  VkPhysicalDevice invalid_device = reinterpret_cast<VkPhysicalDevice>(fdp->ConsumeIntegral<uint64_t>());
  uint32_t invalid_queue_index = fdp->ConsumeIntegral<uint32_t>();
  std::vector<VkQueueFamilyProperties> props;

  bool result = impl->GetPhysicalDevicePresentationSupport(
      invalid_device, props, invalid_queue_index);
  (void)result;

  gfx::AcceleratedWidget window =
      fdp->ConsumeIntegral<gfx::AcceleratedWidget>();
  auto surface = impl->CreateViewSurface(window);

  gfx::GpuMemoryBufferType invalid_type =
      static_cast<gfx::GpuMemoryBufferType>(
          fdp->ConsumeIntegral<int32_t>());
  bool can_import = impl->CanImportGpuMemoryBuffer(nullptr, invalid_type);
  (void)can_import;
}

void FuzzStressTest(FuzzedDataProvider* fdp) {
  auto impl = std::make_unique<gpu::VulkanImplementationOhos>();
  if (!impl) {
    return;
  }

  bool init_result = impl->InitializeVulkanInstance(true);
  if (!init_result) {
    return;
  }

  size_t iterations = fdp->ConsumeIntegralInRange<size_t>(50, 200);

  for (size_t i = 0; i < iterations; ++i) {
    gpu::VulkanInstance* instance = impl->GetVulkanInstance();

    std::vector<const char*> required_ext = impl->GetRequiredDeviceExtensions();
    std::vector<const char*> optional_ext = impl->GetOptionalDeviceExtensions();

    VkExternalSemaphoreHandleTypeFlagBits handle_type =
        impl->GetExternalSemaphoreHandleType();

    gfx::AcceleratedWidget window = fdp->ConsumeIntegral<gfx::AcceleratedWidget>();
    auto surface = impl->CreateViewSurface(window);

    gfx::GpuMemoryBufferType buffer_type =
        static_cast<gfx::GpuMemoryBufferType>(
            fdp->ConsumeIntegralInRange<int32_t>(0, 10));
    bool can_import = impl->CanImportGpuMemoryBuffer(nullptr, buffer_type);

    (void)instance;
    (void)handle_type;
    (void)surface;
    (void)can_import;
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static Environment env;

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 10);

  switch (selector) {
    case 0:
      FuzzInitializeVulkanInstance(&fdp);
      break;
    case 1:
      FuzzGetVulkanInstance(&fdp);
      break;
    case 2:
      FuzzCreateViewSurface(&fdp);
      break;
    case 3:
      FuzzGetPhysicalDevicePresentationSupport(&fdp);
      break;
    case 4:
      FuzzGetRequiredDeviceExtensions(&fdp);
      break;
    case 5:
      FuzzGetOptionalDeviceExtensions(&fdp);
      break;
    case 6:
      FuzzGetExternalSemaphoreHandleType(&fdp);
      break;
    case 7:
      FuzzCanImportGpuMemoryBuffer(&fdp);
      break;
    case 8:
      FuzzMixedOperations(&fdp);
      break;
    case 9:
      FuzzBoundaryValues(&fdp);
      break;
    case 10:
      FuzzInvalidInputs(&fdp);
      break;
    default:
      FuzzInitializeVulkanInstance(&fdp);
      break;
  }

  return 0;
}
