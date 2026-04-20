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

#include "arkweb/chromium_ext/v8/dfx/oom_mojom/v8_oom_monitor_render_impl.h"

#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "third_party/bounds_checking_function/include/securec.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#include "v8-oom-reporter.h"
#include "v8/include/v8-isolate.h"

#define MEM_CONVERT 1024

namespace v8_oom {

using V8OOMInfo = V8OOMMonitorImpl::V8OOMInfo;

static void V8OOMErrorCallback(const char* location,
                               const v8::OOMDetails& details,
                               void* data) {
  if (!V8OOMMonitorImpl::GetInstance()) {
    return;
  }

  V8OOMMonitorImpl::GetInstance()->ReportOOM(location, details);
}

void V8OOMMonitorImpl::SetInitialized() {
  has_initialized_ = true;
  v8::dfx::SetOOMReportCallback(v8_oom::V8OOMErrorCallback);
  mem_info_ = {};
}

template<typename T>
void V8OOMMonitorImpl::ReadProcFile(const std::string& filePath, const std::string& token, T& value)
{
  std::ifstream file(filePath);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file: " << filePath;
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (!token.empty() && line.find(token) != 0) {
      continue;
    }

    size_t pos = token.empty() ? 0 : line.find(":");
    if (pos != std::string::npos) {
      std::string valueStr = line.substr(pos + 1);
      std::stringstream ss(valueStr);
      ss >> value;
      return;
    }
  }
  LOG(ERROR) << "Failed to find token: " << token << " in file: " << filePath;
}

void V8OOMMonitorImpl::UpdateProcessBasicMemoryInfo(V8OOMInfo &mem_info)
{
  if (mem_info.pid == 0) {
    ReadProcFile("/proc/self/status", "NSpid:", mem_info.pid);
  }

  mem_info.rss = 0;
  ReadProcFile("/proc/self/smaps_rollup", "Rss:", mem_info.rss);

  mem_info.pss = 0;
  ReadProcFile("/proc/self/smaps_rollup", "Pss:", mem_info.pss);
}

void V8OOMMonitorImpl::UpdateProcessMemoryInfo(V8OOMInfo &mem_info)
{
  UpdateProcessBasicMemoryInfo(mem_info);

  mem_info.fd_num = 0;
  ReadProcFile("/proc/self/fd_num", "", mem_info.fd_num);

#if BUILDFLAG(ARKWEB_TEST)
  v8::Isolate *isolate = stubIsolate();
#else
  v8::Isolate *isolate = v8::Isolate::GetCurrent();
#endif // ARKWEB_TEST
  if (isolate) {
    v8::HeapStatistics heap_statistics;
    isolate->GetHeapStatistics(&heap_statistics);
    mem_info.js_heap_total = heap_statistics.total_heap_size() / MEM_CONVERT;
    mem_info.js_heap_used = heap_statistics.used_heap_size() / MEM_CONVERT;
  } else {
    LOG(ERROR) << "V8 isolate is null";
    mem_info.js_heap_total = 0;
    mem_info.js_heap_used = 0;
  }
}

static std::string ConvertToBaseValue(const std::string& type, V8OOMInfo &mem_info)
{
  auto mem_info_json = base::Value::Dict()
    .Set("type", type)
    .Set("pid", std::to_string(mem_info.pid))
    .Set("rss", std::to_string(mem_info.rss))
    .Set("pss", std::to_string(mem_info.pss))
    .Set("fd_num", std::to_string(mem_info.fd_num))
    .Set("js_heap_total", std::to_string(mem_info.js_heap_total))
    .Set("js_heap_used", std::to_string(mem_info.js_heap_used))
    .Set("url", mem_info.url)
    .Set("oom_location", mem_info.oom_location)
    .Set("oom_detail", mem_info.oom_detail);
  std::string mem_info_json_string;
  base::JSONWriter::Write(mem_info_json, &mem_info_json_string);
  return mem_info_json_string;
}

void V8OOMMonitorImpl::ReportV8OOMEvent(const std::string& location, const std::string& detail)
{
  if (!remote_.is_bound()) {
    LOG(ERROR) << "ReportV8OOMEvent remote is not bound";
    return;
  }

  mem_info_.oom_location = location;
  mem_info_.oom_detail = detail;

  UpdateProcessMemoryInfo(mem_info_);

  std::string mem_info_json_string = ConvertToBaseValue(V8_JS_MEM_LEAK, mem_info_);
  remote_->ReportV8OOM(V8_JS_MEM_LEAK, mem_info_json_string);
}

void V8OOMMonitorImpl::ReportOOM(const char* location, const v8::OOMDetails& details) {
  std::string loc_str = location ? location : "";
  std::string detail_str = details.detail ? details.detail : "";
  ReportV8OOMEvent(loc_str, detail_str);
}

std::shared_ptr<V8OOMMonitorImpl> V8OOMMonitorImpl::GetInstance()
{
  static std::shared_ptr<V8OOMMonitorImpl> instance = std::make_shared<V8OOMMonitorImpl>();
  return instance;
}

} // namespace v8_oom
