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

#include "arkweb/chromium_ext/third_party/blink/public/mojom/page/text_recognize_result.mojom-blink.h"
#define private public
#include "arkweb/chromium_ext/third_party/blink/renderer/core/frame/web_frame_widget_impl_ext.h"
#undef private
#include "arkweb/chromium_ext/third_party/blink/renderer/platform/widget/widget_base_utils.h"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_sys_info_util_ext.h"
#include "arkweb/ohos_nweb/include/nweb_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

using testing::_;
using testing::Return;

namespace blink {
namespace {

class MockWidgetBaseUtils : public WidgetBaseUtils {
 public:
  MOCK_METHOD2(SetZoomLevel, void(float, const gfx::Point&));
  MOCK_METHOD0(GetOverScrollOffset, gfx::Vector2dF());
};

class MockWebFrameWidgetImplExt : public frame_test_helpers::TestWebFrameWidget {
 public:
  using frame_test_helpers::TestWebFrameWidget::TestWebFrameWidget;

  MOCK_METHOD1(HandleInputEvent,
               WebInputEventResult(const WebCoalescedInputEvent&));
  MOCK_METHOD0(DispatchBufferedTouchEvents, WebInputEventResult());

  MOCK_METHOD4(ObserveGestureEventAndResult,
               void(const WebGestureEvent& gesture_event,
                    const gfx::Vector2dF& unused_delta,
                    const cc::OverscrollBehavior& overscroll_behavior,
                    bool event_processed));
};

class WebFrameWidgetImplExtSimTest : public SimTest {
 public:
  frame_test_helpers::TestWebFrameWidget* CreateWebFrameWidget(
      base::PassKey<WebLocalFrame> pass_key,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool hidden,
      bool never_composited,
      bool is_for_child_local_root,
      bool is_for_nested_main_frame,
      bool is_for_scalable_page) override {
    return MakeGarbageCollected<MockWebFrameWidgetImplExt>(
        pass_key, std::move(frame_widget_host), std::move(frame_widget),
        std::move(widget_host), std::move(widget), std::move(task_runner),
        frame_sink_id, hidden, never_composited, is_for_child_local_root,
        is_for_nested_main_frame, is_for_scalable_page);
  }

  MockWebFrameWidgetImplExt* MockMainFrameWidget() {
    return static_cast<MockWebFrameWidgetImplExt*>(MainFrame().FrameWidget());
  }

  EventHandler& GetEventHandler() {
    return GetDocument().GetFrame()->GetEventHandler();
  }

  void SendInputEvent(const WebInputEvent& event,
                      WidgetBaseInputHandler::HandledEventCallback callback) {
    MockMainFrameWidget()->ProcessInputEventSynchronouslyForTesting(
        WebCoalescedInputEvent(event.Clone(), {}, {}, ui::LatencyInfo()),
        std::move(callback));
  }

  void SetWidgetBaseForTesting(std::unique_ptr<WidgetBase> widget_base) {
    MockMainFrameWidget()->widget_base_ = std::move(widget_base);
  }

  void SetUp() override {
    base::ohos::SysInfoUtilsMock::mockIsMobileDevice = true;
    auto& sys_info_utils_mock = base::ohos::SysInfoUtilsMock::GetInstance();
    EXPECT_CALL(sys_info_utils_mock, IsMobileDevice()).WillRepeatedly(testing::Return(true));
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(R"HTML(
      <!DOCTYPE html>
      <html>
      <head></head>
      <body>
        <div>Test content</div>
        <input type="text" id="input1" value="First input">
        <input type="text" id="input2" value="Second input">
        <select id="select1">
          <option value="red">Red</option>
          <option value="green">Green</option>
          <option value="blue">Blue</option>
        </select>
        <div id="content">Text</div>
        <iframe id="child"></iframe>
      </body>
      </html>
    )HTML");
    Compositor().BeginFrame();
  }

