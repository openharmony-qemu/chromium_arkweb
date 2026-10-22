/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
#include "arkweb/chromium_ext/base/ohos/sys_info_utils_ext.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "content/public/common/content_switches.h"

namespace {
// Callback that returns true when at least one incognito NWeb is active.
// Registered by the NWeb layer to break the layering dependency between
// chrome/browser/profiles and ohos_nweb.
// Threading: SetHasIncognitoNWebCallback() (the writer) and HasOTRProfile()
// (the reader) must both be called on the UI thread. The single registration
// in NWebImpl::InitializeWebEngine runs once during engine startup on the UI
// thread, before any extension API can call HasOTRProfile(); subsequent calls
// are read-only. The program order on the UI thread provides the required
// happens-before relationship, so no extra synchronization primitive is
// needed. Do NOT call either function from a non-UI thread without adding
// explicit synchronization.
base::NoDestructor<Profile::HasIncognitoNWebCallback>&
GetHasIncognitoNWebCallback() {
  static base::NoDestructor<Profile::HasIncognitoNWebCallback> callback;
  return callback;
}
}  // namespace

// static
void Profile::SetHasIncognitoNWebCallback(HasIncognitoNWebCallback callback) {
  *GetHasIncognitoNWebCallback() = std::move(callback);
}

bool Profile::HasOTRProfile() {
  if ((*base::CommandLine::ForCurrentProcess())
          .HasSwitch(switches::kEnableNwebEx)) {
    if (!off_the_record_) {
      return false;
    }
    // OTR profile always exists after engine startup, but HasOTRProfile
    // should reflect whether an incognito NWeb window is actually active.
    const auto& callback = *GetHasIncognitoNWebCallback();
    return callback ? callback.Run() : false;
  }

  return false;
}

Profile* Profile::GetOTRProfile() {
  if ((*base::CommandLine::ForCurrentProcess())
          .HasSwitch(switches::kEnableNwebEx)) {
    return off_the_record_;
  }

  return nullptr;
}
#endif
