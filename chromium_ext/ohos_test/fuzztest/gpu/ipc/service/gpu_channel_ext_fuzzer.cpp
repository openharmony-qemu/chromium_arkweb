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

#include <fuzzer/FuzzedDataProvider.h>

#include "arkweb/chromium_ext/gpu/ipc/service/gpu_channel_ext.h"
#include "arkweb/build/features/features.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "base/command_line.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <climits>
#include <cstdint>
#include <memory>

namespace gpu {

constexpr int32_t MAX_NATIVE_ID = 10000;
constexpr uint32_t MAX_CLIENT_ID = 10000;
constexpr uint64_t MAX_FRAME_SINK_ID = UINT64_MAX;
constexpr uint64_t MAX_BLANKLESS_KEY = UINT64_MAX;
constexpr int32_t MAX_LCP_TIME = 10000;
constexpr int64_t MAX_PREF_HASH = INT64_MAX;
constexpr int64_t MAX_WIDTH = 4096;
constexpr int64_t MAX_HEIGHT = 4096;
constexpr uint64_t MAX_SYSTEM_TIME = UINT64_MAX;

class GpuChannelExtFuzzer : public GpuChannelTestCommon {
 public:
  GpuChannelExtFuzzer() : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~GpuChannelExtFuzzer() override = default;

  void SetUp() override {}

  void TearDown() override {}

  void TestBody() override {}

#if BUILDFLAG(ARKWEB_SAME_LAYER)
  void FuzzCreateNativeTexture(FuzzedDataProvider* fdp) {
    int32_t client_id = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_CLIENT_ID);
    bool is_gpu_host = fdp->ConsumeBool();

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
    if (!channel_ext) {
      return;
    }

    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);

    int32_t mode_value = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
    gl::ohos::TextureOwnerMode texture_owner_mode;
    switch (mode_value) {
      case 0:
        texture_owner_mode = gl::ohos::TextureOwnerMode::kNativeImageTexture;
        break;
      case 1:
        texture_owner_mode = gl::ohos::TextureOwnerMode::kSameLayerNativeBuffer;
        break;
      default:
        texture_owner_mode = gl::ohos::TextureOwnerMode::kHwVideoZeroCopyNativeBuffer;
        break;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;

    int32_t result = channel_ext->CreateNativeTexture(
        native_id, texture_owner_mode, std::move(receiver));

    (void)result;
  }

