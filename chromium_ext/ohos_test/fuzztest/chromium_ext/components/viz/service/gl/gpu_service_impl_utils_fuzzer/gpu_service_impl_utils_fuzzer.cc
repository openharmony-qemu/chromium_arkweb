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

#include <fuzzer/FuzzedDataProvider.h>

#include "base/functional/callback.h"

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/system/buffer.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/gpu_extra_info.h"

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#endif

#if BUILDFLAG(ARKWEB_OOP_GPU_PROCESS)
#include "gpu/ipc/common/gpu_surface_id_tracker.h"
#endif

#define private public
#include "arkweb/chromium_ext/components/viz/common/frame_sinks/arkweb_copy_output_result_utils.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#undef private

namespace viz {

constexpr size_t kMaxInputSize = 4096;
constexpr uint8_t kMaxScenario = 18;

class FuzzEnvironment {
 public:
  FuzzEnvironment() : io_thread_("FuzzIOThread"), gpu_thread_("FuzzGpuThread") {}

  std::unique_ptr<base::SingleThreadTaskExecutor> task_executor_;
  void Init() {
    task_executor_ = std::make_unique<base::SingleThreadTaskExecutor>();
    mojo::core::Init();
    CHECK(io_thread_.Start());
    CHECK(gpu_thread_.StartWithOptions(base::Thread::Options(base::MessagePumpType::DEFAULT, 0)));

    base::WaitableEvent event;
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([this, &event]() {
          gpu::GPUInfo gpu_info;
          gpu_info.in_process_gpu = false;
          GpuServiceImpl::InitParams init_params;
          init_params.io_runner = io_thread_.task_runner();

          gpu_service_ = std::make_unique<GpuServiceImpl>(
              gpu::GpuPreferences(), gpu_info, gpu::GpuFeatureInfo(),
              std::optional<gpu::GPUInfo>(), std::optional<gpu::GpuFeatureInfo>(),
              gfx::GpuExtraInfo(), std::move(init_params));
        #if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
            mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
            std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
            gpu_service_->gpu_host_ =
                mojo::SharedRemote<mojom::GpuHost>(std::move(gpu_host_proxy));
        #endif
          event.Signal();
        }));
    event.Wait();
  }

  void Shutdown() {
    base::WaitableEvent event;
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([this, &event]() {
          gpu_service_.reset();
          event.Signal();
        }));
    event.Wait();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
    io_thread_.Stop();
    gpu_thread_.Stop();
  }

  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }

  base::Thread* gpu_thread() { return &gpu_thread_; }

 private:
  base::Thread io_thread_;
  base::Thread gpu_thread_;
  std::unique_ptr<GpuServiceImpl> gpu_service_;
};

FuzzEnvironment* g_env = nullptr;

#if BUILDFLAG(ARKWEB_OOP_GPU_PROCESS)
void FuzzGetSurfaceId(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  int32_t native_embed_id = provider.ConsumeIntegral<int32_t>();

  std::string surface_id =
      "fuzz_test_surface_id_" + std::to_string(native_embed_id);
  gpu::GpuSurfaceIdTracker::SurfaceRecord record(123, surface_id);
  gpu::GpuSurfaceIdTracker::Get()->AddSurfaceForNativeWidget(
      std::move(record));

  g_env->gpu_service()->GetSurfaceId(
      native_embed_id, base::BindOnce([](const std::string&) {}));
}

void FuzzSetTransformHint(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  uint32_t rotation = provider.ConsumeIntegral<uint32_t>();
  uint32_t window_id = provider.ConsumeIntegral<uint32_t>();
  g_env->gpu_service()->SetTransformHint(rotation, window_id);
}

void FuzzDestroyNativeWindow(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  uint32_t native_window_id = provider.ConsumeIntegral<uint32_t>();
  g_env->gpu_service()->DestroyNativeWindow(native_window_id);
}

void FuzzDiscard(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  uint32_t native_window_id = provider.ConsumeIntegral<uint32_t>();
  g_env->gpu_service()->Discard(native_window_id);
}
#endif

#if BUILDFLAG(ARKWEB_DFX_DUMP)
void FuzzDumpGpuInfo(const uint8_t* data, size_t size) {
  g_env->gpu_service()->DumpGpuInfo(
      base::BindOnce([](float total_size) { (void)total_size; }));
}
#endif

