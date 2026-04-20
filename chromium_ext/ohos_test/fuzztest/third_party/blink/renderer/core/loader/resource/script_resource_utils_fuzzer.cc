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
#include "arkweb/chromium_ext/third_party/blink/renderer/core/loader/resource/script_resource_utils.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "net/http/http_status_code.h"

#include <memory>
#include <string>

namespace blink {

constexpr size_t kMaxIterations = 10;
constexpr size_t kMaxStringLength = 128;
constexpr size_t kMaxHeaderCount = 10;

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
void FuzzCreateForOfflineResourceClassicJS(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url_str.empty()) {
    url_str = "https://example.com/classic.js";
  }
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin_str.empty()) {
    origin_str = "https://example.com";
  }

  KURL kurl(String::FromUTF8(url_str.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  ResourceResponse response(kurl);
  response.SetCurrentRequestUrl(kurl);

  ScriptResource* resource =
      ScriptResourceUtils::CreateForOfflineResource(kurl, origin_url, response, false);
}

void FuzzCreateForOfflineResourceModuleJS(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url_str.empty()) {
    url_str = "https://example.com/module.js";
  }
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin_str.empty()) {
    origin_str = "https://example.com";
  }

  KURL kurl(String::FromUTF8(url_str.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  ResourceResponse response(kurl);
  response.SetCurrentRequestUrl(kurl);

  ScriptResource* resource =
      ScriptResourceUtils::CreateForOfflineResource(kurl, origin_url, response, true);
}

void FuzzCreateForOfflineResourceWithCrossOrigin(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url_str.empty()) {
    url_str = "https://example.com/script.js";
  }
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin_str.empty()) {
    origin_str = "https://example.com";
  }

  KURL kurl(String::FromUTF8(url_str.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  ResourceResponse response(kurl);
  response.SetCurrentRequestUrl(kurl);
  response.SetHttpHeaderField(AtomicString("Cross-Origin"),
                               AtomicString(fdp->ConsumeRandomLengthString(32).c_str()));

  bool is_module = fdp->ConsumeBool();
  ScriptResource* resource =
      ScriptResourceUtils::CreateForOfflineResource(kurl, origin_url, response, is_module);
}

void FuzzCreateForOfflineResourceWithUseCredentials(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url_str.empty()) {
    url_str = "https://example.com/script.js";
  }
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin_str.empty()) {
    origin_str = "https://example.com";
  }

  KURL kurl(String::FromUTF8(url_str.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  ResourceResponse response(kurl);
  response.SetCurrentRequestUrl(kurl);
  response.SetHttpHeaderField(AtomicString("Cross-Origin"), AtomicString("use-credentials"));

  bool is_module = fdp->ConsumeBool();
  ScriptResource* resource =
      ScriptResourceUtils::CreateForOfflineResource(kurl, origin_url, response, is_module);
}

void FuzzCreateForOfflineResourceWithMultipleHeaders(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url_str.empty()) {
    url_str = "https://example.com/script.js";
  }
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin_str.empty()) {
    origin_str = "https://example.com";
  }

  KURL kurl(String::FromUTF8(url_str.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  ResourceResponse response(kurl);
  response.SetCurrentRequestUrl(kurl);
  response.SetHttpStatusCode(net::HTTP_OK);
  response.SetMimeType(AtomicString("application/javascript"));

  size_t header_count = fdp->ConsumeIntegralInRange<size_t>(0, kMaxHeaderCount);
  for (size_t j = 0; j < header_count; j++) {
    std::string key = fdp->ConsumeRandomLengthString(32);
    std::string value = fdp->ConsumeRandomLengthString(64);
    response.SetHttpHeaderField(AtomicString(key.c_str()), AtomicString(value.c_str()));
  }

  bool is_module = fdp->ConsumeBool();
  ScriptResource* resource =
      ScriptResourceUtils::CreateForOfflineResource(kurl, origin_url, response, is_module);
}

void FuzzConstructorAndTrace(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url_str.empty()) {
    url_str = "https://example.com/script.js";
  }
  std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin_str.empty()) {
    origin_str = "https://example.com";
  }

  KURL kurl(String::FromUTF8(url_str.c_str()));
  KURL origin_url(String::FromUTF8(origin_str.c_str()));

  ResourceResponse response(kurl);
  response.SetCurrentRequestUrl(kurl);

  ScriptResource* resource =
      ScriptResourceUtils::CreateForOfflineResource(kurl, origin_url, response, fdp->ConsumeBool());

  if (resource) {
    ScriptResourceUtils* utils = MakeGarbageCollected<ScriptResourceUtils>(resource);
    utils->scriptResource = resource;
  }
}

void FuzzCreateMultipleScriptResources(FuzzedDataProvider* fdp) {
  for (int j = 0; j < 5; j++) {
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
    if (url_str.empty()) {
      url_str = "https://example.com/script" + std::to_string(j) + ".js";
    }
    std::string origin_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
    if (origin_str.empty()) {
      origin_str = "https://example.com";
    }

    KURL kurl(String::FromUTF8(url_str.c_str()));
    KURL origin_url(String::FromUTF8(origin_str.c_str()));

    ResourceResponse response(kurl);
    response.SetCurrentRequestUrl(kurl);

    bool is_module = fdp->ConsumeBool();
    ScriptResource* resource =
        ScriptResourceUtils::CreateForOfflineResource(kurl, origin_url, response, is_module);
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
    int scenario = fdp.ConsumeIntegralInRange<int>(0, 6);

    switch (scenario) {
      case 0:
        blink::FuzzCreateForOfflineResourceClassicJS(&fdp);
        break;
      case 1:
        blink::FuzzCreateForOfflineResourceModuleJS(&fdp);
        break;
      case 2:
        blink::FuzzCreateForOfflineResourceWithCrossOrigin(&fdp);
        break;
      case 3:
        blink::FuzzCreateForOfflineResourceWithUseCredentials(&fdp);
        break;
      case 4:
        blink::FuzzCreateForOfflineResourceWithMultipleHeaders(&fdp);
        break;
      case 5:
        blink::FuzzConstructorAndTrace(&fdp);
        break;
      case 6:
        blink::FuzzCreateMultipleScriptResources(&fdp);
        break;
      default:
        break;
    }
#endif
  }

  return 0;
}