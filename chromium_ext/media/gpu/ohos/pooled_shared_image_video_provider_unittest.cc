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

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
// clang-format off
#define private public
#define protected public
#include "arkweb/chromium_ext/media/gpu/ohos/pooled_shared_image_video_provider.h"
#undef protected
#undef private
// clang-format on
#include "arkweb/chromium_ext/media/gpu/ohos/codec_image.h"
#include "arkweb/chromium_ext/media/gpu/ohos/shared_image_video_provider.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Fake SharedImageVideoProvider that records requests and lets the test
// decide when (and whether) to deliver an ImageRecord.
class FakeSharedImageVideoProvider : public SharedImageVideoProvider {
 public:
  FakeSharedImageVideoProvider() = default;

  FakeSharedImageVideoProvider(const FakeSharedImageVideoProvider&) = delete;
  FakeSharedImageVideoProvider& operator=(const FakeSharedImageVideoProvider&) =
      delete;

  ~FakeSharedImageVideoProvider() override = default;

  size_t initialize_count = 0;
  bool initialize_was_called = false;
  GpuInitCB saved_init_cb;

  struct PendingRequest {
    PendingRequest(ImageReadyCB cb, ImageSpec spec)
        : cb(std::move(cb)), spec(std::move(spec)) {}
    ~PendingRequest() = default;
    ImageReadyCB cb;
    ImageSpec spec;
  };
  std::list<PendingRequest> requests;

  void Initialize(GpuInitCB gpu_init_cb) override {
    initialize_was_called = true;
    initialize_count++;
    saved_init_cb = std::move(gpu_init_cb);
  }

  void RequestImage(ImageReadyCB cb, const ImageSpec& spec) override {
    requests.emplace_back(std::move(cb), spec);
  }

  // Deliver a manufactured ImageRecord for the oldest outstanding request,
  // using |holder| as the codec_image_holder.
  void DeliverOne(scoped_refptr<CodecImageHolder> holder) {
    ASSERT_FALSE(requests.empty());
    auto& front = requests.front();
    ImageRecord record;
    record.is_vulkan = false;
    record.codec_image_holder = std::move(holder);
    record.release_cb = base::DoNothing();
    std::move(front.cb).Run(std::move(record));
    requests.pop_front();
  }
};

// Fake GpuHelper that captures OnImageReturned arguments and immediately
// posts the continuation back to the current task runner, so the pool can
// re-process the returned image.
//
class FakeGpuHelper : public PooledSharedImageVideoProvider::GpuHelper {
 public:
  FakeGpuHelper() {
    // Register this instance so the test can access it after construction.
    // Only one FakeGpuHelper should exist at a time per test.
    DCHECK(!current_instance_);
    current_instance_ = this;
  }

  FakeGpuHelper(const FakeGpuHelper&) = delete;
  FakeGpuHelper& operator=(const FakeGpuHelper&) = delete;

  ~FakeGpuHelper() override {
    if (current_instance_ == this) {
      current_instance_ = nullptr;
    }
  }

  // Static accessor used by the test fixture to read recorded state.  The
  // instance is constructed asynchronously by SequenceBound, so the test must
  // call |RunUntilIdle()| before using this.
  static FakeGpuHelper* current_instance() { return current_instance_; }

  static void ResetCurrentInstance() { current_instance_ = nullptr; }

  int call_count = 0;
  gpu::SyncToken last_sync_token;
  scoped_refptr<CodecImageHolder> last_holder;
  scoped_refptr<gpu::RefCountedLock> last_drdc_lock;

  void OnImageReturned(const gpu::SyncToken& sync_token,
                       scoped_refptr<CodecImageHolder> codec_image_holder,
                       base::OnceClosure cb,
                       scoped_refptr<gpu::RefCountedLock> drdc_lock) override {
    call_count++;
    last_sync_token = sync_token;
    last_holder = codec_image_holder;
    last_drdc_lock = drdc_lock;
    // Immediately post |cb| back to the current sequence to mirror the real
    // behaviour of waiting for the sync token then notifying the pool.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             std::move(cb));
  }

 private:
  static FakeGpuHelper* current_instance_;
};

FakeGpuHelper* FakeGpuHelper::current_instance_ = nullptr;

scoped_refptr<CodecImageHolder> MakeHolder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  auto codec_image =
      base::MakeRefCounted<CodecImage>(gfx::Size(100, 100), drdc_lock);
  return base::MakeRefCounted<CodecImageHolder>(
      std::move(task_runner), std::move(codec_image), drdc_lock);
}

}  // namespace

class PooledSharedImageVideoProviderTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  scoped_refptr<gpu::RefCountedLock> drdc_lock_ =
      base::MakeRefCounted<gpu::RefCountedLock>();

  // Creates a PooledSharedImageVideoProvider backed by a FakeGpuHelper and a
  // FakeSharedImageVideoProvider.  The FakeGpuHelper is constructed on the main
  // thread task runner by SequenceBound<FakeGpuHelper> (GpuHelper is abstract,
  // so we cannot construct SequenceBound<GpuHelper> directly).  The instance
  // registers itself in FakeGpuHelper::current_instance() on construction;
  // callers must call RunUntilIdle() before inspecting |helper_ptr_|.
  std::unique_ptr<PooledSharedImageVideoProvider> CreateProviderWithFakeHelper(
      std::unique_ptr<FakeSharedImageVideoProvider> provider) {
    FakeGpuHelper::ResetCurrentInstance();
    auto* provider_ptr = provider.get();
    auto p = std::unique_ptr<PooledSharedImageVideoProvider>(
        new PooledSharedImageVideoProvider(
            base::SequenceBound<FakeGpuHelper>(
                task_environment_.GetMainThreadTaskRunner()),
            std::move(provider), drdc_lock_));
    // The FakeGpuHelper is constructed asynchronously on the main thread;
    // flush pending tasks so |helper_ptr_| is valid before the test continues.
    task_environment_.RunUntilIdle();
    helper_ptr_ = FakeGpuHelper::current_instance();
    provider_ptr_ = provider_ptr;
    return p;
  }

  FakeGpuHelper* helper_ptr_ = nullptr;
  FakeSharedImageVideoProvider* provider_ptr_ = nullptr;
};

// ---- Static factory ----

TEST_F(PooledSharedImageVideoProviderTest, Create_ReturnsNonNull) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  auto inner_provider = std::make_unique<FakeSharedImageVideoProvider>();
  auto provider = PooledSharedImageVideoProvider::Create(
      task_environment_.GetMainThreadTaskRunner(), get_stub_cb,
      std::move(inner_provider), drdc_lock_);
  ASSERT_NE(provider, nullptr);
}

// ---- Initialize forwards to the underlying provider ----

TEST_F(PooledSharedImageVideoProviderTest, Initialize_ForwardsToProvider) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  bool init_called = false;
  provider->Initialize(base::BindOnce(
      [](bool* flag, scoped_refptr<gpu::SharedContextState> state) {
        *flag = true;
      },
      &init_called));
  EXPECT_TRUE(provider_ptr_->initialize_was_called);
  EXPECT_EQ(provider_ptr_->initialize_count, 1u);
  // The pooled provider does not invoke the callback itself; it forwards it
  // to the underlying provider, which holds it.
  EXPECT_FALSE(init_called);
  // Running the saved callback should not crash.
  std::move(provider_ptr_->saved_init_cb).Run(nullptr);
  EXPECT_TRUE(init_called);
}

// ---- RequestImage with empty pool forwards to provider ----

TEST_F(PooledSharedImageVideoProviderTest,
       RequestImage_EmptyPool_ForwardsToProvider) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(100, 100), 1u);
  bool cb_called = false;
  provider->RequestImage(
      base::BindOnce(
          [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
          },
          &cb_called),
      spec);
  // Pool was empty, so the request should have been forwarded.
  ASSERT_EQ(provider_ptr_->requests.size(), 1u);
  EXPECT_FALSE(cb_called);
  // pool_spec_ should match the requested spec.
  EXPECT_TRUE(provider->pool_spec_ == spec);
  EXPECT_TRUE(provider->pool_.empty());
}

// ---- RequestImage with mismatched pool spec clears the pool ----

