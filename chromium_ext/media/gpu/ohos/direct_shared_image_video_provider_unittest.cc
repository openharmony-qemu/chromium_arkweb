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

#include <memory>

#include "base/functional/bind.h"
#define private public
#define protected public
#include "arkweb/chromium_ext/media/gpu/ohos/direct_shared_image_video_provider.h"
#undef private
#undef protected
#include "arkweb/chromium_ext/media/gpu/ohos/frame_info_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class GpuSharedImageVideoFactoryTest : public testing::Test {
 protected:
  void SetUp() override {
    auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
    factory_ = new GpuSharedImageVideoFactory(get_stub_cb);
  }

  void TearDown() override {
        delete factory_;
    }

  GpuSharedImageVideoFactory* factory_;
};

TEST_F(GpuSharedImageVideoFactoryTest, Initialize) {
  bool was_called = false;
  factory_->Initialize(base::BindOnce(
    [](bool* flag, scoped_refptr<gpu::SharedContextState> state) {
        *flag = true;
    },
    &was_called));

  ASSERT_NE(factory_, nullptr);
}

TEST_F(GpuSharedImageVideoFactoryTest, CreateImage) {
  SharedImageVideoProvider::ImageSpec spec;
  spec.coded_size = gfx::Size(100, 100);
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
    base::MakeRefCounted<gpu::RefCountedLock>();

  bool callback_called = false;
  factory_->CreateImage(
    base::BindOnce(
        [](bool* callback_called, SharedImageVideoProvider::ImageRecord record) {
            *callback_called = true;
        },
        &callback_called
    ),
    spec,
    drdc_lock);
  ASSERT_NE(factory_, nullptr);
}

TEST_F(GpuSharedImageVideoFactoryTest, Constructor_default_state) {
  EXPECT_EQ(factory_->stub_, nullptr);
  EXPECT_FALSE(factory_->is_vulkan_);
}

TEST_F(GpuSharedImageVideoFactoryTest, CreateImageInternal_null_stub) {
  SharedImageVideoProvider::ImageSpec spec;
  spec.coded_size = gfx::Size(100, 100);
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();
  gpu::Mailbox mailbox;
  auto image = base::MakeRefCounted<CodecImage>(spec.coded_size, drdc_lock);

  EXPECT_FALSE(factory_->CreateImageInternal(spec, mailbox, image, drdc_lock));
}

TEST_F(GpuSharedImageVideoFactoryTest, Initialize_null_invokes_callback) {
  bool was_called = false;
  factory_->Initialize(base::BindOnce(
      [](bool* flag, scoped_refptr<gpu::SharedContextState> state) {
        *flag = true;
      },
      &was_called));

  ASSERT_TRUE(was_called);
}

TEST_F(GpuSharedImageVideoFactoryTest, CreateImage_null_skips_callback) {
  SharedImageVideoProvider::ImageSpec spec;
  spec.coded_size = gfx::Size(100, 100);
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();

  bool callback_called = false;
  factory_->CreateImage(
      base::BindOnce(
          [](bool* callback_called,
             SharedImageVideoProvider::ImageRecord record) {
            *callback_called = true;
          },
          &callback_called),
      spec,
      drdc_lock);

  EXPECT_FALSE(callback_called);
}

TEST_F(GpuSharedImageVideoFactoryTest, OnWillDestroyStub_clears_stub) {
  factory_->stub_ = reinterpret_cast<gpu::CommandBufferStub*>(0x1);
  factory_->OnWillDestroyStub(true);
  EXPECT_EQ(factory_->stub_, nullptr);
}
}