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
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/command_line.h"
#include "components/input/fling_controller.h"
#include "components/input/input_disposition_handler.h"
#include "components/input/input_router_impl.h"
#include "components/input/input_router_client.h"
#include "ui/base/mojom/ui_base_types.mojom.h"

#include "arkweb/chromium_ext/components/input/arkweb_input_router_impl_ext.h"

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

// Performance optimization: Static shared instances
// These are created once and reused across all fuzzer iterations
static std::unique_ptr<MockInputRouterClient> g_mock_client;
static std::unique_ptr<MockInputDispositionHandler> g_mock_disposition;
static std::unique_ptr<input::InputRouter::Config> g_config;
static std::unique_ptr<input::ArkwebInputRouterImplExt> g_router;

// Initialize shared resources - called once at startup
void InitializeSharedResources() {
    if (!g_mock_client) {
        g_mock_client = std::make_unique<MockInputRouterClient>();
    }
    if (!g_mock_disposition) {
        g_mock_disposition = std::make_unique<MockInputDispositionHandler>();
    }
    if (!g_config) {
        g_config = std::make_unique<input::InputRouter::Config>();
    }
    if (!g_router) {
        // Use MockInputRouterClient as FlingControllerSchedulerClient to avoid Debug assertion failure
        g_router = std::make_unique<input::ArkwebInputRouterImplExt>(
            g_mock_client.get(), g_mock_disposition.get(), g_mock_client.get(), *g_config);
    }
}

void FuzzArkwebInputRouterImplExtConstruction(FuzzedDataProvider* fdp) {
    // Test creating router with valid mocks
    LOG(INFO) << "hexu test fuzz, FuzzArkwebInputRouterImplExtConstruction 1";
    auto client = std::make_unique<MockInputRouterClient>();
    auto disposition = std::make_unique<MockInputDispositionHandler>();
    input::InputRouter::Config config;
    LOG(INFO) << "hexu test fuzz, FuzzArkwebInputRouterImplExtConstruction 2";
    // Use client as FlingControllerSchedulerClient to avoid Debug assertion failure
    input::ArkwebInputRouterImplExt(
        client.get(), disposition.get(), client.get(), config);
    LOG(INFO) << "hexu test fuzz, FuzzArkwebInputRouterImplExtConstruction 3";
}

void FuzzSetGestureEventResult(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzSetGestureEventResult 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzSetGestureEventResult 2";
    bool result = fdp->ConsumeBool();
    bool stopPropagation = fdp->ConsumeBool();
    int32_t fingerId = fdp->ConsumeIntegral<int32_t>();
    LOG(INFO) << "hexu test fuzz, FuzzSetGestureEventResult 3";
    g_router->SetGestureEventResult(result, stopPropagation, fingerId);
    LOG(INFO) << "hexu test fuzz, FuzzSetGestureEventResult 4";
}

void FuzzSetNativeEmbedMode(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzSetNativeEmbedMode 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzSetNativeEmbedMode 2";
    bool flag = fdp->ConsumeBool();
    g_router->SetNativeEmbedMode(flag);
    LOG(INFO) << "hexu test fuzz, FuzzSetNativeEmbedMode 3";
}
void FuzzSetMouseEventResult(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzSetMouseEventResult 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzSetMouseEventResult 2";
    bool result = fdp->ConsumeBool();
    bool stopPropagation = fdp->ConsumeBool();
    g_router->SetMouseEventResult(result, stopPropagation);
    LOG(INFO) << "hexu test fuzz, FuzzSetMouseEventResult 3";
}

void FuzzSetEnableCustomVideoPlayer(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzSetEnableCustomVideoPlayer 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzSetEnableCustomVideoPlayer 2";
    bool flag = fdp->ConsumeBool();
    g_router->SetEnableCustomVideoPlayer(flag);
    LOG(INFO) << "hexu test fuzz, FuzzSetEnableCustomVideoPlayer 3";
}

void FuzzScrollBy(FuzzedDataProvider* fdp) {
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzScrollBy 1";
    float delta_x = fdp->ConsumeFloatingPoint<float>();
    float delta_y = fdp->ConsumeFloatingPoint<float>();
    g_router->ScrollBy(delta_x, delta_y);
    LOG(INFO) << "hexu test fuzz, FuzzScrollBy 2";
}

void FuzzSetBypassVsyncCondition(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzSetBypassVsyncCondition 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzSetBypassVsyncCondition 2";
    int32_t condition = fdp->ConsumeIntegral<int32_t>();
    g_router->SetBypassVsyncCondition(condition);
    LOG(INFO) << "hexu test fuzz, FuzzSetBypassVsyncCondition 3";
}

void FuzzSetFocusWebId(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzSetFocusWebId 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzSetFocusWebId 2";
    int32_t nweb_id = fdp->ConsumeIntegral<int32_t>();
    g_router->SetFocusWebId(nweb_id);
    LOG(INFO) << "hexu test fuzz, FuzzSetFocusWebId 3";
}

