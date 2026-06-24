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

#include "arkweb/build/features/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/trace_event/trace_event.h"

#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/init/gl_factory.h"
#include "base/command_line.h"
#include "ui/gl/gl_context.cc"
#define private public
#define protected public
#include "arkweb/chromium_ext/ui/gl/init/gl_factory_ohos.cc"
#undef private
#undef protected

namespace gl {
namespace init {

class GLFactoryOhosTest : public testing::Test {
 public:
  GLFactoryOhosTest() = default;
  ~GLFactoryOhosTest() = default;

 protected:
  void SetUp() override {
    if (!base::CommandLine::InitializedForCurrentProcess()) {
      base::CommandLine::Init(0, nullptr);
    }
  }

  void TearDown() override {
    ShutdownGL(nullptr, false);
  }
};

TEST_F(GLFactoryOhosTest, GetAllowedGLImplementations) {
  std::vector<GLImplementationParts> implementations = GetAllowedGLImplementations();
  EXPECT_GE(implementations.size(), 1u);
}

TEST_F(GLFactoryOhosTest, InitializeStaticGLBindings_MockGL) {
  bool result = InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));
  EXPECT_TRUE(result);
}

TEST_F(GLFactoryOhosTest, InitializeStaticGLBindings_StubGL) {
  bool result = InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));
  EXPECT_TRUE(result);
}

TEST_F(GLFactoryOhosTest, GetGLImplementation_None) {
  EXPECT_EQ(GetGLImplementation(), kGLImplementationNone);
}

TEST_F(GLFactoryOhosTest, GetGLImplementation_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));
  EXPECT_EQ(GetGLImplementation(), kGLImplementationMockGL);
}

TEST_F(GLFactoryOhosTest, GetGLImplementation_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));
  EXPECT_EQ(GetGLImplementation(), kGLImplementationStubGL);
}

TEST_F(GLFactoryOhosTest, GLImplementationConstants) {
  EXPECT_EQ(kGLImplementationNone, 0);
  EXPECT_EQ(kGLImplementationEGLGLES2, 5);
  EXPECT_EQ(kGLImplementationMockGL, 6);
  EXPECT_EQ(kGLImplementationStubGL, 7);
  EXPECT_EQ(kGLImplementationDisabled, 8);
  EXPECT_EQ(kGLImplementationEGLANGLE, 9);
}

TEST_F(GLFactoryOhosTest, GLImplementationPartsConstruction) {
  GLImplementationParts mock_impl(kGLImplementationMockGL);
  EXPECT_EQ(mock_impl.gl, kGLImplementationMockGL);

  GLImplementationParts stub_impl(kGLImplementationStubGL);
  EXPECT_EQ(stub_impl.gl, kGLImplementationStubGL);

  GLImplementationParts egl_impl(kGLImplementationEGLGLES2);
  EXPECT_EQ(egl_impl.gl, kGLImplementationEGLGLES2);
}

TEST_F(GLFactoryOhosTest, GLImplementationPartsEquality) {
  GLImplementationParts impl1(kGLImplementationMockGL);
  GLImplementationParts impl2(kGLImplementationMockGL);
  EXPECT_TRUE(impl1 == impl2);

  GLImplementationParts impl3(kGLImplementationStubGL);
  EXPECT_FALSE(impl1 == impl3);
}

TEST_F(GLFactoryOhosTest, CreateSurfacelessViewGLSurface) {
  scoped_refptr<Presenter> presenter = CreateSurfacelessViewGLSurface(nullptr, gfx::kNullAcceleratedWidget);
  EXPECT_EQ(presenter, nullptr);
}

TEST_F(GLFactoryOhosTest, SetDisabledExtensionsPlatform_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  SetDisabledExtensionsPlatform("GL_EXT_test");
}

TEST_F(GLFactoryOhosTest, SetDisabledExtensionsPlatform_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));
  SetDisabledExtensionsPlatform("GL_EXT_test");
}

TEST_F(GLFactoryOhosTest, InitializeExtensionSettingsOneOffPlatform_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));
  bool result = InitializeExtensionSettingsOneOffPlatform(nullptr);
  EXPECT_TRUE(result);
}

TEST_F(GLFactoryOhosTest, InitializeExtensionSettingsOneOffPlatform_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));
  bool result = InitializeExtensionSettingsOneOffPlatform(nullptr);
  EXPECT_TRUE(result);
}

