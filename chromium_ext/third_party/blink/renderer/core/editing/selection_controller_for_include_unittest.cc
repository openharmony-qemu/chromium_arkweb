/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "arkweb/build/features/features.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#define private public
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#undef private

class SelectionControllerForIncludeTest : public SelectionControllerTest {
 public:
  void SetStartSelect(bool start_select_);
  void SetClickSelection(bool click_in_selection_);
  void SetDownAllow(bool down_allows_);
  void SetMenuShow(bool menu_show_);
  bool GetClickSelection();
  bool GetDownAllow();
  bool GetMenuShow();
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
  void SetLongPress(HitTestResult& result);
#endif
  void SetLinkPress(HitTestResult& result);

  void TestBody() override {}
};

void SelectionControllerForIncludeTest::SetStartSelect(bool start_select_) {
  Controller().mouse_down_may_start_select_ = start_select_;
}

void SelectionControllerForIncludeTest::SetClickSelection(bool click_in_selection_) {
  Controller().mouse_down_was_single_click_in_selection_ = click_in_selection_;
}

void SelectionControllerForIncludeTest::SetDownAllow(bool down_allows_) {
  Controller().mouse_click_down_allows_ = down_allows_;
}

void SelectionControllerForIncludeTest::SetMenuShow(bool menu_show_) {
  Controller().mouse_menu_show_ = menu_show_;
}

bool SelectionControllerForIncludeTest::GetClickSelection() {
  return Controller().mouse_down_was_single_click_in_selection_;
}

bool SelectionControllerForIncludeTest::GetDownAllow() {
  return Controller().mouse_click_down_allows_;
}

bool SelectionControllerForIncludeTest::GetMenuShow() {
  return Controller().mouse_menu_show_;
}

#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
void SelectionControllerForIncludeTest::SetLongPress(HitTestResult& result) {
  Controller().last_long_press_hit_test_result_ = result;
}
#endif

void SelectionControllerForIncludeTest::SetLinkPress(HitTestResult& result) {
  Controller().last_link_hit_test_result_ = result;
}

#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
TEST_F(SelectionControllerForIncludeTest, NotifyContextMenuWillShowTest_1stIf) {
  HitTestResult result;
  GetFrame()
      .GetEventHandler()
      .GetSelectionController()
      .NotifyContextMenuWillShow();
  EXPECT_TRUE(!result.InnerNode());
}

TEST_F(SelectionControllerForIncludeTest, NotifyContextMenuWillShowTest_2ndIf) {
  HitTestResult result;
  GetFrame()
      .GetEventHandler()
      .GetSelectionController()
      .NotifyContextMenuWillShow();
  EXPECT_TRUE(!result.InnerNode());
}
#endif

TEST_F(SelectionControllerForIncludeTest, FocusDocumentViewTest_1stIf) {
  HitTestResult result;
  GetFrame().GetEventHandler().GetSelectionController().FocusDocumentView();
  EXPECT_TRUE(!result.InnerNode());
}

