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

#include "base/ohos/dynamic_frame_loss_monitor.h"

#include <chrono>

#include "arkweb/ohos_nweb/src/sysevent/event_reporter.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/functional/callback.h"
#include "base/trace_event/trace_event.h"
#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#include "arkweb/ohos_adapter_ndk/interfaces/hiappevent_adapter.h"

using OHOS::NWeb::OhosAdapterHelper;
using OHOS::NWeb::HiAppeventAdapter;
namespace {
const int kSuccessiveFrameLossThreshold = 1;
const int kMicrosecondsPerMillisecond = 1000;

void ReportDynamicStasticToHiAppevent(const std::string& domain,
                                       const std::string& eventName,
                                       int32_t eventType,
                                       const HiAppeventAdapter::DynamicFrameDropInfo& dynamicFrameDropInfo) {
  OhosAdapterHelper::GetInstance().GetHiAppeventAdapterInstance().
      ReportDynamicStastic(domain, eventName, eventType, dynamicFrameDropInfo);
}
}  // namespace

namespace base {
namespace ohos {

DynamicFrameLossMonitor& DynamicFrameLossMonitor::GetInstance() {
  static base::NoDestructor<DynamicFrameLossMonitor> instance;
  return *instance.get();
}

void DynamicFrameLossMonitor::StartMonitor(int32_t nweb_id) {
  std::unique_lock<std::mutex> lock(monitor_mutex_);
  current_nweb_id_ = nweb_id;
  if (is_monitoring_) {
    return;
  }
  is_monitoring_ = true;
  start_time_ = GetCurrentTimestampMS();
}

void DynamicFrameLossMonitor::SetHiAppEventCallback(HiAppEventCallback callback) {
  std::unique_lock<std::mutex> lock(monitor_mutex_);
  hiappevent_callback_ = std::move(callback);
}

void DynamicFrameLossMonitor::StopMonitor() {
  HiAppeventAdapter::DynamicFrameDropInfo drop_info;
  HiAppEventCallback callback_copy;
  bool should_report_happevent = false;
  bool should_report_hisysevent = false;
  int64_t captured_start_time = 0;
  int64_t captured_duration = 0;
  int32_t hisys_total_frames = 0;
  int32_t hisys_missed_frames = 0;
  int64_t captured_max_frametime = 0;
  int32_t hisys_max_seq_missed = 0;

  {
    std::unique_lock<std::mutex> lock(monitor_mutex_);
    if (!is_monitoring_) { return; }
    stop_time_ = GetCurrentTimestampMS();
    captured_start_time = start_time_;
    captured_duration = stop_time_ - start_time_;
    captured_max_frametime = max_app_frametime_;
    if (captured_max_frametime >= HiAppeventAdapter::kJankReportThreshold) {
      should_report_happevent = true;
      drop_info.start_time = captured_start_time;
      drop_info.duration = captured_duration;
      drop_info.max_app_frame_time = captured_max_frametime;
      drop_info.web_id = current_nweb_id_;
      callback_copy = hiappevent_callback_;
    }
    if (total_app_missed_frames_ > 0) {
      should_report_hisysevent = true;
      hisys_total_frames = total_app_frames_;
      hisys_missed_frames = total_app_missed_frames_;
      hisys_max_seq_missed = max_app_seq_missed_frames_;
    }
    ResetStatus();
  }

  if (should_report_happevent) {
    InvokeHiAppEventCallback(drop_info, callback_copy);
  }
  if (should_report_hisysevent) {
    InvokeHiSysEventReport(captured_start_time, captured_duration,
        hisys_total_frames, hisys_missed_frames,
        captured_max_frametime, hisys_max_seq_missed);
  }
}

void DynamicFrameLossMonitor::InvokeHiAppEventCallback(
    const HiAppeventAdapter::DynamicFrameDropInfo& drop_info,
    const HiAppEventCallback& callback_copy) {
  if (callback_copy) {
    callback_copy.Run(drop_info);
  } else {
    ReportDynamicStasticToHiAppevent(HiAppeventAdapter::kDomain,
        HiAppeventAdapter::kEventName, HiAppeventAdapter::kEventTypeStatistic, drop_info);
  }
}

void DynamicFrameLossMonitor::InvokeHiSysEventReport(
    int64_t start_time, int64_t duration,
    int32_t total_frames, int32_t missed_frames,
    int64_t max_frametime, int32_t max_seq_missed) {
  LOG(DEBUG) << "StartTime: " << start_time
             << ", Duration: " << duration
             << ", MaxAppFrametime: " << max_frametime;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::LOWEST},
      base::BindOnce(&ReportSlideJankStats, start_time, duration,
                     total_frames, missed_frames, max_frametime,
                     max_seq_missed));
  TRACE_EVENT1("base", "WEBVIEW::DYNAMIC_FRAME_DROP_STATISTICS", "info",
               "StartTime: " + std::to_string(start_time) +
                   ", Duration: " + std::to_string(duration) +
                   ", MaxAppFrametime: " + std::to_string(max_frametime));
}

void DynamicFrameLossMonitor::OnVsync() {
  std::unique_lock<std::mutex> lock(monitor_mutex_);
  if (!is_monitoring_) {
    return;
  }

  ++total_app_frames_;
  if (!received_first_frame_) {
    return;
  }

  if (cached_buffer_number_ <= 0) {
    // frame loss occurs
    ++total_app_missed_frames_;
    ++app_seq_missed_frames_;
    if (app_seq_frames_ <= kSuccessiveFrameLossThreshold) {
      app_seq_frames_ = 0;
    }
    TRACE_EVENT0("base", "WEBVIEW::DYNAMIC_FRAME_DROP_STATISTICS");
  } else {
    ++app_seq_frames_;
    if (app_seq_frames_ > kSuccessiveFrameLossThreshold) {
      app_seq_missed_frames_ = 0;
    }
    --cached_buffer_number_;
  }

  max_app_seq_missed_frames_ =
      std::max(max_app_seq_missed_frames_, app_seq_missed_frames_);
}

void DynamicFrameLossMonitor::OnSwapBuffer() {
  std::unique_lock<std::mutex> lock(monitor_mutex_);
  if (!is_monitoring_) {
    return;
  }
  received_first_frame_ = true;
  ++cached_buffer_number_;
  if (prev_swap_buffer_time_ == 0) {
    prev_swap_buffer_time_ = GetCurrentTimestampMS();
    return;
  }
  auto current = GetCurrentTimestampMS();

  max_app_frametime_ =
      std::max(max_app_frametime_, current - prev_swap_buffer_time_);
  prev_swap_buffer_time_ = current;
}

int64_t DynamicFrameLossMonitor::GetCurrentTimestampMS() {
  auto currentTime = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(currentTime)
             .count() /
         kMicrosecondsPerMillisecond;
}


void DynamicFrameLossMonitor::ResetStatus() {
  is_monitoring_ = false;
  received_first_frame_ = false;

  start_time_ = 0;
  stop_time_ = 0;
  total_app_frames_ = 0;
  total_app_missed_frames_ = 0;

  max_app_seq_missed_frames_ = 0;
  app_seq_missed_frames_ = 0;
  app_seq_frames_ = 0;

  prev_swap_buffer_time_ = 0;
  max_app_frametime_ = 0;

  cached_buffer_number_ = 0;
  current_nweb_id_ = 0;
}
}  // namespace ohos
}  // namespace base
                    