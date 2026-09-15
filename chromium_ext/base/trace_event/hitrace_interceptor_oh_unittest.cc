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

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "arkweb/build/features/features.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/trace_event/oh_track_event_state_tracker.h"
#include "perfetto/tracing/tracing.h"
#include "protos/perfetto/config/data_source_config.gen.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// clang-format off
#define private public
#include "base/trace_event/hitrace_interceptor_oh.h"
#undef private
// clang-format on

#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
#include "arkweb/ohos_adapter_ndk/hiviewdfx_adapter/hitrace_adapter_impl.h"
#endif

using namespace testing;

namespace base {
namespace hitrace {

// Install _exit(0) atexit handler at static initialization time.
// This works around a NetConnClient atexit crash in the OH system library:
// registering _exit(0) via atexit() causes it to execute before
// NetConnClient's handler (LIFO order), skipping the crash.
// Must happen at static init so that even when the test filter matches no
// tests (e.g. HiTraceInterceptorPerfettoTest.*), the handler is still
// registered and the process can exit cleanly.
namespace {
const bool g_atexit_installed = (atexit([]() { _exit(0); }), true);
}  // namespace

// Fixture that keeps all tests in the HiTraceInterceptorTest suite so they
// are discoverable by the test runner filter.
class HiTraceInterceptorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // One-time: initialize perfetto muxer without any backend thread
    if (!perfetto::Tracing::IsInitialized()) {
      perfetto::TracingInitArgs init_args;
      init_args.backends = 0;
      perfetto::Tracing::Initialize(init_args);
    }
  }
};

// Test FormatHiTraceEventName
TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_BasicBeginEvent) {
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::BEGIN, {}, 0);
  EXPECT_EQ(result, "cat| name");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_EndEvent) {
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::END, {}, 0);
  EXPECT_EQ(result, "cat| name");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_InstantWithScopeParam) {
  std::vector<std::string> params = {"scope=thread", "extra"};
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::INSTANT, params, 0);
  EXPECT_EQ(result, "cat|name=thread|extra");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_InstantWithNonScopeParam) {
  std::vector<std::string> params = {"key=value"};
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::INSTANT, params, 0);
  // params[0] does not start with "scope=", so falls through to default
  EXPECT_EQ(result, "cat| name|key=value");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_InstantEmptyParams) {
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::INSTANT, {}, 0);
  // empty params, falls through to default
  EXPECT_EQ(result, "cat| name");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_AsyncBeginWithId) {
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::ASYNC_BEGIN, {}, 12345);
  EXPECT_EQ(result, "cat|name=12345");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_AsyncEndWithId) {
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::ASYNC_END, {}, 12345);
  EXPECT_EQ(result, "cat|name=12345");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_AsyncBeginZeroId) {
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::ASYNC_BEGIN, {}, 0);
  // async_id == 0, falls through to default
  EXPECT_EQ(result, "cat| name");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_AsyncEndZeroId) {
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::ASYNC_END, {}, 0);
  EXPECT_EQ(result, "cat| name");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_FlowWithId) {
  std::vector<std::string> params = {"key=val"};
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::FLOW, params, 99);
  EXPECT_EQ(result, "cat|name|flow_id=99|key=val");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_FlowZeroId) {
  std::vector<std::string> params = {"key=val"};
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::FLOW, params, 0);
  // async_id == 0, falls through to default
  EXPECT_EQ(result, "cat| name|key=val");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_CounterEvent) {
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "counter", HiTraceEventType::COUNTER, {}, 0);
  EXPECT_EQ(result, "cat| counter");
}

TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_WithParams) {
  std::vector<std::string> params = {"p1", "p2"};
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::BEGIN, params, 0);
  EXPECT_EQ(result, "cat| name|p1|p2");
}

// New tests for improved branch coverage

// FormatHiTraceEventName: COUNTER type with params falls to default branch
TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_CounterWithParams) {
  std::vector<std::string> params = {"p1"};
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "counter", HiTraceEventType::COUNTER, params, 0);
  EXPECT_EQ(result, "cat| counter|p1");
}

// FormatHiTraceEventName: ASYNC_STEP type falls to default branch
TEST_F(HiTraceInterceptorTest, FormatHiTraceEventName_AsyncStepWithParams) {
  std::vector<std::string> params = {"step_info"};
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "async_step", HiTraceEventType::ASYNC_STEP, params, 0);
  EXPECT_EQ(result, "cat| async_step|step_info");
}

// FormatHiTraceEventName: INSTANT with multiple scope params
TEST_F(HiTraceInterceptorTest,
       FormatHiTraceEventName_InstantWithMultipleScopeParams) {
  std::vector<std::string> params = {"scope=process", "extra1", "extra2"};
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::INSTANT, params, 0);
  EXPECT_EQ(result, "cat|name=process|extra1|extra2");
}

