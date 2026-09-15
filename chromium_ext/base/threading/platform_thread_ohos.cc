// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/base/threading/platform_thread_ohos.h"

#include <qos/qos.h>

#include "base/logging.h"
#include "base/notreached.h"

namespace base::internal {

namespace {
QoS_Level ThreadTypeToQosLevel(ThreadType thread_type) {
  switch (thread_type) {
  case ThreadType::kBackground:
    return QoS_Level::QOS_BACKGROUND;
  case ThreadType::kUtility:
    return QoS_Level::QOS_UTILITY;
  case ThreadType::kDefault:
    return QoS_Level::QOS_USER_INITIATED;
  case ThreadType::kDisplayCritical:
  case ThreadType::kInteractive:
  case ThreadType::kRealtimeAudio:
    return QoS_Level::QOS_USER_INTERACTIVE;
  }
  NOTREACHED();
  return QoS_Level::QOS_DEFAULT;
}
} // namespace

bool SetCurrentThreadTypeForOhos(ThreadType thread_type) {
  QoS_Level level = ThreadTypeToQosLevel(thread_type);
  int result = OH_QoS_SetThreadQoS(level);
  if (result != 0) {
    LOG(ERROR) << "Failed to set current thread QoS, tid="
               << PlatformThread::CurrentId() << ", result=" << result;
    return false;
  }
  LOG(INFO) << "Set current thread QoS, tid=" << PlatformThread::CurrentId()
            << ", level=" << static_cast<int>(level);
  return true;
}

} // namespace base::internal
