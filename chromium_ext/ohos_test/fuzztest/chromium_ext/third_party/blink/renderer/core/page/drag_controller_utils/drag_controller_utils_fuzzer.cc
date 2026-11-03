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
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_node.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/page/drag_controller_utils.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/drag_image.h"
#include "third_party/blink/renderer/core/page/drag_state.h"
#include "third_party/blink/renderer/core/page/drag_actions.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "url/gurl.h"

namespace blink {

static std::unique_ptr<DummyPageHolder> g_page_holder;

static const char kTestHtml[] = R"HTML(
    <html>
    <body>
        <a href="https://link.gitcode.com/?target=http%3A%2F%2Fexample.com&from=https%3A%2F%2Fgitcode.com%2Fopenharmony-tpc%2Fchromium_src%2Fissues%2F6188&lang=zh&theme=white" id="link">Test Link</a>
        <img id="image1" src="test1.png" />
        <img id="image2" src="test2.png" />
        <div id="container" style="width:200px;height:100px;background:blue;">Text Content</div>
    </body>
    </html>
)HTML";

static const char kSelectionHtml[] = R"HTML(
    <html>
    <body>
        <p id="para">This is some selectable text content for testing.</p>
    </body>
    </html>
)HTML";

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>();
    SetupHtml();
  }

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

  void SetupSelectionHtml() {
    if (!g_page_holder) {
      return;
    }
    Document& doc = g_page_holder->GetDocument();
    if (!doc.body()) {
      return;
    }
    doc.body()->SetInnerHTMLWithoutTrustedTypes(String::FromUTF8(kSelectionHtml));
    g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  }

  ~Environment() {
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

HitTestResult CreateHitTestResultWithNode(Node* node) {
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  gfx::Point point(0, 0);
  HitTestLocation location(point);
  HitTestResult result(request, location);
  if (node) {
    gfx::Point pt(0, 0);
    PhysicalOffset physicalPoint(pt);
    result.SetNodeAndPosition(node, physicalPoint);
  }
  return result;
}

void FuzzArkClampedImageScale(FuzzedDataProvider* fdp) {
  int image_w = fdp->ConsumeIntegral<int>();
  int image_h = fdp->ConsumeIntegral<int>();
  int elem_w = fdp->ConsumeIntegral<int>();
  int elem_h = fdp->ConsumeIntegral<int>();
  float scale_x = fdp->ConsumeFloatingPoint<float>();
  float scale_y = fdp->ConsumeFloatingPoint<float>();

  gfx::Vector2dF image_scale(scale_x, scale_y);
  gfx::Size image_size(image_w, image_h);
  gfx::Size element_size(elem_w, elem_h);

  ArkClampedImageScale(image_scale, image_size, element_size);
  (void)image_scale;
  (void)image_size;
}

void FuzzArkClampedImageScaleEdgeCases() {
  gfx::Vector2dF scale1(1.0f, 1.0f);
  gfx::Size size1(0, 0);
  ArkClampedImageScale(scale1, size1, gfx::Size(100, 100));

  gfx::Vector2dF scale2(0.0f, 0.0f);
  gfx::Size size2(100, 100);
  ArkClampedImageScale(scale2, size2, gfx::Size(0, 0));

  gfx::Vector2dF scale3(-1.0f, -1.0f);
  gfx::Size size3(100, 100);
  ArkClampedImageScale(scale3, size3, gfx::Size(100, 100));

  gfx::Vector2dF scale4(1000.0f, 1000.0f);
  gfx::Size size4(10000, 10000);
  ArkClampedImageScale(scale4, size4, gfx::Size(1, 1));

  gfx::Vector2dF scale5(1.0f, 1.0f);
  gfx::Size size5(INT_MAX, INT_MAX);
  ArkClampedImageScale(scale5, size5, gfx::Size(INT_MAX, INT_MAX));

  gfx::Vector2dF scale6(0.5f, 0.5f);
  gfx::Size size6(50, 50);
  ArkClampedImageScale(scale6, size6, gfx::Size(200, 200));

  gfx::Vector2dF scale7(2.0f, 2.0f);
  gfx::Size size7(200, 200);
  ArkClampedImageScale(scale7, size7, gfx::Size(50, 50));
}

void FuzzArkClampedImageScaleVariousInputs(FuzzedDataProvider* fdp) {
  for (int i = 0; i < 10; i++) {
    gfx::Vector2dF scale(
        fdp->ConsumeFloatingPointInRange<float>(0.0f, 5.0f),
        fdp->ConsumeFloatingPointInRange<float>(0.0f, 5.0f));
    gfx::Size image_size(
        fdp->ConsumeIntegralInRange<int>(0, 5000),
        fdp->ConsumeIntegralInRange<int>(0, 5000));
    gfx::Size element_size(
        fdp->ConsumeIntegralInRange<int>(1, 5000),
        fdp->ConsumeIntegralInRange<int>(1, 5000));

    ArkClampedImageScale(scale, image_size, element_size);
  }
}

void FuzzDragImageForLink(FuzzedDataProvider* fdp) {
  std::string url_str = fdp->ConsumeRandomLengthString();
  std::string link_text = fdp->ConsumeRandomLengthString();
  float device_scale = fdp->ConsumeFloatingPointInRange<float>(0.1f, 10.0f);

  GURL gurl(url_str);
  KURL kurl(gurl);
  String text = String::FromUTF8(link_text.c_str());

  std::unique_ptr<DragImage> result = DragImageForLink(kurl, text, device_scale, nullptr);
  (void)result;
}

void FuzzDragImageForLinkWithDocument(FuzzedDataProvider* fdp) {
  GetEnv().SetupHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }

  std::string url_str = fdp->ConsumeRandomLengthString();
  std::string link_text = fdp->ConsumeRandomLengthString();
  float device_scale = fdp->ConsumeFloatingPointInRange<float>(0.1f, 10.0f);

  GURL gurl(url_str);
  KURL kurl(gurl);
  String text = String::FromUTF8(link_text.c_str());

  std::unique_ptr<DragImage> result = DragImageForLink(kurl, text, device_scale, doc);
  (void)result;
}

