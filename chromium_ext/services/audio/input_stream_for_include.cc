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

namespace audio {

#if BUILDFLAG(ARKWEB_MEDIA_CAPTURE)
void InputStream::OnPaused(bool is_paused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  LOG(INFO) << "InputStream::OnPaused";
  if (client_) {
    client_->OnPausedStateChanged(is_paused);
  }
}
#endif // ARKWEB_MEDIA_CAPTURE

} // namespace audio