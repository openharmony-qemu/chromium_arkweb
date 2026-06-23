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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/exported/web_element_utils.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class FuzzableWebElementUtils : public WebElementUtils {
 public:
  void SetNode(Node* node) { private_ = node; }
};

static std::unique_ptr<DummyPageHolder> g_page_holder;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }
  ~Environment() { g_page_holder.reset(); }
  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

static const char* kContentEditableValues[] = {
    "true", "false", "plaintext-only", "inherit", "",
    "TRUE", "FALSE", "True", "PlainText-Only",
    "1", "yes", "no", "on", "off", "auto",
};

static const QualifiedName* kHtmlTags[] = {
    &html_names::kDivTag,     &html_names::kSpanTag,
    &html_names::kPTag,       &html_names::kInputTag,
    &html_names::kATag,       &html_names::kH1Tag,
    &html_names::kSectionTag, &html_names::kNavTag,
    &html_names::kArticleTag, &html_names::kButtonTag,
};
constexpr size_t kHtmlTagCount = std::size(kHtmlTags);

void FuzzNullNode(FuzzableWebElementUtils* utils) {
  utils->IsContentEditable();
}

void FuzzHTMLElementWithAttribute(FuzzableWebElementUtils* utils,
                                  FuzzedDataProvider* fdp) {
  Document& doc = g_page_holder->GetDocument();

  size_t tag_idx =
      fdp->ConsumeIntegralInRange<size_t>(0, kHtmlTagCount - 1);
  auto* element = doc.CreateRawElement(*kHtmlTags[tag_idx]);
  if (!element) {
    return;
  }

  int attr_scenario = fdp->ConsumeIntegralInRange<int>(0, 2);
  if (attr_scenario == 1) {
    size_t value_idx = fdp->ConsumeIntegralInRange<size_t>(
        0, std::size(kContentEditableValues) - 1);
    element->setAttribute(html_names::kContenteditableAttr,
                          AtomicString(kContentEditableValues[value_idx]));
  } else if (attr_scenario == 2) {
    std::string random_value = fdp->ConsumeRandomLengthString(64);
    element->setAttribute(html_names::kContenteditableAttr,
                          AtomicString(random_value.c_str()));
  }

  utils->SetNode(element);
  utils->IsContentEditable();
}

void FuzzTextNode(FuzzableWebElementUtils* utils, FuzzedDataProvider* fdp) {
  Document& doc = g_page_holder->GetDocument();
  std::string text = fdp->ConsumeRandomLengthString(32);
  auto* text_node = doc.createTextNode(String(text.c_str()));
  utils->SetNode(text_node);
  utils->IsContentEditable();
}

void FuzzNestedEditability(FuzzableWebElementUtils* utils,
                           FuzzedDataProvider* fdp) {
  Document& doc = g_page_holder->GetDocument();

  auto* parent = doc.CreateRawElement(html_names::kDivTag);
  if (!parent) {
    return;
  }
  parent->setAttribute(html_names::kContenteditableAttr, AtomicString("true"));

  auto* child = doc.CreateRawElement(html_names::kSpanTag);
  if (!child) {
    return;
  }
  parent->AppendChild(child);

  if (fdp->ConsumeBool()) {
    size_t value_idx = fdp->ConsumeIntegralInRange<size_t>(
        0, std::size(kContentEditableValues) - 1);
    child->setAttribute(html_names::kContenteditableAttr,
                        AtomicString(kContentEditableValues[value_idx]));
  }

  utils->SetNode(child);
  utils->IsContentEditable();

  utils->SetNode(parent);
  utils->IsContentEditable();
}

void FuzzStateSequence(FuzzableWebElementUtils* utils, FuzzedDataProvider* fdp) {
  Document& doc = g_page_holder->GetDocument();
  int num_ops = fdp->ConsumeIntegralInRange<int>(2, 8);

  for (int i = 0; i < num_ops && fdp->remaining_bytes() > 2; ++i) {
    int op = fdp->ConsumeIntegralInRange<int>(0, 3);
    switch (op) {
      case 0: {
        utils->SetNode(nullptr);
        utils->IsContentEditable();
        break;
      }
      case 1: {
        size_t tag_idx =
            fdp->ConsumeIntegralInRange<size_t>(0, kHtmlTagCount - 1);
        auto* element = doc.CreateRawElement(*kHtmlTags[tag_idx]);
        if (!element) {
          break;
        }
        size_t value_idx = fdp->ConsumeIntegralInRange<size_t>(
            0, std::size(kContentEditableValues) - 1);
        element->setAttribute(html_names::kContenteditableAttr,
                              AtomicString(kContentEditableValues[value_idx]));
        utils->SetNode(element);
        utils->IsContentEditable();
        break;
      }
      case 2: {
        std::string text = fdp->ConsumeRandomLengthString(16);
        auto* text_node = doc.createTextNode(String(text.c_str()));
        utils->SetNode(text_node);
        utils->IsContentEditable();
        break;
      }
      case 3: {
        auto* element = doc.CreateRawElement(html_names::kDivTag);
        if (!element) {
          break;
        }
        std::string random_value = fdp->ConsumeRandomLengthString(32);
        element->setAttribute(html_names::kContenteditableAttr,
                              AtomicString(random_value.c_str()));
        utils->SetNode(element);
        utils->IsContentEditable();
        break;
      }
    }
  }
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::Environment env;

  if (!blink::g_page_holder) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  blink::FuzzableWebElementUtils utils;

  int32_t scenario = fdp.ConsumeIntegralInRange<int32_t>(0, 4);
  switch (scenario) {
    case 0:
      blink::FuzzNullNode(&utils);
      break;
    case 1:
      blink::FuzzHTMLElementWithAttribute(&utils, &fdp);
      break;
    case 2:
      blink::FuzzTextNode(&utils, &fdp);
      break;
    case 3:
      blink::FuzzNestedEditability(&utils, &fdp);
      break;
    case 4:
      blink::FuzzStateSequence(&utils, &fdp);
      break;
    default:
      blink::FuzzHTMLElementWithAttribute(&utils, &fdp);
      break;
  }

  return 0;
}
