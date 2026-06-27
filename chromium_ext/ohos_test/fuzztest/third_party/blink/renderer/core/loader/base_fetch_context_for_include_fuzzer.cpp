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
#include <string>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class WebSocketHandshakeThrottle;

// Minimal WebDocumentSubresourceFilter whose GetLoadPolicy is controlled by the
// fuzzer, so SubresourceFilter::AllowLoad can be made to allow or block.
class FuzzWebDocumentSubresourceFilter
    : public WebDocumentSubresourceFilter {
 public:
  explicit FuzzWebDocumentSubresourceFilter(LoadPolicy policy)
      : policy_(policy) {}

  LoadPolicy GetLoadPolicy(const WebURL&,
                           network::mojom::RequestDestination,
                           subresource_filter::ScopedRule*) override {
    return policy_;
  }
  LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL&) override {
    return policy_;
  }
  LoadPolicy GetLoadPolicyForWebTransportConnect(const WebURL&) override {
    return policy_;
  }
  void ReportDisallowedLoad() override {}
  bool ShouldLogToConsole() override { return false; }

#if BUILDFLAG(ARKWEB_ADBLOCK)
  void ClearStatistics() override {}
  std::unique_ptr<std::string> GetElementHidingSelectors(const WebURL&,
                                                         bool) override {
    return nullptr;
  }
  bool HasGenericHideTypeOption(const WebURL&, const url::Origin&) override {
    return false;
  }
  bool HasElemHideTypeOption(const WebURL&, const url::Origin&) override {
    return false;
  }
  bool HasDocumentTypeOption(const WebURL&, const url::Origin&) override {
    return false;
  }
  void DidMatchCssRule(const WebURL&, const std::string&, bool) override {}
  void SetDidFinishLoad(bool) override {}
  bool GetDidFinishLoad() override { return false; }
  std::unique_ptr<std::vector<std::string>> GetUserDomPathSelectors(
      const WebURL&, bool) override {
    return nullptr;
  }
  void set_activation_state(bool) override {}
#endif

 private:
  LoadPolicy policy_;
};

// BaseFetchContext subclass that exposes a fuzzer-controlled user subresource
// filter. It mirrors MockBaseFetchContext from base_fetch_context_test.cc,
// keeping the pure-virtual overrides inert so only the
// CanRequestBasedOnSubresourceFilterOnly path is exercised.
class FuzzBaseFetchContext final : public BaseFetchContext {
 public:
  // Mirrors MockBaseFetchContext from base_fetch_context_test.cc, adding a
  // fuzzer-controlled user_filter_ so GetUserSubresourceFilter() can return a
  // real filter. The DetachableResourceFetcherProperties stored by BaseFetchContext
  // transitively roots the TestResourceFetcherProperties and the
  // FetchClientSettingsObjectImpl, so no extra member is needed for them.
  FuzzBaseFetchContext(const DetachableResourceFetcherProperties& properties,
                       ExecutionContext* execution_context,
                       SubresourceFilter* user_filter)
      : BaseFetchContext(
            properties,
            MakeGarbageCollected<DetachableConsoleLogger>(execution_context)),
        execution_context_(execution_context),
        user_filter_(user_filter) {}
  ~FuzzBaseFetchContext() override = default;

  net::SiteForCookies GetSiteForCookies() const override {
    return net::SiteForCookies();
  }
  scoped_refptr<const blink::SecurityOrigin> GetTopFrameOrigin()
      const override {
    return SecurityOrigin::CreateUniqueOpaque();
  }
  bool AllowScript() const override { return false; }
  SubresourceFilter* GetSubresourceFilter() const override { return nullptr; }
  bool ShouldBlockRequestByInspector(const KURL&) const override {
    return false;
  }
  void DispatchDidBlockRequest(const ResourceRequest&,
                               const ResourceLoaderOptions&,
                               ResourceRequestBlockedReason,
                               ResourceType) const override {}
  ContentSecurityPolicy* GetContentSecurityPolicyForWorld(
      const DOMWrapperWorld*) const override {
    return GetContentSecurityPolicy();
  }
  bool IsIsolatedSVGChromeClient() const override { return false; }
  void CountUsage(WebFeature) const override {}
  void CountDeprecation(WebFeature) const override {}
  bool ShouldBlockWebSocketByMixedContentCheck(const KURL&) const override {
    return false;
  }
  std::unique_ptr<WebSocketHandshakeThrottle> CreateWebSocketHandshakeThrottle()
      override {
    return nullptr;
  }
  bool ShouldBlockFetchByMixedContentCheck(
      mojom::blink::RequestContextType,
      network::mojom::blink::IPAddressSpace,
      base::optional_ref<const ResourceRequest::RedirectInfo>,
      const KURL&,
      ReportingDisposition,
      const String&) const override {
    return false;
  }
  bool ShouldBlockFetchAsCredentialedSubresource(const ResourceRequest&,
                                                 const KURL&) const override {
    return false;
  }
  const KURL& Url() const override { return execution_context_->Url(); }
  ContentSecurityPolicy* GetContentSecurityPolicy() const override {
    return execution_context_->GetContentSecurityPolicy();
  }
  ExecutionContext* GetExecutionContext() const override {
    return execution_context_.Get();
  }

#if BUILDFLAG(ARKWEB_ADBLOCK)
  SubresourceFilter* GetUserSubresourceFilter() const override {
    return user_filter_.Get();
  }
#endif

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    visitor->Trace(user_filter_);
    BaseFetchContext::Trace(visitor);
  }

 private:
  Member<ExecutionContext> execution_context_;
  Member<SubresourceFilter> user_filter_;
};

