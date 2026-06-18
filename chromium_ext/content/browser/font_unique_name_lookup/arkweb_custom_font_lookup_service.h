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

#ifndef CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_ARKWEB_CUSTOM_FONT_LOOKUP_SERVICE_H_
#define CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_ARKWEB_CUSTOM_FONT_LOOKUP_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "arkweb/chromium_ext/third_party/blink/public/mojom/custom_font_lookup/arkweb_custom_font_lookup.mojom.h"

namespace content {

class ArkwebCustomFontLookupService
    : public blink::mojom::ArkwebCustomFontLookup {
public:
  ArkwebCustomFontLookupService() = default;
  virtual ~ArkwebCustomFontLookupService() = default;

  ArkwebCustomFontLookupService(const ArkwebCustomFontLookupService &) = delete;
  ArkwebCustomFontLookupService &
  operator=(const ArkwebCustomFontLookupService &) = delete;

  static void
      Create(mojo::PendingReceiver<blink::mojom::ArkwebCustomFontLookup>);
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  void GetUniqueNameLookupTable(
      blink::mojom::ArkwebCustomFontLookup::GetUniqueNameLookupTableCallback
          callback) override;
  void MatchLocalFontByUniqueName(
      const std::string &font_unique_name,
      blink::mojom::ArkwebCustomFontLookup::MatchLocalFontByUniqueNameCallback
          callback) override;
  void FetchAllFontFiles(
      blink::mojom::ArkwebCustomFontLookup::FetchAllFontFilesCallback callback)
      override;

private:
  base::WeakPtrFactory<ArkwebCustomFontLookupService> weak_factory_{this};
};
}  // namespace content
#endif  // CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_ARKWEB_CUSTOM_FONT_LOOKUP_SERVICE_H_