  void FuzzDestroyNativeTexture(FuzzedDataProvider* fdp) {
    int32_t client_id = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_CLIENT_ID);
    bool is_gpu_host = fdp->ConsumeBool();

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
    if (!channel_ext) {
      return;
    }

    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);

    channel_ext->DestroyNativeTexture(native_id);
  }

  void FuzzCreateDestroySequence(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
    if (!channel_ext) {
      return;
    }

    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;

    channel_ext->CreateNativeTexture(
        native_id, gl::ohos::TextureOwnerMode::kHwVideoZeroCopyNativeBuffer, std::move(receiver));

    channel_ext->DestroyNativeTexture(native_id);
  }

  void FuzzDuplicateNativeId(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
    if (!channel_ext) {
      return;
    }

    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver1;
    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver2;

    channel_ext->CreateNativeTexture(
        native_id, gl::ohos::TextureOwnerMode::kHwVideoZeroCopyNativeBuffer, std::move(receiver1));

    int32_t result = channel_ext->CreateNativeTexture(
        native_id, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver2));

    (void)result;
  }

  void FuzzInvalidNativeId(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
    if (!channel_ext) {
      return;
    }

    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(INT32_MIN, -1);

    channel_ext->DestroyNativeTexture(native_id);
  }
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
  void FuzzSetBlanklessDumpInfo(FuzzedDataProvider* fdp) {
    int32_t client_id = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_CLIENT_ID);
    bool is_gpu_host = fdp->ConsumeBool();

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
    if (!channel_ext) {
      return;
    }

    uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();

    base::ohos::BlanklessDumpInfo info;
    info.info.blankless_key = fdp->ConsumeIntegral<uint64_t>();
    info.info.nweb_id = fdp->ConsumeIntegral<uint32_t>();
    info.info.lcp_time = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_LCP_TIME);
    info.info.system_time = fdp->ConsumeIntegral<uint64_t>();
    info.info.pref_hash = fdp->ConsumeIntegral<int64_t>();
    info.info.width = fdp->ConsumeIntegralInRange<int64_t>(0, MAX_WIDTH);
    info.info.height = fdp->ConsumeIntegralInRange<int64_t>(0, MAX_HEIGHT);
    info.dump_enabled = fdp->ConsumeBool();

    channel_ext->SetBlanklessDumpInfo(frame_sink_id, info);
  }

  void FuzzGetBlanklessDumpInfoAndDisable(FuzzedDataProvider* fdp) {
    uint32_t client_id = fdp->ConsumeIntegralInRange<uint32_t>(0, MAX_CLIENT_ID);
    uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();

    base::ohos::BlanklessDumpInfo info;

    bool result = GpuChannelExt::GetBlanklessDumpInfoAndDisableDump(
        client_id, frame_sink_id, info);

    (void)result;
  }

  void FuzzSetGetSequence(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
    if (!channel_ext) {
      return;
    }

    uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();

    base::ohos::BlanklessDumpInfo info;
    info.info.blankless_key = fdp->ConsumeIntegral<uint64_t>();
    info.info.nweb_id = fdp->ConsumeIntegral<uint32_t>();
    info.dump_enabled = true;

    channel_ext->SetBlanklessDumpInfo(frame_sink_id, info);

    base::ohos::BlanklessDumpInfo retrieved_info;
    bool result = GpuChannelExt::GetBlanklessDumpInfoAndDisableDump(
        client_id, frame_sink_id, retrieved_info);

    (void)result;
  }

  void FuzzBoundaryValuesBlankless(FuzzedDataProvider* fdp) {
    int32_t client_id = fdp->ConsumeBool() ? 0 : INT32_MAX;

    GpuChannel* channel = CreateChannel(client_id, false);
    if (!channel) {
      return;
    }

    GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
    if (!channel_ext) {
      return;
    }

    uint64_t frame_sink_id = fdp->ConsumeBool() ? 0 : UINT64_MAX;

    base::ohos::BlanklessDumpInfo info;
    info.info.blankless_key = fdp->ConsumeBool() ? 0 : UINT64_MAX;
    info.info.nweb_id = fdp->ConsumeBool() ? 0 : UINT32_MAX;
    info.info.lcp_time = fdp->ConsumeBool() ? INT32_MIN : INT32_MAX;
    info.info.system_time = fdp->ConsumeBool() ? 0 : UINT64_MAX;
    info.info.pref_hash = fdp->ConsumeBool() ? INT64_MIN : INT64_MAX;
    info.info.width = fdp->ConsumeBool() ? INT64_MIN : INT64_MAX;
    info.info.height = fdp->ConsumeBool() ? INT64_MIN : INT64_MAX;
    info.dump_enabled = fdp->ConsumeBool();

    channel_ext->SetBlanklessDumpInfo(frame_sink_id, info);
  }

  void FuzzMultipleSetGet(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;

    GpuChannel* channel = CreateChannel(client_id, false);
    if (!channel) {
      return;
    }

    GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
    if (!channel_ext) {
      return;
    }

    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, 10);

    for (size_t i = 0; i < iterations; ++i) {
      uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();

      base::ohos::BlanklessDumpInfo info;
      info.info.blankless_key = fdp->ConsumeIntegral<uint64_t>();
      info.dump_enabled = fdp->ConsumeBool();

      channel_ext->SetBlanklessDumpInfo(frame_sink_id, info);

      base::ohos::BlanklessDumpInfo retrieved_info;
      bool result = GpuChannelExt::GetBlanklessDumpInfoAndDisableDump(
          client_id, frame_sink_id, retrieved_info);

      (void)result;
    }
  }

  void FuzzGetNonExistentInfo(FuzzedDataProvider* fdp) {
    uint32_t client_id = fdp->ConsumeIntegralInRange<uint32_t>(0, MAX_CLIENT_ID);
    uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();

    base::ohos::BlanklessDumpInfo info;

    bool result = GpuChannelExt::GetBlanklessDumpInfoAndDisableDump(
        client_id, frame_sink_id, info);

    (void)result;
  }
