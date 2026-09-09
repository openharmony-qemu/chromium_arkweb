// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARKWEB_CHROMIUM_EXT_BASE_THREADING_PLATFORM_THREAD_OHOS_H_
#define ARKWEB_CHROMIUM_EXT_BASE_THREADING_PLATFORM_THREAD_OHOS_H_

#include "base/threading/platform_thread.h"

namespace base::internal {

// Applies the OpenHarmony QoS class for the current thread. The caller still
// runs Chromium's normal Linux thread-type handling afterwards.
bool SetCurrentThreadTypeForOhos(ThreadType thread_type);

} // namespace base::internal

#endif // ARKWEB_CHROMIUM_EXT_BASE_THREADING_PLATFORM_THREAD_OHOS_H_
