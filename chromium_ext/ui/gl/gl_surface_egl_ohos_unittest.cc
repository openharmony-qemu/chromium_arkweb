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

#include <cstdint>
#include <memory>

#include "ui/gfx/extension_set.h"
#define private public
#define protected public
#include "ui/gl/gl_implementation.h"
#include "arkweb/chromium_ext/ui/gl/gl_surface_egl_ohos.cc"
#undef private
#undef protected

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_display_egl_util.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/gl/test/gl_test_support.h"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api.h"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_codec_decoder_adapter.h"

namespace gl {
namespace {

using namespace MockNdkApi;

constexpr int32_t kMockAcceleratedWidget = 12345;
constexpr int32_t kMockWidgetForEnhance = 4;

void DummyPresentationCallback(const gfx::PresentationFeedback& feedback) {}

class GlSurfaceEglOhosTest : public testing::Test {
 public:
  void SetUp() override {
    gl::init::InitializeGLNoExtensionsOneOff(
        true, gl::GpuPreference::kDefault);
    display_ = gl::GLTestSupport::InitializeGL(std::nullopt);
    ASSERT_TRUE(display_ != nullptr);
    surf_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    ASSERT_TRUE(surf_ != nullptr);
    gl_context_ = gl::init::CreateGLContext(nullptr, surf_.get(),
                                            gl::GLContextAttribs());
    ASSERT_TRUE(gl_context_ != nullptr);
    ASSERT_TRUE(gl_context_->MakeCurrent(surf_.get()));
  }

  void TearDown() override {
    if (gl_context_ && surf_) {
      gl_context_->ReleaseCurrent(surf_.get());
    }
    gl_context_ = nullptr;
    surf_ = nullptr;
    if (display_) {
      gl::GLSurfaceTestSupport::ShutdownGL(display_);
    }
  }

 protected:
  bool SetUpSurfaceForResize(NativeViewGLSurfaceEGLOhos* surface) {
    EGLDisplay egl_display = display_->GetAs<GLDisplayEGL>()->GetDisplay();
    auto* egl_surf = static_cast<GLSurfaceEGL*>(surf_.get());
    EGLConfig config = egl_surf->GetConfig();
    if (!config) {
      return false;
    }

    EGLint pbuffer_attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    EGLSurface temp_surface = eglCreatePbufferSurface(
        egl_display, config, pbuffer_attribs);
    if (temp_surface == EGL_NO_SURFACE) {
      return false;
    }

    surface->surface_ = temp_surface;

    if (!gl_context_->MakeCurrent(surface)) {
      surface->surface_ = nullptr;
      eglDestroySurface(egl_display, temp_surface);
      return false;
    }

    return true;
  }

  void RestoreContext() {
    gl_context_->MakeCurrent(surf_.get());
  }

  scoped_refptr<gl::GLSurface> surf_;
  scoped_refptr<gl::GLContext> gl_context_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TEST_F(GlSurfaceEglOhosTest, Create_NoEnhanceSwitch) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);

  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
}

TEST_F(GlSurfaceEglOhosTest, Create_EnhanceSwitch_NoSurfaceInfo) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kOhosHanceSurface);

  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockWidgetForEnhance);
  EXPECT_EQ(surface, nullptr);
}

TEST_F(GlSurfaceEglOhosTest, Create_EnhanceSwitch_WithSurfaceInfo) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kOhosHanceSurface);

  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = EGL_NO_SURFACE;
  info->context = EGL_NO_CONTEXT;
  info->window = reinterpret_cast<void*>(info);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));

  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), res);
  ASSERT_TRUE(surface != nullptr);
}

