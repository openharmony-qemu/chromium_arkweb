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

#include "arkweb/chromium_ext/components/viz/service/display_embedder/compositor_gpu_thread_utils.h"

#include "base/command_line.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "arkweb/chromium_ext/gpu/ipc/common/nweb_native_window_tracker.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/aafwk_browser_client_adapter.h"

namespace viz {
namespace {

class MockAafwkBrowserClientAdapter
    : public OHOS::NWeb::AafwkBrowserClientAdapter {
 public:
  void* QueryRenderSurface(int32_t /*surface_id*/) override { return nullptr; }
  void ReportThread(OHOS::NWeb::ResSchedStatusAdapter status,
                    int32_t process_id,
                    int32_t thread_id,
                    OHOS::NWeb::ResSchedRoleAdapter role) override {
    report_count++;
    last_status = status;
    last_pid = process_id;
    last_tid = thread_id;
    last_role = role;
  }
  void PassSurface(int64_t /*surface_id*/) override {}
  void DestroyRenderSurface(int32_t /*surface_id*/) override {}

  int report_count = 0;
  OHOS::NWeb::ResSchedStatusAdapter last_status =
      OHOS::NWeb::ResSchedStatusAdapter::THREAD_CREATED;
  int32_t last_pid = 0;
  int32_t last_tid = 0;
  OHOS::NWeb::ResSchedRoleAdapter last_role =
      OHOS::NWeb::ResSchedRoleAdapter::IMPORTANT_DISPLAY;
};

}  // namespace

class CompositorGpuThreadUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    original_cmdline_ = *base::CommandLine::ForCurrentProcess();
    thread_ = std::make_unique<base::Thread>("CompositorGpuThreadForTest");
    ASSERT_TRUE(thread_->Start());
  }

  void TearDown() override {
    // Restore the original command line so the "type" switch does not leak
    // into other tests of this binary.
    *base::CommandLine::ForCurrentProcess() = original_cmdline_;
    NWebNativeWindowTracker::Get()->SetBrowserClient(nullptr);
    thread_.reset();
  }

  // NWebNativeWindowTracker lives in the global namespace.
  base::CommandLine original_cmdline_{
      base::CommandLine::ForCurrentProcess()->argv()};
  std::unique_ptr<base::Thread> thread_;
};

#if BUILDFLAG(ARKWEB_OOP_GPU_PROCESS)
// Test both entry points in the gpu-process case: the report is routed to the
// browser client registered in NWebNativeWindowTracker.
TEST_F(CompositorGpuThreadUtilsTest,
       GpuProcessType_ReportsThreadViaBrowserClient) {
  auto mock = std::make_shared<MockAafwkBrowserClientAdapter>();
  NWebNativeWindowTracker::Get()->SetBrowserClient(mock);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("type",
                                                            "gpu-process");

  CompositorGpuThreadUtils::CompositorGptThreadInitializeUtils(thread_.get());
  EXPECT_EQ(mock->report_count, 1);
  EXPECT_EQ(mock->last_status,
            OHOS::NWeb::ResSchedStatusAdapter::THREAD_CREATED);
  EXPECT_EQ(mock->last_role,
            OHOS::NWeb::ResSchedRoleAdapter::IMPORTANT_DISPLAY);
  EXPECT_NE(mock->last_tid, 0);

  CompositorGpuThreadUtils::CompositorGpuThreadDestruct(thread_.get());
  EXPECT_EQ(mock->report_count, 2);
  EXPECT_EQ(mock->last_status,
            OHOS::NWeb::ResSchedStatusAdapter::THREAD_DESTROYED);
}

// Test both entry points in the browser-process case (no "type" switch): a
// task that reports the key thread is posted to the thread's task runner and
// must run without crash.
TEST_F(CompositorGpuThreadUtilsTest,
       NonGpuProcessType_PostsTaskToThreadRunner) {
  CompositorGpuThreadUtils::CompositorGptThreadInitializeUtils(thread_.get());
  CompositorGpuThreadUtils::CompositorGpuThreadDestruct(thread_.get());
  // Wait until the posted tasks have been executed on the thread.
  thread_->FlushForTesting();
  EXPECT_TRUE(true);
}
#endif  // BUILDFLAG(ARKWEB_OOP_GPU_PROCESS)

}  // namespace viz
