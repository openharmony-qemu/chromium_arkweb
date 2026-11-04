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

#include "arkweb/ohos_adapter_ndk/ohos_native_buffer_adapter/ohos_native_buffer_adapter_impl.h"
#include "base/ohos/scoped_native_buffer_handle.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_test_support.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

#include <climits>
#include <cstdint>
#include <memory>

#define private public
#define protected public
#include "gpu/command_buffer/service/shared_image/ohos_native_buffer_image_backing.h"
#undef private
#undef protected

namespace gpu {

constexpr int32_t MAX_WIDTH = 256;
constexpr int32_t MAX_HEIGHT = 256;
constexpr size_t MAX_ESTIMATED_SIZE = 100000000;
constexpr uint32_t MAX_USAGE_FLAGS = 0xFFFFFFFF;

ScopedNativeBufferHandle CreateNativeBufferHandle(int32_t width, int32_t height, int format_type) {
    void* buffer = nullptr;
    OH_NativeBuffer_Config config;

    config.width = width;
    config.height = height;

    OH_NativeBuffer_Format buffer_format;
    switch (format_type) {
      case 0:
        buffer_format = OH_NativeBuffer_Format::NATIVEBUFFER_PIXEL_FMT_RGBA_8888;
        break;
      case 1:
        buffer_format = OH_NativeBuffer_Format::NATIVEBUFFER_PIXEL_FMT_RGBX_8888;
        break;
      case 2:
        buffer_format = OH_NativeBuffer_Format::NATIVEBUFFER_PIXEL_FMT_BGRA_8888;
        break;
      default:
        buffer_format = OH_NativeBuffer_Format::NATIVEBUFFER_PIXEL_FMT_RGBA_8888;
        break;
    }

    config.format = buffer_format;
    config.usage = 1;
    config.stride = 1;

    buffer = OH_NativeBuffer_Alloc(&config);
    if (buffer == nullptr) {
        return gpu::ScopedNativeBufferHandle();
    }

    return gpu::ScopedNativeBufferHandle::Adopt(buffer);
}

class OhosNativeBufferImageBackingFuzzer {
 public:
  OhosNativeBufferImageBackingFuzzer() : task_environment_() {
    base::CommandLine::Init(0, nullptr);

    gl::init::InitializeGLNoExtensionsOneOff(
        true, gl::GpuPreference::kDefault);

    auto display = gl::GLTestSupport::InitializeGL(std::nullopt);
    if (display == nullptr) {
      return;
    }

    surf_ = gl::init::CreateOffscreenGLSurface(display, gfx::Size());
    if (surf_ == nullptr) {
      return;
    }

    gl_context_ = gl::init::CreateGLContext(nullptr, surf_.get(), gl::GLContextAttribs());
    if (gl_context_ == nullptr) {
      return;
    }

    gl_context_->MakeCurrent(surf_.get());

    tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
  }

  ~OhosNativeBufferImageBackingFuzzer() {
    if (gl_context_ && surf_) {
      gl_context_->ReleaseCurrent(surf_.get());
    }
  }

  void FuzzConstructorBasic(FuzzedDataProvider* fdp) {
    if (!gl_context_ || !surf_) {
      return;
    }

    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
    gfx::Size size(width, height);

    viz::SharedImageFormat format;
    int format_type = fdp->ConsumeIntegralInRange<int>(0, 2);
    switch (format_type) {
      case 0:
        format = viz::SinglePlaneFormat::kRGBA_8888;
        break;
      case 1:
        format = viz::SinglePlaneFormat::kRGBX_8888;
        break;
      default:
        format = viz::SinglePlaneFormat::kBGRA_8888;
        break;
    }

    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;

    SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_GLES2_READ,   SHARED_IMAGE_USAGE_GLES2_WRITE,
      SHARED_IMAGE_USAGE_RASTER_READ,  SHARED_IMAGE_USAGE_RASTER_WRITE,
      SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT};
    size_t label_size = fdp->ConsumeIntegralInRange<size_t>(0, 50);
    std::string debug_label = fdp->ConsumeBytesAsString(label_size);

