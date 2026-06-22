/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS-IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fuzzer/FuzzedDataProvider.h>
#include <memory>
#include <string>

#include "arkweb/chromium_ext/content/child/child_process_utils.h"
#include "base/test/task_environment.h"
#include "content/child/child_process.h"

using namespace content;

namespace {

constexpr uint8_t MAX_STRING_LEN = 64;

struct TestEnv {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<ChildProcess> process;
  std::unique_ptr<ChildProcessUtils> utils;

  TestEnv() {
    // Use the io_thread_runner constructor to avoid starting a real IO thread
    // and thread pool. All ChildProcessUtils methods check main_thread_ for
    // null and return early, so we don't need a fully initialized ChildProcess.
    process = std::make_unique<ChildProcess>(
        task_environment.GetMainThreadTaskRunner());
    utils = std::make_unique<ChildProcessUtils>(process.get());
  }
};

static TestEnv* g_env = nullptr;

void EnsureProcessInitialized() {
  if (!g_env) {
    g_env = new TestEnv();
  }
}

void FuzzReportIoThreadStatusNullThread(FuzzedDataProvider* fdp) {
  EnsureProcessInitialized();

  bool is_created = fdp->ConsumeBool();
  bool is_in_browser_process = fdp->ConsumeBool();

  g_env->utils->ReportIoThreadStatus(is_created, is_in_browser_process);
}

void FuzzReportIoThreadStatusInBrowser(FuzzedDataProvider* fdp) {
  EnsureProcessInitialized();

  bool is_created = fdp->ConsumeBool();

  g_env->utils->ReportIoThreadStatus(is_created, true);
}

void FuzzReportCompositorKeyThreadNullThread(FuzzedDataProvider* fdp) {
  EnsureProcessInitialized();

  bool is_created = fdp->ConsumeBool();
  bool is_in_browser_process = fdp->ConsumeBool();

  g_env->utils->ReportCompositorKeyThread(is_created, is_in_browser_process);
}

void FuzzReportCompositorKeyThreadInBrowser(FuzzedDataProvider* fdp) {
  EnsureProcessInitialized();

  bool is_created = fdp->ConsumeBool();

  g_env->utils->ReportCompositorKeyThread(is_created, true);
}

void FuzzReportHisyeventNullThread(FuzzedDataProvider* fdp) {
  EnsureProcessInitialized();

  int64_t block_time = fdp->ConsumeIntegral<int64_t>();
  std::string mode = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);

  g_env->utils->ReportHisyevent(block_time, mode, false);
}

void FuzzReportHisyeventInBrowser(FuzzedDataProvider* fdp) {
  EnsureProcessInitialized();

  int64_t block_time = fdp->ConsumeIntegral<int64_t>();
  std::string mode = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);

  g_env->utils->ReportHisyevent(block_time, mode, true);
}

void FuzzReportHisyeventBoundaryValues(FuzzedDataProvider* fdp) {
  EnsureProcessInitialized();

  int64_t time_values[] = {
    INT64_MIN, -1000, 0, 1, 1000, INT64_MAX
  };

  size_t index = fdp->ConsumeIntegralInRange<size_t>(0, 5);
  std::string mode = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);

  g_env->utils->ReportHisyevent(time_values[index], mode, false);
}

void FuzzAllMethods(FuzzedDataProvider* fdp) {
  EnsureProcessInitialized();

  bool is_created = fdp->ConsumeBool();
  bool is_in_browser_process = fdp->ConsumeBool();
  int64_t block_time = fdp->ConsumeIntegral<int64_t>();
  std::string mode = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);

  g_env->utils->ReportIoThreadStatus(is_created, is_in_browser_process);
  g_env->utils->ReportCompositorKeyThread(is_created, is_in_browser_process);
  g_env->utils->ReportHisyevent(block_time, mode, is_in_browser_process);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 8);

  switch (selector) {
    case 0:
      FuzzReportIoThreadStatusNullThread(&fdp);
      break;
    case 1:
      FuzzReportIoThreadStatusInBrowser(&fdp);
      break;
    case 2:
      FuzzReportCompositorKeyThreadNullThread(&fdp);
      break;
    case 3:
      FuzzReportCompositorKeyThreadInBrowser(&fdp);
      break;
    case 4:
      FuzzReportHisyeventNullThread(&fdp);
      break;
    case 5:
      FuzzReportHisyeventInBrowser(&fdp);
      break;
    case 6:
      FuzzReportHisyeventBoundaryValues(&fdp);
      break;
    case 7:
      FuzzAllMethods(&fdp);
      break;
    default:
      FuzzAllMethods(&fdp);
      break;
  }

  return 0;
}
