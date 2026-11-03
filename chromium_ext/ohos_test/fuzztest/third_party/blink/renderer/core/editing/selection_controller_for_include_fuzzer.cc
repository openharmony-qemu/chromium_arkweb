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
#include <vector>

#include "base/command_line.h"
#include "base/time/time.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#define private public
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#undef private
#include "arkweb/build/features/features.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/editing/selection_controller_for_include.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

static std::unique_ptr<DummyPageHolder> g_page_holder = nullptr;
static SelectionController* g_selection_controller = nullptr;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>();
    if (g_page_holder) {
      LocalFrame* frame = &g_page_holder->GetFrame();
      if (frame) {
        g_selection_controller = MakeGarbageCollected<SelectionController>(*frame);
      }
    }
  }

  ~Environment() {
    g_selection_controller = nullptr;
    g_page_holder.reset();
  }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

void FuzzMaxOffsetTrimTailWhiteSpace(FuzzedDataProvider* fdp) {
  unsigned len = fdp->ConsumeIntegralInRange<unsigned>(0, 200);
  std::string str_bytes = fdp->ConsumeBytesAsString(len);
  String str = String::FromUTF8(str_bytes);
  unsigned result = SelectionControllerUtils::MaxOffsetTrimTailWhiteSpace(str, len);
  (void)result;
  unsigned result2 = SelectionControllerUtils::MaxOffsetTrimTailWhiteSpace(str, 0);
  (void)result2;
  
  const char* test_strs[] = {
    "hello world  ",
    "no trailing  ",
    "   leading",
    "both ends   ",
    "",
    "single",
  };
  for (auto* test_str : test_strs) {
    String s = String::FromUTF8(test_str);
    unsigned r = SelectionControllerUtils::MaxOffsetTrimTailWhiteSpace(s, s.length());
    (void)r;
  }
}

void FuzzOffsetAdjustWhiteSpace(FuzzedDataProvider* fdp) {
  int offset = fdp->ConsumeIntegral<int>();
  int temp_offset = fdp->ConsumeIntegral<int>();
  unsigned len = fdp->ConsumeIntegralInRange<unsigned>(0, 200);
  std::string str_bytes = fdp->ConsumeBytesAsString(len);
  String str = String::FromUTF8(str_bytes);
  bool permission = fdp->ConsumeBool();
  SelectionControllerUtils::OffsetAdjustWhiteSpace(offset, temp_offset, str, permission);
  (void)offset;
  (void)temp_offset;
  int offset2 = 10;
  int temp_offset2 = 10;
  SelectionControllerUtils::OffsetAdjustWhiteSpace(offset2, temp_offset2, str, true);
  (void)offset2;
  (void)temp_offset2;
  
  String test_str = String::FromUTF8("hello world test");
  int off = 5;
  int temp_off = 5;
  SelectionControllerUtils::OffsetAdjustWhiteSpace(off, temp_off, test_str, true);
  
  String test_str2 = String::FromUTF8("   text with leading spaces");
  int off3 = 3;
  int temp_off3 = 3;
  SelectionControllerUtils::OffsetAdjustWhiteSpace(off3, temp_off3, test_str2, true);
}

void FuzzIsValidAISelection(FuzzedDataProvider* fdp) {
  Vector<int8_t> select;
  select.push_back(fdp->ConsumeIntegral<int8_t>());
  select.push_back(fdp->ConsumeIntegral<int8_t>());
  bool result = SelectionControllerUtils::IsValidAISelection(select);
  (void)result;
  Vector<int8_t> select_valid;
  select_valid.push_back(0);
  select_valid.push_back(5);
  bool result2 = SelectionControllerUtils::IsValidAISelection(select_valid);
  (void)result2;
  Vector<int8_t> select_same;
  select_same.push_back(5);
  select_same.push_back(5);
  bool result3 = SelectionControllerUtils::IsValidAISelection(select_same);
  (void)result3;
  Vector<int8_t> select_neg1;
  select_neg1.push_back(-1);
  select_neg1.push_back(5);
  bool result4 = SelectionControllerUtils::IsValidAISelection(select_neg1);
  (void)result4;
  Vector<int8_t> select_neg2;
  select_neg2.push_back(3);
  select_neg2.push_back(-1);
  bool result5 = SelectionControllerUtils::IsValidAISelection(select_neg2);
  (void)result5;
}

