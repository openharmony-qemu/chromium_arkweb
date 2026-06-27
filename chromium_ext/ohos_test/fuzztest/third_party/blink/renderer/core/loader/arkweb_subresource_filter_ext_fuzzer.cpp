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
#include "arkweb/chromium_ext/third_party/blink/renderer/core/loader/arkweb_subresource_filter_ext.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#define private public
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#undef private
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Minimal WebDocumentSubresourceFilter implementation whose GetLoadPolicy is
// controlled by the fuzzer. All other ARKWEB_ADBLOCK-only methods return inert
// defaults so the ext code paths remain deterministic.
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
    return KURL("https://example.com/page");
  }
  return KURL(String::FromUTF8(url_str));
}

#if BUILDFLAG(ARKWEB_ADBLOCK)
static Member<ArkWebSubresourceFilterExt> CreateExt(
    WebDocumentSubresourceFilter::LoadPolicy policy) {
  if (!g_page_holder) {
    return nullptr;
  }
  ExecutionContext* context =
      g_page_holder->GetFrame().DomWindow()->GetExecutionContext();
  auto web_filter = std::make_unique<FuzzWebDocumentSubresourceFilter>(policy);
  auto* filter = MakeGarbageCollected<SubresourceFilter>(context,
                                                          std::move(web_filter));
  return MakeGarbageCollected<ArkWebSubresourceFilterExt>(filter);
}

void FuzzGetElementHidingSelectors(FuzzedDataProvider* fdp) {
  auto ext = CreateExt(WebDocumentSubresourceFilter::kAllow);
  if (!ext) {
    return;
  }
  KURL url = ConsumeKURL(fdp);
  bool need_common = fdp->ConsumeBool();
  ext->GetElementHidingSelectors(url, need_common);
}

void FuzzGetWebDocumentSubresourceFilter(FuzzedDataProvider* fdp) {
  auto ext = CreateExt(
      static_cast<WebDocumentSubresourceFilter::LoadPolicy>(
          fdp->ConsumeIntegralInRange<int32_t>(0, 2)));
  if (!ext) {
    return;
  }
  WebDocumentSubresourceFilter* web_filter = ext->GetWebDocumentSubresourceFilter();
  (void)web_filter;
}

void FuzzDidMatchCssRule(FuzzedDataProvider* fdp) {
  auto ext = CreateExt(WebDocumentSubresourceFilter::kAllow);
  if (!ext) {
    return;
  }
  KURL url = ConsumeKURL(fdp);
  std::string dom_path = fdp->ConsumeRandomLengthString(128);
  bool is_for_report = fdp->ConsumeBool();
  ext->DidMatchCssRule(url, dom_path, is_for_report);
}

void FuzzGetDidFinishLoad(FuzzedDataProvider* fdp) {
  auto ext = CreateExt(
      static_cast<WebDocumentSubresourceFilter::LoadPolicy>(
          fdp->ConsumeIntegralInRange<int32_t>(0, 2)));
  if (!ext) {
    return;
  }
  (void)ext->GetDidFinishLoad();
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
      blink::FuzzGetElementHidingSelectors(&fdp);
      break;
    case 1:
      blink::FuzzGetWebDocumentSubresourceFilter(&fdp);
      break;
    case 2:
      blink::FuzzDidMatchCssRule(&fdp);
      break;
    case 3:
      blink::FuzzGetDidFinishLoad(&fdp);
      break;
#endif
    default:
#if BUILDFLAG(ARKWEB_ADBLOCK)
      blink::FuzzGetElementHidingSelectors(&fdp);
#endif
      break;
  }

  return 0;
}
