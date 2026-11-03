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

#include <cstdint>
#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

#define private public
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/shared_image_stub.h"
#undef private

#include "arkweb/chromium_ext/gpu/command_buffer/service/shared_image/shared_image_factory_ext.h"

namespace gpu {

// Defined in
// arkweb/chromium_ext/gpu/command_buffer/service/shared_image/shared_image_factory_ext.cc
// (no public header declares it).
const char* GmbTypeToString(gfx::GpuMemoryBufferType type);

namespace {

class FakeBackingFactoryExt : public SharedImageBackingFactory {
 public:
  FakeBackingFactoryExt() : SharedImageBackingFactory(kUsageAll) {}

  using SharedImageBackingFactory::CreateSharedImage;

  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      void* window_buffer) override {
    auto si_format = viz::GetSharedImageFormat(format);
    return std::make_unique<TestImageBacking>(
        mailbox, si_format, size, color_space, surface_origin, alpha_type,
        SharedImageUsageSet(usage), si_format.EstimatedSizeInBytes(size));
  }

  SharedImageBackingType GetBackingType() override {
    return SharedImageBackingType::kTest;
  }

 protected:
  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override {
    return true;
  }
};

}  // namespace

class SharedImageFactoryExtTest : public GpuChannelTestCommon {
 public:
  SharedImageFactoryExtTest() : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~SharedImageFactoryExtTest() override = default;

  void SetUp() override {
    channel_ = CreateChannel(kDefaultClientId, true /* is_gpu_host */);
    ASSERT_TRUE(channel_);
    ASSERT_TRUE(channel_->shared_image_stub_);
    factory_ext_ = channel_->shared_image_stub_->factory()
                       ->AsSharedImageFactoryExt();
    ASSERT_TRUE(factory_ext_);
  }

  void TearDown() override {
    if (channel_ && channel_->shared_image_stub_) {
      // Reset the testing factory so later tests see the default state.
      channel_->shared_image_stub_->factory()
          ->RegisterSharedImageBackingFactoryForTesting(nullptr);
    }
    factory_ext_ = nullptr;
    channel_ = nullptr;
  }

  static constexpr int32_t kDefaultClientId = 1;
  GpuChannel* channel_ = nullptr;
  SharedImageFactoryExt* factory_ext_ = nullptr;
};

// Test GmbTypeToString for every GpuMemoryBufferType value of this build.
TEST_F(SharedImageFactoryExtTest, GmbTypeToString_AllTypes) {
  EXPECT_STREQ(GmbTypeToString(gfx::EMPTY_BUFFER), "empty");
  EXPECT_STREQ(GmbTypeToString(gfx::SHARED_MEMORY_BUFFER), "shared_memory");
  EXPECT_STREQ(GmbTypeToString(gfx::NATIVE_PIXMAP), "platform");
#if BUILDFLAG(ARKWEB_VULKAN)
  EXPECT_STREQ(GmbTypeToString(gfx::OHOS_NATIVE_BUFFER), "platform");
#endif
}

// Test that the factory created by the stub is a SharedImageFactoryExt.
TEST_F(SharedImageFactoryExtTest, AsSharedImageFactoryExt_ReturnsExt) {
  EXPECT_EQ(channel_->shared_image_stub_->factory()->AsSharedImageFactoryExt(),
            factory_ext_);
}

#if BUILDFLAG(ARKWEB_HEIF_SUPPORT)
// Test CreateSharedImage when no backing factory supports the requested
// params (shared memory handle + display usage): returns false.
TEST_F(SharedImageFactoryExtTest, CreateSharedImage_NoFactoryFound_ReturnsFalse) {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;

  bool ret = factory_ext_->CreateSharedImage(
      Mailbox::Generate(), std::move(handle), gfx::BufferFormat::RGBA_8888,
      gfx::BufferPlane::DEFAULT, gfx::Size(10, 10),
      gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_DISPLAY_READ, nullptr);
  EXPECT_FALSE(ret);
}

// Test CreateSharedImage when a backing factory is registered for testing:
// a backing is created and registered successfully.
TEST_F(SharedImageFactoryExtTest, CreateSharedImage_WithBacking_ReturnsTrue) {
  FakeBackingFactoryExt factory;
  channel_->shared_image_stub_->factory()
      ->RegisterSharedImageBackingFactoryForTesting(&factory);

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;

  bool ret = factory_ext_->CreateSharedImage(
      Mailbox::Generate(), std::move(handle), gfx::BufferFormat::RGBA_8888,
      gfx::BufferPlane::DEFAULT, gfx::Size(10, 10),
      gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_DISPLAY_READ, nullptr);
  EXPECT_TRUE(ret);
}
#endif  // BUILDFLAG(ARKWEB_HEIF_SUPPORT)

}  // namespace gpu