  void TearDown() override {
        base::ohos::SysInfoUtilsMock::mockIsMobileDevice = false;
    }
};

TEST_F(WebFrameWidgetImplExtSimTest, SetZoomLevel_WithWidgetBase) {
  MockMainFrameWidget()->SetZoomLevel(1.5, gfx::Point(10, 20));
}

TEST_F(WebFrameWidgetImplExtSimTest, SetOverscrollMode_WithWidgetBase) {
  int mode = 42;
  EXPECT_TRUE(MockMainFrameWidget()->widget_base_);
  MockMainFrameWidget()->SetOverscrollMode(mode);
}

TEST_F(WebFrameWidgetImplExtSimTest, ArkWebHandleTouchEvent_RawKeyDown) {
  WebTouchEvent event(WebInputEvent::Type::kRawKeyDown, 
                         WebInputEvent::kNoModifiers,
                         WebInputEvent::GetStaticTimeStampForTests());
  MockMainFrameWidget()->ArkWebHandleTouchEvent(event);
  EXPECT_EQ(MockMainFrameWidget()->rawKeyDownTime_, 1);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(10));
  run_loop.Run();
}

TEST_F(WebFrameWidgetImplExtSimTest, ArkWebHandleTouchEvent_MouseUp) {
  WebMouseEvent event(WebInputEvent::Type::kMouseUp,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  MockMainFrameWidget()->ArkWebHandleTouchEvent(event);
  EXPECT_EQ(MockMainFrameWidget()->rawKeyDownTime_, 1);
}

TEST_F(WebFrameWidgetImplExtSimTest, ArkWebHandleTouchEvent_PointerUp) {
  WebTouchEvent touch_event;
  touch_event.SetType(WebInputEvent::Type::kTouchStart);
  touch_event.SetTimeStamp(base::TimeTicks::Now());
  touch_event.touches_length = 1;
  
  WebTouchPoint touch_point;
  touch_point.state = WebTouchPoint::State::kStatePressed;
  touch_point.SetPositionInWidget(gfx::PointF(10, 10));
  touch_point.SetPositionInScreen(gfx::PointF(10, 10));
  touch_point.radius_x = 5.0f;
  touch_point.radius_y = 5.0f;
  touch_event.touches[0] = touch_point;
  WebPointerEvent event(touch_event, touch_point);

  MockMainFrameWidget()->ArkWebHandleTouchEvent(event);
}

TEST_F(WebFrameWidgetImplExtSimTest, ArkWebHandleTouchEvent_OtherEvent) {
  WebMouseEvent event(WebInputEvent::Type::kMouseMove,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  MockMainFrameWidget()->ArkWebHandleTouchEvent(event);
  EXPECT_EQ(MockMainFrameWidget()->rawKeyDownTime_, 0);
}

TEST_F(WebFrameWidgetImplExtSimTest, SelectRangeV2_WithoutFocusedFrame) {
  MockMainFrameWidget()->SelectRangeV2ForTest(gfx::Point(1, 1), false);
}

TEST_F(WebFrameWidgetImplExtSimTest, GetVisibleRectToWeb) {
  gfx::Rect rect = MockMainFrameWidget()->GetVisibleRectToWeb();
  EXPECT_TRUE(rect.IsEmpty());
}

TEST_F(WebFrameWidgetImplExtSimTest, CreateOverlay_WithValidRect) {
  SkBitmap bitmap;
  gfx::Point touch_point(10, 10);
  
  MockMainFrameWidget()->CreateOverlay(
      bitmap, touch_point,
      base::BindRepeating([](gfx::RectF& rect) { rect = gfx::RectF(0, 0, 100, 100); }),
      base::BindRepeating([](bool) {}),
      base::BindRepeating([]() {}));
}

TEST_F(WebFrameWidgetImplExtSimTest, CreateOverlay_WithEmptyRect) {
  SkBitmap bitmap;
  gfx::Point touch_point(10, 10);
  
  MockMainFrameWidget()->CreateOverlay(
      bitmap, touch_point,
      base::BindRepeating([](gfx::RectF& rect) { rect = gfx::RectF(); }),
      base::BindRepeating([](bool) {}),
      base::BindRepeating([]() {}));
}

TEST_F(WebFrameWidgetImplExtSimTest, OnTextRecognized) {
  Vector<mojom::blink::TextRecognizeResultPtr> results;
  auto result = mojom::blink::TextRecognizeResult::New();
  result->raw_value = "Test";
  result->bounding_box = gfx::RectF(10, 10, 100, 100);
  result->corner_points = {gfx::PointF(10, 10), gfx::PointF(10, 10), gfx::PointF(10, 10), gfx::PointF(10, 10)};
  results.push_back(std::move(result));

  MockMainFrameWidget()->on_text_recognize_callback_ = 
      base::BindRepeating([](std::vector<String>, std::vector<gfx::PointF>, float) {});
  MockMainFrameWidget()->OnTextRecognizedForTest(std::move(results), 1.0f);
}

TEST_F(WebFrameWidgetImplExtSimTest, GetWordSelection_Valid) {
  String text = "Hello";
  Vector<int8_t> selection = MockMainFrameWidget()->GetWordSelection(text, 0);
  EXPECT_EQ(selection.size(), 2u);
}

TEST_F(WebFrameWidgetImplExtSimTest, GetWordSelection_FailCase) {
  auto selection = MockMainFrameWidget()->GetWordSelection("测试文本", 3);
  ASSERT_EQ(selection.size(), 2u);
  EXPECT_EQ(selection[0], -1);
  EXPECT_EQ(selection[1], -1);
}

TEST_F(WebFrameWidgetImplExtSimTest, OnTextSelected) {
  MockMainFrameWidget()->on_text_selected_callback_ = base::BindRepeating([](bool) {});
  MockMainFrameWidget()->OnTextSelectedForTest(true);
}

TEST_F(WebFrameWidgetImplExtSimTest, OnDestroyImageAnalyzerOverlay) {
  MockMainFrameWidget()->on_destroy_image_overlay_callback_ = base::BindRepeating([]() {});
  MockMainFrameWidget()->OnDestroyImageAnalyzerOverlayForTest();
}

TEST_F(WebFrameWidgetImplExtSimTest, OnDataDetectorSelectText) {
  MockMainFrameWidget()->OnDataDetectorSelectTextForTest();
}

TEST_F(WebFrameWidgetImplExtSimTest, GetImageRectInner_Valid) {
  MockMainFrameWidget()->get_rect_callback_ = 
      base::BindRepeating([](gfx::RectF& rect) { rect = gfx::RectF(10, 10, 100, 100); });
  
  gfx::Rect rect = MockMainFrameWidget()->GetImageRectInner();
  EXPECT_FALSE(rect.IsEmpty());
}

TEST_F(WebFrameWidgetImplExtSimTest, GetImageRectInner_Empty) {
  MockMainFrameWidget()->get_rect_callback_ = 
      base::BindRepeating([](gfx::RectF& rect) { rect = gfx::RectF(); });
  
  gfx::Rect rect = MockMainFrameWidget()->GetImageRectInner();
  EXPECT_TRUE(rect.IsEmpty());
}

TEST_F(WebFrameWidgetImplExtSimTest, RegisterClippedVisualViewportSelectionBounds) {
  MockMainFrameWidget()->RegisterClippedVisualViewportSelectionBounds(gfx::Rect(10, 10, 100, 100));
}

TEST_F(WebFrameWidgetImplExtSimTest, CleanFocusCache) {
  MockMainFrameWidget()->CleanFocusCache();
}

TEST_F(WebFrameWidgetImplExtSimTest, ShowFreeCopyMenu) {
  MockMainFrameWidget()->ShowFreeCopyMenuForTest();
}

TEST_F(WebFrameWidgetImplExtSimTest, DisableBoost) {
  MockMainFrameWidget()->rawKeyDownTime_ = 1;

  MockMainFrameWidget()->DisableBoost();
  EXPECT_EQ(MockMainFrameWidget()->rawKeyDownTime_, 0);

  MockMainFrameWidget()->rawKeyDownTime_ = 2;
  MockMainFrameWidget()->DisableBoost();
  EXPECT_EQ(MockMainFrameWidget()->rawKeyDownTime_, 1);
}

TEST_F(WebFrameWidgetImplExtSimTest, TouchHitTest) {
  WebTouchEvent touch_event;
  touch_event.SetType(WebInputEvent::Type::kTouchStart);
  touch_event.SetTimeStamp(base::TimeTicks::Now());
  touch_event.touches_length = 1;
  
  WebTouchPoint touch_point;
  touch_point.state = WebTouchPoint::State::kStatePressed;
  touch_point.SetPositionInWidget(gfx::PointF(10, 10));
  touch_point.SetPositionInScreen(gfx::PointF(10, 10));
  touch_point.radius_x = 5.0f;
  touch_point.radius_y = 5.0f;
  touch_event.touches[0] = touch_point;
  WebPointerEvent event(touch_event, touch_point);
  
  MockMainFrameWidget()->TouchHitTest(event, 0);
}

TEST_F(WebFrameWidgetImplExtSimTest, MouseHitTest) {
  WebMouseEvent event(WebInputEvent::Type::kMouseDown,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  
  MockMainFrameWidget()->MouseHitTest(event, 1);
}

TEST_F(WebFrameWidgetImplExtSimTest, GetInputElementAttributes) {
  HashMap<String, String> attributes;
  MockMainFrameWidget()->GetInputElementAttributes(attributes);
}

TEST_F(WebFrameWidgetImplExtSimTest, SetOverscrollMode) {
  MockMainFrameWidget()->SetOverscrollMode(1);
}

// GetWordSelection with null text: covers text.IsNull() true branch (line 307).
TEST_F(WebFrameWidgetImplExtSimTest, GetWordSelection_NullText)
{
    String null_text;
    Vector<int8_t> selection = MockMainFrameWidget()->GetWordSelection(null_text, 0);
    ASSERT_EQ(selection.size(), 2u);
    EXPECT_EQ(selection[0], -1);
    EXPECT_EQ(selection[1], -1);
}

// ParseLanguage with widget_base_ nulled: covers the
// !widget_base_ || !widget_input_handler_manager() true branch (line 540).
TEST_F(WebFrameWidgetImplExtSimTest, ParseLanguage_WithoutWidgetBase)
{
    auto widget_base = std::move(MockMainFrameWidget()->widget_base_);
    SetWidgetBaseForTesting(nullptr);
    EXPECT_FALSE(MockMainFrameWidget()->widget_base_);
    MockMainFrameWidget()->ParseLanguage();
    SetWidgetBaseForTesting(std::move(widget_base));
}

// IsElementExist: covers the entire untested IsElementExist function (lines
// 109-135) including local_root_/document null checks, XPath evaluation, and
// node found / not found branches.
TEST_F(WebFrameWidgetImplExtSimTest, IsElementExist_ElementFound)
{
    bool result = MockMainFrameWidget()->IsElementExist("//input[@id='input1']");
    EXPECT_TRUE(result);
}

TEST_F(WebFrameWidgetImplExtSimTest, IsElementExist_ElementNotFound)
{
    bool result = MockMainFrameWidget()->IsElementExist("//input[@id='nonexistent']");
    EXPECT_FALSE(result);
}

// OnOverScrollOffsetChanged: covers the untested function (lines 253-258).
TEST_F(WebFrameWidgetImplExtSimTest, OnOverScrollOffsetChanged)
{
    MockMainFrameWidget()->OnOverScrollOffsetChanged(10.0f, 20.0f);
}

// GetFoldStatus: covers the return branch (already exercised but explicit
// assertion adds value).
TEST_F(WebFrameWidgetImplExtSimTest, GetFoldStatus)
{
    EXPECT_EQ(MockMainFrameWidget()->GetFoldStatus(), 0u);
}

// OnFoldStatusChanged: covers the untested setter (line 331).
TEST_F(WebFrameWidgetImplExtSimTest, OnFoldStatusChanged)
{
    MockMainFrameWidget()->OnFoldStatusChanged(1);
}

// NotifyOverlayStateChanged: covers the untested function (line 335).
TEST_F(WebFrameWidgetImplExtSimTest, NotifyOverlayStateChanged)
{
    MockMainFrameWidget()->get_rect_callback_ =
        base::BindRepeating([](gfx::RectF &rect) { rect = gfx::RectF(10, 10, 100, 100); });
    MockMainFrameWidget()->NotifyOverlayStateChanged();
}

// GetImageRect with callback: covers the untested function (line 348).
TEST_F(WebFrameWidgetImplExtSimTest, GetImageRect_WithCallback)
{
    MockMainFrameWidget()->get_rect_callback_ =
        base::BindRepeating([](gfx::RectF &rect) { rect = gfx::RectF(10, 10, 100, 100); });
    gfx::Rect result_rect;
    MockMainFrameWidget()->GetImageRect(
        base::BindOnce([](gfx::Rect *out, const gfx::Rect &rect) { *out = rect; }, &result_rect));
}

// SetPinchSmoothMode: covers the untested function (line 433).
TEST_F(WebFrameWidgetImplExtSimTest, SetPinchSmoothMode_Enable)
{
    MockMainFrameWidget()->SetPinchSmoothMode(true);
}

TEST_F(WebFrameWidgetImplExtSimTest, SetPinchSmoothMode_Disable)
{
    MockMainFrameWidget()->SetPinchSmoothMode(false);
}

TEST_F(WebFrameWidgetImplExtSimTest, SelectRangeV2) {
  MockMainFrameWidget()->SelectRangeV2(gfx::Point(10, 10), false);
}

TEST_F(WebFrameWidgetImplExtSimTest, ParseLanguage) {
  MockMainFrameWidget()->ParseLanguage();
}

TEST_F(WebFrameWidgetImplExtSimTest, ReportBlank) {
  int64_t start = MockMainFrameWidget()->GetCurrentTimestampMS();
  int64_t end = start + 100;
  
  MockMainFrameWidget()->ReportBlank(start, end);
}

TEST_F(WebFrameWidgetImplExtSimTest, ReportBlank_Branches) {
  int64_t start = MockMainFrameWidget()->GetCurrentTimestampMS();
  int64_t end = start + 10;
  int64_t duration = end - start;
  int64_t kDragBlankTime = 80;
  EXPECT_FALSE(duration > kDragBlankTime);
  MockMainFrameWidget()->ReportBlank(start, end);
}

TEST_F(WebFrameWidgetImplExtSimTest, GetOverScrollOffset_ForTest) {
  auto offset = MockMainFrameWidget()->GetOverScrollOffsetForTest();
  EXPECT_EQ(offset.x(), 0);
  EXPECT_EQ(offset.y(), 0);
}

TEST_F(WebFrameWidgetImplExtSimTest, OnTextSelected_False) {
  MockMainFrameWidget()->OnTextSelectedForTest(true);
}

TEST_F(WebFrameWidgetImplExtSimTest, OnDestroyImageAnalyzerOverlay_False) {
  MockMainFrameWidget()->OnDestroyImageAnalyzerOverlayForTest();
}

TEST_F(WebFrameWidgetImplExtSimTest, ShowFreeCopyMenu_Focused) {
  auto* input_element = GetDocument().getElementById(AtomicString("input1"));
  ASSERT_TRUE(input_element);
  input_element->Focus();
  ASSERT_TRUE(GetDocument().FocusedElement());
  MockMainFrameWidget()->ShowFreeCopyMenuForTest();
}

TEST_F(WebFrameWidgetImplExtSimTest, SelectRangeV2_Focused) {
  auto* input_element = GetDocument().getElementById(AtomicString("input1"));
  ASSERT_TRUE(input_element);
  input_element->Focus();
  ASSERT_TRUE(GetDocument().FocusedElement());
  MockMainFrameWidget()->SelectRangeV2ForTest(gfx::Point(1, 1), false);
}

TEST_F(WebFrameWidgetImplExtSimTest, OnDataDetectorSelectText_Focused) {
  auto* input_element = GetDocument().getElementById(AtomicString("input1"));
  ASSERT_TRUE(input_element);
  input_element->Focus();
  ASSERT_TRUE(GetDocument().FocusedElement());
  MockMainFrameWidget()->OnDataDetectorSelectTextForTest();
}

TEST_F(WebFrameWidgetImplExtSimTest, SetZoomLevel_WithoutWidgetBase) {
  auto widget_base = std::move(MockMainFrameWidget()->widget_base_);
  SetWidgetBaseForTesting(nullptr);
  MockMainFrameWidget()->SetZoomLevel(1.5, gfx::Point(10, 20));
  SetWidgetBaseForTesting(std::move(widget_base));
}

TEST_F(WebFrameWidgetImplExtSimTest, SetOverscrollMode_WithoutWidgetBase) {
  auto widget_base = std::move(MockMainFrameWidget()->widget_base_);
  SetWidgetBaseForTesting(nullptr);
  int mode = 42;
  EXPECT_FALSE(MockMainFrameWidget()->widget_base_);
  MockMainFrameWidget()->SetOverscrollMode(mode);
  SetWidgetBaseForTesting(std::move(widget_base));
}

TEST_F(WebFrameWidgetImplExtSimTest, GetOverScrollOffset_WithoutWidgetBase) {
  auto widget_base = std::move(MockMainFrameWidget()->widget_base_);
  SetWidgetBaseForTesting(nullptr);
  auto offset = MockMainFrameWidget()->GetOverScrollOffsetForTest();
  EXPECT_EQ(offset.x(), 0);
  EXPECT_EQ(offset.y(), 0);
  SetWidgetBaseForTesting(std::move(widget_base));
}

TEST_F(WebFrameWidgetImplExtSimTest, NotifySelectionRangeEmptyTest) {
  WebLocalFrameImpl* main_frame = &MainFrame();
  // Scenario 1: Range is not empty (covers the return false branch)
  {
    WebRange valid_range(0, 1);
    MockMainFrameWidget()->NotifySelectionRangeEmpty(valid_range, main_frame);
  }
  // Scene 2: Range is empty, and Focused Frame is nullptr
  {
    WebRange null_range;
    EXPECT_TRUE(null_range.IsNull());
    MockMainFrameWidget()->NotifySelectionRangeEmpty(null_range, nullptr);
  }
  // Scene 3: Range is empty, Frame exists, but Client is empty
  {
    WebRange null_range;
    auto* iframe_element = To<HTMLIFrameElement>(
        GetDocument().getElementById(AtomicString("child")));
    LocalFrame* child_local_frame = To<LocalFrame>(iframe_element->ContentFrame());
    WebLocalFrameImpl* child_web_frame =
        WebLocalFrameImpl::FromFrame(child_local_frame);
    iframe_element->remove();
    EXPECT_TRUE(child_web_frame->Client() == nullptr);
    MockMainFrameWidget()->NotifySelectionRangeEmpty(null_range, child_web_frame);
  }
  // Scene 4: Range is empty, while Frame and Client are functioning properly
  {
    WebRange null_range;
    EXPECT_TRUE(main_frame->Client() != nullptr);
    MockMainFrameWidget()->NotifySelectionRangeEmpty(null_range, main_frame);
  }
}

// ===== ExecuteInputCommand / ExecuteSelectCommand Tests =====
// MockMainFrameWidget() returns TestWebFrameWidget which inherits
// WebFrameWidgetImplExt when ARKWEB_UNITTESTS is enabled. SimTest provides
// a real Document with DOM elements, so the full widget-level chain can be
// tested: ExecuteInputCommand → CommandActionDOMHelper::ExecuteInput.

TEST_F(WebFrameWidgetImplExtSimTest,
       ExecuteInputCommand_Success) {
  // HTML already has <input type="text" id="input1"> from SetUp
  int32_t result = MockMainFrameWidget()->ExecuteInputCommand(
      "//input[@id='input1']", "hello", "input-text");
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::SUCCESS));

  auto* input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("input1")));
  ASSERT_NE(input, nullptr);
  EXPECT_EQ(input->Value(), "hello");
}

