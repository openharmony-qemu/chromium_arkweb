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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "arkweb/chromium_ext/third_party/blink/renderer/core/ohos_adblock/ohos_adblock_util.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

static std::unique_ptr<DummyPageHolder> g_page_holder;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }
  // No dtor: libFuzzer terminates the process.
  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

namespace {

// Static "interesting" HTML fragments that exercise distinct DOM-path
// branches: id, class, nested elements, sibling collisions, input[type],
// shadow-root host, body/head/html shortcuts, deep nesting.
constexpr const char* kCorpus[] = {
    "<div id='a'><span class='c1 c2'>x</span></div>",
    "<ul><li>1</li><li class='x'>2</li><li class='y'>3</li></ul>",
    "<form><input type='text' name='n'><input type='password'></form>",
    "<table><tr><td>a</td><td id='cell'>b</td></tr></table>",
    "<section><article><p class='para'>hello</p><p>world</p></article></section>",
    "<div><div><div><div><div>deep</div></div></div></div></div>",
    "<header><nav><a href='#'>x</a><a href='#' class='cur'>y</a></nav></header>",
    "<div class='x y z'><div class='x'></div><div class='y'></div></div>",
    "<main><h1>t</h1><h1 id='h'>u</h1><h2 class='c'>v</h2></main>",
    "<body><div></div></body>",  // body-as-parent edge case
    "",                          // empty document
};

std::string FuzzString(FuzzedDataProvider* fdp, size_t max_len) {
  return fdp->ConsumeRandomLengthString(max_len);
}

// Returns a random Element somewhere in the document tree, or nullptr.
Element* PickRandomElement(FuzzedDataProvider* fdp, Document& doc) {
  // Walk the tree collecting all elements (capped to keep perf bounded).
  std::vector<Element*> elements;
  for (Node* n = doc.documentElement(); n;) {
    if (auto* e = DynamicTo<Element>(n)) {
      elements.push_back(e);
      if (elements.size() > 64) {
        break;
      }
    }
    // Pre-order traversal.
    if (n->firstChild()) {
      n = n->firstChild();
    } else {
      while (n && !n->nextSibling()) {
        n = n->parentNode();
      }
      if (n) {
        n = n->nextSibling();
      }
    }
  }
  if (elements.empty()) {
    return nullptr;
  }
  return elements[fdp->ConsumeIntegralInRange<size_t>(0, elements.size() - 1)];
}

void LoadCorpus(FuzzedDataProvider* fdp) {
  HTMLElement* body = g_page_holder->GetDocument().body();
  if (!body) {
    return;
  }
  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, std::size(kCorpus) - 1);
  String html = String(kCorpus[idx]);
  // Optionally splice in fuzz-supplied bytes to widen the input space.
  if (fdp->ConsumeBool()) {
    std::string extra = FuzzString(fdp, 256);
    html = html + String(extra.c_str());
  }
  body->SetInnerHTMLWithoutTrustedTypes(html);
}

void FuzzEscape(FuzzedDataProvider* fdp) {
  std::string s = FuzzString(fdp, 256);
  (void)blink::Escape(s);
}

void FuzzIsValidTarget(FuzzedDataProvider* fdp) {
  LoadCorpus(fdp);
  Document& doc = g_page_holder->GetDocument();
  Element* el = PickRandomElement(fdp, doc);

  int top = fdp->ConsumeIntegralInRange<int>(-2000, 2000);
  int left = fdp->ConsumeIntegralInRange<int>(-2000, 2000);
  int width = fdp->ConsumeIntegralInRange<int>(-100, 5000);
  int height = fdp->ConsumeIntegralInRange<int>(-100, 5000);

  // Also exercise the nullptr branch.
  if (fdp->ConsumeBool()) {
    (void)blink::IsValidTarget(nullptr, top, left, width, height);
  } else {
    (void)blink::IsValidTarget(el, top, left, width, height);
  }
}

void FuzzPrefixedClassNames(FuzzedDataProvider* fdp) {
  LoadCorpus(fdp);
  Document& doc = g_page_holder->GetDocument();
  Element* el = PickRandomElement(fdp, doc);
  if (!el) {
    return;
  }
  bool pure = fdp->ConsumeBool();
  (void)blink::PrefixedElementClassNames(*el, pure);
}

