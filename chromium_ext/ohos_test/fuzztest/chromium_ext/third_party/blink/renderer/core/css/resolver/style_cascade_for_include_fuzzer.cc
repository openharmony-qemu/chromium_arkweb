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

#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/css/resolver/style_cascade_for_include.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

#if BUILDFLAG(IS_ARKWEB)

namespace blink {

namespace {
constexpr uint8_t MAX_STRING_LENGTH = 255;
}  // namespace

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

static void InitState(StyleResolverState& state, Document& document) {
  document.GetStyleEngine().UpdateViewportSize();
  const ComputedStyle* initial_style =
      document.GetStyleResolver().InitialStyleForElement();
  state.CreateNewClonedStyle(*initial_style);
  state.SetParentStyle(initial_style);
  state.SetOldStyle(state.GetElement().GetComputedStyle());
}

#if BUILDFLAG(ARKWEB_ADBLOCK)
void FuzzLookupAndApplyDeclarationExt(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }

  Document& document = g_page_holder->GetDocument();
  Element* body = document.body();
  if (!body) {
    return;
  }

  StyleResolverState state(document, *body);
  InitState(state, document);
  StyleCascade cascade(state);

  bool from_adblock = fdp->ConsumeBool();
  bool from_user_adblock = fdp->ConsumeBool();
  cascade.MutableMatchResult().SetDisplayNoneFromAdblock(from_adblock);
  cascade.MutableMatchResult().SetDisplayNoneFromUserAdblock(from_user_adblock);

  int32_t display_choice = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
  EDisplay display_value;
  switch (display_choice) {
    case 0:
      display_value = EDisplay::kNone;
      break;
    case 1:
      display_value = EDisplay::kBlock;
      break;
    default:
      display_value = EDisplay::kInline;
      break;
  }

  bool use_display_property = fdp->ConsumeBool();
  CSSPropertyID property_id =
      use_display_property ? CSSPropertyID::kDisplay : CSSPropertyID::kWidth;

  int32_t style_display_choice = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
  switch (style_display_choice) {
    case 0:
      state.StyleBuilder().SetDisplay(EDisplay::kNone);
      break;
    case 1:
      state.StyleBuilder().SetDisplay(EDisplay::kBlock);
      break;
    default:
      state.StyleBuilder().SetDisplay(EDisplay::kInline);
      break;
  }

  StyleCascadeUtil::LookupAndApplyDeclarationExt(
      &cascade, CSSProperty::Get(property_id), display_value);
}
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)

void FuzzGetEnvironmentVariableExt(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }

  Document& document = g_page_holder->GetDocument();
  Element* body = document.body();
  if (!body) {
    return;
  }

  StyleResolverState state(document, *body);
  InitState(state, document);
  StyleCascade cascade(state);

  std::string name_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  AtomicString var_name(name_str.c_str());

  size_t index_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  Vector<unsigned> indices;
  for (size_t i = 0; i < index_count; ++i) {
    indices.push_back(fdp->ConsumeIntegral<unsigned>());
  }

  bool is_ua_scope = fdp->ConsumeBool();

  StyleCascadeUtil::GetEnvironmentVariableExt(&cascade, var_name, indices,
                                               is_ua_scope);
}

}  // namespace blink

#endif  // BUILDFLAG(IS_ARKWEB)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

#if BUILDFLAG(IS_ARKWEB)
  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 1);

#if BUILDFLAG(ARKWEB_ADBLOCK)
  if (selector == 0) {
    blink::FuzzLookupAndApplyDeclarationExt(&fdp);
  }
#endif

  if (selector == 1) {
    blink::FuzzGetEnvironmentVariableExt(&fdp);
  }

#endif

  return 0;
}