TEST_F(GlSurfaceEglOhosTest, Constructor_WithValidWindow) {
  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = eglGetCurrentSurface(EGL_DRAW);
  info->context = eglGetCurrentContext();
  info->window = reinterpret_cast<void*>(0x5555);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));
  void* window = NWebNativeWindowTracker::GetInstance()->GetNativeWindow(res);

  auto* surface = new NativeViewGLSurfaceEGLOhos(
      display_->GetAs<gl::GLDisplayEGL>(),
      reinterpret_cast<EGLNativeWindowType>(window));

  ASSERT_TRUE(surface != nullptr);
}

TEST_F(GlSurfaceEglOhosTest, SwapBuffers_FirstSwapBuffers) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  EXPECT_TRUE(surface->is_first_swapbuffers_);

  gfx::FrameData frame_data;
  auto result = surface->SwapBuffers(
      base::BindOnce(&DummyPresentationCallback), frame_data);

  EXPECT_TRUE(result == gfx::SwapResult::SWAP_ACK ||
              result == gfx::SwapResult::SWAP_FAILED);
  EXPECT_FALSE(surface->is_first_swapbuffers_);
}

TEST_F(GlSurfaceEglOhosTest, SwapBuffers_NotFirstSwapBuffers) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  gfx::FrameData frame_data;
  surface->SwapBuffers(base::BindOnce(&DummyPresentationCallback), frame_data);
  ASSERT_FALSE(surface->is_first_swapbuffers_);

  auto result2 = surface->SwapBuffers(
      base::BindOnce(&DummyPresentationCallback), frame_data);
  EXPECT_TRUE(result2 == gfx::SwapResult::SWAP_ACK ||
              result2 == gfx::SwapResult::SWAP_FAILED);
}

TEST_F(GlSurfaceEglOhosTest, SwapBuffers_ExplicitNotFirst) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->is_first_swapbuffers_ = false;

  gfx::FrameData frame_data;
  auto result = surface->SwapBuffers(
      base::BindOnce(&DummyPresentationCallback), frame_data);
  EXPECT_TRUE(result == gfx::SwapResult::SWAP_ACK ||
              result == gfx::SwapResult::SWAP_FAILED);
}

TEST_F(GlSurfaceEglOhosTest, SwapBuffers_EnableReplaceBufferOutput) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->enable_replace_swap_buffer_output_ = true;

  gfx::FrameData frame_data;
  auto result = surface->SwapBuffers(
      base::BindOnce(&DummyPresentationCallback), frame_data);
  EXPECT_TRUE(result == gfx::SwapResult::SWAP_ACK ||
              result == gfx::SwapResult::SWAP_FAILED);
}

TEST_F(GlSurfaceEglOhosTest, SwapBuffers_EnableReplace_NotFirst) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->enable_replace_swap_buffer_output_ = true;
  surface->is_first_swapbuffers_ = false;

  gfx::FrameData frame_data;
  auto result = surface->SwapBuffers(
      base::BindOnce(&DummyPresentationCallback), frame_data);
  EXPECT_TRUE(result == gfx::SwapResult::SWAP_ACK ||
              result == gfx::SwapResult::SWAP_FAILED);
}

TEST_F(GlSurfaceEglOhosTest, SwapBuffers_MultipleCalls) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  gfx::FrameData frame_data;

  auto result1 = surface->SwapBuffers(
      base::BindOnce(&DummyPresentationCallback), frame_data);
  EXPECT_TRUE(result1 == gfx::SwapResult::SWAP_ACK ||
              result1 == gfx::SwapResult::SWAP_FAILED);

  auto result2 = surface->SwapBuffers(
      base::BindOnce(&DummyPresentationCallback), frame_data);
  EXPECT_TRUE(result2 == gfx::SwapResult::SWAP_ACK ||
              result2 == gfx::SwapResult::SWAP_FAILED);
}

TEST_F(GlSurfaceEglOhosTest, Resize_ValidSize) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  if (!SetUpSurfaceForResize(surface.get())) {
    return;
  }

  gfx::Size valid_size(100, 100);
  surface->Resize(valid_size, 1.0f, gfx::ColorSpace(), false);

  RestoreContext();
}

