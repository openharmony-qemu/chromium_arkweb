/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <deviceinfo.h>
#include <sstream>
#include "event_reporter.h"

#include "arkweb/build/features/features.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif
#include "arkweb/chromium_ext/third_party/crashpad/crashpad/util/linux/crashpad_dfx.h"
#include "arkweb/chromium_ext/base/ohos/nweb_engine_event_logger.h"
#include "arkweb/chromium_ext/base/ohos/nweb_engine_event_logger_code.h"
#include "arkweb/chromium_ext/url/ohos/log_utils.h"
#include "oh_web_performance_timing.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#if BUILDFLAG(ARKWEB_NWEB_EX)
#include "ohos_nweb_ex/overrides/cef/libcef/browser/alloy/alloy_ark_web_global_config.h"
#endif

using OHOS::NWeb::HiSysEventAdapter;
using OHOS::NWeb::OhosAdapterHelper;
namespace {
// For page load statistics
constexpr char PAGE_LOAD_STATISTICS[] = "PAGE_LOAD_STATISTICS";
constexpr char CURRENT_INSTANCE_ID[] = "INSTANCE_ID";
constexpr char ACCESS_SUM_COUNT[] = "ACCESS_SUM_COUNT";
constexpr char ACCESS_SUCC_COUNT[] = "ACCESS_SUCC_COUNT";
constexpr char ACCESS_FAIL_COUNT[] = "ACCESS_FAIL_COUNT";
constexpr char ACCESS_FAIL_RATIO[] = "ACCESS_FAIL_RATIO";

// For mutiple instance statistics
constexpr char MULTI_INSTANCE_STATISTICS[] = "MULTI_INSTANCE_STATISTICS";
constexpr char CURRENT_INSTANCE_COUNT[] = "INSTANCE_COUNT";
constexpr char INSTANCE_MAX_COUNT[] = "MAX_COUNT";

// For page load error info
constexpr char PAGE_LOAD_ERROR[] = "PAGE_LOAD_ERROR";
constexpr char ERROR_TYPE[] = "ERROR_TYPE";
constexpr char ERROR_CODE[] = "ERROR_CODE";
constexpr char ERROR_DESC[] = "ERROR_DESC";
constexpr char ERROR_COUNT[] = "ERROR_COUNT";

constexpr char JANK_STATS_APP[] = "JANK_STATS_APP";
constexpr char STARTTIME[] = "STARTTIME";
constexpr char DURATION[] = "DURATION";
constexpr char JANK_STATS[] = "JANK_STATS";
constexpr char JANK_STATS_VER[] = "JANK_STATS_VER";

constexpr char RENDER_JIT_LOCKDOWN[] = "RENDER_JIT_LOCKDOWN";
constexpr char LOCKDOWN_MODE_STATUS[] = "JIT_LOCKDOWN_MODE";

// For force zoom enable
constexpr char FORCE_ENABLE_ZOOM[] = "FORCE_ENABLE_ZOOM";
constexpr char ENABLE_FORCE_ZOOM_STATUS[] = "ENABLE_FORCE_ZOOM_STATUS";

// For open private mode
constexpr char OPEN_PRIVATE_MODE[] = "OPEN_PRIVATE_MODE";
constexpr char OPEN_PRIVATE_STATUS[] = "OPEN_PRIVATE_STATUS";

// For page download error info
constexpr char PAGE_DOWNLOAD_ERROR[] = "PAGE_DOWNLOAD_ERROR";
constexpr char DOWNLOAD_ID[] = "DOWNLOAD_ID";

// For media behavior info
constexpr char DRM_ENCRYPTED_PLAYBACK[] = "DRM_ENCRYPTED_PLAYBACK";
constexpr char MEDIA_TYPE[] = "MEDIA_TYPE";
constexpr char DRM_SYSTEM[] = "DRM_SYSTEM";
constexpr char ENCRYPTION_ALGORITHM[] = "ENCRYPTION_ALGORITHM";

constexpr char VIDEO_ENCODE_FORMAT[] = "VIDEO_ENCODE_FORMAT";
constexpr char ENCODE_FORMAT[] = "ENCODE_FORMAT";

constexpr char FFMPEG_CODEC_OPERATION[] = "FFMPEG_CODEC_OPERATION";
constexpr char CODEC_TYPE[] = "CODEC_TYPE";
constexpr char FORMAT[] = "FORMAT";

constexpr char AUDIO_HARDWARE_DECODE[] = "AUDIO_HARDWARE_DECODE";
constexpr char CODEC_FORMAT[] = "CODEC_FORMAT";

constexpr char PICTURE_DECODE[] = "PICTURE_DECODE";
constexpr char PICTURE_TYPE[] = "PICTURE_TYPE";

// For GetImageInfos
constexpr char GET_IMAGES_BY_IDS[] = "WEB_MSDP_SCENE_ERROR";
constexpr char GET_IMAGES_BY_IDS_EVENT_TYPE[] = "EVENT_TYPE";
constexpr char GET_IMAGES_BY_IDS_ERROR_INFO[] = "ERROR_INFO";

// For audio/video error info
constexpr char AUDIO_PLAY_ERROR[] = "AUDIO_PLAY_ERROR";
constexpr char VIDEO_PLAY_ERROR[] = "VIDEO_PLAY_ERROR";

// For audio/video frame dropped statistics
constexpr char AUDIO_FRAME_DROP_STATISTICS[] = "AUDIO_FRAME_DROP_STATISTICS";
constexpr char AUDIO_BLANK_FRAME_COUNT[] = "AUDIO_BLANK_FRAME_COUNT";
constexpr char VIDEO_FRAME_DROP_STATISTICS[] = "VIDEO_FRAME_DROP_STATISTICS";
constexpr char VIDEO_FRAME_DROPPED_COUNT[] = "VIDEO_FRAME_DROPPED_COUNT";
constexpr char VIDEO_FRAME_DROPPED_DURATION[] = "VIDEO_FRAME_DROPPED_DURATION";

// For duplicate file upload statistics
constexpr char DUPLICATE_FILE_UPLOAD[] = "DUPLICATE_FILE_UPLOAD";

constexpr char NWEB_ID[] = "NWEB_ID";
constexpr char PLAIN_TEXT[] = "PLAIN_TEXT";
constexpr char LINK_URL[] = "LINK_URL";
constexpr char HTML[] = "HTML";

constexpr char DYNAMIC_FRAME_DROP_STATISTICS[] =
    "DYNAMIC_FRAME_DROP_STATISTICS";
constexpr char TOTAL_APP_FRAMES[] = "TOTAL_APP_FRAMES";
constexpr char TOTAL_APP_MISSED_FRAMES[] = "TOTAL_APP_MISSED_FRAMES";
constexpr char MAX_APP_FRAMETIME[] = "MAX_APP_FRAMETIME";
constexpr char MAX_APP_SEQ_MISSSED_FRAMES[] = "MAX_APP_SEQ_MISSSED_FRAMES";

// For site_isolation
constexpr char SITE_ISOLATION_MODE[] = "SITE_ISOLATION_MODE";
constexpr char SITE_ISOLATION_STATUS[] = "SITE_ISOLATION_STATUS";

// For renderer memory statistics
constexpr char PAGE_MEM_LEAK[] = "PAGE_MEM_LEAK";
constexpr char BASIC_RENDER_MEM[] = "BASIC_RENDER_MEM";
constexpr char BASIC_BROWSER_MEM[] = "BASIC_BROWSER_MEM";
constexpr char TYPE[] = "TYPE";
constexpr char PID[] = "PID";
constexpr char RSS[] = "RSS";
constexpr char PSS[] = "PSS";
constexpr char SWAP_PSS[] = "SWAP_PSS";
constexpr char FD_NUM[] = "FD_NUM";
constexpr char OOM_SCORE_ADJ[] = "OOM_SCORE_ADJ";
constexpr char JS_HEAP_TOTAL[] = "JS_HEAP_TOTAL";
constexpr char JS_HEAP_USED[] = "JS_HEAP_USED";
constexpr char PA[] = "PA";
constexpr char GPU_MEM[] = "GPU_MEM";
constexpr char URL[] = "URL";

constexpr char CHILD_PROCESS_INIT_FAIL[] = "CHILD_PROCESS_INIT_FAIL";
constexpr char PROCESS_TYPE[] = "PROCESS_TYPE";
constexpr char RENDER_JS_FREEZE[] = "RENDER_JS_FREEZE";

// For web play error info,such as pip/drm
constexpr char WEB_MEDIA_PLAY_ERROR[] = "WEB_MEDIA_PLAY_ERROR";
 
// For av session
constexpr char WEB_AV_SESSION_DISABLE[] = "WEB_AV_SESSION_DISABLE";
constexpr char DISABLE_WEB_AV_SESSION_STATUS[] = "DISABLE_WEB_AV_SESSION_STATUS";

constexpr char PAGE_DRAG_BLANK[] = "PAGE_DRAG_BLANK";
constexpr char PAGE_BLANK_TIME[] = "PAGE_BLANK_TIME";

const char GPU_DISPLAY_ERROR[] = "GPU_DISPLAY_ERROR";
const char EVENT_TYPE[] = "EVENT_TYPE";
const char EVENT_CONTENT[] = "EVENT_CONTENT";

constexpr char TIMEOUT[] = "TIMEOUT";
constexpr char MAILBOX_NONEXISTENT[] = "MAILBOX_NONEXISTENT";

constexpr char RENDER_INIT_BLOCK[] = "RENDER_INIT_BLOCK";
constexpr char BLOCK_TIME[] = "BLOCK_TIME";

// For render freeze monitoring
constexpr char PROCESS_FREEZE_WARNING[] = "PROCESS_FREEZE_WARNING";
// For skia out of memory error
constexpr char SKIA_OOM_ERROR[] = "SKIA_OOM_ERROR";

constexpr char RENDER_PROCESS_TERMINATE[] = "RENDER_PROCESS_TERMINATE";
constexpr char RENDER_PROCESS_NOTRESPONDING[] = "RENDER_PROCESS_NOTRESPONDING";
constexpr char RENDER_PROCESS_RESPONDING[] = "RENDER_PROCESS_RESPONDING";

constexpr char BUNDLE_NAME[] = "BUNDLE_NAME";
constexpr char UNIVERSAL_ACCESS_BLOCKED[] = "UNIVERSAL_ACCESS_BLOCKED";

// for uma/ukm log report
constexpr char HISTOGRAM_UMA_UKM[] = "HISTOGRAM_UMA_UKM";
constexpr char HISTOGRAM_TYPE[] = "HISTOGRAM_TYPE";
constexpr char HISTOGRAM_CONTENT[] = "HISTOGRAM_CONTENT";

constexpr char CACHE_DATA_SEPARATE_STATISTICS[] = "CACHE_DATA_SEPARATE_STATISTICS";
constexpr char CACHE_DATA_MIGRATE_STATISTICS[] = "CACHE_DATA_MIGRATE_STATISTICS";

#if BUILDFLAG(ARKWEB_NWEB_EX)
bool ShouldReportByPerformanceSampleRate() {
  int sample_rate =
      nweb_ex::AlloyArkWebGlobalConfig::GetInstance()->GetPerformanceSampleRate();
  if (sample_rate <= 0 || sample_rate > 10000) {
    return false;
  }
  int sample_random_value = base::RandInt(0, 9999);
  return sample_random_value < sample_rate;
}

bool ShouldReportByProcessSampleRate() {
  int sample_rate =
      nweb_ex::AlloyArkWebGlobalConfig::GetInstance()->GetProcessSampleRate();
  if (sample_rate <= 0 || sample_rate > 10000) {
    return false;
  }
  int sample_random_value = base::RandInt(0, 9999);
  return sample_random_value < sample_rate;
}

bool ShouldReportByJankSampleRate() {
  int sample_rate =
      nweb_ex::AlloyArkWebGlobalConfig::GetInstance()->GetJankSampleRate();
  if (sample_rate <= 0 || sample_rate > 10000) {
    return false;
  }
  int sample_random_value = base::RandInt(0, 9999);
  return sample_random_value < sample_rate;
}

const char* ConnectionTypeToString(net::NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:   return "UNKNOWN";
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:  return "ETHERNET";
    case net::NetworkChangeNotifier::CONNECTION_WIFI:      return "WIFI";
    case net::NetworkChangeNotifier::CONNECTION_2G:        return "2G";
    case net::NetworkChangeNotifier::CONNECTION_3G:        return "3G";
    case net::NetworkChangeNotifier::CONNECTION_4G:        return "4G";
    case net::NetworkChangeNotifier::CONNECTION_NONE:      return "NONE";
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH: return "BLUETOOTH";
    case net::NetworkChangeNotifier::CONNECTION_5G:        return "5G";
    default: return "UNKNOWN";
  }
}

