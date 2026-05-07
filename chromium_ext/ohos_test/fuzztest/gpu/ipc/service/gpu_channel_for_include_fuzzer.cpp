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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"

#include <fuzzer/FuzzedDataProvider.h>

#include "arkweb/build/features/features.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "base/command_line.h"
#include "base/unguessable_token.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <climits>
#include <cstdint>
#include <memory>

#define private public
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel.cc"
#include "arkweb/chromium_ext/gpu/ipc/service/gpu_channel_ext.h"
#undef private

namespace gpu {

constexpr int32_t MAX_NATIVE_ID = 10000;
constexpr int32_t MAX_TEXTURE_OWNER_MODE_VALUE = 2;
constexpr uint32_t MAX_NWEB_ID = 10000;
constexpr uint64_t MAX_BLANKLESS_KEY = UINT64_MAX;
constexpr int32_t MAX_LCP_TIME = 10000;
constexpr int64_t MAX_PREF_HASH = INT64_MAX;
constexpr uint64_t MAX_FRAME_SINK_ID = UINT64_MAX;

class GpuChannelForIncludeFuzzer : public GpuChannelTestCommon {
 public:
  GpuChannelForIncludeFuzzer() : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~GpuChannelForIncludeFuzzer() override = default;

  void SetUp() override {}

  void TearDown() override {}

  void TestBody() override {}

#if BUILDFLAG(ARKWEB_SAME_LAYER)
  void FuzzTryCreateNativeTextureWithNullChannel(FuzzedDataProvider* fdp) {
    base::WeakPtr<GpuChannel> null_channel;

    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);

    int32_t mode_value = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_TEXTURE_OWNER_MODE_VALUE);
    gl::ohos::TextureOwnerMode texture_owner_mode;
    switch (mode_value) {
      case 0:
        texture_owner_mode = gl::ohos::TextureOwnerMode::kSameLayerNativeBuffer;
        break;
      case 1:
        texture_owner_mode = gl::ohos::TextureOwnerMode::kNativeImageTexture;
        break;
      default:
        texture_owner_mode = gl::ohos::TextureOwnerMode::kHwVideoZeroCopyNativeBuffer;
        break;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
  }

  void FuzzTryCreateNativeTextureWithValidChannel(FuzzedDataProvider* fdp) {
    int32_t client_id = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_NWEB_ID);
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

