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
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point_f.h"

#if BUILDFLAG(ARKWEB_AI)
#define private public
#include "arkweb/chromium_ext/third_party/blink/renderer/core/input/mouse_event_manager_ext.h"
#undef private
#include "third_party/blink/renderer/core/input/scroll_manager.h"
#endif

namespace blink {

static std::unique_ptr<DummyPageHolder> g_dummy_page_holder = nullptr;
#if BUILDFLAG(ARKWEB_AI)
static MouseEventManagerExt* g_mouse_event_manager_ext = nullptr;
#endif

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_dummy_page_holder = std::make_unique<DummyPageHolder>();
    if (g_dummy_page_holder) {
      LocalFrame* frame = &g_dummy_page_holder->GetFrame();
      if (frame) {
#if BUILDFLAG(ARKWEB_AI)
        auto* scroll_manager = MakeGarbageCollected<ScrollManager>(*frame);
        g_mouse_event_manager_ext = MakeGarbageCollected<MouseEventManagerExt>(*frame, *scroll_manager);
#endif
      }
    }
  }

  ~Environment() {
#if BUILDFLAG(ARKWEB_AI)
    g_mouse_event_manager_ext = nullptr;
#endif
    g_dummy_page_holder.reset();
  }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

#if BUILDFLAG(ARKWEB_AI)
static LocalFrame* GetFrame() {
  if (g_dummy_page_holder) {
    LocalFrame* frame = &g_dummy_page_holder->GetFrame();
    if (frame && frame->View()) {
      return frame;
    }
  }
  return nullptr;
}

void FuzzOnFoldStatusChanged(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  uint32_t fold_status = fdp->ConsumeIntegralInRange<uint32_t>(0, 4);
  g_mouse_event_manager_ext->OnFoldStatusChanged(fold_status);
}

void FuzzSetOverlayInProgress(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  bool flag = fdp->ConsumeBool();
  g_mouse_event_manager_ext->SetOverlayInProgress(flag);
}

void FuzzSetOverlayCreatingStatus(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  bool flag = fdp->ConsumeBool();
  g_mouse_event_manager_ext->SetOverlayCreatingStatus(flag);
}

void FuzzIsImageAnalyzerEnabled(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (frame && frame->GetSettings()) {
    frame->GetSettings()->SetImageAnalyzerEnabled(fdp->ConsumeBool());
  }
  bool result = g_mouse_event_manager_ext->IsImageAnalyzerEnabled();
  (void)result;
}

void FuzzStopCreateOverlayTimer(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  g_mouse_event_manager_ext->StopCreateOverlayTimer();
}

void FuzzOnDestroyImageAnalyzerOverlay(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  g_mouse_event_manager_ext->OnDestroyImageAnalyzerOverlay();
}

void FuzzGetAbsImageRect(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  gfx::RectF abs_rect;
  g_mouse_event_manager_ext->GetAbsImageRect(abs_rect);
  (void)abs_rect;
}

void FuzzIsValidOverlayNode(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  HitTestResult hit_test_result;
  Node* node = hit_test_result.InnerNodeOrImageMapImage();
  bool result = g_mouse_event_manager_ext->IsValidOverlayNode(node);
  (void)result;
  bool result2 = g_mouse_event_manager_ext->IsValidOverlayNode(nullptr);
  (void)result2;
}

void FuzzCloseImageOverlay(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  g_mouse_event_manager_ext->SetOverlayInProgress(false);
  g_mouse_event_manager_ext->CloseImageOverlay();
  g_mouse_event_manager_ext->SetOverlayInProgress(true);
  g_mouse_event_manager_ext->CloseImageOverlay();
}

void FuzzCreateOverlayCallback(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (frame && frame->GetSettings()) {
    frame->GetSettings()->SetImageAnalyzerEnabled(fdp->ConsumeBool());
  }
  g_mouse_event_manager_ext->CreateOverlayCallback();
}

void FuzzGetHitOverlayStatus(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  bool ignore_overlay_status = fdp->ConsumeBool();
  g_mouse_event_manager_ext->SetOverlayInProgress(true);
  g_mouse_event_manager_ext->overlay_creating_ = fdp->ConsumeBool();
  HitTestResult hit_test_result;
  HitOverlayStatus status = g_mouse_event_manager_ext->GetHitOverlayStatus(hit_test_result, ignore_overlay_status);
  (void)status;
}

void FuzzGetOverlayInProgress(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  g_mouse_event_manager_ext->SetOverlayInProgress(fdp->ConsumeBool());
  bool result = g_mouse_event_manager_ext->GetOverlayInProgress();
  (void)result;
}

void FuzzMouseEventWithHitTestResults(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame || !frame->View()) {
    return;
  }
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
  HitTestResult hit_test_result = frame->GetEventHandler().HitTestResultAtLocation(location);
  MouseEventWithHitTestResults event_with_hit_test(mouse_event, location, hit_test_result);
  g_mouse_event_manager_ext->CloseImageOverlayWhenMousePress(event_with_hit_test);
  HitOverlayStatus status = g_mouse_event_manager_ext->GetHitOverlayStatusFromMouseEvent(event_with_hit_test);
  (void)status;
}

