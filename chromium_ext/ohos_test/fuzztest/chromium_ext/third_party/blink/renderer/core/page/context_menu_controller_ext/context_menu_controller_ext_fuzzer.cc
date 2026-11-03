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
#include "arkweb/chromium_ext/third_party/blink/renderer/core/page/context_menu_controller_ext.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "ui/gfx/geometry/point.h"

namespace blink {

static std::unique_ptr<DummyPageHolder> g_page_holder;
static ContextMenuControllerExt* g_context_menu_controller_ext = nullptr;

static const char kTestHtml[] = R"HTML(
    <html>
    <body>
        <a href="https://link.gitcode.com/?target=http%3A%2F%2Fexample.com&from=https%3A%2F%2Fgitcode.com%2Fopenharmony-tpc%2Fchromium_src%2Fissues%2F6188&lang=zh&theme=white" id="link">Test Link</a>
        <div id="container">
            <img id="image1" src="test1.png" />
            <span id="child1">Child 1</span>
            <span id="child2">Child 2</span>
        </div>
        <img id="image2" src="test2.png" />
        <img id="image3" src="test3.png" />
    </body>
    </html>
)HTML";

static const char kImageHtml[] = R"HTML(
    <html>
    <body>
        <div id="container" style="background-image: url('bg.png')">
            <img id="image1" src="test1.png" />
            <a href="https://link.gitcode.com/?target=http%3A%2F%2Fexample.com&from=https%3A%2F%2Fgitcode.com%2Fopenharmony-tpc%2Fchromium_src%2Fissues%2F6188&lang=zh&theme=white" id="link">
                <img id="nested" src="nested.png" />
            </a>
        </div>
    </body>
    </html>
)HTML";

static const char kMultipleImagesHtml[] = R"HTML(
    <html>
    <body>
        <div id="container">
            <img id="img1" src="image1.png" />
            <img id="img2" src="image2.png" />
            <img id="img3" src="image3.png" />
        </div>
        <img id="img4" src="image4.png" />
    </body>
    </html>
)HTML";

static const char kLinkImageHtml[] = R"HTML(
    <html>
    <body>
        <a href="https://link.gitcode.com/?target=http%3A%2F%2Fexample.com&from=https%3A%2F%2Fgitcode.com%2Fopenharmony-tpc%2Fchromium_src%2Fissues%2F6188&lang=zh&theme=white" id="alink">
            <img id="linkedimg" src="linked.png" />
        </a>
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
        ContextMenuController& controller = page->GetContextMenuController();
        g_context_menu_controller_ext = controller.AsContextMenuControllerExt();
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

  void SetupMultipleImagesHtml() {
    if (!g_page_holder) {
      return;
    }
    Document& doc = g_page_holder->GetDocument();
    if (!doc.body()) {
      return;
    }
    doc.body()->SetInnerHTMLWithoutTrustedTypes(String::FromUTF8(kMultipleImagesHtml));
    g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  }

  void SetupLinkImageHtml() {
    if (!g_page_holder) {
      return;
    }
    Document& doc = g_page_holder->GetDocument();
    if (!doc.body()) {
      return;
    }
    doc.body()->SetInnerHTMLWithoutTrustedTypes(String::FromUTF8(kLinkImageHtml));
    g_page_holder->GetFrameView().UpdateAllLifecyclePhasesForTest();
  }

  ~Environment() {
    g_context_menu_controller_ext = nullptr;
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

void FuzzGetAbsoluteSrcUrlNullElement() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  blink::WebElement null_element;
  GURL result = g_context_menu_controller_ext->GetAbsoluteSrcUrl(null_element);
  (void)result;
}

void FuzzGetAbsoluteSrcUrlWithElement() {
  if (!g_context_menu_controller_ext) {
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
  Element* element = doc->getElementById(AtomicString("image1"));
  if (element) {
    blink::WebElement web_element(element);
    GURL result = g_context_menu_controller_ext->GetAbsoluteSrcUrl(web_element);
    (void)result;
  }
}

void FuzzGetAbsoluteSrcUrlMultipleImages() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupMultipleImagesHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  const char* ids[] = {"img1", "img2", "img3", "img4"};
  for (auto id : ids) {
    Element* element = doc->getElementById(AtomicString(id));
    if (element) {
      blink::WebElement web_element(element);
      GURL result = g_context_menu_controller_ext->GetAbsoluteSrcUrl(web_element);
      (void)result;
    }
  }
}

