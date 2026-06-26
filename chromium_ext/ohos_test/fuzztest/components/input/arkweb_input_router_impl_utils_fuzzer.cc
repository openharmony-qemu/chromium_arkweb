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

#include <cfloat>
#include <fuzzer/FuzzedDataProvider.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/input/fling_controller.h"
#include "components/input/input_disposition_handler.h"
#include "components/input/input_router_impl.h"
#include "components/input/input_router_client.h"
#include "ui/base/mojom/ui_base_types.mojom.h"

#define private public
#include "arkweb/chromium_ext/components/input/arkweb_input_router_impl_utils.h"
#undef private

namespace {

// Mock WidgetInputHandler - provides minimal implementation
class MockWidgetInputHandler : public blink::mojom::WidgetInputHandler {
public:
    MockWidgetInputHandler() = default;
    ~MockWidgetInputHandler() override = default;

    // Required virtual methods - minimal stubs
    void SetFocus(blink::mojom::FocusState focus_state) override {}
    void MouseCaptureLost() override {}
    void SetEditCommandsForNextKeyEvent(std::vector<blink::mojom::EditCommandPtr> commands) override {}
    void CursorVisibilityChanged(bool visible) override {}
    void ImeSetComposition(const std::u16string& text,
                          const std::vector<ui::ImeTextSpan>& ime_text_spans,
                          const gfx::Range& range,
                          int32_t start,
                          int32_t end,
                          ImeSetCompositionCallback callback) override {}
    void ImeCommitText(const std::u16string& text,
                      const std::vector<ui::ImeTextSpan>& ime_text_spans,
                      const gfx::Range& range,
                      int32_t relative_cursor_position,
                      ImeCommitTextCallback callback) override {}
    void ImeFinishComposingText(bool keep_selection) override {}
    void RequestTextInputStateUpdate() override {}
    void RequestCompositionUpdates(bool immediate_request, bool monitor_request) override {}

    void DispatchEvent(std::unique_ptr<blink::WebCoalescedInputEvent> event,
                      std::optional<std::unique_ptr<blink::WebCoalescedInputEvent>> original_event_for_gesture,
                      DispatchEventCallback callback) override {}
    void DispatchNonBlockingEvent(std::unique_ptr<blink::WebCoalescedInputEvent> event) override {}
    void WaitForInputProcessed(WaitForInputProcessedCallback callback) override {}
    void PingMainThread(PingMainThreadCallback callback) override {}
    void GetFrameWidgetInputHandler(mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetInputHandler> interface_request) override {}
    void UpdateBrowserControlsState(cc::BrowserControlsState constraints,
                                    cc::BrowserControlsState current,
                                    bool animate,
                                    const std::optional<cc::BrowserControlsOffsetTagModifications>& offset_tag_modifications) override {}

    // Required pure virtual methods from WidgetInputHandler
    void TryStartFling() override {}
    void TryFinishFling() override {}

    void SetGestureEventResult(bool result, bool stopPropagation, int32_t fingerId) override {
        last_gesture_result_ = result;
        last_gesture_stop_ = stopPropagation;
        last_gesture_finger_ = fingerId;
    }
    void SetNativeEmbedMode(bool flag) override { native_embed_mode_ = flag; }
    void AttachSoftwareCompositorOhos(::mojo::PendingReceiver<::blink::mojom::SoftwareCompositorOhos> compositor_request) override {}
    void ScrollBy(float delta_x, float delta_y) override {
        last_scroll_x_ = delta_x;
        last_scroll_y_ = delta_y;
    }
    void SetMouseEventResult(bool result, bool stopPropagation) override {
        last_mouse_result_ = result;
        last_mouse_stop_ = stopPropagation;
    }
    void SetBypassVsyncCondition(int32_t condition) override { last_vsync_condition_ = condition; }
    void SetEnableCustomVideoPlayer(bool flag) override { custom_video_player_ = flag; }