void FuzzSameEditablePreviousSibling(Document& doc, FuzzedDataProvider* fdp) {
  Node* body = doc.body();
  if (!body) {
    return;
  }
  Node* result = SelectionControllerUtils::SameEditablePreviousSibling(body);
  (void)result;
  Node* result2 = SelectionControllerUtils::SameEditablePreviousSibling(nullptr);
  (void)result2;
  
  Node* first_child = body->firstChild();
  if (first_child) {
    Node* prev = SelectionControllerUtils::SameEditablePreviousSibling(first_child);
    (void)prev;
  }
  
  Node* last_child = body->lastChild();
  if (last_child) {
    Node* prev2 = SelectionControllerUtils::SameEditablePreviousSibling(last_child);
    (void)prev2;
  }
}

void FuzzSameEditableParent(Document& doc, FuzzedDataProvider* fdp) {
  Node* body = doc.body();
  if (!body) {
    return;
  }
  Node* result = SelectionControllerUtils::SameEditableParent(body);
  (void)result;
  Node* result2 = SelectionControllerUtils::SameEditableParent(nullptr);
  (void)result2;
  
  Node* first_child = body->firstChild();
  if (first_child) {
    Node* parent = SelectionControllerUtils::SameEditableParent(first_child);
    (void)parent;
  }
}

void FuzzHandleEmptyLine(Document& doc, FuzzedDataProvider* fdp) {
  Node* body = doc.body();
  int depth = fdp->ConsumeIntegralInRange<int>(0, 64);
  PositionInFlatTree pos;
  PositionInFlatTree result = SelectionControllerUtils::HandleEmptyLine(body, pos, depth);
  (void)result;
  PositionInFlatTree result2 = SelectionControllerUtils::HandleEmptyLine(nullptr, pos, depth);
  (void)result2;
  PositionInFlatTree result3 = SelectionControllerUtils::HandleEmptyLine(body, PositionInFlatTree(), depth);
  (void)result3;
  if (body) {
    PositionInFlatTree valid_pos = PositionInFlatTree::CreateWithoutValidation(*body, 0);
    PositionInFlatTree result4 = SelectionControllerUtils::HandleEmptyLine(body, valid_pos, depth);
    (void)result4;
    
    PositionInFlatTree result5 = SelectionControllerUtils::HandleEmptyLine(body, valid_pos, 0);
    (void)result5;
    
    PositionInFlatTree result6 = SelectionControllerUtils::HandleEmptyLine(body, valid_pos, 1);
    (void)result6;
  }
}

void FuzzUpdateAnchorIfWhiteSpace(Document& doc, FuzzedDataProvider* fdp) {
  Node* body = doc.body();
  PositionInFlatTree pos;
  if (body) {
    pos = PositionInFlatTree::CreateWithoutValidation(*body, 0);
  }
  Node* result = SelectionControllerUtils::UpdateAnchorIfWhiteSpace(body, pos);
  (void)result;
  Node* result2 = SelectionControllerUtils::UpdateAnchorIfWhiteSpace(nullptr, pos);
  (void)result2;
  if (body) {
    PositionInFlatTree valid_pos = PositionInFlatTree::CreateWithoutValidation(*body, 0);
    Node* result3 = SelectionControllerUtils::UpdateAnchorIfWhiteSpace(body, valid_pos);
    (void)result3;
  }
  
  Node* null_anchor_pos = PositionInFlatTree().AnchorNode();
  (void)null_anchor_pos;
}