void FuzzGetAbsoluteUrlNullNode() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  blink::WebNode null_node;
  std::u16string empty_fragment;
  GURL result = g_context_menu_controller_ext->GetAbsoluteUrl(null_node, empty_fragment);
  (void)result;
}

void FuzzGetAbsoluteUrlEmptyFragment() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  blink::WebNode null_node;
  GURL result = g_context_menu_controller_ext->GetAbsoluteUrl(null_node, std::u16string());
  (void)result;
}

void FuzzGetAbsoluteUrlRandomFragment(FuzzedDataProvider* fdp) {
  if (!g_context_menu_controller_ext) {
    return;
  }
  blink::WebNode null_node;
  std::string fragment = fdp->ConsumeRandomLengthString();
  std::u16string url_fragment(fragment.begin(), fragment.end());
  GURL result = g_context_menu_controller_ext->GetAbsoluteUrl(null_node, url_fragment);
  (void)result;
}

void FuzzGetImgChildRandomPoint(FuzzedDataProvider* fdp) {
  if (!g_context_menu_controller_ext) {
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
  Element* container = doc->getElementById(AtomicString("container"));
  if (container) {
    blink::WebNode web_node(container);
    int x = fdp->ConsumeIntegral<int>();
    int y = fdp->ConsumeIntegral<int>();
    gfx::Point point(x, y);
    blink::WebElement result = g_context_menu_controller_ext->GetImgChild(web_node, point);
    (void)result;
  }
}

void FuzzGetImgChildMultipleImages(FuzzedDataProvider* fdp) {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupMultipleImagesHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* container = doc->getElementById(AtomicString("container"));
  if (container) {
    blink::WebNode web_node(container);
    int x = fdp->ConsumeIntegral<int>();
    int y = fdp->ConsumeIntegral<int>();
    gfx::Point point(x, y);
    blink::WebElement result = g_context_menu_controller_ext->GetImgChild(web_node, point);
    (void)result;
  }
}

void FuzzGetChildImageUrlFromElementNullElement() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  blink::WebElement null_element;
  gfx::Point point(0, 0);
  GURL result = g_context_menu_controller_ext->GetChildImageUrlFromElement(null_element, point);
  (void)result;
}

void FuzzGetChildImageUrlFromElementWithLink(FuzzedDataProvider* fdp) {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupImageHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* link = doc->getElementById(AtomicString("link"));
  if (link) {
    blink::WebElement web_element(link);
    int x = fdp->ConsumeIntegral<int>();
    int y = fdp->ConsumeIntegral<int>();
    gfx::Point point(x, y);
    GURL result = g_context_menu_controller_ext->GetChildImageUrlFromElement(web_element, point);
    (void)result;
  }
}

void FuzzGetChildImageUrlFromElementRandomPoint(FuzzedDataProvider* fdp) {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupImageHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* container = doc->getElementById(AtomicString("container"));
  if (container) {
    blink::WebElement web_element(container);
    int x = fdp->ConsumeIntegral<int>();
    int y = fdp->ConsumeIntegral<int>();
    gfx::Point point(x, y);
    GURL result = g_context_menu_controller_ext->GetChildImageUrlFromElement(web_element, point);
    (void)result;
  }
}

void FuzzGetChildImageUrlFromLinkedImage(FuzzedDataProvider* fdp) {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupLinkImageHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* alink = doc->getElementById(AtomicString("alink"));
  if (alink) {
    blink::WebElement web_element(alink);
    int x = fdp->ConsumeIntegral<int>();
    int y = fdp->ConsumeIntegral<int>();
    gfx::Point point(x, y);
    GURL result = g_context_menu_controller_ext->GetChildImageUrlFromElement(web_element, point);
    (void)result;
  }
}

void FuzzGetImgChildWithBodyNode(FuzzedDataProvider* fdp) {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupMultipleImagesHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc || !doc->body()) {
    return;
  }
  blink::WebNode web_node(doc->body());
  int x = fdp->ConsumeIntegral<int>();
  int y = fdp->ConsumeIntegral<int>();
  gfx::Point point(x, y);
  blink::WebElement result = g_context_menu_controller_ext->GetImgChild(web_node, point);
  (void)result;
}

