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

#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/drag_actions.h"
#include "third_party/blink/renderer/core/page/drag_state.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#define private public
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/page/drag_controller_ext.h"
#undef private

#include "v8/include/cppgc/allocation.h"

namespace blink {

static std::unique_ptr<DummyPageHolder> g_page_holder;
static DragControllerExt* g_drag_controller_ext = nullptr;

static const char kTestHtml[] = R"HTML(
    <html>
    <body>
        <a href="https://link.gitcode.com/?target=http%3A%2F%2Fexample.com&from=https%3A%2F%2Fgitcode.com%2Fopenharmony-tpc%2Fchromium_src%2Fissues%2F6188&lang=zh&theme=white" id="link" style="color: blue;">Test Link</a>
        <div id="container">
            <span id="child1">Child 1</span>
            <span id="child2">Child 2</span>
        </div>
        <img id="image" src="test.png" />
    </body>
    </html>
)HTML";

static const char kGrayStyleHtml[] = R"HTML(
    <html>
    <body>
        <a href="https://link.gitcode.com/?target=http%3A%2F%2Fexample.com&from=https%3A%2F%2Fgitcode.com%2Fopenharmony-tpc%2Fchromium_src%2Fissues%2F6188&lang=zh&theme=white" id="graylink" style="color: gray;">Gray Link</a>
        <div id="container" style="color: gray;">
            <span id="child1" style="color: gray;">Child 1</span>
            <span id="child2">Child 2</span>
        </div>
    </body>
    </html>
)HTML";

static const char kGrayStyleMiddleHtml[] = R"HTML(
    <html>
    <body>
        <a href="https://link.gitcode.com/?target=http%3A%2F%2Fexample.com&from=https%3A%2F%2Fgitcode.com%2Fopenharmony-tpc%2Fchromium_src%2Fissues%2F6188&lang=zh&theme=white" id="link" style="font-size: 12px; color: gray; font-weight: bold;">Middle Gray</a>
        <div id="container" style="color: gray; display: flex;">
            <span id="child1" style="color: gray; padding: 5px;">Child 1</span>
        </div>
    </body>
    </html>
)HTML";

static const char kGrayStyleType1Html[] = R"HTML(
    <html>
    <body>
        <a href="https://link.gitcode.com/?target=http%3A%2F%2Fexample.com&from=https%3A%2F%2Fgitcode.com%2Fopenharmony-tpc%2Fchromium_src%2Fissues%2F6188&lang=zh&theme=white" id="link" style="font-size: 12px; color: gray;">Type1 Gray</a>
    </body>
    </html>
)HTML";

static const char kGrayStyleType2Html[] = R"HTML(
    <html>
    <body>
        <a href="https://link.gitcode.com/?target=http%3A%2F%2Fexample.com&from=https%3A%2F%2Fgitcode.com%2Fopenharmony-tpc%2Fchromium_src%2Fissues%2F6188&lang=zh&theme=white" id="link" style="font-size: 12px; color: gray">Type2 Gray</a>
    </body>
    </html>
)HTML";

static const char kImageHtml[] = R"HTML(
    <html>
    <body>
        <img id="image1" src="test1.png" />
        <img id="image2" src="test2.png" style="opacity: 0.8;" />
        <a id="anchor_image" href="#"><img id="nested_image" src="nested.png" /></a>
    </body>
    </html>
)HTML";

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>();
    if (g_page_holder) {
      Page* page = &g_page_holder->GetPage();
      if (page) {
        g_drag_controller_ext = &page->GetDragController();
      }
    }
    SetupHtml();
  }

 public:
  void SetupHtml() {
    if (!g_page_holder) {
      return;
    }
    Document& doc = g_page_holder->GetDocument();
    if (!doc.body()) {
      return;
    }
    doc.body()->SetInnerHTMLWithoutTrustedTypes(String::FromUTF8(kTestHtml));
    g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  }

  void SetupGrayStyleHtml() {
    if (!g_page_holder) {
      return;
    }
    Document& doc = g_page_holder->GetDocument();
    if (!doc.body()) {
      return;
    }
    doc.body()->SetInnerHTMLWithoutTrustedTypes(String::FromUTF8(kGrayStyleHtml));
    g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  }

  void SetupGrayStyleMiddleHtml() {
    if (!g_page_holder) {
      return;
    }
    Document& doc = g_page_holder->GetDocument();
    if (!doc.body()) {
      return;
    }
    doc.body()->SetInnerHTMLWithoutTrustedTypes(
        String::FromUTF8(kGrayStyleMiddleHtml));
    g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  }

  void SetupGrayStyleType1Html() {
    if (!g_page_holder) {
      return;
    }
    Document& doc = g_page_holder->GetDocument();
    if (!doc.body()) {
      return;
    }
    doc.body()->SetInnerHTMLWithoutTrustedTypes(
        String::FromUTF8(kGrayStyleType1Html));
    g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  }

  void SetupGrayStyleType2Html() {
    if (!g_page_holder) {
      return;
    }
    Document& doc = g_page_holder->GetDocument();
    if (!doc.body()) {
      return;
    }
    doc.body()->SetInnerHTMLWithoutTrustedTypes(
        String::FromUTF8(kGrayStyleType2Html));
    g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  }

  void SetupImageHtml() {
    if (!g_page_holder) {
      return;
    }
    Document& doc = g_page_holder->GetDocument();
    if (!doc.body()) {
      return;
    }
    doc.body()->SetInnerHTMLWithoutTrustedTypes(String::FromUTF8(kImageHtml));
    g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  }

  ~Environment() {
    g_drag_controller_ext = nullptr;
    g_page_holder.reset();
  }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