TEST_F(PooledSharedImageVideoProviderTest,
       RequestImage_PoolSpecMismatch_ClearsPool) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());

  // Seed the pool with an image that matches the old spec.
  SharedImageVideoProvider::ImageSpec old_spec(gfx::Size(100, 100), 1u);
  provider->pool_spec_ = old_spec;
  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  auto pooled_image =
      base::MakeRefCounted<PooledSharedImageVideoProvider::PooledImage>(
          old_spec, SharedImageVideoProvider::ImageRecord());
  pooled_image->record.codec_image_holder = holder;
  provider->pool_.push_front(pooled_image);
  ASSERT_EQ(provider->pool_.size(), 1u);

  // Request with a different spec (different generation id).
  SharedImageVideoProvider::ImageSpec new_spec(gfx::Size(100, 100), 2u);
  bool cb_called = false;
  provider->RequestImage(
      base::BindOnce(
          [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
          },
          &cb_called),
      new_spec);

  // Pool should have been cleared and the request forwarded to the provider.
  EXPECT_TRUE(provider->pool_.empty());
  EXPECT_TRUE(provider->pool_spec_ == new_spec);
  ASSERT_EQ(provider_ptr_->requests.size(), 1u);
}

// ---- RequestImage with matching pool image reuses it ----

TEST_F(PooledSharedImageVideoProviderTest,
       RequestImage_MatchingPoolImage_ReusesIt) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());

  SharedImageVideoProvider::ImageSpec spec(gfx::Size(100, 100), 1u);
  provider->pool_spec_ = spec;
  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  auto pooled_image =
      base::MakeRefCounted<PooledSharedImageVideoProvider::PooledImage>(
          spec, SharedImageVideoProvider::ImageRecord());
  pooled_image->record.codec_image_holder = holder;
  provider->pool_.push_front(pooled_image);
  ASSERT_EQ(provider->pool_.size(), 1u);

  bool cb_called = false;
  scoped_refptr<gpu::ClientSharedImage> received_shared_image;
  provider->RequestImage(
      base::BindOnce(
          [](bool* flag, scoped_refptr<gpu::ClientSharedImage>* out,
             SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
            *out = std::move(record.shared_image);
          },
          &cb_called, &received_shared_image),
      spec);

  // The pooled image should have been used, so the callback fires synchronously
  // and the request is not forwarded to the underlying provider.
  EXPECT_TRUE(cb_called);
  EXPECT_TRUE(provider->pool_.empty());
  EXPECT_TRUE(provider_ptr_->requests.empty());
  // The request should have been recorded in pending_requests_ and then
  // immediately fulfilled.
  EXPECT_TRUE(provider->pending_requests_.empty());
}

// ---- OnImageCreated with matching pending request fulfils it ----

TEST_F(PooledSharedImageVideoProviderTest,
       OnImageCreated_MatchingPendingRequest_FulfilsRequest) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(100, 100), 1u);

  bool cb_called = false;
  provider->RequestImage(
      base::BindOnce(
          [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
          },
          &cb_called),
      spec);
  ASSERT_EQ(provider->pending_requests_.size(), 1u);

  // Simulate the underlying provider delivering an image.
  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  SharedImageVideoProvider::ImageRecord record;
  record.codec_image_holder = holder;
  record.release_cb = base::DoNothing();
  provider->OnImageCreated(spec, std::move(record));

  EXPECT_TRUE(cb_called);
  EXPECT_TRUE(provider->pending_requests_.empty());
}

// ---- OnImageCreated with no pending requests pools the image ----

TEST_F(PooledSharedImageVideoProviderTest,
       OnImageCreated_NoPendingRequests_PoolsImage) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(100, 100), 1u);
  provider->pool_spec_ = spec;

  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  SharedImageVideoProvider::ImageRecord record;
  record.codec_image_holder = holder;
  record.release_cb = base::DoNothing();
  provider->OnImageCreated(spec, std::move(record));

  ASSERT_EQ(provider->pool_.size(), 1u);
  EXPECT_TRUE(provider->pool_.front()->spec == spec);
}

// ---- OnImageCreated with no pending requests and mismatched spec drops it

TEST_F(PooledSharedImageVideoProviderTest,
       OnImageCreated_NoPendingRequests_MismatchedSpec_DropsImage) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec pool_spec(gfx::Size(100, 100), 1u);
  SharedImageVideoProvider::ImageSpec other_spec(gfx::Size(100, 100), 2u);
  provider->pool_spec_ = pool_spec;

  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  SharedImageVideoProvider::ImageRecord record;
  record.codec_image_holder = holder;
  record.release_cb = base::DoNothing();
  provider->OnImageCreated(other_spec, std::move(record));

  EXPECT_TRUE(provider->pool_.empty());
}

// ---- OnImageCreated with non-matching pending request drops the image ----