void FuzzDragImageForLinkEdgeCases() {
  std::unique_ptr<DragImage> result1 = DragImageForLink(KURL(), String(), 1.0f, nullptr);
  (void)result1;

  std::unique_ptr<DragImage> result2 = DragImageForLink(
      KURL(GURL("http://example.com")), String::FromUTF8("Test Link"), 1.0f, nullptr);
  (void)result2;

  std::unique_ptr<DragImage> result3 = DragImageForLink(
      KURL(GURL("http://example.com")), String::FromUTF8(""), 0.0f, nullptr);
  (void)result3;

  std::unique_ptr<DragImage> result4 = DragImageForLink(
      KURL(GURL("http://example.com")), String::FromUTF8("Test Link"), -1.0f, nullptr);
  (void)result4;

  std::unique_ptr<DragImage> result5 = DragImageForLink(
      KURL(GURL("")), String::FromUTF8("Empty URL"), 1.0f, nullptr);
  (void)result5;

  std::unique_ptr<DragImage> result6 = DragImageForLink(
      KURL(GURL("javascript:alert(1)")), String::FromUTF8("JS URL"), 1.0f, nullptr);
  (void)result6;
}

void FuzzDragImageForLinkVariousUrls() {
  const char* urls[] = {
      "http://example.com",
      "https://example.com/path?query=value",
      "file:///path/to/file",
      "data:text/html,<h1>Test</h1>",
      "about:blank",
      "",
  };

  for (const char* url : urls) {
    GURL gurl(url);
    KURL kurl(gurl);
    String text = String::FromUTF8("Link Text");
    float scale = 1.0f;
    std::unique_ptr<DragImage> result = DragImageForLink(kurl, text, scale, nullptr);
    (void)result;
  }
}

void FuzzGetImageRectFromImageNodeNullFrame() {
  HitTestResult result;
  gfx::Rect rect = GetImageRectFromImageNode(nullptr, result);
  (void)rect;
}