void FuzzHitTestResultBasic(Document& doc, FuzzedDataProvider* fdp) {
  HitTestResult result;
  (void)result;
  HitTestResult copy_result(result);
  (void)copy_result;
  bool is_live_link = result.IsLiveLink();
  (void)is_live_link;
  Node* inner_node = result.InnerNode();
  (void)inner_node;
  Element* url_element = result.URLElement();
  (void)url_element;
  HitTestResult result2(result);
  (void)result2;
  result.CacheValues(result2);
  (void)result2.EqualForCacheability(result);
  
  HitTestResult empty_result;
  bool is_live2 = empty_result.IsLiveLink();
  (void)is_live2;
}

void FuzzMouseEventWithHitTestResults(Document& doc, FuzzedDataProvider* fdp) {
  int modifiers = fdp->ConsumeIntegral<int>();
  int click_count = fdp->ConsumeIntegralInRange<int>(0, 5);
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  base::TimeTicks time_stamp = base::TimeTicks::FromInternalValue(fdp->ConsumeIntegral<int64_t>());
  WebMouseEvent mouse_event(
      WebInputEvent::Type::kMouseDown,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      click_count,
      modifiers,
      time_stamp);
  PhysicalOffset offset(gfx::Point(fdp->ConsumeIntegral<int>(), fdp->ConsumeIntegral<int>()));
  HitTestLocation location(offset);
  HitTestResult hit_test_result;
  MouseEventWithHitTestResults event_with_hit_test(mouse_event, location, hit_test_result);
  (void)event_with_hit_test;
  bool is_over_link = event_with_hit_test.IsOverLink();
  (void)is_over_link;
  Node* inner_node = event_with_hit_test.InnerNode();
  (void)inner_node;
  Element* inner_element = event_with_hit_test.InnerElement();
  (void)inner_element;
  bool is_over_content = event_with_hit_test.IsOverEmbeddedContentView();
  (void)is_over_content;
  
  WebMouseEvent mouse_up(
      WebInputEvent::Type::kMouseUp,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      click_count,
      modifiers,
      time_stamp);
  MouseEventWithHitTestResults event_up(mouse_up, location, hit_test_result);
  (void)event_up;
  
  WebMouseEvent mouse_dblclick(
      WebInputEvent::Type::kMouseDown,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      2,
      modifiers,
      time_stamp);
  MouseEventWithHitTestResults event_dbl(mouse_dblclick, location, hit_test_result);
  (void)event_dbl;
}

void FuzzSelectClosestWordFromLiveLink(Document& doc, FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  HitTestResult hit_test_result;
  HitTestResult cached_result = hit_test_result;
  cached_result.CacheValues(cached_result);
  bool result = g_selection_controller->SelectClosestWordFromLiveLink(hit_test_result);
  (void)result;
  
  HitTestResult empty_result2;
  bool result2 = g_selection_controller->SelectClosestWordFromLiveLink(empty_result2);
  (void)result2;
  
  HitTestResult null_result;
  bool result3 = g_selection_controller->SelectClosestWordFromLiveLink(null_result);
  (void)result3;
}

void FuzzSetLastLongPressHitTestResult(FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  HitTestResult hit_test_result;
  g_selection_controller->SetLastLongPressHitTestResult(hit_test_result);
  
  HitTestResult empty;
  g_selection_controller->SetLastLongPressHitTestResult(empty);
}

void FuzzShowSelectionByLastLongPressHitTestResult(Document& doc, FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  HitTestResult hit_test_result;
  g_selection_controller->SetLastLongPressHitTestResult(hit_test_result);
  bool result = g_selection_controller->ShowSelectionByLastLongPressHitTestResult();
  (void)result;
  HitTestResult null_result;
  g_selection_controller->SetLastLongPressHitTestResult(null_result);
  bool result2 = g_selection_controller->ShowSelectionByLastLongPressHitTestResult();
  (void)result2;
}