const char* EffectiveConnectionTypeToString(net::EffectiveConnectionType type) {
  switch (type) {
    case net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN:   return "UNKNOWN";
    case net::EFFECTIVE_CONNECTION_TYPE_OFFLINE:   return "OFFLINE";
    case net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G:   return "SLOW_2G";
    case net::EFFECTIVE_CONNECTION_TYPE_2G:        return "2G";
    case net::EFFECTIVE_CONNECTION_TYPE_3G:        return "3G";
    case net::EFFECTIVE_CONNECTION_TYPE_4G:        return "4G";
    default: return "UNKNOWN";
  }
}

std::string BuildPageLoadTimeStatsMessage(const OhWebPerformanceTiming& timing,
                                          const std::string& masked_url) {
  std::ostringstream ostr;
  ostr << "navigation_id: " << timing.navigation_id
       << ", navigation_start: " << timing.actual_navigation_start
       << ", redirect_count: " << timing.redirect_count
       << ", redirect_start: " << timing.redirect_start
       << ", redirect_end: " << timing.redirect_end
       << ", fetch_start: " << timing.fetch_start
       << ", worker_start: " << timing.worker_start
       << ", domain_lookup_start: " << timing.domain_lookup_start
       << ", domain_lookup_end: " << timing.domain_lookup_end
       << ", connect_start: " << timing.connect_start
       << ", secure_connect_start: " << timing.secure_connect_start
       << ", connect_end: " << timing.connect_end
       << ", request_start: " << timing.request_start
       << ", response_start: " << timing.response_start
       << ", response_end: " << timing.response_end
       << ", dom_interactive: " << timing.dom_interactive
       << ", dom_content_loaded_event_start: " << timing.dom_content_loaded_event_start
       << ", dom_content_loaded_event_end: " << timing.dom_content_loaded_event_end
       << ", load_event_start: " << timing.load_event_start
       << ", load_event_end: " << timing.load_event_end
       << ", first_paint: " << timing.first_paint
       << ", first_contentful_paint: " << timing.first_contentful_paint
       << ", largest_contentful_paint: " << timing.largest_contentful_paint
       << ", render_init_block: " << timing.render_init_block
       << ", first_input_delay: " << timing.first_input_delay
       << ", total_web_contents: " << timing.total_web_contents
       << ", is_ssl_cert_error: " << timing.is_ssl_cert_error
       << ", current_connection_type: "
       << ConnectionTypeToString(timing.current_connection_type)
       << ", effective_connection_type: "
       << EffectiveConnectionTypeToString(timing.effective_connection_type)
       << ", url: " << masked_url;
  return ostr.str();
}
#endif  // ARKWEB_NWEB_EX

}  // namespace

