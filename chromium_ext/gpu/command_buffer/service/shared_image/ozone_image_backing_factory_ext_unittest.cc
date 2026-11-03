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

#include "base/containers/flat_map.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing_factory_ext.h"

#include "base/test/task_environment.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gpu {

class OzoneImageBackingFactoryExtTest : public SharedImageTestBase {
 public:
  OzoneImageBackingFactoryExtTest() = default;
  ~OzoneImageBackingFactoryExtTest() override = default;

  void SetUp() override {
    // SharedImageTestBase::InitializeContext() expects GL bindings to be
    // already initialized: with the default multi-process test launcher some
    // earlier fixture in the binary usually did that, but to stay independent
    // of test ordering (and to work when running with --single-process-tests)
    // initialize stub bindings explicitly here.
    gl_display_ = gl::GLSurfaceTestSupport::InitializeOneOffWithStubBindings();
    ASSERT_TRUE(gl_display_);
    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGL));
    factory_ = std::make_unique<OzoneImageBackingFactoryExt>(
        context_state_, gpu_workarounds_, gpu_preferences_);
    ASSERT_TRUE(factory_);
  }

  void TearDown() override {
    factory_.reset();
    gl::GLSurfaceTestSupport::ShutdownGL(gl_display_);
  }

  raw_ptr<gl::GLDisplay> gl_display_ = nullptr;
  std::unique_ptr<OzoneImageBackingFactoryExt> factory_;
};

// Test that the ext factory returns itself from AsOzoneImageBackingFactoryExt.
TEST_F(OzoneImageBackingFactoryExtTest, AsOzoneImageBackingFactoryExt_ReturnsExt) {
  EXPECT_EQ(factory_->AsOzoneImageBackingFactoryExt(), factory_.get());
}

#if BUILDFLAG(ARKWEB_HEIF_SUPPORT)
// Test CreateSharedImage with a pixmap handle: on the headless ozone
// platform an empty pixmap handle still produces a NativePixmapDmaBuf, so a
// backing must be created and reported cleared.
TEST_F(OzoneImageBackingFactoryExtTest, CreateSharedImage_NativePixmap_ReturnsBacking) {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;

  auto backing = factory_->CreateSharedImage(
      Mailbox::Generate(), std::move(handle), gfx::BufferFormat::RGBA_8888,
      gfx::BufferPlane::DEFAULT, gfx::Size(64, 64),
      gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_DISPLAY_READ, nullptr);
  EXPECT_NE(backing, nullptr);
  EXPECT_TRUE(backing->IsCleared());
}
#endif  // BUILDFLAG(ARKWEB_HEIF_SUPPORT)

}  // namespace gpu
