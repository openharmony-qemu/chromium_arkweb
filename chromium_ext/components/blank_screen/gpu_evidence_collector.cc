// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/components/blank_screen/gpu_evidence_collector.h"

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#include "base/logging.h"
#include "base/process/kill.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"

namespace blank_screen {

namespace {

const char* TerminationStatusToString(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return "NORMAL_TERMINATION";
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      return "ABNORMAL_TERMINATION";
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return "PROCESS_WAS_KILLED";
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      return "PROCESS_CRASHED";
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return "STILL_RUNNING";
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      return "LAUNCH_FAILED";
    default:
      return "UNKNOWN";
  }
}

}  // namespace

GpuEvidenceCollector& GpuEvidenceCollector::GetInstance() {
  static base::NoDestructor<GpuEvidenceCollector> instance;
  return *instance;
}

GpuEvidenceCollector::GpuEvidenceCollector() {
  content::BrowserChildProcessObserver::Add(this);
  LOG(DEBUG) << "[BlankScreenMonitor] GpuEvidenceCollector: registered observer";
}

GpuEvidenceCollector::~GpuEvidenceCollector() {
  content::BrowserChildProcessObserver::Remove(this);
}

GpuEvidenceCollector::CallbackId GpuEvidenceCollector::RegisterCallback(
    GpuEventCallback callback) {
  base::AutoLock lock(lock_);
  CallbackId id = ++next_id_;
  callbacks_.push_back({id, std::move(callback)});
  return id;
}

void GpuEvidenceCollector::UnregisterCallback(CallbackId id) {
  base::AutoLock lock(lock_);
  for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
    if (it->id == id) {
      callbacks_.erase(it);
      return;
    }
  }
}

void GpuEvidenceCollector::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type != content::PROCESS_TYPE_GPU)
    return;
  LOG(ERROR) << "[BlankScreenMonitor] GPU process crashed, exit_code="
             << info.exit_code
             << " status=" << TerminationStatusToString(info.status);
  HandleGpuProcessTermination(info.exit_code,
                              static_cast<int>(info.status));
}

void GpuEvidenceCollector::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type != content::PROCESS_TYPE_GPU)
    return;
  LOG(ERROR) << "[BlankScreenMonitor] GPU process killed, exit_code="
             << info.exit_code
             << " status=" << TerminationStatusToString(info.status);
  HandleGpuProcessTermination(info.exit_code,
                              static_cast<int>(info.status));
}

void GpuEvidenceCollector::HandleGpuProcessTermination(int exit_code,
                                                        int status) {
  // RESULT_CODE_HUNG indicates GPU watchdog timeout; all other codes are crashes.
  bool is_watchdog = (exit_code == content::RESULT_CODE_HUNG);

  std::string reason;
  if (is_watchdog) {
    reason = base::StringPrintf("WATCHDOG_TIMEOUT(exit_code=%d)", exit_code);
  } else {
    reason = base::StringPrintf("%s(exit_code=%d)",
                                TerminationStatusToString(
                                    static_cast<base::TerminationStatus>(status)),
                                exit_code);
  }

  LOG(ERROR) << "[BlankScreenMonitor] GPU process terminated: "
             << (is_watchdog ? "watchdog timeout" : "crash")
             << " exit_code=" << exit_code
             << " reason=" << reason;

  std::vector<GpuEventCallback> callbacks_copy;
  {
    base::AutoLock lock(lock_);
    for (const auto& entry : callbacks_) {
      callbacks_copy.push_back(entry.callback);
    }
  }

  for (const auto& cb : callbacks_copy) {
    cb.Run(is_watchdog, exit_code, reason);
  }
}

}  // namespace blank_screen

#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)
