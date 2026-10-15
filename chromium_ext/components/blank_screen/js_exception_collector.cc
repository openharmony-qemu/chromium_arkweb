// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/components/blank_screen/js_exception_collector.h"

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#include <string>

#include "arkweb/chromium_ext/components/blank_screen/blank_screen_reporter.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"

namespace blank_screen {

namespace {

// Extract URL and line number from V8 Error.stack string.
// Stack format: "Error: msg\n    at func (http://example.com:80/file.js:10:20)"
bool ExtractUrlAndLineFromStack(const std::string& stack_str,
                                std::string& url, int& line) {
  size_t at_pos = stack_str.find(" at ");
  if (at_pos == std::string::npos)
    return false;

  size_t paren_start = stack_str.find('(', at_pos);
  size_t paren_end = stack_str.find(')', paren_start);
  if (paren_start == std::string::npos || paren_end == std::string::npos)
    return false;

  // Content between parens: "url:line:col"
  std::string location = stack_str.substr(paren_start + 1,
                                          paren_end - paren_start - 1);
  size_t last_colon = location.rfind(':');
  if (last_colon == std::string::npos)
    return false;

  size_t second_colon = location.rfind(':', last_colon - 1);
  if (second_colon == std::string::npos)
    return false;

  url = location.substr(0, second_colon);
  std::string line_str = location.substr(second_colon + 1,
                                         last_colon - second_colon - 1);
  line = static_cast<int>(strtol(line_str.c_str(), nullptr, 10));
  return true;
}

}  // namespace

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

  isolate->AddMessageListener(MessageListenerCallback);
  isolate->SetPromiseRejectCallback(PromiseRejectCallback);

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

// static
void JsExceptionCollector::PromiseRejectCallback(
    v8::PromiseRejectMessage message) {
  // Only handle unhandled promise rejections.
  if (message.GetEvent() != v8::kPromiseRejectWithNoHandler)
    return;

  auto& collector = GetInstance();
  v8::Isolate* isolate = collector.isolate_;
  if (!isolate)
    return;

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  // Extract error message from reason.
  std::string message_str;
  v8::Local<v8::Value> reason = message.GetValue();
  if (!reason.IsEmpty()) {
    v8::Local<v8::String> reason_str;
    if (reason->ToString(isolate->GetCurrentContext()).ToLocal(&reason_str)) {
      v8::String::Utf8Value utf8(isolate, reason_str);
      if (*utf8)
        message_str = *utf8;
    }
  }

  if (message_str.length() > 200)
    message_str = message_str.substr(0, 200);

  // Extract URL and line from reason's stack property.
  std::string url_str;
  int line_number = 0;
  if (!reason.IsEmpty() && reason->IsObject()) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Object> err_obj = reason.As<v8::Object>();
    v8::Local<v8::Value> stack_val;
    if (err_obj->Get(context, v8::String::NewFromUtf8(isolate, "stack")
                         .ToLocalChecked())
            .ToLocal(&stack_val) &&
        stack_val->IsString()) {
      v8::String::Utf8Value utf8(isolate, stack_val);
      if (*utf8) {
        ExtractUrlAndLineFromStack(*utf8, url_str, line_number);
      }
    }
  }

  LOG(DEBUG) << "[BlankScreenMonitor] JS async exception (unhandled rejection): "
            << message_str << " line=" << line_number;

  collector.SendException(kSourceAsync, message_str, url_str, line_number);
}

}  // namespace blank_screen

#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)
