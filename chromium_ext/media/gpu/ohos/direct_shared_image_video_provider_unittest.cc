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
// clang-format off
#define private public
#define protected public
#include "arkweb/chromium_ext/media/gpu/ohos/direct_shared_image_video_provider.h"
#undef protected
#undef private
// clang-format on
#include "arkweb/chromium_ext/media/gpu/ohos/frame_info_helper.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class GpuSharedImageVideoFactoryTest : public testing::Test {
 protected:
  void SetUp() override {
    auto get_stub_cb = base::BindRepeating(
        []() -> gpu::CommandBufferStub* { return nullptr; });
    factory_ = new GpuSharedImageVideoFactory(get_stub_cb);
  }

  void TearDown() override { delete factory_; }

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
  factory_->CreateImage(base::BindOnce(
                            [](bool* callback_called,
                               SharedImageVideoProvider::ImageRecord record) {
                              *callback_called = true;
                            },
                            &callback_called),
                        spec, drdc_lock);
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
  factory_->CreateImage(base::BindOnce(
                            [](bool* callback_called,
                               SharedImageVideoProvider::ImageRecord record) {
                              *callback_called = true;
                            },
                            &callback_called),
                        spec, drdc_lock);

  EXPECT_FALSE(callback_called);
}

TEST_F(GpuSharedImageVideoFactoryTest, OnWillDestroyStub_clears_stub) {
  factory_->stub_ = reinterpret_cast<gpu::CommandBufferStub*>(0x1);
  factory_->OnWillDestroyStub(true);
  EXPECT_EQ(factory_->stub_, nullptr);
}

// ---- Phase 2 — Additional branch-coverage tests ----

TEST_F(GpuSharedImageVideoFactoryTest,
       Initialize_null_invokes_callback_with_null_state) {
  scoped_refptr<gpu::SharedContextState> received_state;
  bool was_called = false;
  factory_->Initialize(base::BindOnce(
      [](bool* flag, scoped_refptr<gpu::SharedContextState>* out_state,
         scoped_refptr<gpu::SharedContextState> state) {
        *flag = true;
        *out_state = std::move(state);
      },
      &was_called, &received_state));

  ASSERT_TRUE(was_called);
  EXPECT_EQ(received_state, nullptr);
}

TEST_F(GpuSharedImageVideoFactoryTest, OnWillDestroyStub_false_context) {
  factory_->stub_ = reinterpret_cast<gpu::CommandBufferStub*>(0x1);
  factory_->OnWillDestroyStub(false);
  EXPECT_EQ(factory_->stub_, nullptr);
}

class DirectSharedImageVideoProviderTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(DirectSharedImageVideoProviderTest, ConstructorAndDestructor) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();
  auto provider = std::make_unique<DirectSharedImageVideoProvider>(
      task_environment_.GetMainThreadTaskRunner(), get_stub_cb, drdc_lock);
  ASSERT_NE(provider, nullptr);
}

TEST_F(DirectSharedImageVideoProviderTest, Initialize) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();
  auto provider = std::make_unique<DirectSharedImageVideoProvider>(
      task_environment_.GetMainThreadTaskRunner(), get_stub_cb, drdc_lock);

  bool was_called = false;
  provider->Initialize(base::BindOnce(
      [](bool* flag, scoped_refptr<gpu::SharedContextState> state) {
        *flag = true;
      },
      &was_called));

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(was_called);
}

TEST_F(DirectSharedImageVideoProviderTest, RequestImage) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();
  auto provider = std::make_unique<DirectSharedImageVideoProvider>(
      task_environment_.GetMainThreadTaskRunner(), get_stub_cb, drdc_lock);

  SharedImageVideoProvider::ImageSpec spec;
  spec.coded_size = gfx::Size(100, 100);
  bool callback_called = false;
  provider->RequestImage(
      base::BindOnce(
          [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
          },
          &callback_called),
      spec);

  task_environment_.RunUntilIdle();
  // With null stub, CreateImage returns early without invoking callback.
  EXPECT_FALSE(callback_called);
}

// ---- Phase 3 — Additional branch-coverage tests ----

// Verify that Initialize can be called multiple times without crashing.
TEST_F(DirectSharedImageVideoProviderTest, Initialize_MultipleCalls) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();
  auto provider = std::make_unique<DirectSharedImageVideoProvider>(
      task_environment_.GetMainThreadTaskRunner(), get_stub_cb, drdc_lock);

  int call_count = 0;
  auto cb = base::BindOnce(
      [](int* count, scoped_refptr<gpu::SharedContextState> state) {
        (*count)++;
      },
      &call_count);
  provider->Initialize(std::move(cb));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(call_count, 1);

  // Second Initialize call.
  auto cb2 = base::BindOnce(
      [](int* count, scoped_refptr<gpu::SharedContextState> state) {
        (*count)++;
      },
      &call_count);
  provider->Initialize(std::move(cb2));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(call_count, 2);
}

