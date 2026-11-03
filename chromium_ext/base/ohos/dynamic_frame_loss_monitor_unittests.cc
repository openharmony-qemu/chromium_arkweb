/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ohos_nweb/src/sysevent/event_reporter.h"
#define private public
#include "base/ohos/dynamic_frame_loss_monitor.h"
#undef private

namespace base {
namespace ohos {

class DynamicFrameLossMonitorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dynamic_frame = std::make_shared<DynamicFrameLossMonitor>();
    callback_called_ = false;
  }

  void TearDown() override {}

  std::shared_ptr<DynamicFrameLossMonitor> dynamic_frame;
  bool callback_called_ = false;
};

TEST_F(DynamicFrameLossMonitorTest, GetInstance) {
  dynamic_frame->prev_swap_buffer_time_ = 0;
  DynamicFrameLossMonitor& ret = dynamic_frame->GetInstance();
  EXPECT_EQ(ret.prev_swap_buffer_time_, 0);
}

TEST_F(DynamicFrameLossMonitorTest, StartMonitor_001) {
  int32_t nweb_id = 1;
  dynamic_frame->is_monitoring_ = false;
  dynamic_frame->StartMonitor(nweb_id);
  EXPECT_TRUE(dynamic_frame->is_monitoring_);
}

TEST_F(DynamicFrameLossMonitorTest, StartMonitor_002) {
  int32_t nweb_id = 1;
  dynamic_frame->is_monitoring_ = false;
  dynamic_frame->StartMonitor(nweb_id);
  EXPECT_EQ(dynamic_frame->current_nweb_id_, nweb_id);
}

TEST_F(DynamicFrameLossMonitorTest, StopMonitor_001) {
  dynamic_frame->is_monitoring_ = false;
  dynamic_frame->StopMonitor();
  EXPECT_FALSE(dynamic_frame->is_monitoring_);
}

TEST_F(DynamicFrameLossMonitorTest, StopMonitor_002) {
  dynamic_frame->stop_time_ = 0;
  dynamic_frame->is_monitoring_ = true;
  dynamic_frame->StopMonitor();
  EXPECT_EQ(dynamic_frame->stop_time_, 0);
}

TEST_F(DynamicFrameLossMonitorTest, OnVsync_001) {
  dynamic_frame->is_monitoring_ = false;
  dynamic_frame->OnVsync();
  EXPECT_FALSE(dynamic_frame->is_monitoring_);
}

TEST_F(DynamicFrameLossMonitorTest, OnVsync_002) {
  dynamic_frame->total_app_frames_ = 0;
  dynamic_frame->is_monitoring_ = true;
  dynamic_frame->received_first_frame_ = false;
  dynamic_frame->OnVsync();
  EXPECT_NE(dynamic_frame->total_app_frames_, 0);
}

TEST_F(DynamicFrameLossMonitorTest, OnVsync_003) {
  dynamic_frame->total_app_frames_ = 0;
  dynamic_frame->total_app_missed_frames_ = 0;
  dynamic_frame->is_monitoring_ = true;
  dynamic_frame->received_first_frame_ = true;
  dynamic_frame->cached_buffer_number_ = 1;
  dynamic_frame->app_seq_frames_ = 0;
  dynamic_frame->OnVsync();
  EXPECT_NE(dynamic_frame->app_seq_frames_, 0);
}

TEST_F(DynamicFrameLossMonitorTest, OnVsync_004) {
  dynamic_frame->total_app_frames_ = 0;
  dynamic_frame->total_app_missed_frames_ = 0;
  dynamic_frame->is_monitoring_ = true;
  dynamic_frame->received_first_frame_ = true;
  dynamic_frame->cached_buffer_number_ = 1;
  dynamic_frame->app_seq_frames_ = 2;
  dynamic_frame->OnVsync();
  EXPECT_NE(dynamic_frame->app_seq_frames_, 2);
}

TEST_F(DynamicFrameLossMonitorTest, OnVsync_005) {
  dynamic_frame->is_monitoring_ = true;
  dynamic_frame->received_first_frame_ = true;
  dynamic_frame->cached_buffer_number_ = 0;
  dynamic_frame->total_app_missed_frames_ = 0;
  dynamic_frame->app_seq_frames_ = 2;
  dynamic_frame->OnVsync();
  EXPECT_EQ(dynamic_frame->total_app_missed_frames_, 1);
}

TEST_F(DynamicFrameLossMonitorTest, OnVsync_006) {
  dynamic_frame->is_monitoring_ = true;
  dynamic_frame->received_first_frame_ = true;
  dynamic_frame->cached_buffer_number_ = 0;
  dynamic_frame->total_app_missed_frames_ = 0;
  dynamic_frame->app_seq_frames_ = 1;
  dynamic_frame->OnVsync();
  EXPECT_EQ(dynamic_frame->app_seq_frames_, 0);
}

TEST_F(DynamicFrameLossMonitorTest, OnSwapBuffer_001) {
  auto initial_received_first_frame = dynamic_frame->received_first_frame_;
  dynamic_frame->is_monitoring_ = false;
  dynamic_frame->OnSwapBuffer();
  EXPECT_EQ(dynamic_frame->received_first_frame_, initial_received_first_frame);
}

TEST_F(DynamicFrameLossMonitorTest, OnSwapBuffer_002) {
  dynamic_frame->is_monitoring_ = true;
  dynamic_frame->OnSwapBuffer();
  auto received_first_frame = dynamic_frame->received_first_frame_;
  EXPECT_TRUE(received_first_frame);
  EXPECT_GT(dynamic_frame->prev_swap_buffer_time_, 0);
  EXPECT_EQ(0, dynamic_frame->max_app_frametime_);
}

