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

#include "arkweb/chromium_ext/v8/dfx/oom_mojom/v8_oom_reporter_browser_impl.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <map>
#include <sstream>

#include "arkweb/ohos_nweb/src/sysevent/event_reporter.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"

void V8OOMReporterImpl::ProcessPendingReceiver(mojo::PendingReceiver<v8_oom::mojom::V8OOMReporter>& receiver)
{
  mojo::MakeSelfOwnedReceiver(std::make_unique<V8OOMReporterImpl>(), std::move(receiver));
}

namespace {

// Hardening limits for renderer-supplied OOM telemetry (see security review):
// the whole payload is untrusted input from a possibly-compromised renderer.
constexpr size_t kMaxOomJsonSize = 8 * 1024;
constexpr size_t kMaxPidLen = 10;
constexpr size_t kMaxFieldLen = 256;
std::atomic<int> g_rejectedCount{0};

// Truncate to a UTF-8 code point boundary so no multi-byte character is split.
std::string TruncateUtf8(const std::string& s, size_t maxBytes = kMaxFieldLen) {
  if (s.size() <= maxBytes) {
    return s;
  }
  size_t cut = maxBytes;
  while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) {
    --cut;  // back off to the start of the multi-byte sequence
  }
  return s.substr(0, cut) + "...(truncated)";
}

}  // namespace

static void ReportV8OOMTraceNLog(std::map<std::string, std::string>& oomMap)
{
  std::ostringstream logStream;
  logStream << "type = " << TruncateUtf8(oomMap["type"]) << ", "
            << "pid = " << TruncateUtf8(oomMap["pid"]) << ", "
            << "rss = " << TruncateUtf8(oomMap["rss"]) << "KB, "
            << "pss = " << TruncateUtf8(oomMap["pss"]) << "KB, "
            << "fd_num = " << TruncateUtf8(oomMap["fd_num"]) << ", "
            << "js_heap_total = " << TruncateUtf8(oomMap["js_heap_total"])
            << "KB, "
            << "js_heap_used = " << TruncateUtf8(oomMap["js_heap_used"])
            << "KB, "
            << "url = " << TruncateUtf8(oomMap["url"]) << ", "
            << "oom_location = " << TruncateUtf8(oomMap["oom_location"]) << ", "
            << "oom_detail = " << TruncateUtf8(oomMap["oom_detail"]);
  std::string logMsg = logStream.str();

  TRACE_EVENT1("base", "WEBVIEW::V8_JS_MEM_LEAK", "V8_OOM_INFO", logMsg);
  LOG(INFO) << logMsg;
}

static void ReportV8OOMInfo(const std::string& oomInfoJson)
{
  // Untrusted input: cap the payload size before parsing (parse cost stays off
  // the growth path of a hostile renderer).
  if (oomInfoJson.size() > kMaxOomJsonSize) {
    LOG(ERROR) << "oomInfoJson too large (" << oomInfoJson.size() << " > "
               << kMaxOomJsonSize << "), rejected #" << ++g_rejectedCount;
    return;
  }

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
  // Cast each byte to unsigned char before isdigit: passing a negative int
  // (non-ASCII UTF-8 byte) to isdigit is undefined behaviour.
  if (pid.empty() || pid.size() > kMaxPidLen || pid == "0" ||
      !std::all_of(pid.begin(), pid.end(),
                   [](unsigned char c) { return std::isdigit(c) != 0; })) {
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