void FuzzGetImageRectFromImageNodeWithFrame() {
  GetEnv().SetupHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* element = doc->getElementById(AtomicString("image1"));
  if (!element) {
    return;
  }
  HitTestResult result = CreateHitTestResultWithNode(element);
  gfx::Rect rect = GetImageRectFromImageNode(frame, result);
  (void)rect;
}

void FuzzGetImageRectFromImageNodeWithEmptyResult() {
  GetEnv().SetupHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  HitTestResult result;
  gfx::Rect rect = GetImageRectFromImageNode(frame, result);
  (void)rect;
}

void FuzzGetImageRectFromImageNodeVariousElements() {
  GetEnv().SetupHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }

  Element* elements[] = {
      doc->getElementById(AtomicString("image1")),
      doc->getElementById(AtomicString("image2")),
      doc->getElementById(AtomicString("link")),
      doc->getElementById(AtomicString("container")),
  };

  HitTestResult result;
  for (Element* elem : elements) {
    if (elem) {
      result = CreateHitTestResultWithNode(elem);
      gfx::Rect rect = GetImageRectFromImageNode(frame, result);
      (void)rect;
    }
  }
}

void FuzzGetImageRectFromImageNodeVariousPositions() {
  GetEnv().SetupHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* element = doc->getElementById(AtomicString("image1"));
  if (!element) {
    return;
  }

  HitTestResult result = CreateHitTestResultWithNode(element);

  gfx::Point positions[] = {
      gfx::Point(0, 0),
      gfx::Point(10, 10),
      gfx::Point(50, 50),
      gfx::Point(100, 100),
      gfx::Point(-10, -10),
  };

  for (const auto& pos : positions) {
    result.SetPointInInnerNodeFrame(PhysicalOffset(pos));
    gfx::Rect rect = GetImageRectFromImageNode(frame, result);
    (void)rect;
  }
}

void FuzzCreateImgAndRectForSelectionEmptyVisibleRect() {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }

  std::unique_ptr<DragImage> drag_image;
  gfx::Rect drag_obj_rect;
  gfx::RectF emptyRect(0, 0, 0, 0);

  CreateImgAndRectForSelection(drag_image, drag_obj_rect, frame, emptyRect);
  (void)drag_image;
  (void)drag_obj_rect;
}

void FuzzCreateImgAndRectForSelectionWithVisibleRect(FuzzedDataProvider* fdp) {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }

  int x = fdp->ConsumeIntegral<int>();
  int y = fdp->ConsumeIntegral<int>();
  int w = fdp->ConsumeIntegralInRange<int>(1, 800);
  int h = fdp->ConsumeIntegralInRange<int>(1, 600);

  std::unique_ptr<DragImage> drag_image;
  gfx::Rect drag_obj_rect;
  gfx::RectF visibleRect(x, y, w, h);

  CreateImgAndRectForSelection(drag_image, drag_obj_rect, frame, visibleRect);
  (void)drag_image;
  (void)drag_obj_rect;
}

void FuzzCreateImgAndRectForSelectionLargeRect() {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }

  std::unique_ptr<DragImage> drag_image;
  gfx::Rect drag_obj_rect;
  gfx::RectF largeRect(0, 0, 2000, 2000);

  CreateImgAndRectForSelection(drag_image, drag_obj_rect, frame, largeRect);
  (void)drag_image;
  (void)drag_obj_rect;
}

void FuzzCreateImgAndRectForSelectionNegativeRect() {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }

  std::unique_ptr<DragImage> drag_image;
  gfx::Rect drag_obj_rect;
  gfx::RectF negativeRect(-100, -100, 200, 200);

  CreateImgAndRectForSelection(drag_image, drag_obj_rect, frame, negativeRect);
  (void)drag_image;
  (void)drag_obj_rect;
}

