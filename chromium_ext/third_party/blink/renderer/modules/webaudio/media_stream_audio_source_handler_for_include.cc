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
void MediaStreamAudioSourceHandler::SetOnSourcePausedChangedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  DCHECK(IsMainThread());
  LOG(INFO) << "MediaStreamAudioSourceHandler::SetOnSourcePausedChangedCallback";
  if (audio_source_provider_) {
    audio_source_provider_->SetOnSourcePausedChangedCallback(std::move(callback));
  }
}
#endif // ARKWEB_MEDIA_CAPTURE

} // namespace blink