TEST_F(PooledSharedImageVideoProviderTest,
       OnImageCreated_PendingRequestSpecMismatch_DropsImage) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec request_spec(gfx::Size(100, 100), 1u);
  SharedImageVideoProvider::ImageSpec other_spec(gfx::Size(100, 100), 2u);

  bool cb_called = false;
  provider->RequestImage(
      base::BindOnce(
          [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
          },
          &cb_called),
      request_spec);
  ASSERT_EQ(provider->pending_requests_.size(), 1u);

  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  SharedImageVideoProvider::ImageRecord record;
  record.codec_image_holder = holder;
  record.release_cb = base::DoNothing();
  provider->OnImageCreated(other_spec, std::move(record));

  // The image should have been discarded; the pending request is still there.
  EXPECT_FALSE(cb_called);
  ASSERT_EQ(provider->pending_requests_.size(), 1u);
  EXPECT_TRUE(provider->pool_.empty());
}

// ---- ProcessFreePooledImage with empty pending and matching spec pools ----

TEST_F(PooledSharedImageVideoProviderTest,
       ProcessFreePooledImage_NoPending_MatchingSpec_AddsToPool) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(100, 100), 1u);
  provider->pool_spec_ = spec;

  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  auto pooled_image =
      base::MakeRefCounted<PooledSharedImageVideoProvider::PooledImage>(
          spec, SharedImageVideoProvider::ImageRecord());
  pooled_image->record.codec_image_holder = holder;
  provider->ProcessFreePooledImage(pooled_image);

  ASSERT_EQ(provider->pool_.size(), 1u);
}

// ---- ProcessFreePooledImage with empty pending and mismatched spec drops --

TEST_F(PooledSharedImageVideoProviderTest,
       ProcessFreePooledImage_NoPending_MismatchedSpec_DropsImage) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec pool_spec(gfx::Size(100, 100), 1u);
  SharedImageVideoProvider::ImageSpec other_spec(gfx::Size(100, 100), 2u);
  provider->pool_spec_ = pool_spec;

  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  auto pooled_image =
      base::MakeRefCounted<PooledSharedImageVideoProvider::PooledImage>(
          other_spec, SharedImageVideoProvider::ImageRecord());
  pooled_image->record.codec_image_holder = holder;
  provider->ProcessFreePooledImage(pooled_image);

  EXPECT_TRUE(provider->pool_.empty());
}

// ---- OnImageReturned routes through the GpuHelper ----

TEST_F(PooledSharedImageVideoProviderTest,
       OnImageReturned_RoutesThroughGpuHelper) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(100, 100), 1u);
  provider->pool_spec_ = spec;

  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  auto pooled_image =
      base::MakeRefCounted<PooledSharedImageVideoProvider::PooledImage>(
          spec, SharedImageVideoProvider::ImageRecord());
  pooled_image->record.codec_image_holder = holder;

  gpu::SyncToken sync_token;
  provider->OnImageReturned(pooled_image, sync_token);
  // The AsyncCall posts to the gpu task runner (the main thread here), so we
  // need to run pending tasks before the fake helper records the call.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(helper_ptr_->call_count, 1);
  EXPECT_TRUE(helper_ptr_->last_sync_token == sync_token);
  EXPECT_NE(helper_ptr_->last_holder, nullptr);
  EXPECT_EQ(helper_ptr_->last_drdc_lock.get(), drdc_lock_.get());

  // The fake helper posts |cb| back to the current sequence, which will call
  // ProcessFreePooledImage. With no pending requests and a matching spec, the
  // image should end up in the pool.
  task_environment_.RunUntilIdle();
  ASSERT_EQ(provider->pool_.size(), 1u);
}

// ---- GpuHelperImpl with null stub does not create a helper ----

TEST_F(PooledSharedImageVideoProviderTest,
       GpuHelperImpl_NullStub_NoCommandBufferHelper) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  PooledSharedImageVideoProvider::GpuHelperImpl helper(get_stub_cb);
  EXPECT_EQ(helper.command_buffer_helper_, nullptr);
}

// ---- GpuHelperImpl::OnSyncTokenCleared with null lock runs cb ----

TEST_F(PooledSharedImageVideoProviderTest,
       GpuHelperImpl_OnSyncTokenCleared_NullLock_RunsCallback) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  PooledSharedImageVideoProvider::GpuHelperImpl helper(get_stub_cb);

  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), nullptr);
  bool cb_called = false;
  helper.OnSyncTokenCleared(
      holder, base::BindOnce([](bool* flag) { *flag = true; }, &cb_called),
      nullptr);
  EXPECT_TRUE(cb_called);
}