#if BUILDFLAG(ARKWEB_REPORT_LOSS_FRAME)
void FuzzStartMonitor(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  int32_t nweb_id = provider.ConsumeIntegral<int32_t>();
  g_env->gpu_service()->StartMonitor(nweb_id);
}

void FuzzStopMonitor(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  g_env->gpu_service()->StopMonitor();
}
#endif

#if BUILDFLAG(ARKWEB_SLIDE_LTPO)
void FuzzSetVisible(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  int32_t nweb_id = provider.ConsumeIntegral<int32_t>();
  bool visible = provider.ConsumeBool();
  g_env->gpu_service()->SetVisible(nweb_id, visible);
}

void FuzzSetHasTouchPoint(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  bool has_touch_point = provider.ConsumeBool();
  g_env->gpu_service()->SetHasTouchPoint(has_touch_point);
}

void FuzzReportSlidingFrameRate(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  int32_t frame_rate = provider.ConsumeIntegral<int32_t>();
  g_env->gpu_service()->ReportSlidingFrameRate(frame_rate);
}

void FuzzSetLTPOStrategy(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  int32_t strategy = provider.ConsumeIntegral<int32_t>();
  g_env->gpu_service()->SetLTPOStrategy(strategy);
}
#endif

#if BUILDFLAG(ARKWEB_D_VSYNC)
void FuzzSetIsFling(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  bool is_fling = provider.ConsumeBool();
  g_env->gpu_service()->SetIsFling(is_fling);
}
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
void FuzzOnFrameSnapshotCopyOutputResultNullptr(const uint8_t* data,
                                                size_t size) {
  g_env->gpu_service()->OnFrameSnapshotCopyOutputResult(nullptr);
}

void FuzzOnFrameSnapshotCopyOutputResultInvalidBlankless(
    const uint8_t* data,
    size_t size) {
  FuzzedDataProvider provider(data, size);
  int32_t width = provider.ConsumeIntegralInRange<int32_t>(0, 1920);
  int32_t height = provider.ConsumeIntegralInRange<int32_t>(0, 1080);

  auto result = std::make_unique<CopyOutputResult>(
      CopyOutputResult::Format::RGBA,
      CopyOutputResult::Destination::kSystemMemory,
      gfx::Rect(0, 0, width, height), false);
  g_env->gpu_service()->OnFrameSnapshotCopyOutputResult(std::move(result));
}

void FuzzOnFrameSnapshotCopyOutputResultEmptyBitmap(const uint8_t* data,
                                                    size_t size) {
  FuzzedDataProvider provider(data, size);
  int32_t width = provider.ConsumeIntegralInRange<int32_t>(1, 1920);
  int32_t height = provider.ConsumeIntegralInRange<int32_t>(1, 1080);
  uint64_t blankless_key = provider.ConsumeIntegral<uint64_t>();

  auto result = std::make_unique<CopyOutputResult>(
      CopyOutputResult::Format::RGBA,
      CopyOutputResult::Destination::kSystemMemory,
      gfx::Rect(0, 0, width, height), false);
  result->copy_output_result_utils()->info_.blankless_key = blankless_key;
  g_env->gpu_service()->OnFrameSnapshotCopyOutputResult(std::move(result));
}

void FuzzOnFrameSnapshotCopyOutputResultSkBitmap(const uint8_t* data,
                                                 size_t size) {
  FuzzedDataProvider provider(data, size);
  int32_t width = provider.ConsumeIntegralInRange<int32_t>(1, 100);
  int32_t height = provider.ConsumeIntegralInRange<int32_t>(1, 100);
  uint64_t blankless_key = provider.ConsumeIntegral<uint64_t>();
  if (blankless_key == base::ohos::BlanklessController::INVALID_BLANKLESS_KEY) {
    blankless_key = 1;
  }

  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);

  auto result = std::make_unique<CopyOutputSkBitmapResult>(
      gfx::Rect(0, 0, width, height), std::move(bitmap));
  result->copy_output_result_utils()->info_.blankless_key = blankless_key;
  g_env->gpu_service()->OnFrameSnapshotCopyOutputResult(std::move(result));
}

void FuzzGetWeakPtr(const uint8_t* data, size_t size) {
  auto weak_ptr = g_env->gpu_service()->GetWeakPtr();
  (void)weak_ptr;
}

