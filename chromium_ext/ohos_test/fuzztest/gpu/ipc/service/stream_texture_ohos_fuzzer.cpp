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

#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/rect.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "gpu/ipc/service/gpu_channel.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <climits>
#include <cstdint>
#include <memory>

#define private public
#include "gpu/ipc/service/stream_texture_ohos.h"
#undef private

namespace gpu {

constexpr int32_t MAX_WIDTH = 4096;
constexpr int32_t MAX_HEIGHT = 4096;
constexpr GLuint MAX_SERVICE_ID = 10000;

class StreamTextureOhosFuzzer : public GpuChannelTestCommon {
 public:
  StreamTextureOhosFuzzer() : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~StreamTextureOhosFuzzer() override = default;

  void SetUp() override {}

  void TearDown() override {}

  void TestBody() override {}

  void FuzzCreate(FuzzedDataProvider* fdp) {
    int32_t stream_id = fdp->ConsumeIntegralInRange<int32_t>(1, 1000);

    int32_t client_id = fdp->ConsumeIntegralInRange<int32_t>(1, 100);
    bool is_gpu_host = fdp->ConsumeBool();

    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;

    int32_t mode_value = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
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

    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, stream_id, texture_owner_mode, std::move(receiver));

    if (stream_texture) {
      stream_texture->ReleaseChannel();
    }
  }

  void FuzzOnFrameAvailable(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    bool use_client = fdp->ConsumeBool();
    if (use_client) {
      mojo::PendingAssociatedRemote<mojom::StreamTextureClient> pending_client;
      stream_texture->StartListening(std::move(pending_client));
    }

    bool set_size = fdp->ConsumeBool();
    if (set_size) {
      int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
      int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
      gfx::Size rotated_visible_size(width, height);
      stream_texture->UpdateRotatedVisibleSize(rotated_visible_size);
    }

    stream_texture->OnFrameAvailable();
    stream_texture->ReleaseChannel();
  }

  void FuzzUpdateRotatedVisibleSize(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_HEIGHT);
    gfx::Size rotated_visible_size(width, height);

    bool set_pending = fdp->ConsumeBool();
    if (set_pending) {
      stream_texture->has_pending_frame_ = true;
    }