TEST_F(GLFactoryOhosTest, InitializeStaticGLBindings_MockGLThenStubGL) {
  bool result1 = InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));
  EXPECT_TRUE(result1);

  ShutdownGL(nullptr, false);

  bool result2 = InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));
  EXPECT_TRUE(result2);
}

TEST_F(GLFactoryOhosTest, GetGLWindowSystemBindingInfo_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  GLVersionInfo gl_info("OpenGL ES 2.0", "Mock", gfx::ExtensionSet());
  GLWindowSystemBindingInfo info;

  bool result = GetGLWindowSystemBindingInfo(gl_info, &info);
  EXPECT_FALSE(result);
}

TEST_F(GLFactoryOhosTest, GetGLWindowSystemBindingInfo_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  GLVersionInfo gl_info("OpenGL ES 2.0", "Mock", gfx::ExtensionSet());
  GLWindowSystemBindingInfo info;

  bool result = GetGLWindowSystemBindingInfo(gl_info, &info);
  EXPECT_FALSE(result);
}

TEST_F(GLFactoryOhosTest, GetGLWindowSystemBindingInfo_None) {
  GLVersionInfo gl_info("OpenGL ES 2.0", "Mock", gfx::ExtensionSet());
  GLWindowSystemBindingInfo info;

  bool result = GetGLWindowSystemBindingInfo(gl_info, &info);
  EXPECT_FALSE(result);
}

TEST_F(GLFactoryOhosTest, GetGLWindowSystemBindingInfo_MockGL_NullInfo) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  GLVersionInfo gl_info("OpenGL ES 2.0", "Mock", gfx::ExtensionSet());

  bool result = GetGLWindowSystemBindingInfo(gl_info, nullptr);
  EXPECT_FALSE(result);
}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurfaceWithFormat_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(100, 100), GLSurfaceFormat());
  EXPECT_NE(surface, nullptr);

}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurfaceWithFormat_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(100, 100), GLSurfaceFormat());
  EXPECT_NE(surface, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurfaceWithFormat_MockGL_ZeroSize) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(0, 0), GLSurfaceFormat());
  EXPECT_NE(surface, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurfaceWithFormat_StubGL_ZeroSize) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(0, 0), GLSurfaceFormat());
  EXPECT_NE(surface, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurface_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));
  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurface(
      nullptr, gfx::Size(100, 100));
  EXPECT_NE(surface, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurface_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurface(
      nullptr, gfx::Size(100, 100));
  EXPECT_NE(surface, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateGLContext_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurface(
      nullptr, gfx::Size(100, 100));
  ASSERT_NE(surface, nullptr);

  scoped_refptr<GLContext> context = CreateGLContext(
      nullptr, surface.get(), GLContextAttribs());
  EXPECT_NE(context, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateGLContext_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurface(
      nullptr, gfx::Size(100, 100));
  ASSERT_NE(surface, nullptr);

  scoped_refptr<GLContext> context = CreateGLContext(
      nullptr, surface.get(), GLContextAttribs());
  EXPECT_NE(context, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateGLContext_MockGL_WithShareGroup) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurface(
      nullptr, gfx::Size(100, 100));
  ASSERT_NE(surface, nullptr);

  scoped_refptr<GLShareGroup> share_group = new GLShareGroup();
  scoped_refptr<GLContext> context = CreateGLContext(
      share_group.get(), surface.get(), GLContextAttribs());
  EXPECT_NE(context, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateGLContext_StubGL_WithShareGroup) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  scoped_refptr<GLSurface> surface = CreateOffscreenGLSurface(
      nullptr, gfx::Size(100, 100));
  ASSERT_NE(surface, nullptr);

  scoped_refptr<GLShareGroup> share_group = new GLShareGroup();
  scoped_refptr<GLContext> context = CreateGLContext(
      share_group.get(), surface.get(), GLContextAttribs());
  EXPECT_NE(context, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateGLContext_MockGL_NullSurface) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  scoped_refptr<GLContext> context = CreateGLContext(
      nullptr, nullptr, GLContextAttribs());
  EXPECT_NE(context, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateGLContext_StubGL_NullSurface) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  scoped_refptr<GLContext> context = CreateGLContext(
      nullptr, nullptr, GLContextAttribs());
  EXPECT_NE(context, nullptr);
}

TEST_F(GLFactoryOhosTest, MultipleInitializeStaticGLBindings) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  ShutdownGL(nullptr, false);

  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));
}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurface_MultipleSizes_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  scoped_refptr<GLSurface> surface1 = CreateOffscreenGLSurface(
      nullptr, gfx::Size(0, 0));
  scoped_refptr<GLSurface> surface2 = CreateOffscreenGLSurface(
      nullptr, gfx::Size(100, 100));
  scoped_refptr<GLSurface> surface3 = CreateOffscreenGLSurface(
      nullptr, gfx::Size(512, 512));

  EXPECT_NE(surface1, nullptr);
  EXPECT_NE(surface2, nullptr);
  EXPECT_NE(surface3, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurface_MultipleSizes_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  scoped_refptr<GLSurface> surface1 = CreateOffscreenGLSurface(
      nullptr, gfx::Size(0, 0));
  scoped_refptr<GLSurface> surface2 = CreateOffscreenGLSurface(
      nullptr, gfx::Size(100, 100));
  scoped_refptr<GLSurface> surface3 = CreateOffscreenGLSurface(
      nullptr, gfx::Size(512, 512));

  EXPECT_NE(surface1, nullptr);
  EXPECT_NE(surface2, nullptr);
  EXPECT_NE(surface3, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurfaceWithFormat_MultipleSizes_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  scoped_refptr<GLSurface> surface1 = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(0, 0), GLSurfaceFormat());
  scoped_refptr<GLSurface> surface2 = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(256, 256), GLSurfaceFormat());
  scoped_refptr<GLSurface> surface3 = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(1024, 1024), GLSurfaceFormat());

  EXPECT_NE(surface1, nullptr);
  EXPECT_NE(surface2, nullptr);
  EXPECT_NE(surface3, nullptr);
}