// ---- GpuHelperImpl::OnSyncTokenCleared with real lock runs cb ----

TEST_F(PooledSharedImageVideoProviderTest,
       GpuHelperImpl_OnSyncTokenCleared_WithLock_RunsCallback) {
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  PooledSharedImageVideoProvider::GpuHelperImpl helper(get_stub_cb);

  auto drdc_lock = base::MakeRefCounted<gpu::RefCountedLock>();
  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock);
  bool cb_called = false;
  helper.OnSyncTokenCleared(
      holder, base::BindOnce([](bool* flag) { *flag = true; }, &cb_called),
      drdc_lock);
  EXPECT_TRUE(cb_called);
}

// ---- PooledImage and PendingRequest constructors/destructors ----

TEST_F(PooledSharedImageVideoProviderTest, PooledImage_ConstructAndDestruct) {
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(100, 100), 1u);
  SharedImageVideoProvider::ImageRecord record;
  auto pooled_image =
      base::MakeRefCounted<PooledSharedImageVideoProvider::PooledImage>(
          spec, std::move(record));
  EXPECT_TRUE(pooled_image->spec == spec);
}

TEST_F(PooledSharedImageVideoProviderTest,
       PendingRequest_ConstructAndDestruct) {
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(100, 100), 1u);
  bool cb_called = false;
  PooledSharedImageVideoProvider::PendingRequest request(
      spec, base::BindOnce(
                [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
                  *flag = true;
                },
                &cb_called));
  EXPECT_TRUE(request.spec == spec);
  EXPECT_FALSE(cb_called);
  // Run the callback to release it.
  SharedImageVideoProvider::ImageRecord record;
  std::move(request.cb).Run(std::move(record));
  EXPECT_TRUE(cb_called);
}

// ---- Constructor and destructor smoke test ----

TEST_F(PooledSharedImageVideoProviderTest, ConstructorAndDestructor_SmokeTest) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  ASSERT_NE(provider, nullptr);
  provider.reset();
}

// ---- RequestImage spec change then matching pool reuse path ----

TEST_F(PooledSharedImageVideoProviderTest,
       RequestImage_SpecChange_FromSpecAToSpecB) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec spec_a(gfx::Size(100, 100), 1u);
  SharedImageVideoProvider::ImageSpec spec_b(gfx::Size(100, 100), 2u);

  // First request with spec_a.
  bool cb1_called = false;
  provider->RequestImage(
      base::BindOnce(
          [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
          },
          &cb1_called),
      spec_a);
  ASSERT_EQ(provider_ptr_->requests.size(), 1u);
  EXPECT_TRUE(provider->pool_spec_ == spec_a);

  // Second request with spec_b: pool_spec_ differs, so pool is cleared and
  // request forwarded.
  bool cb2_called = false;
  provider->RequestImage(
      base::BindOnce(
          [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
          },
          &cb2_called),
      spec_b);
  EXPECT_TRUE(provider->pool_spec_ == spec_b);
  ASSERT_EQ(provider_ptr_->requests.size(), 2u);
}

// ---- OnImageCreated with matching spec but no pending request pools it --

TEST_F(PooledSharedImageVideoProviderTest,
       OnImageCreated_MatchingSpec_NoPending_PoolsAndThenReuses) {
  auto provider = CreateProviderWithFakeHelper(
      std::make_unique<FakeSharedImageVideoProvider>());
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(100, 100), 1u);
  provider->pool_spec_ = spec;

  // Deliver an image with no pending request; it should be pooled.
  auto holder =
      MakeHolder(task_environment_.GetMainThreadTaskRunner(), drdc_lock_);
  SharedImageVideoProvider::ImageRecord record;
  record.codec_image_holder = holder;
  record.release_cb = base::DoNothing();
  provider->OnImageCreated(spec, std::move(record));
  ASSERT_EQ(provider->pool_.size(), 1u);

  // Now request with the same spec; the pooled image should be reused.
  bool cb_called = false;
  provider->RequestImage(
      base::BindOnce(
          [](bool* flag, SharedImageVideoProvider::ImageRecord record) {
            *flag = true;
          },
          &cb_called),
      spec);
  EXPECT_TRUE(cb_called);
  EXPECT_TRUE(provider->pool_.empty());
}

}  // namespace media