void ReportChildProcessInitFail(bool is_gpu, int err) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      CHILD_PROCESS_INIT_FAIL, HiSysEventAdapter::EventType::FAULT,
      {PROCESS_TYPE, is_gpu ? std::string("gpu") : std::string("render"),
      ERROR_CODE, std::to_string(err)});
#if BUILDFLAG(ARKWEB_NWEB_EX)
  if (ShouldReportByProcessSampleRate()) {
    std::ostringstream ostr;
    ostr << "process_type: " << (is_gpu ? "gpu" : "render")
         << ", error_code: " << err;
    base::ohos::ReportEngineEvent(base::ohos::kModuleComponent,
                                  base::ohos::kDefaultUrl,
                                  base::ohos::kChildProcessInitFail,
                                  ostr.str());
    LOG(INFO) << "event_message[ChildProcessInitFail]: " << ostr.str();
  }
#endif  // ARKWEB_NWEB_EX
}

void ReportRenderJsFreeze(int32_t pid, const std::string& packageName, const std::string& processName,
                          const std::string& freezeMsg, int32_t uid) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      RENDER_JS_FREEZE, HiSysEventAdapter::EventType::FAULT,
      {
        "PID", pid,
        "PACKAGE_NAME", packageName,
        "PROCESS_NAME", processName,
        "MSG", freezeMsg,
        "UID", uid
      });
