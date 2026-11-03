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

#include "base/command_line.h"
#include "base/time/time.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/selection_bound.h"

#if BUILDFLAG(IS_ARKWEB)
#define private public
#include "ui/touch_selection/touch_selection_controller.h"
#include "arkweb/chromium_ext/ui/touch_selection/touch_selection_controller_ext.h"
#include "arkweb/chromium_ext/ui/touch_selection/touch_handle_ext.h"
#include "ui/touch_selection/touch_handle.h"
#undef private
#endif

namespace ui {

class MockTouchSelectionControllerClient : public TouchSelectionControllerClient {
 public:
  MockTouchSelectionControllerClient() = default;
  ~MockTouchSelectionControllerClient() override = default;

  bool SupportsAnimation() const override { return true; }
  void SetNeedsAnimate() override {}
  void MoveCaret(const gfx::PointF& position) override {}
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override {}
  void SelectBetweenCoordinates(const gfx::PointF& base,
                               const gfx::PointF& extent) override {}
  void OnSelectionEvent(SelectionEventType event) override {}
  void OnDragUpdate(const TouchSelectionDraggable::Type type,
                    const gfx::PointF& position) override {}
  std::unique_ptr<TouchHandleDrawable> CreateDrawable() override { return nullptr; }
  void DidScroll() override {}

#if BUILDFLAG(ARKWEB_MENU)
  void SelectBetweenCoordinatesV2(const gfx::PointF& position, bool is_base) override {}
  void NotifyShowMagnifier() override {}
  bool IsShowHandle() override { return true; }
#endif
};

class MockTouchHandleDrawable : public TouchHandleDrawable {
 public:
  MockTouchHandleDrawable() = default;
  ~MockTouchHandleDrawable() override = default;

  void SetEnabled(bool enabled) override {}
  void SetOrientation(TouchHandleOrientation orientation,
                      bool mirror_vertical,
                      bool mirror_horizontal) override {}
  void SetOrigin(const gfx::PointF& origin) override {}
  void SetAlpha(float alpha) override {}
  gfx::RectF GetVisibleBounds() const override { return gfx::RectF(0, 0, 10, 10); }
  float GetDrawableHorizontalPaddingRatio() const override { return 0; }

#if BUILDFLAG(ARKWEB_MENU)
  void SetEdge(const gfx::PointF& top, const gfx::PointF& bottom) override {}
#endif
};

class MockTouchHandleClient : public TouchHandleClient {
 public:
  MockTouchHandleClient() = default;
  ~MockTouchHandleClient() override = default;

  void OnDragBegin(const TouchSelectionDraggable& handler,
                   const gfx::PointF& drag_position) override {}
  void OnDragUpdate(const TouchSelectionDraggable& handler,
                    const gfx::PointF& drag_position) override {}
  void OnDragEnd(const TouchSelectionDraggable& handler) override {}
  void UpdateSelectionChanged(const TouchSelectionDraggable& draggable) override {}
  bool IsWithinTapSlop(const gfx::Vector2dF& delta) const override { return true; }
  void OnHandleTapped(const TouchHandle& handle) override {}
  void SetNeedsAnimate() override {}
  std::unique_ptr<TouchHandleDrawable> CreateDrawable() override {
    return std::make_unique<MockTouchHandleDrawable>();
  }
  base::TimeDelta GetMaxTapDuration() const override { return base::Milliseconds(200); }
  bool IsAdaptiveHandleOrientationEnabled() const override { return true; }
};

}  // namespace ui

#if BUILDFLAG(IS_ARKWEB)

namespace arkweb {

static constexpr float kDefaultDrawableSize = 10.f;
static const gfx::RectF kDefaultViewportRect(0, 0, 560, 1200);

struct Environment {
  ui::MockTouchSelectionControllerClient client;
  ui::TouchSelectionController::Config config;
  ui::TouchSelectionControllerExt* controller;
  std::shared_ptr<ui::MockTouchHandleClient> handle_client;

  Environment() : controller(nullptr) {
    base::CommandLine::Init(0, nullptr);
    handle_client = std::make_shared<ui::MockTouchHandleClient>();
    controller = new ui::TouchSelectionControllerExt(&client, config);
  }

  ~Environment() {
    delete controller;
  }