void FuzzSendBlanklessSnapshotInfo(const uint8_t* data, size_t size) {
    FuzzedDataProvider provider(data, size);
    uint64_t blankless_key = provider.ConsumeIntegral<uint64_t>();
    uint32_t nweb_id = provider.ConsumeIntegral<uint32_t>();
    
    mojom::BlanklessSendInfoPtr info_ptr = mojom::BlanklessSendInfo::New();
    info_ptr->blankless_key = blankless_key;
    info_ptr->nweb_id = nweb_id;
    
    mojom::BlanklessBitmapMetadataPtr metadata =
        mojom::BlanklessBitmapMetadata::New();
    metadata->width = 100;
    metadata->height = 100;
    metadata->color_type = 0;
    metadata->alpha_type = 0;
    metadata->size = 40000;
    
    mojo::ScopedSharedBufferHandle buffer =
        mojo::SharedBufferHandle::Create(1024);
    
    g_env->gpu_service()->SendBlanklessSnapshotInfo(
        std::move(info_ptr), std::move(buffer), std::move(metadata));
}

void FuzzClearBlanklessSnapshotInfo(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  uint64_t blankless_key = provider.ConsumeIntegral<uint64_t>();
  g_env->gpu_service()->ClearBlanklessSnapshotInfo(blankless_key);
}
#endif

// }  // namespace
}  // namespace viz

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  viz::g_env = new viz::FuzzEnvironment();
  viz::g_env->Init();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!data || size == 0)
    return 0;
  if (size > viz::kMaxInputSize)
    return 0;

  FuzzedDataProvider provider(data, size);
  uint8_t scenario =
      provider.ConsumeIntegralInRange<uint8_t>(0, viz::kMaxScenario);

  auto remaining = provider.ConsumeRemainingBytes<uint8_t>();
  const uint8_t* rem_data = remaining.data();
  size_t rem_size = remaining.size();

  switch (scenario) {
#if BUILDFLAG(ARKWEB_OOP_GPU_PROCESS)
    case 0:
      viz::FuzzGetSurfaceId(rem_data, rem_size);
      break;
    case 1:
      viz::FuzzSetTransformHint(rem_data, rem_size);
      break;
    case 2:
      viz::FuzzDestroyNativeWindow(rem_data, rem_size);
      break;
    case 3:
      viz::FuzzDiscard(rem_data, rem_size);
      break;
#endif
#if BUILDFLAG(ARKWEB_DFX_DUMP)
    case 4:
      viz::FuzzDumpGpuInfo(rem_data, rem_size);
      break;
#endif
#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
    case 5:
      viz::FuzzOnFrameSnapshotCopyOutputResultNullptr(rem_data, rem_size);
      break;
    case 6:
      viz::FuzzOnFrameSnapshotCopyOutputResultInvalidBlankless(rem_data,
                                                               rem_size);
      break;
    case 7:
      viz::FuzzOnFrameSnapshotCopyOutputResultEmptyBitmap(rem_data, rem_size);
      break;
    case 8:
      viz::FuzzOnFrameSnapshotCopyOutputResultSkBitmap(rem_data, rem_size);
      break;
    case 9:
      viz::FuzzGetWeakPtr(rem_data, rem_size);
      break;
    case 10:
      viz::FuzzSendBlanklessSnapshotInfo(rem_data, rem_size);
      break;
    case 11:
      viz::FuzzClearBlanklessSnapshotInfo(rem_data, rem_size);
      break;
#endif
#if BUILDFLAG(ARKWEB_REPORT_LOSS_FRAME)
    case 12:
      viz::FuzzStartMonitor(rem_data, rem_size);
      break;
    case 13:
      viz::FuzzStopMonitor(rem_data, rem_size);
      break;
#endif
#if BUILDFLAG(ARKWEB_SLIDE_LTPO)
    case 14:
      viz::FuzzSetVisible(rem_data, rem_size);
      break;
    case 15:
      viz::FuzzSetHasTouchPoint(rem_data, rem_size);
      break;
    case 16:
      viz::FuzzReportSlidingFrameRate(rem_data, rem_size);
      break;
    case 17:
      viz::FuzzSetLTPOStrategy(rem_data, rem_size);
      break;
#endif
#if BUILDFLAG(ARKWEB_D_VSYNC)
    case 18:
      viz::FuzzSetIsFling(rem_data, rem_size);
      break;
#endif
    default:
      break;
  }

  return 0;
}
