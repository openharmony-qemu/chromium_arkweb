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

#include <climits>
#include <cstdint>
#include <memory>

#include "base/trace_event/trace_event.h"
#define private public
#define protected public
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_egl.cc"
#undef private
#undef protected
#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/egl_mock.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_display_egl_util.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/gl/test/gl_test_support.h"
#include "ui/gl/gl_surface_egl_ohos.h"
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"

namespace gl {
namespace {

constexpr int32_t kMockAcceleratedWidget = 12345;

class GlSurfaceEglForIncludeTest : public testing::Test {
 public:
  void SetUp() override {
    gl::init::InitializeGLNoExtensionsOneOff(
        true, gl::GpuPreference::kDefault);
    display_ = gl::GLTestSupport::InitializeGL(std::nullopt);
    ASSERT_NE(display_, nullptr);
    surf_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    ASSERT_NE(surf_, nullptr);
    gl_context_ =
        gl::init::CreateGLContext(nullptr, surf_.get(), gl::GLContextAttribs());
    ASSERT_NE(gl_context_, nullptr);
    bool make_current_result = gl_context_->MakeCurrent(surf_.get());
    ASSERT_TRUE(make_current_result);
  }

 protected:
  scoped_refptr<gl::GLSurface> surf_;
  scoped_refptr<gl::GLContext> gl_context_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TEST_F(GlSurfaceEglForIncludeTest, GetPresentBufferAge_NotSupported) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_, kMockAcceleratedWidget);
  ASSERT_TRUE(surface);
  surface->supports_post_sub_buffer_ = false;
  gl::GLSurface* base = surface.get();
  EXPECT_EQ(base->GetPresentBufferAge(), 0);
}

TEST_F(GlSurfaceEglForIncludeTest, GetPresentBufferAge_Supported) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_, kMockAcceleratedWidget);
  ASSERT_TRUE(surface);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 5;
  gl::GLSurface* base = surface.get();
  EXPECT_EQ(base->GetPresentBufferAge(), 5);
}

TEST_F(GlSurfaceEglForIncludeTest, GetLastBufferDamageRect) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_, kMockAcceleratedWidget);
  ASSERT_TRUE(surface);
  gfx::Rect rect(10, 20, 30, 40);
  surface->last_damage_rects_ = rect;
  gl::GLSurface* base = surface.get();
  EXPECT_EQ(base->GetLastBufferDamageRect(), rect);
}

TEST_F(GlSurfaceEglForIncludeTest, GetSameBufferDamageCnt) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_, kMockAcceleratedWidget);
  ASSERT_TRUE(surface);
  surface->same_damage_count_ = 7;
  gl::GLSurface* base = surface.get();
  EXPECT_EQ(base->GetSameBufferDamageCnt(), 7);
}

TEST_F(GlSurfaceEglForIncludeTest, SetPresentBufferDamage_NotSupported) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_, kMockAcceleratedWidget);
  ASSERT_TRUE(surface);
  surface->supports_post_sub_buffer_ = false;
  gl::GLSurface* base = surface.get();
  EXPECT_TRUE(base->SetPresentBufferDamage(
      gfx::Rect(0, 0, 10, 10), gfx::Rect(0, 0, 100, 100)));
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_MatchingRect_EmptyDamage_BufferAgeZero) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_, kMockAcceleratedWidget);
  ASSERT_TRUE(surface);
  surface->supports_post_sub_buffer_ = true;
  gfx::Rect curr_rect(0, 0, 100, 100);
  surface->last_damage_rects_ = curr_rect;
  surface->present_buffer_age_ = 0;
  surface->same_damage_count_ = 3;
  gl::GLSurface* base = surface.get();
  EXPECT_TRUE(base->SetPresentBufferDamage(gfx::Rect(), curr_rect));
  EXPECT_EQ(surface->same_damage_count_.load(), 0);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_MatchingRect_EmptyDamage_BufferAgeNonZero) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_, kMockAcceleratedWidget);
  ASSERT_TRUE(surface);
  surface->supports_post_sub_buffer_ = true;
  gfx::Rect curr_rect(0, 0, 100, 100);
  surface->last_damage_rects_ = curr_rect;
  surface->present_buffer_age_ = 1;
  surface->same_damage_count_ = 5;
  gl::GLSurface* base = surface.get();
  EXPECT_TRUE(base->SetPresentBufferDamage(gfx::Rect(), curr_rect));
  EXPECT_EQ(surface->same_damage_count_.load(), 6);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_NonMatchingRect_EmptyDamage) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kOhosHanceSurface);
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_, kMockAcceleratedWidget);
  ASSERT_TRUE(surface);
  surface->supports_post_sub_buffer_ = true;
  gfx::Rect curr_rect(0, 0, 100, 100);
  surface->last_damage_rects_ = gfx::Rect(0, 0, 50, 50);
  surface->present_buffer_age_ = 1;
  surface->same_damage_count_ = 5;
  gl::GLSurface* base = surface.get();
  EXPECT_TRUE(base->SetPresentBufferDamage(gfx::Rect(), curr_rect));
  EXPECT_EQ(surface->same_damage_count_.load(), 0);
  EXPECT_EQ(surface->last_damage_rects_.load(), gfx::Rect(0, 0, 100, 100));
}

TEST_F(GlSurfaceEglForIncludeTest, IsPartialDrawEnable) {
  bool result1 = IsPartialDrawEnable();
  bool result2 = IsPartialDrawEnable();
  EXPECT_EQ(result1, result2);
}

}  // namespace
}  // namespace gl