static std::unique_ptr<DummyPageHolder> g_page_holder = nullptr;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>();
  }
  ~Environment() { g_page_holder.reset(); }
  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

static KURL ConsumeKURL(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(256);
  if (url_str.empty()) {
    return KURL("https://example.com/sub");
  }
  return KURL(String::FromUTF8(url_str));
}

#if BUILDFLAG(ARKWEB_ADBLOCK)
static Member<FuzzBaseFetchContext> CreateContext(
    SubresourceFilter* user_filter) {
  ExecutionContext* context =
      g_page_holder->GetFrame().DomWindow()->GetExecutionContext();
  auto* settings_object =
      MakeGarbageCollected<FetchClientSettingsObjectImpl>(*context);
  auto* properties =
      MakeGarbageCollected<TestResourceFetcherProperties>(*settings_object);
  return MakeGarbageCollected<FuzzBaseFetchContext>(
      properties->MakeDetachable(), context, user_filter);
}

static Member<SubresourceFilter> CreateUserFilter(
    WebDocumentSubresourceFilter::LoadPolicy policy) {
  ExecutionContext* context =
      g_page_holder->GetFrame().DomWindow()->GetExecutionContext();
  auto web_filter = std::make_unique<FuzzWebDocumentSubresourceFilter>(policy);
  return MakeGarbageCollected<SubresourceFilter>(context,
                                                 std::move(web_filter));
}

void FuzzCanRequestBasedOnSubresourceFilterOnlyNull(FuzzedDataProvider* fdp) {
  // No user subresource filter: the ARKWEB_ADBLOCK branch is skipped and the
  // request is allowed (nullopt).
  auto context = CreateContext(nullptr);
  if (!context) {
    return;
  }
  KURL url = ConsumeKURL(fdp);
  ResourceRequest request(url);
  request.SetRequestDestination(network::mojom::RequestDestination::kDocument);
  ResourceLoaderOptions options(nullptr /* world */);
  (void)context->CanRequestBasedOnSubresourceFilterOnly(
      ResourceType::kImage, request, url, options,
      ReportingDisposition::kReport, request.GetRedirectInfo());
}

void FuzzCanRequestBasedOnSubresourceFilterOnlyAllowed(
    FuzzedDataProvider* fdp) {
  auto filter = CreateUserFilter(WebDocumentSubresourceFilter::kAllow);
  auto context = CreateContext(filter);
  if (!context) {
    return;
  }
  KURL url = ConsumeKURL(fdp);
  ResourceRequest request(url);
  request.SetRequestDestination(network::mojom::RequestDestination::kDocument);
  ResourceLoaderOptions options(nullptr /* world */);
  (void)context->CanRequestBasedOnSubresourceFilterOnly(
      ResourceType::kImage, request, url, options,
      ReportingDisposition::kReport, request.GetRedirectInfo());
}

void FuzzCanRequestBasedOnSubresourceFilterOnlyBlocked(
    FuzzedDataProvider* fdp) {
  auto filter = CreateUserFilter(WebDocumentSubresourceFilter::kDisallow);
  auto context = CreateContext(filter);
  if (!context) {
    return;
  }
  KURL url = ConsumeKURL(fdp);
  ResourceRequest request(url);
  request.SetRequestDestination(network::mojom::RequestDestination::kDocument);
  ResourceLoaderOptions options(nullptr /* world */);
  (void)context->CanRequestBasedOnSubresourceFilterOnly(
      ResourceType::kImage, request, url, options,
      ReportingDisposition::kReport, request.GetRedirectInfo());
}

void FuzzCanRequestBasedOnSubresourceFilterOnlySilent(
    FuzzedDataProvider* fdp) {
  // ReportingDisposition::kSuppressReporting skips DispatchDidBlockRequest.
  auto filter = CreateUserFilter(WebDocumentSubresourceFilter::kDisallow);
  auto context = CreateContext(filter);
  if (!context) {
    return;
  }
  KURL url = ConsumeKURL(fdp);
  ResourceRequest request(url);
  request.SetRequestDestination(network::mojom::RequestDestination::kDocument);
  ResourceLoaderOptions options(nullptr /* world */);
  (void)context->CanRequestBasedOnSubresourceFilterOnly(
      ResourceType::kImage, request, url, options,
      ReportingDisposition::kSuppressReporting, request.GetRedirectInfo());
}
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 3);
  switch (selector) {
#if BUILDFLAG(ARKWEB_ADBLOCK)
    case 0:
      blink::FuzzCanRequestBasedOnSubresourceFilterOnlyNull(&fdp);
      break;
    case 1:
      blink::FuzzCanRequestBasedOnSubresourceFilterOnlyAllowed(&fdp);
      break;
    case 2:
      blink::FuzzCanRequestBasedOnSubresourceFilterOnlyBlocked(&fdp);
      break;
    case 3:
      blink::FuzzCanRequestBasedOnSubresourceFilterOnlySilent(&fdp);
      break;
#endif
    default:
#if BUILDFLAG(ARKWEB_ADBLOCK)
      blink::FuzzCanRequestBasedOnSubresourceFilterOnlyNull(&fdp);
#endif
      break;
  }

  return 0;
}
