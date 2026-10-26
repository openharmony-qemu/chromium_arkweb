// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on stream_texture_wrapper_impl.cc originally written by
// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos_sdk/openharmony/native/llvm/bin/../include/libcxx-ohos/include/c++/v1/__ranges/lazy_split_view.h"
#define private public
#include "content/renderer/media/ohos/native_texture_factory.h"
#include "content/renderer/media/ohos/native_texture_wrapper_impl.h"
#undef private
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/mock_gpu_channel.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

class MockSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  MOCK_METHOD(bool, BelongsToCurrentThread, (), (const, override));
  MOCK_METHOD(bool,
              DeleteOrReleaseSoonInternal,
              (const base::Location& from_here,
               void (*deleter)(const void*),
               const void* object),
              (override));
  MOCK_METHOD(bool, RunsTasksInCurrentSequence, (), (const, override));
  MOCK_METHOD(bool,
              PostDelayedTaskAt,
              (base::subtle::PostDelayedTaskPassKey,
               const base::Location& from_here,
               base::OnceClosure task,
               base::TimeTicks delayed_run_time,
               base::subtle::DelayPolicy delay_policy),
              (override));
  MOCK_METHOD(base::DelayedTaskHandle,
              PostCancelableDelayedTask,
              (base::subtle::PostDelayedTaskPassKey,
               const base::Location& from_here,
               base::OnceClosure task,
               base::TimeDelta delay),
              (override));
  MOCK_METHOD(bool,
              PostNonNestableDelayedTask,
              (const base::Location& from_here,
               base::OnceClosure task,
               base::TimeDelta delay),
              (override));
  MOCK_METHOD(bool,
              PostDelayedTask,
              (const base::Location& from_here,
               base::OnceClosure task,
               base::TimeDelta delay),
              (override));
};

class MockClientSharedImageInterface : public gpu::ClientSharedImageInterface {
 public:
  MockClientSharedImageInterface(gpu::SharedImageInterfaceProxy* proxy,
                                 scoped_refptr<gpu::GpuChannelHost> channel)
      : gpu::ClientSharedImageInterface(proxy, channel) {};
  // gpu::SharedImageInterface implementation.
  MOCK_METHOD3(CreateSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   const gpu::SharedImageInfo& si_info,
                   gpu::SurfaceHandle surface_handle,
                   std::optional<gpu::SharedImagePoolId>));
  MOCK_METHOD2(CreateSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   const gpu::SharedImageInfo& si_info,
                   base::span<const uint8_t> pixel_data));
  MOCK_METHOD4(CreateSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   const gpu::SharedImageInfo& si_info,
                   gpu::SurfaceHandle surface_handle,
                   gfx::BufferUsage buffer_usage,
                   gfx::GpuMemoryBufferHandle buffer_handle));
  MOCK_METHOD2(CreateSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   const gpu::SharedImageInfo& si_info,
                   gfx::GpuMemoryBufferHandle buffer_handle));
  MOCK_METHOD2(UpdateSharedImage,
               void(const gpu::SyncToken& sync_token,
                    const gpu::Mailbox& mailbox));
  MOCK_METHOD2(DestroySharedImage,
               void(const gpu::SyncToken& sync_token,
                    const gpu::Mailbox& mailbox));
  MOCK_METHOD2(DestroySharedImage,
               void(const gpu::SyncToken& sync_token,
                    scoped_refptr<gpu::ClientSharedImage> client_shared_image));
  MOCK_METHOD1(ImportSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   gpu::ExportedSharedImage exported_shared_image));
  MOCK_METHOD2(PresentSwapChain,
               void(const gpu::SyncToken& sync_token,
                    const gpu::Mailbox& mailbox));
  MOCK_METHOD0(GenUnverifiedSyncToken, gpu::SyncToken());
  MOCK_METHOD0(GenVerifiedSyncToken, gpu::SyncToken());
  MOCK_METHOD1(VerifySyncToken, void(gpu::SyncToken& sync_token));
  MOCK_METHOD1(WaitSyncToken, void(const gpu::SyncToken& sync_token));
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD1(UsageForMailbox,
               gpu::SharedImageUsageSet(const gpu::Mailbox& mailbox));

  // Informs that existing |mailbox| with the specified metadata can be passed
  // to DestroySharedImage().
  MOCK_METHOD7(
      NotifyMailboxAdded1,
      scoped_refptr<gpu::ClientSharedImage>(const gpu::Mailbox& mailbox,
                                            viz::SharedImageFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            GrSurfaceOrigin surface_origin,
                                            SkAlphaType alpha_type,
                                            gpu::SharedImageUsageSet usage));
  MOCK_METHOD1(GetNativePixmap,
               scoped_refptr<gfx::NativePixmap>(const gpu::Mailbox& mailbox));
  MOCK_METHOD8(
      NotifyMailboxAddedWithTarget,
      scoped_refptr<gpu::ClientSharedImage>(const gpu::Mailbox& mailbox,
                                            viz::SharedImageFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            GrSurfaceOrigin surface_origin,
                                            SkAlphaType alpha_type,
                                            gpu::SharedImageUsageSet usage,
                                            uint32_t texture_target));

  virtual scoped_refptr<gpu::ClientSharedImage> NotifyMailboxAdded(
      const gpu::Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      uint32_t texture_target,
      std::string_view debug_label) override {
    return NotifyMailboxAddedWithTarget(mailbox, format, size, color_space,
                                        surface_origin, alpha_type, usage,
                                        texture_target);
  }
  MOCK_METHOD0(GetCapabilities, const gpu::SharedImageCapabilities&());

 protected:
  ~MockClientSharedImageInterface() override = default;
};

