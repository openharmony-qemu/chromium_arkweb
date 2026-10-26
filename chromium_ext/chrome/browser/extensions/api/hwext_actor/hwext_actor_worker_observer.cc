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
#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_worker_observer.h"

#include <memory>
#include <string>

#include "arkweb/build/features/features.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_owner_registry.h"
#include "arkweb/chromium_ext/chrome/common/extensions/api/hwext_actor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/process_manager.h"

namespace extensions {

namespace {

// Converts the internal ActorTask::State to the extension API enum string.
// Mirrors the same conversion in hwext_actor_event_router.cc.
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

}  // namespace

HwextActorWorkerObserver::HwextActorWorkerObserver(
    content::BrowserContext* context)
    : browser_context_(context),
      process_manager_(ProcessManager::Get(context)) {
  if (process_manager_) {
    process_manager_observation_.Observe(process_manager_);
  }
}

HwextActorWorkerObserver::~HwextActorWorkerObserver() = default;

void HwextActorWorkerObserver::Shutdown() {
  process_manager_observation_.Reset();
  process_manager_ = nullptr;
  browser_context_ = nullptr;
}

void HwextActorWorkerObserver::OnStartedTrackingServiceWorkerInstance(
    const WorkerId& worker_id) {
  // When an extension's service worker starts (or restarts after suspension),
  // dispatch the current task snapshot so the extension can rebuild its
  // in-memory state.
  DispatchTasksRestored(worker_id.extension_id);
}

void HwextActorWorkerObserver::OnProcessManagerShutdown(
    ProcessManager* manager) {
  process_manager_observation_.Reset();
  process_manager_ = nullptr;
}

void HwextActorWorkerObserver::DispatchTasksRestored(
    const std::string& extension_id) {
  auto* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  auto* registry = HwextActorOwnerRegistry::Get(browser_context_);
  if (!registry) {
    return;
  }

  auto* actor_service = actor::ActorKeyedService::Get(browser_context_);
  if (!actor_service) {
    return;
  }

  // Build the task snapshot: iterate all active tasks and filter by ownership.
  base::Value::List event_args;
  base::Value::List task_list;
  for (const auto& [task_id, task_ptr] : actor_service->GetActiveTasks()) {
    if (!registry->CheckOwnership(task_id, extension_id)) {
      continue;
    }
    base::Value::Dict task_dict;
    task_dict.Set("taskId", task_id.value());
    task_dict.Set("state", StateToString(task_ptr->GetState()));
    if (!task_ptr->title().empty()) {
      task_dict.Set("title", task_ptr->title());
    }
    task_list.Append(std::move(task_dict));
  }

  // If the extension owns no active tasks, no event is dispatched.
  if (task_list.empty()) {
    return;
  }

  event_args.Append(std::move(task_list));

  auto event = std::make_unique<Event>(
      events::HWEXT_ACTOR_ONTASKSRESTORED,
      api::hwext_actor::OnTasksRestored::kEventName,
      std::move(event_args), browser_context_);
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

}  // namespace extensions
