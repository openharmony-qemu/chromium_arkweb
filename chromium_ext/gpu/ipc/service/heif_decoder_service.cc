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

#include "gpu/ipc/service/heif_decoder_service.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "arkweb/chromium_ext/media/gpu/ohos/ohos_heif_image_decoder.h"

namespace gpu {

HeifDecoderService::HeifDecoderService() {
  LOG(INFO) << "[HeifSupport] HeifDecoderService created";
}

HeifDecoderService::~HeifDecoderService() {
  LOG(INFO) << "[HeifSupport] HeifDecoderService destroyed";
}

bool HeifDecoderService::DecodeImageSync(const std::vector<uint8_t>& heif_data,
                            std::vector<uint8_t>& pixels,
                            gfx::Size& size,
                            std::string& error) {
  LOG(INFO) << "[HeifSupport] DecodeImageSync called, data size: " 
            << heif_data.size();

  if (heif_data.size() == 0) {
    LOG(ERROR) << "[HeifSupport] Empty HEIF data";
    error = "Empty HEIF data";
    return false;
  }

  base::span<const uint8_t> data_span(heif_data.data(), heif_data.size());
  
  // 1. Lazy loading to initialize the decoder
  if (!decoder_) {
    decoder_ = std::make_unique<media::OhosHeifImageDecoder>();
    if (!decoder_->Initialize()) {
      LOG(ERROR) << "[HeifSupport] Failed to initialize OhosHeifImageDecoder";
      error = "Decoder initialization failed";
      return false;
    }
  }

  // 2. Decode HEIF data
  media::OhosImageDecodeStatus status = 
      decoder_->Decode(heif_data.data(), heif_data.size());
  if (status != media::OhosImageDecodeStatus::kSuccess) {
    error = base::StringPrintf(
        "Decode failed with status %d", static_cast<int>(status));
    LOG(ERROR) << "[HeifSupport] " << error;
    return false;
  }

  // 3. Get image dimensions
  size = gfx::Size(decoder_->GetImageWidth(), decoder_->GetImageHeight());

  // 4. Obtain pixel data (RGBA_8888)
  pixels = decoder_->GetPixelData();
  LOG(INFO) << "[HeifSupport] Successfully decoded to pixel buffer: " 
            << pixels.size() << " bytes (RGBA_8888)";
  return true;
}

}  // namespace gpu