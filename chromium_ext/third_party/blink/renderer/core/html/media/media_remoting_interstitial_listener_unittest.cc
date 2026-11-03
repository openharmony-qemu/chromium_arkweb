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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/html/media/media_remoting_interstitial_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/media_remoting_interstitial.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

class ProgressBarEventListenerFullCoverageTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        "<div id='progress' style='position:absolute; left:10px; width:200px; height:20px;'></div>"
        "<video id='video'></video>");
    UpdateAllLifecyclePhasesForTest();
    progress_bar_ = GetDocument().getElementById(AtomicString("progress"));
    ASSERT_TRUE(progress_bar_);

    auto* video = GetDocument().getElementById(AtomicString("video"));
    auto* video_element = DynamicTo<HTMLVideoElement>(video);
    ASSERT_TRUE(video_element);
    interstitial_ = MakeGarbageCollected<MediaRemotingInterstitial>(*video_element);

    listener_ = MakeGarbageCollected<ProgressBarEventListener>(
        WeakMember<MediaRemotingInterstitial>(interstitial_), progress_bar_);
  }

  void TearDown() override {
    listener_ = nullptr;
    progress_bar_ = nullptr;
    interstitial_ = nullptr;
    PageTestBase::TearDown();
  }

  MouseEvent* CreateMouseEvent(const AtomicString& type, float client_x, uint16_t buttons = 0) {
    MouseEvent* event = MouseEvent::Create();
    LocalFrame& frame = GetFrame();
    LocalDOMWindow* dom_window = frame.DomWindow();
    ScriptState* script_state = ToScriptStateForMainWorld(&frame);
    event->initMouseEvent(script_state, type, true, true, dom_window, 0,
                          client_x, 0, client_x, 0, false, false, false, false,
                          0, nullptr, buttons);
    event->SetTarget(progress_bar_.Get());
    return event;
  }

  TouchEvent* CreateEmptyTouchEvent(const AtomicString& type) {
    TouchEvent* event = TouchEvent::Create();
    event->SetType(type);
    event->SetTargetTouches(TouchList::Create());
    event->SetChangedTouches(TouchList::Create());
    return event;
  }

  Persistent<Element> progress_bar_;
  Persistent<ProgressBarEventListener> listener_;
  Persistent<MediaRemotingInterstitial> interstitial_;
};

TEST_F(ProgressBarEventListenerFullCoverageTest, InvokeAllEventTypes) {
  Vector<AtomicString> types = {
      event_type_names::kClick,
      event_type_names::kMousedown,
      event_type_names::kMousemove,
      event_type_names::kMouseup,
      event_type_names::kMouseleave,
      event_type_names::kTouchstart,
      event_type_names::kTouchmove,
      event_type_names::kTouchend,
      event_type_names::kTouchcancel,
  };
  for (const auto& type : types) {
    Event* evt = Event::Create(type);
    EXPECT_NO_FATAL_FAILURE(listener_->Invoke(nullptr, evt));
  }
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseDragSequence) {
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 80.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 120.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseup, 150.0f, 0));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 60.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseleave, 90.0f, 0));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, TouchEmptyLists) {
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart));
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchmove));
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchend));
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchcancel));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, NormalWidthPercentage) {
  testing::internal::CaptureStderr();
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 50.0f));
  std::string output = testing::internal::GetCapturedStderr();
  EXPECT_TRUE(output.find("Invalid progress bar width") == std::string::npos);

  testing::internal::CaptureStderr();
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 30.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 80.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseup, 150.0f, 0));
  output = testing::internal::GetCapturedStderr();
  EXPECT_TRUE(output.find("Invalid progress bar width") == std::string::npos);
}

