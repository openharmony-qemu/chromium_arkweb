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

#include "base/memory/ref_counted.h"
#define private public
#define protected public
#include "arkweb/chromium_ext/gpu/ipc/service/stream_texture_ohos.h"
#undef private
#undef protected
#include "gtest/gtest.h"
#include <cstdint>
#include "gpu/ipc/service/gpu_channel.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/task/single_thread_task_runner.h"

namespace gpu {

class StreamChannelTest : public GpuChannelTestCommon {
 public:
  StreamChannelTest() : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~StreamChannelTest() override = default;

  void SetUp() {
    channel_ = CreateChannel(kDefaultClientId, true /* is_gpu_host */);
    ASSERT_TRUE(channel_);
  }

  void TearDown() {
    channel_ = nullptr;
  }

  static constexpr int32_t kDefaultClientId = 1;
  GpuChannel* channel_ = nullptr;
};

class StreamTextureOHOSTest : public StreamChannelTest {
 protected:
  void SetUp() {
    StreamChannelTest::SetUp();
    context_state_ =
        channel_->gpu_channel_manager()->GetSharedContextState(&result_);
    ASSERT_EQ(result_, ContextResult::kSuccess);
  }

  void TearDown() {
    StreamChannelTest::TearDown();
  }

  ContextResult result_;
  scoped_refptr<SharedContextState> context_state_;
  mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
};

TEST_F(StreamTextureOHOSTest, Create) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, ReleaseChannel) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, RunCallback) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  base::Thread thread("MyThread");
  thread.Start();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      thread.task_runner();
  base::WeakPtrFactory<StreamTexture> weak_factory(stream_texture.get());
  base::WeakPtr<StreamTexture> weak_ptr = weak_factory.GetWeakPtr();
  StreamTexture::RunCallback(task_runner, weak_ptr);
  thread.Stop();
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, RunCallbackOnCurrentThread) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  auto weak_ptr = stream_texture->weak_factory_.GetWeakPtr();
  stream_texture->has_pending_frame_ = false;
  StreamTexture::RunCallback(task_runner, weak_ptr);
  EXPECT_TRUE(stream_texture->has_pending_frame_);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, RunCallbackOnCurrentThreadNullPtr) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  stream_texture->has_pending_frame_ = false;
  StreamTexture::RunCallback(task_runner, nullptr);
  EXPECT_FALSE(stream_texture->has_pending_frame_);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, OnFrameAvailable) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  stream_texture->OnFrameAvailable();
  EXPECT_TRUE(stream_texture->has_pending_frame_);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, OnFrameAvailableNoTextureOwner) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  stream_texture->native_texture_owner_ = nullptr;
  stream_texture->OnFrameAvailable();
  EXPECT_TRUE(stream_texture->has_pending_frame_);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, OnFrameAvailableNoChannel) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  mojo::PendingAssociatedRemote<mojom::StreamTextureClient> pending_client;
  std::ignore = pending_client.InitWithNewEndpointAndPassReceiver();
  pending_client.EnableUnassociatedUsage();
  stream_texture->StartListening(std::move(pending_client));
  stream_texture->ReleaseChannel();
  stream_texture->OnFrameAvailable();
}

