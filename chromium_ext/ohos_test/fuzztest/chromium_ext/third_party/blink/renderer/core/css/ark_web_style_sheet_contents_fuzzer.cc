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
#include <limits>
#include <string>

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

void FuzzParseString(FuzzedDataProvider* fdp) {
  std::string css_input = fdp->ConsumeRemainingBytesAsString();
  size_t data_hash = std::hash<std::string>()(css_input);
  int is_strict_mode = (data_hash & std::numeric_limits<int>::max()) % 2;
  int is_secure_context_mode =
      (std::hash<size_t>()(data_hash) & std::numeric_limits<int>::max()) % 2;

  auto* context = MakeGarbageCollected<CSSParserContext>(
      is_strict_mode ? kHTMLStandardMode : kHTMLQuirksMode,
      is_secure_context_mode ? SecureContextMode::kSecureContext
                             : SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  style_sheet->ParseString(
      String::FromUTF8WithLatin1Fallback(css_input));
}

void FuzzSetStyleSheetType(FuzzedDataProvider* fdp) {
  int32_t type_val = fdp->ConsumeIntegralInRange<int32_t>(0, 4);

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  WebDocument::StyleSheetType type;
  switch (type_val) {
    case 0:
      type = WebDocument::StyleSheetType::kNormal;
      break;
    case 1:
      type = WebDocument::StyleSheetType::kAdBlock;
      break;
    case 2:
      type = WebDocument::StyleSheetType::kUserAdBlock;
      break;
    case 3:
      type = WebDocument::StyleSheetType::kManualAdBlock;
      break;
    default:
      type = WebDocument::StyleSheetType::kNormal;
      break;
  }
  style_sheet->SetStyleSheetType(type);
}

void FuzzParseThenSetType(FuzzedDataProvider* fdp) {
  size_t css_len = fdp->ConsumeIntegralInRange<size_t>(0, 512);
  std::string css_input = fdp->ConsumeBytesAsString(
      std::min(css_len, fdp->remaining_bytes()));
  int32_t type_val = fdp->ConsumeIntegralInRange<int32_t>(0, 3);

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kSecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  style_sheet->ParseString(
      String::FromUTF8WithLatin1Fallback(css_input));

  WebDocument::StyleSheetType type;
  switch (type_val) {
    case 0:
      type = WebDocument::StyleSheetType::kNormal;
      break;
    case 1:
      type = WebDocument::StyleSheetType::kAdBlock;
      break;
    case 2:
      type = WebDocument::StyleSheetType::kUserAdBlock;
      break;
    default:
      type = WebDocument::StyleSheetType::kManualAdBlock;
      break;
  }
  style_sheet->SetStyleSheetType(type);
}

void FuzzCopyStyleSheet(FuzzedDataProvider* fdp) {
  std::string css_input = fdp->ConsumeBytesAsString(
      std::min(static_cast<size_t>(256), fdp->remaining_bytes()));

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLQuirksMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  style_sheet->ParseString(
      String::FromUTF8WithLatin1Fallback(css_input));

  auto* copied = style_sheet->Copy();

  int32_t type_val = fdp->ConsumeIntegralInRange<int32_t>(0, 3);
  WebDocument::StyleSheetType type;
  switch (type_val) {
    case 0:
      type = WebDocument::StyleSheetType::kAdBlock;
      break;
    case 1:
      type = WebDocument::StyleSheetType::kUserAdBlock;
      break;
    default:
      type = WebDocument::StyleSheetType::kNormal;
      break;
  }
  copied->SetStyleSheetType(type);
}

void FuzzRepeatedParse(FuzzedDataProvider* fdp) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kSecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  int32_t iterations = fdp->ConsumeIntegralInRange<int32_t>(1, 5);
  for (int32_t i = 0; i < iterations && fdp->remaining_bytes() > 0; ++i) {
    size_t len = fdp->ConsumeIntegralInRange<size_t>(0, 128);
    std::string chunk = fdp->ConsumeBytesAsString(
        std::min(len, fdp->remaining_bytes()));
    style_sheet->ParseString(
        String::FromUTF8WithLatin1Fallback(chunk));
  }

  style_sheet->SetStyleSheetType(WebDocument::StyleSheetType::kAdBlock);
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static blink::BlinkFuzzerTestSupport test_support;
  blink::test::TaskEnvironment task_environment;

  FuzzedDataProvider fdp(data, size);
  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 4);

  switch (selector) {
    case 0:
      blink::FuzzParseString(&fdp);
      break;
    case 1:
      blink::FuzzSetStyleSheetType(&fdp);
      break;
    case 2:
      blink::FuzzParseThenSetType(&fdp);
      break;
    case 3:
      blink::FuzzCopyStyleSheet(&fdp);
      break;
    case 4:
      blink::FuzzRepeatedParse(&fdp);
      break;
    default:
      blink::FuzzParseString(&fdp);
      break;
  }

  return 0;
}