void FuzzGetAbsoluteSrcUrlWithNoSrcElement() {
  if (!g_context_menu_controller_ext) {
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
  Element* element = doc->getElementById(AtomicString("container"));
  if (element) {
    blink::WebElement web_element(element);
    GURL result = g_context_menu_controller_ext->GetAbsoluteSrcUrl(web_element);
    (void)result;
  }
}

HitTestResult CreateHitTestResultWithNode(Node* node) {
  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  gfx::Point point(0, 0);
  HitTestLocation location(point);
  HitTestResult result(request, location);
  if (node) {
    PhysicalOffset physicalPoint(point);
    result.SetNodeAndPosition(node, physicalPoint);
  }
  return result;
}

void FuzzShouldShowFreeCopyMenu() {
  if (!g_context_menu_controller_ext) {
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
  Node* body = doc->body();
  HitTestResult result = CreateHitTestResultWithNode(body);
  bool showMenu = g_context_menu_controller_ext->ShouldShowFreeCopyMenu(result);
  (void)showMenu;
}

void FuzzShouldShowFreeCopyMenuWithElement() {
  if (!g_context_menu_controller_ext) {
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
  Element* element = doc->getElementById(AtomicString("image1"));
  HitTestResult result = CreateHitTestResultWithNode(element);
  bool showMenu = g_context_menu_controller_ext->ShouldShowFreeCopyMenu(result);
  (void)showMenu;
}

void FuzzFindImgUrlNullHitTest(FuzzedDataProvider* fdp) {
  if (!g_context_menu_controller_ext) {
    return;
  }
  ContextMenuData data;
  HitTestResult result;
  int x = fdp->ConsumeIntegral<int>();
  int y = fdp->ConsumeIntegral<int>();
  gfx::Point point(x, y);
  PhysicalOffset physicalPoint(point);
  g_context_menu_controller_ext->FindImgUrl(data, result, physicalPoint);
}

void FuzzFindImgUrlWithHitTest(FuzzedDataProvider* fdp) {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupMultipleImagesHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* element = doc->getElementById(AtomicString("img1"));
  ContextMenuData data;
  HitTestResult result = CreateHitTestResultWithNode(element);
  int x = fdp->ConsumeIntegral<int>();
  int y = fdp->ConsumeIntegral<int>();
  gfx::Point point(x, y);
  PhysicalOffset physicalPoint(point);
  g_context_menu_controller_ext->FindImgUrl(data, result, physicalPoint);
}

void FuzzGetImgUrlNullHitTest() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  HitTestResult result;
  ContextMenuData data;
  g_context_menu_controller_ext->GetImgUrl(result, data, WebMenuSourceType::kMenuSourceMouse);
}

void FuzzGetImgUrlWithHitTest() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupMultipleImagesHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* element = doc->getElementById(AtomicString("img2"));
  HitTestResult result = CreateHitTestResultWithNode(element);
  ContextMenuData data;
  g_context_menu_controller_ext->GetImgUrl(result, data, WebMenuSourceType::kMenuSourceLongPress);
}

void FuzzSetArkWebMenuDataNullHitTest() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  ContextMenuData data;
  HitTestResult result;
  g_context_menu_controller_ext->SetArkWebMenuData(data, result);
}

void FuzzSetArkWebMenuDataWithHitTest() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupLinkImageHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* element = doc->getElementById(AtomicString("linkedimg"));
  HitTestResult result = CreateHitTestResultWithNode(element);
  ContextMenuData data;
  g_context_menu_controller_ext->SetArkWebMenuData(data, result);
}

void FuzzIsAILinkNullHitTest() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  ContextMenuData data;
  HitTestResult result;
  g_context_menu_controller_ext->IsAILink(data, result);
}

void FuzzIsAILinkWithHitTest() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupLinkImageHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* element = doc->getElementById(AtomicString("alink"));
  HitTestResult result = CreateHitTestResultWithNode(element);
  ContextMenuData data;
  g_context_menu_controller_ext->IsAILink(data, result);
}

