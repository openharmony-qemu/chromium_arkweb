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
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/loader/arkweb_document_loader_ext.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/loader/arkweb_subresource_filter_ext.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/gfx/geometry/size.h"
#include "sanitizer/common_interface_defs.h"

namespace blink {

#if BUILDFLAG(ARKWEB_ADBLOCK)
// Forward declaration of the target. The symbol is defined in
// arkweb/chromium_ext/.../element_rule_collector_for_include.cc and merged into
// the core/css translation unit via #include.
void DidMatchCssRuleForInclude(const RuleData& rule_data,
                               const ElementResolveContext& context,
                               MatchResult& result);
#endif

namespace {

#if BUILDFLAG(ARKWEB_ADBLOCK)

class StubWebDocumentSubresourceFilter : public WebDocumentSubresourceFilter {
 public:
  StubWebDocumentSubresourceFilter() = default;

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

  void ClearStatistics() override {}
  std::unique_ptr<std::string> GetElementHidingSelectors(const WebURL&,
                                                        bool) override {
    return std::make_unique<std::string>();
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
  void DidMatchCssRule(const WebURL&, const std::string&, bool) override {
    ++match_count_;
  }
  void SetDidFinishLoad(bool finished) override { did_finish_load_ = finished; }
  bool GetDidFinishLoad() override { return did_finish_load_; }
  std::unique_ptr<std::vector<std::string>> GetUserDomPathSelectors(
      const WebURL&,
      bool) override {
    return std::make_unique<std::vector<std::string>>();
  }
  void set_activation_state(bool) override {}

  size_t match_count() const { return match_count_; }

 private:
  bool did_finish_load_ = false;
  size_t match_count_ = 0;
};

struct GlobalFixture {
  std::unique_ptr<DummyPageHolder> page_holder;
  Persistent<Element> element;

  ~GlobalFixture() {
    page_holder.release();
    element.Clear();
  }
};

static std::unique_ptr<GlobalFixture> g_fixture;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();