    size_t estimated_size = fdp->ConsumeIntegralInRange<size_t>(0, MAX_ESTIMATED_SIZE);
    bool is_thread_safe = fdp->ConsumeBool();

    base::ScopedFD initial_upload_fd;
    GLFormatCaps gl_format_caps;
    Mailbox mailbox = Mailbox::Generate();

    auto backing = std::make_unique<OhosNativeBufferImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        std::move(debug_label), estimated_size, is_thread_safe,
        std::move(initial_upload_fd), gl_format_caps);

    if (backing) {
      (void)backing->GetType();
      (void)backing->ClearedRect();
      backing->Update(nullptr);
    }
  }

  void FuzzConstructorWithHandle(FuzzedDataProvider* fdp) {
    if (!gl_context_ || !surf_) {
      return;
    }

    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
    int format_type = fdp->ConsumeIntegralInRange<int>(0, 2);

    auto handle = CreateNativeBufferHandle(width, height, format_type);
    if (!handle.is_valid()) {
      return;
    }

    gfx::Size size(width, height);
    viz::SharedImageFormat format;
    switch (format_type) {
      case 0:
        format = viz::SinglePlaneFormat::kRGBA_8888;
        break;
      case 1:
        format = viz::SinglePlaneFormat::kRGBX_8888;
        break;
      default:
        format = viz::SinglePlaneFormat::kBGRA_8888;
        break;
    }

    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;

    SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE,
      SHARED_IMAGE_USAGE_RASTER_READ, SHARED_IMAGE_USAGE_RASTER_WRITE,
      SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT
    };
    std::string debug_label = "test";

    size_t estimated_size = fdp->ConsumeIntegralInRange<size_t>(0, MAX_ESTIMATED_SIZE);
    bool is_thread_safe = fdp->ConsumeBool();
    bool use_passthrough = fdp->ConsumeBool();

    base::ScopedFD initial_upload_fd;
    GLFormatCaps gl_format_caps;
    Mailbox mailbox = Mailbox::Generate();

    auto backing = std::make_unique<OhosNativeBufferImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(handle), estimated_size, is_thread_safe,
      std::move(initial_upload_fd), use_passthrough, gl_format_caps);

    if (backing) {
      (void)backing->GetType();
      (void)backing->ClearedRect();
    }
  }

  void FuzzProduceGLTexture(FuzzedDataProvider* fdp) {
    if (!gl_context_ || !surf_ || !tracker_) {
      return;
    }

    Mailbox mailbox = Mailbox::Generate();
    viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
    gfx::Size size(10, 10);
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE,
      SHARED_IMAGE_USAGE_RASTER_READ, SHARED_IMAGE_USAGE_RASTER_WRITE,
      SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT
    };
    std::string debug_label = "";
    size_t estimated_size = 0;
    bool is_thread_safe = false;
    base::ScopedFD initial_upload_fd;
    GLFormatCaps gl_format_caps;

    auto backing_without_handle = std::make_unique<OhosNativeBufferImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        std::move(debug_label), estimated_size, is_thread_safe,
        std::move(initial_upload_fd), gl_format_caps);

    if (backing_without_handle) {
      auto representation = backing_without_handle->ProduceGLTexture(&manager_, tracker_.get());
      (void)representation;
    }

    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
    int format_type = fdp->ConsumeIntegralInRange<int>(0, 2);

    auto handle = CreateNativeBufferHandle(width, height, format_type);
    if (!handle.is_valid()) {
      return;
    }

    viz::SharedImageFormat handle_format;
    switch (format_type) {
      case 0:
        handle_format = viz::SinglePlaneFormat::kRGBA_8888;
        break;
      case 1:
        handle_format = viz::SinglePlaneFormat::kRGBX_8888;
        break;
      default:
        handle_format = viz::SinglePlaneFormat::kBGRA_8888;
        break;
    }

    gfx::Size handle_size(width, height);
    bool use_passthrough = fdp->ConsumeBool();

    auto backing_with_handle = std::make_unique<OhosNativeBufferImageBacking>(
      mailbox, handle_format, handle_size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(handle), estimated_size, is_thread_safe,
      std::move(initial_upload_fd), use_passthrough, gl_format_caps);

    if (backing_with_handle) {
      auto representation = backing_with_handle->ProduceGLTexture(&manager_, tracker_.get());
      (void)representation;
    }
  }

  void FuzzProduceGLTexturePassthrough(FuzzedDataProvider* fdp) {
    if (!gl_context_ || !surf_ || !tracker_) {
      return;
    }

    Mailbox mailbox = Mailbox::Generate();
    viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
    gfx::Size size(10, 10);
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE,
      SHARED_IMAGE_USAGE_RASTER_READ, SHARED_IMAGE_USAGE_RASTER_WRITE,
      SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT
    };
    std::string debug_label = "";
    size_t estimated_size = 0;
    bool is_thread_safe = false;
    base::ScopedFD initial_upload_fd;
    GLFormatCaps gl_format_caps;

    auto backing_without_handle = std::make_unique<OhosNativeBufferImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), estimated_size, is_thread_safe,
      std::move(initial_upload_fd), gl_format_caps);

    if (backing_without_handle) {
      auto representation = backing_without_handle->ProduceGLTexturePassthrough(&manager_, tracker_.get());
      (void)representation;
    }

    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
    int format_type = fdp->ConsumeIntegralInRange<int>(0, 2);

    auto handle = CreateNativeBufferHandle(width, height, format_type);
    if (!handle.is_valid()) {
      return;
    }

    viz::SharedImageFormat handle_format;
    switch (format_type) {
      case 0:
        handle_format = viz::SinglePlaneFormat::kRGBA_8888;
        break;
      case 1:
        handle_format = viz::SinglePlaneFormat::kRGBX_8888;
        break;
      default:
        handle_format = viz::SinglePlaneFormat::kBGRA_8888;
        break;
    }

    gfx::Size handle_size(width, height);
    bool use_passthrough = fdp->ConsumeBool();

    auto backing_with_handle = std::make_unique<OhosNativeBufferImageBacking>(
      mailbox, handle_format, handle_size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(handle), estimated_size, is_thread_safe,
      std::move(initial_upload_fd), use_passthrough, gl_format_caps);

    if (backing_with_handle) {
      auto representation = backing_with_handle->ProduceGLTexturePassthrough(&manager_, tracker_.get());
      (void)representation;
    }
  }

  void FuzzClearedRectOperations(FuzzedDataProvider* fdp) {
    if (!gl_context_ || !surf_) {
      return;
    }

    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
    int format_type = fdp->ConsumeIntegralInRange<int>(0, 2);

    auto handle = CreateNativeBufferHandle(width, height, format_type);
    if (!handle.is_valid()) {
      return;
    }

    gfx::Size size(width, height);
    viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE,
      SHARED_IMAGE_USAGE_RASTER_READ, SHARED_IMAGE_USAGE_RASTER_WRITE,
      SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT
    };
    std::string debug_label = "";
    size_t estimated_size = 0;
    bool is_thread_safe = fdp->ConsumeBool();
    bool use_passthrough = fdp->ConsumeBool();
    base::ScopedFD initial_upload_fd;
    GLFormatCaps gl_format_caps;
    Mailbox mailbox = Mailbox::Generate();

    auto backing = std::make_unique<OhosNativeBufferImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(handle), estimated_size, is_thread_safe,
      std::move(initial_upload_fd), use_passthrough, gl_format_caps);

    if (!backing) {
      return;
    }

    gfx::Rect cleared_rect = backing->ClearedRect();
    (void)cleared_rect;

    int32_t rect_x = fdp->ConsumeIntegralInRange<int32_t>(0, width);
    int32_t rect_y = fdp->ConsumeIntegralInRange<int32_t>(0, height);
    int32_t rect_width = fdp->ConsumeIntegralInRange<int32_t>(1, width);
    int32_t rect_height = fdp->ConsumeIntegralInRange<int32_t>(1, height);
    gfx::Rect rect(rect_x, rect_y, rect_width, rect_height);

    backing->SetClearedRect(rect);

    cleared_rect = backing->ClearedRect();
    (void)cleared_rect;
  }

  void FuzzGetNativeBufferHandle(FuzzedDataProvider* fdp) {
    if (!gl_context_ || !surf_) {
      return;
    }

    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
    int format_type = fdp->ConsumeIntegralInRange<int>(0, 2);

    auto handle = CreateNativeBufferHandle(width, height, format_type);
    if (!handle.is_valid()) {
      return;
    }

    gfx::Size size(width, height);
    viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE,
      SHARED_IMAGE_USAGE_RASTER_READ, SHARED_IMAGE_USAGE_RASTER_WRITE,
      SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT
    };
    std::string debug_label = "";
    size_t estimated_size = 0;
    bool is_thread_safe = false;
    bool use_passthrough = fdp->ConsumeBool();
    base::ScopedFD initial_upload_fd;
    GLFormatCaps gl_format_caps;
    Mailbox mailbox = Mailbox::Generate();

    auto backing = std::make_unique<OhosNativeBufferImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(handle), estimated_size, is_thread_safe,
      std::move(initial_upload_fd), use_passthrough, gl_format_caps);

    if (backing) {
      gpu::ScopedNativeBufferHandle result_handle = backing->GetNativeBufferHandle();
      (void)result_handle;
    }
  }

  void FuzzUpdate(FuzzedDataProvider* fdp) {
    if (!gl_context_ || !surf_) {
      return;
    }

    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
    int format_type = fdp->ConsumeIntegralInRange<int>(0, 2);

    auto handle = CreateNativeBufferHandle(width, height, format_type);
    if (!handle.is_valid()) {
      return;
    }

    gfx::Size size(width, height);
    viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE,
      SHARED_IMAGE_USAGE_RASTER_READ, SHARED_IMAGE_USAGE_RASTER_WRITE,
      SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT
    };
    std::string debug_label = "";
    size_t estimated_size = 0;
    bool is_thread_safe = fdp->ConsumeBool();
    bool use_passthrough = fdp->ConsumeBool();
    base::ScopedFD initial_upload_fd;
    GLFormatCaps gl_format_caps;
    Mailbox mailbox = Mailbox::Generate();

    auto backing = std::make_unique<OhosNativeBufferImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(handle), estimated_size, is_thread_safe,
      std::move(initial_upload_fd), use_passthrough, gl_format_caps);

    if (backing) {
      backing->Update(nullptr);
    }
  }

  void FuzzMixedOperations(FuzzedDataProvider* fdp) {
    if (!gl_context_ || !surf_ || !tracker_) {
      return;
    }

    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(3, 10);

    for (size_t i = 0; i < iterations; ++i) {
      int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
      int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
      int format_type = fdp->ConsumeIntegralInRange<int>(0, 2);

      auto handle = CreateNativeBufferHandle(width, height, format_type);
      if (!handle.is_valid()) {
        continue;
      }

      gfx::Size size(width, height);
      viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
      gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
      GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
      SkAlphaType alpha_type = kPremul_SkAlphaType;
      SharedImageUsageSet usage = {
        SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE,
        SHARED_IMAGE_USAGE_RASTER_READ, SHARED_IMAGE_USAGE_RASTER_WRITE,
        SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT
      };
      std::string debug_label = "";
      size_t estimated_size = 0;
      bool is_thread_safe = fdp->ConsumeBool();
      bool use_passthrough = fdp->ConsumeBool();
      base::ScopedFD initial_upload_fd;
      GLFormatCaps gl_format_caps;
      Mailbox mailbox = Mailbox::Generate();

      auto backing = std::make_unique<OhosNativeBufferImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        std::move(debug_label), std::move(handle), estimated_size, is_thread_safe,
        std::move(initial_upload_fd), use_passthrough, gl_format_caps);

      if (!backing) {
        continue;
      }

      int operation_type = fdp->ConsumeIntegralInRange<int>(0, 5);

      switch (operation_type) {
        case 0:
          {
            auto representation = backing->ProduceGLTexture(&manager_, tracker_.get());
            (void)representation;
          }
          break;
        case 1:
          {
            auto representation = backing->ProduceGLTexturePassthrough(&manager_, tracker_.get());
            (void)representation;
          }
          break;
        case 2:
          backing->Update(nullptr);
          break;
        case 3:
          {
            gfx::Rect cleared_rect = backing->ClearedRect();
            backing->SetClearedRect(cleared_rect);
          }
          break;
        case 4:
          {
            gpu::ScopedNativeBufferHandle result_handle = backing->GetNativeBufferHandle();
            (void)result_handle;
          }
          break;
        case 5:
          {
            int32_t rect_x = fdp->ConsumeIntegralInRange<int32_t>(0, width);
            int32_t rect_y = fdp->ConsumeIntegralInRange<int32_t>(0, height);
            gfx::Rect rect(rect_x, rect_y, width, height);
            backing->SetClearedRect(rect);
          }
          break;
      }
    }
  }

  void FuzzDestructor(FuzzedDataProvider* fdp) {
    if (!gl_context_ || !surf_) {
      return;
    }

    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
    int format_type = fdp->ConsumeIntegralInRange<int>(0, 2);

    auto handle = CreateNativeBufferHandle(width, height, format_type);
    if (!handle.is_valid()) {
      return;
    }

    gfx::Size size(width, height);
    viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE,
      SHARED_IMAGE_USAGE_RASTER_READ, SHARED_IMAGE_USAGE_RASTER_WRITE,
      SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT
    };
    std::string debug_label = "";
    size_t estimated_size = 0;
    bool is_thread_safe = fdp->ConsumeBool();
    bool use_passthrough = fdp->ConsumeBool();
    base::ScopedFD initial_upload_fd;
    GLFormatCaps gl_format_caps;
    Mailbox mailbox = Mailbox::Generate();

    auto backing = std::make_unique<OhosNativeBufferImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(handle), estimated_size, is_thread_safe,
      std::move(initial_upload_fd), use_passthrough, gl_format_caps);

    backing.reset();
  }

  void RunFuzzTest(FuzzedDataProvider* fdp) {
    int32_t selector = fdp->ConsumeIntegralInRange<int32_t>(0, 7);

    switch (selector) {
      case 0:
        FuzzConstructorBasic(fdp);
        break;
      case 1:
        FuzzConstructorWithHandle(fdp);
        break;
      case 2:
        FuzzProduceGLTexture(fdp);
        break;
      case 3:
        FuzzProduceGLTexturePassthrough(fdp);
        break;
      case 4:
        FuzzClearedRectOperations(fdp);
        break;
      case 5:
        FuzzGetNativeBufferHandle(fdp);
        break;
      case 6:
        FuzzUpdate(fdp);
        break;
      case 7:
        FuzzMixedOperations(fdp);
        break;
      default:
        FuzzMixedOperations(fdp);
        break;
    }
  }

 private:
  base::test::TaskEnvironment task_environment_{};
  scoped_refptr<gl::GLSurface> surf_;
  scoped_refptr<gl::GLContext> gl_context_;
  std::unique_ptr<MemoryTypeTracker> tracker_;
  SharedImageManager manager_;
};

}  // namespace gpu

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static bool initialized = []() {
    base::CommandLine::Init(0, nullptr);
    return true;
  }();

  static gpu::OhosNativeBufferImageBackingFuzzer* fuzzer = nullptr;

  if (fuzzer == nullptr) {
    fuzzer = new gpu::OhosNativeBufferImageBackingFuzzer();
  }

  FuzzedDataProvider fdp(data, size);

  fuzzer->RunFuzzTest(&fdp);

  return 0;
}