static LocalFrame* GetFrame() {
  if (!g_page_holder) {
    return nullptr;
  }
  return &g_page_holder->GetFrame();
}

static Environment& GetEnv() {
  static Environment env;
  return env;
}

void FuzzUpdateLinkStyleRecursive() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Node* node = frame->GetDocument()->getElementById(AtomicString("container"));
  if (node) {
    g_drag_controller_ext->UpdateLinkStyle(node);
  }
  g_drag_controller_ext->UpdateLinkStyle(node);
}

void FuzzUpdateLinkStyleWithChildren() {
  if (!g_drag_controller_ext) {
    return;
  }
  if (!g_page_holder) {
    return;
  }
  Node* body = g_page_holder->GetDocument().body();
  if (body) {
    g_drag_controller_ext->UpdateLinkStyle(body);
  }
}

void FuzzFindAndRemoveGrayStyleNormal() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Element* element = frame->GetDocument()->getElementById(AtomicString("link"));
  if (element) {
    g_drag_controller_ext->FindAndRemoveGrayStyle(element);
  }
}

void FuzzFindAndRemoveGrayStyleGray() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Element* element = frame->GetDocument()->getElementById(AtomicString("graylink"));
  if (element) {
    g_drag_controller_ext->FindAndRemoveGrayStyle(element);
  }
}

void FuzzRestoreLinkStyleNormal() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Node* node = frame->GetDocument()->getElementById(AtomicString("container"));
  if (node) {
    g_drag_controller_ext->RestoreLinkStyle(node);
  }
}

void FuzzRestoreLinkStyleDeep() {
  if (!g_drag_controller_ext) {
    return;
  }
  if (!g_page_holder) {
    return;
  }
  Node* body = g_page_holder->GetDocument().body();
  if (body) {
    g_drag_controller_ext->RestoreLinkStyle(body);
  }
}

void FuzzLinkStyleRoundTrip() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Node* node = frame->GetDocument()->getElementById(AtomicString("container"));
  if (node) {
    g_drag_controller_ext->UpdateLinkStyle(node);
    g_drag_controller_ext->RestoreLinkStyle(node);
  }
}

void FuzzLinkStyleMultipleRounds() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Node* node = frame->GetDocument()->getElementById(AtomicString("container"));
  if (node) {
    for (int i = 0; i < 3; i++) {
      g_drag_controller_ext->UpdateLinkStyle(node);
    }
    for (int i = 0; i < 3; i++) {
      g_drag_controller_ext->RestoreLinkStyle(node);
    }
  }
}

void FuzzIsHyperLinkDragging() {
  if (!g_drag_controller_ext) {
    return;
  }
  bool result = g_drag_controller_ext->IsHyperLinkDragging();
  (void)result;
}

void FuzzIsInTextDraging() {
  if (!g_drag_controller_ext) {
    return;
  }
  bool result = g_drag_controller_ext->IsInTextDraging();
  (void)result;
}

void FuzzIsInImageDraging() {
  if (!g_drag_controller_ext) {
    return;
  }
  bool result = g_drag_controller_ext->IsInImageDraging();
  (void)result;
}