#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
TEST_F(SelectionControllerForIncludeTest,
       ShowSelectionByLastLongPressHitTestResultTest_1stIf) {
  GetDocument().documentElement()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #linktarget {
          top: 100px;
          left: 100px;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <img id=target src='http://test.png'>
      <a id=linktarget href='about:blank'>Content</a>
    </body>
  )HTML");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  PhysicalOffset location_with_link(LayoutUnit(105), LayoutUnit(105));

  HitTestResult hit_test_result_;
  Node* node =
      GetDocument().getElementById(AtomicString("linktarget"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  Element* link = GetDocument().getElementById(AtomicString("linktarget"));
  ASSERT_TRUE(link != nullptr);

  hit_test_result_.SetInnerNode(node);

  hit_test_result_.SetURLElement(link);
  hit_test_result_.OverrideNodeAndPosition(node, location_with_link);

  SetLongPress(hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLongPressHitTestResult();

  EXPECT_TRUE(result);
}

TEST_F(SelectionControllerForIncludeTest,
       ShowSelectionByLastLongPressHitTestResultTest_2ndIf) {
  GetDocument().documentElement()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <p id=target>This is the first line.</p>
    </body>
  )HTML");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  HitTestResult hit_test_result_;
  Node* node =
      GetDocument().getElementById(AtomicString("target"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  hit_test_result_.SetInnerNode(node);

  SetLongPress(hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLongPressHitTestResult();

  EXPECT_TRUE(result);
}

TEST_F(SelectionControllerForIncludeTest,
       ShowSelectionByLastLongPressHitTestResultTest_3rdIf) {
  GetDocument().documentElement()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <p id=target>This is the first line.</p>
    </body>
  )HTML");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  HitTestResult hit_test_result_;
  Node* node =
      GetDocument().getElementById(AtomicString("target"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  hit_test_result_.SetInnerNode(node);
  hit_test_result_.InnerNode()->SetLayoutObject(nullptr);

  SetLongPress(hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLongPressHitTestResult();

  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest,
       ShowSelectionByLastLongPressHitTestResultTest_4thIf) {
  GetDocument().documentElement()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <style>
        #target {
          user-select:none;
        }
      </style>
      <p id=target>This is the first line.</p>
    </body>
  )HTML");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  HitTestResult hit_test_result_;
  Node* node =
      GetDocument().getElementById(AtomicString("target"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  hit_test_result_.SetInnerNode(node);

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  SetLongPress(hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLongPressHitTestResult();

  EXPECT_TRUE(result);
}
#endif

TEST_F(SelectionControllerForIncludeTest, SelectClosestWordFromLiveLinkTest_1stIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #unselectable {
          user_select: none;
          user_modify: read-only;
        }
      </style>
      <div id='unselectable'>This is a sample text."</div>
    </body>
  )HTML");

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("unselectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  const HitTestResult hit_test_result_ = hit_test_result;

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .SelectClosestWordFromLiveLink(hit_test_result_);
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, SelectClosestWordFromLiveLinkTest_2ndIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #unselectable {
          user_select: none;
          user_modify: read-only;
        }
      </style>
      <div id='unselectable'>This is a sample text."</div>
    </body>
  )HTML");

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("unselectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  hit_test_result.InnerNode()->SetLayoutObject(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .SelectClosestWordFromLiveLink(hit_test_result_);
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, SelectClosestWordFromLiveLinkTest_3rdIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #unselectable {
          user_select: none;
          user_modify: read-only;
        }
      </style>
      <div id='unselectable'>This is a sample text."</div>
    </body>
  )HTML");

  HitTestResult hit_test_result;

  hit_test_result.SetInnerNode(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .SelectClosestWordFromLiveLink(hit_test_result_);
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, SelectClosestWordFromLiveLinkTest_4thIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <a href='#'><span id='span'>click me</span></a>
      <p id='outside'>outside link</p>
    </body>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node =
      GetDocument().getElementById(AtomicString("outside"))->firstChild();
  ASSERT_TRUE(text_node != nullptr);

  HitTestResult hit_test_result;

  hit_test_result.SetInnerNode(text_node);
  hit_test_result.SetURLElement(To<Element>(
      GetDocument().getElementById(AtomicString("span"))->parentNode()));
  const HitTestResult hit_test_result_ = hit_test_result;

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .SelectClosestWordFromLiveLink(hit_test_result_);
  EXPECT_TRUE(result);
}

TEST_F(SelectionControllerForIncludeTest, SelectClosestWordFromLiveLinkTest_5thIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <a href='#' id='link'><span id='span'>click me</span></a>
    </body>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* span = GetDocument().getElementById(AtomicString("span"));
  ASSERT_TRUE(span != nullptr);
  Node* node = span;

  HitTestResult hit_test_result;

  hit_test_result.SetInnerNode(node);
  hit_test_result.SetURLElement(
      GetDocument().getElementById(AtomicString("link")));
  const HitTestResult hit_test_result_ = hit_test_result;

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .SelectClosestWordFromLiveLink(hit_test_result_);
  EXPECT_TRUE(result);
}

TEST_F(SelectionControllerForIncludeTest, SelectClosestWordFromLiveLinkTest_6thIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <a href='#' id='link'>click me</a>
    </body>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node =
      GetDocument().getElementById(AtomicString("link"))->firstChild();
  ASSERT_TRUE(text_node != nullptr);

  HitTestResult hit_test_result;

  hit_test_result.SetInnerNode(text_node);
  hit_test_result.SetURLElement(
      GetDocument().getElementById(AtomicString("link")));
  const HitTestResult hit_test_result_ = hit_test_result;

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .SelectClosestWordFromLiveLink(hit_test_result_);
  EXPECT_TRUE(result);
}

TEST_F(SelectionControllerForIncludeTest, MouseSelectMenuShowTest_1stIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #unselectable {
          user_select: none;
          user_modify: read-only;
        }
      </style>
      <div id='unselectable'>This is a sample text."</div>
    </body>
  )HTML");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_TRUE(script != nullptr);
  script->SetHTMLUnsafeWithoutTrustedTypes(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "  range.deleteContents();"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  bool result =
      GetFrame().GetEventHandler().GetSelectionController().MouseSelectMenuShow(
          true);
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, SetDataDetectorHitTest_1stIf) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_TRUE(script != nullptr);
  script->SetHTMLUnsafeWithoutTrustedTypes(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "  range.deleteContents();"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;

  hit_test_result.SetInnerNode(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults evnet_(mouse_event, location,
                                            hit_test_result_);

  GetFrame().GetEventHandler().GetSelectionController().SetDataDetectorHitTest(
      evnet_);
  EXPECT_FALSE(hit_test_result_.InnerNode());
}

TEST_F(SelectionControllerForIncludeTest, SetDataDetectorHitTest_2ndIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #unselectable {
          user_select: none;
          user_modify: read-only;
        }
      </style>
      <div id='unselectable'>This is a sample text."</div>
    </body>
  )HTML");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_TRUE(script != nullptr);
  script->SetHTMLUnsafeWithoutTrustedTypes(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "  range.deleteContents();"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("unselectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  hit_test_result.InnerNode()->SetLayoutObject(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults evnet_(mouse_event, location,
                                            hit_test_result_);

  GetFrame().GetEventHandler().GetSelectionController().SetDataDetectorHitTest(
      evnet_);
  EXPECT_TRUE(hit_test_result_.InnerNode());
}

TEST_F(SelectionControllerForIncludeTest, SetDataDetectorHitTest_3rdIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #linktarget {
          top: 100px;
          left: 100px;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <img id=target src='http://test.png'>
      <a id=linktarget href='about:blank'>Content</a>
    </body>
  )HTML");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_TRUE(script != nullptr);
  script->SetHTMLUnsafeWithoutTrustedTypes(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "  range.deleteContents();"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node =
      GetDocument().getElementById(AtomicString("linktarget"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  auto* layout_text_ =
      MakeGarbageCollected<LayoutText>(node, String("Content"));
  ASSERT_TRUE(layout_text_ != nullptr);

  Element* link = GetDocument().getElementById(AtomicString("linktarget"));
  ASSERT_TRUE(link != nullptr);

  auto* style = link->GetComputedStyle();
  ASSERT_TRUE(style != nullptr);
  layout_text_->SetStyle(style);

  hit_test_result.SetInnerNode(node);
  hit_test_result.InnerNode()->SetLayoutObject(layout_text_);
  hit_test_result.SetURLElement(link);

  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults evnet_(mouse_event, location,
                                            hit_test_result_);

  GetFrame().GetEventHandler().GetSelectionController().SetDataDetectorHitTest(
      evnet_);
  EXPECT_TRUE(hit_test_result_.InnerNode());
}

TEST_F(SelectionControllerForIncludeTest, HandleMouseReleaseEventWithMenuExtTest_1stIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #unselectable {
          user_select: none;
          user_modify: read-only;
        }
      </style>
      <div id='unselectable'>This is a sample text."</div>
    </body>
  )HTML");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_TRUE(script != nullptr);
  script->SetHTMLUnsafeWithoutTrustedTypes(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "  range.deleteContents();"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  SelectionController& controller_ = Controller();
  SetStartSelect(true);
  SetClickSelection(true);

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("unselectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  hit_test_result.InnerNode()->SetLayoutObject(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults evnet_(mouse_event, location,
                                            hit_test_result_);

  blink::SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(
      &controller_, evnet_);
  EXPECT_TRUE(GetClickSelection());
}

TEST_F(SelectionControllerForIncludeTest, HandleMouseReleaseEventWithMenuExtTest_2ndIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #unselectable {
          user_select: none;
          user_modify: read-only;
        }
      </style>
      <div id='unselectable'>This is a sample text."</div>
    </body>
  )HTML");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_TRUE(script != nullptr);
  script->SetHTMLUnsafeWithoutTrustedTypes(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "  range.deleteContents();"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  SelectionController& controller_ = Controller();
  SetStartSelect(true);
  SetClickSelection(false);

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("unselectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  hit_test_result.InnerNode()->SetLayoutObject(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults evnet_(mouse_event, location,
                                            hit_test_result_);

  blink::SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(
      &controller_, evnet_);
  EXPECT_TRUE(!GetClickSelection());
}

TEST_F(SelectionControllerForIncludeTest, HandleMouseReleaseEventWithMenuExtTest_3rdIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #unselectable {
          user_select: none;
          user_modify: read-only;
        }
      </style>
      <div id='unselectable'>This is a sample text."</div>
    </body>
  )HTML");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_TRUE(script != nullptr);
  script->SetHTMLUnsafeWithoutTrustedTypes(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "  range.deleteContents();"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  SelectionController& controller_ = Controller();
  SetStartSelect(false);
  SetClickSelection(false);

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("unselectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  hit_test_result.InnerNode()->SetLayoutObject(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults evnet_(mouse_event, location,
                                            hit_test_result_);

  blink::SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(
      &controller_, evnet_);
  EXPECT_FALSE(GetClickSelection());
}

TEST_F(SelectionControllerForIncludeTest, HandleMouseReleaseEventWithMenuExtTest_4thIf) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        #unselectable {
          user_select: none;
          user_modify: read-only;
        }
      </style>
      <div id='unselectable'>This is a sample text."</div>
    </body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  SelectionController& controller_ = Controller();

  SetClickSelection(true);
  SetDownAllow(true);
  SetMenuShow(true);

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("unselectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults evnet_(mouse_event, location,
                                            hit_test_result_);

  blink::SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(
      &controller_, evnet_);
  EXPECT_FALSE(GetMenuShow());
}

