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

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
#include "arkweb/chromium_ext/components/viz/host/blankless_data_controller.h"
#include "base/task/thread_pool.h"
#endif

#if BUILDFLAG(ARKWEB_SAFEBROWSING) && BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/chromium_ext/components/viz/host/safebrowsing_data_controller.h"
#include "base/task/thread_pool.h"
#endif

#if BUILDFLAG(ARKWEB_REPORT_LOSS_FRAME)
#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#include "arkweb/ohos_adapter_ndk/interfaces/hiappevent_adapter.h"
#include "base/task/thread_pool.h"
#endif

#if BUILDFLAG(ARKWEB_OOP_GPU_PROCESS)
#include "mojo/public/cpp/bindings/callback_helpers.h"
#endif

namespace viz {

#if BUILDFLAG(ARKWEB_REPORT_LOSS_FRAME)
void GpuHostImpl::StartMonitor(int32_t nweb_id) {
  gpu_service_remote_->StartMonitor(nweb_id);
}

void GpuHostImpl::StopMonitor() {
  gpu_service_remote_->StopMonitor();
}

namespace {
void ReportDynamicStasticToHiAppevent(
    const OHOS::NWeb::HiAppeventAdapter::DynamicFrameDropInfo& info) {
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .GetHiAppeventAdapterInstance()
      .ReportDynamicStastic(OHOS::NWeb::HiAppeventAdapter::kDomain,
                            OHOS::NWeb::HiAppeventAdapter::kEventName,
                            OHOS::NWeb::HiAppeventAdapter::kEventTypeStatistic,
                            info);
}
}  // namespace

void GpuHostImpl::ReportDynamicFrameLoss(mojom::DynamicFrameDropDataPtr data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data) {
    return;
  }
  // Defense-in-depth: filter below-threshold data from GPU process
  if (data->max_app_frame_time < OHOS::NWeb::HiAppeventAdapter::kJankReportThreshold) {
    return;
  }
  OHOS::NWeb::HiAppeventAdapter::DynamicFrameDropInfo info;
  info.start_time = data->start_time;
  info.duration = static_cast<int64_t>(data->duration);
  info.max_app_frame_time = static_cast<int64_t>(data->max_app_frame_time);
  info.web_id = data->web_id;
  ReportDynamicStasticToHiAppevent(info);
}
#endif

#if BUILDFLAG(IS_ARKWEB)
void GpuHostImpl::SetVisible(int32_t nweb_id, bool visible) {
#if BUILDFLAG(ARKWEB_SLIDE_LTPO)
  gpu_service_remote_->SetVisible(nweb_id, visible);
#endif
}
#endif

#if BUILDFLAG(ARKWEB_SLIDE_LTPO)
void GpuHostImpl::SetHasTouchPoint(bool has_touch_point) {
  gpu_service_remote_->SetHasTouchPoint(has_touch_point);
}

void GpuHostImpl::ReportSlidingFrameRate(int32_t frame_rate) {
  gpu_service_remote_->ReportSlidingFrameRate(frame_rate);
}

void GpuHostImpl::SetLTPOStrategy(int32_t strategy) {
  gpu_service_remote_->SetLTPOStrategy(strategy);
}
#endif
#if BUILDFLAG(ARKWEB_INVISIBLE_RENDER_THROTTLE)
void GpuHostImpl::SetWebComponentVisible(int32_t nweb_id, bool visible) {
  gpu_service_remote_->SetWebComponentVisible(nweb_id, visible);
}

void GpuHostImpl::InitInvisibleThrottle(int32_t strategy, int32_t timeout, int32_t ratio) {
  gpu_service_remote_->InitInvisibleThrottle(strategy, timeout, ratio);
}
#endif

#if BUILDFLAG(ARKWEB_D_VSYNC)
void GpuHostImpl::SetIsFling(bool is_fling_enabled) {
  gpu_service_remote_->SetIsFling(is_fling_enabled);
}
#endif

#if BUILDFLAG(ARKWEB_OOP_GPU_PROCESS)
std::string GpuHostImpl::GetSurfaceId(int32_t native_embed_id) {
  LOG(DEBUG) << "get surface id = " << native_embed_id;
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync;
  std::string surface_id = "";
  gpu_service_remote_->GetSurfaceId(native_embed_id, &surface_id);
  return surface_id;
}

void GpuHostImpl::GetSurfaceId(int32_t native_embed_id,
                               GetSurfaceIdCallback callback) {
  LOG(DEBUG) << "get surface id async = " << native_embed_id;
  // Async path: no ScopedAllowSyncCall needed, the calling thread (UI) is not
  // blocked and the Mojo reply is delivered back on the binding thread (UI).
  // Wrap with a default invocation so the callback always runs even if the
  // GPU process drops the connection (e.g. crash), surfacing an empty string
  // that the caller treats as a hard error.
  gpu_service_remote_->GetSurfaceId(
      native_embed_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                  std::string()));
}

void GpuHostImpl::SetTransformHint(uint32_t rotation, uint32_t window_id) {
  LOG(DEBUG) << "SetTransformHint angle: " << rotation;
  gpu_service_remote_->SetTransformHint(rotation, window_id);
}

void GpuHostImpl::DestroyNativeWindow(uint32_t native_window_id) {
  LOG(DEBUG) << "destroy native window id = " << native_window_id;
  gpu_service_remote_->DestroyNativeWindow(native_window_id);
}
#endif