void FuzzSetScrollable(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzSetScrollable 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzSetScrollable 2";
    bool enable = fdp->ConsumeBool();
    g_router->SetScrollable(enable);
    LOG(INFO) << "hexu test fuzz, FuzzSetScrollable 3";
}

void FuzzAllMethods(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzAllMethods 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzAllMethods 2";
    // Test all methods with valid parameters
    g_router->SetGestureEventResult(true, false, 1);
    g_router->SetNativeEmbedMode(true);
    g_router->SetMouseEventResult(true, false);
    g_router->SetEnableCustomVideoPlayer(true);
    g_router->ScrollBy(10.0f, 20.0f);
    g_router->SetBypassVsyncCondition(1);
    g_router->SetFocusWebId(123);
    g_router->SetScrollable(true);
    LOG(INFO) << "hexu test fuzz, FuzzAllMethods 3";
}

void FuzzBoundaryValues(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzBoundaryValues 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzBoundaryValues 2";
    // Test boundary values for ScrollBy
    std::vector<float> delta_values = {0.0f, -1.0f, 1.0f, 100.0f, -100.0f, 
                                        FLT_MAX, -FLT_MAX, FLT_MIN, -FLT_MIN};
    for (const auto& delta : delta_values) {
        g_router->ScrollBy(delta, delta);
    }
    
    // Test boundary values for SetBypassVsyncCondition
    std::vector<int32_t> condition_values = {0, 1, -1, 2147483647, -2147483648, INT32_MAX, INT32_MIN};
    for (const auto& condition : condition_values) {
        g_router->SetBypassVsyncCondition(condition);
    }
    
    // Test boundary values for SetFocusWebId
    std::vector<int32_t> id_values = {0, 1, -1, 12345, -12345, INT32_MAX, INT32_MIN};
    for (const auto& id : id_values) {
        g_router->SetFocusWebId(id);
    }
    
    // Test boundary values for SetGestureEventResult
    g_router->SetGestureEventResult(true, true, INT32_MAX);
    g_router->SetGestureEventResult(false, false, INT32_MIN);
    g_router->SetGestureEventResult(true, false, 0);
    LOG(INFO) << "hexu test fuzz, FuzzBoundaryValues 3";
}

void FuzzMultipleSequences(FuzzedDataProvider* fdp) {
    LOG(INFO) << "hexu test fuzz, FuzzMultipleSequences 1";
    InitializeSharedResources();
    LOG(INFO) << "hexu test fuzz, FuzzMultipleSequences 2";
    // Simulate multiple sequences of operations
    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, 10);
    
    for (size_t i = 0; i < iterations; ++i) {
        int method = fdp->ConsumeIntegralInRange<int>(0, 7);
        
        switch (method) {
            case 0:
                g_router->SetGestureEventResult(fdp->ConsumeBool(), fdp->ConsumeBool(), 
                                               fdp->ConsumeIntegral<int32_t>());
                break;
            case 1:
                g_router->SetNativeEmbedMode(fdp->ConsumeBool());
                break;
            case 2:
                g_router->SetMouseEventResult(fdp->ConsumeBool(), fdp->ConsumeBool());
                break;
            case 3:
                g_router->SetEnableCustomVideoPlayer(fdp->ConsumeBool());
                break;
            case 4:
                g_router->ScrollBy(fdp->ConsumeFloatingPoint<float>(), 
                                  fdp->ConsumeFloatingPoint<float>());
                break;
            case 5:
                g_router->SetBypassVsyncCondition(fdp->ConsumeIntegral<int32_t>());
                break;
            case 6:
                g_router->SetFocusWebId(fdp->ConsumeIntegral<int32_t>());
                break;
            case 7:
                g_router->SetScrollable(fdp->ConsumeBool());
                break;
        }
    }
    LOG(INFO) << "hexu test fuzz, FuzzMultipleSequences 3";
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Initialize CommandLine if not already initialized
    static bool command_line_initialized = [] {
        return base::CommandLine::Init(0, nullptr);
    }();

    if (data == nullptr || size < 1) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);

    // 10 scenarios covering all methods and edge cases
    int scenario = fdp.ConsumeIntegralInRange<int>(0, 9);
    
            FuzzArkwebInputRouterImplExtConstruction(&fdp);
            FuzzSetGestureEventResult(&fdp);
            FuzzSetNativeEmbedMode(&fdp);
            FuzzSetMouseEventResult(&fdp);
            FuzzSetEnableCustomVideoPlayer(&fdp);
            FuzzScrollBy(&fdp);
            FuzzSetBypassVsyncCondition(&fdp);
            FuzzSetFocusWebId(&fdp);
            FuzzSetScrollable(&fdp);
            FuzzBoundaryValues(&fdp);
            FuzzMultipleSequences(&fdp);
            FuzzAllMethods(&fdp);
    // Use _exit to avoid static destructor issues during fuzzer execution
    // The fuzzer will restart the process for the next input
    _exit(0);
}