    int32_t mode_value = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_TEXTURE_OWNER_MODE_VALUE);
    gl::ohos::TextureOwnerMode texture_owner_mode;
    switch (mode_value) {
      case 0:
        texture_owner_mode = gl::ohos::TextureOwnerMode::kSameLayerNativeBuffer;
        break;
      case 1:
        texture_owner_mode = gl::ohos::TextureOwnerMode::kNativeImageTexture;
        break;
      default:
        texture_owner_mode = gl::ohos::TextureOwnerMode::kHwVideoZeroCopyNativeBuffer;
        break;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
  }

  void FuzzGpuChannelMessageFilterCreateNativeTexture(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelMessageFilter* filter = channel->filter_.get();
    if (!filter) {
      return;
    }

    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);
    int32_t texture_owner_mode = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_TEXTURE_OWNER_MODE_VALUE);

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    mojom::GpuChannel::CreateNativeTextureCallback callback;

    filter->CreateNativeTexture(native_id, texture_owner_mode, std::move(receiver), std::move(callback));
  }

  void FuzzGpuChannelMessageFilterCreateNativeTextureNullChannel(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelMessageFilter* filter = channel->filter_.get();
    if (!filter) {
      return;
    }

    filter->gpu_channel_ = nullptr;

    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);
    int32_t texture_owner_mode = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_TEXTURE_OWNER_MODE_VALUE);

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    mojom::GpuChannel::CreateNativeTextureCallback callback;

    filter->CreateNativeTexture(native_id, texture_owner_mode, std::move(receiver), std::move(callback));
  }

  void FuzzCreateNativeTextureWithInvalidTextureOwnerMode(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelMessageFilter* filter = channel->filter_.get();
    if (!filter) {
      return;
    }

    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);
    int32_t texture_owner_mode = fdp->ConsumeIntegral<int32_t>();

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    mojom::GpuChannel::CreateNativeTextureCallback callback;

    filter->CreateNativeTexture(native_id, texture_owner_mode, std::move(receiver), std::move(callback));
  }

  void FuzzCreateNativeTextureWithDuplicateId(FuzzedDataProvider* fdp) {
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
  }

  void FuzzCreateNativeTextureBoundaryValues(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelMessageFilter* filter = channel->filter_.get();
    if (!filter) {
      return;
    }

    int32_t native_id = fdp->ConsumeBool() ? INT32_MIN : INT32_MAX;
    int32_t texture_owner_mode = fdp->ConsumeBool() ? INT32_MIN : INT32_MAX;

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    mojom::GpuChannel::CreateNativeTextureCallback callback;

    filter->CreateNativeTexture(native_id, texture_owner_mode, std::move(receiver), std::move(callback));
  }
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
  void FuzzGpuChannelMessageFilterSetBlanklessDumpInfo(FuzzedDataProvider* fdp) {
    int32_t client_id = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_NWEB_ID);
    bool is_gpu_host = fdp->ConsumeBool();

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelMessageFilter* filter = channel->filter_.get();
    if (!filter) {
      return;
    }

    uint32_t nweb_id = fdp->ConsumeIntegralInRange<uint32_t>(0, MAX_NWEB_ID);
    uint64_t blankless_key = fdp->ConsumeIntegral<uint64_t>();
    uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();
    int32_t lcp_time = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_LCP_TIME);
    int64_t pref_hash = fdp->ConsumeIntegral<int64_t>();

    filter->SetBlanklessDumpInfo(nweb_id, blankless_key, frame_sink_id, lcp_time, pref_hash);
  }

  void FuzzGpuChannelMessageFilterSetBlanklessDumpInfoNullChannel(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelMessageFilter* filter = channel->filter_.get();
    if (!filter) {
      return;
    }

    filter->gpu_channel_ = nullptr;

    uint32_t nweb_id = fdp->ConsumeIntegralInRange<uint32_t>(0, MAX_NWEB_ID);
    uint64_t blankless_key = fdp->ConsumeIntegral<uint64_t>();
    uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();
    int32_t lcp_time = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_LCP_TIME);
    int64_t pref_hash = fdp->ConsumeIntegral<int64_t>();

    filter->SetBlanklessDumpInfo(nweb_id, blankless_key, frame_sink_id, lcp_time, pref_hash);
  }

  void FuzzGpuChannelMessageFilterSetBlanklessDumpInfoBoundaryValues(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelMessageFilter* filter = channel->filter_.get();
    if (!filter) {
      return;
    }

    uint32_t nweb_id = fdp->ConsumeBool() ? 0 : UINT32_MAX;
    uint64_t blankless_key = fdp->ConsumeBool() ? 0 : UINT64_MAX;
    uint64_t frame_sink_id = fdp->ConsumeBool() ? 0 : UINT64_MAX;
    int32_t lcp_time = fdp->ConsumeBool() ? INT32_MIN : INT32_MAX;
    int64_t pref_hash = fdp->ConsumeBool() ? INT64_MIN : INT64_MAX;

    filter->SetBlanklessDumpInfo(nweb_id, blankless_key, frame_sink_id, lcp_time, pref_hash);
  }

  void FuzzSetBlanklessDumpInfoMultipleTimes(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelMessageFilter* filter = channel->filter_.get();
    if (!filter) {
      return;
    }

    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, 10);

    for (size_t i = 0; i < iterations; ++i) {
      uint32_t nweb_id = fdp->ConsumeIntegralInRange<uint32_t>(0, MAX_NWEB_ID);
      uint64_t blankless_key = fdp->ConsumeIntegral<uint64_t>();
      uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();
      int32_t lcp_time = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_LCP_TIME);
      int64_t pref_hash = fdp->ConsumeIntegral<int64_t>();

      filter->SetBlanklessDumpInfo(nweb_id, blankless_key, frame_sink_id, lcp_time, pref_hash);
    }
  }
#endif

  void FuzzMixedOperations(FuzzedDataProvider* fdp) {
    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(5, 20);

    for (size_t i = 0; i < iterations; ++i) {
      int32_t client_id = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_NWEB_ID);
      bool is_gpu_host = fdp->ConsumeBool();

      GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
      if (!channel) {
        continue;
      }

      GpuChannelMessageFilter* filter = channel->filter_.get();
      if (!filter) {
        continue;
      }

#if BUILDFLAG(ARKWEB_SAME_LAYER)
      if (fdp->ConsumeBool()) {
        int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);
        int32_t texture_owner_mode = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_TEXTURE_OWNER_MODE_VALUE);

        mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
        mojom::GpuChannel::CreateNativeTextureCallback callback;

        filter->CreateNativeTexture(native_id, texture_owner_mode, std::move(receiver), std::move(callback));
      }

      if (fdp->ConsumeBool()) {
        base::WeakPtr<GpuChannel> weak_channel = channel->AsWeakPtr();
        int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);
        gl::ohos::TextureOwnerMode mode = static_cast<gl::ohos::TextureOwnerMode>(
            fdp->ConsumeIntegralInRange<int32_t>(0, MAX_TEXTURE_OWNER_MODE_VALUE));

        mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;

        // int32_t result = TryCreateNativeTexture(weak_channel, native_id, mode, std::move(receiver));
        // (void)result;
      }
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
      if (fdp->ConsumeBool()) {
        uint32_t nweb_id = fdp->ConsumeIntegralInRange<uint32_t>(0, MAX_NWEB_ID);
        uint64_t blankless_key = fdp->ConsumeIntegral<uint64_t>();
        uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();
        int32_t lcp_time = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_LCP_TIME);
        int64_t pref_hash = fdp->ConsumeIntegral<int64_t>();

        filter->SetBlanklessDumpInfo(nweb_id, blankless_key, frame_sink_id, lcp_time, pref_hash);
      }
