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
#include <string>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#define private public
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#undef private
#include "arkweb/chromium_ext/third_party/blink/renderer/core/loader/arkweb_document_loader_ext.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Minimal WebDocumentSubresourceFilter whose GetLoadPolicy is controlled by the
// fuzzer so the adblock getter paths exercise both present/absent filter cases.
class FuzzWebDocumentSubresourceFilter
    : public WebDocumentSubresourceFilter {
 public:
  FuzzWebDocumentSubresourceFilter() = default;

  LoadPolicy GetLoadPolicy(const WebURL&,
                           network::mojom::RequestDestination,
                           subresource_filter::ScopedRule*) override {
    return kAllow;
  }
  LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL&) override {
    return kAllow;
  }
  LoadPolicy GetLoadPolicyForWebTransportConnect(const WebURL&) override {
    return kAllow;
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
};

class TestExtraData : public WebDocumentLoader::ExtraData {
 public:
  std::unique_ptr<ExtraData> Clone() override {
    return std::make_unique<TestExtraData>();
  }
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

static Persistent<ArkWebDocumentLoaderExt> CreateLoader(const KURL& url) {
  if (!g_page_holder) {
    return nullptr;
  }
  LocalFrame* frame = &(g_page_holder->GetFrame());
  auto nav_params = std::make_unique<WebNavigationParams>();
  nav_params->url = url;
  auto policy_container = PolicyContainer::CreateEmpty();
  auto extra = std::make_unique<TestExtraData>();
  return MakeGarbageCollected<ArkWebDocumentLoaderExt>(
      frame, WebNavigationType::kWebNavigationTypeOther,
      std::move(nav_params), std::move(policy_container), std::move(extra));
}

static KURL ConsumeKURL(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString(256);
  if (url_str.empty()) {
    return KURL("https://example.com/");
  }
  return KURL(String::FromUTF8(url_str));
}

#if BUILDFLAG(ARKWEB_ADBLOCK)
void FuzzGetWebSubresourceFilterNull(FuzzedDataProvider* fdp) {
  auto loader = CreateLoader(ConsumeKURL(fdp));
  if (!loader) {
    return;
  }
  WebDocumentSubresourceFilter* filter = loader->GetWebSubresourceFilter();
  (void)filter;
}

void FuzzGetWebUserSubresourceFilterNull(FuzzedDataProvider* fdp) {
  auto loader = CreateLoader(ConsumeKURL(fdp));
  if (!loader) {
    return;
  }
  WebDocumentSubresourceFilter* filter = loader->GetWebUserSubresourceFilter();
  (void)filter;
}

void FuzzSetUserSubresourceFilter(FuzzedDataProvider* fdp) {
  auto loader = CreateLoader(ConsumeKURL(fdp));
  if (!loader) {
    return;
  }
  ExecutionContext* context =
      g_page_holder->GetFrame().DomWindow()->GetExecutionContext();
  auto web_filter = std::make_unique<FuzzWebDocumentSubresourceFilter>();
  auto* filter = MakeGarbageCollected<SubresourceFilter>(context,
                                                         std::move(web_filter));
  loader->SetUserSubresourceFilter(filter);
  WebDocumentSubresourceFilter* returned = loader->GetWebUserSubresourceFilter();
  (void)returned;
}

void FuzzSetWebUserSubresourceFilter(FuzzedDataProvider* fdp) {
  auto loader = CreateLoader(ConsumeKURL(fdp));
  if (!loader) {
    return;
  }
  loader->SetWebUserSubresourceFilter(new FuzzWebDocumentSubresourceFilter());
  WebDocumentSubresourceFilter* returned = loader->GetWebUserSubresourceFilter();
  (void)returned;
}
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)

#if BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
void FuzzOnGetImageFromCache(FuzzedDataProvider* fdp) {
  auto loader = CreateLoader(ConsumeKURL(fdp));
  if (!loader) {
    return;
  }
  String url = String::FromUTF8(fdp->ConsumeRandomLengthString(256));
  scoped_refptr<const SharedBuffer> buffer = loader->OnGetImageFromCache(url);
  (void)buffer;
}
#endif  // BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
void FuzzNotifyLcpForBlankless(FuzzedDataProvider* fdp) {
  // Mix empty / about:blank / normal URLs to exercise both early-return and
  // client-notification branches.
  int32_t kind = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
  KURL url;
  switch (kind) {
    case 0:
      url = KURL();
      break;
    case 1:
      url = KURL("about:blank");
      break;
    default:
      url = ConsumeKURL(fdp);
      break;
  }
  auto loader = CreateLoader(url);
  if (!loader) {
    return;
  }
  loader->NotifyLcpForBlankless();
}
#endif  // BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 5);
  switch (selector) {
#if BUILDFLAG(ARKWEB_ADBLOCK)
    case 0:
      blink::FuzzGetWebSubresourceFilterNull(&fdp);
      break;
    case 1:
      blink::FuzzGetWebUserSubresourceFilterNull(&fdp);
      break;
    case 2:
      blink::FuzzSetUserSubresourceFilter(&fdp);
      break;
    case 3:
      blink::FuzzSetWebUserSubresourceFilter(&fdp);
      break;
#endif
#if BUILDFLAG(ARKWEB_MENU) || BUILDFLAG(IS_ARKWEB_EXT)
    case 4:
      blink::FuzzOnGetImageFromCache(&fdp);
      break;
#endif
#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
    case 5:
      blink::FuzzNotifyLcpForBlankless(&fdp);
      break;
#endif
    default:
#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
      blink::FuzzNotifyLcpForBlankless(&fdp);
#elif BUILDFLAG(ARKWEB_ADBLOCK)
      blink::FuzzGetWebSubresourceFilterNull(&fdp);
#endif
      break;
  }

  return 0;
}