void FuzzHandleCreateOverlayWhenDrag(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame || !frame->View()) {
    return;
  }
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
  HitTestResult hit_test_result = frame->GetEventHandler().HitTestResultAtLocation(location);
  MouseEventWithHitTestResults event_with_hit_test(mouse_event, location, hit_test_result);
  g_mouse_event_manager_ext->HandleCreateOverlayWhenDrag(event_with_hit_test);
}

void FuzzHandleGestureCreateOverlay(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (frame && frame->GetSettings()) {
    frame->GetSettings()->SetImageAnalyzerEnabled(true);
  }
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  int modifiers = fdp->ConsumeIntegral<int>();
  base::TimeTicks time_stamp = base::TimeTicks::FromInternalValue(fdp->ConsumeIntegral<int64_t>());
  WebGestureEvent gesture_event(
      WebInputEvent::Type::kGestureTap,
      modifiers,
      time_stamp);
  gesture_event.SetPositionInWidget(gfx::PointF(x, y));
  g_mouse_event_manager_ext->HandleGestureCreateOverlay(gesture_event);
}

void FuzzHandleCreateOverlayGestureEvent(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (!frame || !frame->View()) {
    return;
  }
  if (frame->GetSettings()) {
    frame->GetSettings()->SetImageAnalyzerEnabled(true);
  }
  g_mouse_event_manager_ext->SetOverlayInProgress(true);
  g_mouse_event_manager_ext->SetOverlayCreatingStatus(fdp->ConsumeBool());
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  int modifiers = fdp->ConsumeIntegral<int>();
  base::TimeTicks time_stamp = base::TimeTicks::FromInternalValue(fdp->ConsumeIntegral<int64_t>());
  WebGestureEvent gesture_event(
      WebInputEvent::Type::kGestureTap,
      modifiers,
      time_stamp);
  gesture_event.SetPositionInWidget(gfx::PointF(x, y));
  g_mouse_event_manager_ext->HandleCreateOverlay(gesture_event);
}

void FuzzStateCombinations(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  bool overlay_in_progress = fdp->ConsumeBool();
  bool overlay_creating = fdp->ConsumeBool();
  bool image_analyzer_enabled = fdp->ConsumeBool();
  uint32_t fold_status = fdp->ConsumeIntegralInRange<uint32_t>(0, 4);
  LocalFrame* frame = GetFrame();
  if (frame && frame->GetSettings()) {
    frame->GetSettings()->SetImageAnalyzerEnabled(image_analyzer_enabled);
  }
  g_mouse_event_manager_ext->SetOverlayInProgress(overlay_in_progress);
  g_mouse_event_manager_ext->SetOverlayCreatingStatus(overlay_creating);
  g_mouse_event_manager_ext->OnFoldStatusChanged(fold_status);
  g_mouse_event_manager_ext->GetOverlayInProgress();
  g_mouse_event_manager_ext->IsImageAnalyzerEnabled();
  g_mouse_event_manager_ext->StopCreateOverlayTimer();
  g_mouse_event_manager_ext->OnDestroyImageAnalyzerOverlay();
  gfx::RectF abs_rect;
  g_mouse_event_manager_ext->GetAbsImageRect(abs_rect);
  (void)abs_rect;
  g_mouse_event_manager_ext->IsValidOverlayNode(nullptr);
  HitTestResult hit_test_result;
  g_mouse_event_manager_ext->GetHitOverlayStatus(hit_test_result, fdp->ConsumeBool());
  g_mouse_event_manager_ext->CloseImageOverlay();
}

void FuzzSequenceStateSetup(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  LocalFrame* frame = GetFrame();
  if (frame && frame->GetSettings()) {
    frame->GetSettings()->SetImageAnalyzerEnabled(true);
  }
  g_mouse_event_manager_ext->SetOverlayInProgress(true);
  g_mouse_event_manager_ext->SetOverlayCreatingStatus(false);
  g_mouse_event_manager_ext->OnFoldStatusChanged(1);
  g_mouse_event_manager_ext->fold_screen_status_ = 2.0;
  for (int i = 0; i < 3; i++) {
    g_mouse_event_manager_ext->GetOverlayInProgress();
    g_mouse_event_manager_ext->IsImageAnalyzerEnabled();
  }
  g_mouse_event_manager_ext->StopCreateOverlayTimer();
  gfx::RectF abs_rect;
  g_mouse_event_manager_ext->GetAbsImageRect(abs_rect);
  (void)abs_rect;
}