void GpuHostImpl::Discard(uint32_t native_window_id)
{
  if (!gpu_service_remote_) {
    return;
  }
  LOG(DEBUG) << "discard native window id = " << native_window_id;
  gpu_service_remote_->Discard(native_window_id);
}

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
void GpuHostImpl::SendBlanklessSnapshotInfo(mojom::BlanklessSendInfoPtr infoPtr,
                                            mojo::ScopedSharedBufferHandle buffer,
                                            mojom::BlanklessBitmapMetadataPtr metadata) {
  base::ThreadPool::PostTask(
    FROM_HERE,
    {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::TaskPriority::USER_BLOCKING},
    base::BindOnce(&GpuHostImpl::DumpBlanklessSnapshot,
                   std::move(infoPtr), std::move(buffer), std::move(metadata))
  );
}

void GpuHostImpl::DumpBlanklessSnapshot(mojom::BlanklessSendInfoPtr infoPtr,
                                        mojo::ScopedSharedBufferHandle buffer,
                                        mojom::BlanklessBitmapMetadataPtr metadata) {
  if (!infoPtr) {
    LOG(WARNING) << "blankless DumpBlanklessSnapshot invalid infoPtr.";
    return;
  }

  // the next all the process is sync, here we restore the skbitmap from mojo
  if (!buffer || !buffer.is_valid() || !metadata ||
      metadata->width <= 0 || metadata->height <= 0 || metadata->size == 0 ||
      metadata->color_type < static_cast<int32_t>(SkColorType::kUnknown_SkColorType) ||
      metadata->color_type > static_cast<int32_t>(SkColorType::kLastEnum_SkColorType) ||
      metadata->alpha_type < static_cast<int32_t>(SkAlphaType::kUnknown_SkAlphaType) ||
      metadata->alpha_type > static_cast<int32_t>(SkAlphaType::kLastEnum_SkAlphaType)) {
    LOG(WARNING) << "blankless GpuHostImpl::DumpBlanklessSnapshot invalid snapshot info";
    return;
  }

  TRACE_EVENT1("io", "blankless GpuHostImpl::DumpBlanklessSnapshot", "blankless_key", infoPtr->blankless_key);
  LOG(DEBUG) << "blankless GpuHostImpl::DumpBlanklessSnapshot begin : key " << infoPtr->blankless_key
             << ", lcp_time " << infoPtr->lcp_time << ", pref_hash " << infoPtr->pref_hash;
  auto& databaseAdapter = base::ohos::BlanklessDataController::GetInstance();
  databaseAdapter.DumpBlanklessSnapshot(std::move(infoPtr), std::move(buffer), std::move(metadata));
}

void GpuHostImpl::ClearBlanklessSnapshotInfo(uint64_t blankless_key) {
  TRACE_EVENT1("io", "blankless GpuHostImpl::ClearBlanklessSnapshotInfo", "blankless_key", blankless_key);
  LOG(DEBUG) << "blankless clear last snapshot info, blankless_key " << blankless_key;
  auto& databaseAdapter = base::ohos::BlanklessDataController::GetInstance();
  databaseAdapter.ClearSnapshot(blankless_key);
}
#endif

#if BUILDFLAG(ARKWEB_SAFEBROWSING)
void GpuHostImpl::SendAntiFraudSnapshotInfo(mojo::ScopedSharedBufferHandle buffer,
                                            mojom::SbBitmapMetadataPtr metadata) {
  base::ThreadPool::PostTask(
    FROM_HERE,
    {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::TaskPriority::USER_BLOCKING},
    base::BindOnce(&GpuHostImpl::DumpAntiFraudSnapshot,
                   std::move(buffer), std::move(metadata))
  );
}
 
void GpuHostImpl::DumpAntiFraudSnapshot(mojo::ScopedSharedBufferHandle buffer,
                                        mojom::SbBitmapMetadataPtr metadata) {
  // the next all the process is sync, here we restore the skbitmap from mojo
  if (!buffer || !buffer.is_valid() || !metadata ||
      metadata->width <= 0 || metadata->height <= 0 || metadata->size == 0 ||
      metadata->color_type < static_cast<int32_t>(SkColorType::kUnknown_SkColorType) ||
      metadata->color_type > static_cast<int32_t>(SkColorType::kLastEnum_SkColorType) ||
      metadata->alpha_type < static_cast<int32_t>(SkAlphaType::kUnknown_SkAlphaType) ||
      metadata->alpha_type > static_cast<int32_t>(SkAlphaType::kLastEnum_SkAlphaType)) {
    LOG(WARNING) << "safebrowsing GpuHostImpl::DumpAntiFraudSnapshot invalid snapshot info";
    return;
  }

#if BUILDFLAG(IS_ARKWEB_EXT)
  TRACE_EVENT0("io", "safebrowsing GpuHostImpl::DumpAntiFraudSnapshot");
  auto& databaseAdapter = base::ohos::SafeBrowsingDataController::GetInstance();
  databaseAdapter.DumpAntiFraudSnapshot(std::move(buffer), std::move(metadata));
#endif
}
 
void GpuHostImpl::RequestSnapshotOnce() {
  if (!gpu_service_remote_) {
    return;
  }
  gpu_service_remote_->RequestSnapshotOnce();
}
#endif

} // namespace viz