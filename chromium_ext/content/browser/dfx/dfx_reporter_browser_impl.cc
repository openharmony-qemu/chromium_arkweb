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

#include <map>
#include <memory>
#include <sstream>

#include "dfx_reporter_browser_impl.h"
#include "arkweb/ohos_nweb/src/sysevent/event_reporter.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "collect_memory_utils.h"
#include "content/public/browser/gpu_data_manager.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"

namespace {

bool ParseRenderPid(const std::string& pid, pid_t& pid_value)
{
  int parsed_pid = 0;
  if (!base::StringToInt(pid, &parsed_pid) || parsed_pid <= 0) {
    LOG(ERROR) << "render process pid is invalid";
    return false;
  }
  pid_value = static_cast<pid_t>(parsed_pid);
  return true;
}

std::string GetStringOrDefault(const base::Value::Dict& dict,
                               const std::string& key,
                               const std::string& default_value)
{
  const std::string* value = dict.FindString(key);
  return value ? *value : default_value;
}

using HiSysEventHandler =
    base::RepeatingCallback<void(const base::Value::Dict& info)>;

// Maximum JSON payload size accepted from the renderer (DoS protection).
constexpr size_t kMaxHiSysEventJsonSize = 64 * 1024;

constexpr int32_t kMaxMediaErrorCode = 100000;

void HandleBlankScreen(const base::Value::Dict& info) {
  auto reason = info.FindInt("reason");
  auto count = info.FindInt("count");
  if (!reason || !count) {
    LOG(WARNING) << "ARKWEB_BLANK_SCREEN_EVENT missing fields";
    return;
  }
  // Validate ranges: reason must be a known DetectedBlankScreenReason enum
  // value (0 or 1), count must be non-negative.
  if (*reason < 0 || *reason > 1 || *count < 0) {
    LOG(WARNING) << "ARKWEB_BLANK_SCREEN_EVENT invalid range: reason="
                 << *reason << " count=" << *count;
    return;
  }
  ReportBlankScreen(*reason, *count);
}

void HandleWebMediaPlayError(const base::Value::Dict& info) {
  const std::string* error_type = info.FindString("error_type");
  auto error_code = info.FindInt("error_code");
  const std::string* error_desc = info.FindString("error_desc");
  if (!error_type || !error_code || !error_desc) {
    LOG(WARNING) << "WEB_MEDIA_PLAY_ERROR missing fields";
    return;
  }
  // Validate range: error_code is a media error code, typically in the
  // range [0, 100000].  Reject obviously bogus values from a compromised
  // renderer.
  if (*error_code < 0 || *error_code > kMaxMediaErrorCode) {
    LOG(WARNING) << "WEB_MEDIA_PLAY_ERROR invalid error_code=" << *error_code;
    return;
  }
  ReportWebMediaPlayErrorInfo(*error_type, *error_code, *error_desc);
}

void HandleVideoEncodeFormat(const base::Value::Dict& info) {
  const std::string* format = info.FindString("encode_format");
  if (!format) {
    LOG(WARNING) << "VIDEO_ENCODE_FORMAT missing encode_format";
    return;
  }
  ReportVideoEncodeFormat(*format);
}

void HandlePictureDecode(const base::Value::Dict& info) {
  const std::string* type = info.FindString("picture_type");
  if (!type) {
    LOG(WARNING) << "PICTURE_DECODE missing picture_type";
    return;
  }
  ReportPictureDecode(*type);
}

void HandleFfmpegCodecOperation(const base::Value::Dict& info) {
  const std::string* codec_type = info.FindString("codec_type");
  const std::string* format = info.FindString("format");
  if (!codec_type || !format) {
    LOG(WARNING) << "FFMPEG_CODEC_OPERATION missing fields";
    return;
  }
  ReportFfmpegCodecOperation(*codec_type, *format);
}

void HandleDuplicateFileUpload(const base::Value::Dict& info) {
  const std::string* desc = info.FindString("error_desc");
  if (!desc) {
    LOG(WARNING) << "DUPLICATE_FILE_UPLOAD missing error_desc";
    return;
  }
  ReportDuplicateFileUpload(*desc);
}

struct EventHandlerEntry {
  HiSysEventHandler handler;
  // If true, the raw JSON string is passed to the handler path instead of
  // the parsed dict. Used by legacy memory events that re-parse internally.
  bool raw_json = false;
};

const base::flat_map<std::string, EventHandlerEntry>&
GetHiSysEventHandlerRegistry() {
  static const base::NoDestructor<
      base::flat_map<std::string, EventHandlerEntry>>
      registry({
          {DfxReporterImpl::PAGE_MEM_LEAK,
           {HiSysEventHandler(), true}},
          {DfxReporterImpl::BASIC_RENDER_MEM,
           {HiSysEventHandler(), true}},
          {"ARKWEB_BLANK_SCREEN_EVENT",
           {base::BindRepeating(&HandleBlankScreen)}},
          {"WEB_MEDIA_PLAY_ERROR",
           {base::BindRepeating(&HandleWebMediaPlayError)}},
          {"VIDEO_ENCODE_FORMAT",
           {base::BindRepeating(&HandleVideoEncodeFormat)}},
          {"PICTURE_DECODE",
           {base::BindRepeating(&HandlePictureDecode)}},
          {"FFMPEG_CODEC_OPERATION",
           {base::BindRepeating(&HandleFfmpegCodecOperation)}},
          {"DUPLICATE_FILE_UPLOAD",
           {base::BindRepeating(&HandleDuplicateFileUpload)}},
      });
  return *registry;
}

}  // namespace

