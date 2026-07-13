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
#include <memory>
#include <optional>
#include "arkweb/build/features/features.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
namespace blink {
// ResourceFinishAsError is defined in
// arkweb/chromium_ext/.../resource_fetcher_for_include.cc (textually included
// into resource_fetcher.cc). It has external linkage but no header declaration.
#if BUILDFLAG(ARKWEB_ADBLOCK)
bool ResourceFinishAsError(FetchContext& context,
                           Resource* resource,
                           ResourceRequest& last_resource_request,
                           FetchParameters& params,
                           scoped_refptr<base::SingleThreadTaskRunner>
                               freezable_task_runner);
#endif
// FetchContext subclass whose CanRequest always returns nullopt (request is
// allowed), to exercise the non-blocking branch of ResourceFinishAsError.
class AllowAllFetchContext : public FetchContext {
 public:
  std::optional<ResourceRequestBlockedReason> CanRequest(
      ResourceType,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      ReportingDisposition,
      base::optional_ref<const ResourceRequest::RedirectInfo>) const override {
    return std::nullopt;
  }
};
static KURL ConsumeKURL(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(256);
  if (url_str.empty()) {
    return KURL("https://example.com/test");
  }
  return KURL(String::FromUTF8(url_str));
}
static ResourceType ConsumeResourceType(FuzzedDataProvider* fdp) {
  // Restrict to the valid (non-test) range of ResourceType values.
  int32_t value = fdp->ConsumeIntegralInRange<int32_t>(
      static_cast<int32_t>(ResourceType::kImage),
      static_cast<int32_t>(ResourceType::kVideo));
  return static_cast<ResourceType>(value);
}
#if BUILDFLAG(ARKWEB_ADBLOCK)
void FuzzResourceFinishAsErrorBlocked(FuzzedDataProvider* fdp) {
  // FetchContext::NullInstance() CanRequest returns kOther, so the request is
  // always blocked and FinishAsError is invoked.
  FetchContext& context = FetchContext::NullInstance();
  KURL url = ConsumeKURL(fdp);
  ResourceType type = ConsumeResourceType(fdp);
  ResourceRequest request(url);
  auto* resource = RawResource::CreateForTest(request, type);
  FetchParameters params = FetchParameters::CreateForTest(ResourceRequest(url));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  ResourceFinishAsError(context, resource, request, params, task_runner);
}
void FuzzResourceFinishAsErrorAllowed(FuzzedDataProvider* fdp) {
  auto* allow_context = MakeGarbageCollected<AllowAllFetchContext>();
  KURL url = ConsumeKURL(fdp);
  ResourceType type = ConsumeResourceType(fdp);
  ResourceRequest request(url);
  auto* resource = RawResource::CreateForTest(request, type);
  FetchParameters params = FetchParameters::CreateForTest(ResourceRequest(url));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  ResourceFinishAsError(*allow_context, resource, request, params, task_runner);
}
void FuzzResourceFinishAsErrorEmptyUrl(FuzzedDataProvider* fdp) {
  FetchContext& context = FetchContext::NullInstance();
  KURL url;  // empty/invalid URL
  ResourceType type = ConsumeResourceType(fdp);
  ResourceRequest request(url);
  auto* resource = RawResource::CreateForTest(request, type);
  FetchParameters params = FetchParameters::CreateForTest(ResourceRequest(url));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  ResourceFinishAsError(context, resource, request, params, task_runner);
}
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)
}  // namespace blink
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }
  static blink::BlinkFuzzerTestSupport test_support;
  blink::test::TaskEnvironment task_environment;
  FuzzedDataProvider fdp(data, size);
  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 2);
  switch (selector) {
#if BUILDFLAG(ARKWEB_ADBLOCK)
    case 0:
      blink::FuzzResourceFinishAsErrorBlocked(&fdp);
      break;
    case 1:
      blink::FuzzResourceFinishAsErrorAllowed(&fdp);
      break;
    case 2:
      blink::FuzzResourceFinishAsErrorEmptyUrl(&fdp);
      break;
#endif
    default:
#if BUILDFLAG(ARKWEB_ADBLOCK)
      blink::FuzzResourceFinishAsErrorBlocked(&fdp);
#endif
      break;
  }
  return 0;
}