// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_
#error "must be included from audio_context.cc"
#endif

#if BUILDFLAG(ARKWEB_MEDIA_POLICY)
#include "arkweb/chromium_ext/base/ohos/sys_info_utils_ext.h"
#include "base/logging.h"
#endif

namespace blink {

#if BUILDFLAG(ARKWEB_MEDIA_POLICY)
void AudioContext::RequestPause(bool triggered_by_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  if (!base::ohos::IsPcDevice()) {
    return;
  }
  const bool cleared = IsContextCleared();
  if (cleared || !destination() ||
      ContextState() == V8AudioContextState::Enum::kClosed) {
    LOG(INFO) << "[PC_AUDIO_CONTEXT_SUSPEND] stage=context_pause_skipped";
    return;
  }

  // Match an explicit suspend: stop the destination as well as changing the
  // state, and do not let a source node implicitly restart the context. A later
  // explicit resume() is still allowed and retains the normal autoplay checks.
  suspended_by_user_ = true;
  SuspendRendering();
  LOG(INFO) << "[PC_AUDIO_CONTEXT_SUSPEND] stage=context_pause_done";
}
#endif

}  // namespace blink
