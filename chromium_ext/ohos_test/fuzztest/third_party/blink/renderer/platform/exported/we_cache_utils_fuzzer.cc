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

#include "fuzzer/FuzzedDataProvider.h"

#include "arkweb/chromium_ext/third_party/blink/renderer/platform/exported/we_cache_utils.h"
#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace blink {

constexpr size_t kMaxIterations = 10;
constexpr size_t kMaxStringLength = 128;
constexpr size_t kMaxHeaderCount = 10;
constexpr size_t kMaxResourceSize = 4096;

static std::unique_ptr<content::BlinkTestEnvironmentWithIsolate> g_blink_environment = nullptr;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    g_blink_environment = std::make_unique<content::BlinkTestEnvironmentWithIsolate>();
    g_blink_environment->SetUp();
  }

  ~Environment() {
    g_blink_environment.reset();
  }
};

#if BUILDFLAG(ARKWEB_INJECT_OFFLINE_RESOURCE)
void FuzzAddResourceToCacheImage(FuzzedDataProvider* fdp) {
  MemoryCache* cache = MemoryCache::Get();
  if (!cache) {
    return;
  }

  std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url.empty()) {
    url = "https://example.com/test.js";
  }
  std::string origin = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin.empty()) {
    origin = "https://example.com";
  }

  size_t resource_size = fdp->ConsumeIntegralInRange<size_t>(0, kMaxResourceSize);
  std::vector<uint8_t> resource(resource_size);
  fdp->ConsumeData(resource.data(), resource_size);

  base::flat_map<std::string, std::string> response_headers;
  size_t header_count = fdp->ConsumeIntegralInRange<size_t>(0, kMaxHeaderCount);
  for (size_t i = 0; i < header_count; i++) {
    std::string key = fdp->ConsumeRandomLengthString(64);
    std::string value = fdp->ConsumeRandomLengthString(64);
    response_headers[key] = value;
  }

  uint64_t type = static_cast<uint64_t>(OfflineResourceType::IMAGE);
  WebCacheUtils::AddResourceToCache(url, origin, resource, response_headers, type);
}

void FuzzAddResourceToCacheCSS(FuzzedDataProvider* fdp) {
  MemoryCache* cache = MemoryCache::Get();
  if (!cache) {
    return;
  }

  std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url.empty()) {
    url = "https://example.com/test.js";
  }
  std::string origin = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin.empty()) {
    origin = "https://example.com";
  }

  size_t resource_size = fdp->ConsumeIntegralInRange<size_t>(0, kMaxResourceSize);
  std::vector<uint8_t> resource(resource_size);
  fdp->ConsumeData(resource.data(), resource_size);

  base::flat_map<std::string, std::string> response_headers;
  size_t header_count = fdp->ConsumeIntegralInRange<size_t>(0, kMaxHeaderCount);
  for (size_t i = 0; i < header_count; i++) {
    std::string key = fdp->ConsumeRandomLengthString(64);
    std::string value = fdp->ConsumeRandomLengthString(64);
    response_headers[key] = value;
  }

  uint64_t type = static_cast<uint64_t>(OfflineResourceType::CSS);
  WebCacheUtils::AddResourceToCache(url, origin, resource, response_headers, type);
}

void FuzzAddResourceToCacheClassicJS(FuzzedDataProvider* fdp) {
  MemoryCache* cache = MemoryCache::Get();
  if (!cache) {
    return;
  }

  std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url.empty()) {
    url = "https://example.com/test.js";
  }
  std::string origin = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin.empty()) {
    origin = "https://example.com";
  }

  size_t resource_size = fdp->ConsumeIntegralInRange<size_t>(0, kMaxResourceSize);
  std::vector<uint8_t> resource(resource_size);
  fdp->ConsumeData(resource.data(), resource_size);

  base::flat_map<std::string, std::string> response_headers;
  size_t header_count = fdp->ConsumeIntegralInRange<size_t>(0, kMaxHeaderCount);
  for (size_t i = 0; i < header_count; i++) {
    std::string key = fdp->ConsumeRandomLengthString(64);
    std::string value = fdp->ConsumeRandomLengthString(64);
    response_headers[key] = value;
  }

  uint64_t type = static_cast<uint64_t>(OfflineResourceType::CLASSIC_JS);
  WebCacheUtils::AddResourceToCache(url, origin, resource, response_headers, type);
}

void FuzzAddResourceToCacheModuleJS(FuzzedDataProvider* fdp) {
  MemoryCache* cache = MemoryCache::Get();
  if (!cache) {
    return;
  }

  std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url.empty()) {
    url = "https://example.com/test.js";
  }
  std::string origin = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin.empty()) {
    origin = "https://example.com";
  }

  size_t resource_size = fdp->ConsumeIntegralInRange<size_t>(0, kMaxResourceSize);
  std::vector<uint8_t> resource(resource_size);
  fdp->ConsumeData(resource.data(), resource_size);

  base::flat_map<std::string, std::string> response_headers;
  size_t header_count = fdp->ConsumeIntegralInRange<size_t>(0, kMaxHeaderCount);
  for (size_t i = 0; i < header_count; i++) {
    std::string key = fdp->ConsumeRandomLengthString(64);
    std::string value = fdp->ConsumeRandomLengthString(64);
    response_headers[key] = value;
  }

  uint64_t type = static_cast<uint64_t>(OfflineResourceType::MODULE_JS);
  WebCacheUtils::AddResourceToCache(url, origin, resource, response_headers, type);
}

