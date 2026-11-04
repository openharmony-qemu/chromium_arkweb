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

#ifndef GPU_IPC_SERVICE_HEIF_DECODER_SERVICE_H_
#define GPU_IPC_SERVICE_HEIF_DECODER_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/geometry/size.h"
#include "arkweb/chromium_ext/media/gpu/ohos/ohos_heif_image_decoder.h"

namespace gpu {

class HeifDecoderService {
 public:
  HeifDecoderService();
  ~HeifDecoderService();

  HeifDecoderService(const HeifDecoderService&) = delete;
  HeifDecoderService& operator=(const HeifDecoderService&) = delete;


  bool DecodeImageSync(const std::vector<uint8_t>& heif_data,
                            std::vector<uint8_t>& pixels,
                            gfx::Size& size,
                            std::string& error);

 private:
  std::unique_ptr<media::OhosHeifImageDecoder> decoder_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_HEIF_DECODER_SERVICE_H_