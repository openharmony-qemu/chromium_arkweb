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

namespace media {

#if BUILDFLAG(ARKWEB_MEDIA_CAPTURE)
void AudioManagerBase::AddInputStreamToNWebMap(int nweb_id, AudioInputStream* stream) {
  if (nweb_id != -1) {
      nweb_input_streams_map_[nweb_id].insert(stream);
  }
}

void AudioManagerBase::RemoveInputStreamFromNWebMap(int nweb_id, AudioInputStream* stream) {
  if (nweb_id != -1) {
    auto it = nweb_input_streams_map_.find(nweb_id);
    if (it != nweb_input_streams_map_.end()) {
      it->second.erase(stream);
      if (it->second.empty()) {
        nweb_input_streams_map_.erase(it);
      }
    }
  }
}

std::set<raw_ptr<AudioInputStream, CtnExperimental>>
    AudioManagerBase::GetInputStreamsByNWebId(int nweb_id) {
  auto it = nweb_input_streams_map_.find(nweb_id);
  if (it != nweb_input_streams_map_.end()) {
    return it->second;
  }
  return {};
}
#endif //  ARKWEB_MEDIA_CAPTURE

} // namespace media