void FuzzNotifyContextMenuWillShow(FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  g_selection_controller->NotifyContextMenuWillShow();
}

void FuzzFocusDocumentView(FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  g_selection_controller->FocusDocumentView();
}

void FuzzMouseSelectMenuShow(FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  bool show = fdp->ConsumeBool();
  bool result = g_selection_controller->MouseSelectMenuShow(show);
  (void)result;
  bool result2 = g_selection_controller->MouseSelectMenuShow(!show);
  (void)result2;
  bool result3 = g_selection_controller->MouseSelectMenuShow(true);
  (void)result3;
  bool result4 = g_selection_controller->MouseSelectMenuShow(false);
  (void)result4;
}

void FuzzSetDataDetectorHitTest(Document& doc, FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  int modifiers = fdp->ConsumeIntegral<int>();
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  base::TimeTicks time_stamp = base::TimeTicks::FromInternalValue(fdp->ConsumeIntegral<int64_t>());
  WebMouseEvent mouse_event(
      WebInputEvent::Type::kMouseDown,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      0,
      modifiers,
      time_stamp);
  PhysicalOffset offset(gfx::Point(fdp->ConsumeIntegral<int>(), fdp->ConsumeIntegral<int>()));
  HitTestLocation location(offset);
  HitTestResult hit_test_result;
  MouseEventWithHitTestResults event_with_hit_test(mouse_event, location, hit_test_result);
  g_selection_controller->SetDataDetectorHitTest(event_with_hit_test);
  g_selection_controller->SetDataDetectorHitTest(event_with_hit_test);
}

void FuzzShowSelectionByLastLinkHitTestResult(FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  bool result = g_selection_controller->ShowSelectionByLastLinkHitTestResult();
  (void)result;
}

void FuzzHandleArkWebAISelectionExt(Document& doc, FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  Node* body = doc.body();
  if (!body) {
    return;
  }
  HitTestResult hit_test_result;
  PositionInFlatTree pos = PositionInFlatTree::CreateWithoutValidation(*body, 0);
  PositionInFlatTreeWithAffinity pos_with_affinity(pos, TextAffinity::kDownstream);
  bool is_double_click = fdp->ConsumeBool();
  bool layout_change = fdp->ConsumeBool();
  SelectionInFlatTree result = SelectionControllerUtils::HandleArkWebAISelectionExt(
      g_selection_controller, hit_test_result, body, pos_with_affinity, is_double_click, layout_change);
  (void)result;
  SelectionInFlatTree result2 = SelectionControllerUtils::HandleArkWebAISelectionExt(
      g_selection_controller, hit_test_result, nullptr, pos_with_affinity, is_double_click, layout_change);
  (void)result2;
  PositionInFlatTree empty_pos;
  PositionInFlatTreeWithAffinity empty_pos_with_affinity(empty_pos, TextAffinity::kUpstream);
  SelectionInFlatTree result3 = SelectionControllerUtils::HandleArkWebAISelectionExt(
      g_selection_controller, hit_test_result, body, empty_pos_with_affinity, is_double_click, layout_change);
  (void)result3;
  SelectionInFlatTree result4 = SelectionControllerUtils::HandleArkWebAISelectionExt(
      g_selection_controller, hit_test_result, body, pos_with_affinity, true, layout_change);
  (void)result4;
  SelectionInFlatTree result5 = SelectionControllerUtils::HandleArkWebAISelectionExt(
      g_selection_controller, hit_test_result, body, pos_with_affinity, false, layout_change);
  (void)result5;
  
  PositionInFlatTree pos2 = PositionInFlatTree::CreateWithoutValidation(*body, 5);
  PositionInFlatTreeWithAffinity pos_with_affinity2(pos2, TextAffinity::kUpstream);
  SelectionInFlatTree result6 = SelectionControllerUtils::HandleArkWebAISelectionExt(
      g_selection_controller, hit_test_result, body, pos_with_affinity2, true, layout_change);
  (void)result6;
  
  SelectionInFlatTree result7 = SelectionControllerUtils::HandleArkWebAISelectionExt(
      g_selection_controller, hit_test_result, body, pos_with_affinity2, false, false);
  (void)result7;
}

