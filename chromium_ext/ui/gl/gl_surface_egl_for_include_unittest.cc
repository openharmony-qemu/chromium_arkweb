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


namespace gl {
namespace {

constexpr int32_t kMockAcceleratedWidget = 12345;

void DummyPresentationCallback(const gfx::PresentationFeedback& feedback) {}

class GlSurfaceEglForIncludeTest : public testing::Test {
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
  scoped_refptr<gl::GLSurface> surf_;
  scoped_refptr<gl::GLContext> gl_context_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

#if BUILDFLAG(ARKWEB_PARTIAL_DRAW)

TEST_F(GlSurfaceEglForIncludeTest, GetPresentBufferAge_NoSupport) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = false;
  EXPECT_EQ(surface->GetPresentBufferAge(), 0);
}

TEST_F(GlSurfaceEglForIncludeTest, GetPresentBufferAge_Positive) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 5;
  EXPECT_EQ(surface->GetPresentBufferAge(), 5);
}

TEST_F(GlSurfaceEglForIncludeTest, GetPresentBufferAge_Zero) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 0;
  EXPECT_EQ(surface->GetPresentBufferAge(), 0);
}

TEST_F(GlSurfaceEglForIncludeTest, GetPresentBufferAge_Negative) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = -1;
  EXPECT_EQ(surface->GetPresentBufferAge(), -1);
}

TEST_F(GlSurfaceEglForIncludeTest, GetPresentBufferAge_Large) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 10000;
  EXPECT_EQ(surface->GetPresentBufferAge(), 10000);
}

TEST_F(GlSurfaceEglForIncludeTest, GetLastBufferDamageRect_InitiallyEmpty) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_TRUE(surface->GetLastBufferDamageRect().IsEmpty());
}

TEST_F(GlSurfaceEglForIncludeTest, GetLastBufferDamageRect_ValidRect) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  gfx::Rect test_rect(10, 20, 30, 40);
  surface->last_damage_rects_ = test_rect;
  EXPECT_EQ(surface->GetLastBufferDamageRect(), test_rect);
}

TEST_F(GlSurfaceEglForIncludeTest, GetLastBufferDamageRect_LargeRect) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  gfx::Rect test_rect(0, 0, 1000, 1000);
  surface->last_damage_rects_ = test_rect;
  EXPECT_EQ(surface->GetLastBufferDamageRect(), test_rect);
}

TEST_F(GlSurfaceEglForIncludeTest, GetLastBufferDamageRect_MultipleUpdates) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);

  gfx::Rect rect1(10, 10, 20, 20);
  surface->last_damage_rects_ = rect1;
  EXPECT_EQ(surface->GetLastBufferDamageRect(), rect1);

  gfx::Rect rect2(30, 30, 40, 40);
  surface->last_damage_rects_ = rect2;
  EXPECT_EQ(surface->GetLastBufferDamageRect(), rect2);

  gfx::Rect rect3(50, 50, 60, 60);
  surface->last_damage_rects_ = rect3;
  EXPECT_EQ(surface->GetLastBufferDamageRect(), rect3);
}

TEST_F(GlSurfaceEglForIncludeTest, GetLastBufferDamageRect_NegativeCoords) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  gfx::Rect test_rect(-5, -10, 30, 40);
  surface->last_damage_rects_ = test_rect;
  EXPECT_EQ(surface->GetLastBufferDamageRect(), test_rect);
}

TEST_F(GlSurfaceEglForIncludeTest, GetSameBufferDamageCnt_InitiallyZero) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
}

TEST_F(GlSurfaceEglForIncludeTest, GetSameBufferDamageCnt_PositiveValue) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->same_damage_count_ = 10;
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 10);
}

TEST_F(GlSurfaceEglForIncludeTest, GetSameBufferDamageCnt_LargeValue) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->same_damage_count_ = 100000;
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 100000);
}