#if BUILDFLAG(ARKWEB_NWEB_EX)
  if (ShouldReportByProcessSampleRate()) {
    std::ostringstream ostr;
    ostr << "pid: " << pid
         << ", package_name: " << packageName
         << ", process_name: " << processName
         << ", msg: " << freezeMsg
         << ", uid: " << uid;
    base::ohos::ReportEngineEvent(base::ohos::kModuleContentRender,
                                  base::ohos::kDefaultUrl,
                                  base::ohos::kRenderJsFreeze,
                                  ostr.str());
    LOG(INFO) << "event_message[RenderJsFreeze]: " << ostr.str();
  }
#endif  // ARKWEB_NWEB_EX
}

void ReportRenderProcessTerminate(bool is_gpu, int32_t pid, const std::string& reason, int32_t error) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      RENDER_PROCESS_TERMINATE, HiSysEventAdapter::EventType::FAULT,
      {
        "PROCESS_TYPE", is_gpu ? std::string("gpu") : std::string("render"),
        "PID", std::to_string(pid),
        "REASON", reason,
        "ERROR", std::to_string(error)
      });
#if BUILDFLAG(ARKWEB_NWEB_EX)
  if (ShouldReportByProcessSampleRate()) {
    std::ostringstream ostr;
    ostr << "process_type: " << (is_gpu ? "gpu" : "render")
         << ", pid: " << pid
         << ", reason: " << reason
         << ", error: " << error;
    base::ohos::ReportEngineEvent(base::ohos::kModuleContentRender,
                                  base::ohos::kDefaultUrl,
                                  base::ohos::kRenderProcessTerminate,
                                  ostr.str());
    LOG(INFO) << "event_message[RenderProcessTerminate]: " << ostr.str();
  }
#endif  // ARKWEB_NWEB_EX
}

void ReportPageLoadStats(int instanceId,
                         int accessSumCount,
                         int accessSuccCount,
                         int accessFailCount) {
  float failRatio = float(accessFailCount) / accessSumCount;
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      PAGE_LOAD_STATISTICS, HiSysEventAdapter::EventType::STATISTIC,
      {CURRENT_INSTANCE_ID, std::to_string(instanceId), ACCESS_SUM_COUNT,
       std::to_string(accessSumCount), ACCESS_SUCC_COUNT,
       std::to_string(accessSuccCount), ACCESS_FAIL_COUNT,
       std::to_string(accessFailCount), ACCESS_FAIL_RATIO,
       std::to_string(failRatio)});
}

void ReportMultiInstanceStats(int instanceId, int nwebCount, int nwebMaxCount) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      MULTI_INSTANCE_STATISTICS, HiSysEventAdapter::EventType::STATISTIC,
      {CURRENT_INSTANCE_ID, std::to_string(instanceId), CURRENT_INSTANCE_COUNT,
       std::to_string(nwebCount), INSTANCE_MAX_COUNT,
       std::to_string(nwebMaxCount)});
}

void ReportPageLoadErrorInfo(int instanceId,
                             const std::string errorType,
                             int errorCode,
                             uint32_t errorCount,
                             const std::string errorDesc) {
  std::string error_type = "";
  std::string error_desc = "";
  int error_code = errorCode;
  if (errorType != "") {
    error_type = errorType;
  }
  if (errorDesc != "") {
    error_desc = errorDesc;
  }
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      PAGE_LOAD_ERROR, HiSysEventAdapter::EventType::FAULT,
      {CURRENT_INSTANCE_ID, std::to_string(instanceId), ERROR_TYPE, error_type, ERROR_CODE, std::to_string(error_code),
       ERROR_COUNT, std::to_string(errorCount), ERROR_DESC, error_desc});
}

void ReportJankStats(int64_t startTime,
                     int duration,
                     std::vector<uint16_t> jankStats,
                     int jankStatsVer,
                     const GURL& url) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      JANK_STATS_APP, HiSysEventAdapter::EventType::STATISTIC,
      {STARTTIME, startTime, DURATION, duration, JANK_STATS, jankStats,
       JANK_STATS_VER, jankStatsVer});
#if BUILDFLAG(ARKWEB_NWEB_EX)
  if (ShouldReportByJankSampleRate()) {
    std::string masked_url = url::LogUtils::ConvertUrl(url.spec(), true);
    std::ostringstream ostr;
    ostr << "start_time: " << startTime
         << ", duration: " << duration
         << ", jank_stats_ver: " << jankStatsVer
         << ", url: " << masked_url;
    base::ohos::ReportEngineEvent(base::ohos::kModuleComponent,
                                  url.is_empty() ? base::ohos::kDefaultUrl : url.GetHost(),
                                  base::ohos::kJankStats,
                                  ostr.str());
    LOG(INFO) << "event_message[JankStats]: " << ostr.str();
  }
