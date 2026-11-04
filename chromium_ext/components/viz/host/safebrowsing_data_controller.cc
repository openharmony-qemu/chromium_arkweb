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
 
#include "safebrowsing_data_controller.h"
 
#include "arkweb/chromium_ext/components/viz/service/display/afd_frame_snapshot_copy_output_request.h"
#include "arkweb/ohos_adapter_ndk/ohos_image_adapter/ohos_image_encoder_adapter.h"
#include "arkweb/ohos_nweb_ex/overrides/ohos_nweb/src/cef_delegate/nweb_anti_fraud_detection_handler.h"
#include "base/logging.h"
#include "cef/ohos_cef_ext/libcef/browser/ohos_safe_browsing/ohos_sb_snapshot_info.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
 
using namespace OHOS::NWeb;
 
namespace base {
namespace ohos {
 
SafeBrowsingDataController& SafeBrowsingDataController::GetInstance() {
  static SafeBrowsingDataController instance;
  return instance;
}
 
SafeBrowsingDataController::SafeBrowsingDataController() = default;
 
void SafeBrowsingDataController::DumpAntiFraudSnapshot(
    mojo::ScopedSharedBufferHandle buffer,
    viz::mojom::SbBitmapMetadataPtr metadata) {
 
  mojo::ScopedSharedBufferMapping mapping = buffer->Map(metadata->size);
  if (!mapping) {
    LOG(ERROR) << "safebrowsing DumpAntiFraudSnapshot failed to map shared buffer";
    return;
  }
 
  SkImageInfo info = SkImageInfo::Make(
      metadata->width, metadata->height,
      static_cast<SkColorType>(metadata->color_type),
      static_cast<SkAlphaType>(metadata->alpha_type));
  SkBitmap bitmap;
  if (!bitmap.installPixels(info, mapping.get(), info.minRowBytes())) {
    LOG(ERROR) << "safebrowsing DumpAntiFraudSnapshot failed to install pixels into bitmap";
    return;
  }
 
  LOG(DEBUG) << "safebrowsing DumpAntiFraudSnapshot bitmap info: width=" << bitmap.width()
             << ", height=" << bitmap.height() << ", size=" << metadata->size;
 
  OHOS::NWeb::NWebAntiFraudDetectionHandler::GetInstance().SetFmpUrl(
      ohos_safe_browsing::SafeBrowsingSnapshotManager::GetInstance().GetPendingFmpUrl());
  OHOS::NWeb::NWebAntiFraudDetectionHandler::GetInstance().HandleAntiFraudDetection(
      OHOS::NWeb::OhosImageEncoderAdapter::GetInstance().CreatePixelmap(bitmap),
      ohos_safe_browsing::SafeBrowsingSnapshotManager::
          GetInstance().GetAntiFraudDetectingResult());
  ohos_safe_browsing::SafeBrowsingSnapshotManager::GetInstance().ResetFmp();
}
 
}  // namespace ohos
}  // namespace base