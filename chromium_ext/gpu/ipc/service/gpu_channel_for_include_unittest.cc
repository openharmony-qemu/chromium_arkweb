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
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/stream_texture_ohos.h"
#include "base/threading/thread.h"
#include "base/logging.h"

#define private public
#include "arkweb/chromium_ext/gpu/ipc/service/gpu_channel_ext.h"
#include "gpu/ipc/service/gpu_channel.h"
#undef private

namespace gpu {

// GpuChannelMessageFilter is defined privately inside gpu_channel.cc, so it is
// not visible to test translation units. This minimal re-declaration exposes
// only the methods implemented in
// arkweb/chromium_ext/gpu/ipc/service/gpu_channel_for_include.cc. The calls
// resolve to the real definitions linked from //gpu/ipc/service. No object
// layout knowledge is required, since members are never touched here.
class GpuChannelMessageFilter {
 public:
  void CreateNativeTexture(
      int32_t native_id,
      int32_t texture_owner_mode,
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      bool is_custom_media_player,
#endif
      mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver,
      mojom::GpuChannel::CreateNativeTextureCallback callback);
  void SetBlanklessDumpInfo(uint32_t nweb_id,
                            uint64_t blankless_key,
                            uint64_t frame_sink_id,
                            int32_t lcp_time,
                            int64_t pref_hash);
  void Destroy();
};

class GpuChannelForIncludeTest : public GpuChannelTestCommon {
 public:
  GpuChannelForIncludeTest() : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~GpuChannelForIncludeTest() override = default;

  void SetUp() override {
    channel_ = CreateChannel(kDefaultClientId, true /* is_gpu_host */);
    ASSERT_TRUE(channel_);
    filter_ = channel_->filter_.get();
    ASSERT_TRUE(filter_);
  }

  void TearDown() override {
    channel_ = nullptr;
    filter_ = nullptr;
  }

  static constexpr int32_t kDefaultClientId = 1;
  static constexpr int32_t kValidNativeId = 1;
  static constexpr uint64_t kFrameSinkId = 20261022;
  GpuChannel* channel_ = nullptr;
  GpuChannelMessageFilter* filter_ = nullptr;
};

#if BUILDFLAG(ARKWEB_SAME_LAYER)
// Test CreateNativeTexture with a live channel: TryCreateNativeTexture runs on
// the main task runner and replies with the embed id.
TEST_F(GpuChannelForIncludeTest, CreateNativeTexture_WithChannel_RepliesWithEmbedId) {
  int32_t callback_result = -100;
  bool callback_called = false;
  auto callback = base::BindOnce(
      [](int32_t* result, bool* called, int32_t value) {
        *result = value;
        *called = true;
      },
      base::Unretained(&callback_result), base::Unretained(&callback_called));

  mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
  filter_->CreateNativeTexture(
      kValidNativeId,
      static_cast<int32_t>(gl::ohos::TextureOwnerMode::kNativeImageTexture),
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif
      std::move(receiver), std::move(callback));
  task_environment().RunUntilIdle();

  EXPECT_TRUE(callback_called);
  EXPECT_NE(callback_result, -1);
}

// Test CreateNativeTexture after Destroy(): gpu_channel_ is null so the
// receiver is reset and the callback is never run.
TEST_F(GpuChannelForIncludeTest, CreateNativeTexture_AfterDestroy_CallbackNotRun) {
  filter_->Destroy();

  bool callback_called = false;
  auto callback = base::BindOnce(
      [](bool* called, int32_t /*value*/) { *called = true; },
      base::Unretained(&callback_called));

  mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
  filter_->CreateNativeTexture(
      kValidNativeId,
      static_cast<int32_t>(gl::ohos::TextureOwnerMode::kNativeImageTexture),
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      false,
#endif
      std::move(receiver), std::move(callback));
  task_environment().RunUntilIdle();

  EXPECT_FALSE(callback_called);
}
#endif  // BUILDFLAG(ARKWEB_SAME_LAYER)

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
// Test SetBlanklessDumpInfo with a live channel: the info is forwarded to
// GpuChannelExt and stored under (client_id, frame_sink_id).
TEST_F(GpuChannelForIncludeTest, SetBlanklessDumpInfo_WithChannel_StoresDumpInfo) {
  filter_->SetBlanklessDumpInfo(111u, 222u, kFrameSinkId, 44, 55);

  base::ohos::BlanklessDumpInfo info;
  bool result = GpuChannelExt::GetBlanklessDumpInfoAndDisableDump(
      kDefaultClientId, kFrameSinkId, info);
  EXPECT_TRUE(result);
  EXPECT_TRUE(info.dump_enabled);
  EXPECT_EQ(info.info.nweb_id, 111u);
  EXPECT_EQ(info.info.blankless_key, 222u);
  EXPECT_EQ(info.info.lcp_time, 44);
  EXPECT_EQ(info.info.pref_hash, 55);
}

// Test SetBlanklessDumpInfo after Destroy(): returns early without crash.
TEST_F(GpuChannelForIncludeTest, SetBlanklessDumpInfo_AfterDestroy_NoCrash) {
  filter_->Destroy();
  filter_->SetBlanklessDumpInfo(111u, 222u, kFrameSinkId, 44, 55);
  EXPECT_TRUE(true);
}
#endif  // BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)

}  // namespace gpu