void FuzzIsDragEnabled() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (frame) {
    Document* doc = frame->GetDocument();
    Element* element = doc->getElementById(AtomicString("link"));
    if (element) {
      g_drag_controller_ext->IsDragEnabled();
    }
  }
  bool result = g_drag_controller_ext->IsDragEnabled();
  (void)result;
}

void FuzzInvalidateSelectionForDrag() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (frame) {
    Document* doc = frame->GetDocument();
    g_drag_controller_ext->InvalidateSelectionForDrag(doc);
  }
  g_drag_controller_ext->InvalidateSelectionForDrag(nullptr);
}

void FuzzStartDragLinkEffects() {
  if (!g_drag_controller_ext) {
    return;
  }
  g_drag_controller_ext->StartDragLinkEffects();
}

void FuzzRestoreDragLinkEffects() {
  if (!g_drag_controller_ext) {
    return;
  }
  g_drag_controller_ext->RestoreDragLinkEffects();
}

void FuzzStartDragImageEffects() {
  if (!g_drag_controller_ext) {
    return;
  }
  g_drag_controller_ext->StartDragImageEffects();
}

void FuzzRestoreDragImageEffects() {
  if (!g_drag_controller_ext) {
    return;
  }
  g_drag_controller_ext->RestoreDragImageEffects();
}

void FuzzStartDragTextEffects() {
  if (!g_drag_controller_ext) {
    return;
  }
  g_drag_controller_ext->StartDragTextEffects();
}

void FuzzRestoreDragTextEffects() {
  if (!g_drag_controller_ext) {
    return;
  }
  g_drag_controller_ext->RestoreDragTextEffects();
}

void FuzzGetVisibleRectToUIInRootFrame() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  gfx::RectF result = g_drag_controller_ext->GetVisibleRectToUIInRootFrame(frame);
  (void)result;
  gfx::RectF result_null = g_drag_controller_ext->GetVisibleRectToUIInRootFrame(nullptr);
  (void)result_null;
}

void FuzzFindAndRemoveGrayStyleMiddle() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Element* element =
      frame->GetDocument()->getElementById(AtomicString("link"));
  if (element) {
    g_drag_controller_ext->FindAndRemoveGrayStyle(element);
  }
}

void FuzzFindAndRemoveGrayStyleType1() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Element* element =
      frame->GetDocument()->getElementById(AtomicString("link"));
  if (element) {
    g_drag_controller_ext->FindAndRemoveGrayStyle(element);
  }
}

void FuzzFindAndRemoveGrayStyleType2() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Element* element =
      frame->GetDocument()->getElementById(AtomicString("link"));
  if (element) {
    g_drag_controller_ext->FindAndRemoveGrayStyle(element);
  }
}

void FuzzFindAndRemoveGrayStyleNull() {
  if (!g_drag_controller_ext) {
    return;
  }
}

void FuzzDragLinkCheckSrcAndType() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Node* anchor = doc->getElementById(AtomicString("link"));
  if (!anchor) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;
  bool old_did = g_drag_controller_ext->did_initiate_drag_;

  DragState* state1 = MakeGarbageCollected<DragState>();
  state1->drag_type_ = kDragSourceActionLink;
  state1->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state1;
  g_drag_controller_ext->did_initiate_drag_ = true;
  bool r1 = g_drag_controller_ext->DragLinkCheckSrcAndType();
  (void)r1;

  g_drag_controller_ext->did_initiate_drag_ = false;
  bool r2 = g_drag_controller_ext->DragLinkCheckSrcAndType();
  (void)r2;

  DragState* state2 = MakeGarbageCollected<DragState>();
  state2->drag_type_ = kDragSourceActionImage;
  state2->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state2;
  g_drag_controller_ext->did_initiate_drag_ = true;
  bool r3 = g_drag_controller_ext->DragLinkCheckSrcAndType();
  (void)r3;

  DragState* state3 = MakeGarbageCollected<DragState>();
  state3->drag_type_ = kDragSourceActionLink;
  state3->drag_src_ = nullptr;
  g_drag_controller_ext->drag_state_ = state3;
  bool r4 = g_drag_controller_ext->DragLinkCheckSrcAndType();
  (void)r4;

  g_drag_controller_ext->drag_state_ = nullptr;
  bool r5 = g_drag_controller_ext->DragLinkCheckSrcAndType();
  (void)r5;

  g_drag_controller_ext->drag_state_ = old_state;
  g_drag_controller_ext->did_initiate_drag_ = old_did;
}

void FuzzIsHyperLinkDraggingWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Node* anchor = doc->getElementById(AtomicString("link"));
  if (!anchor) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;
  bool old_did = g_drag_controller_ext->did_initiate_drag_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionLink;
  state->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state;
  g_drag_controller_ext->did_initiate_drag_ = true;
  bool r1 = g_drag_controller_ext->IsHyperLinkDragging();
  (void)r1;

  g_drag_controller_ext->did_initiate_drag_ = false;
  bool r2 = g_drag_controller_ext->IsHyperLinkDragging();
  (void)r2;

  state->drag_type_ = kDragSourceActionImage;
  g_drag_controller_ext->did_initiate_drag_ = true;
  bool r3 = g_drag_controller_ext->IsHyperLinkDragging();
  (void)r3;

  state->drag_type_ = kDragSourceActionLink;
  state->drag_src_ = nullptr;
  bool r4 = g_drag_controller_ext->IsHyperLinkDragging();
  (void)r4;

  g_drag_controller_ext->drag_state_ = old_state;
  g_drag_controller_ext->did_initiate_drag_ = old_did;
}

void FuzzStartDragLinkEffectsWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Node* anchor = doc->getElementById(AtomicString("link"));
  if (!anchor) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;
  bool old_did = g_drag_controller_ext->did_initiate_drag_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionLink;
  state->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state;
  g_drag_controller_ext->did_initiate_drag_ = true;
  g_drag_controller_ext->StartDragLinkEffects();

  Element* container =
      doc->getElementById(AtomicString("container"));
  if (container) {
    state->drag_src_ = container;
    g_drag_controller_ext->StartDragLinkEffects();
  }

  g_drag_controller_ext->drag_state_ = nullptr;
  g_drag_controller_ext->did_initiate_drag_ = old_did;
  g_drag_controller_ext->StartDragLinkEffects();

  g_drag_controller_ext->drag_state_ = old_state;
}

void FuzzRestoreDragLinkEffectsWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Node* anchor = doc->getElementById(AtomicString("link"));
  if (!anchor) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;
  bool old_did = g_drag_controller_ext->did_initiate_drag_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionLink;
  state->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state;
  g_drag_controller_ext->did_initiate_drag_ = true;
  g_drag_controller_ext->RestoreDragLinkEffects();

  g_drag_controller_ext->drag_state_ = nullptr;
  g_drag_controller_ext->did_initiate_drag_ = old_did;
  g_drag_controller_ext->RestoreDragLinkEffects();

  g_drag_controller_ext->drag_state_ = old_state;
}

void FuzzStartDragTextEffectsWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Node* anchor = doc->getElementById(AtomicString("link"));
  if (!anchor) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionSelection;
  state->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state;
  g_drag_controller_ext->StartDragTextEffects();

  state->drag_type_ = kDragSourceActionImage;
  g_drag_controller_ext->StartDragTextEffects();

  state->drag_type_ = kDragSourceActionLink;
  g_drag_controller_ext->StartDragTextEffects();

  g_drag_controller_ext->drag_state_ = old_state;
}

void FuzzRestoreDragTextEffectsWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Node* anchor = doc->getElementById(AtomicString("link"));
  if (!anchor) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionSelection;
  state->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state;
  g_drag_controller_ext->RestoreDragTextEffects();

  state->drag_type_ = kDragSourceActionImage;
  g_drag_controller_ext->RestoreDragTextEffects();

  state->drag_type_ = kDragSourceActionLink;
  g_drag_controller_ext->RestoreDragTextEffects();

  g_drag_controller_ext->drag_state_ = old_state;
}

void FuzzStartDragImageEffectsWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionImage;
  g_drag_controller_ext->drag_state_ = state;
  g_drag_controller_ext->StartDragImageEffects();

  Element* image = doc->getElementById(AtomicString("image1"));
  if (image) {
    state->drag_src_ = image;
    g_drag_controller_ext->StartDragImageEffects();
  }

  Element* image_opaque =
      doc->getElementById(AtomicString("image2"));
  if (image_opaque) {
    state->drag_src_ = image_opaque;
    g_drag_controller_ext->StartDragImageEffects();
  }

  state->drag_src_ = nullptr;
  g_drag_controller_ext->StartDragImageEffects();

  g_drag_controller_ext->drag_state_ = old_state;
}

void FuzzRestoreDragImageEffectsWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;
  bool old_did_start = g_drag_controller_ext->did_start_drag_image_effects_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionImage;
  g_drag_controller_ext->drag_state_ = state;

  Element* image = doc->getElementById(AtomicString("image1"));
  if (image) {
    state->drag_src_ = image;
    g_drag_controller_ext->RestoreDragImageEffects();
    g_drag_controller_ext->did_start_drag_image_effects_ = true;
    g_drag_controller_ext->RestoreDragImageEffects();
  }

  state->drag_src_ = nullptr;
  g_drag_controller_ext->RestoreDragImageEffects();

  g_drag_controller_ext->drag_state_ = old_state;
  g_drag_controller_ext->did_start_drag_image_effects_ = old_did_start;
}

void FuzzIsInTextDragingWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Node* anchor = doc->getElementById(AtomicString("link"));
  if (!anchor) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;
  bool old_did = g_drag_controller_ext->did_initiate_drag_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionSelection;
  state->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state;
  g_drag_controller_ext->did_initiate_drag_ = true;
  bool r1 = g_drag_controller_ext->IsInTextDraging();
  (void)r1;

  g_drag_controller_ext->did_initiate_drag_ = false;
  bool r2 = g_drag_controller_ext->IsInTextDraging();
  (void)r2;

  state->drag_type_ = kDragSourceActionImage;
  g_drag_controller_ext->did_initiate_drag_ = true;
  bool r3 = g_drag_controller_ext->IsInTextDraging();
  (void)r3;

  state->drag_type_ = kDragSourceActionLink;
  bool r4 = g_drag_controller_ext->IsInTextDraging();
  (void)r4;

  g_drag_controller_ext->drag_state_ = old_state;
  g_drag_controller_ext->did_initiate_drag_ = old_did;
}

void FuzzIsInImageDragingWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Node* anchor = doc->getElementById(AtomicString("link"));
  if (!anchor) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;
  bool old_did = g_drag_controller_ext->did_initiate_drag_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionImage;
  state->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state;
  g_drag_controller_ext->did_initiate_drag_ = true;
  bool r1 = g_drag_controller_ext->IsInImageDraging();
  (void)r1;

  g_drag_controller_ext->did_initiate_drag_ = false;
  bool r2 = g_drag_controller_ext->IsInImageDraging();
  (void)r2;

  state->drag_type_ = kDragSourceActionSelection;
  g_drag_controller_ext->did_initiate_drag_ = true;
  bool r3 = g_drag_controller_ext->IsInImageDraging();
  (void)r3;

  state->drag_type_ = kDragSourceActionLink;
  bool r4 = g_drag_controller_ext->IsInImageDraging();
  (void)r4;

  g_drag_controller_ext->drag_state_ = old_state;
  g_drag_controller_ext->did_initiate_drag_ = old_did;
}

void FuzzIsDragEnabledWithState() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Node* anchor = doc->getElementById(AtomicString("link"));
  if (!anchor) {
    return;
  }

  DragState* old_state = g_drag_controller_ext->drag_state_;
  bool old_did = g_drag_controller_ext->did_initiate_drag_;

  DragState* state = MakeGarbageCollected<DragState>();
  state->drag_type_ = kDragSourceActionLink;
  state->drag_src_ = anchor;
  g_drag_controller_ext->drag_state_ = state;
  g_drag_controller_ext->did_initiate_drag_ = true;
  bool r1 = g_drag_controller_ext->IsDragEnabled();
  (void)r1;

  state->drag_src_ = nullptr;
  bool r2 = g_drag_controller_ext->IsDragEnabled();
  (void)r2;

  g_drag_controller_ext->drag_state_ = old_state;
  g_drag_controller_ext->did_initiate_drag_ = old_did;
}

