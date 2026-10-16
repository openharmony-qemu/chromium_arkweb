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

#ifndef OHOS_NWEB_SRC_CAPI_NWEB_WEB_TRANSLATE_ITEM_H_
#define OHOS_NWEB_SRC_CAPI_NWEB_WEB_TRANSLATE_ITEM_H_

#include <string>

#include "arkweb/build/features/features.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif
#if BUILDFLAG(ARKWEB_NWEB_EX)
#include "ohos_nweb_ex/public/capi/nweb_basic_types.h"
#endif

struct LanguageDetectionDetails {
  std::string url;                      // The URL of the web page
  std::string content_language;         // The Content-Language HTTP header value
  std::string html_root_language;       // The lang attribute of the <html> element
  std::string model_detected_language;  // The language detected by the model
  std::string adopted_language;         // The final adopted language for translation

  void clear() {
    url.clear();
    content_language.clear();
    html_root_language.clear();
    model_detected_language.clear();
    adopted_language.clear();
  }
};

#if BUILDFLAG(ARKWEB_NWEB_EX)
typedef void (*OnWebLanguageDetectedCallback)(
    int32_t nweb_id,
    const ArkWebPbBuffer* pb_result_buffer);
#endif

#endif  // OHOS_NWEB_SRC_CAPI_NWEB_WEB_TRANSLATE_ITEM_H_