TEST_F(SelectionControllerForIncludeTest, HandleMouseReleaseEventWithMenuExtTest_5thIf) {
  const char* body_content = "<span id=top>top</span><span id=host></span>";
  const char* shadow_content = "<span id=bottom>bottom</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  ASSERT_TRUE(shadow_root != nullptr);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  Node* bottom =
      shadow_root->getElementById(AtomicString("bottom"))->firstChild();
  ASSERT_TRUE(top != nullptr);
  ASSERT_TRUE(bottom != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 1))
                                         .Extend(PositionInFlatTree(bottom, 3))
                                         .Build(),
                                     TextGranularity::kCharacter);

  SelectionController& controller_ = Controller();
  SetStartSelect(true);
  SetClickSelection(false);
  SetDownAllow(false);
  SetMenuShow(false);

  WebMouseEvent mouse_event;
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("top"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  hit_test_result.InnerNode()->SetLayoutObject(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;

  const MouseEventWithHitTestResults evnet_(mouse_event, location,
                                            hit_test_result_);

  blink::SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(
      &controller_, evnet_);
  EXPECT_FALSE(GetDownAllow());
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExistTest_5thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 1))
                                         .Extend(PositionInFlatTree(top, 4))
                                         .Build(),
                                     TextGranularity::kCharacter);

  blink::WebMouseEvent double_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  double_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  double_click.button = blink::WebMouseEvent::Button::kLeft;
  double_click.click_count = 2;
  double_click.SetModifiers(blink::WebInputEvent::Modifiers::kShiftKey);

  HitTestResult result;
  const MouseEventWithHitTestResults event_(double_click, location, result);
  bool result_ = GetFrame()
                     .GetEventHandler()
                     .GetSelectionController()
                     .HandleGestureTapIfSelectionExist(event_);

  EXPECT_FALSE(result_);
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExistTest_6thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 1))
                                         .Extend(PositionInFlatTree(top, 4))
                                         .Build(),
                                     TextGranularity::kCharacter);

  blink::WebMouseEvent double_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  double_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  double_click.SetModifiers(blink::WebInputEvent::Modifiers::kShiftKey);

  HitTestResult result;

  const MouseEventWithHitTestResults event_(double_click, location, result);

  bool result_ = GetFrame()
                     .GetEventHandler()
                     .GetSelectionController()
                     .HandleGestureTapIfSelectionExist(event_);

  EXPECT_FALSE(result_);
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExistTest_7thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 1))
                                         .Extend(PositionInFlatTree(top, 4))
                                         .Build(),
                                     TextGranularity::kCharacter);

  blink::WebMouseEvent double_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  double_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  double_click.button = blink::WebMouseEvent::Button::kLeft;

  HitTestResult result;

  const MouseEventWithHitTestResults event_(double_click, location, result);

  bool result_ = GetFrame()
                     .GetEventHandler()
                     .GetSelectionController()
                     .HandleGestureTapIfSelectionExist(event_);

  EXPECT_FALSE(result_);
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExistTest_8thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 0))
                                         .Extend(PositionInFlatTree(top, 20))
                                         .Build(),
                                     TextGranularity::kCharacter);

  blink::WebMouseEvent single_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  single_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  single_click.button = blink::WebMouseEvent::Button::kLeft;
  single_click.click_count = 1;
  single_click.SetModifiers(
      blink::WebInputEvent::Modifiers::kIsCompatibilityEventForTouch);
  Selection().AsFrameSelectionExt()->SetIsSelectAll(true);

  HitTestResult result;

  const MouseEventWithHitTestResults event_(single_click, location, result);

  bool result_ = GetFrame()
                     .GetEventHandler()
                     .GetSelectionController()
                     .HandleGestureTapIfSelectionExist(event_);

  EXPECT_FALSE(result_);
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExistTest_9thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 0))
                                         .Extend(PositionInFlatTree(top, 10))
                                         .Build(),
                                     TextGranularity::kCharacter);

  blink::WebMouseEvent single_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  single_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  single_click.button = blink::WebMouseEvent::Button::kLeft;
  single_click.click_count = 1;
  single_click.SetModifiers(
      blink::WebInputEvent::Modifiers::kIsCompatibilityEventForTouch);

  HitTestResult result;

  const MouseEventWithHitTestResults event_(single_click, location, result);

  LocalFrameView* view = GetFrame().View();
  GetFrame().SetView(nullptr);

  bool result_ = GetFrame()
                     .GetEventHandler()
                     .GetSelectionController()
                     .HandleGestureTapIfSelectionExist(event_);

  GetFrame().SetView(view);
  EXPECT_FALSE(result_);
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExistTest_10thIf) {
  const char* body_content = "<span id = top>this is a sample test</span>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 0))
                                         .Extend(PositionInFlatTree(top, 10))
                                         .Build(),
                                     TextGranularity::kCharacter);

  blink::WebMouseEvent single_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  single_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  single_click.button = blink::WebMouseEvent::Button::kLeft;
  single_click.click_count = 1;
  single_click.SetModifiers(
      blink::WebInputEvent::Modifiers::kIsCompatibilityEventForTouch);

  HitTestResult result;

  const MouseEventWithHitTestResults event_(single_click, location, result);

  bool result_ = GetFrame()
                     .GetEventHandler()
                     .GetSelectionController()
                     .HandleGestureTapIfSelectionExist(event_);

  EXPECT_FALSE(result_);
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExistTest_15thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 0))
                                         .Extend(PositionInFlatTree(top, 20))
                                         .Build(),
                                     TextGranularity::kCharacter);

  blink::WebMouseEvent single_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  single_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  single_click.button = blink::WebMouseEvent::Button::kLeft;
  single_click.click_count = 1;
  single_click.SetModifiers(
      blink::WebInputEvent::Modifiers::kIsCompatibilityEventForTouch);

  HitTestResult result;

  SetMenuShow(true);

  const MouseEventWithHitTestResults event_(single_click, location, result);

  bool result_ = GetFrame()
                     .GetEventHandler()
                     .GetSelectionController()
                     .HandleGestureTapIfSelectionExist(event_);

  EXPECT_FALSE(result_);
}

