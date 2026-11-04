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

#include "gpu/command_buffer/service/shared_image/ohos_native_buffer_image_backing_factory.h"

#include "base/command_line.h"
#include "base/task/single_thread_task_executor.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_test_support.h"
#include <fuzzer/FuzzedDataProvider.h>

namespace gpu {

constexpr SharedImageUsageSet kFuzzerSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE | SHARED_IMAGE_USAGE_CPU_UPLOAD |
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE;

class FuzzEnvironment {
public:
  scoped_refptr<gl::GLSurface> surf_;
  scoped_refptr<gl::GLContext> gl_context_;
  scoped_refptr<gles2::FeatureInfo> feature_info_;
  std::unique_ptr<OHOSNativeBufferImageBackingFactory> factory_;
  bool initialized_ = false;
  std::unique_ptr<base::SingleThreadTaskExecutor> task_executor_;

  bool Initialize() {
    task_executor_ = std::make_unique<base::SingleThreadTaskExecutor>();
    if (initialized_)
      return true;
    if (!gl::init::InitializeGLNoExtensionsOneOff(
            true, gl::GpuPreference::kDefault))
      return false;
    auto display = gl::GLTestSupport::InitializeGL(std::nullopt);
    if (!display)
      return false;
    surf_ = gl::init::CreateOffscreenGLSurface(display, gfx::Size());
    if (!surf_)
      return false;
    gl_context_ =
        gl::init::CreateGLContext(nullptr, surf_.get(), gl::GLContextAttribs());
    if (!gl_context_)
      return false;
    if (!gl_context_->MakeCurrent(surf_.get()))
      return false;

    GpuDriverBugWorkarounds workarounds;
    GpuFeatureInfo feature_info;
    feature_info_ = base::MakeRefCounted<gles2::FeatureInfo>(workarounds,
                                                              feature_info);
    feature_info_->Initialize(CONTEXT_TYPE_OPENGLES2, true,
                              gles2::DisallowedFeatures());

    GpuPreferences prefs;
    factory_ = std::make_unique<OHOSNativeBufferImageBackingFactory>(
        feature_info_.get(), prefs);
    initialized_ = true;
    return true;
  }
};

FuzzEnvironment* g_env = nullptr;

viz::SharedImageFormat GetFormat(uint8_t idx) {
  switch (idx % 7) {
    case 0:
      return viz::SinglePlaneFormat::kRGBA_8888;
    case 1:
      return viz::SinglePlaneFormat::kBGR_565;
    case 2:
      return viz::SinglePlaneFormat::kRGBA_F16;
    case 3:
      return viz::SinglePlaneFormat::kRGBX_8888;
    case 4:
      return viz::SinglePlaneFormat::kRGBA_1010102;
    case 5:
      return viz::SinglePlaneFormat::kALPHA_8;
    case 6:
      return viz::MultiPlaneFormat::kYV12;
  }
  return viz::SinglePlaneFormat::kRGBA_8888;
}

gfx::GpuMemoryBufferType GetGmbType(uint8_t idx) {
  switch (idx % 4) {
    case 0:
      return gfx::EMPTY_BUFFER;
    case 1:
      return gfx::SHARED_MEMORY_BUFFER;
    case 2:
      return gfx::NATIVE_PIXMAP;
    case 3:
      return gfx::OHOS_NATIVE_BUFFER;
  }
  return gfx::EMPTY_BUFFER;
}

void FuzzIsSupported(FuzzedDataProvider& provider) {
  uint8_t format_idx = provider.ConsumeIntegral<uint8_t>();
  viz::SharedImageFormat format = GetFormat(format_idx);
  gfx::Size size(provider.ConsumeIntegralInRange<int>(0, 8192),
                provider.ConsumeIntegralInRange<int>(0, 8192));
  bool thread_safe = provider.ConsumeBool();
  uint8_t gmb_idx = provider.ConsumeIntegral<uint8_t>();
  gfx::GpuMemoryBufferType gmb_type = GetGmbType(gmb_idx);
  GrContextType gr_context = GrContextType::kGL;
  size_t pixel_size = provider.ConsumeIntegralInRange<size_t>(0, 256);
  std::vector<uint8_t> pixel_data = provider.ConsumeBytes<uint8_t>(pixel_size);

  g_env->factory_->IsSupported(kFuzzerSupportedUsage, format, size,
                              thread_safe, gmb_type, gr_context,
                              base::span<const uint8_t>(pixel_data));
}

void FuzzMakeBacking(FuzzedDataProvider& provider) {
  auto mailbox = Mailbox::Generate();
  uint8_t format_idx = provider.ConsumeIntegral<uint8_t>();
  viz::SharedImageFormat format = GetFormat(format_idx);
  gfx::Size size(provider.ConsumeIntegralInRange<int>(0, 512),
                provider.ConsumeIntegralInRange<int>(0, 512));
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  std::string debug_label = provider.ConsumeRandomLengthString(32);
  bool is_thread_safe = provider.ConsumeBool();

  uint8_t pixel_scenario = provider.ConsumeIntegralInRange<uint8_t>(0, 3);
  std::vector<uint8_t> pixel_data_vec;
  base::span<const uint8_t> pixel_data;

  switch (pixel_scenario) {
    case 0:
      break;
    case 1: {
      if (size.width() > 0 && size.height() > 0) {
        size_t total_size =
            static_cast<size_t>(4) * size.width() * size.height();
        if (total_size <= 1024 * 1024) {
          pixel_data_vec.resize(total_size);
          size_t to_consume =
              std::min(pixel_data_vec.size(), provider.remaining_bytes());
          provider.ConsumeData(pixel_data_vec.data(), to_consume);
          pixel_data = base::span<const uint8_t>(pixel_data_vec);
        }
      }
      break;
    }
    case 2: {
      pixel_data_vec = provider.ConsumeBytes<uint8_t>(10);
      pixel_data = base::span<const uint8_t>(pixel_data_vec);
      break;
    }
    case 3: {
      pixel_data_vec = provider.ConsumeBytes<uint8_t>(100);
      pixel_data = base::span<const uint8_t>(pixel_data_vec);
      break;
    }
  }

  auto backing = g_env->factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type,
      kFuzzerSupportedUsage, debug_label, is_thread_safe, pixel_data);