TEST_F(GlSurfaceEglOhosTest, Resize_LargeSize) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  if (!SetUpSurfaceForResize(surface.get())) {
    return;
  }

  gfx::Size large_size(1000, 1000);
  surface->Resize(large_size, 1.0f, gfx::ColorSpace(), false);

  RestoreContext();
}

TEST_F(GlSurfaceEglOhosTest, Resize_InnerWebTrue) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->SetNativeInnerWeb(true);

  if (!SetUpSurfaceForResize(surface.get())) {
    return;
  }

  gfx::Size size(100, 100);
  surface->Resize(size, 1.0f, gfx::ColorSpace(), false);

  RestoreContext();
}

TEST_F(GlSurfaceEglOhosTest, Resize_InnerWebFalse) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->SetNativeInnerWeb(false);

  if (!SetUpSurfaceForResize(surface.get())) {
    return;
  }

  gfx::Size size(100, 100);
  surface->Resize(size, 1.0f, gfx::ColorSpace(), false);

  RestoreContext();
}

TEST_F(GlSurfaceEglOhosTest, Resize_DifferentParameters) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  if (!SetUpSurfaceForResize(surface.get())) {
    return;
  }

  gfx::Size size1(100, 100);
  surface->Resize(size1, 1.0f, gfx::ColorSpace(), false);
  RestoreContext();

  SetUpSurfaceForResize(surface.get());
  gfx::Size size2(150, 150);
  surface->Resize(size2, 2.0f, gfx::ColorSpace(), true);
  RestoreContext();
}

TEST_F(GlSurfaceEglOhosTest, GetSize_Initially) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  gfx::Size size = surface->GetSize();
  EXPECT_TRUE(size.width() >= 0);
  EXPECT_TRUE(size.height() >= 0);
}

TEST_F(GlSurfaceEglOhosTest, GetSize_AfterResize) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  if (!SetUpSurfaceForResize(surface.get())) {
    return;
  }

  gfx::Size new_size(200, 200);
  surface->Resize(new_size, 1.0f, gfx::ColorSpace(), false);
  RestoreContext();

  gfx::Size size = surface->GetSize();
}

TEST_F(GlSurfaceEglOhosTest, SwapBuffers_AfterResize) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  if (!SetUpSurfaceForResize(surface.get())) {
    return;
  }

  gfx::Size size(200, 200);
  surface->Resize(size, 1.0f, gfx::ColorSpace(), false);
  RestoreContext();

  gfx::FrameData frame_data;
  auto result = surface->SwapBuffers(
      base::BindOnce(&DummyPresentationCallback), frame_data);
  EXPECT_TRUE(result == gfx::SwapResult::SWAP_ACK ||
              result == gfx::SwapResult::SWAP_FAILED);
}

TEST_F(GlSurfaceEglOhosTest, SetBackbufferAllocation_AllocatedTrue) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  bool result = surface->SetBackbufferAllocation(true);
  EXPECT_TRUE(result);
}

TEST_F(GlSurfaceEglOhosTest, SetBackbufferAllocation_InnerWebTrue) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->SetNativeInnerWeb(true);
  bool result = surface->SetBackbufferAllocation(false);
  EXPECT_TRUE(result);
}

TEST_F(GlSurfaceEglOhosTest, SetBackbufferAllocation_WithWindowAndContext) {
  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = eglGetCurrentSurface(EGL_DRAW);
  info->context = eglGetCurrentContext();
  info->window = reinterpret_cast<void*>(0x2222);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));
  void* window = NWebNativeWindowTracker::GetInstance()->GetNativeWindow(res);

  auto* surface = new NativeViewGLSurfaceEGLOhos(
      display_->GetAs<gl::GLDisplayEGL>(),
      reinterpret_cast<EGLNativeWindowType>(window));

  surface->SetNativeInnerWeb(false);
  bool result = surface->SetBackbufferAllocation(false);
  EXPECT_TRUE(result);
}

