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

#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"

namespace blink {

#if BUILDFLAG(ARKWEB_MEDIA_CAPTURE)
void BaseAudioContext::RegisterMediaStreamAudioDestinationNode(
    MediaStreamAudioDestinationNode* node) {
  LOG(INFO) << "BaseAudioContext::RegisterMediaStreamAudioDestinationNode";
  DCHECK(IsMainThread());
  if (node) {
    media_stream_destination_nodes_.insert(node);
  }
}

void BaseAudioContext::UnregisterMediaStreamAudioDestinationNode(
    MediaStreamAudioDestinationNode* node) {
  DCHECK(IsMainThread());
  LOG(INFO) << "BaseAudioContext::UnregisterMediaStreamAudioDestinationNode";
  if (node) {
    media_stream_destination_nodes_.erase(node);
  }
}

void BaseAudioContext::OnMediaStreamSourcePausedChanged(bool is_paused) {
  DCHECK(IsMainThread());
  LOG(INFO) << "BaseAudioContext::OnMediaStreamSourcePausedChanged";
  for (auto& dest_node : media_stream_destination_nodes_) {
    if (dest_node) {
      dest_node->OnSourcePausedChanged(is_paused);
    }
  }
}
#endif // ARKWEB_MEDIA_CAPTURE

} // namespace blink