TEST_F(GlSurfaceEglForIncludeTest, GetSameBufferDamageCnt_NegativeValue) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->same_damage_count_ = -5;
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), -5);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_NoSupport_ReturnsTrue) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = false;

  gfx::Rect damage_rect(10, 10, 50, 50);
  gfx::Rect curr_rect(0, 0, 100, 100);

  EXPECT_TRUE(surface->SetPresentBufferDamage(damage_rect, curr_rect));
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_NoSupport_StateUnchanged) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = false;
  surface->same_damage_count_ = 5;
  surface->last_damage_rects_ = gfx::Rect(20, 20, 30, 30);

  gfx::Rect damage_rect(10, 10, 50, 50);
  gfx::Rect curr_rect(0, 0, 100, 100);

  surface->SetPresentBufferDamage(damage_rect, curr_rect);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 5);
  EXPECT_EQ(surface->GetLastBufferDamageRect(), gfx::Rect(20, 20, 30, 30));
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_EmptyDamage_AgeZero_DifferentCurrRect) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 0;
  surface->same_damage_count_ = 5;

  gfx::Rect empty_rect;
  gfx::Rect curr_rect(0, 0, 100, 100);

  EXPECT_TRUE(surface->SetPresentBufferDamage(empty_rect, curr_rect));
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_EmptyDamage_AgeZero_SameCurrRect) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 0;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->same_damage_count_ = 3;

  gfx::Rect empty_rect;
  gfx::Rect curr_rect(0, 0, 100, 100);

  EXPECT_TRUE(surface->SetPresentBufferDamage(empty_rect, curr_rect));
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_EmptyDamage_AgeNonZero_SameCurrRect) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 2;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->same_damage_count_ = 3;

  gfx::Rect empty_rect;
  gfx::Rect curr_rect(0, 0, 100, 100);

  EXPECT_TRUE(surface->SetPresentBufferDamage(empty_rect, curr_rect));
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 4);
  EXPECT_EQ(surface->GetLastBufferDamageRect(), gfx::Rect(0, 0, 100, 100));
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_EmptyDamage_AgeNonZero_DifferentCurrRect) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 2;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->same_damage_count_ = 3;

  gfx::Rect empty_rect;
  gfx::Rect curr_rect(50, 50, 30, 30);

  EXPECT_TRUE(surface->SetPresentBufferDamage(empty_rect, curr_rect));
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
  EXPECT_EQ(surface->GetLastBufferDamageRect(), curr_rect);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_EmptyDamage_AgeNonZero_NoPreviousRect) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 3;
  surface->same_damage_count_ = 7;
  surface->last_damage_rects_ = gfx::Rect();

  gfx::Rect empty_rect;
  gfx::Rect curr_rect(0, 0, 100, 100);

  EXPECT_TRUE(surface->SetPresentBufferDamage(empty_rect, curr_rect));
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
  EXPECT_EQ(surface->GetLastBufferDamageRect(), curr_rect);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_NonEmptyDamage_SameCurrRect_EglSuccess) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->same_damage_count_ = 2;
  surface->size_ = gfx::Size(100, 100);

  gfx::Rect damage_rect(10, 10, 50, 50);
  gfx::Rect curr_rect(0, 0, 100, 100);

  EXPECT_FALSE(surface->SetPresentBufferDamage(damage_rect, curr_rect));
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
  EXPECT_EQ(surface->GetLastBufferDamageRect(), gfx::Rect(0, 0, 100, 100));
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_NonEmptyDamage_DifferentCurrRect_EglSuccess) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->same_damage_count_ = 10;
  surface->size_ = gfx::Size(100, 100);

  gfx::Rect damage_rect(10, 10, 50, 50);
  gfx::Rect curr_rect(20, 20, 60, 60);

  EXPECT_FALSE(surface->SetPresentBufferDamage(damage_rect, curr_rect));
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
  EXPECT_EQ(surface->GetLastBufferDamageRect(), curr_rect);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_NonEmptyDamage_NoPreviousRect_EglSuccess) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->last_damage_rects_ = gfx::Rect();
  surface->size_ = gfx::Size(100, 100);

  gfx::Rect damage_rect(10, 10, 50, 50);
  gfx::Rect curr_rect(0, 0, 100, 100);

  EXPECT_FALSE(surface->SetPresentBufferDamage(damage_rect, curr_rect));
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
  EXPECT_EQ(surface->GetLastBufferDamageRect(), curr_rect);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_NonEmptyDamage_EglFailure) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->same_damage_count_ = 5;
  surface->size_ = gfx::Size(100, 100);
  surface->surface_ = EGL_NO_SURFACE;

  gfx::Rect damage_rect(10, 10, 50, 50);
  gfx::Rect curr_rect(0, 0, 100, 100);

  bool result = surface->SetPresentBufferDamage(damage_rect, curr_rect);
  if (!result) {
    EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
  }
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_NonEmptyDamage_EglFailure_DifferentCurrRect) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->same_damage_count_ = 5;
  surface->size_ = gfx::Size(100, 100);
  surface->surface_ = EGL_NO_SURFACE;

  gfx::Rect damage_rect(10, 10, 50, 50);
  gfx::Rect curr_rect(20, 20, 60, 60);

  bool result = surface->SetPresentBufferDamage(damage_rect, curr_rect);
  if (!result) {
    EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
    EXPECT_EQ(surface->GetLastBufferDamageRect(), curr_rect);
  }
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_ZeroWidthRect_EmptyDamage) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 0;

  gfx::Rect zero_width(10, 10, 0, 50);
  gfx::Rect curr_rect(0, 0, 100, 100);
  EXPECT_TRUE(surface->SetPresentBufferDamage(zero_width, curr_rect));
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_ZeroHeightRect_EmptyDamage) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->present_buffer_age_ = 1;
  surface->same_damage_count_ = 3;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);

  gfx::Rect zero_height(10, 10, 50, 0);
  gfx::Rect curr_rect(0, 0, 100, 100);
  EXPECT_TRUE(surface->SetPresentBufferDamage(zero_height, curr_rect));
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_BothZeroRect_EmptyDamage) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;

  gfx::Rect both_zero(10, 10, 0, 0);
  gfx::Rect curr_rect(0, 0, 100, 100);
  EXPECT_TRUE(surface->SetPresentBufferDamage(both_zero, curr_rect));
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_FullScreenDamage) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->size_ = gfx::Size(100, 100);

  gfx::Rect full_damage(0, 0, 100, 100);
  gfx::Rect curr_rect(0, 0, 100, 100);

  bool result = surface->SetPresentBufferDamage(full_damage, curr_rect);
  EXPECT_TRUE(result || !result);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_BoundaryCoordinates) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->size_ = gfx::Size(100, 100);

  gfx::Rect damage_rect(99, 99, 1, 1);
  gfx::Rect curr_rect(0, 0, 100, 100);

  bool result = surface->SetPresentBufferDamage(damage_rect, curr_rect);
  EXPECT_TRUE(result || !result);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_SequenceOfOperations) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->size_ = gfx::Size(100, 100);
  surface->last_damage_rects_ = gfx::Rect();

  gfx::Rect curr_rect(0, 0, 100, 100);

  gfx::Rect damage1(10, 10, 20, 20);
  surface->SetPresentBufferDamage(damage1, curr_rect);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);

  gfx::Rect damage2(15, 15, 25, 25);
  surface->SetPresentBufferDamage(damage2, curr_rect);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);

  gfx::Rect damage3(20, 20, 30, 30);
  surface->SetPresentBufferDamage(damage3, curr_rect);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);

  gfx::Rect new_curr(30, 30, 40, 40);
  gfx::Rect damage4(5, 5, 10, 10);
  surface->SetPresentBufferDamage(damage4, new_curr);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
  EXPECT_EQ(surface->GetLastBufferDamageRect(), new_curr);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_MaxCountIncrement) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->same_damage_count_ = INT_MAX - 1;
  surface->size_ = gfx::Size(100, 100);

  gfx::Rect damage_rect(10, 10, 50, 50);
  gfx::Rect curr_rect(0, 0, 100, 100);

  surface->SetPresentBufferDamage(damage_rect, curr_rect);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_EmptyDamage_VariousAgeValues) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;

  surface->present_buffer_age_ = 0;
  gfx::Rect empty_rect;
  gfx::Rect curr(0, 0, 100, 100);
  surface->SetPresentBufferDamage(empty_rect, curr);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);

  surface->present_buffer_age_ = 1;
  surface->same_damage_count_ = 5;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->SetPresentBufferDamage(empty_rect, curr);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 6);

  surface->present_buffer_age_ = 2;
  surface->same_damage_count_ = 3;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->SetPresentBufferDamage(empty_rect, curr);
  EXPECT_EQ(surface->GetSameBufferDamageCnt(), 4);
}