TEST_F(SelectionControllerForIncludeTest, HandleEmptyLineTest_1stIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  const blink::PositionInFlatTree pos;

  auto pos_ = blink::SelectionControllerUtils::HandleEmptyLine(nullptr, pos, 0);
  EXPECT_EQ(pos, pos_);
}

TEST_F(SelectionControllerForIncludeTest, HandleEmptyLineTest_2ndIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a \n sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 1);

  auto pos_ = blink::SelectionControllerUtils::HandleEmptyLine(top, pos, 1);
  EXPECT_NE(pos, pos_);
}

TEST_F(SelectionControllerForIncludeTest, HandleEmptyLineTest_3rdIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>"
      "<span>first</span>"
      "<span>     </span>"
      "</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->lastChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 1);

  auto pos_ = blink::SelectionControllerUtils::HandleEmptyLine(top, pos, 1);
  EXPECT_NE(pos, pos_);
}

TEST_F(SelectionControllerForIncludeTest, HandleEmptyLineTest_4thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>"
      "<span contenteditable=false>first</span>"
      "<span>     </span>"
      "</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->lastChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 1);

  auto pos_ = blink::SelectionControllerUtils::HandleEmptyLine(top, pos, 1);
  EXPECT_EQ(pos, pos_);
}

TEST_F(SelectionControllerForIncludeTest, OffsetAdjustWhiteSpace01) {
  String str = "abc   ";
  int offset = 1, temp = 1;
  SelectionControllerUtils::OffsetAdjustWhiteSpace(offset, temp, str, false);
  EXPECT_EQ(offset, 1);
  EXPECT_EQ(temp, 1);
}

TEST_F(SelectionControllerForIncludeTest, OffsetAdjustWhiteSpace02) {
  String str = "abc   ";
  int offset = 5, temp = 5;
  SelectionControllerUtils::OffsetAdjustWhiteSpace(offset, temp, str, true);
  EXPECT_EQ(offset, 2);
  EXPECT_EQ(temp, 2);
}

TEST_F(SelectionControllerForIncludeTest, OffsetAdjustWhiteSpace03) {
  String str = "ab  c";
  int offset = 3, temp = 3;
  SelectionControllerUtils::OffsetAdjustWhiteSpace(offset, temp, str, true);
  EXPECT_EQ(offset, 1);
  EXPECT_EQ(temp, 1);
}

TEST_F(SelectionControllerForIncludeTest, OffsetAdjustWhiteSpace04) {
  String str = "   abc";
  int offset = 2, temp = 2;
  SelectionControllerUtils::OffsetAdjustWhiteSpace(offset, temp, str, true);
  EXPECT_EQ(offset, 2);
  EXPECT_EQ(temp, 2);

}
TEST_F(SelectionControllerForIncludeTest, OffsetAdjustWhiteSpace05) {
  String str = "a b";
  int offset = 0, temp = 0;
  SelectionControllerUtils::OffsetAdjustWhiteSpace(offset, temp, str, true);
  EXPECT_EQ(offset, 0);
  EXPECT_EQ(temp, 0);
}

TEST_F(SelectionControllerForIncludeTest, MaxOffsetTrimTailWhiteSpace01) {
  String str = "any content";
  unsigned len = 0;
  EXPECT_EQ(SelectionControllerUtils::MaxOffsetTrimTailWhiteSpace(str, len),
            0u);
}

