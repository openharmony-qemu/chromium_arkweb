/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#ifndef BASE_OHOS_DYNAMIC_FRAME_LOSS_MONITOR_H_
#define BASE_OHOS_DYNAMIC_FRAME_LOSS_MONITOR_H_

#include <mutex>
#include "base/functional/callback.h"
#include "arkweb/ohos_adapter_ndk/interfaces/hiappevent_adapter.h"

namespace base {
namespace ohos {

// Callback type for reporting HiAppEvent data externally (e.g., via IPC).
// When set, ReportToHiAppEvent will call this callback instead of reporting
// directly, allowing the caller to forward data to another process.
using HiAppEventCallback =
    base::RepeatingCallback<void(const OHOS::NWeb::HiAppeventAdapter::DynamicFrameDropInfo&)>;

// Count the Frame Loss event when page is sliding
class DynamicFrameLossMonitor {
 public:
  DynamicFrameLossMonitor() = default;
  ~DynamicFrameLossMonitor() = default;

  DynamicFrameLossMonitor(const DynamicFrameLossMonitor&) = delete;
  DynamicFrameLossMonitor& operator=(const DynamicFrameLossMonitor&) = delete;

  static DynamicFrameLossMonitor& GetInstance();

  void StartMonitor(int32_t nweb_id);
  void StopMonitor();
  void OnSwapBuffer();
  void OnVsync();

  void SetHiAppEventCallback(HiAppEventCallback callback);

 private:
  int64_t GetCurrentTimestampMS();
  void ResetStatus();
  void InvokeHiAppEventCallback(
      const OHOS::NWeb::HiAppeventAdapter::DynamicFrameDropInfo& drop_info,
      const HiAppEventCallback& callback_copy);
  void InvokeHiSysEventReport(int64_t start_time, int64_t duration,
                               int32_t total_frames, int32_t missed_frames,
                               int64_t max_frametime, int32_t max_seq_missed);

  std::mutex monitor_mutex_;
  bool is_monitoring_ = false;

  // record frame loss only after receive first response frame
  bool received_first_frame_ = false;

  // interaction scene begin time
  int64_t start_time_ = 0;
  // interaction scene stop time
  int64_t stop_time_ = 0;
  // total app frames during the scene
  int32_t total_app_frames_ = 0;
  // total app missed frames during the scene
  int32_t total_app_missed_frames_ = 0;

  // max single frame during the scene
  int64_t max_app_frametime_ = 0;
  // timestamp of previous swap buffer
  int64_t prev_swap_buffer_time_ = 0;

  // max successive missed frames during the scene
  int32_t max_app_seq_missed_frames_ = 0;
  // successive missed frames during the scene
  int32_t app_seq_missed_frames_ = 0;
  int32_t app_seq_frames_ = 0;

  // the number of buffer, if cached_buffer_number < 0, it will be loss frame
  int32_t cached_buffer_number_ = 0;

  // the id fo current nweb
  int32_t current_nweb_id_ = 0;

  HiAppEventCallback hiappevent_callback_;
};
}  // namespace ohos
}  // namespace base

#endif  // BASE_OHOS_DYNAMIC_FRAME_LOSS_MONITOR_H_
