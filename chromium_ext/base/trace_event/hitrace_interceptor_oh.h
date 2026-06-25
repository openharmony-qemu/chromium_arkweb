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

#ifndef BASE_TRACE_EVENT_HITRACE_INTERCEPTOR_OH_H_
#define BASE_TRACE_EVENT_HITRACE_INTERCEPTOR_OH_H_

#include "base/base_export.h"
#include <map>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include "third_party/perfetto/include/perfetto/tracing/interceptor.h"
#include "base/trace_event/oh_track_event_state_tracker.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace perfetto {
class TracingSession;
}

namespace base {
namespace hitrace {

class BASE_EXPORT HiTraceInterceptor
    : public perfetto::Interceptor<HiTraceInterceptor> {
 public:
  void OnSetup(const SetupArgs& args) override;

  void OnStart(const StartArgs& args) override;

  void OnStop(const StopArgs& args) override;

  static void OnTracePacket(InterceptorContext context);

  static HiTraceInterceptor* GetInstance();

  static bool IsHiTraceAvailable();

  static void Register();

  static std::string FormatHiTraceEventName(
      const std::string& category_str,
      const std::string& name_str,
      HiTraceEventType hitrace_type,
      const std::vector<std::string>& params,
      int64_t async_id);

  // Thread-local state for the interceptor
  struct ThreadLocalState : public perfetto::InterceptorBase::ThreadLocalState {
    explicit ThreadLocalState(
        perfetto::Interceptor<HiTraceInterceptor>::ThreadLocalStateArgs&) {}
    ~ThreadLocalState() override = default;

    base::hitrace::OHTrackEventStateTracker::SequenceState sequence_state;
  };

  HiTraceInterceptor() = default;

  HiTraceInterceptor(const HiTraceInterceptor&) = delete;
  HiTraceInterceptor& operator=(const HiTraceInterceptor&) = delete;

 private:
  class Delegate;

  base::hitrace::OHTrackEventStateTracker::SessionState session_state_;

  // Category filtering: Check whether the category should be traced.
  static bool ShouldForwardCategory(const std::string& category_str);

  bool enabled_ = false;
};

}  // namespace hitrace
}  // namespace base

#endif  // BASE_TRACE_EVENT_HITRACE_INTERCEPTOR_H_