TEST_F(GLFactoryOhosTest, CreateOffscreenGLSurfaceWithFormat_MultipleSizes_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  scoped_refptr<GLSurface> surface1 = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(0, 0), GLSurfaceFormat());
  scoped_refptr<GLSurface> surface2 = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(256, 256), GLSurfaceFormat());
  scoped_refptr<GLSurface> surface3 = CreateOffscreenGLSurfaceWithFormat(
      nullptr, gfx::Size(1024, 1024), GLSurfaceFormat());

  EXPECT_NE(surface1, nullptr);
  EXPECT_NE(surface2, nullptr);
  EXPECT_NE(surface3, nullptr);
}

TEST_F(GLFactoryOhosTest, SetDisabledExtensionsPlatform_EmptyString_MockGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  SetDisabledExtensionsPlatform("");
}

TEST_F(GLFactoryOhosTest, SetDisabledExtensionsPlatform_EmptyString_StubGL) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationStubGL));

  SetDisabledExtensionsPlatform("");
}

TEST_F(GLFactoryOhosTest, SetDisabledExtensionsPlatform_MultipleExtensions) {
  InitializeStaticGLBindingsImplementation(
      GLImplementationParts(kGLImplementationMockGL));

  SetDisabledExtensionsPlatform("GL_EXT_test1,GL_EXT_test2");
}

TEST_F(GLFactoryOhosTest, GetAllowedGLImplementations_ContainsEGLGLES2) {
  std::vector<GLImplementationParts> implementations = GetAllowedGLImplementations();

  bool has_egl_gles2 = false;
  for (const auto& impl : implementations) {
    if (impl.gl == kGLImplementationEGLGLES2) {
      has_egl_gles2 = true;
      break;
    }
  }
  EXPECT_TRUE(has_egl_gles2);
}

TEST_F(GLFactoryOhosTest, GLImplementationParts_MockGL_NotEqualStubGL) {
  GLImplementationParts mock_impl(kGLImplementationMockGL);
  GLImplementationParts stub_impl(kGLImplementationStubGL);

  EXPECT_FALSE(mock_impl == stub_impl);
  EXPECT_TRUE(mock_impl != stub_impl);
}

TEST_F(GLFactoryOhosTest, GLImplementationParts_EGLGLES2_NotEqualMockGL) {
  GLImplementationParts egl_impl(kGLImplementationEGLGLES2);
  GLImplementationParts mock_impl(kGLImplementationMockGL);

  EXPECT_FALSE(egl_impl == mock_impl);
  EXPECT_TRUE(egl_impl != mock_impl);
}

}  // namespace init
}  // namespace gl