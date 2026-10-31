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

#include "arkweb/chromium_ext/v8/dfx/oom_mojom/v8-oom-reporter.h"

#include <mutex>

#include "base/no_destructor.h"

namespace v8 {
namespace dfx {
namespace {

V8OOMReportCallbackWithData g_oom_report_callback{nullptr};
base::NoDestructor<std::mutex> g_report_mutex;

}  // namespace

// static
V8OOMReportCallbackWithData GetOOMReportCallback() {
  std::lock_guard<std::mutex> lock(*g_report_mutex);
  return g_oom_report_callback;
}

void SetOOMReportCallback(V8OOMReportCallbackWithData callback) {
  std::lock_guard<std::mutex> lock(*g_report_mutex);
  g_oom_report_callback = callback;
}
}  // namespace dfx
}  // namespace v8