void DfxReporterImpl::ProcessPendingReceiver(mojo::PendingReceiver<dfx::mojom::DfxReporter>& receiver)
{
  mojo::MakeSelfOwnedReceiver(std::make_unique<DfxReporterImpl>(), std::move(receiver));
}

void ReportMemTraceNLog(std::map<std::string, std::string>& memMap)
{
  std::ostringstream logStream;
  logStream << "type = " << memMap["type"] << ", "
            << "pid = " << memMap["pid"] << ", "
            << "rss = " << memMap["rss"] << "KB, "
            << "pss = " << memMap["pss"] << "KB, "
            << "fd_num = " << memMap["fd_num"] << ", "
            << "js_heap_total = " << memMap["js_heap_total"] << "KB, "
            << "js_heap_used = " << memMap["js_heap_used"] << "KB, "
            << "gpu_mem = " << memMap["gpu_mem"] << "KB";
  std::string logMsg = logStream.str();

  TRACE_EVENT1("base", "WEBVIEW::PAGE_MEM_LEAK", "MEMORY_INFO", logMsg);
  LOG(INFO) << logMsg;
}

void OnVideoMemoryUsageStatsUpdate(pid_t pid, const std::map<std::string, std::string>& memMap, bool isSysEvent,
                                   const gpu::VideoMemoryUsageStats& gpu_memory_stats)
{
  std::map<std::string, std::string> reportMap = memMap;
  if (!gpu_memory_stats.process_map.empty()) {
    auto it = gpu_memory_stats.process_map.find(pid);
    if (it != gpu_memory_stats.process_map.end()) {
      const auto& stats = it->second;
      reportMap["gpu_mem"] = std::to_string(stats.video_memory / DfxReporterImpl::BYTES_TO_KB);
    } else {
      reportMap["gpu_mem"] = "0";
    }
  }

  ReportMemTraceNLog(reportMap);
#if !defined(COMPONENT_BUILD)
  if (isSysEvent) {
    ReportRendererMem(reportMap["type"], reportMap["pid"], reportMap["rss"], reportMap["pss"],
                      reportMap["js_heap_total"], reportMap["js_heap_used"], reportMap["gpu_mem"], reportMap["url"]);
  }
#endif
}

void GetGpuUsage(pid_t pid, const std::map<std::string, std::string>& memMap, bool isSysEvent)
{
  const auto gpu_data_manager = content::GpuDataManager::GetInstance();
  if (!gpu_data_manager) {
    LOG(ERROR) << "GetGpuUsage gpu_data_manager is nullptr";
    return;
  }

  gpu_data_manager->RequestVideoMemoryUsageStatsUpdate(
      base::BindOnce(&OnVideoMemoryUsageStatsUpdate, pid, memMap, isSysEvent));
}