  if (backing) {
    (void)backing->GetType();
  }
}

void FuzzCreateSharedImageWithHandle(FuzzedDataProvider& provider) {
  auto mailbox = Mailbox::Generate();
  uint8_t format_idx = provider.ConsumeIntegral<uint8_t>();
  viz::SharedImageFormat format = GetFormat(format_idx);
  gfx::Size size(provider.ConsumeIntegralInRange<int>(1, 256),
                provider.ConsumeIntegralInRange<int>(1, 256));
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  std::string debug_label = provider.ConsumeRandomLengthString(32);

  gfx::GpuMemoryBufferHandle handle;
  uint8_t handle_type = provider.ConsumeIntegralInRange<uint8_t>(0, 1);
  if (handle_type == 0) {
    handle.type = gfx::OHOS_NATIVE_BUFFER;
  } else {
    handle.type = gfx::EMPTY_BUFFER;
  }
  bool is_thread_safe = provider.ConsumeBool();

  g_env->factory_->CreateSharedImage(mailbox, format, size, color_space,
                                      surface_origin, alpha_type,
                                      kFuzzerSupportedUsage, debug_label, is_thread_safe,
                                      std::move(handle));
}

void FuzzCreateSharedImageNoPixels(FuzzedDataProvider& provider) {
  auto mailbox = Mailbox::Generate();
  uint8_t format_idx = provider.ConsumeIntegralInRange<uint8_t>(0, 5);
  viz::SharedImageFormat format = GetFormat(format_idx);
  gfx::Size size(provider.ConsumeIntegralInRange<int>(1, 256),
                provider.ConsumeIntegralInRange<int>(1, 256));
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  std::string debug_label = provider.ConsumeRandomLengthString(32);
  bool is_thread_safe = provider.ConsumeBool();

  auto backing = g_env->factory_->CreateSharedImage(
      mailbox, format, gpu::kNullSurfaceHandle, size, color_space,
      surface_origin, alpha_type, kFuzzerSupportedUsage, debug_label,
      is_thread_safe);
  if (backing) {
    (void)backing->GetType();
  }
}

