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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/css/resolver/font_builder_utils.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/css/resolver/font_builder.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER) || BUILDFLAG(ARKWEB_CSS_FONT)

namespace blink {

namespace {
constexpr float kMinWeight = 1.0f;
constexpr float kMaxWeight = 1000.0f;
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

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
void FuzzUpdateFixedFontSize(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }

  FontBuilder font_builder(&g_page_holder->GetDocument());
  FontBuilderUtils font_builder_utils(&font_builder);

  FontDescription description;
  float weight = fdp->ConsumeFloatingPointInRange<float>(kMinWeight, kMaxWeight);
  description.SetWeight(FontSelectionValue(weight));

  font_builder_utils.UpdateFixedFontSize(description);
}
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)

#if BUILDFLAG(ARKWEB_CSS_FONT)
void FuzzUpdateWeightScale(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }

  FontBuilder font_builder(&g_page_holder->GetDocument());
  FontBuilderUtils font_builder_utils(&font_builder);

  FontDescription description;
  float weight = fdp->ConsumeFloatingPointInRange<float>(kMinWeight, kMaxWeight);
  description.SetWeight(FontSelectionValue(weight));

  font_builder_utils.UpdateWeightScale(description);
}

void FuzzUpdateFontDescription(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }

  FontBuilder font_builder(&g_page_holder->GetDocument());
  FontBuilderUtils font_builder_utils(&font_builder);

  FontDescription description;
  float weight = fdp->ConsumeFloatingPointInRange<float>(kMinWeight, kMaxWeight);
  description.SetWeight(FontSelectionValue(weight));

  bool modified = false;
  font_builder_utils.UpdateFontDescription(description, modified);
}
#endif  // BUILDFLAG(ARKWEB_CSS_FONT)

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER) || BUILDFLAG(ARKWEB_CSS_FONT)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER) || BUILDFLAG(ARKWEB_CSS_FONT)
  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 2);

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  if (selector == 0) {
    blink::FuzzUpdateFixedFontSize(&fdp);
  }
#endif

#if BUILDFLAG(ARKWEB_CSS_FONT)
  if (selector == 1) {
    blink::FuzzUpdateWeightScale(&fdp);
  }
  if (selector == 2) {
    blink::FuzzUpdateFontDescription(&fdp);
  }
#endif

#endif

  return 0;
}