void ReportRendererInfo(const std::string& sysEventInfoJson, bool isSysEvent)
{
  const auto sysEventInfo = base::JSONReader::ReadAndReturnValueWithError(
      sysEventInfoJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!sysEventInfo.has_value()) {
    LOG(ERROR) << "get sysEventInfo error:" << sysEventInfo.error().message;
    return;
  }

  if (!sysEventInfo->is_dict()) {
    LOG(ERROR) << "sysEventInfo is not a dictionary type";
    return;
  }

  const base::Value::Dict& sysEventInfoDict = sysEventInfo->GetDict();
  const std::string pid = GetStringOrDefault(sysEventInfoDict, "pid", "0");
  pid_t pid_value = 0;
  if (!ParseRenderPid(pid, pid_value)) {
    return;
  }
  std::map<std::string, std::string> memMap = {
    {"type", GetStringOrDefault(sysEventInfoDict, "type", "")},
    {"pid", pid},
    {"rss", GetStringOrDefault(sysEventInfoDict, "rss", "0")},
    {"pss", GetStringOrDefault(sysEventInfoDict, "pss", "0")},
    {"fd_num", GetStringOrDefault(sysEventInfoDict, "fd_num", "0")},
    {"js_heap_total", GetStringOrDefault(sysEventInfoDict, "js_heap_total", "0")},
    {"js_heap_used", GetStringOrDefault(sysEventInfoDict, "js_heap_used", "0")},
    {"url", GetStringOrDefault(sysEventInfoDict, "url", "")}
  };
  GetGpuUsage(pid_value, memMap, isSysEvent);
}

void DfxReporterImpl::ReportMemInfo(const std::string& sysEventInfoJson)
{
  ReportRendererInfo(sysEventInfoJson, false);
}

void DfxReporterImpl::OnRenderMemReport(pid_t pid, const std::map<std::string, std::string>& memMap,
                       const gpu::VideoMemoryUsageStats& gpu_memory_stats)
{
  std::map<std::string, std::string> reportMap = memMap;
  reportMap["gpu_mem"] = "0";
  if (!gpu_memory_stats.process_map.empty()) {
    auto it = gpu_memory_stats.process_map.find(pid);
    if (it != gpu_memory_stats.process_map.end()) {
      const auto& stats = it->second;
      reportMap["gpu_mem"] = std::to_string(stats.video_memory / DfxReporterImpl::BYTES_TO_KB);
    }
  }
  ReportBasicRendererMem(reportMap["pid"],
                         reportMap["rss"],
                         reportMap["pss"],
                         reportMap["swap_pss"],
                         reportMap["fd_num"],
                         reportMap["oom_score_adj"],
                         reportMap["js_heap_total"],
                         reportMap["js_heap_used"],
                         reportMap["pa"],
                         reportMap["gpu_mem"]);
}

void DfxReporterImpl::CollectBasicBrowserMem()
{
  DfxReporterImpl::BrowserMemInfo browser_info;
  content::CollectMemoryUtils collectMemoryUtils;
  collectMemoryUtils.GetProcessBasicMemoryInfo(browser_info.pid, browser_info.rss, browser_info.pss);
  collectMemoryUtils.ReadProcFile("/proc/self/smaps_rollup", "SwapPss:", browser_info.swap_pss);
  collectMemoryUtils.GetFdCount("/proc/self/fd", browser_info.fd_num);
  collectMemoryUtils.GetOomScoreAdj("/proc/self/oom_score_adj", browser_info.oom_score_adj);

  // get PartitionAllocator
  collectMemoryUtils.GetPartitionAllocatorMem(browser_info.pa);

  ReportBasicBrowserMem(std::to_string(browser_info.pid),
                        std::to_string(browser_info.rss),
                        std::to_string(browser_info.pss),
                        std::to_string(browser_info.swap_pss),
                        std::to_string(browser_info.fd_num),
                        std::to_string(browser_info.oom_score_adj),
                        std::to_string(browser_info.pa));
}

void DfxReporterImpl::CollectGpuMemory(pid_t pid, const std::map<std::string, std::string>& memMap)
{
  const auto gpu_data_manager = content::GpuDataManager::GetInstance();
  if (!gpu_data_manager) {
    LOG(ERROR) << "CollectGpuMemory gpu_data_manager is nullptr";
    OnRenderMemReport(pid, memMap, gpu::VideoMemoryUsageStats());
    return;
  }
  gpu_data_manager->RequestVideoMemoryUsageStatsUpdate(
      base::BindOnce(&DfxReporterImpl::OnRenderMemReport, weak_factory_.GetWeakPtr(), pid, memMap));
}

