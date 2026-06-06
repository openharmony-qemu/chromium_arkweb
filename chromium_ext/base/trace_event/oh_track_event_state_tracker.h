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

#ifndef BASE_TRACE_EVENT_OH_TRACK_EVENT_STATE_TRACKER_H_
#define BASE_TRACE_EVENT_OH_TRACK_EVENT_STATE_TRACKER_H_

#include <map>
#include <string>
#include <vector>
#include <cstdint>

#include "protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace perfetto {
namespace protos {
namespace pbzero {
class TracePacket_Decoder;
class TrackEvent;
class TrackEvent_Decoder;
class TrackEvent_LegacyEvent_Decoder;
}  // namespace pbzero
}  // namespace protos
}  // namespace perfetto

namespace base {
namespace hitrace {

enum class HiTraceEventType {
  BEGIN = 0,
  END = 1,
  INSTANT = 2,
  ASYNC_BEGIN = 3,
  ASYNC_END = 4,
  ASYNC_STEP = 5,
  COUNTER = 6,
  FLOW = 7,
  UNSUPPORTED = 8
};

class OHTrackEventStateTracker {
 public:
  ~OHTrackEventStateTracker();

  struct StackFrame {
    uint64_t timestamp{};

    // Only one of |name| and |name_iid| will be set.
    std::string name;
    uint64_t name_iid{};
    uint64_t name_hash{};

    // Only one of |category| and |category_iid| will be set.
    std::string category;
    uint64_t category_iid{};
  };

  struct Track {
    uint64_t uuid{};
    uint32_t index{};

    std::string name;
    int64_t pid{};
    int64_t tid{};

    // Opaque user data associated with the track.
    std::vector<uint8_t> user_data;

    // Stack of opened slices on this track.
    std::vector<StackFrame> stack;
  };

  struct SequenceState {
    // Trace packet sequence defaults.
    Track track;

#if !defined(NDEBUG) && !defined(OS_NDEBUG)
    uint32_t sequence_id{};
#endif

    std::map<uint64_t /*iid*/, std::string> event_names;
    std::map<uint64_t /*iid*/, std::string> event_categories;
    std::map<uint64_t /*iid*/, std::string> debug_annotation_names;

    uint64_t most_recent_absolute_time_ns = 0;

    uint32_t default_clock_id = 0;
  };

  struct SessionState {
    // Non-thread-bound tracks.
    std::map<uint64_t /*uuid*/, Track> tracks;
  };

  struct ParsedTrackEvent {
    explicit ParsedTrackEvent(
        const perfetto::protos::pbzero::TrackEvent_Decoder&);

    const perfetto::protos::pbzero::TrackEvent_Decoder& track_event;

    uint64_t timestamp_ns{};
    uint64_t duration_ns{};

    size_t stack_depth{};

    protozero::ConstChars category{};
    protozero::ConstChars name{};
    uint64_t name_hash{};

    int32_t legacy_phase{};
    std::vector<std::string> debug_params;
    int64_t async_id{};
    int64_t counter_value{};
    HiTraceEventType hitrace_type{HiTraceEventType::UNSUPPORTED};
  };

  class Delegate {
   public:
    virtual ~Delegate();

    virtual SessionState* GetSessionState() = 0;

    virtual void OnTrackUpdated(Track&) = 0;

    virtual void OnTrackEvent(const Track&, const ParsedTrackEvent&) = 0;
  };

  // Process Standard Events
  static void ProcessTracePacket(
      Delegate&,
      SequenceState&,
      const perfetto::protos::pbzero::TracePacket_Decoder&);

  // Process Legacy Events
  static void ProcessTracePacketWithLegacy(
      Delegate&,
      SequenceState&,
      const perfetto::protos::pbzero::TracePacket_Decoder&);

 private:
  static void UpdateIncrementalState(
      Delegate&,
      SequenceState&,
      const perfetto::protos::pbzero::TracePacket_Decoder&);

  static void ParseDebugAnnotations(
      const perfetto::protos::pbzero::TrackEvent::Decoder&,
      SequenceState&,
      std::vector<std::string>& params);

  struct PhaseMapping {
    bool use_interceptor;
    HiTraceEventType hitrace_type;
    const char* description;
  };

  static PhaseMapping MapPhaseToHiTraceType(
      int32_t event_type,
      int32_t legacy_phase);
};

}  // namespace hitrace
}  // namespace base

#endif  // BASE_TRACE_EVENT_OH_TRACK_EVENT_STATE_TRACKER_H_
