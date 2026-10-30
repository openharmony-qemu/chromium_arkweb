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

#include "base/trace_event/hitrace_interceptor_oh.h"

#include <atomic>
#include <inttypes.h>
#include <iomanip>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event_ohos.h"
#include "base/tracing_buildflags.h"
#include "perfetto/tracing/tracing.h"
#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/internal/track_event_internal.h"
#include "protos/perfetto/common/interceptor_descriptor.gen.h"
#include "perfetto/protozero/proto_decoder.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/track_event/track_event.pbzero.h"
#include "protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "protos/perfetto/config/interceptor_config.gen.h"
#include "protos/perfetto/config/track_event/track_event_config.gen.h"
#include "protos/perfetto/config/data_source_config.gen.h"
#include "protos/perfetto/config/trace_config.gen.h"

#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
#include "arkweb/ohos_adapter_ndk/hiviewdfx_adapter/hitrace_adapter_impl.h"
#endif

namespace base {
namespace hitrace {

class HiTraceBridgeImpl {
 public:
  static int32_t Hash64To32(int64_t id) {
    return static_cast<int32_t>((id >> 32) ^ (id & 0xFFFFFFFF));
  }

  static bool IsAvailable() {
#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
    return OHOS::NWeb::HiTraceAdapterImpl::GetInstance().IsHiTraceEnable();
#else
    return false;
#endif
  }