#endif  // ARKWEB_NWEB_EX
}

void ReportLockdownModeStatus(void) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      RENDER_JIT_LOCKDOWN, HiSysEventAdapter::EventType::BEHAVIOR,
      {LOCKDOWN_MODE_STATUS, "true"});
}

void ReportForceZoomEnable(void) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      FORCE_ENABLE_ZOOM, HiSysEventAdapter::EventType::BEHAVIOR,
      {ENABLE_FORCE_ZOOM_STATUS, "true"});
}

void ReportOpenPrivateMode(void) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      OPEN_PRIVATE_MODE, HiSysEventAdapter::EventType::BEHAVIOR,
      {OPEN_PRIVATE_STATUS, "true"});
}

void ReportPageDownLoadErrorInfo(long downloadId, int errorCode) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      PAGE_DOWNLOAD_ERROR, HiSysEventAdapter::EventType::FAULT,
      {DOWNLOAD_ID, std::to_string(downloadId), ERROR_CODE,
       std::to_string(errorCode)});
}

void ReportPageLoadTimeStats(OhWebPerformanceTiming loadPageTime, const GURL& url) {
  const std::string input = "NAVIGATION_ID" + std::to_string(loadPageTime.navigation_id) +
    "NAVIGATION_START" + std::to_string(loadPageTime.navigation_start) +
    "REDIRECT_COUNT" + std::to_string(loadPageTime.redirect_count) +
    "REDIRECT_START" + std::to_string(loadPageTime.redirect_start) +
    "REDIRECT_END" + std::to_string(loadPageTime.redirect_end) +
    "FETCH_START" + std::to_string(loadPageTime.fetch_start) +
    "WORKER_START" + std::to_string(loadPageTime.worker_start) +
    "DOMAIN_LOOKUP_START" + std::to_string(loadPageTime.domain_lookup_start) +
    "DOMAIN_LOOKUP_END" + std::to_string(loadPageTime.domain_lookup_end) +
    "CONNECT_START" + std::to_string(loadPageTime.connect_start) +
    "SECURE_CONNECT_START" + std::to_string(loadPageTime.secure_connect_start) +
    "CONNECT_END" + std::to_string(loadPageTime.connect_end) +
    "REQUEST_START" + std::to_string(loadPageTime.request_start) +
    "RESPONSE_START" + std::to_string(loadPageTime.response_start) +
    "RESPONSE_END" + std::to_string(loadPageTime.response_end) +
    "DOM_INTERACTIVE" + std::to_string(loadPageTime.dom_interactive) +
    "DOM_CONTENT_LOADED_EVENT_START" + std::to_string(loadPageTime.dom_content_loaded_event_start) +
    "DOM_CONTENT_LOADED_EVENT_END" + std::to_string(loadPageTime.dom_content_loaded_event_end) +
    "LOAD_EVENT_START" + std::to_string(loadPageTime.load_event_start) +
    "LOAD_EVENT_END" + std::to_string(loadPageTime.load_event_end) +
    "FIRST_PAINT" + std::to_string(loadPageTime.first_paint) +
    "FIRST_CONTENTFUL_PAINT" + std::to_string(loadPageTime.first_contentful_paint) +
    "LARGEST_CONTENTFUL_PAINT" + std::to_string(loadPageTime.largest_contentful_paint) +
    "RENDER_INIT_BLOCK" + std::to_string(loadPageTime.render_init_block);

  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      "PAGE_LOAD_TIME", HiSysEventAdapter::EventType::STATISTIC,{input, ""});
#if BUILDFLAG(ARKWEB_NWEB_EX)
  if (ShouldReportByPerformanceSampleRate()) {
    std::string masked_url = url::LogUtils::ConvertUrl(url.spec(), true);
    std::string message = BuildPageLoadTimeStatsMessage(loadPageTime, masked_url);
    base::ohos::ReportEngineEvent(base::ohos::kModuleNet,
                                  url.is_empty() ? base::ohos::kDefaultUrl : url.GetHost(),
                                  base::ohos::kPageLoadTimeStats,
                                  message);
    LOG(INFO) << "event_message[PageLoadTimeStats]: " << message;
  }
#endif  // ARKWEB_NWEB_EX
}

void ReportAudioPlayErrorInfo(const std::string errorType,
                              int errorCode,
                              const std::string errorDesc) {
  std::string error_type = "";
  std::string error_desc = "";
  int error_code = errorCode;
  if (!errorType.empty()) {
    error_type = errorType;
  }
  if (!errorDesc.empty()) {
    error_desc = errorDesc;
  }
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      AUDIO_PLAY_ERROR, HiSysEventAdapter::EventType::FAULT,
      {ERROR_TYPE, error_type, ERROR_CODE, std::to_string(error_code),
       ERROR_DESC, error_desc});
}

void ReportVideoPlayErrorInfo(const std::string errorType,
                              int errorCode,
                              const std::string errorDesc) {
  std::string error_type = "";
  std::string error_desc = "";
  int error_code = errorCode;
  if (!errorType.empty()) {
    error_type = errorType;
  }
  if (!errorDesc.empty()) {
    error_desc = errorDesc;
  }
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      VIDEO_PLAY_ERROR, HiSysEventAdapter::EventType::FAULT,
      {ERROR_TYPE, error_type, ERROR_CODE, std::to_string(error_code),
       ERROR_DESC, error_desc});
}