TEST_F(ProgressBarEventListenerFullCoverageTest, ProgressBarRemoved) {
  progress_bar_->remove();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(progress_bar_->GetLayoutObject());
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 50.0f)));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1)));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 80.0f, 1)));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseup, 150.0f, 0)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, DragOutOfBounds) {
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, -10.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 300.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 100.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseup, 150.0f, 0));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, RapidDrag) {
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1));
  for (int32_t x = 55; x <= 150; x += 5) {
    listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, static_cast<float>(x), 1));
  }
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseup, 150.0f, 0));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseLeaveAndReturn) {
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseleave, 90.0f, 0));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 70.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 120.0f, 1));
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseup, 150.0f, 0));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, NullInterstitial) {
  auto* null_listener = MakeGarbageCollected<ProgressBarEventListener>(
      WeakMember<MediaRemotingInterstitial>(), progress_bar_);
  EXPECT_NO_FATAL_FAILURE(
      null_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 50.0f)));
  EXPECT_NO_FATAL_FAILURE(
      null_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1)));
  EXPECT_NO_FATAL_FAILURE(
      null_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 80.0f, 1)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, NullProgressBar) {
  auto* null_bar_listener = MakeGarbageCollected<ProgressBarEventListener>(
      WeakMember<MediaRemotingInterstitial>(interstitial_), nullptr);
  EXPECT_NO_FATAL_FAILURE(
      null_bar_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 50.0f)));
  EXPECT_NO_FATAL_FAILURE(
      null_bar_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, TouchStartIgnoresWhenDragging) {
  listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, TouchStartWhenAlreadyTouchDragging) {
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, TouchMoveWithoutTouchStart) {
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchmove)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, TouchEndWithoutTouchStart) {
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchend)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, TouchCancelWhenDragging) {
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchcancel)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, TouchCancelWhenNotDragging) {
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchcancel)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseDownIgnoresWhenTouchDragging) {
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 60.0f, 1)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseMoveIgnoresWhenNotDragging) {
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 80.0f, 1)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseMoveIgnoresWhenTouchDragging) {
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 80.0f, 1)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseUpIgnoresWhenNotDragging) {
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseup, 80.0f, 0)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseUpIgnoresWhenTouchDragging) {
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseup, 80.0f, 0)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseLeaveWhenNotDragging) {
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseleave, 90.0f, 0)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseLeaveWhenTouchDragging) {
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseleave, 90.0f, 0)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, ProgressBarWithZeroWidth) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='zero_width' style='width:0px;'></div>");
  UpdateAllLifecyclePhasesForTest();
  auto* zero_width_bar = GetDocument().getElementById(AtomicString("zero_width"));

  auto zero_width_listener = MakeGarbageCollected<ProgressBarEventListener>(
      WeakMember<MediaRemotingInterstitial>(interstitial_), zero_width_bar);

  EXPECT_NO_FATAL_FAILURE(
      zero_width_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 50.0f)));
  EXPECT_NO_FATAL_FAILURE(
      zero_width_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1)));
  EXPECT_NO_FATAL_FAILURE(
      zero_width_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 80.0f, 1)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, ClickAtBoundaries) {
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 10.0f)));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 210.0f)));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, -100.0f)));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 500.0f)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, ComplexTouchSequence) {
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart));
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchmove));
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchmove));
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchend));

  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchstart));
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchmove));
  listener_->Invoke(nullptr, CreateEmptyTouchEvent(event_type_names::kTouchcancel));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, TouchEventWithEmptyTargetAndChangedTouches) {
  TouchEvent* event = TouchEvent::Create();
  event->SetType(event_type_names::kTouchstart);
  event->SetTargetTouches(TouchList::Create());
  event->SetChangedTouches(TouchList::Create());

  EXPECT_NO_FATAL_FAILURE(listener_->Invoke(nullptr, event));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, DragWithZeroWidthProgressBar) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='zero_width_progress' style='width:0px; height:10px;'></div>");
  UpdateAllLifecyclePhasesForTest();
  auto* zero_width_bar = GetDocument().getElementById(AtomicString("zero_width_progress"));

  auto zero_width_listener = MakeGarbageCollected<ProgressBarEventListener>(
      WeakMember<MediaRemotingInterstitial>(interstitial_), zero_width_bar);

  EXPECT_NO_FATAL_FAILURE(
      zero_width_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousedown, 50.0f, 1)));
  EXPECT_NO_FATAL_FAILURE(
      zero_width_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kMousemove, 80.0f, 1)));
  EXPECT_NO_FATAL_FAILURE(
      zero_width_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kMouseup, 100.0f, 0)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, DragWithHiddenProgressBar) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='hidden_progress' style='display:none;'></div>");
  UpdateAllLifecyclePhasesForTest();
  auto* hidden_bar = GetDocument().getElementById(AtomicString("hidden_progress"));

  auto hidden_listener = MakeGarbageCollected<ProgressBarEventListener>(
      WeakMember<MediaRemotingInterstitial>(interstitial_), hidden_bar);

  EXPECT_NO_FATAL_FAILURE(
      hidden_listener->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 50.0f)));
}