TEST_F(GlSurfaceEglOhosTest, SetBackbufferAllocation_NullContext) {
  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = eglGetCurrentSurface(EGL_DRAW);
  info->context = eglGetCurrentContext();
  info->window = reinterpret_cast<void*>(0x3333);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));
  void* window = NWebNativeWindowTracker::GetInstance()->GetNativeWindow(res);

  auto* surface = new NativeViewGLSurfaceEGLOhos(
      display_->GetAs<gl::GLDisplayEGL>(),
      reinterpret_cast<EGLNativeWindowType>(window));

  surface->SetNativeInnerWeb(false);
  gl_context_->ReleaseCurrent(surf_.get());
  bool result = surface->SetBackbufferAllocation(false);
  EXPECT_TRUE(result);

  ASSERT_TRUE(gl_context_->MakeCurrent(surf_.get()));
}

TEST_F(GlSurfaceEglOhosTest, SetBackbufferAllocation_NoWindowInTracker) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->SetNativeInnerWeb(false);
  bool result = surface->SetBackbufferAllocation(false);
  EXPECT_TRUE(result);
}

TEST_F(GlSurfaceEglOhosTest, SetBackbufferAllocation_MultipleCalls) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  EXPECT_TRUE(surface->SetBackbufferAllocation(true));
  EXPECT_TRUE(surface->SetBackbufferAllocation(false));
  EXPECT_TRUE(surface->SetBackbufferAllocation(true));
  EXPECT_TRUE(surface->SetBackbufferAllocation(false));
}

TEST_F(GlSurfaceEglOhosTest, SetNativeInnerWeb_True) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->SetNativeInnerWeb(true);
#if BUILDFLAG(ARKWEB_SAME_LAYER)
  EXPECT_TRUE(surface->isInnerWeb_);
#endif
}

TEST_F(GlSurfaceEglOhosTest, SetNativeInnerWeb_False) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->SetNativeInnerWeb(false);
#if BUILDFLAG(ARKWEB_SAME_LAYER)
  EXPECT_FALSE(surface->isInnerWeb_);
#endif
}

TEST_F(GlSurfaceEglOhosTest, SetNativeInnerWeb_MultipleToggle) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->SetNativeInnerWeb(true);
  surface->SetNativeInnerWeb(false);
  surface->SetNativeInnerWeb(true);
  surface->SetNativeInnerWeb(false);

#if BUILDFLAG(ARKWEB_SAME_LAYER)
  EXPECT_FALSE(surface->isInnerWeb_);
#endif
}

TEST_F(GlSurfaceEglOhosTest, SetBypassVsyncCondition_ZeroCondition) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->SetBypassVsyncCondition(0);
}

TEST_F(GlSurfaceEglOhosTest, SetBypassVsyncCondition_ConditionOne) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

#if BUILDFLAG(ARKWEB_VSYNC_SCHEDULE)
  surface->condition_ = 1;
#endif
  surface->SetBypassVsyncCondition(2);
}

TEST_F(GlSurfaceEglOhosTest, SetBypassVsyncCondition_WithWindow) {
  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = eglGetCurrentSurface(EGL_DRAW);
  info->context = eglGetCurrentContext();
  info->window = reinterpret_cast<void*>(0x4444);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));
  void* window = NWebNativeWindowTracker::GetInstance()->GetNativeWindow(res);

  auto* surface = new NativeViewGLSurfaceEGLOhos(
      display_->GetAs<gl::GLDisplayEGL>(),
      reinterpret_cast<EGLNativeWindowType>(window));

  surface->SetBypassVsyncCondition(5);
}

TEST_F(GlSurfaceEglOhosTest, SetBypassVsyncCondition_NoWindow) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface->SetBypassVsyncCondition(5);
}

TEST_F(GlSurfaceEglOhosTest, Destructor_Normal) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  surface = nullptr;
  EXPECT_TRUE(surface == nullptr);
}