void FuzzSetImageRectFromPotentialImageNodeNull() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  ContextMenuData data;
  g_context_menu_controller_ext->SetImageRectFromPotentialImageNode(data, nullptr);
}

void FuzzSetImageRectFromPotentialImageNodeWithNode() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupMultipleImagesHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* element = doc->getElementById(AtomicString("img3"));
  ContextMenuData data;
  g_context_menu_controller_ext->SetImageRectFromPotentialImageNode(data, element);
}

void FuzzHandleArkWebContextMenuNullHitTest() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  ContextMenuData data;
  HitTestResult result;
  g_context_menu_controller_ext->HandleArkWebContextMenu(data, frame, result, frame, WebMenuSourceType::kMenuSourceMouse);
}

void FuzzHandleArkWebContextMenuWithHitTest() {
  if (!g_context_menu_controller_ext) {
    return;
  }
  GetEnv().SetupLinkImageHtml();
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }
  Document* doc = frame->GetDocument();
  if (!doc) {
    return;
  }
  Element* element = doc->getElementById(AtomicString("alink"));
  HitTestResult result = CreateHitTestResultWithNode(element);
  ContextMenuData data;
  g_context_menu_controller_ext->HandleArkWebContextMenu(data, frame, result, frame, WebMenuSourceType::kMenuSourceLongTap);
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  blink::GetEnv();

  FuzzedDataProvider fdp(data, size);

  if (!blink::g_context_menu_controller_ext) {
    return 0;
  }

  int selector = fdp.ConsumeIntegralInRange<int>(0, 27);
  switch (selector) {
    case 0:
      blink::FuzzGetAbsoluteSrcUrlNullElement();
      break;
    case 1:
      blink::FuzzGetAbsoluteSrcUrlWithElement();
      break;
    case 2:
      blink::FuzzGetAbsoluteSrcUrlMultipleImages();
      break;
    case 3:
      blink::FuzzGetAbsoluteUrlNullNode();
      break;
    case 4:
      blink::FuzzGetAbsoluteUrlEmptyFragment();
      break;
    case 5:
      blink::FuzzGetAbsoluteUrlRandomFragment(&fdp);
      break;
    case 6:
      blink::FuzzGetImgChildRandomPoint(&fdp);
      break;
    case 7:
      blink::FuzzGetImgChildMultipleImages(&fdp);
      break;
    case 8:
      blink::FuzzGetChildImageUrlFromElementNullElement();
      break;
    case 9:
      blink::FuzzGetChildImageUrlFromElementWithLink(&fdp);
      break;
    case 10:
      blink::FuzzGetChildImageUrlFromElementRandomPoint(&fdp);
      break;
    case 11:
      blink::FuzzGetChildImageUrlFromLinkedImage(&fdp);
      break;
    case 12:
      blink::FuzzGetImgChildWithBodyNode(&fdp);
      break;
    case 13:
      blink::FuzzGetAbsoluteSrcUrlWithNoSrcElement();
      break;
    case 14:
      blink::FuzzGetImgChildRandomPoint(&fdp);
      blink::FuzzGetAbsoluteSrcUrlWithElement();
      break;
    case 15:
      blink::FuzzShouldShowFreeCopyMenu();
      break;
    case 16:
      blink::FuzzShouldShowFreeCopyMenuWithElement();
      break;
    case 17:
      blink::FuzzFindImgUrlNullHitTest(&fdp);
      break;
    case 18:
      blink::FuzzFindImgUrlWithHitTest(&fdp);
      break;
    case 19:
      blink::FuzzGetImgUrlNullHitTest();
      break;
    case 20:
      blink::FuzzGetImgUrlWithHitTest();
      break;
    case 21:
      blink::FuzzSetArkWebMenuDataNullHitTest();
      break;
    case 22:
      blink::FuzzSetArkWebMenuDataWithHitTest();
      break;
    case 23:
      blink::FuzzIsAILinkNullHitTest();
      break;
    case 24:
      blink::FuzzIsAILinkWithHitTest();
      break;
    case 25:
      blink::FuzzSetImageRectFromPotentialImageNodeNull();
      break;
    case 26:
      blink::FuzzSetImageRectFromPotentialImageNodeWithNode();
      break;
    case 27:
      blink::FuzzHandleArkWebContextMenuNullHitTest();
      break;
  }

  return 0;
}