void ReportAudioFrameDropStats(int frameCount) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      AUDIO_FRAME_DROP_STATISTICS, HiSysEventAdapter::EventType::STATISTIC,
      {AUDIO_BLANK_FRAME_COUNT, frameCount});
}

void ReportVideoFrameDropStats(uint32_t frameCount, uint64_t frameDuration) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      VIDEO_FRAME_DROP_STATISTICS, HiSysEventAdapter::EventType::STATISTIC,
      {VIDEO_FRAME_DROPPED_COUNT, frameCount, VIDEO_FRAME_DROPPED_DURATION,
       frameDuration});
}
void ReportDragDropStatus(const std::string& eventName, int32_t id) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      eventName, HiSysEventAdapter::EventType::BEHAVIOR,
      {NWEB_ID, std::to_string(id)});
}

void ReportDuplicateFileUpload(const std::string errorDesc) {
  std::string error_desc = "";
  if (!errorDesc.empty()) {
    error_desc = errorDesc;
  }
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      DUPLICATE_FILE_UPLOAD, HiSysEventAdapter::EventType::STATISTIC,
      {ERROR_DESC, error_desc});
}

void ReportDragDropInfo(const std::string& eventName,
                        int32_t id,
                        const std::string& fragment,
                        const std::string& linkUrl,
                        const std::string& linkHtml) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      eventName, HiSysEventAdapter::EventType::BEHAVIOR,
      {NWEB_ID, std::to_string(id), PLAIN_TEXT, std::to_string(fragment.size()),
       LINK_URL, std::to_string(linkUrl.size()), HTML,
       std::to_string(linkHtml.size())});
}

void ReportSlideJankStats(int64_t startTime,
                          int64_t duration,
                          int32_t totalAppFrames,
                          int32_t totalAppMissedFrames,
                          int64_t maxAppFrametime,
                          int32_t maxAppSeqMissedFrames) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      DYNAMIC_FRAME_DROP_STATISTICS, HiSysEventAdapter::EventType::STATISTIC,
      {STARTTIME, startTime, DURATION, duration, TOTAL_APP_FRAMES,
       totalAppFrames, TOTAL_APP_MISSED_FRAMES, totalAppMissedFrames,
       MAX_APP_FRAMETIME, maxAppFrametime, MAX_APP_SEQ_MISSSED_FRAMES,
       maxAppSeqMissedFrames});
#if BUILDFLAG(ARKWEB_NWEB_EX)
  if (ShouldReportByJankSampleRate()) {
    std::ostringstream ostr;
    ostr << "start_time: " << startTime
         << ", duration: " << duration
         << ", total_app_frames: " << totalAppFrames
         << ", total_app_missed_frames: " << totalAppMissedFrames
         << ", max_app_frametime: " << maxAppFrametime
         << ", max_app_seq_missed_frames: " << maxAppSeqMissedFrames;
    base::ohos::ReportEngineEvent(base::ohos::kModuleComponent,
                                  base::ohos::kDefaultUrl,
                                  base::ohos::kSlideJankStats,
                                  ostr.str());
    LOG(INFO) << "event_message[SlideJankStats]: " << ostr.str();
  }
#endif  // ARKWEB_NWEB_EX
}

void ReportSiteIsolationMode(const std::string site_isolation_status) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      SITE_ISOLATION_MODE, HiSysEventAdapter::EventType::BEHAVIOR,
      {SITE_ISOLATION_STATUS, site_isolation_status});
}

void ReportDrmEncryptedPlayback(const std::string& mediaType,
                                const std::string& drmSystem,
                                const std::string& encryptedAlgo) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      DRM_ENCRYPTED_PLAYBACK, HiSysEventAdapter::EventType::BEHAVIOR,
      {MEDIA_TYPE, mediaType,
       DRM_SYSTEM, drmSystem,
       ENCRYPTION_ALGORITHM, encryptedAlgo});
}

void ReportVideoEncodeFormat(const std::string& encodeFormat) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      VIDEO_ENCODE_FORMAT, HiSysEventAdapter::EventType::BEHAVIOR,
      {ENCODE_FORMAT, encodeFormat});
}

void ReportFfmpegCodecOperation(const std::string& codecType, const std::string& format) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      FFMPEG_CODEC_OPERATION, HiSysEventAdapter::EventType::BEHAVIOR,
      {CODEC_TYPE, codecType,
       FORMAT, format});
}

void ReportAudioHardwareDecode(const std::string& codecFormat) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      AUDIO_HARDWARE_DECODE, HiSysEventAdapter::EventType::BEHAVIOR,
      {CODEC_FORMAT, codecFormat});
}

void ReportPictureDecode(const std::string& pictureType) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      PICTURE_DECODE, HiSysEventAdapter::EventType::BEHAVIOR,
      {PICTURE_TYPE, pictureType});
}  

void ReportGetAllImage(const std::string& error_code) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      GET_IMAGES_BY_IDS, HiSysEventAdapter::EventType::FAULT,
      {GET_IMAGES_BY_IDS_EVENT_TYPE, "GET_IMAGES_BY_IDS_EVENT",
       GET_IMAGES_BY_IDS_ERROR_INFO, error_code});
}