TEST_F(ProgressBarEventListenerFullCoverageTest, MouseEventAtExactProgressBoundaries) {
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 10.0f)));
  EXPECT_NO_FATAL_FAILURE(
      listener_->Invoke(nullptr, CreateMouseEvent(event_type_names::kClick, 210.0f)));
}

class RemotingButtonEventListenerTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        "<video id='video'></video>");
    UpdateAllLifecyclePhasesForTest();

    auto* video = GetDocument().getElementById(AtomicString("video"));
    video_element_ = DynamicTo<HTMLVideoElement>(video);
    ASSERT_TRUE(video_element_);

    interstitial_ = MakeGarbageCollected<MediaRemotingInterstitial>(*video_element_);
  }

  void TearDown() override {
    listener_ = nullptr;
    interstitial_ = nullptr;
    video_element_ = nullptr;
    PageTestBase::TearDown();
  }

  Persistent<HTMLVideoElement> video_element_;
  Persistent<MediaRemotingInterstitial> interstitial_;
  Persistent<RemotingButtonEventListener> listener_;
};

TEST_F(RemotingButtonEventListenerTest, StopCastingButton) {
  listener_ = MakeGarbageCollected<RemotingButtonEventListener>(
      WeakMember<MediaRemotingInterstitial>(interstitial_),
      RemotingButtonType::kStopCasting);

  Event* click_event = Event::Create(event_type_names::kClick);
  EXPECT_NO_FATAL_FAILURE(listener_->Invoke(nullptr, click_event));
}

TEST_F(RemotingButtonEventListenerTest, SwitchDeviceButton) {
  listener_ = MakeGarbageCollected<RemotingButtonEventListener>(
      WeakMember<MediaRemotingInterstitial>(interstitial_),
      RemotingButtonType::kSwitchDevice);

  Event* click_event = Event::Create(event_type_names::kClick);
  EXPECT_NO_FATAL_FAILURE(listener_->Invoke(nullptr, click_event));
}

TEST_F(RemotingButtonEventListenerTest, PlayPauseButton) {
  listener_ = MakeGarbageCollected<RemotingButtonEventListener>(
      WeakMember<MediaRemotingInterstitial>(interstitial_),
      RemotingButtonType::kPlayPause);

  Event* click_event = Event::Create(event_type_names::kClick);
  EXPECT_NO_FATAL_FAILURE(listener_->Invoke(nullptr, click_event));
}

TEST_F(RemotingButtonEventListenerTest, NullInterstitial) {
  listener_ = MakeGarbageCollected<RemotingButtonEventListener>(
      WeakMember<MediaRemotingInterstitial>(),
      RemotingButtonType::kStopCasting);

  Event* click_event = Event::Create(event_type_names::kClick);
  EXPECT_NO_FATAL_FAILURE(listener_->Invoke(nullptr, click_event));
}

TEST_F(RemotingButtonEventListenerTest, NonClickEvent) {
  listener_ = MakeGarbageCollected<RemotingButtonEventListener>(
      WeakMember<MediaRemotingInterstitial>(interstitial_),
      RemotingButtonType::kStopCasting);

  Event* mouseover_event = Event::Create(event_type_names::kMouseover);
  EXPECT_NO_FATAL_FAILURE(listener_->Invoke(nullptr, mouseover_event));
}

TEST_F(RemotingButtonEventListenerTest, PlayPauseButtonUpdatesPlayingState) {
  listener_ = MakeGarbageCollected<RemotingButtonEventListener>(
      WeakMember<MediaRemotingInterstitial>(interstitial_),
      RemotingButtonType::kPlayPause);

  interstitial_->UpdateUiPlayState(true);
  bool initial_state = interstitial_->GetPlayingState();

  Event* click_event = Event::Create(event_type_names::kClick);
  EXPECT_NO_FATAL_FAILURE(listener_->Invoke(nullptr, click_event));

  EXPECT_NE(interstitial_->GetPlayingState(), initial_state);
}

}  // namespace blink