void FuzzCreateImgAndRectForSelectionVariousSizes(FuzzedDataProvider* fdp) {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }

  int sizes[] = {1, 10, 100, 500, 1000, 2000};
  for (int size : sizes) {
    std::unique_ptr<DragImage> drag_image;
    gfx::Rect drag_obj_rect;
    gfx::RectF rect(0, 0, size, size);
    CreateImgAndRectForSelection(drag_image, drag_obj_rect, frame, rect);
  }
}

void FuzzDragRectForSelectionDragEmptyRect() {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  gfx::RectF emptyRect(0, 0, 0, 0);
  gfx::Rect rect = DragRectForSelectionDrag(*frame, emptyRect);
  (void)rect;
}

void FuzzDragRectForSelectionDragWithRect(FuzzedDataProvider* fdp) {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  int x = fdp->ConsumeIntegral<int>();
  int y = fdp->ConsumeIntegral<int>();
  int w = fdp->ConsumeIntegralInRange<int>(1, 800);
  int h = fdp->ConsumeIntegralInRange<int>(1, 600);
  gfx::RectF visibleRect(x, y, w, h);
  gfx::Rect rect = DragRectForSelectionDrag(*frame, visibleRect);
  (void)rect;
}

void FuzzDragRectForSelectionDragLargeRect() {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  gfx::RectF largeRect(0, 0, 2000, 2000);
  gfx::Rect rect = DragRectForSelectionDrag(*frame, largeRect);
  (void)rect;
}

void FuzzDragRectForSelectionDragNegativeRect() {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  gfx::RectF negativeRect(-100, -100, 200, 200);
  gfx::Rect rect = DragRectForSelectionDrag(*frame, negativeRect);
  (void)rect;
}

void FuzzDragRectForSelectionDragVariousSizes() {
  GetEnv().SetupSelectionHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  int sizes[] = {1, 10, 100, 500, 1000, 2000};
  for (int size : sizes) {
    gfx::RectF rect(0, 0, size, size);
    gfx::Rect result = DragRectForSelectionDrag(*frame, rect);
    (void)result;
  }
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  blink::GetEnv();

  FuzzedDataProvider fdp(data, size);

  int selector = fdp.ConsumeIntegralInRange<int>(0, 19);
  switch (selector) {
    case 0:
      blink::FuzzArkClampedImageScale(&fdp);
      break;
    case 1:
      blink::FuzzArkClampedImageScaleEdgeCases();
      break;
    case 2:
      blink::FuzzArkClampedImageScaleVariousInputs(&fdp);
      break;
    case 3:
      blink::FuzzDragImageForLink(&fdp);
      break;
    case 4:
      blink::FuzzDragImageForLinkWithDocument(&fdp);
      break;
    case 5:
      blink::FuzzDragImageForLinkEdgeCases();
      break;
    case 6:
      blink::FuzzDragImageForLinkVariousUrls();
      break;
    case 7:
      blink::FuzzGetImageRectFromImageNodeNullFrame();
      break;
    case 8:
      blink::FuzzGetImageRectFromImageNodeWithFrame();
      break;
    case 9:
      blink::FuzzGetImageRectFromImageNodeWithEmptyResult();
      break;
    case 10:
      blink::FuzzGetImageRectFromImageNodeVariousElements();
      break;
    case 11:
      blink::FuzzGetImageRectFromImageNodeVariousPositions();
      break;
    case 12:
      blink::FuzzCreateImgAndRectForSelectionEmptyVisibleRect();
      break;
    case 13:
      blink::FuzzCreateImgAndRectForSelectionWithVisibleRect(&fdp);
      break;
    case 14:
      blink::FuzzCreateImgAndRectForSelectionLargeRect();
      break;
    case 15:
      blink::FuzzCreateImgAndRectForSelectionNegativeRect();
      break;
    case 16:
      blink::FuzzCreateImgAndRectForSelectionVariousSizes(&fdp);
      break;
    case 17:
      blink::FuzzDragRectForSelectionDragEmptyRect();
      break;
    case 18:
      blink::FuzzDragRectForSelectionDragWithRect(&fdp);
      break;
    case 19:
      blink::FuzzDragRectForSelectionDragLargeRect();
      break;
  }

  return 0;
}