void FuzzConstructor(FuzzedDataProvider* fdp) {
  WebCache* web_cache = nullptr;
  WebCacheUtils utils(web_cache);
}

void FuzzConstructorWithValidCache(FuzzedDataProvider* fdp) {
  WebCacheUtils utils(nullptr);
}

void FuzzAddResourceToCacheEmptyResource(FuzzedDataProvider* fdp) {
  std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url.empty()) {
    url = "https://example.com/test.js";
  }
  std::string origin = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin.empty()) {
    origin = "https://example.com";
  }
  std::vector<uint8_t> resource;
  base::flat_map<std::string, std::string> response_headers;
  uint64_t type = static_cast<uint64_t>(OfflineResourceType::CSS);
  WebCacheUtils::AddResourceToCache(url, origin, resource, response_headers, type);
}

void FuzzAddResourceToCacheWithMinimalURL(FuzzedDataProvider* fdp) {
  std::string url = "https://test.com/resource";
  std::string origin = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin.empty()) {
    origin = "https://example.com";
  }
  size_t resource_size = fdp->ConsumeIntegralInRange<size_t>(1, kMaxResourceSize);
  std::vector<uint8_t> resource(resource_size);
  fdp->ConsumeData(resource.data(), resource_size);
  base::flat_map<std::string, std::string> response_headers;
  uint64_t type = static_cast<uint64_t>(OfflineResourceType::CLASSIC_JS);
  WebCacheUtils::AddResourceToCache(url, origin, resource, response_headers, type);
}

void FuzzAddResourceToCacheInvalidType(FuzzedDataProvider* fdp) {
  std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url.empty()) {
    url = "https://example.com/test.js";
  }
  std::string origin = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin.empty()) {
    origin = "https://example.com";
  }
  size_t resource_size = fdp->ConsumeIntegralInRange<size_t>(1, kMaxResourceSize);
  std::vector<uint8_t> resource(resource_size);
  fdp->ConsumeData(resource.data(), resource_size);
  base::flat_map<std::string, std::string> response_headers;
  uint64_t type = fdp->ConsumeIntegralInRange<uint64_t>(100, 999);
  WebCacheUtils::AddResourceToCache(url, origin, resource, response_headers, type);
}

void FuzzAddResourceToCacheWithCacheControl(FuzzedDataProvider* fdp) {
  std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (url.empty()) {
    url = "https://example.com/test.js";
  }
  std::string origin = fdp->ConsumeRandomLengthString(kMaxStringLength);
  if (origin.empty()) {
    origin = "https://example.com";
  }
  size_t resource_size = fdp->ConsumeIntegralInRange<size_t>(1, kMaxResourceSize);
  std::vector<uint8_t> resource(resource_size);
  fdp->ConsumeData(resource.data(), resource_size);
  base::flat_map<std::string, std::string> response_headers;
  response_headers["Cache-Control"] = "max-age=" + std::to_string(fdp->ConsumeIntegralInRange<int>(0, 3600));
  response_headers["Content-Type"] = "application/javascript";
  uint64_t type = static_cast<uint64_t>(OfflineResourceType::CLASSIC_JS);
  WebCacheUtils::AddResourceToCache(url, origin, resource, response_headers, type);
}

void FuzzMultipleResources(FuzzedDataProvider* fdp) {
  for (int j = 0; j < 5; j++) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    std::string origin = fdp->ConsumeRandomLengthString(kMaxStringLength);
    size_t resource_size = fdp->ConsumeIntegralInRange<size_t>(0, kMaxResourceSize);
    std::vector<uint8_t> resource(resource_size);
    fdp->ConsumeData(resource.data(), resource_size);
    base::flat_map<std::string, std::string> response_headers;
    uint64_t type = static_cast<uint64_t>(fdp->ConsumeIntegralInRange<int>(0, 3));
    WebCacheUtils::AddResourceToCache(url, origin, resource, response_headers, type);
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
    int scenario = fdp.ConsumeIntegralInRange<int>(0, 11);

    switch (scenario) {
      case 0:
        blink::FuzzAddResourceToCacheImage(&fdp);
        break;
      case 1:
        blink::FuzzAddResourceToCacheCSS(&fdp);
        break;
      case 2:
        blink::FuzzAddResourceToCacheClassicJS(&fdp);
        break;
      case 3:
        blink::FuzzAddResourceToCacheModuleJS(&fdp);
        break;
      case 4:
        blink::FuzzConstructor(&fdp);
        break;
      case 5:
        blink::FuzzConstructorWithValidCache(&fdp);
        break;
      case 6:
        blink::FuzzAddResourceToCacheEmptyResource(&fdp);
        break;
      case 7:
        blink::FuzzAddResourceToCacheWithMinimalURL(&fdp);
        break;
      case 8:
        blink::FuzzAddResourceToCacheInvalidType(&fdp);
        break;
      case 9:
        blink::FuzzAddResourceToCacheWithCacheControl(&fdp);
        break;
      case 10:
        blink::FuzzMultipleResources(&fdp);
        break;
      default:
        break;
    }
#endif
  }

  return 0;
}