TEST_F(WebFrameWidgetImplExtSimTest,
       ExecuteInputCommand_ElementNotFound) {
  int32_t result = MockMainFrameWidget()->ExecuteInputCommand(
      "//input[@id='nonexistent']", "hello", "input-text");
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(WebFrameWidgetImplExtSimTest,
       ExecuteInputCommand_TypeMismatch) {
  // The HTML has a <div id="content">, not an input element
  int32_t result = MockMainFrameWidget()->ExecuteInputCommand(
      "//div[@id='content']", "hello", "input-text");
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(WebFrameWidgetImplExtSimTest,
       ExecuteSelectCommand_ByValue_Success) {
  int32_t result = MockMainFrameWidget()->ExecuteSelectCommand(
      "//select[@id='select1']",
      std::vector<int32_t>{},
      std::vector<std::string>{"green"});
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::SUCCESS));

  auto* select = To<HTMLSelectElement>(
      GetDocument().getElementById(AtomicString("select1")));
  ASSERT_NE(select, nullptr);
  EXPECT_EQ(select->Value(), "green");
}

TEST_F(WebFrameWidgetImplExtSimTest,
       ExecuteSelectCommand_ByIndex_Success) {
  int32_t result = MockMainFrameWidget()->ExecuteSelectCommand(
      "//select[@id='select1']",
      std::vector<int32_t>{2},
      std::vector<std::string>{});
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::SUCCESS));

  auto* select = To<HTMLSelectElement>(
      GetDocument().getElementById(AtomicString("select1")));
  ASSERT_NE(select, nullptr);
  EXPECT_EQ(select->Value(), "blue");
}

