/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_WORKER_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_WORKER_OBSERVER_H_

#include "arkweb/build/features/features.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/process_manager_observer.h"
#include "chrome/common/actor/task_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ProcessManager;

// HwextActorWorkerObserver monitors MV3 service worker lifecycle events.
// When an extension's service worker starts (or restarts after suspension),
// it dispatches the `onTasksRestored` event containing the current snapshot
// of all active tasks owned by that extension. This allows the extension to
// rebuild its in-memory task state after the service worker context has been
// destroyed and recreated.
//
// Lifetime: KeyedService, created eagerly with the BrowserContext.
// Thread: UI thread only.
class HwextActorWorkerObserver : public KeyedService,
                                 public ProcessManagerObserver {
 public:
  explicit HwextActorWorkerObserver(content::BrowserContext* context);
  HwextActorWorkerObserver(const HwextActorWorkerObserver&) = delete;
  HwextActorWorkerObserver& operator=(const HwextActorWorkerObserver&) = delete;
  ~HwextActorWorkerObserver() override;

  void Shutdown() override;

  void OnStartedTrackingServiceWorkerInstance(
      const WorkerId& worker_id) override;
  void OnProcessManagerShutdown(ProcessManager* manager) override;

 private:
  // Dispatches the onTasksRestored event to |extension_id| with the current
  // snapshot of active tasks owned by that extension.
  void DispatchTasksRestored(const std::string& extension_id);

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<ProcessManager> process_manager_;

  // Observation for ProcessManager lifecycle events.
  base::ScopedObservation<ProcessManager, ProcessManagerObserver>
      process_manager_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_WORKER_OBSERVER_H_