    auto fixture = std::make_unique<GlobalFixture>();
    fixture->page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    Document& doc = fixture->page_holder->GetDocument();
    auto* body = doc.body();
    if (body) {
      auto* div = doc.CreateRawElement(html_names::kDivTag);
      div->setAttribute(html_names::kIdAttr, AtomicString("fuzz-target"));
      div->setAttribute(html_names::kClassAttr, AtomicString("fuzz-class"));
      body->AppendChild(div);
      fixture->element = div;
    } else {
      fixture->element = doc.documentElement();
    }
    g_fixture = std::move(fixture);
  }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

StyleRule* BuildStyleRule(Document& doc,
                          bool for_adblock,
                          bool for_user_adblock,
                          const String& selector_text) {
  css_test_helpers::TestStyleSheet sheet;
  String rule_text = selector_text + " { color: tomato; }";
  sheet.AddCSSRules(rule_text);
  RuleSet& rule_set = sheet.GetRuleSet();
  base::span<const RuleData> rules = rule_set.IdRules(AtomicString("fuzz-target"));
  if (rules.empty()) {
    rules = rule_set.ClassRules(AtomicString("fuzz-class"));
  }
  if (rules.empty()) {
    rules = rule_set.UniversalRules();
  }
  if (rules.empty()) {
    return nullptr;
  }
  StyleRule* style_rule = rules.front().Rule();
  if (!style_rule) {
    return nullptr;
  }
  style_rule->SetForAdBlock(for_adblock);
  style_rule->SetForUserAdBlock(for_user_adblock);
  return style_rule;
}

// Build a RuleData wrapping `rule` we can pass by const-ref. The RuleData
// constructor needs a bloom_hash_backing storage area whose lifetime exceeds
// the RuleData; we keep it on the stack of the caller.
RuleData MakeRuleData(StyleRule* rule, Vector<uint16_t>& backing) {
  return RuleData(rule, /*selector_index=*/0u, /*position=*/0u,
                  /*style_scope=*/nullptr, kRuleHasNoSpecialState, backing);
}

void InstallSystemFilter(DocumentLoader* loader,
                         std::unique_ptr<WebDocumentSubresourceFilter> f) {
  if (loader && f) {
    loader->SetSubresourceFilter(f.release());
  }
}

void InstallUserFilter(DocumentLoader* loader,
                       std::unique_ptr<WebDocumentSubresourceFilter> f) {
  if (!loader) {
    return;
  }
  auto* ext = loader->AsArkWebDocumentLoaderExt();
  if (!ext) {
    return;
  }
  if (!f) {
    ext->SetUserSubresourceFilter(nullptr);
    return;
  }
  LocalDOMWindow* context =
      (loader->GetFrame() && loader->GetFrame()->DomWindow())
          ? loader->GetFrame()->DomWindow()
          : nullptr;
  if (!context) {
    return;
  }
  SubresourceFilter* sf =
      MakeGarbageCollected<SubresourceFilter>(context, std::move(f));
  ext->SetUserSubresourceFilter(sf);
}

void ClearAllFilters() {
  if (!g_fixture || !g_fixture->page_holder) {
    return;
  }
  DocumentLoader* loader = g_fixture->page_holder->GetFrame().Loader().GetDocumentLoader();
  if (!loader) {
    return;
  }
  if (auto* ext = loader->AsArkWebDocumentLoaderExt()) {
    ext->SetUserSubresourceFilter(nullptr);
  }
}

String FuzzSelector(FuzzedDataProvider* fdp) {
  static const char* kSelectors[] = {
      "#fuzz-target",
      ".fuzz-class",
      "div",
      "*",
      "html body div",
      "div.fuzz-class",
      "#fuzz-target.fuzz-class",
  };
  return String(
      kSelectors[fdp->ConsumeIntegralInRange<size_t>(
          0, std::size(kSelectors) - 1)]);
}

// 1. Rule has neither adblock flag → both `if (rule->IsForAdBlock())` and
//    `if (rule->IsForUserAdBlock())` short-circuit.
void FuzzNeitherFlag(FuzzedDataProvider* fdp) {
  Document& doc = g_fixture->page_holder->GetDocument();
  StyleRule* rule = BuildStyleRule(doc, /*for_adblock=*/false,
                                   /*for_user_adblock=*/false, FuzzSelector(fdp));
  if (!rule || !g_fixture->element) {
    return;
  }
  Vector<uint16_t> backing;
  RuleData rule_data = MakeRuleData(rule, backing);
  ElementResolveContext context(*g_fixture->element);
  MatchResult result;
  DidMatchCssRuleForInclude(rule_data, context, result);
}

// 2. AdBlock flag set, no system filter → enters branch but utils->DidMatchCssRule
//    is skipped because Loader()->GetSubresourceFilter() == nullptr.
void FuzzAdblockNoFilter(FuzzedDataProvider* fdp) {
  Document& doc = g_fixture->page_holder->GetDocument();
  StyleRule* rule =
      BuildStyleRule(doc, /*for_adblock=*/true,
                     /*for_user_adblock=*/false, FuzzSelector(fdp));
  if (!rule || !g_fixture->element) {
    return;
  }
  Vector<uint16_t> backing;
  RuleData rule_data = MakeRuleData(rule, backing);
  ElementResolveContext context(*g_fixture->element);
  MatchResult result;
  DidMatchCssRuleForInclude(rule_data, context, result);
}

// 3. AdBlock flag set with system filter installed → full chain, must invoke
//    utils->DidMatchCssRule and set MatchResult::DisplayNoneFromAdblock.
void FuzzAdblockWithFilter(FuzzedDataProvider* fdp) {
  Document& doc = g_fixture->page_holder->GetDocument();
  StyleRule* rule =
      BuildStyleRule(doc, /*for_adblock=*/true,
                     /*for_user_adblock=*/false, FuzzSelector(fdp));
  if (!rule || !g_fixture->element) {
    return;
  }
  DocumentLoader* loader = doc.Loader();
  if (!loader) {
    return;
  }
  InstallSystemFilter(loader, std::make_unique<StubWebDocumentSubresourceFilter>());

  Vector<uint16_t> backing;
  RuleData rule_data = MakeRuleData(rule, backing);
  ElementResolveContext context(*g_fixture->element);
  MatchResult result;
  DidMatchCssRuleForInclude(rule_data, context, result);
}

// 4. UserAdBlock flag, no user filter installed.
void FuzzUserAdblockNoFilter(FuzzedDataProvider* fdp) {
  Document& doc = g_fixture->page_holder->GetDocument();
  StyleRule* rule =
      BuildStyleRule(doc, /*for_adblock=*/false,
                     /*for_user_adblock=*/true, FuzzSelector(fdp));
  if (!rule || !g_fixture->element) {
    return;
  }
  ClearAllFilters();
  Vector<uint16_t> backing;
  RuleData rule_data = MakeRuleData(rule, backing);
  ElementResolveContext context(*g_fixture->element);
  MatchResult result;
  DidMatchCssRuleForInclude(rule_data, context, result);
}

// 5. UserAdBlock flag, user filter installed with DidFinishLoad==false →
//    enters the `!GetDidFinishLoad()` branch and reports the matched rule.
void FuzzUserAdblockNotFinished(FuzzedDataProvider* fdp) {
  Document& doc = g_fixture->page_holder->GetDocument();
  StyleRule* rule =
      BuildStyleRule(doc, /*for_adblock=*/false,
                     /*for_user_adblock=*/true, FuzzSelector(fdp));
  if (!rule || !g_fixture->element) {
    return;
  }
  DocumentLoader* loader = doc.Loader();
  if (!loader) {
    return;
  }
  auto stub = std::make_unique<StubWebDocumentSubresourceFilter>();
  stub->SetDidFinishLoad(false);
  InstallUserFilter(loader, std::move(stub));

  Vector<uint16_t> backing;
  RuleData rule_data = MakeRuleData(rule, backing);
  ElementResolveContext context(*g_fixture->element);
  MatchResult result;
  DidMatchCssRuleForInclude(rule_data, context, result);
}

// 6. UserAdBlock flag with DidFinishLoad==true → must still set the flag but
//    skip the call to utils->DidMatchCssRule (negation branch).
void FuzzUserAdblockAlreadyFinished(FuzzedDataProvider* fdp) {
  Document& doc = g_fixture->page_holder->GetDocument();
  StyleRule* rule =
      BuildStyleRule(doc, /*for_adblock=*/false,
                     /*for_user_adblock=*/true, FuzzSelector(fdp));
  if (!rule || !g_fixture->element) {
    return;
  }
  DocumentLoader* loader = doc.Loader();
  if (!loader) {
    return;
  }
  auto stub = std::make_unique<StubWebDocumentSubresourceFilter>();
  stub->SetDidFinishLoad(true);
  InstallUserFilter(loader, std::move(stub));

  Vector<uint16_t> backing;
  RuleData rule_data = MakeRuleData(rule, backing);
  ElementResolveContext context(*g_fixture->element);
  MatchResult result;
  DidMatchCssRuleForInclude(rule_data, context, result);
}

// 7. Both flags set, both filters installed → exercise full plumbing for both
//    branches in a single invocation.
void FuzzBothFlagsBothFilters(FuzzedDataProvider* fdp) {
  Document& doc = g_fixture->page_holder->GetDocument();
  StyleRule* rule =
      BuildStyleRule(doc, /*for_adblock=*/true,
                     /*for_user_adblock=*/true, FuzzSelector(fdp));
  if (!rule || !g_fixture->element) {
    return;
  }
  DocumentLoader* loader = doc.Loader();
  if (!loader) {
    return;
  }
  InstallSystemFilter(loader,
                      std::make_unique<StubWebDocumentSubresourceFilter>());
  auto user_stub = std::make_unique<StubWebDocumentSubresourceFilter>();
  user_stub->SetDidFinishLoad(fdp->ConsumeBool());
  InstallUserFilter(loader, std::move(user_stub));

  Vector<uint16_t> backing;
  RuleData rule_data = MakeRuleData(rule, backing);
  ElementResolveContext context(*g_fixture->element);
  MatchResult result;
  DidMatchCssRuleForInclude(rule_data, context, result);
}

// 8. Random combination across selector / flags / filter presence.
void FuzzFreeform(FuzzedDataProvider* fdp) {
  Document& doc = g_fixture->page_holder->GetDocument();
  bool for_adblock = fdp->ConsumeBool();
  bool for_user_adblock = fdp->ConsumeBool();
  bool install_system = fdp->ConsumeBool();
  bool install_user = fdp->ConsumeBool();
  bool did_finish = fdp->ConsumeBool();

  StyleRule* rule =
      BuildStyleRule(doc, for_adblock, for_user_adblock, FuzzSelector(fdp));
  if (!rule || !g_fixture->element) {
    return;
  }
  DocumentLoader* loader = doc.Loader();
  if (loader) {
    if (install_system) {
      InstallSystemFilter(
          loader, std::make_unique<StubWebDocumentSubresourceFilter>());
    }
    if (install_user) {
      auto user_stub = std::make_unique<StubWebDocumentSubresourceFilter>();
      user_stub->SetDidFinishLoad(did_finish);
      InstallUserFilter(loader, std::move(user_stub));
    } else {
      InstallUserFilter(loader, nullptr);
    }
  }

  Vector<uint16_t> backing;
  RuleData rule_data = MakeRuleData(rule, backing);
  ElementResolveContext context(*g_fixture->element);
  MatchResult result;

  // Pre-seed the MatchResult flags to ensure idempotency on the false arms.
  if (fdp->ConsumeBool()) {
    result.SetDisplayNoneFromAdblock(true);
  }
  if (fdp->ConsumeBool()) {
    result.SetDisplayNoneFromUserAdblock(true);
  }
  DidMatchCssRuleForInclude(rule_data, context, result);
}

#endif  // BUILDFLAG(ARKWEB_ADBLOCK)

}  // namespace
}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
#if BUILDFLAG(ARKWEB_ADBLOCK)
  static blink::Environment env;
  if (!blink::g_fixture || !blink::g_fixture->page_holder ||
      !blink::g_fixture->element) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 7);
  switch (selector) {
    case 0:
      blink::FuzzNeitherFlag(&fdp);
      break;
    case 1:
      blink::FuzzAdblockNoFilter(&fdp);
      break;
    case 2:
      blink::FuzzAdblockWithFilter(&fdp);
      break;
    case 3:
      blink::FuzzUserAdblockNoFilter(&fdp);
      break;
    case 4:
      blink::FuzzUserAdblockNotFinished(&fdp);
      break;
    case 5:
      blink::FuzzUserAdblockAlreadyFinished(&fdp);
      break;
    case 6:
      blink::FuzzBothFlagsBothFilters(&fdp);
      break;
    case 7:
    default:
      blink::FuzzFreeform(&fdp);
      break;
  }

  blink::ClearAllFilters();
#else
  (void)data;
  (void)size;
#endif
  return 0;
}