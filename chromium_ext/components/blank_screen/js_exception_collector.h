// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_JS_EXCEPTION_COLLECTOR_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_JS_EXCEPTION_COLLECTOR_H_

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#include <map>
#include <string>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "v8/include/v8.h"

namespace blank_screen {

// Renderer-side JS exception collector singleton.
// Captures JS runtime errors via V8 AddMessageListener (sync) and
// SetPromiseRejectCallback (async), sends them to browser via Mojo.
class JsExceptionCollector {
 public:
  // Exception source: 0=sync (AddMessageListener), 1=async (PromiseRejectCallback)
  static constexpr int kSourceSync = 0;
  static constexpr int kSourceAsync = 1;

  static JsExceptionCollector& GetInstance();

  JsExceptionCollector(const JsExceptionCollector&) = delete;
  JsExceptionCollector& operator=(const JsExceptionCollector&) = delete;

  // Register V8 callbacks. Called once per isolate (V8 hooks are per-isolate).
  void InstallHooks(v8::Isolate* isolate,
                    const viz::FrameSinkId& frame_sink_id);

  void ResetForNewNavigation(const viz::FrameSinkId& frame_sink_id);
  void RemoveFrameSink(const viz::FrameSinkId& frame_sink_id);

 private:
  friend class base::NoDestructor<JsExceptionCollector>;
  JsExceptionCollector();
  ~JsExceptionCollector();

  static void MessageListenerCallback(v8::Local<v8::Message> message,
                                      v8::Local<v8::Value> data);
  static void PromiseRejectCallback(v8::PromiseRejectMessage message);

  void SendException(int source,
                     const std::string& message,
                     const std::string& url,
                     int line);

  bool CheckRateLimit(const viz::FrameSinkId& frame_sink_id);

  struct PerFrameState {
    base::TimeTicks window_start;
    int exception_count = 0;
  };

  v8::Isolate* isolate_ = nullptr;
  viz::FrameSinkId active_frame_sink_id_ GUARDED_BY(lock_);
  bool hooks_installed_ = false;

  mutable base::Lock lock_;
  std::map<viz::FrameSinkId, PerFrameState> states_ GUARDED_BY(lock_);

  static constexpr int kMaxExceptionsPerSecond = 5;
};

}  // namespace blank_screen

#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_JS_EXCEPTION_COLLECTOR_H_