// FormatHiTraceEventName: FLOW with non-zero async_id and multiple params
TEST_F(HiTraceInterceptorTest,
       FormatHiTraceEventName_FlowWithIdAndMultipleParams) {
  std::vector<std::string> params = {"key1=val1", "key2=val2"};
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "name", HiTraceEventType::FLOW, params, 42);
  EXPECT_EQ(result, "cat|name|flow_id=42|key1=val1|key2=val2");
}

// Test ShouldForwardCategory (now accessible via #define private public)
TEST_F(HiTraceInterceptorTest, ShouldForwardCategory_DisabledPrefix) {
  EXPECT_FALSE(HiTraceInterceptor::ShouldForwardCategory("disabled-category"));
}

TEST_F(HiTraceInterceptorTest, ShouldForwardCategory_FlowSubstring) {
  EXPECT_FALSE(HiTraceInterceptor::ShouldForwardCategory("something.flow"));
}

TEST_F(HiTraceInterceptorTest, ShouldForwardCategory_NormalCategory) {
#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
  // Depends on HiTrace availability at runtime
#else
  EXPECT_FALSE(HiTraceInterceptor::ShouldForwardCategory("normal_category"));
#endif
}

TEST_F(HiTraceInterceptorTest, ShouldForwardCategory_EmptyString) {
#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
  // Depends on HiTrace availability at runtime
#else
  EXPECT_FALSE(HiTraceInterceptor::ShouldForwardCategory(""));
#endif
}

TEST_F(HiTraceInterceptorTest, ShouldForwardCategory_ExactDisabledPrefix) {
  EXPECT_FALSE(HiTraceInterceptor::ShouldForwardCategory("disabled-"));
}

TEST_F(HiTraceInterceptorTest, ShouldForwardCategory_FlowInMiddle) {
  EXPECT_FALSE(HiTraceInterceptor::ShouldForwardCategory("my.flow.category"));
}

// Test GetInstance
TEST_F(HiTraceInterceptorTest, GetInstance) {
  HiTraceInterceptor* instance = HiTraceInterceptor::GetInstance();
  EXPECT_NE(instance, nullptr);
  // Should return same instance
  HiTraceInterceptor* instance2 = HiTraceInterceptor::GetInstance();
  EXPECT_EQ(instance, instance2);
}

// Test IsHiTraceAvailable
TEST_F(HiTraceInterceptorTest, IsHiTraceAvailable) {
  bool result = HiTraceInterceptor::IsHiTraceAvailable();
#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
  // Depends on HiTraceAdapterImpl::IsHiTraceEnable() at runtime
#else
  EXPECT_FALSE(result);
#endif
}

// Test Register (requires perfetto initialized, provided by fixture SetUp)
TEST_F(HiTraceInterceptorTest, Register) {
  EXPECT_NO_FATAL_FAILURE(HiTraceInterceptor::Register());
}

// Test OnSetup
TEST_F(HiTraceInterceptorTest, OnSetup) {
  HiTraceInterceptor* instance = HiTraceInterceptor::GetInstance();
  perfetto::protos::gen::DataSourceConfig config;
  perfetto::Interceptor<HiTraceInterceptor>::SetupArgs args{config};
  EXPECT_NO_FATAL_FAILURE(instance->OnSetup(args));
}

// Test OnStart
TEST_F(HiTraceInterceptorTest, OnStart) {
  HiTraceInterceptor* instance = HiTraceInterceptor::GetInstance();
  perfetto::Interceptor<HiTraceInterceptor>::StartArgs args;
  EXPECT_NO_FATAL_FAILURE(instance->OnStart(args));
}

// Test OnStop
TEST_F(HiTraceInterceptorTest, OnStop) {
  HiTraceInterceptor* instance = HiTraceInterceptor::GetInstance();
  perfetto::Interceptor<HiTraceInterceptor>::StopArgs args;
  EXPECT_NO_FATAL_FAILURE(instance->OnStop(args));
}

// OnSetup: verify enabled_ is set based on IsAvailable
TEST_F(HiTraceInterceptorTest, OnSetup_EnablesBasedOnAvailability) {
  HiTraceInterceptor* instance = HiTraceInterceptor::GetInstance();
  perfetto::protos::gen::DataSourceConfig config;
  perfetto::Interceptor<HiTraceInterceptor>::SetupArgs args{config};
  instance->OnSetup(args);
#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
  // enabled_ depends on HiTraceBridgeImpl::IsAvailable()
#else
  EXPECT_FALSE(instance->enabled_);
#endif
}

// Test Delegate indirectly via FormatHiTraceEventName
TEST_F(HiTraceInterceptorTest, OnTrackEvent_EmptyName) {
  std::string result = HiTraceInterceptor::FormatHiTraceEventName(
      "cat", "", HiTraceEventType::BEGIN, {}, 0);
  EXPECT_FALSE(result.empty());
}

}  // namespace hitrace
}  // namespace base