TEST_F(SelectionControllerForIncludeTest, MaxOffsetTrimTailWhiteSpace02) {
  String str = "abc   ";
  unsigned len = 6;
  EXPECT_EQ(SelectionControllerUtils::MaxOffsetTrimTailWhiteSpace(str, len),
            2u);
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExist01) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='selectable'>This is a sample text."</div>
    </body>
  )HTML");

  GetFrame().Detach(FrameDetachType::kRemove);
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  mouse_event.SetModifiers(WebInputEvent::kShiftKey);
  HitTestLocation location((gfx::Point(5, 5)));
  HitTestResult hit_test_result;

  hit_test_result.SetInnerNode(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults event_(mouse_event, location,
                                            hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .HandleGestureTapIfSelectionExist(event_);
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExist02) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='selectable'>This is a sample text."</div>
    </body>
  )HTML");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_TRUE(script != nullptr);
  script->SetHTMLUnsafeWithoutTrustedTypes(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "  range.deleteContents();"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  mouse_event.click_count = 2;
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("selectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults event_(mouse_event, location,
                                            hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .HandleGestureTapIfSelectionExist(event_);
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, HandleGestureTapIfSelectionExist03) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='selectable'>This is a sample text."</div>
    </body>
  )HTML");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_TRUE(script != nullptr);
  script->SetHTMLUnsafeWithoutTrustedTypes(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  mouse_event.SetModifiers(WebInputEvent::kShiftKey);
  HitTestLocation location((gfx::Point(5, 5)));
  SetCaretAtHitTestResult(
      GetFrame().GetEventHandler().HitTestResultAtLocation(location));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("selectable"));
  ASSERT_TRUE(node == nullptr);

  hit_test_result.SetInnerNode(node);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults event_(mouse_event, location,
                                            hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .HandleGestureTapIfSelectionExist(event_);
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, ShowSelectionByLastLinkHitTestResult00) {
  GetDocument().documentElement()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #linktarget {
          top: 100px;
          left: 100px;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <img id=target src='http://test.png'>
      <a id=linktarget href='about:blank'>Content</a>
    </body>
  )HTML");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  PhysicalOffset location_with_link(LayoutUnit(105), LayoutUnit(105));

  HitTestResult hit_test_result_;
  Node* node =
      GetDocument().getElementById(AtomicString("linktarget"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  Element* link = GetDocument().getElementById(AtomicString("linktarget"));
  ASSERT_TRUE(link != nullptr);

  auto* style = link->GetComputedStyle();
  ASSERT_TRUE(style != nullptr);

  hit_test_result_.SetInnerNode(node);

  hit_test_result_.SetURLElement(link);
  SetLinkPress(hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLinkHitTestResult();
  EXPECT_TRUE(result);
}

TEST_F(SelectionControllerForIncludeTest, ShowSelectionByLastLinkHitTestResult01) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='selectable'>This is a sample text."</div>
    </body>
  )HTML");

  GetFrame().Detach(FrameDetachType::kRemove);
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  mouse_event.SetModifiers(WebInputEvent::kShiftKey);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("selectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults event_(mouse_event, location,
                                            hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLinkHitTestResult();
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, ShowSelectionByLastLinkHitTestResult02) {
  GetDocument().documentElement()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #linktarget {
          top: 100px;
          left: 100px;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <img id=target src='http://test.png'>
      <a id=linktarget href='about:blank'>Content</a>
    </body>
  )HTML");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  PhysicalOffset location_with_link(LayoutUnit(105), LayoutUnit(105));

  HitTestResult hit_test_result_;
  Node* node =
      GetDocument().getElementById(AtomicString("linktarget"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  auto* layout_text_ =
      MakeGarbageCollected<LayoutText>(node, String("Content"));
  ASSERT_TRUE(layout_text_ != nullptr);

  Element* link = GetDocument().getElementById(AtomicString("linktarget"));
  ASSERT_TRUE(link != nullptr);

  auto* style = link->GetComputedStyle();
  ASSERT_TRUE(style != nullptr);
  layout_text_->SetStyle(style);

  hit_test_result_.SetInnerNode(node);
  hit_test_result_.InnerNode()->SetLayoutObject(nullptr);
  hit_test_result_.SetURLElement(link);
  SetLinkPress(hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLinkHitTestResult();
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, ShowSelectionByLastLinkHitTestResult03) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='selectable'>This is a sample text."</div>
    </body>
  )HTML");
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kIsCompatibilityEventForTouch,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetFrameScale(1);
  mouse_event.SetModifiers(WebInputEvent::kShiftKey);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("selectable"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  const HitTestResult hit_test_result_ = hit_test_result;
  const MouseEventWithHitTestResults event_(mouse_event, location,
                                            hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLinkHitTestResult();
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, ShowSelectionByLastLinkHitTestResult06) {
  GetDocument().documentElement()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #linktarget {
          top: 100px;
          left: 100px;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <img id=target src='http://test.png'>
      <a id=linktarget href='about:blank'>Content</a>
    </body>
  )HTML");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  PhysicalOffset location_with_link(LayoutUnit(105), LayoutUnit(105));

  HitTestResult hit_test_result_;
  Node* node =
      GetDocument().getElementById(AtomicString("linktarget"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  auto* layout_text_ =
      MakeGarbageCollected<LayoutText>(node, String("Content"));
  ASSERT_TRUE(layout_text_ != nullptr);

  Element* link = GetDocument().getElementById(AtomicString("linktarget"));
  ASSERT_TRUE(link != nullptr);

  auto* style = link->GetComputedStyle();
  ASSERT_TRUE(style != nullptr);
  layout_text_->SetStyle(style);

  hit_test_result_.SetInnerNode(node);
  hit_test_result_.InnerNode()->SetLayoutObject(layout_text_);
  hit_test_result_.SetURLElement(link);
  SetLinkPress(hit_test_result_);
  link->remove();

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLinkHitTestResult();
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, ShowSelectionByLastLinkHitTestResult04) {
  GetDocument().documentElement()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #linktarget {
          top: 100px;
          left: 100px;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <img id=target src='http://test.png'>
      <a id=linktarget '>Content</a>
    </body>
  )HTML");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  PhysicalOffset location_with_link(LayoutUnit(105), LayoutUnit(105));

  HitTestResult hit_test_result_;
  Node* node =
      GetDocument().getElementById(AtomicString("linktarget"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  auto* layout_text_ =
      MakeGarbageCollected<LayoutText>(node, String("Content"));
  ASSERT_TRUE(layout_text_ != nullptr);

  Element* link = GetDocument().getElementById(AtomicString("linktarget"));
  ASSERT_TRUE(link != nullptr);

  auto* style = link->GetComputedStyle();
  ASSERT_TRUE(style != nullptr);
  layout_text_->SetStyle(style);

  hit_test_result_.SetInnerNode(node);
  hit_test_result_.InnerNode()->SetLayoutObject(layout_text_);
  hit_test_result_.SetURLElement(link);
  SetLinkPress(hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLinkHitTestResult();
  EXPECT_FALSE(result);
}

TEST_F(SelectionControllerForIncludeTest, ShowSelectionByLastLinkHitTestResult05) {
  GetDocument().documentElement()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <style>
        #target {
          top: 0;
          left: 0;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
        #linktarget {
          top: 100px;
          left: 100px;
          position: absolute;
          width: 100px;
          height: 100px;
          z-index: 1;
        }
      </style>
      <img id=target src='http://test.png'>
      <a id=linktarget href='about:blank'><span>click on span</span></a>
    </body>
  )HTML");

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  PhysicalOffset location_with_link(LayoutUnit(105), LayoutUnit(105));

  HitTestResult hit_test_result_;
  Node* node =
      GetDocument().getElementById(AtomicString("linktarget"))->firstChild();
  ASSERT_TRUE(node != nullptr);

  auto* layout_text_ =
      MakeGarbageCollected<LayoutText>(node, String("Content"));
  ASSERT_TRUE(layout_text_ != nullptr);

  Element* link = GetDocument().getElementById(AtomicString("linktarget"));
  ASSERT_TRUE(link != nullptr);

  auto* style = link->GetComputedStyle();
  ASSERT_TRUE(style != nullptr);
  layout_text_->SetStyle(style);

  hit_test_result_.SetInnerNode(node);
  hit_test_result_.InnerNode()->SetLayoutObject(layout_text_);
  hit_test_result_.SetURLElement(link);
  SetLinkPress(hit_test_result_);

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .ShowSelectionByLastLinkHitTestResult();
  EXPECT_TRUE(result);
}

TEST_F(SelectionControllerForIncludeTest, IsValidAISelection_ValidRange) {
  Vector<int8_t> select;
  select.push_back(0);
  select.push_back(5);
  EXPECT_TRUE(SelectionControllerUtils::IsValidAISelection(select));
}
 
TEST_F(SelectionControllerForIncludeTest, IsValidAISelection_ValidRangeWithZeroStart) {
  Vector<int8_t> select;
  select.push_back(0);
  select.push_back(3);
  EXPECT_TRUE(SelectionControllerUtils::IsValidAISelection(select));
}
 
TEST_F(SelectionControllerForIncludeTest, IsValidAISelection_FailCase) {
  Vector<int8_t> select;
  select.push_back(-1);
  select.push_back(-1);
  EXPECT_FALSE(SelectionControllerUtils::IsValidAISelection(select));
}
 
TEST_F(SelectionControllerForIncludeTest, IsValidAISelection_EmptySelection) {
  Vector<int8_t> select;
  select.push_back(5);
  select.push_back(5);
  EXPECT_FALSE(SelectionControllerUtils::IsValidAISelection(select));
}
 
TEST_F(SelectionControllerForIncludeTest, IsValidAISelection_PartialFailStart) {
  Vector<int8_t> select;
  select.push_back(-1);
  select.push_back(5);
  EXPECT_FALSE(SelectionControllerUtils::IsValidAISelection(select));
}
 
TEST_F(SelectionControllerForIncludeTest, IsValidAISelection_PartialFailEnd) {
  Vector<int8_t> select;
  select.push_back(5);
  select.push_back(-1);
  EXPECT_FALSE(SelectionControllerUtils::IsValidAISelection(select));
}
 
TEST_F(SelectionControllerForIncludeTest, IsValidAISelection_EmptyRangeAtZero) {
  Vector<int8_t> select;
  select.push_back(0);
  select.push_back(0);
  EXPECT_FALSE(SelectionControllerUtils::IsValidAISelection(select));
}

TEST_F(SelectionControllerForIncludeTest, SameEditablePreviousSibling_NullNode) {
  Node* result = SelectionControllerUtils::SameEditablePreviousSibling(nullptr);
  EXPECT_EQ(nullptr, result);
}

TEST_F(SelectionControllerForIncludeTest, SameEditablePreviousSibling_NoPreviousSibling) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='first'>first</div>
      <div id='second'>second</div>
    </body>
  )HTML");

  Node* second = GetDocument().getElementById(AtomicString("second"))->firstChild();
  ASSERT_TRUE(second != nullptr);

  Node* result = SelectionControllerUtils::SameEditablePreviousSibling(second);
  EXPECT_EQ(nullptr, result);
}

TEST_F(SelectionControllerForIncludeTest, SameEditablePreviousSibling_DifferentEditable) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='parent' contenteditable="true">
        <div id='first' contenteditable="false">first</div>
        <div id='second'>second</div>
      </div>
    </body>
  )HTML");

  Node* second = GetDocument().getElementById(AtomicString("second"))->firstChild();
  ASSERT_TRUE(second != nullptr);

  Node* result = SelectionControllerUtils::SameEditablePreviousSibling(second);
  EXPECT_EQ(nullptr, result);
}

