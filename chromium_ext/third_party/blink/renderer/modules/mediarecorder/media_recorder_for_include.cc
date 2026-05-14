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

#include "base/logging.h"

namespace blink {

#if BUILDFLAG(ARKWEB_MEDIA_CAPTURE)
void MediaRecorder::InternalPause() {
  LOG(INFO) << "MediaRecorder::InternalPause, enter";
  if (state_ != State::kRecording) {
    LOG(INFO) << "InternalPause state is recording";
    return;
  }
  state_ = State::kPaused;
  recorder_handler_->Pause();
  ScheduleDispatchEvent(Event::Create(event_type_names::kPause));
}
 
void MediaRecorder::InternalResume() {
  LOG(INFO) << "MediaRecorder::InternalResume, enter";
  if (state_ != State::kPaused) {
    LOG(INFO) << "InternalResume state is paused";
    return;
  }
  state_ = State::kRecording;
  recorder_handler_->Resume();
  ScheduleDispatchEvent(Event::Create(event_type_names::kResume));
}
#endif // ARKWEB_MEDIA_CAPTURE

} // namespace blink