#endif

      if (fdp->ConsumeBool()) {
        filter->gpu_channel_ = nullptr;
      }
    }
  }

  void FuzzChannelDestroyed(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    GpuChannelMessageFilter* filter = channel->filter_.get();
    if (!filter) {
      return;
    }

#if BUILDFLAG(ARKWEB_SAME_LAYER)
    int32_t native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);
    int32_t texture_owner_mode = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_TEXTURE_OWNER_MODE_VALUE);

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    mojom::GpuChannel::CreateNativeTextureCallback callback;

    filter->CreateNativeTexture(native_id, texture_owner_mode, std::move(receiver), std::move(callback));
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
    uint32_t nweb_id = fdp->ConsumeIntegralInRange<uint32_t>(0, MAX_NWEB_ID);
    uint64_t blankless_key = fdp->ConsumeIntegral<uint64_t>();
    uint64_t frame_sink_id = fdp->ConsumeIntegral<uint64_t>();
    int32_t lcp_time = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_LCP_TIME);
    int64_t pref_hash = fdp->ConsumeIntegral<int64_t>();

    filter->SetBlanklessDumpInfo(nweb_id, blankless_key, frame_sink_id, lcp_time, pref_hash);
#endif

    filter->gpu_channel_ = nullptr;

#if BUILDFLAG(ARKWEB_SAME_LAYER)
    native_id = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_NATIVE_ID);
    texture_owner_mode = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_TEXTURE_OWNER_MODE_VALUE);

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver2;
    mojom::GpuChannel::CreateNativeTextureCallback callback2;

    filter->CreateNativeTexture(native_id, texture_owner_mode, std::move(receiver2), std::move(callback2));
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
    nweb_id = fdp->ConsumeIntegralInRange<uint32_t>(0, MAX_NWEB_ID);
    blankless_key = fdp->ConsumeIntegral<uint64_t>();
    frame_sink_id = fdp->ConsumeIntegral<uint64_t>();
    lcp_time = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_LCP_TIME);
    pref_hash = fdp->ConsumeIntegral<int64_t>();

    filter->SetBlanklessDumpInfo(nweb_id, blankless_key, frame_sink_id, lcp_time, pref_hash);
#endif
  }

  void RunFuzzTest(FuzzedDataProvider* fdp) {
    int32_t selector = fdp->ConsumeIntegralInRange<int32_t>(0, 17);

    switch (selector) {
#if BUILDFLAG(ARKWEB_SAME_LAYER)
      case 0:
        FuzzTryCreateNativeTextureWithNullChannel(fdp);
        break;
      case 1:
        FuzzTryCreateNativeTextureWithValidChannel(fdp);
        break;
      case 2:
        FuzzGpuChannelMessageFilterCreateNativeTexture(fdp);
        break;
      case 3:
        FuzzGpuChannelMessageFilterCreateNativeTextureNullChannel(fdp);
        break;
      case 4:
        FuzzCreateNativeTextureWithInvalidTextureOwnerMode(fdp);
        break;
      case 5:
        FuzzCreateNativeTextureWithDuplicateId(fdp);
        break;
      case 6:
        FuzzCreateNativeTextureBoundaryValues(fdp);
        break;
#endif
#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
      case 7:
        FuzzGpuChannelMessageFilterSetBlanklessDumpInfo(fdp);
        break;
      case 8:
        FuzzGpuChannelMessageFilterSetBlanklessDumpInfoNullChannel(fdp);
        break;
      case 9:
        FuzzGpuChannelMessageFilterSetBlanklessDumpInfoBoundaryValues(fdp);
        break;
      case 10:
        FuzzSetBlanklessDumpInfoMultipleTimes(fdp);
        break;
#endif
      case 11:
        FuzzMixedOperations(fdp);
        break;

      case 12:
        FuzzChannelDestroyed(fdp);
        break;

      default:
        FuzzMixedOperations(fdp);
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

  static gpu::GpuChannelForIncludeFuzzer* fuzzer = new gpu::GpuChannelForIncludeFuzzer();

  FuzzedDataProvider fdp(data, size);

  fuzzer->RunFuzzTest(&fdp);

  return 0;
}
#pragma clang diagnostic pop