TEST_F(StreamTextureOHOSTest, OnFrameAvailableWithClientEmptySize) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  mojo::PendingAssociatedRemote<mojom::StreamTextureClient> pending_client;
  std::ignore = pending_client.InitWithNewEndpointAndPassReceiver();
  pending_client.EnableUnassociatedUsage();
  stream_texture->StartListening(std::move(pending_client));
  stream_texture->OnFrameAvailable();
  EXPECT_TRUE(stream_texture->has_pending_frame_);
  EXPECT_TRUE(stream_texture->rotated_visible_size_.IsEmpty());
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, OnFrameAvailableSizeChanged) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  mojo::PendingAssociatedRemote<mojom::StreamTextureClient> pending_client;
  std::ignore = pending_client.InitWithNewEndpointAndPassReceiver();
  pending_client.EnableUnassociatedUsage();
  stream_texture->StartListening(std::move(pending_client));
  stream_texture->UpdateRotatedVisibleSize(gfx::Size(100, 200));
  stream_texture->OnFrameAvailable();
  EXPECT_FALSE(stream_texture->has_pending_frame_);
  EXPECT_EQ(stream_texture->coded_size_, gfx::Size(100, 200));
  EXPECT_EQ(stream_texture->visible_rect_, gfx::Rect(100, 200));
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, OnFrameAvailableSizeUnchanged) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  mojo::PendingAssociatedRemote<mojom::StreamTextureClient> pending_client;
  std::ignore = pending_client.InitWithNewEndpointAndPassReceiver();
  pending_client.EnableUnassociatedUsage();
  stream_texture->StartListening(std::move(pending_client));
  stream_texture->UpdateRotatedVisibleSize(gfx::Size(100, 200));
  stream_texture->OnFrameAvailable();
  stream_texture->OnFrameAvailable();
  EXPECT_FALSE(stream_texture->has_pending_frame_);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, UpdatesSize) {
  const gfx::Size kTestSize(100, 200);

  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_TRUE(stream_texture->rotated_visible_size_.IsEmpty());
  stream_texture->UpdateRotatedVisibleSize(kTestSize);
  EXPECT_EQ(stream_texture->rotated_visible_size_, kTestSize);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, UpdateRotatedVisibleSizeDeferredFrame) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  mojo::PendingAssociatedRemote<mojom::StreamTextureClient> pending_client;
  std::ignore = pending_client.InitWithNewEndpointAndPassReceiver();
  pending_client.EnableUnassociatedUsage();
  stream_texture->StartListening(std::move(pending_client));
  stream_texture->OnFrameAvailable();
  EXPECT_TRUE(stream_texture->has_pending_frame_);
  EXPECT_TRUE(stream_texture->rotated_visible_size_.IsEmpty());
  stream_texture->UpdateRotatedVisibleSize(gfx::Size(100, 200));
  EXPECT_FALSE(stream_texture->has_pending_frame_);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, UpdateRotatedVisibleSizeFromNotEmpty) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  stream_texture->UpdateRotatedVisibleSize(gfx::Size(100, 200));
  stream_texture->has_pending_frame_ = true;
  stream_texture->UpdateRotatedVisibleSize(gfx::Size(200, 300));
  EXPECT_TRUE(stream_texture->has_pending_frame_);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, ReturnsCachedValue) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  stream_texture->native_embed_id_ = 123;

  EXPECT_EQ(stream_texture->NativeEmbedID(), 123);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, GeneratesNewIdWhenUnset) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  stream_texture->native_embed_id_ = -1;

  EXPECT_NE(stream_texture->NativeEmbedID(), -1);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, IsUsingGpuMemoryReturnsTrue) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  EXPECT_TRUE(stream_texture->IsUsingGpuMemory());
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, HasTextureOwnerReturnsTrue) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  EXPECT_TRUE(stream_texture->HasTextureOwner());
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, TextureOwnerBindsTextureOnUpdate) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  EXPECT_EQ(stream_texture->TextureOwnerBindsTextureOnUpdate(),
            stream_texture->native_texture_owner_->binds_texture_on_update());
  //stream_texture->RenderToOverlay();
  stream_texture->GetTextureBase();
  stream_texture->GetNativeBuffer();
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, UpdateAndBindTexImage) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  stream_texture->UpdateAndBindTexImage(1);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, StartListeningBindsClient) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  EXPECT_FALSE(stream_texture->client_);
  mojo::PendingAssociatedRemote<mojom::StreamTextureClient> pending_client;
  std::ignore = pending_client.InitWithNewEndpointAndPassReceiver();
  pending_client.EnableUnassociatedUsage();
  stream_texture->StartListening(std::move(pending_client));
  EXPECT_TRUE(stream_texture->client_);
  stream_texture->ReleaseChannel();
}

TEST_F(StreamTextureOHOSTest, NotifyOverlayPromotion) {
  auto stream_texture = StreamTexture::Create(
      channel_, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER
      std::move(receiver));
  EXPECT_NE(stream_texture, nullptr);
  stream_texture->NotifyOverlayPromotion(true, gfx::Rect(0, 0, 100, 100));
  stream_texture->ReleaseChannel();
}

}  // namespace gpu