namespace content {
class TestGpuChannelHost : public gpu::GpuChannelHost {
 public:
  explicit TestGpuChannelHost(gpu::mojom::GpuChannel& gpu_channel)
      : GpuChannelHost(0,
                       gpu::GPUInfo(),
                       gpu::GpuFeatureInfo(),
                       gpu::SharedImageCapabilities(),
                       mojo::ScopedMessagePipeHandle(
                           mojo::MessagePipeHandle(mojo::kInvalidHandleValue))),
        gpu_channel_(gpu_channel) {}

  gpu::mojom::GpuChannel& GetGpuChannel() override { return *gpu_channel_; }

 protected:
  ~TestGpuChannelHost() override = default;

  const raw_ref<gpu::mojom::GpuChannel> gpu_channel_;
};

class NativeTextureWrapperImplTest : public testing::Test {
 public:
  NativeTextureWrapperImplTest() {}
  ~NativeTextureWrapperImplTest() override {}
  struct NativeTextureWrapperImplDeleter {
    void operator()(NativeTextureWrapperImpl* ptr) {
      if (ptr) {
        ptr->Destroy();
      }
    }
  };

 protected:
  void SetUp() override {
    task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    channel_ = base::MakeRefCounted<TestGpuChannelHost>(mock_gpu_channel_);
    factory_ = NativeTextureFactory::Create(channel_);
    ON_CALL(mock_gpu_channel_,
            CreateNativeTextureReturnBool(::testing::_, ::testing::_,
                                          ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](int32_t a, int32_t b,
               mojo::PendingAssociatedReceiver<gpu::mojom::StreamTexture>,
               int32_t* out_value) {
              *out_value = 1;
              return true;
            }));

    impl_ = std::unique_ptr<NativeTextureWrapperImpl,
                            NativeTextureWrapperImplDeleter>(
        new NativeTextureWrapperImpl(
            true, gl::ohos::TextureOwnerMode::kNone,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
            false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
            factory_, base::SingleThreadTaskRunner::GetCurrentDefault()),
        NativeTextureWrapperImplDeleter{});
    impl_->compositor_task_runner_ =
        base::MakeRefCounted<MockSingleThreadTaskRunner>();
    ASSERT_NE(impl_, nullptr);
  }

