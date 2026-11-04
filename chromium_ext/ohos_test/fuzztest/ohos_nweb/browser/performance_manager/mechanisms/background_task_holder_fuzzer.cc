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


#include <cstdint>

#include <memory>

#include <fuzzer/FuzzedDataProvider.h>

#include "third_party/ohos_ndk/includes/ohos_adapter/background_task_adapter.h"
#include "base/check.h"

#define private public
#include "background_task_holder.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#undef private

namespace {

using namespace OHOS::NWeb;
using BackgroundTaskHolder = performance_manager::mechanism::BackgroundTaskHolder;
using BackgroundStateChangeCallback =
    performance_manager::mechanism::BackgroundStateChangeCallback;
using ApplicationStateChangeCallback =
    performance_manager::mechanism::ApplicationStateChangeCallback;
using OHOSAudioCapturerSourceCallbackImpl =
    performance_manager::mechanism::OHOSAudioCapturerSourceCallbackImpl;

class MockApplicationStateCallback : public ApplicationStateChangeCallback {
public:
    void NotifyApplicationForeground() override { foreground_called_ = true; }
    void NotifyApplicationBackground() override { background_called_ = true; }

    bool foreground_called_ = false;
    bool background_called_ = false;
};

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
using FuzzAudioControl = performance_manager::mechanism::FuzzAudioControl;

class MockBackgroundTaskAdapter : public BackgroundTaskAdapter {
public:
    bool RequestBackgroundTaskRunning(bool running,
                                      BackgroundModeAdapter bgMode) override
    {
        return true;
    }
    void RegisterBackgroundTaskPolicyCallback(
        std::shared_ptr<BackgroundStateChangeCallbackAdapter> callback) override
    {
        registered_callback_ = std::move(callback);
    }