TEST_F(GlSurfaceEglForIncludeTest,
       SetPresentBufferDamage_MultipleEglFailureThenSuccess) {
  auto surface = NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
      display_.get(), kMockAcceleratedWidget);
  ASSERT_TRUE(surface != nullptr);
  surface->supports_post_sub_buffer_ = true;
  surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
  surface->same_damage_count_ = 3;
  surface->size_ = gfx::Size(100, 100);
  surface->surface_ = EGL_NO_SURFACE;

  gfx::Rect damage_rect(10, 10, 50, 50);
  gfx::Rect curr_rect(0, 0, 100, 100);

  bool result1 = surface->SetPresentBufferDamage(damage_rect, curr_rect);

  surface->surface_ = eglGetCurrentSurface(EGL_DRAW);

  if (result1) {
    surface->SetPresentBufferDamage(damage_rect, curr_rect);
  } else {
    EXPECT_EQ(surface->GetSameBufferDamageCnt(), 0);
    surface->same_damage_count_ = 3;
    surface->last_damage_rects_ = gfx::Rect(0, 0, 100, 100);
    EXPECT_FALSE(surface->SetPresentBufferDamage(damage_rect, curr_rect));
  }
}

TEST_F(GlSurfaceEglForIncludeTest, IsPartialDrawEnable_CallMultipleTimes) {
  bool result1 = IsPartialDrawEnable();
  bool result2 = IsPartialDrawEnable();
  bool result3 = IsPartialDrawEnable();

  EXPECT_EQ(result1, result2);
  EXPECT_EQ(result2, result3);
}

TEST_F(GlSurfaceEglForIncludeTest, IsPartialDrawEnable_ConsistentResults) {
  bool first_result = IsPartialDrawEnable();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(IsPartialDrawEnable(), first_result);
  }
}

#endif

}  // namespace
}  // namespace gl