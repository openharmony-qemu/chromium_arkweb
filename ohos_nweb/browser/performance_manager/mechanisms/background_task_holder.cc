/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "background_task_holder.h"
#include "background_task_adapter.h"
#include "ohos_adapter_helper.h"
#include "base/logging.h"
#include "base/check.h"

namespace performance_manager::mechanism {
using namespace OHOS::NWeb;

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
FuzzAudioControl* g_fuzz_audio_control = nullptr;
std::unique_ptr<OHOS::NWeb::BackgroundTaskAdapter> g_fuzz_background_task_adapter;
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

void BackgroundStateChangeCallback::RegisterBackgroundTaskPolicyCallback(
    const std::weak_ptr<ApplicationStateChangeCallback> &applicationStateChangeCallback)
{
    std::lock_guard<std::mutex> lock(applicationStateCallbackLock_);
    applicationStateCallback_.push_back(applicationStateChangeCallback);
    LOG(INFO) << "BackgroundStateChangeCallback::RegisterBackgroundTaskPolicyCallback is called.";
}

void BackgroundStateChangeCallback::NotifyApplicationForeground()
{
    std::lock_guard<std::mutex> lock(applicationStateCallbackLock_);
    for (auto callback : applicationStateCallback_) {
      auto callbackSptr = callback.lock();
      if (callbackSptr != nullptr) {
        callbackSptr->NotifyApplicationForeground();
      }
    }
    LOG(INFO) << "BackgroundStateChangeCallback::NotifyApplicationForeground is called.";
}

void BackgroundStateChangeCallback::NotifyApplicationBackground()
{
    std::lock_guard<std::mutex> lock(applicationStateCallbackLock_);
    for (auto callback : applicationStateCallback_) {
      auto callbackSptr = callback.lock();
      if (callbackSptr != nullptr) {
        callbackSptr->NotifyApplicationBackground();
      }
    }
    LOG(INFO) << "BackgroundStateChangeCallback::NotifyApplicationBackground is called.";
}

BackgroundTaskHolder::BackgroundTaskHolder()
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (g_fuzz_background_task_adapter) {
        backgroundTaskAdapter_ = std::move(g_fuzz_background_task_adapter);
    } else {
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    backgroundTaskAdapter_ = OhosAdapterHelper::GetInstance().CreateBackgroundTaskAdapter();
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    }
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (!backgroundTaskAdapter_) {
      LOG(ERROR) << "BackgroundTaskHolder::backgroundTaskAdapter_ is nullptr.";
      return;
    }
    backgroundStateChangeCallbackAdapter_ = std::make_shared<BackgroundStateChangeCallback>();
    if (backgroundStateChangeCallbackAdapter_) {
      backgroundTaskAdapter_->RegisterBackgroundTaskPolicyCallback(backgroundStateChangeCallbackAdapter_);
      LOG(INFO) << "BackgroundTaskHolder::backgroundStateChangeCallbackAdapter_ is registered.";
    } else {
      LOG(ERROR) << "BackgroundTaskHolder::backgroundStateChangeCallbackAdapter_ is nullptr.";
    }
}

bool BackgroundTaskHolder::MaybeRequestBackgroundRunning(
    bool running,
    BackgroundModeAdapter bgMode) {
  return BackgroundTaskAdapter::RequestBackgroundRunning(running, bgMode);
}

void OHOSAudioCapturerSourceCallbackImpl::NotifyApplicationForeground() {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  if (!g_fuzz_audio_control || !g_fuzz_audio_control->has_main_loop) {
    LOG(DEBUG) << "main_loop is null.";
    return;
  }
  if (!g_fuzz_audio_control->has_audio_manager) {
    LOG(DEBUG) << "audio_manager is null.";
    return;
  }
  if (g_fuzz_audio_control->input_streams_empty) {
    LOG(INFO) << "no audio input streams to resume.";
    return;
  }
  LOG(INFO) << "audio output streams have webrtc = " << g_fuzz_audio_control->is_webrtc;
  if (!g_fuzz_audio_control->is_webrtc) {
    LOG(INFO) << "resume foreground audio recording only.";
  }
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
}

void OHOSAudioCapturerSourceCallbackImpl::NotifyApplicationBackground() {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  if (!g_fuzz_audio_control || !g_fuzz_audio_control->has_main_loop) {
    LOG(DEBUG) << "main_loop is null.";
    return;
  }
  if (!g_fuzz_audio_control->has_audio_manager) {
    LOG(DEBUG) << "audio_manager is null.";
    return;
  }
  if (g_fuzz_audio_control->input_streams_empty) {
    LOG(INFO) << "no audio input streams to pause.";
    return;
  }
  LOG(INFO) << "audio output streams have webrtc = " << g_fuzz_audio_control->is_webrtc;
  if (!g_fuzz_audio_control->is_webrtc) {
    LOG(INFO) << "pause background audio recording only.";
  }
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
}
}  // namespace performance_manager::mechanism