    std::shared_ptr<BackgroundStateChangeCallbackAdapter> registered_callback_;
};

class FuzzAudioControlScope {
public:
    explicit FuzzAudioControlScope(FuzzAudioControl& control)
        : saved_(performance_manager::mechanism::g_fuzz_audio_control)
    {
        performance_manager::mechanism::g_fuzz_audio_control = &control;
    }
    ~FuzzAudioControlScope()
    {
        performance_manager::mechanism::g_fuzz_audio_control = saved_;
    }
    FuzzAudioControlScope(const FuzzAudioControlScope&) = delete;
    FuzzAudioControlScope& operator=(const FuzzAudioControlScope&) = delete;

private:
    FuzzAudioControl* saved_;
};

void FuzzAudioNoControl()
{
    OHOSAudioCapturerSourceCallbackImpl impl;
    impl.NotifyApplicationForeground();
    impl.NotifyApplicationBackground();
}

void FuzzAudioWithControl(bool has_main_loop, bool has_audio_manager,
                          bool input_streams_empty, bool is_webrtc)
{
    FuzzAudioControl control;
    control.has_main_loop = has_main_loop;
    control.has_audio_manager = has_audio_manager;
    control.input_streams_empty = input_streams_empty;
    control.is_webrtc = is_webrtc;
    FuzzAudioControlScope scope(control);

    OHOSAudioCapturerSourceCallbackImpl impl;
    impl.NotifyApplicationForeground();
    impl.NotifyApplicationBackground();
}

void FuzzConstructorWithMockAdapter()
{
    performance_manager::mechanism::g_fuzz_background_task_adapter =
        std::make_unique<MockBackgroundTaskAdapter>();
    auto holder = std::make_unique<BackgroundTaskHolder>();
    CHECK(holder);
    CHECK(holder->backgroundStateChangeCallbackAdapter_);
    performance_manager::mechanism::g_fuzz_background_task_adapter.reset();
}

void FuzzConstructorWithNullAdapter()
{
    performance_manager::mechanism::g_fuzz_background_task_adapter.reset();
    auto holder = std::make_unique<BackgroundTaskHolder>();
    if (!holder) {
        return;
    }
    if (holder->backgroundTaskAdapter_) {
        CHECK(holder->backgroundStateChangeCallbackAdapter_);
    }
}
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

void FuzzRegisterAndNotify()
{
    auto callback = std::make_shared<BackgroundStateChangeCallback>();
    if (!callback) {
        return;
    }
    auto mock = std::make_shared<MockApplicationStateCallback>();
    if (!mock) {
        return;
    }

    callback->RegisterBackgroundTaskPolicyCallback(mock);

    mock->foreground_called_ = false;
    callback->NotifyApplicationForeground();
    CHECK(mock->foreground_called_);

    mock->background_called_ = false;
    callback->NotifyApplicationBackground();
    CHECK(mock->background_called_);
}

void FuzzNotifyWithExpiredWeak()
{
    auto callback = std::make_shared<BackgroundStateChangeCallback>();
    if (!callback) {
        return;
    }
    {
        auto temp = std::make_shared<MockApplicationStateCallback>();
        callback->RegisterBackgroundTaskPolicyCallback(temp);
    }
    callback->NotifyApplicationForeground();
    callback->NotifyApplicationBackground();
}

void FuzzNotifyEmpty()
{
    auto callback = std::make_shared<BackgroundStateChangeCallback>();
    if (!callback) {
        return;
    }
    callback->NotifyApplicationForeground();
    callback->NotifyApplicationBackground();
}

void FuzzMultipleRegistrations()
{
    auto callback = std::make_shared<BackgroundStateChangeCallback>();
    if (!callback) {
        return;
    }
    auto cb1 = std::make_shared<MockApplicationStateCallback>();
    auto cb2 = std::make_shared<MockApplicationStateCallback>();
    if (!cb1 || !cb2) {
        return;
    }

    callback->RegisterBackgroundTaskPolicyCallback(cb1);
    callback->RegisterBackgroundTaskPolicyCallback(cb2);

    callback->NotifyApplicationForeground();
    CHECK(cb1->foreground_called_);
    CHECK(cb2->foreground_called_);

    callback->NotifyApplicationBackground();
    CHECK(cb1->background_called_);
    CHECK(cb2->background_called_);
}

void FuzzBackgroundTaskHolderConstructor()
{
    auto holder = std::make_unique<BackgroundTaskHolder>();
    CHECK(holder);
}

void FuzzMaybeRequestBackgroundRunning()
{
    auto holder = std::make_unique<BackgroundTaskHolder>();
    if (!holder) {
        return;
    }
    holder->MaybeRequestBackgroundRunning(true,
        BackgroundModeAdapter::AUDIO_PLAYBACK);
    holder->MaybeRequestBackgroundRunning(false,
        BackgroundModeAdapter::AUDIO_PLAYBACK);
    holder->MaybeRequestBackgroundRunning(true,
        BackgroundModeAdapter::DATA_TRANSFER);
    holder->MaybeRequestBackgroundRunning(false,
        BackgroundModeAdapter::AUDIO_RECORDING);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (data == nullptr || size < sizeof(uint8_t)) {
        return 0;
    }

    static bool initialized = false;
    if (!initialized) {
        initialized = true;
    }

    FuzzedDataProvider fdp(data, size);
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    constexpr uint8_t kMaxScenario = 13;
#else
    constexpr uint8_t kMaxScenario = 5;
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    uint8_t scenario = fdp.ConsumeIntegralInRange<uint8_t>(0, kMaxScenario);

    switch (scenario) {
    case 0:
        FuzzRegisterAndNotify();
        break;
    case 1:
        FuzzNotifyWithExpiredWeak();
        break;
    case 2:
        FuzzNotifyEmpty();
        break;
    case 3:
        FuzzMultipleRegistrations();
        break;
    case 4:
        FuzzBackgroundTaskHolderConstructor();
        break;
    case 5:
        FuzzMaybeRequestBackgroundRunning();
        break;
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    case 6:
        FuzzAudioNoControl();
        break;
    case 7:
        FuzzAudioWithControl(false, true, false, false);
        break;
    case 8:
        FuzzAudioWithControl(true, false, false, false);
        break;
    case 9:
        FuzzAudioWithControl(true, true, true, false);
        break;
    case 10:
        FuzzAudioWithControl(true, true, false, true);
        break;
    case 11:
        FuzzAudioWithControl(true, true, false, false);
        break;
    case 12:
        FuzzConstructorWithMockAdapter();
        break;
    case 13:
        FuzzConstructorWithNullAdapter();
        break;
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    default:
        break;
    }

    return 0;
}
