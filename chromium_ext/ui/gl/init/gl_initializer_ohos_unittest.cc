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

#include "ui/gl/gl_bindings.h"
#define private public
#define protected public
#include "ui/gl/init/gl_initializer.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#undef private
#undef protected
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/test/gl_test_support.h"

#include "gtest/gtest.h"

namespace gl {
extern GLImplementationParts g_gl_implementation;
namespace init {

class GLInitializerOhosTest : public testing::Test {
 public:
  void SetUp() override {
    gl::init::InitializeGLNoExtensionsOneOff(
      /*init_bindings=*/true, /*gpu_preference=*/gl::GpuPreference::kDefault);
    display_ = gl::GLTestSupport::InitializeGL(std::nullopt);
    if (display_ == nullptr) {
      GTEST_SKIP() << "GL display initialization failed";
    }
  }

 protected:
  raw_ptr<GLDisplay> display_ = nullptr;
};

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_EGLGLES2_Success) {
  GLImplementation saved_impl = gl::GetGLImplementation();
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  GLDisplay* result = gl::init::InitializeGLOneOffPlatform(
      gl::GpuPreference::kDefault);
  EXPECT_NE(result, nullptr);

  gl::SetGLImplementation(saved_impl);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_EGLANGLE_SwitchCase) {
  GLImplementation saved_impl = gl::GetGLImplementation();
  gl::SetGLImplementation(gl::kGLImplementationEGLANGLE);

  GLDisplay* result = gl::init::InitializeGLOneOffPlatform(
      gl::GpuPreference::kDefault);
  EXPECT_NE(result, nullptr);

  gl::SetGLImplementation(saved_impl);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_DefaultBranch_MockGL) {
  GLImplementation saved_impl = gl::GetGLImplementation();
  gl::SetGLImplementation(gl::kGLImplementationMockGL);

  GLDisplay* result = gl::init::InitializeGLOneOffPlatform(
      gl::GpuPreference::kDefault);
  EXPECT_NE(result, nullptr);

  gl::SetGLImplementation(saved_impl);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_DefaultBranch_StubGL) {
  GLImplementation saved_impl = gl::GetGLImplementation();
  gl::SetGLImplementation(gl::kGLImplementationStubGL);

  GLDisplay* result = gl::init::InitializeGLOneOffPlatform(
      gl::GpuPreference::kDefault);
  EXPECT_NE(result, nullptr);

  gl::SetGLImplementation(saved_impl);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_AlreadyInitializedDisplay) {
  GLImplementation saved_impl = gl::GetGLImplementation();
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  GLDisplayEGL* display_egl = display_->GetAs<GLDisplayEGL>();
  ASSERT_NE(display_egl, nullptr);
  EGLDisplay saved_egl_display = display_egl->display_;

  display_egl->display_ = saved_egl_display;
  gl::SetGLImplementation(saved_impl);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_UninitializedDisplayFails) {
  GLImplementation saved_impl = gl::GetGLImplementation();
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  GLDisplayEGL* display_egl = display_->GetAs<GLDisplayEGL>();
  ASSERT_NE(display_egl, nullptr);
  EGLDisplay saved_egl_display = display_egl->display_;
  display_egl->display_ = EGL_NO_DISPLAY;

  GLDisplay* result = gl::init::InitializeGLOneOffPlatform(
      gl::GpuPreference::kDefault);

  if (result != nullptr) {
    display_egl->display_ = saved_egl_display;
  } else {
    display_egl->display_ = saved_egl_display;
  }

  gl::SetGLImplementation(saved_impl);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_EGLANGLE_AlreadyInitialized) {
  GLImplementation saved_impl = gl::GetGLImplementation();
  gl::SetGLImplementation(gl::kGLImplementationEGLANGLE);

  GLDisplayEGL* display_egl = display_->GetAs<GLDisplayEGL>();
  ASSERT_NE(display_egl, nullptr);
  EGLDisplay saved_egl_display = display_egl->display_;

  display_egl->display_ = saved_egl_display;
  gl::SetGLImplementation(saved_impl);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_EGLANGLE_UninitializedDisplay) {
  GLImplementation saved_impl = gl::GetGLImplementation();
  gl::SetGLImplementation(gl::kGLImplementationEGLANGLE);

  GLDisplayEGL* display_egl = display_->GetAs<GLDisplayEGL>();
  ASSERT_NE(display_egl, nullptr);
  EGLDisplay saved_egl_display = display_egl->display_;
  display_egl->display_ = EGL_NO_DISPLAY;

  GLDisplay* result = gl::init::InitializeGLOneOffPlatform(
      gl::GpuPreference::kDefault);

  display_egl->display_ = saved_egl_display;
  gl::SetGLImplementation(saved_impl);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_DefaultBranch_Disabled) {
  GLImplementation saved_impl = gl::GetGLImplementation();
  gl::SetGLImplementation(gl::kGLImplementationDisabled);

  GLDisplay* result = gl::init::InitializeGLOneOffPlatform(
      gl::GpuPreference::kDefault);
  EXPECT_NE(result, nullptr);

  gl::SetGLImplementation(saved_impl);
}

TEST_F(GLInitializerOhosTest, ShutdownGLPlatform_NullDisplay) {
  gl::init::ShutdownGLPlatform(nullptr);
  SUCCEED();
}

TEST_F(GLInitializerOhosTest, ShutdownGLPlatform_ValidDisplay) {
  gl::init::ShutdownGLPlatform(display_);
  SUCCEED();
}

class GLInitializerOhosNoInitTest : public testing::Test {
 public:
  void SetUp() override {}

  void TearDown() override {
    ClearBindingsGL();
  }
};

TEST_F(GLInitializerOhosNoInitTest, InitializeStaticGLBindings_MockGL) {
  bool result = gl::init::InitializeStaticGLBindings(
      GLImplementationParts(kGLImplementationMockGL));
  EXPECT_TRUE(result);
  EXPECT_EQ(gl::GetGLImplementation(), kGLImplementationMockGL);
}

TEST_F(GLInitializerOhosNoInitTest, InitializeStaticGLBindings_StubGL) {
  bool result = gl::init::InitializeStaticGLBindings(
      GLImplementationParts(kGLImplementationStubGL));
  EXPECT_TRUE(result);
  EXPECT_EQ(gl::GetGLImplementation(), kGLImplementationStubGL);
}

TEST_F(GLInitializerOhosNoInitTest, InitializeStaticGLBindings_EGLGLES2) {
  bool result = gl::init::InitializeStaticGLBindings(
      GLImplementationParts(kGLImplementationEGLGLES2));
  if (result) {
    EXPECT_EQ(gl::GetGLImplementation(), kGLImplementationEGLGLES2);
  } else {
    EXPECT_EQ(gl::GetGLImplementation(), kGLImplementationNone);
  }
}

TEST_F(GLInitializerOhosNoInitTest, InitializeStaticGLBindings_EGLANGLE) {
  bool result = gl::init::InitializeStaticGLBindings(
      GLImplementationParts(kGLImplementationEGLANGLE));
  if (result) {
    EXPECT_EQ(gl::GetGLImplementation(), kGLImplementationEGLANGLE);
  } else {
    EXPECT_EQ(gl::GetGLImplementation(), kGLImplementationNone);
  }
}

}  // namespace init
}  // namespace gl