TEST_F(GlSurfaceEglOhosTest, Destructor_AfterOperations) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  gfx::FrameData frame_data;
  surface->SwapBuffers(base::BindOnce(&DummyPresentationCallback), frame_data);

  if (SetUpSurfaceForResize(surface.get())) {
    gfx::Size size(100, 100);
    surface->Resize(size, 1.0f, gfx::ColorSpace(), false);
    RestoreContext();
  }

  surface->SetNativeInnerWeb(true);

  surface = nullptr;
  EXPECT_TRUE(surface == nullptr);
}

TEST_F(GlSurfaceEglOhosTest, MultipleSurfaceCreation) {
  auto surface1 = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface1 != nullptr);

  auto surface2 = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget + 1);
  ASSERT_TRUE(surface2 != nullptr);

  auto surface3 = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget + 2);
  ASSERT_TRUE(surface3 != nullptr);
}

TEST_F(GlSurfaceEglOhosTest, WindowRefManagement) {
  auto surface1 = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface1 != nullptr);

  auto surface2 = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget + 1);
  ASSERT_TRUE(surface2 != nullptr);

  surface1 = nullptr;
  surface2 = nullptr;
}

TEST_F(GlSurfaceEglOhosTest, CommandLine_SwitchEffect) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);

  auto surface1 = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface1 != nullptr);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kOhosHanceSurface);

  auto surface2 = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockWidgetForEnhance);
  EXPECT_EQ(surface2, nullptr);

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);
}

TEST_F(GlSurfaceEglOhosTest, Combined_ResizeSwapBuffersSetBackbuffer) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  if (SetUpSurfaceForResize(surface.get())) {
    gfx::Size size(200, 200);
    surface->Resize(size, 1.0f, gfx::ColorSpace(), false);
    RestoreContext();
  }

  gfx::FrameData frame_data;
  auto result = surface->SwapBuffers(
      base::BindOnce(&DummyPresentationCallback), frame_data);
  EXPECT_TRUE(result == gfx::SwapResult::SWAP_ACK ||
              result == gfx::SwapResult::SWAP_FAILED);

  EXPECT_TRUE(surface->SetBackbufferAllocation(true));
}

#if BUILDFLAG(ARKWEB_VSYNC_SCHEDULE)

TEST_F(GlSurfaceEglOhosTest, SetBypassVsyncCondition_BufferLoopAllSuccess) {
  g_mock_OH_NativeWindow_NativeObjectReference =
      [](void* obj) -> int32_t { return 0; };
  g_mock_OH_NativeWindow_NativeObjectUnreference =
      [](void* obj) -> int32_t { return 0; };

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = true;
  EXPECT_CALL(OHOS::NWeb::OhosInterfaceMock::GetInstance(),
              OH_NativeWindow_NativeWindowHandleOpt(testing::_, testing::_,
                                                    testing::_))
      .WillOnce(testing::Invoke(
          [](OHNativeWindow* window, int32_t code, uint64_t value) -> int32_t {
            if (code == GET_BUFFERQUEUE_SIZE) {
              *reinterpret_cast<int*>(static_cast<uintptr_t>(value)) = 3;
            }
            return 0;
          }));

  int request_count = 0;
  g_mock_OH_NativeWindow_NativeWindowRequestBuffer =
      [&request_count](OHNativeWindow* window,
                       OHNativeWindowBuffer** buffer,
                       int* fenceFd) -> int32_t {
    *buffer = reinterpret_cast<OHNativeWindowBuffer*>(0x1000 + request_count);
    *fenceFd = -1;
    request_count++;
    return 0;
  };

  g_mock_OH_NativeWindow_NativeWindowAbortBuffer =
      [](OHNativeWindow* window, OHNativeWindowBuffer* buffer) -> int32_t {
    return 0;
  };

  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = EGL_NO_SURFACE;
  info->context = EGL_NO_CONTEXT;
  info->window = reinterpret_cast<void*>(info);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));
  void* window = NWebNativeWindowTracker::GetInstance()->GetNativeWindow(res);

  scoped_refptr<NativeViewGLSurfaceEGLOhos> surface(
      new NativeViewGLSurfaceEGLOhos(
          display_->GetAs<gl::GLDisplayEGL>(),
          reinterpret_cast<EGLNativeWindowType>(window)));

  surface->SetBypassVsyncCondition(2);

  surface = nullptr;

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = false;
  g_mock_OH_NativeWindow_NativeObjectReference = nullptr;
  g_mock_OH_NativeWindow_NativeObjectUnreference = nullptr;
  g_mock_OH_NativeWindow_NativeWindowRequestBuffer = nullptr;
  g_mock_OH_NativeWindow_NativeWindowAbortBuffer = nullptr;

  NWebNativeWindowTracker::GetInstance()->DestroyNativeWindow(res);
  delete info;
}