void FuzzHandleMouseReleaseEventWithAIExt(Document& doc, FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  int modifiers = fdp->ConsumeIntegral<int>();
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  base::TimeTicks time_stamp = base::TimeTicks::FromInternalValue(fdp->ConsumeIntegral<int64_t>());
  WebMouseEvent mouse_event(
      WebInputEvent::Type::kMouseUp,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      0,
      modifiers,
      time_stamp);
  PhysicalOffset offset(gfx::Point(fdp->ConsumeIntegral<int>(), fdp->ConsumeIntegral<int>()));
  HitTestLocation location(offset);
  HitTestResult hit_test_result;
  MouseEventWithHitTestResults event_with_hit_test(mouse_event, location, hit_test_result);
  SelectionControllerUtils::HandleMouseReleaseEventWithAIExt(g_selection_controller, event_with_hit_test);
  
  WebMouseEvent mouse_down(
      WebInputEvent::Type::kMouseDown,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      1,
      modifiers,
      time_stamp);
  MouseEventWithHitTestResults event_down(mouse_down, location, hit_test_result);
  SelectionControllerUtils::HandleMouseReleaseEventWithAIExt(g_selection_controller, event_down);
  
  WebMouseEvent mouse_up2(
      WebInputEvent::Type::kMouseUp,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      1,
      modifiers,
      time_stamp);
  MouseEventWithHitTestResults event_up2(mouse_up2, location, hit_test_result);
  SelectionControllerUtils::HandleMouseReleaseEventWithAIExt(g_selection_controller, event_up2);
}

void FuzzHandleMouseReleaseEventWithMenuExt(Document& doc, FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  int modifiers = fdp->ConsumeIntegral<int>();
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  base::TimeTicks time_stamp = base::TimeTicks::FromInternalValue(fdp->ConsumeIntegral<int64_t>());
  WebMouseEvent mouse_event(
      WebInputEvent::Type::kMouseUp,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      1,
      modifiers,
      time_stamp);
  PhysicalOffset offset(gfx::Point(fdp->ConsumeIntegral<int>(), fdp->ConsumeIntegral<int>()));
  HitTestLocation location(offset);
  HitTestResult hit_test_result;
  MouseEventWithHitTestResults event_with_hit_test(mouse_event, location, hit_test_result);
  
  g_selection_controller->mouse_down_was_single_click_in_selection_ = false;
  g_selection_controller->mouse_down_may_start_select_ = false;
  g_selection_controller->mouse_click_down_allows_ = false;
  g_selection_controller->mouse_menu_show_ = false;
  SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(g_selection_controller, event_with_hit_test);
  
  g_selection_controller->mouse_down_was_single_click_in_selection_ = true;
  g_selection_controller->mouse_down_may_start_select_ = true;
  g_selection_controller->mouse_click_down_allows_ = true;
  g_selection_controller->mouse_menu_show_ = true;
  SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(g_selection_controller, event_with_hit_test);
  
  g_selection_controller->mouse_down_was_single_click_in_selection_ = true;
  g_selection_controller->mouse_down_may_start_select_ = true;
  g_selection_controller->mouse_click_down_allows_ = false;
  g_selection_controller->mouse_menu_show_ = false;
  SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(g_selection_controller, event_with_hit_test);
  
  g_selection_controller->mouse_down_was_single_click_in_selection_ = false;
  g_selection_controller->mouse_down_may_start_select_ = true;
  g_selection_controller->mouse_click_down_allows_ = true;
  g_selection_controller->mouse_menu_show_ = true;
  SelectionControllerUtils::HandleMouseReleaseEventWithMenuExt(g_selection_controller, event_with_hit_test);
}