    // Track last calls for verification
    bool last_gesture_result_ = false;
    bool last_gesture_stop_ = false;
    int32_t last_gesture_finger_ = 0;
    bool native_embed_mode_ = false;
    float last_scroll_x_ = 0.0f;
    float last_scroll_y_ = 0.0f;
    bool last_mouse_result_ = false;
    bool last_mouse_stop_ = false;
    int32_t last_vsync_condition_ = 0;
    bool custom_video_player_ = false;
};

// Mock InputDispositionHandler - minimal implementation
class MockInputDispositionHandler : public input::InputDispositionHandler {
public:
    MockInputDispositionHandler() = default;
    ~MockInputDispositionHandler() override = default;

    void OnWheelEventAck(const input::MouseWheelEventWithLatencyInfo& event,
                        blink::mojom::InputEventResultSource ack_source,
                        blink::mojom::InputEventResultState ack_result) override {}
    void OnTouchEventAck(const input::TouchEventWithLatencyInfo& event,
                        blink::mojom::InputEventResultSource ack_source,
                        blink::mojom::InputEventResultState ack_result) override {}
    void OnGestureEventAck(const input::GestureEventWithLatencyInfo& event,
                          blink::mojom::InputEventResultSource ack_source,
                          blink::mojom::InputEventResultState ack_result) override {}
};

// Mock InputRouterClient - provides all required virtual implementations
// Also inherits FlingControllerSchedulerClient to avoid Debug build assertion failure
class MockInputRouterClient : public input::InputRouterClient,
                              public input::FlingControllerSchedulerClient {
public:
    MockInputRouterClient() : widget_handler_(std::make_unique<MockWidgetInputHandler>()) {}
    ~MockInputRouterClient() override = default;

    // Required InputRouterClient methods
    blink::mojom::InputEventResultState FilterInputEvent(
        const blink::WebInputEvent& input_event,
        const ui::LatencyInfo& latency_info) override {
        return blink::mojom::InputEventResultState::kNotConsumed;
    }

    void IncrementInFlightEventCount() override {}
    void DecrementInFlightEventCount(blink::mojom::InputEventResultSource ack_source) override {}

    void DidOverscroll(blink::mojom::DidOverscrollParamsPtr params) override {}
    void OnSetCompositorAllowedTouchAction(cc::TouchAction touch_action) override {}
    void DidStartScrollingViewport() override {}

    void ForwardGestureEventWithLatencyInfo(
        const blink::WebGestureEvent& gesture_event,
        const ui::LatencyInfo& latency_info) override {}
    void ForwardWheelEventWithLatencyInfo(
        const blink::WebMouseWheelEvent& wheel_event,
        const ui::LatencyInfo& latency_info) override {}

    bool IsWheelScrollInProgress() override { return false; }
    bool IsAutoscrollInProgress() override { return false; }
    void SetMouseCapture(bool capture) override {}
    void SetAutoscrollSelectionActiveInMainFrame(bool autoscroll_selection) override {}

    void RequestMouseLock(bool from_user_gesture,
                         bool unadjusted_movement,
                         blink::mojom::WidgetInputHandlerHost::RequestMouseLockCallback response) override {}

    gfx::Size GetRootWidgetViewportSize() override { return gfx::Size(1920, 1080); }
    void OnInvalidInputEventSource() override {}

    void DynamicFrameLossEvent(const std::string& sceneId, bool isStart) override {
        last_scene_id_ = sceneId;
        last_is_start_ = isStart;
    }

    blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override {
        return widget_handler_.get();
    }

    void OnImeCancelComposition() override {}
    void OnImeCompositionRangeChanged(
        const gfx::Range& range,
        const std::optional<std::vector<gfx::Rect>>& character_bounds) override {}

    input::StylusInterface* GetStylusInterface() override { return nullptr; }
    void OnStartStylusWriting() override {}
    void OnUnconfirmedTapConvertedToTap() override {}

    input::DispatchToRendererCallback GetDispatchToRendererCallback() override {
        return input::DispatchToRendererCallback();
    }

    // Required FlingControllerSchedulerClient methods
    void ScheduleFlingProgress(
        base::WeakPtr<input::FlingController> fling_controller) override {}
    void DidStopFlingingOnBrowser(
        base::WeakPtr<input::FlingController> fling_controller) override {}
    bool ProgressFlingOnFlingStart() override { return false; }
    bool ShouldUseMobileFlingCurve() override { return false; }
    gfx::Vector2dF GetPixelsPerInch(
        const gfx::PointF& position_in_screen) override {
        return gfx::Vector2dF(96.0, 96.0);
    }

    // Track last calls
    std::string last_scene_id_;
    bool last_is_start_ = false;

private:
    std::unique_ptr<MockWidgetInputHandler> widget_handler_;
};

// CreateInputRouterComponents - creates all objects fresh for each fuzzer iteration
// This ensures statelessness per libFuzzer contract
void CreateInputRouterComponents(
    std::unique_ptr<MockInputRouterClient>& out_client,
    std::unique_ptr<MockInputDispositionHandler>& out_disposition,
    std::unique_ptr<input::InputRouter::Config>& out_config,
    std::unique_ptr<input::InputRouterImpl>& out_router,
    std::shared_ptr<input::ArkwebInputRouterImplUtils>& out_utils) {
    
    auto client = std::make_unique<MockInputRouterClient>();
    auto disposition = std::make_unique<MockInputDispositionHandler>();
    auto config = std::make_unique<input::InputRouter::Config>();
    auto router = std::make_unique<input::InputRouterImpl>(
        client.get(), disposition.get(), client.get(), *config);
    auto utils = std::make_shared<input::ArkwebInputRouterImplUtils>(router.get());
    
    out_client = std::move(client);
    out_disposition = std::move(disposition);
    out_config = std::move(config);
    out_router = std::move(router);
    out_utils = std::move(utils);
}

// Scenario 1: Test SendGestureEventEx with different gesture types
void FuzzSendGestureEventEx(FuzzedDataProvider* fdp) {
    // Create fresh objects for each iteration - stateless fuzzer pattern
    std::unique_ptr<MockInputRouterClient> mock_client;
    std::unique_ptr<MockInputDispositionHandler> mock_disposition;
    std::unique_ptr<input::InputRouter::Config> config;
    std::unique_ptr<input::InputRouterImpl> router;
    std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
    
    CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
    
    int event_type = fdp->ConsumeIntegralInRange<int>(0, 5);
    blink::WebInputEvent::Type type;
    
    switch (event_type) {
        case 0:
            type = blink::WebInputEvent::Type::kGestureScrollUpdate;
            break;
        case 1:
            type = blink::WebInputEvent::Type::kGestureScrollEnd;
            break;
        case 2:
            type = blink::WebInputEvent::Type::kGestureScrollBegin;
            break;
        case 3:
            type = blink::WebInputEvent::Type::kGestureTap;
            break;
        case 4:
            type = blink::WebInputEvent::Type::kGesturePinchBegin;
            break;
        default:
            type = blink::WebInputEvent::Type::kGestureScrollUpdate;
            break;
    }
    
    // Create gesture event with the selected type
    input::GestureEventWithLatencyInfo gesture(
        type, blink::WebInputEvent::kNoModifiers, base::TimeTicks(), ui::LatencyInfo());
    
    // Test: The timestamp logic is triggered naturally by calling the method
    // The internal timeStamp_ and prePerfTimeStamp_ are set by SendGestureEventEx itself
    utils->SendGestureEventEx(gesture);
}

// Scenario 2: Test TracingAndSceneReport with different event types
void FuzzTracingAndSceneReport(FuzzedDataProvider* fdp) {
    // Create fresh objects for each iteration
    std::unique_ptr<MockInputRouterClient> mock_client;
    std::unique_ptr<MockInputDispositionHandler> mock_disposition;
    std::unique_ptr<input::InputRouter::Config> config;
    std::unique_ptr<input::InputRouterImpl> router;
    std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
    
    CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
    
    int event_type = fdp->ConsumeIntegralInRange<int>(0, 4);
    blink::WebInputEvent::Type type;
    
    switch (event_type) {
        case 0:
            type = blink::WebInputEvent::Type::kGestureScrollUpdate;
            break;
        case 1:
            type = blink::WebInputEvent::Type::kGestureScrollEnd;
            break;
        case 2:
            type = blink::WebInputEvent::Type::kGestureTap;
            break;
        case 3:
            type = blink::WebInputEvent::Type::kMouseDown;
            break;
        default:
            type = blink::WebInputEvent::Type::kGestureScrollUpdate;
            break;
    }
    
    ui::LatencyInfo latency_info;
    latency_info.set_trace_id(fdp->ConsumeIntegral<int64_t>());
    
    blink::WebGestureEvent event(type, blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->TracingAndSceneReport(event, latency_info);
}

// Scenario 3: Test ProcessFilteredEvent with various event types
void FuzzProcessFilteredEvent(FuzzedDataProvider* fdp) {
    // Create fresh objects for each iteration
    std::unique_ptr<MockInputRouterClient> mock_client;
    std::unique_ptr<MockInputDispositionHandler> mock_disposition;
    std::unique_ptr<input::InputRouter::Config> config;
    std::unique_ptr<input::InputRouterImpl> router;
    std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
    
    CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
    
    int event_type = fdp->ConsumeIntegralInRange<int>(0, 6);
    blink::WebInputEvent::Type type;
    
    switch (event_type) {
        case 0:
            type = blink::WebInputEvent::Type::kGestureScrollUpdate;
            break;
        case 1:
            type = blink::WebInputEvent::Type::kTouchMove;
            break;
        case 2:
            type = blink::WebInputEvent::Type::kGesturePinchUpdate;
            break;
        case 3:
            type = blink::WebInputEvent::Type::kTouchStart;
            break;
        case 4:
            type = blink::WebInputEvent::Type::kMouseDown;
            break;
        case 5:
            type = blink::WebInputEvent::Type::kMouseUp;
            break;
        default:
            type = blink::WebInputEvent::Type::kPointerCausedUaAction;
            break;
    }
    
    blink::WebGestureEvent event(type, blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(event);
}

// Scenario 4: Test boundary values for SendGestureEventEx
void FuzzBoundaryValuesSendGesture(FuzzedDataProvider* fdp) {
    // Test 1: Multiple rapid calls to test timestamp comparison logic
    {
        std::unique_ptr<MockInputRouterClient> mock_client;
        std::unique_ptr<MockInputDispositionHandler> mock_disposition;
        std::unique_ptr<input::InputRouter::Config> config;
        std::unique_ptr<input::InputRouterImpl> router;
        std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
        
        CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
        
        // First call - should set initial timestamp
        input::GestureEventWithLatencyInfo gesture1(
            blink::WebInputEvent::Type::kGestureScrollUpdate,
            blink::WebInputEvent::kNoModifiers, base::TimeTicks(), ui::LatencyInfo());
        utils->SendGestureEventEx(gesture1);
        
        // Second call immediately - tests timestamp comparison logic internally
        input::GestureEventWithLatencyInfo gesture2(
            blink::WebInputEvent::Type::kGestureScrollUpdate,
            blink::WebInputEvent::kNoModifiers, base::TimeTicks(), ui::LatencyInfo());
        utils->SendGestureEventEx(gesture2);
    }
    
    // Test 2: ScrollEnd event
    {
        std::unique_ptr<MockInputRouterClient> mock_client;
        std::unique_ptr<MockInputDispositionHandler> mock_disposition;
        std::unique_ptr<input::InputRouter::Config> config;
        std::unique_ptr<input::InputRouterImpl> router;
        std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
        
        CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
        
        input::GestureEventWithLatencyInfo gesture3(
            blink::WebInputEvent::Type::kGestureScrollEnd,
            blink::WebInputEvent::kNoModifiers, base::TimeTicks(), ui::LatencyInfo());
        utils->SendGestureEventEx(gesture3);
    }
}

// Scenario 5: Test ProcessFilteredEvent with ARKWEB_SAME_LAYER flag
void FuzzProcessFilteredEventWithSameLayer(FuzzedDataProvider* fdp) {
    // Create fresh objects for each iteration
    std::unique_ptr<MockInputRouterClient> mock_client;
    std::unique_ptr<MockInputDispositionHandler> mock_disposition;
    std::unique_ptr<input::InputRouter::Config> config;
    std::unique_ptr<input::InputRouterImpl> router;
    std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
    
    CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
    
    // Test TouchStart - triggers native_result_ = false in ARKWEB_SAME_LAYER
    blink::WebTouchEvent touch_start(
        blink::WebInputEvent::Type::kTouchStart,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(touch_start);
    
    // Test MouseDown - triggers mouse_native_result_ = false in ARKWEB_SAME_LAYER
    blink::WebMouseEvent mouse_down(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(mouse_down);
}

// Scenario 6: Test all methods in sequence
void FuzzAllMethodsInSequence(FuzzedDataProvider* fdp) {
    // Create fresh objects for each iteration
    std::unique_ptr<MockInputRouterClient> mock_client;
    std::unique_ptr<MockInputDispositionHandler> mock_disposition;
    std::unique_ptr<input::InputRouter::Config> config;
    std::unique_ptr<input::InputRouterImpl> router;
    std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
    
    CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
    
    // First, test SendGestureEventEx
    input::GestureEventWithLatencyInfo gesture(
        blink::WebInputEvent::Type::kGestureScrollUpdate,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks(), ui::LatencyInfo());
    utils->SendGestureEventEx(gesture);
    
    // Then test TracingAndSceneReport
    ui::LatencyInfo latency_info;
    latency_info.set_trace_id(12345);
    blink::WebGestureEvent scroll_event(
        blink::WebInputEvent::Type::kGestureScrollUpdate,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->TracingAndSceneReport(scroll_event, latency_info);
    
    // Finally test ProcessFilteredEvent
    blink::WebGestureEvent pinch_event(
        blink::WebInputEvent::Type::kGesturePinchUpdate,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(pinch_event);
}

// Scenario 7: Multiple rapid calls to test timestamp-based branching
void FuzzMultipleIterations(FuzzedDataProvider* fdp) {
    // Create fresh objects for each iteration
    std::unique_ptr<MockInputRouterClient> mock_client;
    std::unique_ptr<MockInputDispositionHandler> mock_disposition;
    std::unique_ptr<input::InputRouter::Config> config;
    std::unique_ptr<input::InputRouterImpl> router;
    std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
    
    CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
    
    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, 20);
    
    for (size_t i = 0; i < iterations; ++i) {
        int method = fdp->ConsumeIntegralInRange<int>(0, 2);
        
        switch (method) {
            case 0: {
                input::GestureEventWithLatencyInfo gesture(
                    fdp->ConsumeBool() ? blink::WebInputEvent::Type::kGestureScrollUpdate
                                       : blink::WebInputEvent::Type::kGestureScrollEnd,
                    blink::WebInputEvent::kNoModifiers, base::TimeTicks(), ui::LatencyInfo());
                utils->SendGestureEventEx(gesture);
                break;
            }
            case 1: {
                ui::LatencyInfo latency_info;
                latency_info.set_trace_id(fdp->ConsumeIntegral<int64_t>());
                blink::WebGestureEvent event(
                    blink::WebInputEvent::Type::kGestureScrollUpdate,
                    blink::WebInputEvent::kNoModifiers, base::TimeTicks());
                utils->TracingAndSceneReport(event, latency_info);
                break;
            }
            case 2: {
                blink::WebGestureEvent event(
                    fdp->ConsumeBool() ? blink::WebInputEvent::Type::kGestureScrollUpdate
                                       : blink::WebInputEvent::Type::kTouchMove,
                    blink::WebInputEvent::kNoModifiers, base::TimeTicks());
                utils->ProcessFilteredEvent(event);
                break;
            }
        }
    }
}

// Scenario 8: Edge case - test various event types for ProcessFilteredEvent
void FuzzEdgeCaseEventTypes(FuzzedDataProvider* fdp) {
    // Create fresh objects for each iteration
    std::unique_ptr<MockInputRouterClient> mock_client;
    std::unique_ptr<MockInputDispositionHandler> mock_disposition;
    std::unique_ptr<input::InputRouter::Config> config;
    std::unique_ptr<input::InputRouterImpl> router;
    std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
    
    CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
    
    // Test all event types that ProcessFilteredEvent handles
    // kGestureScrollUpdate, kTouchMove, kGesturePinchUpdate (normal log)
    // kTouchStart, kMouseDown (sets native_result in ARKWEB_SAME_LAYER)
    
    // Test gesture events
    blink::WebGestureEvent scroll_update(
        blink::WebInputEvent::Type::kGestureScrollUpdate,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(scroll_update);
    
    blink::WebGestureEvent pinch_update(
        blink::WebInputEvent::Type::kGesturePinchUpdate,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(pinch_update);
    
    // Test touch events
    blink::WebTouchEvent touch_move(
        blink::WebInputEvent::Type::kTouchMove,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(touch_move);
    
    blink::WebTouchEvent touch_start(
        blink::WebInputEvent::Type::kTouchStart,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(touch_start);
    
    // Test mouse events
    blink::WebMouseEvent mouse_down(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(mouse_down);
    
    blink::WebMouseEvent mouse_up(
        blink::WebInputEvent::Type::kMouseUp,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks());
    utils->ProcessFilteredEvent(mouse_up);
}

// Scenario 9: Test Trace ID variations for TracingAndSceneReport
void FuzzTraceIdVariations(FuzzedDataProvider* fdp) {
    // Create fresh objects for each iteration
    std::unique_ptr<MockInputRouterClient> mock_client;
    std::unique_ptr<MockInputDispositionHandler> mock_disposition;
    std::unique_ptr<input::InputRouter::Config> config;
    std::unique_ptr<input::InputRouterImpl> router;
    std::shared_ptr<input::ArkwebInputRouterImplUtils> utils;
    
    CreateInputRouterComponents(mock_client, mock_disposition, config, router, utils);
    
    // Test with different trace IDs
    std::vector<int64_t> trace_ids = {
        0,
        1,
        INT64_MAX,
        INT64_MIN,
        -1,
        fdp->ConsumeIntegral<int64_t>()
    };
    
    for (const auto& trace_id : trace_ids) {
        ui::LatencyInfo latency_info;
        latency_info.set_trace_id(trace_id);
        
        blink::WebGestureEvent event(
            blink::WebInputEvent::Type::kGestureScrollUpdate,
            blink::WebInputEvent::kNoModifiers, base::TimeTicks());
        utils->TracingAndSceneReport(event, latency_info);
    }
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (data == nullptr || size < 1) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);

    // 9 scenarios covering all methods and edge cases
    int scenario = fdp.ConsumeIntegralInRange<int>(0, 8);
    
    switch (scenario) {
        case 0:
            FuzzSendGestureEventEx(&fdp);
            break;
        case 1:
            FuzzTracingAndSceneReport(&fdp);
            break;
        case 2:
            FuzzProcessFilteredEvent(&fdp);
            break;
        case 3:
            FuzzBoundaryValuesSendGesture(&fdp);
            break;
        case 4:
            FuzzProcessFilteredEventWithSameLayer(&fdp);
            break;
        case 5:
            FuzzAllMethodsInSequence(&fdp);
            break;
        case 6:
            FuzzMultipleIterations(&fdp);
            break;
        case 7:
            FuzzEdgeCaseEventTypes(&fdp);
            break;
        case 8:
            FuzzTraceIdVariations(&fdp);
            break;
        default:
            FuzzAllMethodsInSequence(&fdp);
            break;
    }
    return 0;
}
