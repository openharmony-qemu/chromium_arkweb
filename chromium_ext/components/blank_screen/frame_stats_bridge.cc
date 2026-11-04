// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/components/blank_screen/frame_stats_bridge.h"

#include "base/logging.h"
#include "base/no_destructor.h"

namespace blank_screen {

FrameStatsBridge& FrameStatsBridge::GetInstance() {
  static base::NoDestructor<FrameStatsBridge> instance;
  return *instance;
}

FrameStatsBridge::FrameStatsBridge() = default;

FrameStatsBridge::~FrameStatsBridge() = default;

void FrameStatsBridge::Register(const viz::FrameSinkId& id,
                                 FrameCallback callback) {
  base::AutoLock lock(lock_);
  CallbackEntry entry;
  entry.callback = std::move(callback);
  entry.task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  callbacks_[id] = std::move(entry);
  LOG(DEBUG) << "[BlankScreenMonitor] FrameStatsBridge registered for "
            << id.ToString();
}

void FrameStatsBridge::Unregister(const viz::FrameSinkId& id) {
  base::AutoLock lock(lock_);
  callbacks_.erase(id);
  LOG(DEBUG) << "[BlankScreenMonitor] FrameStatsBridge unregistered for "
            << id.ToString();
}

void FrameStatsBridge::OnFrameAnalyzedFromGpuProcess(
    const viz::FrameSinkId& id,
    const FrameStats& stats) {
  base::AutoLock lock(lock_);
  auto it = callbacks_.find(id);
  if (it != callbacks_.end()) {
    const auto& entry = it->second;
    if (entry.task_runner) {
      entry.task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(entry.callback, stats));
    }
  } else {
    LOG(DEBUG) << "[BlankScreenMonitor] FrameStatsBridge: no callback for "
                 << id.ToString() << " (registered: " << callbacks_.size()
                 << " sinks)";
  }
}

void FrameStatsBridge::RegisterJsExceptionCallback(
    const viz::FrameSinkId& id,
    JsExceptionCallback callback) {
  base::AutoLock lock(lock_);
  JsExceptionCallbackEntry entry;
  entry.callback = std::move(callback);
  entry.task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  js_callbacks_[id] = std::move(entry);
}

void FrameStatsBridge::UnregisterJsExceptionCallback(
    const viz::FrameSinkId& id) {
  base::AutoLock lock(lock_);
  js_callbacks_.erase(id);
}

void FrameStatsBridge::OnJsExceptionFromRenderer(
    const viz::FrameSinkId& id,
    int source,
    const std::string& message,
    const std::string& url,
    int line) {
  base::AutoLock lock(lock_);
  auto it = js_callbacks_.find(id);
  if (it != js_callbacks_.end()) {
    const auto& entry = it->second;
    if (entry.task_runner) {
      entry.task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(entry.callback, source, message, url, line));
    }
  }
}

}  // namespace blank_screen
