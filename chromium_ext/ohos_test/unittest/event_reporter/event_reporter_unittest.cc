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

#include "arkweb/ohos_nweb/src/sysevent/event_reporter.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>
#include <string>
#include <cstring>

#include "arkweb/ohos_nweb/src/sysevent/oh_web_performance_timing.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/hisysevent_adapter.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

using namespace testing;

namespace content {

class EventReportTest : public testing ::Test {
public: 
    void SetUp() {}
    void TearDown() {}
};

TEST_F(EventReportTest, ReportPageLoadErrorInfo) {
    ReportPageLoadErrorInfo(1, "", 404, 1, "page not found");
    ReportPageLoadErrorInfo(1, "one_error", 404, 1, "page not found");
    ReportPageLoadErrorInfo(1, "one_error", 404, 1, "");
    ReportPageLoadErrorInfo(1, "", 404, 1, "");
}

TEST_F(EventReportTest, ReportAudioPlayErrorInfo) {
    ReportAudioPlayErrorInfo("one_error", 404, "page not found");
    ReportAudioPlayErrorInfo("", 404, "");
    ReportAudioPlayErrorInfo("one_error", 404, "");
    ReportAudioPlayErrorInfo("", 404, "page not found");
}

TEST_F(EventReportTest, ReportVideoPlayErrorInfo) {
    ReportVideoPlayErrorInfo("one_error", 404, "page not found");
    ReportVideoPlayErrorInfo("", 404, "");
    ReportVideoPlayErrorInfo("one_error", 404, "");
    ReportVideoPlayErrorInfo("", 404, "page not found");
}

TEST_F(EventReportTest, ReportWebMediaPlayErrorInfo) {
    ReportWebMediaPlayErrorInfo("one_error", 404, "page not found");
    ReportWebMediaPlayErrorInfo("", 404, "");
    ReportWebMediaPlayErrorInfo("one_error", 404, "");
    ReportWebMediaPlayErrorInfo("", 404, "page not found");
}

TEST_F(EventReportTest, ReportGpuProcessEvent) {
    CrashType type = CrashType::TIMEOUT;
    ReportGpuProcessEvent(type, "TIMEOUT");
    type = CrashType::MAILBOX_NONEXISTENT;
    ReportGpuProcessEvent(type, "MAILBOX_NONEXISTENT");
}

TEST_F(EventReportTest, ReportChildProcessInitFail_Gpu) {
    EXPECT_NO_FATAL_FAILURE(ReportChildProcessInitFail(true, 123));
}

TEST_F(EventReportTest, ReportChildProcessInitFail_Render) {
    EXPECT_NO_FATAL_FAILURE(ReportChildProcessInitFail(false, 456));
}

TEST_F(EventReportTest, ReportRenderJsFreeze) {
    EXPECT_NO_FATAL_FAILURE(ReportRenderJsFreeze(1234, "com.test.package", "test_process", "freeze_msg", 1000));
}

TEST_F(EventReportTest, ReportRenderProcessTerminate_Gpu) {
    EXPECT_NO_FATAL_FAILURE(ReportRenderProcessTerminate(true, 5678, "normal_termination", 0));
}

TEST_F(EventReportTest, ReportRenderProcessTerminate_Render) {
    EXPECT_NO_FATAL_FAILURE(ReportRenderProcessTerminate(false, 5678, "crash", -1));
}

TEST_F(EventReportTest, ReportPageLoadStats) {
    EXPECT_NO_FATAL_FAILURE(ReportPageLoadStats(1, 100, 90, 10));
}

TEST_F(EventReportTest, ReportPageLoadStats_ZeroSumCount) {
    EXPECT_NO_FATAL_FAILURE(ReportPageLoadStats(1, 0, 0, 0));
}

TEST_F(EventReportTest, ReportWebMediaPlayErrorInfo_EmptyStrings) {
    EXPECT_NO_FATAL_FAILURE(ReportWebMediaPlayErrorInfo("", 0, ""));
}

TEST_F(EventReportTest, ReportSkiaOOMError_Empty) {
    EXPECT_NO_FATAL_FAILURE(ReportSkiaOOMError(""));
}

TEST_F(EventReportTest, ReportPageLoadStats_AllSuccess) {
    EXPECT_NO_FATAL_FAILURE(ReportPageLoadStats(2, 50, 50, 0));
}

TEST_F(EventReportTest, ReportPageLoadStats_AllFail) {
    EXPECT_NO_FATAL_FAILURE(ReportPageLoadStats(3, 10, 0, 10));
}

TEST_F(EventReportTest, ReportPageLoadStats_SingleFail) {
    EXPECT_NO_FATAL_FAILURE(ReportPageLoadStats(4, 1, 0, 1));
}

TEST_F(EventReportTest, ReportPageLoadStats_SingleSuccess) {
    EXPECT_NO_FATAL_FAILURE(ReportPageLoadStats(5, 1, 1, 0));
}

TEST_F(EventReportTest, ReportMultiInstanceStats) {
    EXPECT_NO_FATAL_FAILURE(ReportMultiInstanceStats(1, 5, 10));
}

TEST_F(EventReportTest, ReportMultiInstanceStats_MaxReached) {
    EXPECT_NO_FATAL_FAILURE(ReportMultiInstanceStats(2, 10, 10));
}

TEST_F(EventReportTest, ReportJankStats) {
    std::vector<uint16_t> jankData = {1, 2, 3, 4, 5};
    EXPECT_NO_FATAL_FAILURE(ReportJankStats(1234567890LL, 1000, jankData, 1));
}

TEST_F(EventReportTest, ReportJankStats_EmptyVector) {
    std::vector<uint16_t> jankData;
    EXPECT_NO_FATAL_FAILURE(ReportJankStats(1234567890LL, 500, jankData, 2));
}

TEST_F(EventReportTest, ReportJankStats_LargeVector) {
    std::vector<uint16_t> jankData(100, 1);
    EXPECT_NO_FATAL_FAILURE(ReportJankStats(1234567890LL, 2000, jankData, 3));
}

TEST_F(EventReportTest, ReportLockdownModeStatus) {
    EXPECT_NO_FATAL_FAILURE(ReportLockdownModeStatus());
}

TEST_F(EventReportTest, ReportForceZoomEnable) {
    EXPECT_NO_FATAL_FAILURE(ReportForceZoomEnable());
}

TEST_F(EventReportTest, ReportOpenPrivateMode) {
    EXPECT_NO_FATAL_FAILURE(ReportOpenPrivateMode());
}

TEST_F(EventReportTest, ReportPageDownLoadErrorInfo) {
    EXPECT_NO_FATAL_FAILURE(ReportPageDownLoadErrorInfo(12345L, -100));
}

TEST_F(EventReportTest, ReportPageDownLoadErrorInfo_NegativeErrorCode) {
    EXPECT_NO_FATAL_FAILURE(ReportPageDownLoadErrorInfo(99999L, -999));
}

TEST_F(EventReportTest, ReportPageLoadTimeStats) {
    OhWebPerformanceTiming timing;
    timing.navigation_id = 1;
    timing.navigation_start = 1000;
    timing.redirect_count = 0;
    timing.redirect_start = -1;
    timing.redirect_end = -1;
    timing.fetch_start = 2000;
    timing.worker_start = -1;
    timing.domain_lookup_start = 3000;
    timing.domain_lookup_end = 4000;
    timing.connect_start = 5000;
    timing.secure_connect_start = 5500;
    timing.connect_end = 6000;
    timing.request_start = 7000;
    timing.response_start = 8000;
    timing.response_end = 9000;
    timing.dom_interactive = 10000;
    timing.dom_content_loaded_event_start = 11000;
    timing.dom_content_loaded_event_end = 12000;
    timing.load_event_start = 13000;
    timing.load_event_end = 14000;
    timing.first_paint = 11500;
    timing.first_contentful_paint = 11600;
    timing.largest_contentful_paint = 11700;
    timing.render_init_block = 0;
    
    EXPECT_NO_FATAL_FAILURE(ReportPageLoadTimeStats(timing));
}

TEST_F(EventReportTest, ReportPageLoadTimeStats_WithRedirect) {
    OhWebPerformanceTiming timing;
    timing.navigation_id = 2;
    timing.navigation_start = 500;
    timing.redirect_count = 1;
    timing.redirect_start = 600;
    timing.redirect_end = 800;
    timing.fetch_start = 1000;
    timing.first_paint = 5000;
    timing.first_contentful_paint = 5100;
    timing.largest_contentful_paint = 5200;
    timing.render_init_block = 100;
    
    EXPECT_NO_FATAL_FAILURE(ReportPageLoadTimeStats(timing));
}

TEST_F(EventReportTest, ReportAudioFrameDropStats) {
    EXPECT_NO_FATAL_FAILURE(ReportAudioFrameDropStats(10));
}

TEST_F(EventReportTest, ReportAudioFrameDropStats_Zero) {
    EXPECT_NO_FATAL_FAILURE(ReportAudioFrameDropStats(0));
}

TEST_F(EventReportTest, ReportAudioFrameDropStats_HighCount) {
    EXPECT_NO_FATAL_FAILURE(ReportAudioFrameDropStats(1000));
}

TEST_F(EventReportTest, ReportDragDropStatus) {
    EXPECT_NO_FATAL_FAILURE(ReportDragDropStatus("WEB_DRAG_STATUS", 123));
}

TEST_F(EventReportTest, ReportDragDropStatus_DifferentId) {
    EXPECT_NO_FATAL_FAILURE(ReportDragDropStatus("WEB_DRAG_STATUS", 456789));
}

TEST_F(EventReportTest, ReportDuplicateFileUpload_WithData) {
    EXPECT_NO_FATAL_FAILURE(ReportDuplicateFileUpload("same_file_detected"));
}

TEST_F(EventReportTest, ReportDragDropInfo) {
    EXPECT_NO_FATAL_FAILURE(
        ReportDragDropInfo("WEB_DRAG_INFO", 456, "plain_text_content", "https://example.com", "<html>content</html>"));
}

TEST_F(EventReportTest, ReportDragDropInfo_EmptyStrings) {
    EXPECT_NO_FATAL_FAILURE(ReportDragDropInfo("WEB_DRAG_INFO", 789, "", "", ""));
}

TEST_F(EventReportTest, ReportSlideJankStats) {
    EXPECT_NO_FATAL_FAILURE(ReportSlideJankStats(1234567890LL, 16000LL, 60, 5, 500LL, 2));
}

TEST_F(EventReportTest, ReportSlideJankStats_ZeroMissedFrames) {
    EXPECT_NO_FATAL_FAILURE(ReportSlideJankStats(1234567890LL, 16000LL, 60, 0, 166LL, 0));
}

TEST_F(EventReportTest, ReportSlideJankStats_HighMissedFrames) {
    EXPECT_NO_FATAL_FAILURE(ReportSlideJankStats(1234567890LL, 33333LL, 60, 30, 2000LL, 10));
}

TEST_F(EventReportTest, ReportVideoEncodeFormat_H264) {
    EXPECT_NO_FATAL_FAILURE(ReportVideoEncodeFormat("h264"));
}

TEST_F(EventReportTest, ReportVideoEncodeFormat_Vp9) {
    EXPECT_NO_FATAL_FAILURE(ReportVideoEncodeFormat("vp9"));
}

TEST_F(EventReportTest, ReportVideoEncodeFormat_Hevc) {
    EXPECT_NO_FATAL_FAILURE(ReportVideoEncodeFormat("hevc"));
}

TEST_F(EventReportTest, ReportGetAllImage_Success) {
    EXPECT_NO_FATAL_FAILURE(ReportGetAllImage("SUCCESS"));
}

TEST_F(EventReportTest, ReportGetAllImage_Error) {
    EXPECT_NO_FATAL_FAILURE(ReportGetAllImage("ERROR_MEMORY"));
}

TEST_F(EventReportTest, ReportGetAllImage_OtherError) {
    EXPECT_NO_FATAL_FAILURE(ReportGetAllImage("ERROR_TIMEOUT"));
}

TEST_F(EventReportTest, ReportFirstMeaningfulPaintDone) {
    OhWebPerformanceTiming timing;
    timing.navigation_id = 2;
    timing.navigation_start = 1000;
    timing.redirect_count = 0;
    timing.input_time = 500;
    timing.first_paint = 5000;
    timing.first_contentful_paint = 5100;
    timing.first_meaningful_paint = 6000;
    timing.is_paint_done = true;
    
    EXPECT_NO_FATAL_FAILURE(ReportFirstMeaningfulPaintDone(timing));
}

TEST_F(EventReportTest, ReportFirstMeaningfulPaintDone_NotDone) {
    OhWebPerformanceTiming timing;
    timing.navigation_id = 3;
    timing.first_paint = 3000;
    timing.first_contentful_paint = 3100;
    timing.first_meaningful_paint = 4000;
    timing.is_paint_done = false;
    
    EXPECT_NO_FATAL_FAILURE(ReportFirstMeaningfulPaintDone(timing));
}

TEST_F(EventReportTest, ReportAppfreeze) {
    EXPECT_NO_FATAL_FAILURE(
        ReportAppfreeze(9999, "com.test.app", "test_process", "app_freeze_detected", 2000));
}

TEST_F(EventReportTest, ReportAppfreeze_DifferentProcess) {
    EXPECT_NO_FATAL_FAILURE(
        ReportAppfreeze(8888, "com.other.app", "other_process", "freeze detected", 1001));
}

TEST_F(EventReportTest, ReportGpuProcessEvent_Timeout) {
    EXPECT_NO_FATAL_FAILURE(ReportGpuProcessEvent(CrashType::TIMEOUT, "gpu_timeout_event"));
}

TEST_F(EventReportTest, ReportGpuProcessEvent_MailboxNonexistent) {
    EXPECT_NO_FATAL_FAILURE(ReportGpuProcessEvent(CrashType::MAILBOX_NONEXISTENT, "mailbox_not_exist"));
}

TEST_F(EventReportTest, ReportSkiaOOMError_WithData) {
    EXPECT_NO_FATAL_FAILURE(ReportSkiaOOMError("skia_out_of_memory"));
}

TEST_F(EventReportTest, ReportSkiaOOMError_LargeAlloc) {
    EXPECT_NO_FATAL_FAILURE(ReportSkiaOOMError("large_bitmap_alloc_failed"));
}

TEST_F(EventReportTest, ReportRenderProcessNotResponding) {
    EXPECT_NO_FATAL_FAILURE(ReportRenderProcessNotResponding("1234", "high_cpu_usage"));
}

TEST_F(EventReportTest, ReportRenderProcessNotResponding_DifferentReason) {
    EXPECT_NO_FATAL_FAILURE(ReportRenderProcessNotResponding("5678", "memoryPressure"));
}

TEST_F(EventReportTest, ReportRenderProcessResponding) {
    EXPECT_NO_FATAL_FAILURE(ReportRenderProcessResponding());
}

TEST_F(EventReportTest, ReportMetricsLogData_UmaUrl) {
    EXPECT_NO_FATAL_FAILURE(ReportMetricsLogData("48656c6c6f576f726c64", "https://example.com/uma"));
}

TEST_F(EventReportTest, ReportMetricsLogData_UkmUrl) {
    EXPECT_NO_FATAL_FAILURE(ReportMetricsLogData("48656c6c6f576f726c64", "https://example.com/ukm"));
}

TEST_F(EventReportTest, ReportMetricsLogData_OtherUrl) {
    EXPECT_NO_FATAL_FAILURE(ReportMetricsLogData("deadbeef", "https://example.com/other"));
}

TEST_F(EventReportTest, ReportMetricsLogData_EmptyData) {
    EXPECT_NO_FATAL_FAILURE(ReportMetricsLogData("", "https://example.com/uma"));
}

TEST_F(EventReportTest, ReportMetricsLogData_LongData) {
    std::string longData(1000, 'a');
    EXPECT_NO_FATAL_FAILURE(ReportMetricsLogData(longData, "https://example.com/ukm"));
}

}//namespace content