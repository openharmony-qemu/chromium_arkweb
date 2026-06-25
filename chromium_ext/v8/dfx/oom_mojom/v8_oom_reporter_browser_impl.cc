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

#include <map>
#include <sstream>

#include "arkweb/chromium_ext/v8/dfx/oom_mojom/v8_oom_reporter_browser_impl.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "arkweb/ohos_nweb/src/sysevent/event_reporter.h"
#include "base/values.h"
#include "base/json/json_reader.h"
#include "base/trace_event/trace_event.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"

void V8OOMReporterImpl::ProcessPendingReceiver(mojo::PendingReceiver<v8_oom::mojom::V8OOMReporter>& receiver)
{
  mojo::MakeSelfOwnedReceiver(std::make_unique<V8OOMReporterImpl>(), std::move(receiver));
}

static void ReportV8OOMTraceNLog(std::map<std::string, std::string>& oomMap)
{
  std::ostringstream logStream;
  logStream << "type = " << oomMap["type"] << ", "
            << "pid = " << oomMap["pid"] << ", "
            << "rss = " << oomMap["rss"] << "KB, "
            << "pss = " << oomMap["pss"] << "KB, "
            << "fd_num = " << oomMap["fd_num"] << ", "
            << "js_heap_total = " << oomMap["js_heap_total"] << "KB, "
            << "js_heap_used = " << oomMap["js_heap_used"] << "KB, "
            << "url" << oomMap["url"] << ", "
            << "oom_location = " << oomMap["oom_location"] << ", "
            << "oom_detail = " << oomMap["oom_detail"];
  std::string logMsg = logStream.str();

  TRACE_EVENT1("base", "WEBVIEW::V8_JS_MEM_LEAK", "V8_OOM_INFO", logMsg);
  LOG(INFO) << logMsg;
}

static void ReportV8OOMInfo(const std::string& oomInfoJson)
{
  const auto oomInfo = base::JSONReader::ReadAndReturnValueWithError(
      oomInfoJson, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!oomInfo.has_value()) {
    LOG(ERROR) << "get oomInfo error:" << oomInfo.error().message;
    return;
  }

  if (!oomInfo->is_dict()) {
    LOG(ERROR) << "oomInfo is not a dictionary type";
    return;
  }

  const base::Value::Dict& oomInfoDict = oomInfo->GetDict();
  const auto extractString = [&](const std::string& key, const std::string& defaultValue) {
    return oomInfoDict.FindString(key) ? *oomInfoDict.FindString(key) : defaultValue;
  };

  std::string pid = extractString("pid", "0");
  if (pid == "0" || !std::all_of(pid.begin(), pid.end(), ::isdigit)) {
    LOG(ERROR) << "process pid is invalid";
    return;
  }

  std::map<std::string, std::string> oomMap = {
    {"type", extractString("type", "")},
    {"pid", pid},
    {"rss", extractString("rss", "0")},
    {"pss", extractString("pss", "0")},
    {"fd_num", extractString("fd_num", "0")},
    {"js_heap_total", extractString("js_heap_total", "0")},
    {"js_heap_used", extractString("js_heap_used", "0")},
    {"url", extractString("url", "")},
    {"oom_location", extractString("oom_location", "")},
    {"oom_detail", extractString("oom_detail", "")}
  };

  ReportV8OOMTraceNLog(oomMap);

#if !defined(COMPONENT_BUILD)
  ReportRendererMem(oomMap["type"], oomMap["pid"], oomMap["rss"], oomMap["pss"],
                    oomMap["js_heap_total"], oomMap["js_heap_used"], "0", oomMap["url"]);
#endif
}

void V8OOMReporterImpl::ReportV8OOM(const std::string& eventName, const std::string& oomInfoJson)
{
  if (eventName == V8OOMReporterImpl::V8_JS_MEM_LEAK) {
    ReportV8OOMInfo(oomInfoJson);
  }
}
