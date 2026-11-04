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

#include "base/pending_task.h"
#include "base/test/task_environment.h"
#include "gtest/gtest.h"

#define private public
#include "arkweb/chromium_ext/gpu/ipc/service/gpu_hang_adapter.h"
#undef private

namespace gpu {

class GpuHangAdapterTestNoEnv : public testing::Test {
 public:
  GpuHangAdapterTestNoEnv() = default;
  ~GpuHangAdapterTestNoEnv() override = default;
};

class GpuHangAdapterTestWithEnv : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(GpuHangAdapterTestNoEnv, GetProcessName) {
  std::string out = GetProcessName();
  EXPECT_FALSE(out.empty());
}

TEST_F(GpuHangAdapterTestNoEnv, LogGpuHungEvent) {
  LogGpuHungEvent(0, 0);
  LogGpuHungEvent(1, 100);
}

TEST_F(GpuHangAdapterTestNoEnv, IsHangTestEnabledFalse) {
  bool ret = IsHangTestEnabled("abc");
  EXPECT_FALSE(ret);
}

TEST_F(GpuHangAdapterTestNoEnv, IsHangTestEnabledTrue) {
  bool ret = IsHangTestEnabled("false");
  EXPECT_TRUE(ret);
}

TEST_F(GpuHangAdapterTestNoEnv, ReportGpuFreeze) {
  ReportGpuFreeze();
}

TEST_F(GpuHangAdapterTestNoEnv, SimulateHangForTestingFalse) {
  int32_t test_count = 0;
  SimulateHangForTesting(test_count, false, "test_thread");
  EXPECT_EQ(test_count, 0);
}

TEST_F(GpuHangAdapterTestNoEnv, SimulateHangForTestingTrueModuloFalse) {
  int32_t test_count = 1;
  SimulateHangForTesting(test_count, true, "test_thread");
  EXPECT_EQ(test_count, 2);
}

TEST_F(GpuHangAdapterTestNoEnv, CreateAdapterCustomThreadNoRunner) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  EXPECT_NE(adapter, nullptr);
  EXPECT_EQ(adapter->thread_name_, "test_thread");
  EXPECT_FALSE(adapter->checkHangWatch_);
  EXPECT_FALSE(adapter->hang_test_);
}

TEST_F(GpuHangAdapterTestNoEnv, CreateAdapterGpuMainNoRunner) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterForGpuMain();
  EXPECT_NE(adapter, nullptr);
  EXPECT_EQ(adapter->thread_name_, "gpu_main");
}

TEST_F(GpuHangAdapterTestNoEnv, CreateAdapterCompositorGpuNoRunner) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterForCompositorGpu();
  EXPECT_NE(adapter, nullptr);
  EXPECT_EQ(adapter->thread_name_, "compositor_gpu");
}

TEST_F(GpuHangAdapterTestNoEnv, CreateAdapterHangTestTrueNoRunner) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("false");
  EXPECT_NE(adapter, nullptr);
  EXPECT_TRUE(adapter->hang_test_);
}

TEST_F(GpuHangAdapterTestNoEnv, DestructorNoRunner) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  EXPECT_NE(adapter, nullptr);
  adapter.reset();
  EXPECT_EQ(adapter, nullptr);
}

TEST_F(GpuHangAdapterTestNoEnv, WillProcessTaskNoWatchNoHang) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  adapter->checkHangWatch_ = false;
  adapter->hang_test_ = false;
  base::PendingTask pending_task;
  adapter->WillProcessTask(pending_task, false);
  EXPECT_FALSE(adapter->hang_watch_scope_.has_value());
  EXPECT_EQ(adapter->test_count_, 0);
}

TEST_F(GpuHangAdapterTestNoEnv, WillProcessTaskWithHangWatch) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  adapter->checkHangWatch_ = true;
  base::PendingTask pending_task;
  adapter->WillProcessTask(pending_task, false);
  EXPECT_TRUE(adapter->hang_watch_scope_.has_value());
}

TEST_F(GpuHangAdapterTestNoEnv, WillProcessTaskWithHangWatchWasBlocked) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  adapter->checkHangWatch_ = true;
  base::PendingTask pending_task;
  adapter->WillProcessTask(pending_task, true);
  EXPECT_TRUE(adapter->hang_watch_scope_.has_value());
}

TEST_F(GpuHangAdapterTestNoEnv, WillProcessTaskHangTestTrueModuloFalse) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  adapter->hang_test_ = true;
  adapter->test_count_ = 1;
  base::PendingTask pending_task;
  adapter->WillProcessTask(pending_task, false);
  EXPECT_EQ(adapter->test_count_, 2);
}

TEST_F(GpuHangAdapterTestNoEnv, WillProcessTaskHangTestFalse) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  adapter->hang_test_ = false;
  adapter->test_count_ = 0;
  base::PendingTask pending_task;
  adapter->WillProcessTask(pending_task, false);
  EXPECT_EQ(adapter->test_count_, 0);
}

TEST_F(GpuHangAdapterTestNoEnv, DidProcessTaskWithHangWatch) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  adapter->checkHangWatch_ = true;
  base::PendingTask pending_task;
  adapter->WillProcessTask(pending_task, false);
  EXPECT_TRUE(adapter->hang_watch_scope_.has_value());
  adapter->DidProcessTask(pending_task);
  EXPECT_FALSE(adapter->hang_watch_scope_.has_value());
}

TEST_F(GpuHangAdapterTestNoEnv, DidProcessTaskWithoutHangWatch) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  adapter->checkHangWatch_ = false;
  base::PendingTask pending_task;
  adapter->DidProcessTask(pending_task);
  EXPECT_FALSE(adapter->hang_watch_scope_.has_value());
}

TEST_F(GpuHangAdapterTestWithEnv, CreateAdapterWithRunner) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
  EXPECT_NE(adapter, nullptr);
  EXPECT_EQ(adapter->thread_name_, "test_thread");
}

TEST_F(GpuHangAdapterTestWithEnv, CreateGpuMainWithRunner) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterForGpuMain();
  EXPECT_NE(adapter, nullptr);
  EXPECT_EQ(adapter->thread_name_, "gpu_main");
}

TEST_F(GpuHangAdapterTestWithEnv, CreateCompositorGpuWithRunner) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterForCompositorGpu();
  EXPECT_NE(adapter, nullptr);
  EXPECT_EQ(adapter->thread_name_, "compositor_gpu");
}

TEST_F(GpuHangAdapterTestWithEnv, CreateAdapterHangTestTrueWithRunner) {
  auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("false");
  EXPECT_NE(adapter, nullptr);
  EXPECT_TRUE(adapter->hang_test_);
}

TEST_F(GpuHangAdapterTestWithEnv, DestructorWithRunner) {
  {
    auto adapter = GpuHangAdapter::CreateGpuHangAdapterInner("test_thread");
    EXPECT_NE(adapter, nullptr);
  }
}

}  // namespace gpu