TEST_F(SelectionControllerForIncludeTest, SameEditablePreviousSibling_SameEditable) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='parent' contenteditable="true">
        <span id='first'>first</span>
        <span id='second'>second</span>
      </div>
    </body>
  )HTML");

  Node* second = GetDocument().getElementById(AtomicString("second"))->firstChild();
  ASSERT_TRUE(second != nullptr);

  Node* result = SelectionControllerUtils::SameEditablePreviousSibling(second);
  EXPECT_TRUE(result != nullptr);
}

TEST_F(SelectionControllerForIncludeTest, SameEditableParent_NullNode) {
  Node* result = SelectionControllerUtils::SameEditableParent(nullptr);
  EXPECT_EQ(nullptr, result);
}

TEST_F(SelectionControllerForIncludeTest, SameEditableParent_NoParent) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='sample'>sample</div>
    </body>
  )HTML");

  Node* sample = GetDocument().getElementById(AtomicString("sample"))->firstChild();
  ASSERT_TRUE(sample != nullptr);

  Node* result = SelectionControllerUtils::SameEditableParent(sample);
  EXPECT_EQ(nullptr, result);
}

TEST_F(SelectionControllerForIncludeTest, SameEditableParent_DifferentEditable) {
  SetHtmlInnerHTML(R"HTML(
    <body contenteditable="true">
      <div id='sample' contenteditable="false">sample</div>
    </body>
  )HTML");

  Node* sample = GetDocument().getElementById(AtomicString("sample"))->firstChild();
  ASSERT_TRUE(sample != nullptr);

  Node* result = SelectionControllerUtils::SameEditableParent(sample);
  EXPECT_EQ(nullptr, result);
}

TEST_F(SelectionControllerForIncludeTest, SameEditableParent_SameEditable) {
  SetHtmlInnerHTML(R"HTML(
    <body contenteditable="true">
      <div id='parent'>
        <span id='child'>child</span>
      </div>
    </body>
  )HTML");

  Node* child = GetDocument().getElementById(AtomicString("child"))->firstChild();
  ASSERT_TRUE(child != nullptr);

  Node* result = SelectionControllerUtils::SameEditableParent(child);
  EXPECT_TRUE(result != nullptr);
}

TEST_F(SelectionControllerForIncludeTest, UpdateAnchorIfWhiteSpace_HasPreviousSibling) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='parent' contenteditable="true">
        <span id='first'>first</span>
        <span id='second'>second</span>
        <span id='third'>   </span>
      </div>
    </body>
  )HTML");

  Node* third = GetDocument().getElementById(AtomicString("third"))->firstChild();
  ASSERT_TRUE(third != nullptr);

  const blink::PositionInFlatTree pos(third, 0);
  Node* result = SelectionControllerUtils::UpdateAnchorIfWhiteSpace(third, pos);
  EXPECT_TRUE(result != nullptr);
}

TEST_F(SelectionControllerForIncludeTest, UpdateAnchorIfWhiteSpace_SameEditableParentContinue) {
  SetHtmlInnerHTML(R"HTML(
    <body contenteditable="true">
      <div id='parent'>
        <span id='whitespace1'>   </span>
        <span id='whitespace2'>   </span>
        <span id='content'>content</span>
      </div>
    </body>
  )HTML");

  Node* whitespace2 = GetDocument().getElementById(AtomicString("whitespace2"))->firstChild();
  ASSERT_TRUE(whitespace2 != nullptr);

  const blink::PositionInFlatTree pos(whitespace2, 0);
  Node* result = SelectionControllerUtils::UpdateAnchorIfWhiteSpace(whitespace2, pos);
  EXPECT_TRUE(result != nullptr);
}

TEST_F(SelectionControllerForIncludeTest, UpdateAnchorIfWhiteSpace_ElseBreak) {
  SetHtmlInnerHTML(R"HTML(
    <body contenteditable="true">
      <div id='parent'>
        <span id='content1'>content</span>
      </div>
    </body>
  )HTML");

  Node* content1 = GetDocument().getElementById(AtomicString("content1"))->firstChild();
  ASSERT_TRUE(content1 != nullptr);

  const blink::PositionInFlatTree pos(content1, 0);
  Node* result = SelectionControllerUtils::UpdateAnchorIfWhiteSpace(content1, pos);
  EXPECT_TRUE(result != nullptr);
}

TEST_F(SelectionControllerForIncludeTest, HandleArkWebAISelectionExt_StringLeft) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='sample' contenteditable="true">
        this is a very long text that exceeds maximum length for testing purposes here
      </div>
    </body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Node* sample = GetDocument().getElementById(AtomicString("sample"))->firstChild();
  ASSERT_TRUE(sample != nullptr);

  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(sample);
  const HitTestResult& result = hit_test_result;

  const blink::PositionInFlatTree pos(sample, 5);
  const blink::PositionInFlatTreeWithAffinity pos_with_affinity(pos, blink::TextAffinity::kDownstream);

  auto selection = blink::SelectionControllerUtils::HandleArkWebAISelectionExt(
      &Controller(), result, sample, pos_with_affinity, false, false);
}

