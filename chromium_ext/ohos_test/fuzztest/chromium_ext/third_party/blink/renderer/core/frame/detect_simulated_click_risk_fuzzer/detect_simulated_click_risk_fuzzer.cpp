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
#include <climits>
#include <memory>
#include <ranges>
#include <string>

// Pre-include standard headers above to shield from #define private public.
// <ranges> must be parsed before the define, otherwise lazy_split_view.h
// gets its 'private' replaced and fails to compile.
#define private public
#include "arkweb/chromium_ext/third_party/blink/renderer/core/frame/detect_simulated_click_risk_enhanced_impl.h"
#undef private

#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"

namespace blink {
namespace {
constexpr size_t MAX_STRING_LENGTH = 255;
constexpr uint8_t MAX_NONCE_SIZE = 256;
constexpr int32_t MAX_ALGORITHM_VALUE = 1000;
constexpr int32_t MAX_VERSION_VALUE = 100;

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

void FuzzGetErrorMessage(FuzzedDataProvider* fdp) {
    LocalDOMWindow* dom_window = nullptr;
    if (g_page_holder) {
        dom_window = g_page_holder->GetFrame().DomWindow();
    }
    if (!dom_window) {
        return;
    }

    auto* impl = MakeGarbageCollected<DetectSimulatedClickRiskEnhancedImpl>(
        dom_window);

    int32_t error_code = fdp->ConsumeIntegral<int32_t>();
    const char* msg = impl->GetErrorMessage(
        static_cast<BusinessError>(error_code));

    impl->GetErrorMessage(BusinessError::INTERNAL_ERROR);
    impl->GetErrorMessage(BusinessError::NETWORK_UNREACHABLE);
    impl->GetErrorMessage(BusinessError::ACCESS_CLOUD_SERVER_FAIL);
    impl->GetErrorMessage(BusinessError::VERIFY_CLOUD_CAPABILITY_FAIL);
    impl->GetErrorMessage(
        BusinessError::INTERFACE_ACCESS_FREQUENCY_EXCEEDS_LIMIT);
    impl->GetErrorMessage(BusinessError::INTERNAL_TIMEOUT);
    impl->GetErrorMessage(BusinessError::INVALID_PARAMETERS);
    impl->GetErrorMessage(static_cast<BusinessError>(0));
    impl->GetErrorMessage(static_cast<BusinessError>(999999));
}

void FuzzGenerateRequestId(FuzzedDataProvider* fdp) {
    LocalDOMWindow* dom_window = nullptr;
    if (g_page_holder) {
        dom_window = g_page_holder->GetFrame().DomWindow();
    }
    if (!dom_window) {
        return;
    }

    auto* impl = MakeGarbageCollected<DetectSimulatedClickRiskEnhancedImpl>(
        dom_window);

    impl->next_request_id_ = fdp->ConsumeIntegral<int32_t>();
    int32_t id1 = impl->GenerateRequestId();
    int32_t id2 = impl->GenerateRequestId();

    impl->next_request_id_ = INT32_MAX - 1;
    impl->GenerateRequestId();
    impl->GenerateRequestId();

    impl->next_request_id_ = -1;
    impl->GenerateRequestId();

    impl->next_request_id_ = 0;
    impl->GenerateRequestId();
}

void FuzzDispose(FuzzedDataProvider* fdp) {
    LocalDOMWindow* dom_window = nullptr;
    if (g_page_holder) {
        dom_window = g_page_holder->GetFrame().DomWindow();
    }
    if (!dom_window) {
        return;
    }

    auto* impl = MakeGarbageCollected<DetectSimulatedClickRiskEnhancedImpl>(
        dom_window);

    impl->Dispose();
    impl->Dispose();
}

void FuzzOnRequestComplete(FuzzedDataProvider* fdp) {
    LocalDOMWindow* dom_window = nullptr;
    if (g_page_holder) {
        dom_window = g_page_holder->GetFrame().DomWindow();
    }
    if (!dom_window) {
        return;
    }

    auto* impl = MakeGarbageCollected<DetectSimulatedClickRiskEnhancedImpl>(
        dom_window);

    int32_t request_id = fdp->ConsumeIntegral<int32_t>();
    int32_t ans_code = fdp->ConsumeIntegral<int32_t>();

    size_t remaining = fdp->remaining_bytes();
    size_t result_length = std::min(static_cast<size_t>(MAX_STRING_LENGTH), remaining);
    std::string result_str;
    if (result_length > 0) {
        result_str = fdp->ConsumeRandomLengthString(result_length);
    }
    String json_result(result_str);

    impl->OnRequestComplete(request_id, ans_code, json_result);

    impl->OnRequestComplete(0, 0, String("{\"result\":\"ok\"}"));
    impl->OnRequestComplete(1, -1, String(""));

    impl->OnRequestComplete(2,
        static_cast<int32_t>(BusinessError::INTERNAL_ERROR), String());
    impl->OnRequestComplete(3,
        static_cast<int32_t>(BusinessError::INVALID_PARAMETERS),
        String());
}

void FuzzValidParams(FuzzedDataProvider* fdp) {
    LocalDOMWindow* dom_window = nullptr;
    if (g_page_holder) {
        dom_window = g_page_holder->GetFrame().DomWindow();
    }
    if (!dom_window) {
        return;
    }

    auto* impl = MakeGarbageCollected<DetectSimulatedClickRiskEnhancedImpl>(
        dom_window);

    int32_t algorithm = fdp->ConsumeIntegralInRange<int32_t>(0,
                                                              MAX_ALGORITHM_VALUE);
    int32_t version = fdp->ConsumeIntegralInRange<int32_t>(0,
                                                            MAX_VERSION_VALUE);

    size_t nonce_size = fdp->ConsumeIntegralInRange<size_t>(0, MAX_NONCE_SIZE);
    Vector<int32_t> nonce;
    nonce.reserve(nonce_size);
    for (size_t i = 0; i < nonce_size; ++i) {
        nonce.push_back(fdp->ConsumeIntegralInRange<int32_t>(INT8_MIN, INT8_MAX));
    }

    ScriptState* script_state = ToScriptStateForMainWorld(dom_window);
    if (!script_state) {
        return;
    }

    ScriptState::Scope scope(script_state);
    DummyExceptionStateForTesting exception_state;

    impl->DetectSimulatedClickRiskEnhanced(
        script_state, algorithm, nonce, version, exception_state);
}

void FuzzBoundaryValues(FuzzedDataProvider* fdp) {
    LocalDOMWindow* dom_window = nullptr;
    if (g_page_holder) {
        dom_window = g_page_holder->GetFrame().DomWindow();
    }
    if (!dom_window) {
        return;
    }

    auto* impl = MakeGarbageCollected<DetectSimulatedClickRiskEnhancedImpl>(
        dom_window);

    int32_t algorithm = fdp->ConsumeBool() ? INT32_MIN : INT32_MAX;
    int32_t version = fdp->ConsumeBool() ? INT32_MIN : INT32_MAX;

    Vector<int32_t> nonce;
    size_t nonce_count = fdp->ConsumeIntegralInRange<size_t>(0, 3);
    for (size_t i = 0; i < nonce_count; ++i) {
        if (fdp->ConsumeBool()) {
            nonce.push_back(INT8_MIN);
        } else {
            nonce.push_back(INT8_MAX);
        }
    }

    ScriptState* script_state = ToScriptStateForMainWorld(dom_window);
    if (!script_state) {
        return;
    }

    ScriptState::Scope scope(script_state);
    DummyExceptionStateForTesting exception_state;

    impl->DetectSimulatedClickRiskEnhanced(
        script_state, algorithm, nonce, version, exception_state);
}

void FuzzInvalidParameters(FuzzedDataProvider* fdp) {
    LocalDOMWindow* dom_window = nullptr;
    if (g_page_holder) {
        dom_window = g_page_holder->GetFrame().DomWindow();
    }
    if (!dom_window) {
        return;
    }

    auto* impl = MakeGarbageCollected<DetectSimulatedClickRiskEnhancedImpl>(
        dom_window);

    Vector<int32_t> invalid_nonce;
    size_t nonce_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
    for (size_t i = 0; i < nonce_count; ++i) {
        if (fdp->ConsumeBool()) {
            invalid_nonce.push_back(fdp->ConsumeIntegralInRange<int32_t>(
                INT32_MIN, INT8_MIN - 1));
        } else {
            invalid_nonce.push_back(fdp->ConsumeIntegralInRange<int32_t>(
                INT8_MAX + 1, INT32_MAX));
        }
    }

    int32_t algorithm = fdp->ConsumeIntegral<int32_t>();
    int32_t version = fdp->ConsumeIntegral<int32_t>();

    ScriptState* script_state = ToScriptStateForMainWorld(dom_window);
    if (!script_state) {
        return;
    }

    ScriptState::Scope scope(script_state);
    DummyExceptionStateForTesting exception_state;

    impl->DetectSimulatedClickRiskEnhanced(
        script_state, algorithm, invalid_nonce, version, exception_state);
}

}  // namespace
}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (data == nullptr || size < sizeof(int32_t)) {
        return 0;
    }

    static blink::Environment env;

    FuzzedDataProvider fdp(data, size);

    int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 6);

    switch (selector) {
        case 0:
            blink::FuzzGetErrorMessage(&fdp);
            break;
        case 1:
            blink::FuzzGenerateRequestId(&fdp);
            break;
        case 2:
            blink::FuzzDispose(&fdp);
            break;
        case 3:
            blink::FuzzOnRequestComplete(&fdp);
            break;
        case 4:
            blink::FuzzValidParams(&fdp);
            break;
        case 5:
            blink::FuzzBoundaryValues(&fdp);
            break;
        case 6:
            blink::FuzzInvalidParameters(&fdp);
            break;
        default:
            blink::FuzzValidParams(&fdp);
            break;
    }

    return 0;
}