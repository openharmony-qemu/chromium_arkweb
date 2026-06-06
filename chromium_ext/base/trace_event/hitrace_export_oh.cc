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

perfetto::TraceConfig CreateTraceConfigForHiTrace() {
  perfetto::TraceConfig config;

  auto* data_source = config.add_data_sources();
  auto* data_source_config = data_source->mutable_config();
  data_source_config->set_name("track_event");

  perfetto::protos::gen::TrackEventConfig track_event_config;
  track_event_config.add_enabled_categories("*");
  data_source_config->set_track_event_config_raw(
      track_event_config.SerializeAsString());

  data_source_config->mutable_interceptor_config()->set_name("hitrace_interceptor");

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

}  // namespace

// static
void EnableHiTraceExport() {
  static base::NoDestructor<HiTraceExportController> controller;
  (void)controller;
}

}  // namespace hitrace
}  // namespace base

#endif  // BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
