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
#include "arkweb/chromium_ext/third_party/blink/renderer/core/loader/resource/css_style_sheet_resource_utils.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "content/public/test/blink_test_environment.h"
#include "base/command_line.h"

#include <memory>
#include <string>

namespace blink {

constexpr size_t kMaxIterations = 10;
constexpr size_t kMaxStringLength = 128;

static std::unique_ptr<content::BlinkTestEnvironment> g_blink_environment = nullptr;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    g_blink_environment = std::make_unique<content::BlinkTestEnvironment>();
    g_blink_environment->SetUp();
  }

  ~Environment() {
    g_blink_environment.reset();
  }
};

#if BUILDFLAG(ARKWEB_INJECT_OFFLINE_RESOURCE)
void FuzzCreateForOfflineResourceValid(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);

  KURL kurl(String::FromUTF8(url_str.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  CSSStyleSheetResource* resource =
      CSSStyleSheetResourceUtils::CreateForOfflineResource(kurl, origin_url);
}

void FuzzCreateForOfflineResourceEmptyURL(FuzzedDataProvider* fdp) {
  KURL empty_url;
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  CSSStyleSheetResource* resource =
      CSSStyleSheetResourceUtils::CreateForOfflineResource(empty_url, origin_url);
}

void FuzzCreateForOfflineResourceInvalidURL(FuzzedDataProvider* fdp) {
  std::string invalid_url = fdp->ConsumeRandomLengthString(kMaxStringLength);
  KURL kurl(String::FromUTF8(invalid_url.c_str()));
  KURL origin_url(kurl);

  CSSStyleSheetResource* resource =
      CSSStyleSheetResourceUtils::CreateForOfflineResource(kurl, origin_url);
}

void FuzzCreateForOfflineResourceWithNullOrigin(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  KURL kurl(String::FromUTF8(url_str.c_str()));
  KURL null_origin;

  CSSStyleSheetResource* resource =
      CSSStyleSheetResourceUtils::CreateForOfflineResource(kurl, null_origin);
}

void FuzzCreateForOfflineResourceDataURL(FuzzedDataProvider* fdp) {
  std::string data_url = "data:image/png;base64," + fdp->ConsumeRandomLengthString(64);
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);

  KURL kurl(String::FromUTF8(data_url.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  CSSStyleSheetResource* resource =
      CSSStyleSheetResourceUtils::CreateForOfflineResource(kurl, origin_url);
}

void FuzzCreateForOfflineResourceBlobURL(FuzzedDataProvider* fdp) {
  std::string blob_url = "blob:" + fdp->ConsumeRandomLengthString(64);
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);

  KURL kurl(String::FromUTF8(blob_url.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  CSSStyleSheetResource* resource =
      CSSStyleSheetResourceUtils::CreateForOfflineResource(kurl, origin_url);
}

void FuzzConstructorAndTrace(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);

  KURL kurl(String::FromUTF8(url_str.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  CSSStyleSheetResource* resource =
      CSSStyleSheetResourceUtils::CreateForOfflineResource(kurl, origin_url);

  if (resource) {
    CSSStyleSheetResourceUtils* utils = MakeGarbageCollected<CSSStyleSheetResourceUtils>(resource);
    utils->cssStyleSheetResource = resource;
  }
}

void FuzzMultipleImageResources(FuzzedDataProvider* fdp) {
  for (int j = 0; j < 5; j++) {
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
    std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);

    KURL kurl(String::FromUTF8(url_str.c_str()));
    KURL origin_url(String::FromUTF8(origin_str.c_str()));

  CSSStyleSheetResource* resource =
      CSSStyleSheetResourceUtils::CreateForOfflineResource(kurl, origin_url);
  }
}
#endif

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);
  size_t iterations = std::min(fdp.ConsumeIntegralInRange<size_t>(1, blink::kMaxIterations), size);

  for (size_t i = 0; i < iterations; ++i) {
#if BUILDFLAG(ARKWEB_INJECT_OFFLINE_RESOURCE)
    int scenario = fdp.ConsumeIntegralInRange<int>(0, 7);

    switch (scenario) {
      case 0:
        blink::FuzzCreateForOfflineResourceValid(&fdp);
        break;
      case 1:
        blink::FuzzCreateForOfflineResourceEmptyURL(&fdp);
        break;
      case 2:
        blink::FuzzCreateForOfflineResourceInvalidURL(&fdp);
        break;
      case 3:
        blink::FuzzCreateForOfflineResourceWithNullOrigin(&fdp);
        break;
      case 4:
        blink::FuzzCreateForOfflineResourceDataURL(&fdp);
        break;
      case 5:
        blink::FuzzCreateForOfflineResourceBlobURL(&fdp);
        break;
      case 6:
        blink::FuzzConstructorAndTrace(&fdp);
        break;
      case 7:
        blink::FuzzMultipleImageResources(&fdp);
        break;
      default:
        break;
    }
#endif
  }

  return 0;
}
