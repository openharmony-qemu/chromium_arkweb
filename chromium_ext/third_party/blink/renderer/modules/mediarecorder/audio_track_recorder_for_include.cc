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

namespace blink {

#if BUILDFLAG(ARKWEB_MEDIA_CAPTURE)
void AudioTrackRecorder::OnSourcePausedChanged(bool is_paused) {
  LOG(INFO) << "AudioTrackRecorder::OnSourcePausedChanged, is_paused: " << is_paused;
  auto* callback = callback_interface_->Get();
  if (!callback) {
    LOG(ERROR) << "AudioTrackRecorder::OnSourcePausedChanged callback is nullptr";
    return;
  }
 
  if (is_paused) {
    callback->OnSourcePaused();
  } else {
    callback->OnSourceResumed();
  }
}
#endif // ARKWEB_MEDIA_CAPTURE

} // namespace blink