void FuzzOverlayStateTransitions(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  g_mouse_event_manager_ext->SetOverlayInProgress(false);
  g_mouse_event_manager_ext->CloseImageOverlay();
  g_mouse_event_manager_ext->SetOverlayInProgress(true);
  g_mouse_event_manager_ext->CloseImageOverlay();
  g_mouse_event_manager_ext->SetOverlayCreatingStatus(false);
  g_mouse_event_manager_ext->SetOverlayCreatingStatus(true);
  g_mouse_event_manager_ext->OnDestroyImageAnalyzerOverlay();
  g_mouse_event_manager_ext->SetOverlayInProgress(true);
  g_mouse_event_manager_ext->OnDestroyImageAnalyzerOverlay();
  g_mouse_event_manager_ext->SetOverlayInProgress(false);
  g_mouse_event_manager_ext->OnDestroyImageAnalyzerOverlay();
}

void FuzzHitOverlayStatusWithPrivateState(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  g_mouse_event_manager_ext->overlay_in_progress_ = fdp->ConsumeBool();
  g_mouse_event_manager_ext->overlay_creating_ = fdp->ConsumeBool();
  g_mouse_event_manager_ext->hit_image_node_ = nullptr;
  HitTestResult hit_test_result;
  HitOverlayStatus status1 = g_mouse_event_manager_ext->GetHitOverlayStatus(hit_test_result, true);
  (void)status1;
  HitOverlayStatus status2 = g_mouse_event_manager_ext->GetHitOverlayStatus(hit_test_result, false);
  (void)status2;
}

void FuzzGetAbsImageRectWithPrivateState(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  g_mouse_event_manager_ext->hit_image_node_ = nullptr;
  gfx::RectF abs_rect1;
  g_mouse_event_manager_ext->GetAbsImageRect(abs_rect1);
  (void)abs_rect1;
  Node* dummy_node = nullptr;
  g_mouse_event_manager_ext->hit_image_node_ = dummy_node;
  gfx::RectF abs_rect2;
  g_mouse_event_manager_ext->GetAbsImageRect(abs_rect2);
  (void)abs_rect2;
}

void FuzzGetHitOverlayStatusAllBranches(FuzzedDataProvider* fdp) {
  if (!g_mouse_event_manager_ext) {
    return;
  }
  bool ignore_overlay = fdp->ConsumeBool();
  g_mouse_event_manager_ext->overlay_in_progress_ = fdp->ConsumeBool();
  g_mouse_event_manager_ext->overlay_creating_ = fdp->ConsumeBool();
  g_mouse_event_manager_ext->hit_image_node_ = nullptr;
  HitTestResult hit_test_result;
  HitOverlayStatus status1 = g_mouse_event_manager_ext->GetHitOverlayStatus(hit_test_result, ignore_overlay);
  (void)status1;
  g_mouse_event_manager_ext->overlay_in_progress_ = true;
  HitOverlayStatus status2 = g_mouse_event_manager_ext->GetHitOverlayStatus(hit_test_result, ignore_overlay);
  (void)status2;
}
#endif

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

#if BUILDFLAG(ARKWEB_AI)
  int selector = fdp.ConsumeIntegralInRange<int>(0, 21);
  switch (selector) {
    case 0:
      blink::FuzzOnFoldStatusChanged(&fdp);
      break;
    case 1:
      blink::FuzzSetOverlayInProgress(&fdp);
      break;
    case 2:
      blink::FuzzSetOverlayCreatingStatus(&fdp);
      break;
    case 3:
      blink::FuzzIsImageAnalyzerEnabled(&fdp);
      break;
    case 4:
      blink::FuzzStopCreateOverlayTimer(&fdp);
      break;
    case 5:
      blink::FuzzOnDestroyImageAnalyzerOverlay(&fdp);
      break;
    case 6:
      blink::FuzzGetAbsImageRect(&fdp);
      break;
    case 7:
      blink::FuzzIsValidOverlayNode(&fdp);
      break;
    case 8:
      blink::FuzzCloseImageOverlay(&fdp);
      break;
    case 9:
      blink::FuzzCreateOverlayCallback(&fdp);
      break;
    case 10:
      blink::FuzzGetHitOverlayStatus(&fdp);
      break;
    case 11:
      blink::FuzzGetOverlayInProgress(&fdp);
      break;
    case 12:
      blink::FuzzMouseEventWithHitTestResults(&fdp);
      break;
    case 13:
      blink::FuzzHandleCreateOverlayWhenDrag(&fdp);
      break;
    case 14:
      blink::FuzzHandleGestureCreateOverlay(&fdp);
      break;
    case 15:
      blink::FuzzHandleCreateOverlayGestureEvent(&fdp);
      break;
    case 16:
      blink::FuzzStateCombinations(&fdp);
      break;
    case 17:
      blink::FuzzSequenceStateSetup(&fdp);
      break;
    case 18:
      blink::FuzzOverlayStateTransitions(&fdp);
      break;
    case 19:
      blink::FuzzHitOverlayStatusWithPrivateState(&fdp);
      break;
    case 20:
      blink::FuzzGetAbsImageRectWithPrivateState(&fdp);
      break;
    case 21:
      blink::FuzzGetHitOverlayStatusAllBranches(&fdp);
      break;
  }
#endif

  return 0;
}