// Verify that RequestImage can be called multiple times with different specs.
TEST_F(DirectSharedImageVideoProviderTest, RequestImage_MultipleCalls) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();
  auto provider = std::make_unique<DirectSharedImageVideoProvider>(
      task_environment_.GetMainThreadTaskRunner(), get_stub_cb, drdc_lock);

  int callback_count = 0;
  for (int i = 0; i < 3; i++) {
    SharedImageVideoProvider::ImageSpec spec;
    spec.coded_size = gfx::Size(100 + i, 100 + i);
    provider->RequestImage(
        base::BindOnce(
            [](int* count, SharedImageVideoProvider::ImageRecord record) {
              (*count)++;
            },
            &callback_count),
        spec);
  }
  task_environment_.RunUntilIdle();
  // With null stub, none of the callbacks should be invoked.
  EXPECT_EQ(callback_count, 0);
}

// Verify that GetDrDcLock returns the lock passed to the constructor.
TEST_F(DirectSharedImageVideoProviderTest, GetDrDcLock_ReturnsProvidedLock) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();
  auto provider = std::make_unique<DirectSharedImageVideoProvider>(
      task_environment_.GetMainThreadTaskRunner(), get_stub_cb, drdc_lock);
  EXPECT_EQ(provider->GetDrDcLock().get(), drdc_lock.get());
}

// Verify that GpuSharedImageVideoFactory::CreateImage does not invoke the
// callback when stub_ is null, even with a valid spec and lock.
TEST_F(GpuSharedImageVideoFactoryTest,
       CreateImage_null_stub_with_valid_spec_no_callback) {
  SharedImageVideoProvider::ImageSpec spec;
  spec.coded_size = gfx::Size(1920, 1080);
  spec.color_space = gfx::ColorSpace::CreateSRGB();
  spec.generation_id = 42u;
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();

  bool callback_called = false;
  factory_->CreateImage(
      base::BindOnce(
          [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
          },
          &callback_called),
      spec, drdc_lock);

  EXPECT_FALSE(callback_called);
  EXPECT_EQ(factory_->stub_, nullptr);
}

// Verify that CreateImageInternal returns false when stub_ is null even with
// a non-zero mailbox.
TEST_F(GpuSharedImageVideoFactoryTest,
       CreateImageInternal_null_stub_with_nonzero_mailbox) {
  SharedImageVideoProvider::ImageSpec spec;
  spec.coded_size = gfx::Size(16, 16);
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  auto image = base::MakeRefCounted<CodecImage>(spec.coded_size, drdc_lock);

  EXPECT_FALSE(factory_->CreateImageInternal(spec, mailbox, image, drdc_lock));
}

// Verify that OnWillDestroyStub can be called multiple times safely (second
// call is a no-op since stub_ is already null).
TEST_F(GpuSharedImageVideoFactoryTest, OnWillDestroyStub_multiple_calls) {
  factory_->stub_ = reinterpret_cast<gpu::CommandBufferStub*>(0x1);
  factory_->OnWillDestroyStub(true);
  EXPECT_EQ(factory_->stub_, nullptr);
  // Second call should be safe even though stub_ is already null.
  factory_->OnWillDestroyStub(false);
  EXPECT_EQ(factory_->stub_, nullptr);
}

// Verify that the provider's destructor does not crash when there are pending
// async operations.
TEST_F(DirectSharedImageVideoProviderTest,
       DestructorWithPendingOperations_NoCrash) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  scoped_refptr<gpu::RefCountedLock> drdc_lock =
      base::MakeRefCounted<gpu::RefCountedLock>();
  auto provider = std::make_unique<DirectSharedImageVideoProvider>(
      task_environment_.GetMainThreadTaskRunner(), get_stub_cb, drdc_lock);

  // Post an Initialize without running the task.
  bool was_called = false;
  provider->Initialize(base::BindOnce(
      [](bool* flag, scoped_refptr<gpu::SharedContextState> state) {
        *flag = true;
      },
      &was_called));

  // Destroy the provider before the async task runs.
  provider.reset();
  task_environment_.RunUntilIdle();
  // The callback may or may not be called depending on SequenceBound teardown,
  // but the key assertion is that no crash occurs.
}
}  // namespace media