TEST_F(GlSurfaceEglOhosTest, SetBypassVsyncCondition_BufferLoopWithFence) {
  g_mock_OH_NativeWindow_NativeObjectReference =
      [](void* obj) -> int32_t { return 0; };
  g_mock_OH_NativeWindow_NativeObjectUnreference =
      [](void* obj) -> int32_t { return 0; };

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = true;
  EXPECT_CALL(OHOS::NWeb::OhosInterfaceMock::GetInstance(),
              OH_NativeWindow_NativeWindowHandleOpt(testing::_, testing::_,
                                                    testing::_))
      .WillOnce(testing::Invoke(
          [](OHNativeWindow* window, int32_t code, uint64_t value) -> int32_t {
            if (code == GET_BUFFERQUEUE_SIZE) {
              *reinterpret_cast<int*>(static_cast<uintptr_t>(value)) = 3;
            }
            return 0;
          }));

  int request_count = 0;
  g_mock_OH_NativeWindow_NativeWindowRequestBuffer =
      [&request_count](OHNativeWindow* window,
                       OHNativeWindowBuffer** buffer,
                       int* fenceFd) -> int32_t {
    *buffer = reinterpret_cast<OHNativeWindowBuffer*>(0x2000 + request_count);
    if (request_count == 0) {
      *fenceFd = 100;
    } else {
      *fenceFd = -1;
    }
    request_count++;
    return 0;
  };

  g_mock_OH_NativeWindow_NativeWindowAbortBuffer =
      [](OHNativeWindow* window, OHNativeWindowBuffer* buffer) -> int32_t {
    return 0;
  };

  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = EGL_NO_SURFACE;
  info->context = EGL_NO_CONTEXT;
  info->window = reinterpret_cast<void*>(info);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));
  void* window = NWebNativeWindowTracker::GetInstance()->GetNativeWindow(res);

  scoped_refptr<NativeViewGLSurfaceEGLOhos> surface(
      new NativeViewGLSurfaceEGLOhos(
          display_->GetAs<gl::GLDisplayEGL>(),
          reinterpret_cast<EGLNativeWindowType>(window)));

  surface->SetBypassVsyncCondition(2);

  surface = nullptr;

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = false;
  g_mock_OH_NativeWindow_NativeObjectReference = nullptr;
  g_mock_OH_NativeWindow_NativeObjectUnreference = nullptr;
  g_mock_OH_NativeWindow_NativeWindowRequestBuffer = nullptr;
  g_mock_OH_NativeWindow_NativeWindowAbortBuffer = nullptr;

  NWebNativeWindowTracker::GetInstance()->DestroyNativeWindow(res);
  delete info;
}