void DfxReporterImpl::CollectBasicRenderMem(const std::string& sysEventInfoJson)
{
  const auto sysEventInfo = base::JSONReader::ReadAndReturnValueWithError(
      sysEventInfoJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!sysEventInfo.has_value()) {
    LOG(ERROR) << "get sysEventInfo error:" << sysEventInfo.error().message;
    return;
  }
  if (!sysEventInfo->is_dict()) {
    LOG(ERROR) << "sysEventInfo is not dictionary type";
    return;
  }

  const base::Value::Dict& sysEventInfoDict = sysEventInfo->GetDict();
  const std::string pid = GetStringOrDefault(sysEventInfoDict, "pid", "0");
  pid_t pid_value = 0;
  if (!ParseRenderPid(pid, pid_value)) {
    return;
  }
  std::map<std::string, std::string> memMap = {
    {"pid", pid},
    {"rss", GetStringOrDefault(sysEventInfoDict, "rss", "0")},
    {"pss", GetStringOrDefault(sysEventInfoDict, "pss", "0")},
    {"swap_pss", GetStringOrDefault(sysEventInfoDict, "swap_pss", "0")},
    {"fd_num", GetStringOrDefault(sysEventInfoDict, "fd_num", "0")},
    {"oom_score_adj", GetStringOrDefault(sysEventInfoDict, "oom_score_adj", "0")},
    {"js_heap_total", GetStringOrDefault(sysEventInfoDict, "js_heap_total", "0")},
    {"js_heap_used", GetStringOrDefault(sysEventInfoDict, "js_heap_used", "0")},
    {"pa", GetStringOrDefault(sysEventInfoDict, "pa", "0")}
  };
  CollectGpuMemory(pid_value, memMap);
}

void DfxReporterImpl::ReportHiSysEvent(const std::string& eventName,
                                       const std::string& sysEventInfoJson)
{
  const auto& registry = GetHiSysEventHandlerRegistry();
  auto it = registry.find(eventName);
  if (it == registry.end()) {
    LOG(WARNING) << "Unknown hiSysEvent from renderer: " << eventName;
    return;
  }

  if (sysEventInfoJson.size() > kMaxHiSysEventJsonSize) {
    LOG(WARNING) << "hiSysEvent JSON too large for event " << eventName
                 << " size=" << sysEventInfoJson.size();
    return;
  }

  // Legacy memory events pass the raw JSON string to their own parsers.
  if (it->second.raw_json) {
    DCHECK(it->second.handler.is_null());
    if (eventName == DfxReporterImpl::PAGE_MEM_LEAK) {
      ReportRendererInfo(sysEventInfoJson, true);
    } else if (eventName == DfxReporterImpl::BASIC_RENDER_MEM) {
      CollectBasicRenderMem(sysEventInfoJson);
      CollectBasicBrowserMem();
    }
    return;
  }

  // Generic path: parse JSON and dispatch to the handler.
  auto info = base::JSONReader::ReadAndReturnValueWithError(
      sysEventInfoJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!info.has_value() || !info->is_dict()) {
    LOG(WARNING) << "Invalid JSON for event " << eventName
                 << (info.has_value()
                         ? " (not a dict)"
                         : ": " + info.error().message);
    return;
  }

  it->second.handler.Run(info->GetDict());
}

// the param `eventInfo` may be used in the future
void FreezeReporterImpl::ReportRenderFreeze(int32_t pid, const std::string& processName,const std::string& freezeMsg,
                                            int32_t uid)
{
#if !defined(COMPONENT_BUILD)
  auto packageName = OHOS::NWeb::OhosAdapterHelper::GetInstance().GetSystemPropertiesInstance().GetBundleName();
  ReportAppfreeze(pid, packageName, processName, freezeMsg, uid);
#endif
}

// static
void FreezeReporterImpl::ProcessPendingReceiver(mojo::PendingReceiver<dfx::mojom::FreezeReporter> receiver)
{
  mojo::MakeSelfOwnedReceiver(std::make_unique<FreezeReporterImpl>(), std::move(receiver));
}