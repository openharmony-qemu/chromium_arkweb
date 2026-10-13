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

#include "base/trace_event/hitrace_export_oh.h"

#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)

#include <memory>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/trace_event/hitrace_interceptor_oh.h"
#include "perfetto/tracing/tracing.h"
#include "protos/perfetto/config/interceptor_config.gen.h"
#include "protos/perfetto/config/track_event/track_event_config.gen.h"
#include "protos/perfetto/config/trace_config.gen.h"

namespace base {
namespace hitrace {

namespace {

constexpr const char* const kHiTraceDisabledCategories[] = {
    "scheduler.long_tasks",
    "toplevel.flow",
    "wakeup.flow",
    "mojom.flow",
    "log",
    "sequence_manager",
    "net.stream",
    "net",
    "extensions",
    "disabled-by-default-*",
};

// Periodic incremental-state clear period. Bounds memory of
// per-sequence interning dictionaries (~2MB/5min growth without it).
constexpr uint32_t kHiTraceIncrementalStateClearPeriodMs = 60000;

// Minimum legal buffer for TracingServiceImpl validation.
// The interceptor bypasses shared memory, so this is never written to.
constexpr uint32_t kHiTraceSessionBufferSizeKb = 4;

perfetto::TraceConfig CreateTraceConfigForHiTrace() {
  perfetto::TraceConfig config;

  auto* data_source = config.add_data_sources();
  auto* data_source_config = data_source->mutable_config();
  data_source_config->set_name("track_event");

  perfetto::protos::gen::TrackEventConfig track_event_config;
  track_event_config.add_enabled_categories("*");
  // Disable specific single categories at the Perfetto fast-path. The disabled
  // exact name wins over the "*" wildcard, so the event never reaches the
  // interceptor (no OnTracePacket, no forwarding cost).
  for (auto category : kHiTraceDisabledCategories) {
    track_event_config.add_disabled_categories(category);
  }
  data_source_config->set_track_event_config_raw(
      track_event_config.SerializeAsString());

  data_source_config->mutable_interceptor_config()->set_name("hitrace_interceptor");

  // Consumer session requires a non-zero buffer to pass validation.
  config.add_buffers()->set_size_kb(kHiTraceSessionBufferSizeKb);
  config.mutable_incremental_state_config()->set_clear_period_ms(
      kHiTraceIncrementalStateClearPeriodMs);

  return config;
}

class HiTraceExportController {
 public:
  HiTraceExportController();
  ~HiTraceExportController() = delete;

  void StartStartupTracing();

 private:
  bool is_registration_complete_ = false;

  std::unique_ptr<perfetto::StartupTracingSession> tracing_session_;
};

HiTraceExportController::HiTraceExportController() {
  HiTraceInterceptor::Register();
  is_registration_complete_ = true;

  if (HiTraceInterceptor::IsHiTraceAvailable()) {
    StartStartupTracing();
  }
}

void HiTraceExportController::StartStartupTracing() {
  if (!is_registration_complete_) {
    return;
  }

  // Setup startup tracing with no timeout (runs continuously)
  perfetto::Tracing::SetupStartupTracingOpts opts;
  opts.timeout_ms = 0;
  opts.backend = perfetto::kCustomBackend;

  perfetto::TraceConfig config = CreateTraceConfigForHiTrace();
  tracing_session_ = perfetto::Tracing::SetupStartupTracing(config, opts);
  LOG(INFO) << "[HiTrace-Interceptor] SetupStartupTracing completed";
}

// Browser-process consumer session handle (NoDestructor, lives until
// process exit). Null in child processes.
std::unique_ptr<perfetto::TracingSession>& HiTraceConsumerSession() {
  static base::NoDestructor<std::unique_ptr<perfetto::TracingSession>> session;
  return *session;
}

}  // namespace

void EnableHiTraceExport() {
  static base::NoDestructor<HiTraceExportController> controller;
  (void)controller;
}

void StartHiTraceConsumerSession() {
  // Keep the availability gate consistent with the startup session.
  if (!HiTraceInterceptor::IsHiTraceAvailable()) {
    return;
  }
  if (HiTraceConsumerSession()) {
    return;
  }

  // Same config as the startup session so the service adopts existing
  // instances instead of creating duplicates.
  perfetto::TraceConfig config = CreateTraceConfigForHiTrace();
  auto session = perfetto::Tracing::NewTrace(perfetto::kCustomBackend);
  session->Setup(config);
  session->Start();
  HiTraceConsumerSession() = std::move(session);
  LOG(INFO) << "[HiTrace-Interceptor] consumer session started"
            << " (clear_period_ms=" << kHiTraceIncrementalStateClearPeriodMs
            << ")";
}

void StopHiTraceConsumerSession() {
  if (HiTraceConsumerSession()) {
    HiTraceConsumerSession()->StopBlocking();
    HiTraceConsumerSession().reset();
    LOG(INFO) << "[HiTrace-Interceptor] consumer session stopped";
  }
}

}  // namespace hitrace
}  // namespace base

#endif  // BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