void ReportWebMediaPlayErrorInfo(const std::string& errorType,
                              int errorCode,
                              const std::string& errorDesc) {
  std::string error_type = "";
  std::string error_desc = "";
  int error_code = errorCode;
  if (!errorType.empty()) {
    error_type = errorType;
  }
  if (!errorDesc.empty()) {
    error_desc = errorDesc;
  }
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      WEB_MEDIA_PLAY_ERROR, HiSysEventAdapter::EventType::FAULT,
      {ERROR_TYPE, error_type, ERROR_CODE, std::to_string(error_code),
       ERROR_DESC, error_desc});
}
 
namespace {
constexpr const char* const KEY_HIVIEW_USER_TYPE = "const.logsystem.versiontype";

bool IsReleaseVersion(){
  const char* osReleaseType = OH_GetOsReleaseType();
  const char* distributionReleaseType = OH_GetDistributionOSReleaseType();
  auto IsNullOrEmpty = [](const char* str) {
    return str == nullptr || str[0] == '\0';
  };
  auto IsRelease = [](const char* str) {
    return str != nullptr && strcmp(str, "Release") == 0;
  };
  return IsRelease(osReleaseType) || IsRelease(distributionReleaseType) ||
      (IsNullOrEmpty(osReleaseType) && IsNullOrEmpty(distributionReleaseType));
}
bool IsBetaVersion() {
  std::string userType =
      OHOS::NWeb::OhosAdapterHelper::GetInstance().GetSystemPropertiesInstance().GetStringParameter(
          KEY_HIVIEW_USER_TYPE, "");
  return userType == "beta";
}
}  // namespace

void ReportAvSessionStatus(const std::string& disable_web_av_session_status) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      WEB_AV_SESSION_DISABLE, HiSysEventAdapter::EventType::STATISTIC,
      {DISABLE_WEB_AV_SESSION_STATUS, disable_web_av_session_status});
}

// The image of the release version does not report the URL, which poses privacy and security risks.
void ReportRendererMem(const std::string& type,
                       const std::string& pid,
                       const std::string& rss,
                       const std::string& pss,
                       const std::string& js_heap_total,
                       const std::string& js_heap_used,
                       const std::string& gpu_mem,
                       const std::string& url)
{
  bool isReleaseBetaVersion = IsReleaseVersion() && (!IsBetaVersion());
  if (isReleaseBetaVersion) {
    OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
        PAGE_MEM_LEAK, HiSysEventAdapter::EventType::STATISTIC,
        {TYPE, type, PID, pid, RSS, rss, PSS, pss, JS_HEAP_TOTAL, js_heap_total,
         JS_HEAP_USED, js_heap_used, GPU_MEM, gpu_mem});
  } else {
    OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
        PAGE_MEM_LEAK, HiSysEventAdapter::EventType::STATISTIC,
        {TYPE, type, PID, pid, RSS, rss, PSS, pss, JS_HEAP_TOTAL, js_heap_total,
         JS_HEAP_USED, js_heap_used, GPU_MEM, gpu_mem, URL, url});
  }
}

void ReportBasicRendererMem(const std::string& pid,
                       const std::string& rss,
                       const std::string& pss,
                       const std::string& swap_pss,
                       const std::string& fd_num,
                       const std::string& oom_score_adj,
                       const std::string& js_heap_total,
                       const std::string& js_heap_used,
                       const std::string& pa,
                       const std::string& gpu_mem)
{
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      BASIC_RENDER_MEM, HiSysEventAdapter::EventType::STATISTIC,
      {PID, pid, RSS, rss, PSS, pss, SWAP_PSS, swap_pss, FD_NUM, fd_num,
       OOM_SCORE_ADJ, oom_score_adj, JS_HEAP_TOTAL, js_heap_total,
       JS_HEAP_USED, js_heap_used, PA, pa, GPU_MEM, gpu_mem});
}
 
void ReportBasicBrowserMem(const std::string& pid,
                       const std::string& rss,
                       const std::string& pss,
                       const std::string& swap_pss,
                       const std::string& fd_num,
                       const std::string& oom_score_adj,
                       const std::string& pa)
{
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      BASIC_BROWSER_MEM, HiSysEventAdapter::EventType::STATISTIC,
      {PID, pid, RSS, rss, PSS, pss, SWAP_PSS, swap_pss, FD_NUM, fd_num,
       OOM_SCORE_ADJ, oom_score_adj, PA, pa});
}

void ReportDragBlank(int64_t duration) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      PAGE_DRAG_BLANK, HiSysEventAdapter::EventType::STATISTIC,
      {PAGE_DRAG_BLANK, std::to_string(duration)});
}

void ReportFirstMeaningfulPaintDone(OhWebPerformanceTiming loadPageTime) {
  const std::string input = "NAVIGATION_ID" + std::to_string(loadPageTime.navigation_id) +
    "NAVIGATION_START" + std::to_string(loadPageTime.navigation_start) +
    "REDIRECT_COUNT" + std::to_string(loadPageTime.redirect_count) +
    "INPUT_TIME" + std::to_string(loadPageTime.input_time) +
    "FIRST_PAINT" + std::to_string(loadPageTime.first_paint) +
    "FIRST_CONTENTFUL_PAINT" + std::to_string(loadPageTime.first_contentful_paint) +
    "FIRST_MEANINGFUL_PAINT" + std::to_string(loadPageTime.first_meaningful_paint) +
    "IS_PAINT_DONE" + std::to_string(loadPageTime.is_paint_done);

  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      "FIRST_MEANINGFUL_PAINT_DONE", HiSysEventAdapter::EventType::STATISTIC,{input, ""});
}