  void SetupHandles() {
    controller->insertion_handle_ = std::make_unique<ui::TouchHandleExt>(
        handle_client.get(), ui::TouchHandleOrientation::CENTER, kDefaultViewportRect);
    controller->start_selection_handle_ = std::make_unique<ui::TouchHandleExt>(
        handle_client.get(), ui::TouchHandleOrientation::LEFT, kDefaultViewportRect);
    controller->end_selection_handle_ = std::make_unique<ui::TouchHandleExt>(
        handle_client.get(), ui::TouchHandleOrientation::RIGHT, kDefaultViewportRect);
  }

  void SetHandleVisible(ui::TouchHandle* handle, bool visible) {
    if (handle) {
      handle->SetVisible(visible, ui::TouchHandle::AnimationStyle::ANIMATION_NONE);
    }
  }

  void SetupSelectionState() {
    gfx::SelectionBound start_bound, end_bound;
    start_bound.SetEdge(gfx::PointF(100, 100), gfx::PointF(100, 150));
    start_bound.set_type(gfx::SelectionBound::LEFT);
    start_bound.set_visible(true);
    end_bound.SetEdge(gfx::PointF(500, 100), gfx::PointF(500, 150));
    end_bound.set_type(gfx::SelectionBound::RIGHT);
    end_bound.set_visible(true);
    controller->OnSelectionBoundsChanged(start_bound, end_bound);
    SetupHandles();
    SetHandleVisible(controller->start_selection_handle_.get(), true);
    SetHandleVisible(controller->end_selection_handle_.get(), true);
  }