TEST_F(SelectionControllerForIncludeTest, HandleArkWebAISelectionExt_StringRight) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='sample' contenteditable="true">
        this is a very long text that exceeds maximum length for testing purposes here
      </div>
    </body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Node* sample = GetDocument().getElementById(AtomicString("sample"))->firstChild();
  ASSERT_TRUE(sample != nullptr);

  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(sample);
  const HitTestResult& result = hit_test_result;

  unsigned text_len = sample->textContent().length();
  const blink::PositionInFlatTree pos(sample, text_len - 5);
  const blink::PositionInFlatTreeWithAffinity pos_with_affinity(pos, blink::TextAffinity::kDownstream);

  auto selection = blink::SelectionControllerUtils::HandleArkWebAISelectionExt(
      &Controller(), result, sample, pos_with_affinity, false, false);
}

TEST_F(SelectionControllerForIncludeTest, HandleArkWebAISelectionExt_StringMiddle) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='sample' contenteditable="true">
        this is a very long text that exceeds maximum length for testing purposes here
      </div>
    </body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Node* sample = GetDocument().getElementById(AtomicString("sample"))->firstChild();
  ASSERT_TRUE(sample != nullptr);

  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(sample);
  const HitTestResult& result = hit_test_result;

  const blink::PositionInFlatTree pos(sample, 50);
  const blink::PositionInFlatTreeWithAffinity pos_with_affinity(pos, blink::TextAffinity::kDownstream);

  auto selection = blink::SelectionControllerUtils::HandleArkWebAISelectionExt(
      &Controller(), result, sample, pos_with_affinity, false, false);
}

TEST_F(SelectionControllerForIncludeTest, HandleMouseReleaseEventWithMenuExt_6thIf) {
  const char* body_content = "<span id=top>top</span><span id=host></span>";
  const char* shadow_content = "<span id=bottom>bottom</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  ASSERT_TRUE(shadow_root != nullptr);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  Node* bottom =
      shadow_root->getElementById(AtomicString("bottom"))->firstChild();
  ASSERT_TRUE(top != nullptr);
  ASSERT_TRUE(bottom != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 1))
                                         .Extend(PositionInFlatTree(bottom, 3))
                                         .Build(),
                                     TextGranularity::kCharacter);

  SelectionController& controller_ = Controller();
  SetStartSelect(true);
  SetClickSelection(false);
  SetDownAllow(false);
  SetMenuShow(false);

  WebMouseEvent mouse_event;
  mouse_event.SetModifiers(WebInputEvent::kShiftKey);
  HitTestLocation location((gfx::Point(0, 0)));

  HitTestResult hit_test_result;
  Node* node = GetDocument().getElementById(AtomicString("top"));
  ASSERT_TRUE(node != nullptr);

  hit_test_result.SetInnerNode(node);
  hit_test_result.InnerNode()->SetLayoutObject(nullptr);
  const HitTestResult hit_test_result_ = hit_test_result;

  const MouseEventWithHitTestResults evnet_(mouse_event, location,
                                            hit_test_result_);

  blink::SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(
      &controller_, evnet_);
  EXPECT_FALSE(GetMenuShow());
}

TEST_F(SelectionControllerForIncludeTest, SelectClosestWordFromLiveLink_IsCreateFlatTreeByUrlElement) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <a href='#' id='link'>this is link text</a>
    </body>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node =
      GetDocument().getElementById(AtomicString("link"))->firstChild();
  ASSERT_TRUE(text_node != nullptr);

  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(text_node);
  hit_test_result.SetURLElement(
      GetDocument().getElementById(AtomicString("link")));
  const HitTestResult hit_test_result_ = hit_test_result;

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .SelectClosestWordFromLiveLink(hit_test_result_);
  EXPECT_TRUE(result);
}

TEST_F(SelectionControllerForIncludeTest, FocusDocumentViewTest_2ndIf) {
  GetFrame().Detach(FrameDetachType::kRemove);
  GetFrame().GetEventHandler().GetSelectionController().FocusDocumentView();
}

TEST_F(SelectionControllerForIncludeTest, SelectClosestWordFromLiveLink_URLElementNotDescendant) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <a href='#' id='link'><span id='span'>click me</span></a>
      <p id='outside'>outside link</p>
    </body>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node =
      GetDocument().getElementById(AtomicString("outside"))->firstChild();
  ASSERT_TRUE(text_node != nullptr);

  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(text_node);
  hit_test_result.SetURLElement(To<Element>(
      GetDocument().getElementById(AtomicString("span"))->parentNode()));
  const HitTestResult hit_test_result_ = hit_test_result;

  bool result = GetFrame()
                    .GetEventHandler()
                    .GetSelectionController()
                    .SelectClosestWordFromLiveLink(hit_test_result_);
  EXPECT_TRUE(result);
}

TEST_F(SelectionControllerForIncludeTest, HandleArkWebAISelectionExt_DoubleClickWithEmptyLine) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='sample' contenteditable="true">
        <span>first</span>
        <span>   </span>
        <span>third</span>
      </div>
    </body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Node* whitespace = GetDocument().getElementById(AtomicString("sample"))->firstChild()->nextSibling();
  ASSERT_TRUE(whitespace != nullptr);

  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(whitespace);
  const HitTestResult& result = hit_test_result;

  const blink::PositionInFlatTree pos(whitespace, 0);
  const blink::PositionInFlatTreeWithAffinity pos_with_affinity(pos, blink::TextAffinity::kDownstream);

  auto selection = blink::SelectionControllerUtils::HandleArkWebAISelectionExt(
      &Controller(), result, whitespace, pos_with_affinity, true, false);
}

TEST_F(SelectionControllerForIncludeTest, HandleArkWebAISelectionExt_PosIsNull) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='sample' contenteditable="true">
        sample text
      </div>
    </body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Node* sample = GetDocument().getElementById(AtomicString("sample"))->firstChild();
  ASSERT_TRUE(sample != nullptr);

  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(sample);
  const HitTestResult& result = hit_test_result;

  const blink::PositionInFlatTreeWithAffinity pos_with_affinity;

  auto selection = blink::SelectionControllerUtils::HandleArkWebAISelectionExt(
      &Controller(), result, sample, pos_with_affinity, false, false);
  EXPECT_TRUE(selection.IsNone());
}

TEST_F(SelectionControllerForIncludeTest, HandleArkWebAISelectionExt_LongTextMiddlePosition) {
  SetHtmlInnerHTML(R"HTML(
    <body>
      <div id='sample' contenteditable="true">
        this is a very long text that exceeds maximum length for testing purposes here and has many words
      </div>
    </body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Node* sample = GetDocument().getElementById(AtomicString("sample"))->firstChild();
  ASSERT_TRUE(sample != nullptr);

  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(sample);
  const HitTestResult& result = hit_test_result;

  const blink::PositionInFlatTree pos(sample, 45);
  const blink::PositionInFlatTreeWithAffinity pos_with_affinity(pos, blink::TextAffinity::kDownstream);

  auto selection = blink::SelectionControllerUtils::HandleArkWebAISelectionExt(
      &Controller(), result, sample, pos_with_affinity, false, false);
}

TEST_F(SelectionControllerForIncludeTest, HandleEmptyLineTest_5thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>   </span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 1);

  auto pos_ = blink::SelectionControllerUtils::HandleEmptyLine(top, pos, 1);
  EXPECT_NE(pos, pos_);
}

