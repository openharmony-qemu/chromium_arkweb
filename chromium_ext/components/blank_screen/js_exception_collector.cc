// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/components/blank_screen/js_exception_collector.h"

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#include <string>

#include "arkweb/chromium_ext/components/blank_screen/blank_screen_reporter.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace blank_screen {

// static
JsExceptionCollector& JsExceptionCollector::GetInstance() {
  static base::NoDestructor<JsExceptionCollector> instance;
  return *instance;
}

JsExceptionCollector::JsExceptionCollector() = default;
JsExceptionCollector::~JsExceptionCollector() = default;

void JsExceptionCollector::InstallHooks(
    v8::Isolate* isolate,
    const viz::FrameSinkId& frame_sink_id) {
  if (!isolate)
    return;

  isolate_ = isolate;
  {
    base::AutoLock lock(lock_);
    active_frame_sink_id_ = frame_sink_id;
    // V8 AddMessageListener does not deduplicate — registering N times creates
    // N listener entries.  Only install hooks once per isolate.
    if (hooks_installed_) {
      LOG(DEBUG) << "[BlankScreenMonitor] JsExceptionCollector: hooks already installed, "
                << "updating frame_sink_id to " << frame_sink_id.ToString();
      return;
    }
    hooks_installed_ = true;
  }

  // Only use AddMessageListener — it is additive (V8 supports multiple
  // listeners) and does not replace Blink's MessageHandlerInMainThread.
  // DO NOT use SetPromiseRejectCallback: it is a single-slot replacement
  // API that would overwrite Blink's PromiseRejectHandlerInMainThread,
  // breaking unhandled promise rejection reporting (e.g. "Uncaught (in
  // promise) TypeError: Failed to fetch" console messages).
  // Async promise rejections are still captured via Path C
  // (OnDidAddMessageToConsole) after Blink processes them.
  isolate->AddMessageListener(MessageListenerCallback);

  LOG(DEBUG) << "[BlankScreenMonitor] JsExceptionCollector: hooks installed for "
            << frame_sink_id.ToString();
}

void JsExceptionCollector::ResetForNewNavigation(
    const viz::FrameSinkId& frame_sink_id) {
  base::AutoLock lock(lock_);
  auto it = states_.find(frame_sink_id);
  if (it != states_.end()) {
    it->second.exception_count = 0;
    it->second.window_start = base::TimeTicks();
  }
  active_frame_sink_id_ = frame_sink_id;
}

void JsExceptionCollector::RemoveFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  base::AutoLock lock(lock_);
  states_.erase(frame_sink_id);
}

bool JsExceptionCollector::CheckRateLimit(
    const viz::FrameSinkId& frame_sink_id) {
  base::AutoLock lock(lock_);
  auto& state = states_[frame_sink_id];
  auto now = base::TimeTicks::Now();
  if (state.window_start.is_null() ||
      now - state.window_start >= base::Seconds(1)) {
    state.window_start = now;
    state.exception_count = 0;
  }
  if (state.exception_count >= kMaxExceptionsPerSecond)
    return false;
  state.exception_count++;
  return true;
}

void JsExceptionCollector::SendException(int source,
                                         const std::string& message,
                                         const std::string& url,
                                         int line) {
  viz::FrameSinkId frame_sink_id;
  {
    base::AutoLock lock(lock_);
    frame_sink_id = active_frame_sink_id_;
  }

  if (!frame_sink_id.is_valid())
    return;

  if (!CheckRateLimit(frame_sink_id))
    return;

  auto host = BlankScreenReporter::GetInstance().GetHost(frame_sink_id);
  if (host) {
    host->OnJsException(frame_sink_id, source, message, url, line);
  }
}

// static
void JsExceptionCollector::MessageListenerCallback(
    v8::Local<v8::Message> message,
    v8::Local<v8::Value> data) {
  auto& collector = GetInstance();
  v8::Isolate* isolate = collector.isolate_;
  if (!isolate)
    return;

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  // Extract error message.
  std::string message_str;
  v8::Local<v8::String> msg_str;
  if (message->Get()->ToString(isolate->GetCurrentContext()).ToLocal(&msg_str)) {
    v8::String::Utf8Value utf8(isolate, msg_str);
    if (*utf8)
      message_str = *utf8;
  }

  if (message_str.length() > 200)
    message_str = message_str.substr(0, 200);

  // Extract source URL.
  std::string url_str;
  auto script_origin = message->GetScriptOrigin();
  if (!script_origin.ResourceName().IsEmpty()) {
    v8::String::Utf8Value utf8(isolate,
                              script_origin.ResourceName().As<v8::String>());
    if (*utf8)
      url_str = *utf8;
  }

  int line_number = message->GetLineNumber(isolate->GetCurrentContext())
                        .FromMaybe(0);

  LOG(DEBUG) << "[BlankScreenMonitor] JS sync exception: " << message_str
            << " line=" << line_number;

  collector.SendException(kSourceSync, message_str, url_str, line_number);
}

}  // namespace blank_screen

#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)