  void SetupInsertionState() {
    gfx::SelectionBound start_bound;
    start_bound.SetEdge(gfx::PointF(100, 100), gfx::PointF(100, 150));
    start_bound.set_type(gfx::SelectionBound::CENTER);
    start_bound.set_visible(true);
    controller->OnSelectionBoundsChanged(start_bound, start_bound);
    SetupHandles();
    SetHandleVisible(controller->insertion_handle_.get(), true);
  }
};

}  // namespace arkweb

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static arkweb::Environment env;
  if (!env.controller) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  int selector = fdp.ConsumeIntegralInRange<int>(0, 19);
  switch (selector) {
    case 0: {
      base::TimeTicks event_time = base::TimeTicks::Now();
      float x = fdp.ConsumeFloatingPoint<float>();
      float y = fdp.ConsumeFloatingPoint<float>();
      ui::test::MockMotionEvent event(ui::MotionEvent::Action::DOWN, event_time, x, y);
      env.controller->GetTouchNums(event);
      break;
    }
    case 1: {
      base::TimeTicks event_time = base::TimeTicks::Now();
      float x = fdp.ConsumeFloatingPoint<float>();
      float y = fdp.ConsumeFloatingPoint<float>();
      ui::test::MockMotionEvent event(ui::MotionEvent::Action::UP, event_time, x, y);
      env.controller->GetTouchNums(event);
      break;
    }
    case 2: {
      float x1 = fdp.ConsumeFloatingPoint<float>();
      float y1 = fdp.ConsumeFloatingPoint<float>();
      float x2 = fdp.ConsumeFloatingPoint<float>();
      float y2 = fdp.ConsumeFloatingPoint<float>();
      base::TimeTicks event_time = base::TimeTicks::Now();
      ui::test::MockMotionEvent event1(ui::MotionEvent::Action::DOWN, event_time, x1, y1);
      ui::test::MockMotionEvent event2(ui::MotionEvent::Action::DOWN, event_time, x2, y2);
      env.controller->GetTouchNums(event1);
      env.controller->GetTouchNums(event2);
      break;
    }
    case 3: {
      float x1 = fdp.ConsumeFloatingPoint<float>();
      float y1 = fdp.ConsumeFloatingPoint<float>();
      float x2 = fdp.ConsumeFloatingPoint<float>();
      float y2 = fdp.ConsumeFloatingPoint<float>();
      base::TimeTicks event_time = base::TimeTicks::Now();
      ui::test::MockMotionEvent event1(ui::MotionEvent::Action::DOWN, event_time, x1, y1);
      ui::test::MockMotionEvent event2(ui::MotionEvent::Action::DOWN, event_time, x2, y2);
      ui::test::MockMotionEvent event3(ui::MotionEvent::Action::DOWN, event_time, x1 + 1, y1 + 1);
      env.controller->GetTouchNums(event1);
      env.controller->GetTouchNums(event2);
      env.controller->GetTouchNums(event3);
      break;
    }
    case 4: {
      base::TimeDelta delta_time = base::Milliseconds(
          fdp.ConsumeIntegralInRange<int32_t>(0, 500));
      float dx = fdp.ConsumeFloatingPointInRange<float>(-50.0f, 50.0f);
      float dy = fdp.ConsumeFloatingPointInRange<float>(-50.0f, 50.0f);
      ui::PreTouchInfo first{static_cast<double>(dx), static_cast<double>(dy),
                              base::TimeTicks::Now() - delta_time};
      ui::PreTouchInfo second{static_cast<double>(dx + 1), static_cast<double>(dy + 1),
                               base::TimeTicks::Now()};
      bool should_process = fdp.ConsumeBool();
      env.controller->IsContinuousEvent(first, second, should_process);
      break;
    }
    case 5: {
      base::TimeDelta delta_time = base::Milliseconds(
          fdp.ConsumeIntegralInRange<int32_t>(500, 1000));
      float dx = fdp.ConsumeFloatingPointInRange<float>(-50.0f, 50.0f);
      float dy = fdp.ConsumeFloatingPointInRange<float>(-50.0f, 50.0f);
      ui::PreTouchInfo first{static_cast<double>(dx), static_cast<double>(dy),
                              base::TimeTicks::Now() - delta_time};
      ui::PreTouchInfo second{static_cast<double>(dx + 1), static_cast<double>(dy + 1),
                               base::TimeTicks::Now()};
      env.controller->IsContinuousEvent(first, second, true);
      break;
    }
    case 6: {
      base::TimeTicks event_time = base::TimeTicks::Now();
      float x = fdp.ConsumeFloatingPoint<float>();
      float y = fdp.ConsumeFloatingPoint<float>();
      ui::test::MockMotionEvent event(ui::MotionEvent::Action::DOWN, event_time, x, y);
      env.controller->SetTouchNumsForHandle(event);
      break;
    }
    case 7: {
      env.SetupInsertionState();
      gfx::SelectionBound start, end;
      start.SetEdge(gfx::PointF(100, 100), gfx::PointF(100, 150));
      start.set_type(gfx::SelectionBound::CENTER);
      end.SetEdge(gfx::PointF(100, 100), gfx::PointF(100, 150));
      end.set_type(gfx::SelectionBound::CENTER);
      env.controller->OnInsertionChangedExt(start, end);
      break;
    }
    case 8: {
      env.SetupSelectionState();
      gfx::SelectionBound start, end;
      start.SetEdge(gfx::PointF(100, 100), gfx::PointF(100, 150));
      start.set_type(gfx::SelectionBound::LEFT);
      start.set_visible(true);
      end.SetEdge(gfx::PointF(500, 100), gfx::PointF(500, 150));
      end.set_type(gfx::SelectionBound::RIGHT);
      end.set_visible(true);
      bool need_swap = fdp.ConsumeBool();
      env.controller->OnHandleSwap(need_swap, start, end);
      break;
    }
    case 9: {
      env.SetupSelectionState();
      float base_x = fdp.ConsumeFloatingPointInRange<float>(0.0f, 500.0f);
      float base_y = fdp.ConsumeFloatingPointInRange<float>(0.0f, 500.0f);
      float extent_x = fdp.ConsumeFloatingPointInRange<float>(0.0f, 500.0f);
      float extent_y = fdp.ConsumeFloatingPointInRange<float>(0.0f, 500.0f);
      env.controller->ArkSelectBetweenCoordinates(gfx::PointF(base_x, base_y),
                                                  gfx::PointF(extent_x, extent_y));
      break;
    }
    case 10: {
      env.SetupSelectionState();
      base::TimeTicks event_time = base::TimeTicks::Now();
      float x = fdp.ConsumeFloatingPointInRange<float>(0.0f, 1000.0f);
      float y = fdp.ConsumeFloatingPointInRange<float>(0.0f, 1000.0f);
      ui::test::MockMotionEvent event(ui::MotionEvent::Action::DOWN, event_time, x, y);
      env.controller->IsEndHandleNotVisible(event);
      break;
    }
    case 11: {
      env.SetupSelectionState();
      env.SetHandleVisible(env.controller->start_selection_handle_.get(), false);
      base::TimeTicks event_time = base::TimeTicks::Now();
      float x = fdp.ConsumeFloatingPointInRange<float>(0.0f, 1000.0f);
      float y = fdp.ConsumeFloatingPointInRange<float>(0.0f, 1000.0f);
      ui::test::MockMotionEvent event(ui::MotionEvent::Action::DOWN, event_time, x, y);
      env.controller->IsEndHandleNotVisible(event);
      break;
    }
    case 12: {
      env.controller->active_status_ = ui::TouchSelectionController::ActiveStatus::SELECTION_ACTIVE;
      env.controller->show_touch_handles_ = true;
      env.SetupHandles();
      env.SetHandleVisible(env.controller->start_selection_handle_.get(), true);
      env.SetHandleVisible(env.controller->end_selection_handle_.get(), true);
      bool result = env.controller->SelectOverImg();
      (void)result;
      break;
    }
    case 13: {
      env.SetupSelectionState();
      env.controller->UpdateSelectionChanged(*env.controller->start_selection_handle_);
      env.controller->UpdateSelectionChanged(*env.controller->end_selection_handle_);
      env.controller->UpdateSelectionChanged(*env.controller->insertion_handle_);
      break;
    }
    case 14: {
      env.SetupSelectionState();
      env.controller->ResetPositionAfterDragEnd(*env.controller->start_selection_handle_);
      env.controller->ResetPositionAfterDragEnd(*env.controller->end_selection_handle_);
      break;
    }
    case 15: {
      bool is_long_press = fdp.ConsumeBool();
      env.controller->SetLongPressEvent(is_long_press);
      bool result = env.controller->IsLongPressEvent();
      (void)result;
      env.controller->ResetLongPressEvent();
      break;
    }
    case 16: {
      ui::test::MockMotionEvent event(ui::MotionEvent::Action::CANCEL, base::TimeTicks::Now(), 0, 0);
      env.controller->GetTouchNums(event);
      break;
    }
    case 17: {
      env.controller->active_status_ = ui::TouchSelectionController::ActiveStatus::SELECTION_ACTIVE;
      env.controller->show_touch_handles_ = true;
      env.SetupHandles();
      env.SetHandleVisible(env.controller->start_selection_handle_.get(), true);
      env.SetHandleVisible(env.controller->end_selection_handle_.get(), true);
      env.controller->start_selection_handle_->BeginDrag();
      bool result = env.controller->SelectOverImg();
      (void)result;
      env.controller->start_selection_handle_->EndDrag();
      break;
    }
    case 18: {
      env.SetupSelectionState();
      env.controller->start_selection_handle_->BeginDrag();
      gfx::SelectionBound start, end;
      start.SetEdge(gfx::PointF(100, 100), gfx::PointF(100, 150));
      start.set_type(gfx::SelectionBound::LEFT);
      start.set_visible(true);
      end.SetEdge(gfx::PointF(500, 100), gfx::PointF(500, 150));
      end.set_type(gfx::SelectionBound::RIGHT);
      end.set_visible(true);
      env.controller->OnHandleSwap(false, start, end);
      env.controller->start_selection_handle_->EndDrag();
      break;
    }
    case 19: {
      env.SetupSelectionState();
      env.controller->start_selection_handle_->BeginDrag();
      env.controller->end_selection_handle_->BeginDrag();
      gfx::SelectionBound start, end;
      start.SetEdge(gfx::PointF(100, 100), gfx::PointF(100, 150));
      start.set_type(gfx::SelectionBound::LEFT);
      end.SetEdge(gfx::PointF(500, 100), gfx::PointF(500, 150));
      end.set_type(gfx::SelectionBound::RIGHT);
      env.controller->OnInsertionChangedExt(start, end);
      env.controller->end_selection_handle_->EndDrag();
      env.controller->start_selection_handle_->EndDrag();
      break;
    }
  }

  return 0;
}

#else

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return 0;
}

#endif