#endif

  void FuzzMultipleChannelOperations(FuzzedDataProvider* fdp) {
    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(5, 20);

    for (size_t i = 0; i < iterations; ++i) {
      int32_t client_id = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_CLIENT_ID);
      bool is_gpu_host = fdp->ConsumeBool();

      GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
      if (!channel) {
        continue;
      }

      GpuChannelExt* channel_ext = channel->AsGpuChannelExt();
      if (!channel_ext) {
        continue;
      }

#if BUILDFLAG(ARKWEB_SAME_LAYER)
      if (fdp->ConsumeBool()) {
        int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);
        mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
        channel_ext->CreateNativeTexture(
            native_id, gl::ohos::TextureOwnerMode::kHwVideoZeroCopyNativeBuffer, std::move(receiver));
      }

      if (fdp->ConsumeBool()) {
        int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);
        channel_ext->DestroyNativeTexture(native_id);
      }
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
      if (fdp->ConsumeBool()) {
        uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();
        base::ohos::BlanklessDumpInfo info;
        info.info.blankless_key = fdp->ConsumeIntegral<uint64_t>();
        info.dump_enabled = fdp->ConsumeBool();
        channel_ext->SetBlanklessDumpInfo(frame_sink_id, info);
      }

      if (fdp->ConsumeBool()) {
        uint32_t cid = fdp->ConsumeIntegral<uint32_t>();
        uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();
        base::ohos::BlanklessDumpInfo info;
        bool result = GpuChannelExt::GetBlanklessDumpInfoAndDisableDump(
            cid, frame_sink_id, info);
        (void)result;
      }
#endif
    }
  }

  void RunFuzzTest(FuzzedDataProvider* fdp) {
    int32_t selector = fdp->ConsumeIntegralInRange<int32_t>(0, 15);

    switch (selector) {
#if BUILDFLAG(ARKWEB_SAME_LAYER)
      case 0:
        FuzzCreateNativeTexture(fdp);
        break;
      case 1:
        FuzzDestroyNativeTexture(fdp);
        break;
      case 2:
        FuzzCreateDestroySequence(fdp);
        break;
      case 3:
        FuzzDuplicateNativeId(fdp);
        break;
      case 4:
        FuzzInvalidNativeId(fdp);
        break;
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
      case 5:
        FuzzSetBlanklessDumpInfo(fdp);
        break;
      case 6:
        FuzzGetBlanklessDumpInfoAndDisable(fdp);
        break;
      case 7:
        FuzzSetGetSequence(fdp);
        break;
      case 8:
        FuzzBoundaryValuesBlankless(fdp);
        break;
      case 9:
        FuzzMultipleSetGet(fdp);
        break;
      case 10:
        FuzzGetNonExistentInfo(fdp);
        break;
#endif

      case 11:
        FuzzMultipleChannelOperations(fdp);
        break;

      default:
        FuzzMultipleChannelOperations(fdp);
        break;
    }
  }
};

}  // namespace gpu

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }
  static bool initialized = []() {
    base::CommandLine::Init(0, nullptr);
    return true;
  }();
  static gpu::GpuChannelExtFuzzer* fuzzer = new gpu::GpuChannelExtFuzzer();

  FuzzedDataProvider fdp(data, size);

  fuzzer->RunFuzzTest(&fdp);

  return 0;
}