TEST_F(GlSurfaceEglOhosTest, SetBypassVsyncCondition_BufferRequestFails) {
  g_mock_OH_NativeWindow_NativeObjectReference =
      [](void* obj) -> int32_t { return 0; };
  g_mock_OH_NativeWindow_NativeObjectUnreference =
      [](void* obj) -> int32_t { return 0; };

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = true;
  EXPECT_CALL(OHOS::NWeb::OhosInterfaceMock::GetInstance(),
              OH_NativeWindow_NativeWindowHandleOpt(testing::_, testing::_,
                                                    testing::_))
      .WillOnce(testing::Invoke(
          [](OHNativeWindow* window, int32_t code, uint64_t value) -> int32_t {
            if (code == GET_BUFFERQUEUE_SIZE) {
              *reinterpret_cast<int*>(static_cast<uintptr_t>(value)) = 3;
            }
            return 0;
          }));

  g_mock_OH_NativeWindow_NativeWindowRequestBuffer =
      [](OHNativeWindow* window,
         OHNativeWindowBuffer** buffer,
         int* fenceFd) -> int32_t {
    *buffer = nullptr;
    *fenceFd = -1;
    return -1;
  };

  g_mock_OH_NativeWindow_NativeWindowAbortBuffer =
      [](OHNativeWindow* window, OHNativeWindowBuffer* buffer) -> int32_t {
    return 0;
  };

  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = EGL_NO_SURFACE;
  info->context = EGL_NO_CONTEXT;
  info->window = reinterpret_cast<void*>(info);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));
  void* window = NWebNativeWindowTracker::GetInstance()->GetNativeWindow(res);

  scoped_refptr<NativeViewGLSurfaceEGLOhos> surface(
      new NativeViewGLSurfaceEGLOhos(
          display_->GetAs<gl::GLDisplayEGL>(),
          reinterpret_cast<EGLNativeWindowType>(window)));

  surface->SetBypassVsyncCondition(2);

  surface = nullptr;

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = false;
  g_mock_OH_NativeWindow_NativeObjectReference = nullptr;
  g_mock_OH_NativeWindow_NativeObjectUnreference = nullptr;
  g_mock_OH_NativeWindow_NativeWindowRequestBuffer = nullptr;
  g_mock_OH_NativeWindow_NativeWindowAbortBuffer = nullptr;

  NWebNativeWindowTracker::GetInstance()->DestroyNativeWindow(res);
  delete info;
}

TEST_F(GlSurfaceEglOhosTest, SetBypassVsyncCondition_BufferRequestPartialFail) {
  g_mock_OH_NativeWindow_NativeObjectReference =
      [](void* obj) -> int32_t { return 0; };
  g_mock_OH_NativeWindow_NativeObjectUnreference =
      [](void* obj) -> int32_t { return 0; };

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = true;
  EXPECT_CALL(OHOS::NWeb::OhosInterfaceMock::GetInstance(),
              OH_NativeWindow_NativeWindowHandleOpt(testing::_, testing::_,
                                                    testing::_))
      .WillOnce(testing::Invoke(
          [](OHNativeWindow* window, int32_t code, uint64_t value) -> int32_t {
            if (code == GET_BUFFERQUEUE_SIZE) {
              *reinterpret_cast<int*>(static_cast<uintptr_t>(value)) = 3;
            }
            return 0;
          }));

  int request_count = 0;
  g_mock_OH_NativeWindow_NativeWindowRequestBuffer =
      [&request_count](OHNativeWindow* window,
                       OHNativeWindowBuffer** buffer,
                       int* fenceFd) -> int32_t {
    if (request_count == 0) {
      *buffer = reinterpret_cast<OHNativeWindowBuffer*>(0x3000);
      *fenceFd = -1;
      request_count++;
      return 0;
    }
    *buffer = nullptr;
    *fenceFd = -1;
    request_count++;
    return -1;
  };

  g_mock_OH_NativeWindow_NativeWindowAbortBuffer =
      [](OHNativeWindow* window, OHNativeWindowBuffer* buffer) -> int32_t {
    return 0;
  };

  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = EGL_NO_SURFACE;
  info->context = EGL_NO_CONTEXT;
  info->window = reinterpret_cast<void*>(info);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));
  void* window = NWebNativeWindowTracker::GetInstance()->GetNativeWindow(res);

  scoped_refptr<NativeViewGLSurfaceEGLOhos> surface(
      new NativeViewGLSurfaceEGLOhos(
          display_->GetAs<gl::GLDisplayEGL>(),
          reinterpret_cast<EGLNativeWindowType>(window)));

  surface->SetBypassVsyncCondition(2);

  surface = nullptr;

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = false;
  g_mock_OH_NativeWindow_NativeObjectReference = nullptr;
  g_mock_OH_NativeWindow_NativeObjectUnreference = nullptr;
  g_mock_OH_NativeWindow_NativeWindowRequestBuffer = nullptr;
  g_mock_OH_NativeWindow_NativeWindowAbortBuffer = nullptr;

  NWebNativeWindowTracker::GetInstance()->DestroyNativeWindow(res);
  delete info;
}

