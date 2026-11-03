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

/*
 * Fuzzer for gesture_manager_utils.cc
 * 
 * Target: arkweb/chromium_ext/third_party/blink/renderer/core/input/gesture_manager_utils.cc
 * Pattern: Mock - Uses mock GestureManager to avoid complex dependencies
 */

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>

#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/dom/document.h"
// Pre-include standard headers above to shield from #define private public.
// <ranges> must be parsed before the define, otherwise lazy_split_view.h
// gets its 'private' replaced and fails to compile.
#define private public
#include "third_party/blink/renderer/core/input/gesture_manager_utils.h"
#undef private

namespace blink {
namespace {
static std::unique_ptr<DummyPageHolder> g_page_holder = nullptr;
 
struct Environment {
    Environment() {
        base::CommandLine::Init(0, nullptr);
        blink_environment_.SetUp();
        g_page_holder = std::make_unique<DummyPageHolder>();
    }
 
    ~Environment() {
        g_page_holder.reset();
    }
 
    content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

void TestCloseAIOverlay(GestureManagerUtils* utils, FuzzedDataProvider* fdp) {
  if (!utils || !fdp) return;
  
  WebGestureEvent gesture_event;
  gesture_event.SetType(WebInputEvent::Type::kGestureTap);
  gesture_event.SetModifiers(fdp->ConsumeIntegral<uint32_t>());
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  gesture_event.SetPositionInWidget(gfx::PointF(x, y));
  gesture_event.SetPositionInScreen(gfx::PointF(x, y));
  
  HitTestResult hit_test_result;
  HitTestLocation location(gfx::PointF(x, y));
  GestureEventWithHitTestResults targeted_event(gesture_event, location, hit_test_result);
  
  utils->CloseAIOverlay(targeted_event);
}

void TestHandleGestureDragLongPress(GestureManagerUtils* utils, FuzzedDataProvider* fdp) {
  if (!utils || !fdp) return;
  
  WebGestureEvent gesture_event;
  gesture_event.SetType(WebInputEvent::Type::kGestureLongPress);
  gesture_event.SetModifiers(fdp->ConsumeIntegral<uint32_t>());
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  gesture_event.SetPositionInWidget(gfx::PointF(x, y));
  gesture_event.SetPositionInScreen(gfx::PointF(x, y));
  gesture_event.data.long_press.width = fdp->ConsumeFloatingPoint<float>();
  gesture_event.data.long_press.height = fdp->ConsumeFloatingPoint<float>();
  
  HitTestResult hit_test_result;
  HitTestLocation location(gfx::PointF(x, y));
  GestureEventWithHitTestResults targeted_event(gesture_event, location, hit_test_result);
  
  utils->HandleGestureDragLongPress(targeted_event);
}

void TestHandleGestureCreateOverlay(GestureManagerUtils* utils, FuzzedDataProvider* fdp) {
  if (!utils || !fdp) return;
  
  WebGestureEvent gesture_event;
  gesture_event.SetType(WebInputEvent::Type::kGestureTap);
  gesture_event.SetModifiers(fdp->ConsumeIntegral<uint32_t>());
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  gesture_event.SetPositionInWidget(gfx::PointF(x, y));
  gesture_event.SetPositionInScreen(gfx::PointF(x, y));
  
  HitTestResult hit_test_result;
  HitTestLocation location(gfx::PointF(x, y));
  GestureEventWithHitTestResults targeted_event(gesture_event, location, hit_test_result);
  
  utils->HandleGestureCreateOverlay(targeted_event);
}

void TestUpdateContextMenuForAI(GestureManagerUtils* utils, FuzzedDataProvider* fdp) {
  if (!utils || !fdp) return;
  
  WebGestureEvent gesture_event;
  gesture_event.SetType(WebInputEvent::Type::kGestureLongPress);
  gesture_event.SetModifiers(fdp->ConsumeIntegral<uint32_t>());
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  gesture_event.SetPositionInWidget(gfx::PointF(x, y));
  gesture_event.SetPositionInScreen(gfx::PointF(x, y));
  
  HitTestResult hit_test_result;
  HitTestLocation location(gfx::PointF(x, y));
  
  utils->UpdateContextMenuForAI(hit_test_result, location, gesture_event);
}

void TestUpdateContextMenuForFreeCopy(GestureManagerUtils* utils, FuzzedDataProvider* fdp) {
  if (!utils || !fdp) return;
  
  float x = fdp->ConsumeFloatingPoint<float>();
  float y = fdp->ConsumeFloatingPoint<float>();
  
  HitTestResult hit_test_result;
  HitTestLocation location(gfx::PointF(x, y));
  
  utils->UpdateContextMenuForFreeCopy(hit_test_result, location);
}

enum class TestScenario {
  CLOSE_AI_OVERLAY = 0,
  HANDLE_GESTURE_DRAG_LONG_PRESS = 1,
  HANDLE_GESTURE_CREATE_OVERLAY = 2,
  UPDATE_CONTEXT_MENU_FOR_AI = 3,
  UPDATE_CONTEXT_MENU_FOR_FREE_COPY = 4,
  MAX_SCENARIO
};

void RunFuzzer(FuzzedDataProvider* fdp, GestureManagerUtils* utils) {
  if (!utils || !fdp) return;
  
  auto scenario = fdp->ConsumeIntegralInRange<uint8_t>(
      static_cast<uint8_t>(TestScenario::CLOSE_AI_OVERLAY),
      static_cast<uint8_t>(TestScenario::MAX_SCENARIO) - 1);
  
  switch (scenario) {
    case static_cast<uint8_t>(TestScenario::CLOSE_AI_OVERLAY):
      TestCloseAIOverlay(utils, fdp);
      break;
    case static_cast<uint8_t>(TestScenario::HANDLE_GESTURE_DRAG_LONG_PRESS):
      TestHandleGestureDragLongPress(utils, fdp);
      break;
    case static_cast<uint8_t>(TestScenario::HANDLE_GESTURE_CREATE_OVERLAY):
      TestHandleGestureCreateOverlay(utils, fdp);
      break;
    case static_cast<uint8_t>(TestScenario::UPDATE_CONTEXT_MENU_FOR_AI):
      TestUpdateContextMenuForAI(utils, fdp);
      break;
    case static_cast<uint8_t>(TestScenario::UPDATE_CONTEXT_MENU_FOR_FREE_COPY):
      TestUpdateContextMenuForFreeCopy(utils, fdp);
      break;
    default:
      TestHandleGestureCreateOverlay(utils, fdp);
      break;
  }
}

}  // namespace
}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < 1) {
    return 0;
  }
  static blink::Environment env;
  blink::LocalDOMWindow* dom_window = nullptr;
  blink::LocalFrame* frame = nullptr;
  blink::EventHandler* event_handler = nullptr;
  blink::GestureManager* gesture_manager = nullptr;
  if (blink::g_page_holder) {
    dom_window = blink::g_page_holder->GetFrame().DomWindow();
    frame = &blink::g_page_holder->GetFrame();
    event_handler = &frame->GetEventHandler();
    gesture_manager = &event_handler->GetGestureManager();
  }
  FuzzedDataProvider fdp(data, size);
  blink::GestureManagerUtils gesture_manager_utils(gesture_manager);
  blink::RunFuzzer(&fdp, &gesture_manager_utils);
  return 0;
}
