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
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area_utils.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

static std::unique_ptr<DummyPageHolder> g_page_holder = nullptr;
static PaintLayerScrollableAreaUtils* g_scroll_utils = nullptr;

static const char kScrollableHtml[] = R"HTML(
    <div id='scroll' style='width: 100px; height: 100px; overflow: scroll;
        will-change: transform'>
      <div id='content' style='position: relative; background: blue;
          width: 2000px; height: 2000px'></div>
    </div>
)HTML";

bool SetupScrollableArea() {
  if (!g_page_holder) {
    return false;
  }
  
  Document& doc = g_page_holder->GetDocument();
  if (!doc.body()) {
    return false;
  }
  
  doc.body()->SetInnerHTMLWithoutTrustedTypes(String::FromUTF8(kScrollableHtml));
  g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  
  Element* scroll_element = doc.getElementById(AtomicString("scroll"));
  if (!scroll_element) {
    return false;
  }
  
  LayoutObject* layout_obj = scroll_element->GetLayoutObject();
  if (!layout_obj || !layout_obj->IsBox()) {
    return false;
  }
  
  LayoutBox* layout_box = To<LayoutBox>(layout_obj);
  PaintLayer* paint_layer = layout_box->Layer();
  if (!paint_layer) {
    return false;
  }
  
  PaintLayerScrollableArea* scrollable_area = paint_layer->GetScrollableArea();
  if (!scrollable_area) {
    return false;
  }
  
  g_scroll_utils = MakeGarbageCollected<PaintLayerScrollableAreaUtils>(scrollable_area);
  return true;
}

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>();
    SetupScrollableArea();
  }

  ~Environment() {
    g_scroll_utils = nullptr;
    g_page_holder.reset();
  }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

void FuzzScaleSize(FuzzedDataProvider* fdp) {
  if (!g_scroll_utils) {
    return;
  }
  int width = fdp->ConsumeIntegralInRange<int>(1, 10000);
  int height = fdp->ConsumeIntegralInRange<int>(1, 10000);
  gfx::Size size(width, height);
  g_scroll_utils->ScaleSize(size);
  (void)size;
  
  gfx::Size size2(100, 200);
  g_scroll_utils->ScaleSize(size2);
  (void)size2;
  
  gfx::Size size3(1, 1);
  g_scroll_utils->ScaleSize(size3);
  (void)size3;
}

void FuzzScaleWidth() {
  if (!g_scroll_utils) {
    return;
  }
  LayoutUnit result = g_scroll_utils->ScaleWidth();
  (void)result;
  
  for (int i = 0; i < 5; i++) {
    LayoutUnit result2 = g_scroll_utils->ScaleWidth();
    (void)result2;
  }
}

void FuzzScaleHeight() {
  if (!g_scroll_utils) {
    return;
  }
  LayoutUnit result = g_scroll_utils->ScaleHeight();
  (void)result;
  
  for (int i = 0; i < 5; i++) {
    LayoutUnit result2 = g_scroll_utils->ScaleHeight();
    (void)result2;
  }
}

void FuzzScaleAll() {
  if (!g_scroll_utils) {
    return;
  }
  gfx::Size size(800, 600);
  g_scroll_utils->ScaleSize(size);
  (void)size;
  
  LayoutUnit width = g_scroll_utils->ScaleWidth();
  (void)width;
  
  LayoutUnit height = g_scroll_utils->ScaleHeight();
  (void)height;
  
  gfx::Size size2(1920, 1080);
  g_scroll_utils->ScaleSize(size2);
  (void)size2;
  
  LayoutUnit width2 = g_scroll_utils->ScaleWidth();
  (void)width2;
  
  LayoutUnit height2 = g_scroll_utils->ScaleHeight();
  (void)height2;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  if (!blink::g_scroll_utils) {
    return 0;
  }

  int selector = fdp.ConsumeIntegralInRange<int>(0, 3);
  switch (selector) {
    case 0:
      blink::FuzzScaleSize(&fdp);
      break;
    case 1:
      blink::FuzzScaleWidth();
      break;
    case 2:
      blink::FuzzScaleHeight();
      break;
    case 3:
      blink::FuzzScaleAll();
      break;
  }

  return 0;
}