    stream_texture->UpdateRotatedVisibleSize(rotated_visible_size);
    stream_texture->ReleaseChannel();
  }

  void FuzzUpdateAndBindTexImage(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    GLuint service_id = fdp->ConsumeIntegralInRange<GLuint>(1, MAX_SERVICE_ID);
    stream_texture->UpdateAndBindTexImage(service_id);
    stream_texture->ReleaseChannel();
  }

  void FuzzIsUsingGpuMemory(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    (void)stream_texture->IsUsingGpuMemory();
    stream_texture->ReleaseChannel();
  }

  void FuzzHasTextureOwner(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    (void)stream_texture->HasTextureOwner();
    stream_texture->ReleaseChannel();
  }

  void FuzzGetTextureBase(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    (void)stream_texture->GetTextureBase();
    stream_texture->ReleaseChannel();
  }

  void FuzzNotifyOverlayPromotion(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    bool promotion = fdp->ConsumeBool();
    int32_t x = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_WIDTH);
    int32_t y = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_HEIGHT);
    int32_t width = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_WIDTH);
    int32_t height = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_HEIGHT);
    gfx::Rect bounds(x, y, width, height);

    stream_texture->NotifyOverlayPromotion(promotion, bounds);
    stream_texture->ReleaseChannel();
  }

  void FuzzTextureOwnerBindsTextureOnUpdate(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    (void)stream_texture->TextureOwnerBindsTextureOnUpdate();
    stream_texture->ReleaseChannel();
  }

  void FuzzNativeEmbedID(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    (void)stream_texture->NativeEmbedID();
    stream_texture->ReleaseChannel();
  }

  void FuzzGetNativeBuffer(FuzzedDataProvider* fdp) {
    int32_t client_id = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(client_id, is_gpu_host);
    if (!channel) {
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver;
    scoped_refptr<StreamTexture> stream_texture =
        StreamTexture::Create(channel, 1, gl::ohos::TextureOwnerMode::kNativeImageTexture, std::move(receiver));

    if (!stream_texture) {
      return;
    }

    (void)stream_texture->GetNativeBuffer();
    stream_texture->ReleaseChannel();
  }

  void FuzzMixedOperations(FuzzedDataProvider* fdp) {
    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(5, 20);

    for (size_t i = 0; i < iterations; ++i) {
      uint8_t op_type = fdp->ConsumeIntegralInRange<uint8_t>(0, 10);

      switch (op_type) {
        case 0:
          FuzzCreate(fdp);
          break;
        case 1:
          FuzzOnFrameAvailable(fdp);
          break;
        case 2:
          FuzzUpdateRotatedVisibleSize(fdp);
          break;
        case 3:
          FuzzUpdateAndBindTexImage(fdp);
          break;
        case 4:
          FuzzIsUsingGpuMemory(fdp);
          break;
        case 5:
          FuzzHasTextureOwner(fdp);
          break;
        case 6:
          FuzzGetTextureBase(fdp);
          break;
        case 7:
          FuzzNotifyOverlayPromotion(fdp);
          break;
        case 8:
          FuzzTextureOwnerBindsTextureOnUpdate(fdp);
          break;
        case 9:
          FuzzNativeEmbedID(fdp);
          break;
        case 10:
          FuzzGetNativeBuffer(fdp);
          break;
      }
    }
  }

  void RunFuzzTest(FuzzedDataProvider* fdp) {
    int32_t selector = fdp->ConsumeIntegralInRange<int32_t>(0, 11);

    switch (selector) {
      case 0:
        FuzzCreate(fdp);
        break;
      case 1:
        FuzzOnFrameAvailable(fdp);
        break;
      case 2:
        FuzzUpdateRotatedVisibleSize(fdp);
        break;
      case 3:
        FuzzUpdateAndBindTexImage(fdp);
        break;
      case 4:
        FuzzIsUsingGpuMemory(fdp);
        break;
      case 5:
        FuzzHasTextureOwner(fdp);
        break;
      case 6:
        FuzzGetTextureBase(fdp);
        break;
      case 7:
        FuzzNotifyOverlayPromotion(fdp);
        break;
      case 8:
        FuzzTextureOwnerBindsTextureOnUpdate(fdp);
        break;
      case 9:
        FuzzNativeEmbedID(fdp);
        break;
      case 10:
        FuzzGetNativeBuffer(fdp);
        break;
      case 11:
        FuzzMixedOperations(fdp);
        break;
      default:
        FuzzCreate(fdp);
        break;
    }
  }
};

class MockSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
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
}  // namespace gpu

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static bool initialized = []() {
    base::CommandLine::Init(0, nullptr);
    return true;
  }();

  static gpu::StreamTextureOhosFuzzer* fuzzer = new gpu::StreamTextureOhosFuzzer();

  FuzzedDataProvider fdp(data, size);

  int32_t test_selector = fdp.ConsumeIntegralInRange<int32_t>(0, 11);

  switch (test_selector) {
    case 0:
      fuzzer->FuzzCreate(&fdp);
      break;
    case 1:
      fuzzer->FuzzOnFrameAvailable(&fdp);
      break;
    case 2:
      fuzzer->FuzzUpdateRotatedVisibleSize(&fdp);
      break;
    case 3:
      fuzzer->FuzzUpdateAndBindTexImage(&fdp);
      break;
    case 4:
      fuzzer->FuzzIsUsingGpuMemory(&fdp);
      break;
    case 5:
      fuzzer->FuzzHasTextureOwner(&fdp);
      break;
    case 6:
      fuzzer->FuzzGetTextureBase(&fdp);
      break;
    case 7:
      fuzzer->FuzzNotifyOverlayPromotion(&fdp);
      break;
    case 8:
      fuzzer->FuzzTextureOwnerBindsTextureOnUpdate(&fdp);
      break;
    case 9:
      fuzzer->FuzzNativeEmbedID(&fdp);
      break;
    case 10:
      fuzzer->FuzzGetNativeBuffer(&fdp);
      break;
    case 11:
      fuzzer->FuzzMixedOperations(&fdp);
      break;
    default:
      fuzzer->FuzzCreate(&fdp);
      break;
  }

  return 0;
}
