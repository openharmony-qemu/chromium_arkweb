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

#include "gtest/gtest.h"
#include "base/test/task_environment.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

class ImageTransportSurfaceOHOSTest : public GpuChannelTestCommon {
 public:
  ImageTransportSurfaceOHOSTest()
      : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~ImageTransportSurfaceOHOSTest() override = default;

  void SetUp() override {
    channel_ = CreateChannel(kDefaultClientId, true /* is_gpu_host */);
    ASSERT_TRUE(channel_);
    // Do NOT call GetSharedContextState() here: under stub bindings it would
    // still drive the full SharedContextState + ArkWeb SharedImage factory
    // initialization path, which is not supported in the unittest harness and
    // crashes (Signal 5). Both tests below hit the kGLImplementationStubGL
    // early-return branches of CreateNativeGLSurface / CreatePresenter, which
    // never dereference the context state, so a null context_state_ is safe.
  }

  void TearDown() override { channel_ = nullptr; }

  static constexpr int32_t kDefaultClientId = 1;
  GpuChannel* channel_ = nullptr;
};

// Test CreateNativeGLSurface: with stub bindings gl::init::CreateViewGLSurface
// returns a stub view which must not be null.
TEST_F(ImageTransportSurfaceOHOSTest, CreateNativeGLSurface_ReturnsSurface) {
  SurfaceHandle surface_handle = 1;
  auto surface = ImageTransportSurface::CreateNativeGLSurface(
      gl::GetDefaultDisplayEGL(), surface_handle, gl::GLSurfaceFormat());
  EXPECT_NE(surface, nullptr);
}

// Test CreatePresenter: with stub bindings no GLOzone is installed, so the
// presenter cannot be created and null is returned. The call must
// not crash.
TEST_F(ImageTransportSurfaceOHOSTest, CreatePresenter_NoGLOzone_ReturnsNull) {
  SurfaceHandle surface_handle = 1;
  auto presenter = ImageTransportSurface::CreatePresenter(
      /*context_state=*/nullptr, GpuDriverBugWorkarounds(), GpuFeatureInfo(),
      surface_handle);
  EXPECT_EQ(presenter, nullptr);
}

}  // namespace gpu
