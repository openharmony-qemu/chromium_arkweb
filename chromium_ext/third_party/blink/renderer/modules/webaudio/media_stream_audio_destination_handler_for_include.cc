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

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/webaudio_media_stream_source.h"

namespace blink {

#if BUILDFLAG(ARKWEB_MEDIA_CAPTURE)
void MediaStreamAudioDestinationHandler::OnSourcePausedChanged(
    bool is_paused) {
  LOG(INFO) << "MediaStreamAudioDestinationHandler::OnSourcePausedChanged";
  DCHECK(IsMainThread());
  base::AutoLock locker(consumer_lock_);
  if (destination_consumer_) {
    // The consumer is a WebAudioMediaStreamSource, which inherits from both
    // WebAudioDestinationConsumer and MediaStreamAudioSource.
    // SetPausedState() will propagate the pause state to all tracks connected
    // to this source, which will reach AudioTrackRecorder and ultimately
    // MediaRecorder::InternalPause/Resume().
    auto* audio_source = static_cast<WebAudioMediaStreamSource*>(
        destination_consumer_.get());
    audio_source->SetPausedState(is_paused);
  }
}
#endif // ARKWEB_MEDIA_CAPTURE

} // namespace blink