TEST_F(DynamicFrameLossMonitorTest, OnSwapBuffer_003) {
  dynamic_frame->is_monitoring_ = true;
  dynamic_frame->OnSwapBuffer();
  int64_t initialTime = dynamic_frame->prev_swap_buffer_time_;
  dynamic_frame->prev_swap_buffer_time_ = initialTime + 100;
  dynamic_frame->OnSwapBuffer();
  EXPECT_TRUE(dynamic_frame->received_first_frame_);
  EXPECT_EQ(2, dynamic_frame->cached_buffer_number_);
}

TEST_F(DynamicFrameLossMonitorTest, OnSwapBuffer_004) {
  dynamic_frame->is_monitoring_ = true;
  dynamic_frame->OnSwapBuffer();
  int64_t initialTime = dynamic_frame->prev_swap_buffer_time_;
  dynamic_frame->prev_swap_buffer_time_ = initialTime + 50;
  dynamic_frame->OnSwapBuffer();
  dynamic_frame->prev_swap_buffer_time_ = initialTime + 150;
  dynamic_frame->OnSwapBuffer();
  EXPECT_NE(24, dynamic_frame->max_app_frametime_);
}

TEST_F(DynamicFrameLossMonitorTest, ResetStatus_001) {
  dynamic_frame->ResetStatus();
  EXPECT_FALSE(dynamic_frame->is_monitoring_);
  EXPECT_FALSE(dynamic_frame->received_first_frame_);
  EXPECT_EQ(0, dynamic_frame->start_time_);
  EXPECT_EQ(0, dynamic_frame->stop_time_);
  EXPECT_EQ(0, dynamic_frame->total_app_frames_);
  EXPECT_EQ(0, dynamic_frame->total_app_missed_frames_);
  EXPECT_EQ(0, dynamic_frame->max_app_seq_missed_frames_);
  EXPECT_EQ(0, dynamic_frame->app_seq_missed_frames_);
  EXPECT_EQ(0, dynamic_frame->app_seq_frames_);
  EXPECT_EQ(0, dynamic_frame->prev_swap_buffer_time_);
  EXPECT_EQ(0, dynamic_frame->max_app_frametime_);
  EXPECT_EQ(0, dynamic_frame->cached_buffer_number_);
}

TEST_F(DynamicFrameLossMonitorTest, SetHiAppEventCallback_001) {
  callback_called_ = false;
  dynamic_frame->SetHiAppEventCallback(base::BindRepeating(
      [](bool* callback_called, const OHOS::NWeb::HiAppeventAdapter::DynamicFrameDropInfo&) {
        *callback_called = true;
      }, &callback_called_));
  EXPECT_TRUE(dynamic_frame->hiappevent_callback_);
}

TEST_F(DynamicFrameLossMonitorTest, FullStartStopCycleWithCallback) {
  int call_count = 0;
  dynamic_frame->SetHiAppEventCallback(base::BindRepeating(
      [](int* count, const OHOS::NWeb::HiAppeventAdapter::DynamicFrameDropInfo& info) {
        ++*count;
        EXPECT_GE(info.max_app_frame_time, 50);
      }, &call_count));
  // Start monitor
  dynamic_frame->StartMonitor(1);
  EXPECT_TRUE(dynamic_frame->is_monitoring_);
  // Simulate swap buffers with high frame time
  dynamic_frame->OnSwapBuffer();
  dynamic_frame->prev_swap_buffer_time_ = 0;
  dynamic_frame->max_app_frametime_ = 100;
  // Stop monitor — should trigger callback
  dynamic_frame->StopMonitor();
  EXPECT_EQ(call_count, 1);
  EXPECT_FALSE(dynamic_frame->is_monitoring_);
}

TEST_F(DynamicFrameLossMonitorTest, CallbackSetButThresholdNotMet) {
  int call_count = 0;
  dynamic_frame->SetHiAppEventCallback(base::BindRepeating(
      [](int* count, const OHOS::NWeb::HiAppeventAdapter::DynamicFrameDropInfo&) {
        ++*count;
      }, &call_count));
  // Start monitor
  dynamic_frame->StartMonitor(1);
  // Simulate swap buffers with low frame time (below threshold)
  dynamic_frame->OnSwapBuffer();
  dynamic_frame->prev_swap_buffer_time_ = 0;
  dynamic_frame->max_app_frametime_ = 30;
  // Stop monitor — should NOT trigger callback
  dynamic_frame->StopMonitor();
  EXPECT_EQ(call_count, 0);
  // Second cycle with high frame time — callback should still work
  dynamic_frame->StartMonitor(2);
  dynamic_frame->OnSwapBuffer();
  dynamic_frame->prev_swap_buffer_time_ = 0;
  dynamic_frame->max_app_frametime_ = 100;
  dynamic_frame->StopMonitor();
  EXPECT_EQ(call_count, 1);
}

TEST_F(DynamicFrameLossMonitorTest, ResetStatusClearsNwebId) {
  dynamic_frame->current_nweb_id_ = 5;
  dynamic_frame->ResetStatus();
  EXPECT_EQ(dynamic_frame->current_nweb_id_, 0);
}

TEST_F(DynamicFrameLossMonitorTest, StopMonitorFallbackWithoutCallback) {
  // No callback set — should use fallback direct report path
  dynamic_frame->StartMonitor(1);
  dynamic_frame->OnSwapBuffer();
  dynamic_frame->prev_swap_buffer_time_ = 0;
  dynamic_frame->max_app_frametime_ = 100;
  dynamic_frame->StopMonitor();
  EXPECT_FALSE(dynamic_frame->is_monitoring_);
}

}  // namespace ohos
}  // namespace base
