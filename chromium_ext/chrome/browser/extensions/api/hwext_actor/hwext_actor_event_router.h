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

#ifndef CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_EVENT_ROUTER_H_

#include "arkweb/build/features/features.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/browser/actor/actor_task.h"
namespace content {
class BrowserContext;
}

namespace actor {
class ActorKeyedService;
}  // namespace actor

namespace extensions {

// HwextActorEventRouter listens to actor task state changes from
// ActorKeyedService and dispatches them as extension events
// (chrome.hwext.actor.onTaskStateChanged) to extensions.
//
// onTaskRemoved is dispatched when a task transitions to a terminal state
// (kCancelled, kFinished, or kFailed).
class HwextActorEventRouter : public KeyedService {
 public:
  explicit HwextActorEventRouter(content::BrowserContext* context);
  HwextActorEventRouter(const HwextActorEventRouter&) = delete;
  HwextActorEventRouter& operator=(const HwextActorEventRouter&) = delete;
  ~HwextActorEventRouter() override;

  void Shutdown() override;

 private:
  // Called when an actor task's state changes.
  void OnTaskStateChanged(actor::TaskId task_id,
                          actor::ActorTask::State state);

  // Dispatches the onTaskStateChanged event to all listening extensions.
  void DispatchTaskStateChanged(actor::TaskId task_id,
                                actor::ActorTask::State state);

  // Dispatches the onTaskRemoved event to all listening extensions.
  void DispatchTaskRemoved(actor::TaskId task_id);

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<actor::ActorKeyedService> actor_service_;

  // Subscription to ActorKeyedService::AddTaskStateChangedCallback.
  // Kept alive to remain registered; destroyed on Shutdown.
  base::CallbackListSubscription task_state_changed_subscription_;

  base::WeakPtrFactory<HwextActorEventRouter> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_EVENT_ROUTER_H_
