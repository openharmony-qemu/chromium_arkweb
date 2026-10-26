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

#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_event_router.h"

#include <memory>
#include <string>

#include "arkweb/build/features/features.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/browser/extensions/api/hwext_actor/hwext_actor_owner_registry.h"
#include "arkweb/chromium_ext/chrome/common/extensions/api/hwext_actor.h"
#include "extensions/browser/event_router.h"

namespace extensions {

namespace {

// Converts the internal ActorTask::State to the extension API enum string.
// The string values must match the IDL enum names in hwext_actor.idl.
std::string StateToString(actor::ActorTask::State state) {
  switch (state) {
    case actor::ActorTask::State::kCreated:
      return "CREATED";
    case actor::ActorTask::State::kActing:
      return "ACTING";
    case actor::ActorTask::State::kReflecting:
      return "REFLECTING";
    case actor::ActorTask::State::kPausedByActor:
      return "PAUSED_BY_ACTOR";
    case actor::ActorTask::State::kPausedByUser:
      return "PAUSED_BY_USER";
    case actor::ActorTask::State::kCancelled:
      return "CANCELLED";
    case actor::ActorTask::State::kFinished:
      return "FINISHED";
    case actor::ActorTask::State::kWaitingOnUser:
      return "WAITING_ON_USER";
    case actor::ActorTask::State::kFailed:
      return "FAILED";
  }
}

// Returns true if the state is terminal (task is completed).
bool IsTerminalState(actor::ActorTask::State state) {
  return state == actor::ActorTask::State::kCancelled ||
         state == actor::ActorTask::State::kFinished ||
         state == actor::ActorTask::State::kFailed;
}

}  // namespace

HwextActorEventRouter::HwextActorEventRouter(content::BrowserContext* context)
    : browser_context_(context),
      actor_service_(actor::ActorKeyedService::Get(context)) {
  if (actor_service_) {
    task_state_changed_subscription_ =
        actor_service_->AddTaskStateChangedCallback(base::BindRepeating(
            &HwextActorEventRouter::OnTaskStateChanged,
            weak_factory_.GetWeakPtr()));
  }
}

HwextActorEventRouter::~HwextActorEventRouter() = default;

void HwextActorEventRouter::Shutdown() {
  task_state_changed_subscription_ = {};
  weak_factory_.InvalidateWeakPtrs();
}

void HwextActorEventRouter::OnTaskStateChanged(actor::TaskId task_id,
                                               actor::ActorTask::State state) {
  DispatchTaskStateChanged(task_id, state);

  if (IsTerminalState(state)) {
    DispatchTaskRemoved(task_id);
  }
}

void HwextActorEventRouter::DispatchTaskStateChanged(
    actor::TaskId task_id,
    actor::ActorTask::State state) {
  auto* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  // Owner-level isolation: dispatch only to the extension that owns the task.
  auto* registry = HwextActorOwnerRegistry::Get(browser_context_);
  if (!registry) {
    return;
  }
  std::string owner_extension_id = registry->GetOwner(task_id);
  if (owner_extension_id.empty()) {
    return;
  }

  base::Value::List event_args;
  event_args.Append(task_id.value());
  event_args.Append(StateToString(state));

  auto event = std::make_unique<Event>(
      events::HWEXT_ACTOR_ONTASKSTATECHANGED,
      api::hwext_actor::OnTaskStateChanged::kEventName,
      std::move(event_args), browser_context_);
  event_router->DispatchEventToExtension(owner_extension_id, std::move(event));
}

void HwextActorEventRouter::DispatchTaskRemoved(actor::TaskId task_id) {
  auto* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  // Owner-level isolation: dispatch only to the extension that owns the task.
  auto* registry = HwextActorOwnerRegistry::Get(browser_context_);
  if (!registry) {
    return;
  }
  std::string owner_extension_id = registry->GetOwner(task_id);
  if (owner_extension_id.empty()) {
    return;
  }

  base::Value::List event_args;
  event_args.Append(task_id.value());

  auto event = std::make_unique<Event>(
      events::HWEXT_ACTOR_ONTASKREMOVED,
      api::hwext_actor::OnTaskRemoved::kEventName,
      std::move(event_args), browser_context_);
  event_router->DispatchEventToExtension(owner_extension_id, std::move(event));

  // Clean up the ownership mapping after the task is removed.
  registry->Remove(task_id);
}

}  // namespace extensions