  void InitializeOnMainThread() {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        base::MakeRefCounted<MockSingleThreadTaskRunner>();
    auto mock_task_runner =
        static_cast<MockSingleThreadTaskRunner*>(task_runner.get());
    ON_CALL(*mock_task_runner, BelongsToCurrentThread())
        .WillByDefault(testing::Return(true));
    impl_->main_task_runner_ = task_runner;

    EXPECT_CALL(mock_gpu_channel_,
                CreateNativeTextureReturnBool(::testing::_, ::testing::_,
                                              ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](int32_t a, int32_t b,
               mojo::PendingAssociatedReceiver<gpu::mojom::StreamTexture>,
               int32_t* out_value) {
              *out_value = 1;
              return true;
            }));
    base::RepeatingClosure received_frame_cb = base::DoNothing();
    bool init_result = false;
    base::OnceCallback<void(bool)> init_cb = base::BindOnce(
        [](bool* result, bool success) { *result = success; }, &init_result);
    base::OnceCallback<void(int)> create_texture_cb = base::DoNothing();
    base::RepeatingCallback<void()> destroy_texture_cb = base::DoNothing();
    impl_->InitializeOnMainThread(received_frame_cb, std::move(init_cb),
                                  std::move(create_texture_cb),
                                  destroy_texture_cb);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::test::TaskEnvironment task_environment_;
  gpu::MockGpuChannel mock_gpu_channel_;
  scoped_refptr<NativeTextureFactory> factory_;
  std::unique_ptr<NativeTextureWrapperImpl, NativeTextureWrapperImplDeleter>
      impl_;
  scoped_refptr<TestGpuChannelHost> channel_;
};

TEST_F(NativeTextureWrapperImplTest, NativeTextureWrapperImpRelease) {
  EXPECT_NO_FATAL_FAILURE(impl_->native_texture_proxy_.reset());
}