  static void ForwardEvent(const char* category,
                           const char* name,
                           HiTraceEventType type,
                           uint64_t track_uuid = 0,
                           uint64_t timestamp_ns = 0,
                           size_t stack_depth = 0,
                           int64_t task_id = 0,
                           int64_t counter_value = 0) {
#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
    auto& hitrace = OHOS::NWeb::HiTraceAdapterImpl::GetInstance();

    std::string event_name;
    if (name) {
      event_name = name;
    } else {
      return;
    }

    switch (type) {
      case HiTraceEventType::BEGIN: {
        hitrace.StartTrace(event_name, -1.0f);
        break;
      }
      case HiTraceEventType::END: {
        hitrace.FinishTrace();
        break;
      }
      case HiTraceEventType::INSTANT: {
        hitrace.StartTrace(event_name, -1.0f);
        hitrace.FinishTrace();
        break;
      }
      case HiTraceEventType::ASYNC_BEGIN: {
        if (task_id != 0) {
          hitrace.StartAsyncTrace(event_name, Hash64To32(task_id), -1.0f);
        }
        break;
      }
      case HiTraceEventType::ASYNC_END: {
        if (task_id != 0) {
          hitrace.FinishAsyncTrace(event_name, Hash64To32(task_id));
        }
        break;
      }
      case HiTraceEventType::ASYNC_STEP: {
        hitrace.StartTrace(event_name, -1.0f);
        hitrace.FinishTrace();
        break;
      }
      case HiTraceEventType::COUNTER: {
        hitrace.CountTrace(event_name, static_cast<int>(counter_value));
        break;
      }
      case HiTraceEventType::FLOW: {
        hitrace.StartTrace(event_name, -1.0f);
        hitrace.FinishTrace();
        break;
      }
      default:
        break;
    }
#endif
  }
};

class HiTraceInterceptor::Delegate
    : public base::hitrace::OHTrackEventStateTracker::Delegate {
 public:
  Delegate(perfetto::LockedHandle<HiTraceInterceptor> locked_self,
           base::hitrace::OHTrackEventStateTracker::SequenceState& sequence_state)
      : sequence_state_(sequence_state), locked_self_(std::move(locked_self)) {
    DCHECK(locked_self_);
  }

  ~Delegate() override = default;

  base::hitrace::OHTrackEventStateTracker::SessionState* GetSessionState() override {
    return &locked_self_->session_state_;
  }

  void OnTrackUpdated(base::hitrace::OHTrackEventStateTracker::Track& track) override {
  }

  void OnTrackEvent(
      const base::hitrace::OHTrackEventStateTracker::Track& track,
      const base::hitrace::OHTrackEventStateTracker::ParsedTrackEvent& event) override {
    std::string category_str(event.category.data, event.category.size);
    std::string name_str(event.name.data, event.name.size);

    // Filter empty name event
    if (name_str.empty()) {
      return;
    }

    if (!HiTraceInterceptor::ShouldForwardCategory(category_str)) {
      return;
    }

    std::string hitrace_event_name = HiTraceInterceptor::FormatHiTraceEventName(
        category_str, name_str,
        event.hitrace_type,
        event.debug_params, event.async_id);

    HiTraceBridgeImpl::ForwardEvent(
        category_str.c_str(),
        hitrace_event_name.c_str(),
        event.hitrace_type,
        track.uuid,
        event.timestamp_ns,
        event.stack_depth,
        event.async_id,
        event.counter_value);
  }

 private:
  raw_ref<base::hitrace::OHTrackEventStateTracker::SequenceState> sequence_state_;
  perfetto::LockedHandle<HiTraceInterceptor> locked_self_;
};

HiTraceInterceptor* HiTraceInterceptor::GetInstance() {
  static base::NoDestructor<HiTraceInterceptor> instance;
  return instance.get();
}

bool HiTraceInterceptor::IsHiTraceAvailable() {
  return HiTraceBridgeImpl::IsAvailable();
}

void HiTraceInterceptor::Register() {
  perfetto::protos::gen::InterceptorDescriptor descriptor;
  descriptor.set_name("hitrace_interceptor");
  perfetto::Interceptor<HiTraceInterceptor>::Register(descriptor);
}

std::string HiTraceInterceptor::FormatHiTraceEventName(
    const std::string& category_str,
    const std::string& name_str,
    HiTraceEventType hitrace_type,
    const std::vector<std::string>& params,
    int64_t async_id) {
  std::string hitrace_event_name = category_str + "|";

  if (hitrace_type == HiTraceEventType::INSTANT && !params.empty() &&
      params[0].find("scope=") == 0) {
    hitrace_event_name += name_str + "=" + params[0].substr(6);
    for (size_t i = 1; i < params.size(); i++) {
      hitrace_event_name += "|" + params[i];
    }
  } else if ((hitrace_type == HiTraceEventType::ASYNC_BEGIN ||
              hitrace_type == HiTraceEventType::ASYNC_END) && async_id != 0) {
    hitrace_event_name += name_str + "=" + std::to_string(async_id);
  } else if (hitrace_type == HiTraceEventType::FLOW && async_id != 0) {
    hitrace_event_name += name_str + "|flow_id=" + std::to_string(async_id);
    for (const auto& param : params) {
      hitrace_event_name += "|" + param;
    }
  } else {
    hitrace_event_name += " " + name_str;
    for (const auto& param : params) {
      hitrace_event_name += "|" + param;
    }
  }

  return hitrace_event_name;
}

void HiTraceInterceptor::OnSetup(const SetupArgs& args) {
#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
  enabled_ = HiTraceBridgeImpl::IsAvailable();
#else
  enabled_ = false;
#endif
}

void HiTraceInterceptor::OnStart(const StartArgs&) {
  LOG(INFO) << "[HiTrace-Interceptor] ========== Interceptor Started ==========";
}

void HiTraceInterceptor::OnStop(const StopArgs&) {
  LOG(INFO) << "[HiTrace-Interceptor] ========== Interceptor Stop Summary ==========";
}

void HiTraceInterceptor::OnTracePacket(InterceptorContext context) {
  auto& tls = context.GetThreadLocalState();
  perfetto::LockedHandle<HiTraceInterceptor> locked_self =
      context.GetInterceptorLocked();
  if (!locked_self) {
    return;
  }

#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
  if (!HiTraceBridgeImpl::IsAvailable()) {
    return;
  }
#else
  return;
#endif

  Delegate delegate(std::move(locked_self), tls.sequence_state);

  // Defensive guard: a well-formed TrackEvent packet (event +
  // interned data + clock snapshot) is a few hundred bytes at most. A packet
  // larger than 1 MiB means the buffer is corrupted or stitched wrongly; feed
  // it to TypedProtoDecoderBase::ParseAllFields() and repeated fields can
  // drive ExpandHeapStorage() doubling up to >2GiB, which PartitionAlloc
  // aborts on (PartitionExcessiveAllocationSize). Drop it and log instead.
  constexpr size_t kMaxInterceptorPacketSize = 1 << 20;  // 1 MiB
  if (context.packet_data.size > kMaxInterceptorPacketSize) {
    LOG(ERROR) << "[HiTrace-Interceptor] dropping oversized packet: "
               << context.packet_data.size << " bytes";
    return;
  }

  if (!context.packet_data.data || context.packet_data.size == 0) {
    LOG(ERROR) << "[HiTrace-Interceptor] dropping packet with null/empty data, size="
              << context.packet_data.size;
    return;
  }

  {
    protozero::ProtoDecoder packet_scan(context.packet_data.data,
                                        context.packet_data.size);
    protozero::Field track_event_field =
        packet_scan.FindField(11 /* TracePacket::track_event */);
    if (track_event_field.valid()) {
      protozero::ProtoDecoder track_event_scan(
          reinterpret_cast<const uint8_t*>(track_event_field.data()),
          track_event_field.size());
      protozero::Field type_field =
          track_event_scan.FindField(9 /* TrackEvent::type */);
      uint32_t event_type = type_field.valid() ? type_field.as_uint32() : 0u;
      if (event_type ==
              static_cast<uint32_t>(perfetto::protos::pbzero::TrackEvent::
                                        TYPE_SLICE_BEGIN) ||
          event_type ==
              static_cast<uint32_t>(perfetto::protos::pbzero::TrackEvent::
                                        TYPE_SLICE_END)) {
        return;
      }
    }
  }

  perfetto::protos::pbzero::TracePacket::Decoder packet(
      context.packet_data.data, context.packet_data.size);

  base::hitrace::OHTrackEventStateTracker::ProcessTracePacket(
      delegate, tls.sequence_state, packet);
}

bool HiTraceInterceptor::ShouldForwardCategory(const std::string& category_str) {
#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_DFX_TRACING)
  if (!HiTraceBridgeImpl::IsAvailable()) {
    return false;
  }

  constexpr std::string_view kDisabledPrefix = "disabled-";
  if (base::StartsWith(category_str, kDisabledPrefix)) {
    return false;
  }

  constexpr std::string_view kExcludedSubstrings[] = {
      ".flow",
  };
  for (auto sub : kExcludedSubstrings) {
    if (category_str.find(sub) != std::string::npos) {
      return false;
    }
  }
  return true;
#else
  return false;
#endif
}

}  // namespace hitrace
}  // namespace base