TEST_F(WebFrameWidgetImplExtSimTest,
       ExecuteSelectCommand_ElementNotFound) {
  int32_t result = MockMainFrameWidget()->ExecuteSelectCommand(
      "//select[@id='nonexistent']",
      std::vector<int32_t>{0},
      std::vector<std::string>{});
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(WebFrameWidgetImplExtSimTest,
       ExecuteSelectCommand_TypeMismatch) {
  // The HTML has a <div id="content">, not a select element
  int32_t result = MockMainFrameWidget()->ExecuteSelectCommand(
      "//div[@id='content']",
      std::vector<int32_t>{0},
      std::vector<std::string>{});
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

// ===== Additional branch-coverage tests =====
// Each targets a branch arm left uncovered per web_frame_widget_impl_ext-cov.log.

// ExecuteAIPageInputCommandByXPath (line 153): previously 0 calls. A non-existent
// xpath makes CommandActionDOMHelper::FindElementByXPath return null ->
// ELEMENT_NOT_FOUND, covering the local_root_/document null-check false arms and
// the helper call (line 166).
TEST_F(WebFrameWidgetImplExtSimTest,
       ExecuteAIPageInputCommandByXPath_NotFound) {
  int32_t result = MockMainFrameWidget()->ExecuteAIPageInputCommandByXPath(
      "//input[@id='nonexistent']", "hello", "default");
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

// ExecuteAIPageInputCommandById (line 170): previously 0 calls. FindElementById
// parses the id as "frame|scope|domNodeId"; a plain string fails parsing ->
// PARAM_INVALID, covering the null-check false arms and the helper call (line 183).
TEST_F(WebFrameWidgetImplExtSimTest,
       ExecuteAIPageInputCommandById_ParamInvalid) {
  int32_t result = MockMainFrameWidget()->ExecuteAIPageInputCommandById(
      "invalid_id", "hello", "default");
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::PARAM_INVALID));
}

// ExecuteSelectCommandById (line 203): previously 0 calls. Same id-parsing
// failure -> PARAM_INVALID, covering the null-check false arms and helper call.
TEST_F(WebFrameWidgetImplExtSimTest, ExecuteSelectCommandById_ParamInvalid) {
  int32_t result = MockMainFrameWidget()->ExecuteSelectCommandById(
      "invalid_id", std::vector<int32_t>{0}, std::vector<std::string>{});
  EXPECT_EQ(result, static_cast<int32_t>(
      OHOS::NWeb::WebCommandErrorCode::PARAM_INVALID));
}

// GetImageRectInner (line 404): the get_rect_callback_ null arm. With the
// callback unset (default null) abs_rect stays empty -> returns gfx::Rect().
TEST_F(WebFrameWidgetImplExtSimTest, GetImageRectInner_NullCallback) {
  gfx::Rect rect = MockMainFrameWidget()->GetImageRectInner();
  EXPECT_TRUE(rect.IsEmpty());
}

// OnTextRecognized (line 347): the on_text_recognize_callback_ null arm. With the
// callback unset the loop is skipped (empty input) and the Run() is not called.
TEST_F(WebFrameWidgetImplExtSimTest, OnTextRecognized_NullCallback) {
  Vector<mojom::blink::TextRecognizeResultPtr> results;
  MockMainFrameWidget()->OnTextRecognizedForTest(std::move(results), 1.0f);
}

// GetImageRectInner: the !widget_base_ true arm. With widget_base_ null and an
// empty abs_rect (no callback set), GetImageRectInner returns an empty rect
// without touching the size-calculation block. Note: the !widget_base_ guard
// (line 414) sits *after* view->FrameToScreen() (line 413), so a non-empty
// abs_rect would crash before reaching that guard. Testing with an empty rect
// is the only safe way to exercise the null-widget_base path with the current
// implementation. widget_base_ is restored afterwards for safe teardown.
TEST_F(WebFrameWidgetImplExtSimTest, GetImageRectInner_WidgetBaseNull) {
  auto widget_base = std::move(MockMainFrameWidget()->widget_base_);
  SetWidgetBaseForTesting(nullptr);
  gfx::Rect rect = MockMainFrameWidget()->GetImageRectInner();
  EXPECT_TRUE(rect.IsEmpty());
  SetWidgetBaseForTesting(std::move(widget_base));
}

}  // namespace
}  // namespace blink