void ReportAppfreeze(int32_t pid, const std::string& packageName, const std::string& processName,
                     const std::string& freezeMsg, int32_t uid) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      PROCESS_FREEZE_WARNING, HiSysEventAdapter::EventType::FAULT,
      {
        "PID", pid,
        "PACKAGE_NAME", packageName,
        "PROCESS_NAME", processName,
        "MSG", freezeMsg,
        "UID", uid
      });
}

void ReportGpuProcessEvent(CrashType type, std::string eventcontent) {
  switch(type) {
    case CrashType::TIMEOUT:
      OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
        GPU_DISPLAY_ERROR, HiSysEventAdapter::EventType::STATISTIC,
        {EVENT_TYPE, TIMEOUT, EVENT_CONTENT, eventcontent});
      break;
    case CrashType::MAILBOX_NONEXISTENT:
      OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
        GPU_DISPLAY_ERROR, HiSysEventAdapter::EventType::STATISTIC,
        {EVENT_TYPE, MAILBOX_NONEXISTENT, EVENT_CONTENT, eventcontent});
      break;
  }
}

void ReportSkiaOOMError(const std::string errorDesc) {
  std::string error_desc = "";
  if (!errorDesc.empty()) {
    error_desc = errorDesc;
  }
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      SKIA_OOM_ERROR, HiSysEventAdapter::EventType::FAULT,
      {ERROR_DESC, error_desc});
#if BUILDFLAG(ARKWEB_NWEB_EX)
  if (ShouldReportByProcessSampleRate()) {
    std::ostringstream ostr;
    ostr << "error_desc: " << error_desc;
    base::ohos::ReportEngineEvent(base::ohos::kModuleComponent,
                                  base::ohos::kDefaultUrl,
                                  base::ohos::kSkiaOOMError,
                                  ostr.str());
    LOG(INFO) << "event_message[SkiaOOMError]: " << ostr.str();
  }
#endif  // ARKWEB_NWEB_EX
}

void ReportRenderProcessNotResponding(const std::string&pid, const std::string&reason, const GURL& url) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      RENDER_PROCESS_NOTRESPONDING, HiSysEventAdapter::EventType::FAULT,
      {
        "PID", pid,
        "REASON", reason
      });
#if BUILDFLAG(ARKWEB_NWEB_EX)
  if (ShouldReportByProcessSampleRate()) {
    std::string masked_url = url::LogUtils::ConvertUrl(url.spec(), true);
    std::ostringstream ostr;
    ostr << "pid: " << pid
         << ", reason: " << reason
         << ", url: " << masked_url;
    base::ohos::ReportEngineEvent(base::ohos::kModuleContentRender,
                                  url.is_empty() ? base::ohos::kDefaultUrl : url.GetHost(),
                                  base::ohos::kRenderProcessNotResponding,
                                  ostr.str());
    LOG(INFO) << "event_message[RenderProcessNotResponding]: " << ostr.str();
  }
#endif  // ARKWEB_NWEB_EX
}

void ReportRenderProcessResponding() {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      RENDER_PROCESS_RESPONDING, HiSysEventAdapter::EventType::BEHAVIOR,
      {
        "RESPONDING_STATUS", "true"
      });
}

void ReportMetricsLogData(const std::string& compressed_log_data, const std::string& url) {
  if (IsReleaseVersion() && (url.find("ukm") != std::string::npos)) {
    LOG(ERROR)<<"release version!";
    return;
  }
  std::string_view log_data(compressed_log_data);
  if (url.find("uma") != std::string::npos) {
    OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      HISTOGRAM_UMA_UKM, HiSysEventAdapter::EventType::STATISTIC,
      {HISTOGRAM_TYPE, "arkweb.histogram.uma", HISTOGRAM_CONTENT, base::HexEncode(log_data)}
    );
  } else {
    OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      HISTOGRAM_UMA_UKM, HiSysEventAdapter::EventType::STATISTIC,
      {HISTOGRAM_TYPE, "arkweb.histogram.ukm", HISTOGRAM_CONTENT, base::HexEncode(log_data)}
    );
  }
}

bool ReportUniversalAccessBlocked() {
  std::string bundle_name = crashpad::CrashpadDfx::GetProcessBundleName();
  if (bundle_name.empty()) {
    return false;
  }
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      UNIVERSAL_ACCESS_BLOCKED, HiSysEventAdapter::EventType::STATISTIC,
      {
        BUNDLE_NAME, bundle_name
      });
  return true;
}

void ReportCacheDataSeparate(bool isSeparate) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      CACHE_DATA_SEPARATE_STATISTICS, HiSysEventAdapter::EventType::STATISTIC,
      {
        "SEPARATE_MODE", isSeparate ? std::string("separate") : std::string("not_separate")
      });
}
 
void ReportCacheDataMigrate(bool isMigrate) {
  OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      CACHE_DATA_MIGRATE_STATISTICS, HiSysEventAdapter::EventType::STATISTIC,
      {
        "MIGRATE_MODE", isMigrate ? std::string("migrate") : std::string("not_migrate")
      });
}