void FuzzHandleGestureTapIfSelectionExist(Document& doc, FuzzedDataProvider* fdp) {
  if (!g_selection_controller) {
    return;
  }
  int modifiers = fdp->ConsumeIntegral<int>();
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  int64_t time_value = fdp->ConsumeIntegral<int64_t>();
  base::TimeTicks time_stamp = base::TimeTicks::FromInternalValue(time_value);
  WebMouseEvent mouse_event(
      WebInputEvent::Type::kMouseDown,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      1,
      modifiers,
      time_stamp);
  PhysicalOffset offset(gfx::Point(fdp->ConsumeIntegral<int>(), fdp->ConsumeIntegral<int>()));
  HitTestLocation location(offset);
  HitTestResult hit_test_result;
  MouseEventWithHitTestResults event_with_hit_test(mouse_event, location, hit_test_result);
  bool result = g_selection_controller->HandleGestureTapIfSelectionExist(event_with_hit_test);
  (void)result;
  
  WebMouseEvent mouse_down2(
      WebInputEvent::Type::kMouseDown,
      gfx::PointF(x, y),
      gfx::PointF(x, y),
      WebMouseEvent::Button::kNoButton,
      2,
      modifiers,
      time_stamp);
  MouseEventWithHitTestResults event_down2(mouse_down2, location, hit_test_result);
  bool result3 = g_selection_controller->HandleGestureTapIfSelectionExist(event_down2);
  (void)result3;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  blink::Document& doc = *blink::g_page_holder->GetFrame().GetDocument();

  int selector = fdp.ConsumeIntegralInRange<int>(0, 20);
  switch (selector) {
    case 0:
      blink::FuzzMaxOffsetTrimTailWhiteSpace(&fdp);
      break;
    case 1:
      blink::FuzzOffsetAdjustWhiteSpace(&fdp);
      break;
    case 2:
      blink::FuzzIsValidAISelection(&fdp);
      break;
    case 3:
      blink::FuzzSameEditablePreviousSibling(doc, &fdp);
      break;
    case 4:
      blink::FuzzSameEditableParent(doc, &fdp);
      break;
    case 5:
      blink::FuzzHandleEmptyLine(doc, &fdp);
      break;
    case 6:
      blink::FuzzUpdateAnchorIfWhiteSpace(doc, &fdp);
      break;
    case 7:
      blink::FuzzHitTestResultBasic(doc, &fdp);
      break;
    case 8:
      blink::FuzzMouseEventWithHitTestResults(doc, &fdp);
      break;
    case 9:
      blink::FuzzSelectClosestWordFromLiveLink(doc, &fdp);
      break;
    case 10:
      blink::FuzzSetLastLongPressHitTestResult(&fdp);
      break;
    case 11:
      blink::FuzzShowSelectionByLastLongPressHitTestResult(doc, &fdp);
      break;
    case 12:
      blink::FuzzNotifyContextMenuWillShow(&fdp);
      break;
    case 13:
      blink::FuzzFocusDocumentView(&fdp);
      break;
    case 14:
      blink::FuzzMouseSelectMenuShow(&fdp);
      break;
    case 15:
      blink::FuzzSetDataDetectorHitTest(doc, &fdp);
      break;
    case 16:
      blink::FuzzShowSelectionByLastLinkHitTestResult(&fdp);
      break;
    case 17:
      blink::FuzzHandleArkWebAISelectionExt(doc, &fdp);
      break;
    case 18:
      blink::FuzzHandleMouseReleaseEventWithAIExt(doc, &fdp);
      break;
    case 19:
      blink::FuzzHandleMouseReleaseEventWithMenuExt(doc, &fdp);
      break;
    case 20:
      blink::FuzzHandleGestureTapIfSelectionExist(doc, &fdp);
      break;
  }

  return 0;
}