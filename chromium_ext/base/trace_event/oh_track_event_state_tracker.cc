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

#include "base/trace_event/oh_track_event_state_tracker.h"

#include <sstream>

#include "base/check.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "perfetto/ext/base/fnv_hash.h"
#include "perfetto/tracing/internal/track_event_internal.h"
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/trace_packet_defaults.pbzero.h"
#include "protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.pbzero.h"

namespace base {
namespace hitrace {

using perfetto::internal::TrackEventIncrementalState;

namespace {

// Max tracks entries (~1MB at ~200 bytes each). LRU eviction
// prevents unbounded growth from dead tracks.
constexpr size_t kMaxSessionTracks = 4096;

// Evicts the least recently used track when over the limit.
// |protected_uuid| (the descriptor being processed) is never evicted.
void EvictLruTrackIfNeeded(
    OHTrackEventStateTracker::SessionState& session_state,
    uint64_t protected_uuid) {
  if (session_state.tracks.size() <= kMaxSessionTracks) {
    return;
  }
  auto victim = session_state.tracks.end();
  for (auto it = session_state.tracks.begin(); it != session_state.tracks.end();
       ++it) {
    if (it->first == protected_uuid) {
      continue;
    }
    if (victim == session_state.tracks.end() ||
        it->second.last_used < victim->second.last_used) {
      victim = it;
    }
  }
  if (victim != session_state.tracks.end()) {
    session_state.tracks.erase(victim);
  }
}

// Coalescing window for wave-clear of tracks. Each thread notices
// a generation bump at its own pace, so one wave arrives staggered over the
// period. Keep in sync with kHiTraceIncrementalStateClearPeriodMs.
constexpr base::TimeDelta kTracksClearWaveWindow = base::Milliseconds(60000);

}  // namespace

OHTrackEventStateTracker::~OHTrackEventStateTracker() = default;
OHTrackEventStateTracker::Delegate::~Delegate() = default;

OHTrackEventStateTracker::ParsedTrackEvent::ParsedTrackEvent(
    const perfetto::protos::pbzero::TrackEvent::Decoder& track_event_)
    : track_event(track_event_) {}

void OHTrackEventStateTracker::ProcessTracePacket(
    Delegate& delegate,
    SequenceState& sequence_state,
    const perfetto::protos::pbzero::TracePacket::Decoder& packet) {
  UpdateIncrementalState(delegate, sequence_state, packet);

  if (!packet.has_track_event())
    return;

  perfetto::protos::pbzero::TrackEvent::Decoder track_event(
      packet.track_event());

  auto clock_id = packet.timestamp_clock_id();
  if (!packet.has_timestamp_clock_id())
    clock_id = sequence_state.default_clock_id;
  uint64_t timestamp = packet.timestamp();

  if (clock_id == perfetto::internal::TrackEventIncrementalState::kClockIdIncremental) {
    timestamp += sequence_state.most_recent_absolute_time_ns;
    sequence_state.most_recent_absolute_time_ns = timestamp;
  }

  Track* track = &sequence_state.track;
  if (track_event.has_track_uuid()) {
    auto* session_state = delegate.GetSessionState();
    if (!session_state)
      return;
    // Lookup only (never insert); misses fall back to the default
    // track. Insertions happen only in UpdateIncrementalState via
    // TrackDescriptor processing.
    auto track_it = session_state->tracks.find(track_event.track_uuid());
    if (track_it != session_state->tracks.end()) {
      track = &track_it->second;
      track_it->second.last_used = ++session_state->touch_counter;
    }
  }

  protozero::ConstChars category{};
  uint64_t category_iid = 0;
  if (auto iid_it = track_event.category_iids()) {
    category_iid = *iid_it;
    category.data = sequence_state.event_categories[category_iid].data();
    category.size = sequence_state.event_categories[category_iid].size();
  } else if (auto cat_it = track_event.categories()) {
    category.data = reinterpret_cast<const char*>(cat_it->data());
    category.size = cat_it->size();
  }

  protozero::ConstChars name{};
  uint64_t name_iid = track_event.name_iid();
  uint64_t name_hash = 0;
  uint64_t duration = 0;

  if (name_iid) {
    name.data = sequence_state.event_names[name_iid].data();
    name.size = sequence_state.event_names[name_iid].size();
  } else if (track_event.has_name()) {
    name.data = track_event.name().data;
    name.size = track_event.name().size;
  }

  if (name.data) {
    perfetto::base::FnvHasher hash;
    hash.Update(name.data, name.size);
    name_hash = hash.digest();
  }

  auto event_type = track_event.type();
  size_t depth = track->stack.size();

  int32_t phase = 0;
  if (track_event.has_legacy_event()) {
    perfetto::protos::pbzero::TrackEvent::LegacyEvent::Decoder legacy(
        track_event.legacy_event());
    if (legacy.has_phase()) {
      phase = legacy.phase();
    }
  }

  PhaseMapping mapping = MapPhaseToHiTraceType(event_type, phase);

  if (!mapping.use_interceptor ||
      mapping.hitrace_type == HiTraceEventType::UNSUPPORTED) {
    return;
  }

  bool should_callback = false;

  int32_t legacy_phase = 0;
  int64_t counter_value = 0;
  int64_t async_id = 0;
  std::vector<std::string> debug_params;

  switch (event_type) {
    case perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN: {
      StackFrame frame;
      frame.timestamp = timestamp;
      frame.name_hash = name_hash;
      if (track_event.has_track_uuid()) {
        frame.name = name.ToStdString();
        frame.category = category.ToStdString();
      } else {
        frame.name_iid = name_iid;
        frame.category_iid = category_iid;
      }
      track->stack.push_back(std::move(frame));
      break;
    }

    case perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END:
      if (!track->stack.empty()) {
        const auto& prev_frame = track->stack.back();
        if (prev_frame.name_iid) {
          name.data = sequence_state.event_names[prev_frame.name_iid].data();
          name.size = sequence_state.event_names[prev_frame.name_iid].size();
        } else {
          name.data = prev_frame.name.data();
          name.size = prev_frame.name.size();
        }
        name_hash = prev_frame.name_hash;

        if (prev_frame.category_iid) {
          category.data = sequence_state.event_categories[prev_frame.category_iid].data();
          category.size = sequence_state.event_categories[prev_frame.category_iid].size();
        } else {
          category.data = prev_frame.category.data();
          category.size = prev_frame.category.size();
        }

        duration = timestamp - prev_frame.timestamp;
        depth--;
      }
      break;

    case perfetto::protos::pbzero::TrackEvent::TYPE_INSTANT:
      should_callback = true;
      break;

    case perfetto::protos::pbzero::TrackEvent::TYPE_COUNTER:
      if (track_event.has_counter_value()) {
        counter_value = track_event.counter_value();
      } else if (track_event.has_double_counter_value()) {
        counter_value = static_cast<int64_t>(track_event.double_counter_value());
      }
      should_callback = true;
      break;

    case perfetto::protos::pbzero::TrackEvent::TYPE_UNSPECIFIED: {
      switch (mapping.hitrace_type) {
        case HiTraceEventType::COUNTER: {
          if (track_event.has_debug_annotations()) {
            for (auto it = track_event.debug_annotations(); it; ++it) {
              perfetto::protos::pbzero::DebugAnnotation::Decoder annotation(*it);
              if (annotation.has_int_value()) {
                counter_value = annotation.int_value();
                break;
              } else if (annotation.has_uint_value()) {
                counter_value = static_cast<int64_t>(annotation.uint_value());
                break;
              } else if (annotation.has_double_value()) {
                counter_value = static_cast<int64_t>(annotation.double_value());
                break;
              }
            }
          }
          break;
        }

        case HiTraceEventType::ASYNC_BEGIN:
        case HiTraceEventType::ASYNC_END:
        case HiTraceEventType::ASYNC_STEP: {
          if (track_event.has_legacy_event()) {
            perfetto::protos::pbzero::TrackEvent::LegacyEvent::Decoder legacy(
                track_event.legacy_event());
            if (legacy.has_unscoped_id()) {
              async_id = static_cast<int64_t>(legacy.unscoped_id());
            } else if (legacy.has_local_id()) {
              async_id = static_cast<int64_t>(legacy.local_id());
            } else if (legacy.has_global_id()) {
              async_id = static_cast<int64_t>(legacy.global_id());
            }
          }
          break;
        }

        case HiTraceEventType::FLOW: {
          if (track_event.has_legacy_event()) {
            perfetto::protos::pbzero::TrackEvent::LegacyEvent::Decoder legacy(
                track_event.legacy_event());
            if (legacy.has_bind_id()) {
              async_id = static_cast<int64_t>(legacy.bind_id());
            }
          }
          // Add phase information（s=FLOW_BEGIN, t=FLOW_STEP, f=FLOW_END）
          const char* phase_str = "?";
          if (phase == 's') phase_str = "FLOW_BEGIN";
          else if (phase == 't') phase_str = "FLOW_STEP";
          else if (phase == 'f') phase_str = "FLOW_END";
          debug_params.push_back(std::string("phase=") + phase_str);
          break;
        }

        case HiTraceEventType::INSTANT: {
          if (track_event.has_legacy_event()) {
            perfetto::protos::pbzero::TrackEvent::LegacyEvent::Decoder legacy(
                track_event.legacy_event());
            if (legacy.has_instant_event_scope()) {
              const char* scope_str = "UNKNOWN";
              switch (legacy.instant_event_scope()) {
                case static_cast<int32_t>(perfetto::protos::pbzero::TrackEvent::LegacyEvent::InstantEventScope::SCOPE_GLOBAL):
                  scope_str = "GLOBAL";
                  break;
                case static_cast<int32_t>(perfetto::protos::pbzero::TrackEvent::LegacyEvent::InstantEventScope::SCOPE_PROCESS):
                  scope_str = "PROCESS";
                  break;
                case static_cast<int32_t>(perfetto::protos::pbzero::TrackEvent::LegacyEvent::InstantEventScope::SCOPE_THREAD):
                  scope_str = "THREAD";
                  break;
              }
              debug_params.push_back(std::string("scope=") + scope_str);
            }
          }
          break;
        }

        default:
          break;
      }

      legacy_phase = phase;
      should_callback = true;
      break;
    }

    default:
      return;
  }

  if (should_callback) {
    ParsedTrackEvent parsed_event(track_event);
    parsed_event.timestamp_ns = timestamp;
    parsed_event.duration_ns = duration;
    parsed_event.stack_depth = depth;
    parsed_event.category = category;
    parsed_event.name = name;
    parsed_event.name_hash = name_hash;
    parsed_event.legacy_phase = legacy_phase;
    parsed_event.counter_value = counter_value;
    parsed_event.async_id = async_id;
    parsed_event.debug_params = std::move(debug_params);
    parsed_event.hitrace_type = mapping.hitrace_type;

    ParseDebugAnnotations(track_event, sequence_state, parsed_event.debug_params);

    delegate.OnTrackEvent(*track, parsed_event);
  }

  if (event_type == perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END &&
      !track->stack.empty()) {
    track->stack.pop_back();
  }
}

void OHTrackEventStateTracker::UpdateIncrementalState(
    Delegate& delegate,
    SequenceState& sequence_state,
    const perfetto::protos::pbzero::TracePacket::Decoder& packet) {
#if !defined(NDEBUG) && !defined(OS_NDEBUG)
  if (!sequence_state.sequence_id) {
    sequence_state.sequence_id = packet.trusted_packet_sequence_id();
  } else {
    DCHECK(sequence_state.sequence_id ==
           packet.trusted_packet_sequence_id());
  }
#endif

  perfetto::protos::pbzero::ClockSnapshot::Decoder snapshot(
      packet.clock_snapshot());
  for (auto it = snapshot.clocks(); it; ++it) {
    perfetto::protos::pbzero::ClockSnapshot::Clock::Decoder clock(*it);
    if (clock.is_incremental() &&
        clock.clock_id() == perfetto::internal::TrackEventIncrementalState::kClockIdIncremental) {
      sequence_state.most_recent_absolute_time_ns =
          clock.timestamp() * clock.unit_multiplier_ns();
      break;
    }
  }

  if (packet.sequence_flags() &
      perfetto::protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED) {
    auto* session_state = delegate.GetSessionState();
    for (auto& frame : sequence_state.track.stack) {
      if (frame.name_iid) {
        frame.name = sequence_state.event_names[frame.name_iid];
        frame.name_iid = 0u;
      }
      if (frame.category_iid) {
        frame.category = sequence_state.event_categories[frame.category_iid];
        frame.category_iid = 0u;
      }
    }
    sequence_state.event_names.clear();
    sequence_state.event_categories.clear();
    sequence_state.debug_annotation_names.clear();
    sequence_state.track.uuid = 0u;
    sequence_state.track.index = 0u;

    // Wave-clear: ride the periodic clear to drop dead tracks.
    // Guard 1) |established| skips first-packet clears (construction default).
    // Guard 2) |kTracksClearWaveWindow| coalesces staggered flags in one wave.
    if (sequence_state.established) {
      if (session_state &&
          base::TimeTicks::Now() - session_state->last_tracks_clear >=
              kTracksClearWaveWindow) {
        session_state->tracks.clear();
        session_state->last_tracks_clear = base::TimeTicks::Now();
      }
    }
  }

  if (packet.has_interned_data()) {
    perfetto::protos::pbzero::InternedData::Decoder interned_data(
        packet.interned_data());
    for (auto it = interned_data.event_names(); it; it++) {
      perfetto::protos::pbzero::EventName::Decoder entry(*it);
      sequence_state.event_names[entry.iid()] = entry.name().ToStdString();
    }
    for (auto it = interned_data.event_categories(); it; it++) {
      perfetto::protos::pbzero::EventCategory::Decoder entry(*it);
      sequence_state.event_categories[entry.iid()] = entry.name().ToStdString();
    }
    for (auto it = interned_data.debug_annotation_names(); it; it++) {
      perfetto::protos::pbzero::DebugAnnotationName::Decoder entry(*it);
      sequence_state.debug_annotation_names[entry.iid()] =
          entry.name().ToStdString();
    }
  }

  if (packet.has_trace_packet_defaults()) {
    perfetto::protos::pbzero::TracePacketDefaults::Decoder defaults(
        packet.trace_packet_defaults());
    if (defaults.has_track_event_defaults()) {
      perfetto::protos::pbzero::TrackEventDefaults::Decoder
          track_event_defaults(defaults.track_event_defaults());
      sequence_state.track.uuid = track_event_defaults.track_uuid();
      if (defaults.has_timestamp_clock_id())
        sequence_state.default_clock_id = defaults.timestamp_clock_id();
    }
  }

  if (packet.has_track_descriptor()) {
    perfetto::protos::pbzero::TrackDescriptor::Decoder track_descriptor(
        packet.track_descriptor());
    auto* session_state = delegate.GetSessionState();
    if (!session_state) {
        LOG(ERROR) << "[HiTrace-Interceptor] session_state is nullptr";
        return;
    }
    // Only insertion point for SessionState::tracks: the producer
    // emits a TrackDescriptor before the first event referencing the uuid.
    auto& track = session_state->tracks[track_descriptor.uuid()];
    if (!track.index)
      track.index = static_cast<uint32_t>(session_state->tracks.size() + 1);
    track.uuid = track_descriptor.uuid();

    if (track_descriptor.has_name()) {
      track.name = track_descriptor.name().ToStdString();
    } else if (track_descriptor.has_static_name()) {
      track.name = track_descriptor.static_name().ToStdString();
    }
    track.pid = 0;
    track.tid = 0;
    if (track_descriptor.has_process()) {
      perfetto::protos::pbzero::ProcessDescriptor::Decoder process(
          track_descriptor.process());
      track.pid = process.pid();
      if (track.name.empty())
        track.name = process.process_name().ToStdString();
    } else if (track_descriptor.has_thread()) {
      perfetto::protos::pbzero::ThreadDescriptor::Decoder thread(
          track_descriptor.thread());
      track.pid = thread.pid();
      track.tid = thread.tid();
      if (track.name.empty())
        track.name = thread.thread_name().ToStdString();
    }
    delegate.OnTrackUpdated(track);

    if (track.uuid == sequence_state.track.uuid) {
      sequence_state.track.index = track.index;
      sequence_state.track.name = track.name;
      sequence_state.track.pid = track.pid;
      sequence_state.track.tid = track.tid;
      sequence_state.track.user_data = track.user_data;
    }

    // LRU stamp and evict if over limit.
    track.last_used = ++session_state->touch_counter;
    EvictLruTrackIfNeeded(*session_state, track.uuid);
  }

  // Mark this sequence as established (first-packet clear is a
  // construction default, not a service-side periodic clear).
  sequence_state.established = true;
}

void OHTrackEventStateTracker::ParseDebugAnnotations(
    const perfetto::protos::pbzero::TrackEvent::Decoder& track_event,
    SequenceState& sequence_state,
    std::vector<std::string>& params) {
  if (!track_event.has_debug_annotations())
    return;

  for (auto it = track_event.debug_annotations(); it; ++it) {
    perfetto::protos::pbzero::DebugAnnotation::Decoder annotation(*it);

    std::string param_name;
    if (const auto id = annotation.name_iid()) {
      const auto& value = sequence_state.debug_annotation_names[id];
      param_name = std::string(value.data(), value.size());
    } else if (annotation.has_name()) {
      auto name = annotation.name();
      param_name = std::string(name.data, name.size);
    }

    std::string param_value;
    if (annotation.has_bool_value()) {
      param_value = annotation.bool_value() ? "true" : "false";
    } else if (annotation.has_uint_value()) {
      param_value = std::to_string(annotation.uint_value());
    } else if (annotation.has_int_value()) {
      param_value = std::to_string(annotation.int_value());
    } else if (annotation.has_double_value()) {
      param_value = std::to_string(annotation.double_value());
    } else if (annotation.has_string_value()) {
      auto str = annotation.string_value();
      param_value = std::string(str.data, str.size);
    } else if (annotation.has_pointer_value()) {
      std::stringstream ss;
      ss << std::hex << annotation.pointer_value();
      param_value = "0x" + ss.str();
    } else if (annotation.has_legacy_json_value()) {
      auto json = annotation.legacy_json_value();
      param_value = std::string(json.data, json.size);
    }

    if (!param_name.empty() && !param_value.empty()) {
      params.push_back(param_name + "=" + param_value);
    }
  }
}

OHTrackEventStateTracker::PhaseMapping
OHTrackEventStateTracker::MapPhaseToHiTraceType(
    int32_t event_type,
    int32_t legacy_phase) {
  PhaseMapping result;
  result.use_interceptor = false;
  result.hitrace_type = HiTraceEventType::UNSUPPORTED;
  result.description = "Unknown";

  // Process Standard Events
  switch (event_type) {
    case perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN:
      result = {false, HiTraceEventType::UNSUPPORTED,
                "Macro: SLICE_BEGIN lacks timestamp API, use macro instead"};
      return result;
    case perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END:
      result = {false, HiTraceEventType::UNSUPPORTED,
                "Macro: SLICE_END lacks timestamp API, use macro instead"};
      return result;
    case perfetto::protos::pbzero::TrackEvent::TYPE_INSTANT:
      result = {true, HiTraceEventType::INSTANT, "TYPE_INSTANT"};
      return result;
    case perfetto::protos::pbzero::TrackEvent::TYPE_COUNTER:
      result = {true, HiTraceEventType::COUNTER, "TYPE_COUNTER"};
      return result;
    case perfetto::protos::pbzero::TrackEvent::TYPE_UNSPECIFIED:
      break;
    default:
      result = {false, HiTraceEventType::UNSUPPORTED, "Skip: Non-legacy event"};
      return result;
  }

  // Process Legacy Events
  if (legacy_phase == 0) {
    result = {false, HiTraceEventType::UNSUPPORTED, "Skip: No legacy phase"};
    return result;
  }

  // Reference at perfetto/include/perfetto/tracing/internal/track_event_legacy.h
  switch (legacy_phase) {
    case 'B':
      result = { false, HiTraceEventType::UNSUPPORTED, "Macro: Legacy BEGIN needs accurate duration" };
      break;
    case 'E':
      result = { false, HiTraceEventType::UNSUPPORTED, "Macro: Legacy END handled by BEGIN macro" };
      break;
    case 'X':
      result = { false, HiTraceEventType::UNSUPPORTED, "Macro: COMPLETE handled as BEGIN/END" };
      break;
    case 'I':
      result = { true, HiTraceEventType::INSTANT, "Interceptor: Legacy INSTANT" };
      break;
    case 'C':
      result = { true, HiTraceEventType::COUNTER, "Interceptor: Legacy COUNTER" };
      break;
    case 'S':
      result = { true, HiTraceEventType::ASYNC_BEGIN, "Interceptor: ASYNC_BEGIN" };
      break;
    case 'F':
      result = { true, HiTraceEventType::ASYNC_END, "Interceptor: ASYNC_END" };
      break;
    case 'T':
    case 'p':
      result = { true, HiTraceEventType::ASYNC_STEP, "Interceptor: ASYNC_STEP" };
      break;
    case 'b':
      result = { true, HiTraceEventType::ASYNC_BEGIN, "Interceptor: NESTABLE_ASYNC_BEGIN" };
      break;
    case 'e':
      result = { true, HiTraceEventType::ASYNC_END, "Interceptor: NESTABLE_ASYNC_END" };
      break;
    case 'n':
      result = { true, HiTraceEventType::INSTANT, "Interceptor: NESTABLE_ASYNC_INSTANT" };
      break;
    case 'N':
    case 'O':
    case 'D':
      result = { true, HiTraceEventType::INSTANT, "Interceptor: Object event" };
      break;
    case 'P':
      result = { true, HiTraceEventType::INSTANT, "Interceptor: SAMPLE" };
      break;
    case 'R':
      result = { true, HiTraceEventType::INSTANT, "Interceptor: MARK" };
      break;
    case 's':
    case 't':
    case 'f':
      result = { false, HiTraceEventType::UNSUPPORTED,
                 "Interceptor: FLOW event not supported, Reduce the number of instructions" };
      break;
    case 'M':
      result = { false, HiTraceEventType::UNSUPPORTED, "Skip: METADATA not supported" };
      break;
    case 'v':
      result = { false, HiTraceEventType::UNSUPPORTED, "Skip: MEMORY_DUMP not supported" };
      break;
    case 'c':
      result = { false, HiTraceEventType::UNSUPPORTED, "Skip: CLOCK_SYNC not supported" };
      break;
    default:
      result = { false, HiTraceEventType::UNSUPPORTED, "Skip: Unknown phase" };
      break;
  }

  return result;
}

}  // namespace hitrace
}  // namespace base
