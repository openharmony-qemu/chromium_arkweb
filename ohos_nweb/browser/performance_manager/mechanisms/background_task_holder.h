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

#ifndef BACKGROUND_TASK_HOLDER_H_
#define BACKGROUND_TASK_HOLDER_H_

#include <vector>
#include <mutex>
#include "third_party/ohos_ndk/includes/ohos_adapter/background_task_adapter.h"

namespace performance_manager::mechanism {

class ApplicationStateChangeCallback {
public:
    virtual ~ApplicationStateChangeCallback() {}

    virtual void NotifyApplicationForeground() = 0;
    virtual void NotifyApplicationBackground() = 0;
};

class  BackgroundStateChangeCallback : public OHOS::NWeb::BackgroundStateChangeCallbackAdapter {
public:
    BackgroundStateChangeCallback() = default;
    ~BackgroundStateChangeCallback() = default;

    void NotifyApplicationForeground() override;
    void NotifyApplicationBackground() override;
    void RegisterBackgroundTaskPolicyCallback(
        const std::weak_ptr<ApplicationStateChangeCallback> &applicationStateChangeCallback);
private:
    std::vector<std::weak_ptr<ApplicationStateChangeCallback>> applicationStateCallback_;
    std::mutex applicationStateCallbackLock_;
};

class BackgroundTaskHolder {
 public:
  BackgroundTaskHolder();
  BackgroundTaskHolder(const BackgroundTaskHolder& other) = delete;
  BackgroundTaskHolder& operator=(const BackgroundTaskHolder&) = delete;
  ~BackgroundTaskHolder() = default;

  bool MaybeRequestBackgroundRunning(bool running,
                                     OHOS::NWeb::BackgroundModeAdapter bgMode);

  std::shared_ptr<OHOS::NWeb::BackgroundStateChangeCallbackAdapter> backgroundStateChangeCallbackAdapter_;

private:
  std::unique_ptr<OHOS::NWeb::BackgroundTaskAdapter> backgroundTaskAdapter_;

};

class OHOSAudioCapturerSourceCallbackImpl : public ApplicationStateChangeCallback {
 public:
  OHOSAudioCapturerSourceCallbackImpl() = default;
  ~OHOSAudioCapturerSourceCallbackImpl() = default;

  void NotifyApplicationForeground() override;
  void NotifyApplicationBackground() override;
};
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
struct FuzzAudioControl {
    bool has_main_loop = true;
    bool has_audio_manager = true;
    bool input_streams_empty = false;
    bool is_webrtc = false;
};
extern FuzzAudioControl* g_fuzz_audio_control;
extern std::unique_ptr<OHOS::NWeb::BackgroundTaskAdapter> g_fuzz_background_task_adapter;
#endif  // FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

}  // namespace performance_manager::mechanism

#endif  // BACKGROUND_TASK_HOLDER_H_