TEST_F(GlSurfaceEglOhosTest, SetBypassVsyncCondition_BufferNumOne) {
  g_mock_OH_NativeWindow_NativeObjectReference =
      [](void* obj) -> int32_t { return 0; };
  g_mock_OH_NativeWindow_NativeObjectUnreference =
      [](void* obj) -> int32_t { return 0; };

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = true;
  EXPECT_CALL(OHOS::NWeb::OhosInterfaceMock::GetInstance(),
              OH_NativeWindow_NativeWindowHandleOpt(testing::_, testing::_,
                                                    testing::_))
      .WillOnce(testing::Invoke(
          [](OHNativeWindow* window, int32_t code, uint64_t value) -> int32_t {
            if (code == GET_BUFFERQUEUE_SIZE) {
              *reinterpret_cast<int*>(static_cast<uintptr_t>(value)) = 1;
            }
            return 0;
          }));

  g_mock_OH_NativeWindow_NativeWindowRequestBuffer =
      [](OHNativeWindow* window,
         OHNativeWindowBuffer** buffer,
         int* fenceFd) -> int32_t {
    *buffer = reinterpret_cast<OHNativeWindowBuffer*>(0x4000);
    *fenceFd = -1;
    return 0;
  };

  g_mock_OH_NativeWindow_NativeWindowAbortBuffer =
      [](OHNativeWindow* window, OHNativeWindowBuffer* buffer) -> int32_t {
    return 0;
  };

  auto* info = new WindowsSurfaceInfo();
  info->display = eglGetCurrentDisplay();
  info->surface = EGL_NO_SURFACE;
  info->context = EGL_NO_CONTEXT;
  info->window = reinterpret_cast<void*>(info);

  auto res = NWebNativeWindowTracker::GetInstance()->AddNativeWindow(
      static_cast<void*>(info));
  void* window = NWebNativeWindowTracker::GetInstance()->GetNativeWindow(res);

  scoped_refptr<NativeViewGLSurfaceEGLOhos> surface(
      new NativeViewGLSurfaceEGLOhos(
          display_->GetAs<gl::GLDisplayEGL>(),
          reinterpret_cast<EGLNativeWindowType>(window)));

  surface->SetBypassVsyncCondition(2);

  surface = nullptr;

  OHOS::NWeb::OhosInterfaceMock::nativeWindowHandleOpt = false;
  g_mock_OH_NativeWindow_NativeObjectReference = nullptr;
  g_mock_OH_NativeWindow_NativeObjectUnreference = nullptr;
  g_mock_OH_NativeWindow_NativeWindowRequestBuffer = nullptr;
  g_mock_OH_NativeWindow_NativeWindowAbortBuffer = nullptr;

  NWebNativeWindowTracker::GetInstance()->DestroyNativeWindow(res);
  delete info;
}
#endif

}  // namespace
}  // namespace gl