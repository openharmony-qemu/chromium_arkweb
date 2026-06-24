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

#include "arkweb/build/features/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#define private public
#define protected public
#include "arkweb/chromium_ext/ui/gl/init/gl_initializer_ohos.cc"
#undef private
#undef protected

#include "ui/gl/gl_display.h"
#include "base/command_line.h"

namespace gl {
namespace init {

class GLInitializerOhosTest : public testing::Test {
 public:
  GLInitializerOhosTest() = default;
  ~GLInitializerOhosTest() = default;

 protected:
  void SetUp() override {
    SetGLImplementation(kGLImplementationNone);
    base::CommandLine::Init(0, nullptr);
  }

  void TearDown() override {
    SetGLImplementation(kGLImplementationNone);
  }
};

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_MockGL) {
  GLImplementationParts implementation(kGLImplementationMockGL);
  bool result = InitializeStaticGLBindings(implementation);
  EXPECT_TRUE(result);
  EXPECT_EQ(GetGLImplementation(), kGLImplementationMockGL);
  SetGLImplementation(kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_StubGL) {
  GLImplementationParts implementation(kGLImplementationStubGL);
  bool result = InitializeStaticGLBindings(implementation);
  EXPECT_TRUE(result);
  EXPECT_EQ(GetGLImplementation(), kGLImplementationStubGL);
  SetGLImplementation(kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_None) {
  GLImplementationParts implementation(kGLImplementationNone);
  SetGLImplementation(kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_EGLGLES2) {
  SetGLImplementation(kGLImplementationNone);
  GLImplementationParts implementation(kGLImplementationEGLGLES2);
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_EGLANGLE) {
  SetGLImplementation(kGLImplementationNone);
  GLImplementationParts implementation(kGLImplementationEGLANGLE);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_None) {
  SetGLImplementation(kGLImplementationNone);
  GLDisplay* display = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_MockGL) {
  SetGLImplementation(kGLImplementationMockGL);
  GLDisplay* display = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);
  SetGLImplementation(kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_StubGL) {
  SetGLImplementation(kGLImplementationStubGL);
  GLDisplay* display = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);
  SetGLImplementation(kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_Disabled) {
  SetGLImplementation(kGLImplementationDisabled);
  GLDisplay* display = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);
  SetGLImplementation(kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_EGLGLES2) {
  SetGLImplementation(kGLImplementationEGLGLES2);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_EGLANGLE) {
  SetGLImplementation(kGLImplementationEGLANGLE);
}

TEST_F(GLInitializerOhosTest, GetGLImplementationDefault) {
  SetGLImplementation(kGLImplementationNone);
  GLImplementation impl = GetGLImplementation();
  EXPECT_EQ(impl, kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, SetAndGetGLImplementation) {
  SetGLImplementation(kGLImplementationMockGL);
  EXPECT_EQ(GetGLImplementation(), kGLImplementationMockGL);

  SetGLImplementation(kGLImplementationStubGL);
  EXPECT_EQ(GetGLImplementation(), kGLImplementationStubGL);

  SetGLImplementation(kGLImplementationNone);
  EXPECT_EQ(GetGLImplementation(), kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, GLImplementationPartsConstruction) {
  GLImplementationParts mock_impl(kGLImplementationMockGL);
  EXPECT_EQ(mock_impl.gl, kGLImplementationMockGL);

  GLImplementationParts stub_impl(kGLImplementationStubGL);
  EXPECT_EQ(stub_impl.gl, kGLImplementationStubGL);

  GLImplementationParts egl_impl(kGLImplementationEGLGLES2);
  EXPECT_EQ(egl_impl.gl, kGLImplementationEGLGLES2);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_GpuPreferenceDefault) {
  SetGLImplementation(kGLImplementationNone);
  GLDisplay* display = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_GpuPreferenceLowPower) {
  SetGLImplementation(kGLImplementationNone);
  GLDisplay* display = InitializeGLOneOffPlatform(gl::GpuPreference::kLowPower);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_GpuPreferenceHighPerformance) {
  SetGLImplementation(kGLImplementationNone);
  GLDisplay* display = InitializeGLOneOffPlatform(gl::GpuPreference::kHighPerformance);
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_MultipleCalls) {
  SetGLImplementation(kGLImplementationNone);

  GLImplementationParts mock_impl(kGLImplementationMockGL);
  bool result1 = InitializeStaticGLBindings(mock_impl);
  EXPECT_TRUE(result1);

  SetGLImplementation(kGLImplementationNone);

  GLImplementationParts stub_impl(kGLImplementationStubGL);
  bool result2 = InitializeStaticGLBindings(stub_impl);
  EXPECT_TRUE(result2);

  SetGLImplementation(kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindingsGL_Called) {
  SetGLImplementation(kGLImplementationNone);

  GLImplementationParts mock_impl(kGLImplementationMockGL);
  bool result = InitializeStaticGLBindings(mock_impl);
  EXPECT_TRUE(result);

  SetGLImplementation(kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeStaticEGLInternal_EGLGLES2) {
  SetGLImplementation(kGLImplementationNone);
  GLImplementationParts implementation(kGLImplementationEGLGLES2);
}

TEST_F(GLInitializerOhosTest, InitializeStaticEGLInternal_EGLANGLE) {
  SetGLImplementation(kGLImplementationNone);
  GLImplementationParts implementation(kGLImplementationEGLANGLE);
}

TEST_F(GLInitializerOhosTest, ShutdownGLPlatform_NullDisplay) {
  ShutdownGLPlatform(nullptr);
}

TEST_F(GLInitializerOhosTest, GLImplementationConstants) {
  EXPECT_EQ(kGLImplementationNone, 0);
  EXPECT_EQ(kGLImplementationEGLGLES2, 5);
  EXPECT_EQ(kGLImplementationMockGL, 6);
  EXPECT_EQ(kGLImplementationStubGL, 7);
  EXPECT_EQ(kGLImplementationDisabled, 8);
  EXPECT_EQ(kGLImplementationEGLANGLE, 9);
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_MockGLThenReset) {
  GLImplementationParts implementation(kGLImplementationMockGL);
  bool result = InitializeStaticGLBindings(implementation);
  EXPECT_TRUE(result);

  SetGLImplementation(kGLImplementationNone);
  GLImplementation impl = GetGLImplementation();
  EXPECT_EQ(impl, kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_StubGLThenReset) {
  GLImplementationParts implementation(kGLImplementationStubGL);
  bool result = InitializeStaticGLBindings(implementation);
  EXPECT_TRUE(result);

  SetGLImplementation(kGLImplementationNone);
  GLImplementation impl = GetGLImplementation();
  EXPECT_EQ(impl, kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_AllImplementations) {
  std::vector<GLImplementation> implementations = {
    kGLImplementationNone,
    kGLImplementationMockGL,
    kGLImplementationStubGL,
    kGLImplementationDisabled
  };

  for (GLImplementation impl : implementations) {
    SetGLImplementation(impl);
    GLDisplay* display = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);
    SetGLImplementation(kGLImplementationNone);
  }
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_AllSafeImplementations) {
  std::vector<GLImplementation> implementations = {
    kGLImplementationMockGL,
    kGLImplementationStubGL
  };

  for (GLImplementation impl : implementations) {
    SetGLImplementation(kGLImplementationNone);
    GLImplementationParts implementation(impl);
    bool result = InitializeStaticGLBindings(implementation);
    EXPECT_TRUE(result);
    SetGLImplementation(kGLImplementationNone);
  }
}

TEST_F(GLInitializerOhosTest, InitializeGLOneOffPlatform_SwitchBranches) {
  SetGLImplementation(kGLImplementationNone);
  GLDisplay* display1 = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);

  SetGLImplementation(kGLImplementationMockGL);
  GLDisplay* display2 = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);

  SetGLImplementation(kGLImplementationStubGL);
  GLDisplay* display3 = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);

  SetGLImplementation(kGLImplementationDisabled);
  GLDisplay* display4 = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);

  SetGLImplementation(kGLImplementationNone);
}

TEST_F(GLInitializerOhosTest, InitializeStaticGLBindings_SwitchBranches) {
  SetGLImplementation(kGLImplementationNone);

  GLImplementationParts mock_impl(kGLImplementationMockGL);
  bool result1 = InitializeStaticGLBindings(mock_impl);
  EXPECT_TRUE(result1);

  SetGLImplementation(kGLImplementationNone);

  GLImplementationParts stub_impl(kGLImplementationStubGL);
  bool result2 = InitializeStaticGLBindings(stub_impl);
  EXPECT_TRUE(result2);

  SetGLImplementation(kGLImplementationNone);
}

}  // namespace init
}  // namespace gl