void FuzzInvalidateSelectionForDragPaths() {
  if (!g_drag_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (doc) {
    g_drag_controller_ext->InvalidateSelectionForDrag(doc);
  }
  g_drag_controller_ext->InvalidateSelectionForDrag(nullptr);
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  blink::GetEnv();

  FuzzedDataProvider fdp(data, size);

  if (!blink::g_drag_controller_ext) {
    return 0;
  }

  int selector = fdp.ConsumeIntegralInRange<int>(0, 40);
  switch (selector) {
    case 0:
      blink::FuzzUpdateLinkStyleRecursive();
      break;
    case 1:
      blink::FuzzUpdateLinkStyleWithChildren();
      break;
    case 2:
      blink::FuzzFindAndRemoveGrayStyleNormal();
      break;
    case 3:
      blink::FuzzFindAndRemoveGrayStyleGray();
      break;
    case 4:
      blink::FuzzRestoreLinkStyleNormal();
      break;
    case 5:
      blink::FuzzRestoreLinkStyleDeep();
      break;
    case 6:
      blink::FuzzLinkStyleRoundTrip();
      break;
    case 7:
      blink::FuzzLinkStyleMultipleRounds();
      break;
    case 8:
      blink::FuzzIsHyperLinkDragging();
      break;
    case 9:
      blink::FuzzIsInTextDraging();
      break;
    case 10:
      blink::FuzzIsInImageDraging();
      break;
    case 11:
      blink::FuzzIsDragEnabled();
      break;
    case 12:
      blink::FuzzInvalidateSelectionForDrag();
      break;
    case 13:
      blink::FuzzStartDragLinkEffects();
      break;
    case 14:
      blink::FuzzRestoreDragLinkEffects();
      break;
    case 15:
      blink::FuzzStartDragImageEffects();
      break;
    case 16:
      blink::FuzzRestoreDragImageEffects();
      break;
    case 17:
      blink::FuzzStartDragTextEffects();
      break;
    case 18:
      blink::FuzzRestoreDragTextEffects();
      break;
    case 19:
      blink::FuzzGetVisibleRectToUIInRootFrame();
      break;
    case 20:
      blink::GetEnv().SetupGrayStyleHtml();
      blink::FuzzFindAndRemoveGrayStyleGray();
      blink::GetEnv().SetupHtml();
      break;
    case 21:
      blink::GetEnv().SetupGrayStyleMiddleHtml();
      blink::FuzzFindAndRemoveGrayStyleMiddle();
      blink::GetEnv().SetupHtml();
      break;
    case 22:
      blink::GetEnv().SetupGrayStyleType1Html();
      blink::FuzzFindAndRemoveGrayStyleType1();
      blink::GetEnv().SetupHtml();
      break;
    case 23:
      blink::GetEnv().SetupGrayStyleType2Html();
      blink::FuzzFindAndRemoveGrayStyleType2();
      blink::GetEnv().SetupHtml();
      break;
    case 24:
      blink::FuzzFindAndRemoveGrayStyleNull();
      break;
    case 25:
      blink::FuzzDragLinkCheckSrcAndType();
      break;
    case 26:
      blink::FuzzIsHyperLinkDraggingWithState();
      break;
    case 27:
      blink::FuzzStartDragLinkEffectsWithState();
      break;
    case 28:
      blink::FuzzRestoreDragLinkEffectsWithState();
      break;
    case 29:
      blink::FuzzStartDragTextEffectsWithState();
      break;
    case 30:
      blink::FuzzRestoreDragTextEffectsWithState();
      break;
    case 31:
      blink::GetEnv().SetupImageHtml();
      blink::FuzzStartDragImageEffectsWithState();
      blink::GetEnv().SetupHtml();
      break;
    case 32:
      blink::GetEnv().SetupImageHtml();
      blink::FuzzRestoreDragImageEffectsWithState();
      blink::GetEnv().SetupHtml();
      break;
    case 33:
      blink::FuzzIsInTextDragingWithState();
      break;
    case 34:
      blink::FuzzIsInImageDragingWithState();
      break;
    case 35:
      blink::FuzzIsDragEnabledWithState();
      break;
    case 36:
      blink::FuzzInvalidateSelectionForDragPaths();
      break;
    case 37:
      blink::GetEnv().SetupGrayStyleHtml();
      blink::FuzzUpdateLinkStyleRecursive();
      blink::FuzzRestoreLinkStyleNormal();
      blink::GetEnv().SetupHtml();
      break;
    case 38:
      blink::GetEnv().SetupGrayStyleMiddleHtml();
      blink::FuzzUpdateLinkStyleRecursive();
      blink::FuzzRestoreLinkStyleDeep();
      blink::GetEnv().SetupHtml();
      break;
    case 39:
      blink::GetEnv().SetupGrayStyleType1Html();
      blink::FuzzLinkStyleRoundTrip();
      blink::GetEnv().SetupHtml();
      break;
    case 40:
      blink::GetEnv().SetupImageHtml();
      blink::FuzzStartDragImageEffects();
      blink::FuzzRestoreDragImageEffects();
      blink::GetEnv().SetupHtml();
      break;
  }

  return 0;
}