void FuzzGetBackingType(FuzzedDataProvider& provider) {
  g_env->factory_->GetBackingType();
}

void FuzzFormatInfo(FuzzedDataProvider& provider) {
  // OHOSNativeBufferImageBackingFactory::FormatInfo info;
  // (void)info.gl_supported;
}

void FuzzRecreateFactory(FuzzedDataProvider& provider) {
  g_env->factory_.reset();
  GpuDriverBugWorkarounds workarounds;
  GpuFeatureInfo feature_info;
  GpuPreferences prefs;

  g_env->factory_ = std::make_unique<OHOSNativeBufferImageBackingFactory>(
    g_env->feature_info_.get(), prefs);
}

void FuzzCreateSharedImageWithHandleValid(FuzzedDataProvider& provider) {
  auto mailbox = Mailbox::Generate();
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(provider.ConsumeIntegralInRange<int>(1, 256),
                provider.ConsumeIntegralInRange<int>(1, 256));
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  std::string debug_label = provider.ConsumeRandomLengthString(32);

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OHOS_NATIVE_BUFFER;

  bool is_thread_safe = provider.ConsumeBool();

  g_env->factory_->CreateSharedImage(mailbox, format, size, color_space,
                                      surface_origin, alpha_type,
                                      kFuzzerSupportedUsage, debug_label, is_thread_safe,
                                      std::move(handle));
}

void FuzzMakeBackingInValid(FuzzedDataProvider& provider) {
  auto mailbox = Mailbox::Generate();
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size;
  uint8_t invalid_type = provider.ConsumeIntegralInRange<uint8_t>(0, 2);
  switch (invalid_type) {
    case 0:
      size = gfx::Size(0, 0);
      break;
    case 1:
      size = gfx::Size(99999, 99999);
      break;
    case 2:
      format = viz::MultiPlaneFormat::kYV12;
      size = gfx::Size(64, 64);
      break;
  }
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  std::string debug_label = provider.ConsumeRandomLengthString(32);

  // gfx::GpuMemoryBufferHandle handle;
  // handle.type = gfx::OHOS_NATIVE_BUFFER;

  // bool is_thread_safe = provider.ConsumeBool();

  auto image = g_env->factory_->CreateSharedImage(mailbox, format, size, color_space,
                                      surface_origin, alpha_type,
                                      kFuzzerSupportedUsage, debug_label, false,
                                      base::span<const uint8_t>());
}

void FuzzFactory(FuzzedDataProvider& provider) {
  uint8_t scenario = provider.ConsumeIntegralInRange<uint8_t>(0, 9);
  switch (scenario) {
    case 0:
      FuzzIsSupported(provider);
      break;
    case 1:
      FuzzMakeBacking(provider);
      break;
    case 2:
      FuzzCreateSharedImageWithHandle(provider);
      break;
    case 3:
      FuzzCreateSharedImageNoPixels(provider);
      break;
    case 4:
      FuzzGetBackingType(provider);
      break;
    case 5:
      FuzzFormatInfo(provider);
      break;
    case 6:
      FuzzMakeBackingInValid(provider);
      break;
    case 7:
      FuzzCreateSharedImageWithHandleValid(provider);
      break;
    case 8:
    case 9:
      FuzzRecreateFactory(provider);
      break;
  }
}

}  // namespace gpu

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  base::CommandLine::Init(*argc, *argv);
  gpu::g_env = new gpu::FuzzEnvironment();
  if (!gpu::g_env->Initialize()) {
    delete gpu::g_env;
    gpu::g_env = nullptr;
  }
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!data || size == 0 || size > 4096 || !gpu::g_env)
    return 0;
  FuzzedDataProvider provider(data, size);
  gpu::FuzzFactory(provider);
  return 0;
}