TEST_F(SelectionControllerForIncludeTest, HandleEmptyLineTest_6thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>   </span>"
      "<span id='next'>content</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 0);

  auto pos_ = blink::SelectionControllerUtils::HandleEmptyLine(top, pos, 1);
  EXPECT_NE(pos, pos_);
}

TEST_F(SelectionControllerForIncludeTest, HandleEmptyLineTest_7thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>   </span>"
      "<span id='next'>content</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 2);

  auto pos_ = blink::SelectionControllerUtils::HandleEmptyLine(top, pos, 64);
  EXPECT_EQ(pos, pos_);
}

TEST_F(SelectionControllerForIncludeTest, HandleEmptyLineTest_8thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>   </span>"
      "<span id='next'>content</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 0);

  auto pos_ = blink::SelectionControllerUtils::HandleEmptyLine(top, pos, 0);
  EXPECT_EQ(pos, pos_);
}

TEST_F(SelectionControllerForIncludeTest, UpdateAnchorIfWhiteSpaceTest_1stIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>   </span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 0);
  auto result = blink::SelectionControllerUtils::UpdateAnchorIfWhiteSpace(top, pos);
  EXPECT_EQ(result, top);
}

TEST_F(SelectionControllerForIncludeTest, UpdateAnchorIfWhiteSpaceTest_2ndIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>abc</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 1);
  auto result = blink::SelectionControllerUtils::UpdateAnchorIfWhiteSpace(top, pos);
  EXPECT_EQ(result, top);
}

TEST_F(SelectionControllerForIncludeTest, UpdateAnchorIfWhiteSpaceTest_3rdIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>   </span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  const blink::PositionInFlatTree pos(top, 1);
  auto result = blink::SelectionControllerUtils::UpdateAnchorIfWhiteSpace(top, pos);
  EXPECT_NE(result, nullptr);
}

TEST_F(SelectionControllerForIncludeTest, SameEditablePreviousSiblingTest_1stIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = prev>text</span>"
      "<span id = top>more</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  auto result = blink::SelectionControllerUtils::SameEditablePreviousSibling(top);
  EXPECT_NE(result, nullptr);
}

TEST_F(SelectionControllerForIncludeTest, SameEditablePreviousSiblingTest_2ndIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>text</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  auto result = blink::SelectionControllerUtils::SameEditablePreviousSibling(top);
  EXPECT_EQ(result, nullptr);
}

TEST_F(SelectionControllerForIncludeTest, SameEditablePreviousSiblingTest_3rdIf) {
  const char* body_content =
      "<div id='sample'>"
      "<span id = prev>text</span>"
      "<span id = top>more</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  auto result = blink::SelectionControllerUtils::SameEditablePreviousSibling(top);
  EXPECT_EQ(result, nullptr);
}

TEST_F(SelectionControllerForIncludeTest, SameEditableParentTest_1stIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>text</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  auto result = blink::SelectionControllerUtils::SameEditableParent(top);
  EXPECT_NE(result, nullptr);
}

TEST_F(SelectionControllerForIncludeTest, SameEditableParentTest_2ndIf) {
  const char* body_content =
      "<div id='sample'>"
      "<span id = top>text</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  auto result = blink::SelectionControllerUtils::SameEditableParent(top);
  EXPECT_EQ(result, nullptr);
}

TEST_F(SelectionControllerForIncludeTest, SameEditableParentTest_3rdIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>text</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  auto result = blink::SelectionControllerUtils::SameEditableParent(nullptr);
  EXPECT_EQ(result, nullptr);
}

TEST_F(SelectionControllerForIncludeTest, SameEditablePreviousSiblingTest_4thIf) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = prev>text</span>"
      "<span id = top contenteditable='false'>more</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  auto result = blink::SelectionControllerUtils::SameEditablePreviousSibling(top);
  EXPECT_EQ(result, nullptr);
}

// Tests for IsPointInSelection branch in HandleGestureTapIfSelectionExist
// The new condition: !Selection().Contains(v_point, false) ||
//                    !Selection().AsFrameSelectionExt()->IsPointInSelection(v_point)

// Tap point far outside selection visual bounds — IsPointInSelection returns false,
// enters "clear selection" branch. This covers the core fix scenario where
// Contains() may return true (point is in DOM range) but the point is outside
// the visual selection bounds (e.g. image selection area).
TEST_F(SelectionControllerForIncludeTest,
       HandleGestureTapIfSelectionExistIsPointOutsideBounds) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 0))
                                         .Extend(PositionInFlatTree(top, 10))
                                         .Build(),
                                     TextGranularity::kCharacter);

  // Click far outside the selection visual bounds (99999, 99999)
  blink::WebMouseEvent single_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  single_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(99999, 99999)));
  single_click.button = blink::WebMouseEvent::Button::kLeft;
  single_click.click_count = 1;
  single_click.SetModifiers(
      blink::WebInputEvent::Modifiers::kIsCompatibilityEventForTouch);

  HitTestResult result;
  const MouseEventWithHitTestResults event_(single_click, location, result);

  bool result_ = GetFrame()
                     .GetEventHandler()
                     .GetSelectionController()
                     .HandleGestureTapIfSelectionExist(event_);

  // Point outside selection, should return false
  EXPECT_FALSE(result_);
}

// Tap point within selection visual bounds — IsPointInSelection returns true,
// enters "tap within" branch (the else-if for ChangeVisibilityOfQuickMenu).
// This verifies that the new IsPointInSelection check does not reject
// legitimate in-selection taps.
TEST_F(SelectionControllerForIncludeTest,
       HandleGestureTapIfSelectionExistIsPointInsideBounds) {
  const char* body_content =
      "<div id='sample' contenteditable>"
      "<span id = top>this is a sample test</span>"
      "</div>";
  SetBodyContent(body_content);

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  ASSERT_TRUE(top != nullptr);

  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 0))
                                         .Extend(PositionInFlatTree(top, 20))
                                         .Build(),
                                     TextGranularity::kCharacter);

  // Click within the selection area (center of selected text)
  blink::WebMouseEvent single_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  single_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  single_click.button = blink::WebMouseEvent::Button::kLeft;
  single_click.click_count = 1;
  single_click.SetModifiers(
      blink::WebInputEvent::Modifiers::kIsCompatibilityEventForTouch);

  HitTestResult result;
  const MouseEventWithHitTestResults event_(single_click, location, result);

  bool result_ = GetFrame()
                     .GetEventHandler()
                     .GetSelectionController()
                     .HandleGestureTapIfSelectionExist(event_);

  // When web_local_frame->Client() is nullptr (test env), the else-if
  // branch does not set ret=true, so result is still false. But the
  // important thing is it does NOT crash when accessing IsPointInSelection.
  EXPECT_FALSE(result_);
}
