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


#ifndef COMPONENTS_SERVICES_FONT_DATA_FONT_DATA_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_FONT_DATA_FONT_DATA_SERVICE_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "base/files/file.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "arkweb/chromium_ext/components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace font_data_service {

// FontDataService (receiver) manages font requests from the renderer.
// Does the following:
//     1) Construct the SkTypeface based on the font request details via
//     DWriteFactory.
//     2) Store that SkTypeface as an SkStreamAsset (wrapper of the font data as
//     raw bytes) into a shared memory map to be fetched by the renderer.
//     3) Cache the value for future requests.
//
// Instantiated in the browser process and lives on a sequence running in the
// thread pool.
//
// This is meant to replace the existing renderer font integration.
// FontDataServiceImpl is the replacement to DWriteFontProxyImpl. This currently
// only runs on Windows desktop browser as part of an experiment: see
// crbug.com/335680565 for more details.
class FontDataServiceImpl : public mojom::FontDataService {
public:
  FontDataServiceImpl();

  FontDataServiceImpl(const FontDataServiceImpl&) = delete;
  FontDataServiceImpl& operator=(const FontDataServiceImpl&) = delete;

  ~FontDataServiceImpl() override;
  void BindReceiver(mojo::PendingReceiver<mojom::FontDataService> receiver);
  static void ConnectToFontService(
      mojo::PendingReceiver<mojom::FontDataService> receiver);

  void GetFontConfigInfo(GetFontConfigInfoCallback callback) override;

  void GetStylishFontList(int32_t fontType,
                          GetStylishFontListCallback callback) override;
private:

  mojo::ReceiverSet<mojom::FontDataService> receivers_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace font_data_service

#endif  // COMPONENTS_SERVICES_FONT_DATA_FONT_DATA_SERVICE_IMPL_H_
