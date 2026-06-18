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

#ifndef V8_OOM_MONITOR_RENDER_IMPL_H_
#define V8_OOM_MONITOR_RENDER_IMPL_H_

#include "arkweb/chromium_ext/v8/dfx/oom_mojom/mojom/v8_oom_reporting.mojom.h"
#include "base/process/process_metrics.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "v8/include/v8-isolate.h"

namespace v8_oom {
// render
class V8OOMMonitorImpl {
 public:
  V8OOMMonitorImpl() = default;
  ~V8OOMMonitorImpl() = default;

  static constexpr char V8_JS_MEM_LEAK[] = "V8_JS_MEM_LEAK";

  static std::shared_ptr<V8OOMMonitorImpl> GetInstance();
  void Trigger(std::string url)
  {
    if (!has_initialized_) {
      SetInitialized();
    }
    mem_info_.url = url;
  }

  bool IsInitialized() { return has_initialized_; }
  void SetInitialized();

  void ReportOOM(const char* location, const v8::OOMDetails& details);
  void ReportV8OOMEvent(const std::string& location, const std::string& detail);

  mojo::PendingReceiver<v8_oom::mojom::V8OOMReporter> &GetPendingReceiver()
  {
    return receiver_;
  }

  struct V8OOMInfo {
    pid_t pid;  // Process ID of the monitored process
    size_t rss;  // Resident Set Size (RSS) in KB
    size_t pss;  // Proportional Set Size (PSS) in KB
    u_int16_t fd_num;  // Number of file descriptors opened by the process
    size_t js_heap_total;  // Total JavaScript heap size in KB
    size_t js_heap_used;  // Used JavaScript heap size in KB
    std::string url; // URL
    std::string oom_location; // OOM location from V8
    std::string oom_detail; // OOM detail from V8
  };

private:
  void UpdateProcessBasicMemoryInfo(V8OOMInfo &mem_info);
  void UpdateProcessMemoryInfo(V8OOMInfo &mem_info);
  template<typename T> void ReadProcFile(const std::string& filePath, const std::string& token, T& value);

  mojo::Remote<v8_oom::mojom::V8OOMReporter> remote_;
  mojo::PendingReceiver<v8_oom::mojom::V8OOMReporter> receiver_ =
      remote_.BindNewPipeAndPassReceiver();
  V8OOMInfo mem_info_;
  bool has_initialized_ = false;

#if BUILDFLAG(ARKWEB_TEST)
  bool stub_var_ = false;

  v8::Isolate* stubIsolate() {
    if (stub_var_) {
      return v8::Isolate::GetCurrent();
    } else {
      return nullptr;
    }
  }
#endif // ARKWEB_TEST
};

} // namespace v8_oom

#endif // V8_OOM_MONITOR_RENDER_IMPL_H_
