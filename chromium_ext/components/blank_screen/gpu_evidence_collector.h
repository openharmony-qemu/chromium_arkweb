// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_GPU_EVIDENCE_COLLECTOR_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_GPU_EVIDENCE_COLLECTOR_H_

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#include <vector>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_child_process_observer.h"

namespace blank_screen {

// Singleton that observes GPU process lifecycle events via
// BrowserChildProcessObserver. GPU process is global/shared — one crash
// affects all WebViews. Fans out events to all registered detectors.
class GpuEvidenceCollector : public content::BrowserChildProcessObserver {
 public:
  using GpuEventCallback = base::RepeatingCallback<void(
      bool is_watchdog, int exit_code, const std::string& reason)>;
  using CallbackId = int;

  static GpuEvidenceCollector& GetInstance();

  GpuEvidenceCollector(const GpuEvidenceCollector&) = delete;
  GpuEvidenceCollector& operator=(const GpuEvidenceCollector&) = delete;

  CallbackId RegisterCallback(GpuEventCallback callback);
  void UnregisterCallback(CallbackId id);

 private:
  friend class base::NoDestructor<GpuEvidenceCollector>;
  GpuEvidenceCollector();
  ~GpuEvidenceCollector() override;

  // BrowserChildProcessObserver overrides
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  void HandleGpuProcessTermination(int exit_code, int status);

  struct CallbackEntry {
    CallbackId id;
    GpuEventCallback callback;
  };
  std::vector<CallbackEntry> callbacks_;
  CallbackId next_id_ = 0;
  base::Lock lock_;
};

}  // namespace blank_screen

#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)
#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_GPU_EVIDENCE_COLLECTOR_H_