TEST_F(NativeTextureWrapperImplTest, CreateVideoFrame001) {
  gpu::SharedImageInterfaceProxy proxy = gpu::SharedImageInterfaceProxy(
      channel_.get(), 0, gpu::SharedImageCapabilities());
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image =
      base::MakeRefCounted<MockClientSharedImageInterface>(&proxy, channel_);
  auto mock_image =
      static_cast<MockClientSharedImageInterface*>(shared_image.get());
  factory_->shared_image_interface_ = std::move(shared_image);
  // CreateForTesting() defaults to 64x64, but WrapSharedImage CHECKs that
  // coded_size matches the shared image size. Build a shared image whose
  // metadata size matches the coded_size used below.
  gpu::SharedImageMetadata metadata;
  metadata.format = viz::SinglePlaneFormat::kRGBA_8888;
  metadata.size = gfx::Size(100, 100);
  metadata.color_space = gfx::ColorSpace::CreateSRGB();
  metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
  metadata.alpha_type = kPremul_SkAlphaType;
  metadata.usage = gpu::SharedImageUsageSet();
  auto test_shared_image = gpu::ClientSharedImage::CreateForTesting(metadata);
  EXPECT_CALL(*mock_image, NotifyMailboxAddedWithTarget(
                               testing::_, testing::_, testing::_, testing::_,
                               testing::_, testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(test_shared_image));

  gpu::Mailbox mailbox;
  gfx::Size coded_size{100, 100};
  gfx::Rect visible_rect{0, 0, 100, 100};
  absl::optional<gpu::VulkanYCbCrInfo> ycbcr_info;

  impl_->current_frame_.reset();
  impl_->enable_texture_copy_ = false;
  impl_->CreateVideoFrame(mailbox, coded_size, visible_rect, ycbcr_info);
  EXPECT_NE(impl_->current_frame_, nullptr);
}

TEST_F(NativeTextureWrapperImplTest, CreateVideoFrame002) {
  gpu::SharedImageInterfaceProxy proxy = gpu::SharedImageInterfaceProxy(
      channel_.get(), 0, gpu::SharedImageCapabilities());
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image =
      base::MakeRefCounted<MockClientSharedImageInterface>(&proxy, channel_);
  auto mock_image =
      static_cast<MockClientSharedImageInterface*>(shared_image.get());
  factory_->shared_image_interface_ = std::move(shared_image);
  // See CreateVideoFrame001: shared image size must match coded_size.
  gpu::SharedImageMetadata metadata;
  metadata.format = viz::SinglePlaneFormat::kRGBA_8888;
  metadata.size = gfx::Size(100, 100);
  metadata.color_space = gfx::ColorSpace::CreateSRGB();
  metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
  metadata.alpha_type = kPremul_SkAlphaType;
  metadata.usage = gpu::SharedImageUsageSet();
  auto test_shared_image = gpu::ClientSharedImage::CreateForTesting(metadata);
  EXPECT_CALL(*mock_image, NotifyMailboxAddedWithTarget(
                               testing::_, testing::_, testing::_, testing::_,
                               testing::_, testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(test_shared_image));
  gpu::Mailbox mailbox;
  gfx::Size coded_size{100, 100};
  gfx::Rect visible_rect{0, 0, 100, 100};
  absl::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
  impl_->current_frame_.reset();
  impl_->enable_texture_copy_ = true;
  impl_->CreateVideoFrame(mailbox, coded_size, visible_rect, ycbcr_info);
  EXPECT_NE(impl_->current_frame_, nullptr);
}

TEST_F(NativeTextureWrapperImplTest, ClearCBOnAnyThread001) {
  impl_->ClearCBOnAnyThread();
  InitializeOnMainThread();
  impl_->ClearCBOnAnyThread();
  EXPECT_TRUE(impl_->native_texture_proxy_->received_frame_cb_.is_null());
}

TEST_F(NativeTextureWrapperImplTest, UpdateTextureSize001) {
  gfx::Size new_size{640, 480};
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<MockSingleThreadTaskRunner>();
  auto mock_task_runner =
      static_cast<MockSingleThreadTaskRunner*>(task_runner.get());
  ON_CALL(*mock_task_runner, BelongsToCurrentThread())
      .WillByDefault(testing::Return(true));
  impl_->main_task_runner_ = task_runner;
  impl_->UpdateTextureSize(new_size);
  EXPECT_NE(impl_->rotated_visible_size_, new_size);
}

TEST_F(NativeTextureWrapperImplTest, UpdateTextureSize002) {
  gfx::Size new_size{640, 480};
  impl_->native_texture_proxy_.reset();
  impl_->UpdateTextureSize(new_size);
  EXPECT_NE(impl_->rotated_visible_size_, new_size);
}

TEST_F(NativeTextureWrapperImplTest, UpdateTextureSize003) {
  InitializeOnMainThread();
  impl_->rotated_visible_size_.set_width(640);
  impl_->rotated_visible_size_.set_height(480);
  gfx::Size new_size{640, 480};
  impl_->UpdateTextureSize(new_size);
  EXPECT_EQ(impl_->rotated_visible_size_, new_size);
}

TEST_F(NativeTextureWrapperImplTest, UpdateTextureSize004) {
  InitializeOnMainThread();
  impl_->rotated_visible_size_.set_width(640);
  impl_->rotated_visible_size_.set_height(480);
  gfx::Size new_size{640, 280};
  impl_->UpdateTextureSize(new_size);
  EXPECT_EQ(impl_->rotated_visible_size_, new_size);
}

TEST_F(NativeTextureWrapperImplTest, InitializeOnMainThread001) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<MockSingleThreadTaskRunner>();
  auto mock_task_runner =
      static_cast<MockSingleThreadTaskRunner*>(task_runner.get());
  ON_CALL(*mock_task_runner, BelongsToCurrentThread())
      .WillByDefault(testing::Return(true));
  impl_->main_task_runner_ = task_runner;
  bool init_result = false;
  base::RepeatingClosure received_frame_cb = base::DoNothing();
  base::OnceCallback<void(bool)> init_cb = base::BindOnce(
      [](bool* result, bool success) { *result = success; }, &init_result);
  base::OnceCallback<void(int)> create_texture_cb = base::DoNothing();
  base::RepeatingCallback<void()> destroy_texture_cb = base::DoNothing();
  impl_->factory_ = nullptr;
  impl_->InitializeOnMainThread(received_frame_cb, std::move(init_cb),
                                std::move(create_texture_cb),
                                destroy_texture_cb);

  EXPECT_FALSE(init_result);
}

TEST_F(NativeTextureWrapperImplTest, InitializeOnMainThread002) {
  bool init_result = false;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<MockSingleThreadTaskRunner>();
  auto mock_task_runner =
      static_cast<MockSingleThreadTaskRunner*>(task_runner.get());
  ON_CALL(*mock_task_runner, BelongsToCurrentThread())
      .WillByDefault(testing::Return(true));
  impl_->main_task_runner_ = task_runner;
  base::RepeatingClosure received_frame_cb = base::DoNothing();
  base::OnceCallback<void(bool)> init_cb = base::BindOnce(
      [](bool* result, bool success) { *result = success; }, &init_result);
  base::OnceCallback<void(int)> create_texture_cb = base::DoNothing();
  base::RepeatingCallback<void()> destroy_texture_cb = base::DoNothing();
  ON_CALL(mock_gpu_channel_,
          CreateNativeTextureReturnBool(::testing::_, ::testing::_,
                                        ::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke(
          [](int32_t a, int32_t b,
             mojo::PendingAssociatedReceiver<gpu::mojom::StreamTexture>,
             int32_t* out_value) {
            *out_value = -1;
            return true;
          }));
  impl_->InitializeOnMainThread(received_frame_cb, std::move(init_cb),
                                std::move(create_texture_cb),
                                destroy_texture_cb);

  EXPECT_FALSE(init_result);
}

TEST_F(NativeTextureWrapperImplTest, InitializeOnMainThread003) {
  bool init_result = false;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<MockSingleThreadTaskRunner>();
  auto mock_task_runner =
      static_cast<MockSingleThreadTaskRunner*>(task_runner.get());
  ON_CALL(*mock_task_runner, BelongsToCurrentThread())
      .WillByDefault(testing::Return(true));
  impl_->main_task_runner_ = task_runner;

  base::RepeatingClosure received_frame_cb = base::DoNothing();
  base::OnceCallback<void(bool)> init_cb = base::BindOnce(
      [](bool* result, bool success) { *result = success; }, &init_result);
  base::OnceCallback<void(int)> create_texture_cb = base::DoNothing();
  base::RepeatingCallback<void()> destroy_texture_cb = base::DoNothing();
  impl_->InitializeOnMainThread(received_frame_cb, std::move(init_cb),
                                std::move(create_texture_cb),
                                destroy_texture_cb);

  EXPECT_TRUE(init_result);
}

// ---------- Create (static factory) ----------
TEST_F(NativeTextureWrapperImplTest, Create_StaticFactory) {
  auto wrapper = NativeTextureWrapperImpl::Create(
      true, gl::ohos::TextureOwnerMode::kNone,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif  // ARKWEB_CUSTOM_VIDEO_PLAYER
      factory_, base::SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_NE(wrapper.get(), nullptr);
  // The static Create returns a ScopedNativeTextureWrapper (unique_ptr).
  // Release the raw pointer into our deleter-managed unique_ptr so the existing
  // teardown logic runs. First reset the existing impl_ to avoid double
  // ownership.
  if (impl_) {
    impl_.reset();
  }
  NativeTextureWrapperImpl* raw =
      static_cast<NativeTextureWrapperImpl*>(wrapper.release());
  impl_ = std::unique_ptr<NativeTextureWrapperImpl,
                          NativeTextureWrapperImplDeleter>(
      raw, NativeTextureWrapperImplDeleter{});
  impl_->compositor_task_runner_ =
      base::MakeRefCounted<MockSingleThreadTaskRunner>();
}

// ---------- GetCurrentFrame: returns current_frame_ ----------
TEST_F(NativeTextureWrapperImplTest, GetCurrentFrame_NullFrame) {
  impl_->current_frame_ = nullptr;
  auto frame = impl_->GetCurrentFrame();
  EXPECT_EQ(frame, nullptr);
}

TEST_F(NativeTextureWrapperImplTest, GetCurrentFrame_NonNullFrame) {
  auto test_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(320, 240), gfx::Rect(320, 240),
      gfx::Size(320, 240), base::TimeDelta());
  impl_->current_frame_ = test_frame;
  auto frame = impl_->GetCurrentFrame();
  EXPECT_EQ(frame.get(), test_frame.get());
}

// ---------- SetCurrentFrameInternal ----------
TEST_F(NativeTextureWrapperImplTest, SetCurrentFrameInternal) {
  auto test_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, gfx::Size(640, 480), gfx::Rect(640, 480),
      gfx::Size(640, 480), base::TimeDelta());
  impl_->SetCurrentFrameInternal(test_frame);
  EXPECT_EQ(impl_->current_frame_.get(), test_frame.get());
  // Reset to nullptr to avoid holding references during teardown.
  impl_->SetCurrentFrameInternal(nullptr);
  EXPECT_EQ(impl_->current_frame_, nullptr);
}

// ---------- Initialize (public): PostTask to main thread ----------
// Initialize() should PostTask to the main thread. We verify this by setting
// up a mock task runner that does not actually run the task; if Initialize
// directly invoked InitializeOnMainThread, the gpu channel mock would be
// called. By asserting the gpu channel is NOT called and the PostTask IS
// called, we confirm the PostTask branch is taken.
TEST_F(NativeTextureWrapperImplTest, Initialize_PostsToMainThread) {
  scoped_refptr<base::SingleThreadTaskRunner> mock_runner =
      base::MakeRefCounted<MockSingleThreadTaskRunner>();
  auto* mock = static_cast<MockSingleThreadTaskRunner*>(mock_runner.get());
  ON_CALL(*mock, BelongsToCurrentThread()).WillByDefault(testing::Return(true));
  // We expect a PostTask (the queued InitializeOnMainThread).
  EXPECT_CALL(*mock, PostDelayedTask(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(testing::Return(true));
  impl_->main_task_runner_ = mock_runner;

  // The gpu channel should NOT be called since we never run the posted task.
  EXPECT_CALL(mock_gpu_channel_,
              CreateNativeTextureReturnBool(::testing::_, ::testing::_,
                                            ::testing::_, ::testing::_))
      .Times(0);

  base::RepeatingClosure received_frame_cb = base::DoNothing();
  base::OnceCallback<void(bool)> init_cb = base::DoNothing();
  base::OnceCallback<void(int)> create_texture_cb = base::DoNothing();
  base::RepeatingCallback<void()> destroy_texture_cb = base::DoNothing();
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner =
      base::MakeRefCounted<MockSingleThreadTaskRunner>();
  impl_->Initialize(received_frame_cb, compositor_task_runner,
                    std::move(init_cb), std::move(create_texture_cb),
                    destroy_texture_cb);
  // Restore the real task runner for safe teardown (impl_ destructor asserts
  // main_task_runner_->BelongsToCurrentThread()).
  impl_->main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

// ---------- Destroy: non-main-thread branch (PostTask) ----------
TEST_F(NativeTextureWrapperImplTest, Destroy_NonMainThread_PostsTask) {
  // Configure main_task_runner_ to NOT belong to current thread so Destroy
  // takes the PostTask branch. We use a mock task runner that reports false.
  scoped_refptr<base::SingleThreadTaskRunner> mock_runner =
      base::MakeRefCounted<MockSingleThreadTaskRunner>();
  auto* mock = static_cast<MockSingleThreadTaskRunner*>(mock_runner.get());
  ON_CALL(*mock, BelongsToCurrentThread())
      .WillByDefault(testing::Return(false));
  EXPECT_CALL(*mock, PostDelayedTask(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(testing::Return(true));
  impl_->main_task_runner_ = mock_runner;

  // Release ownership from the deleter-managed unique_ptr so Destroy can
  // safely handle deletion (which it won't actually do because it posts).
  NativeTextureWrapperImpl* raw = impl_.release();
  raw->Destroy();
  // Since Destroy posted the task instead of deleting, we need to clean up
  // manually. Re-acquire into our deleter-managed ptr.
  impl_ = std::unique_ptr<NativeTextureWrapperImpl,
                          NativeTextureWrapperImplDeleter>(
      raw, NativeTextureWrapperImplDeleter{});
  // Replace the mock runner with the real one for safe teardown.
  impl_->main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

// ---------- Destructor with non-null native_texture_proxy_ ----------
TEST_F(NativeTextureWrapperImplTest, Destructor_WithProxy) {
  InitializeOnMainThread();
  EXPECT_NE(impl_->native_texture_proxy_, nullptr);
  // Releasing impl_ will call the deleter -> Destroy -> delete this, which
  // invokes ~NativeTextureWrapperImpl() with a non-null proxy. The destructor
  // calls ClearCreateVideoFrameCB() and SetCurrentFrameInternal(nullptr).
  impl_.reset();
}

// ---------- CreateVideoFrame: null frame path ----------
// When VideoFrame::WrapSharedImage returns nullptr (e.g., invalid params),
// CreateVideoFrame logs an error and returns without setting current_frame_.
TEST_F(NativeTextureWrapperImplTest, CreateVideoFrame_NullFrame) {
  gpu::SharedImageInterfaceProxy proxy = gpu::SharedImageInterfaceProxy(
      channel_.get(), 0, gpu::SharedImageCapabilities());
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image =
      base::MakeRefCounted<MockClientSharedImageInterface>(&proxy, channel_);
  auto mock_image =
      static_cast<MockClientSharedImageInterface*>(shared_image.get());
  factory_->shared_image_interface_ = std::move(shared_image);
  // Return a null shared image to force WrapSharedImage to return nullptr.
  EXPECT_CALL(*mock_image, NotifyMailboxAddedWithTarget(
                               testing::_, testing::_, testing::_, testing::_,
                               testing::_, testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(nullptr));

  gpu::Mailbox mailbox;
  gfx::Size coded_size{0, 0};  // zero size to force null frame
  gfx::Rect visible_rect{0, 0, 0, 0};
  absl::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
  impl_->current_frame_.reset();
  impl_->CreateVideoFrame(mailbox, coded_size, visible_rect, ycbcr_info);
  // current_frame_ should remain null because WrapSharedImage returned null.
  EXPECT_EQ(impl_->current_frame_, nullptr);
}

// ---------- UpdateTextureSize: non-main-thread PostTask branch ----------
TEST_F(NativeTextureWrapperImplTest, UpdateTextureSize_NonMainThread) {
  gfx::Size new_size{1280, 720};
  scoped_refptr<base::SingleThreadTaskRunner> mock_runner =
      base::MakeRefCounted<MockSingleThreadTaskRunner>();
  auto* mock = static_cast<MockSingleThreadTaskRunner*>(mock_runner.get());
  ON_CALL(*mock, BelongsToCurrentThread())
      .WillByDefault(testing::Return(false));
  // UpdateTextureSize posts one task; the deleter's Destroy() may also post.
  EXPECT_CALL(*mock, PostDelayedTask(::testing::_, ::testing::_, ::testing::_))
      .Times(::testing::AtLeast(1))
      .WillRepeatedly(testing::Return(true));
  impl_->main_task_runner_ = mock_runner;
  impl_->UpdateTextureSize(new_size);
  // Should not have updated rotated_visible_size_ since it posted to main
  // thread.
  EXPECT_NE(impl_->rotated_visible_size_, new_size);
  // Restore the real task runner for safe teardown (deleter calls Destroy()
  // which checks BelongsToCurrentThread and may PostTask).
  impl_->main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}
}  // namespace content