void FuzzGetDomPathStep(FuzzedDataProvider* fdp) {
  LoadCorpus(fdp);
  Document& doc = g_page_holder->GetDocument();
  Element* el = PickRandomElement(fdp, doc);
  if (!el) {
    return;
  }
  bool optimized = fdp->ConsumeBool();
  bool isTarget = fdp->ConsumeBool();
  bool pure = fdp->ConsumeBool();
  (void)blink::GetDomPathStep(*el, optimized, isTarget, pure);
}

void FuzzGetDomPath(FuzzedDataProvider* fdp) {
  LoadCorpus(fdp);
  Document& doc = g_page_holder->GetDocument();
  Element* el = PickRandomElement(fdp, doc);
  if (!el) {
    return;
  }
  bool optimized = fdp->ConsumeBool();
  bool pure = fdp->ConsumeBool();
  (void)blink::GetDomPath(*el, optimized, pure);
}

// Combined: load corpus once, walk many elements, hit all 5 APIs to maximise
// branch interleaving per iteration.
void FuzzCombined(FuzzedDataProvider* fdp) {
  LoadCorpus(fdp);
  Document& doc = g_page_holder->GetDocument();

  int reps = fdp->ConsumeIntegralInRange<int>(2, 8);
  for (int i = 0; i < reps && fdp->remaining_bytes() > 8; ++i) {
    Element* el = PickRandomElement(fdp, doc);
    if (!el) {
      continue;
    }
    bool optimized = fdp->ConsumeBool();
    bool isTarget = fdp->ConsumeBool();
    bool pure = fdp->ConsumeBool();

    (void)blink::GetDomPath(*el, optimized, pure);
    (void)blink::GetDomPathStep(*el, optimized, isTarget, pure);
    (void)blink::PrefixedElementClassNames(*el, pure);
    (void)blink::IsValidTarget(
        el, fdp->ConsumeIntegralInRange<int>(-100, 100),
        fdp->ConsumeIntegralInRange<int>(-100, 100),
        fdp->ConsumeIntegralInRange<int>(0, 1000),
        fdp->ConsumeIntegralInRange<int>(0, 1000));
  }
  // Bonus: feed the tag-name string of a random element through Escape to
  // exercise CSS identifier serialization on real-world tokens.
  Element* tail = PickRandomElement(fdp, doc);
  if (tail) {
    (void)blink::Escape(tail->nodeName().Utf8());
  }
}

// Edge cases: non-element nodes (text/document) into APIs that early-return
// on getNodeType() != kElementNode.
void FuzzNonElementNode(FuzzedDataProvider* fdp) {
  LoadCorpus(fdp);
  Document& doc = g_page_holder->GetDocument();
  // Use the Document node itself (kDocumentNode).
  Node* doc_node = &doc;
  (void)blink::GetDomPathStep(*doc_node, fdp->ConsumeBool(),
                              fdp->ConsumeBool(), fdp->ConsumeBool());
  (void)blink::GetDomPath(*doc_node, fdp->ConsumeBool(), fdp->ConsumeBool());

  // First child of body, which may be a text node depending on corpus.
  if (HTMLElement* body = doc.body()) {
    if (Node* first = body->firstChild()) {
      (void)blink::GetDomPathStep(*first, fdp->ConsumeBool(),
                                  fdp->ConsumeBool(), fdp->ConsumeBool());
      (void)blink::GetDomPath(*first, fdp->ConsumeBool(),
                              fdp->ConsumeBool());
    }
  }
}

}  // namespace
}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::Environment env;
  if (!blink::g_page_holder) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  int32_t scenario = fdp.ConsumeIntegralInRange<int32_t>(0, 6);
  switch (scenario) {
    case 0:
      blink::FuzzEscape(&fdp);
      break;
    case 1:
      blink::FuzzIsValidTarget(&fdp);
      break;
    case 2:
      blink::FuzzPrefixedClassNames(&fdp);
      break;
    case 3:
      blink::FuzzGetDomPathStep(&fdp);
      break;
    case 4:
      blink::FuzzGetDomPath(&fdp);
      break;
    case 5:
      blink::FuzzCombined(&fdp);
      break;
    case 6:
      blink::FuzzNonElementNode(&fdp);
      break;
    default:
      blink::FuzzCombined(&fdp);
      break;
  }
  return 0;
}
