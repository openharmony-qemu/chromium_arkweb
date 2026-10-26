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

#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_api.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "arkweb/build/features/features.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/aggregated_journal_in_memory_serializer.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/browser/extensions/api/hwext_actor/hwext_actor_owner_registry.h"
#include "chrome/common/actor_webui.mojom.h"
#include "arkweb/chromium_ext/chrome/common/extensions/api/hwext_actor.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "chrome/common/extensions/api/tabs.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "components/tabs/public/tab_interface.h"

#if BUILDFLAG(IS_ARKWEB)
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#endif

namespace extensions {

namespace {
// Maps the extension API state enum to a string for the generated
// TaskInfo dictionary.
api::hwext_actor::ActorTaskState ActorTaskStateToExtension(
    actor::ActorTask::State state) {
  switch (state) {
    case actor::ActorTask::State::kCreated:
      return api::hwext_actor::ActorTaskState::kCreated;
    case actor::ActorTask::State::kActing:
      return api::hwext_actor::ActorTaskState::kActing;
    case actor::ActorTask::State::kReflecting:
      return api::hwext_actor::ActorTaskState::kReflecting;
    case actor::ActorTask::State::kPausedByActor:
      return api::hwext_actor::ActorTaskState::kPausedByActor;
    case actor::ActorTask::State::kPausedByUser:
      return api::hwext_actor::ActorTaskState::kPausedByUser;
    case actor::ActorTask::State::kCancelled:
      return api::hwext_actor::ActorTaskState::kCancelled;
    case actor::ActorTask::State::kFinished:
      return api::hwext_actor::ActorTaskState::kFinished;
    case actor::ActorTask::State::kWaitingOnUser:
      return api::hwext_actor::ActorTaskState::kWaitingOnUser;
    case actor::ActorTask::State::kFailed:
      return api::hwext_actor::ActorTaskState::kFailed;
  }
}

// Builds a TaskInfo dictionary from an ActorTask.
api::hwext_actor::TaskInfo BuildTaskInfo(const actor::ActorTask& task) {
  api::hwext_actor::TaskInfo info;
  info.task_id = task.id().value();
  info.state = ActorTaskStateToExtension(task.GetState());
  if (!task.title().empty()) {
    info.title = task.title();
  }
  return info;
}

// Maps the extension API stop reason to the internal actor stop reason. The
// numeric values of the two enums intentionally differ, so an explicit switch
// (instead of a static cast) is required.
actor::ActorTask::StoppedReason ConvertStopReason(
    api::hwext_actor::ActorTaskStopReason stop_reason) {
  switch (stop_reason) {
    case api::hwext_actor::ActorTaskStopReason::kNone:
      return actor::ActorTask::StoppedReason::kTaskComplete;
    case api::hwext_actor::ActorTaskStopReason::kTaskComplete:
      return actor::ActorTask::StoppedReason::kTaskComplete;
    case api::hwext_actor::ActorTaskStopReason::kStoppedByUser:
      return actor::ActorTask::StoppedReason::kStoppedByUser;
    case api::hwext_actor::ActorTaskStopReason::kModelError:
      return actor::ActorTask::StoppedReason::kModelError;
    case api::hwext_actor::ActorTaskStopReason::kUserStartedNewChat:
      return actor::ActorTask::StoppedReason::kUserStartedNewChat;
    case api::hwext_actor::ActorTaskStopReason::kUserLoadedPreviousChat:
      return actor::ActorTask::StoppedReason::kUserLoadedPreviousChat;
  }
}

}  // namespace

HwextActorApiFunction::HwextActorApiFunction() = default;
HwextActorApiFunction::~HwextActorApiFunction() = default;

bool HwextActorApiFunction::PreRunValidation(std::string* error) {
  if (!actor::ActorKeyedService::Get(browser_context())) {
    *error = "Incognito profile not supported.";
    return false;
  }
  return true;
}

HwextActorCreateTaskFunction::HwextActorCreateTaskFunction() = default;
HwextActorCreateTaskFunction::~HwextActorCreateTaskFunction() = default;

ExtensionFunction::ResponseAction HwextActorCreateTaskFunction::Run() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx)) {
    return RespondNow(Error("Current platform is not browser."));
  }

  TRACE_EVENT0("actor", "actor::createTask");
  auto params = api::hwext_actor::CreateTask::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  actor::webui::mojom::TaskOptionsPtr options =
      actor::webui::mojom::TaskOptions::New();
  if (params->task_options && params->task_options->title) {
    options->title = *params->task_options->title;
  }

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  if (!actor_service) {
    return RespondNow(Error("Incognito profile not supported."));
  }

  // Enforce per-extension task limit.
  auto* registry = HwextActorOwnerRegistry::Get(browser_context());
  if (!registry) {
    return RespondNow(Error("Task registry unavailable."));
  }
  size_t count = registry->GetTaskCountForOwner(extension_id());
  if (count >= HwextActorOwnerRegistry::kMaxTasksPerExtension) {
    LOG(WARNING) << "createTask: extension " << extension_id()
                 << " already has " << count << " tasks (max "
                 << HwextActorOwnerRegistry::kMaxTasksPerExtension << ")";
    return RespondNow(Error(
        "Maximum number of concurrent tasks (" +
        base::NumberToString(
            HwextActorOwnerRegistry::kMaxTasksPerExtension) +
        ") reached for this extension."));
  }

  actor::TaskId task_id = actor_service->CreateTaskWithOptions(
      std::move(options), /*delegate=*/nullptr);

  if (task_id.is_null()) {
    return RespondNow(Error("Actor task system unavailable."));
  }

  // Register TaskId -> ExtensionId mapping for owner-level isolation.
  registry->Register(task_id, extension_id());

  return RespondNow(ArgumentList(
      api::hwext_actor::CreateTask::Results::Create(task_id.value())));
}

HwextActorStopTaskFunction::HwextActorStopTaskFunction() = default;
HwextActorStopTaskFunction::~HwextActorStopTaskFunction() = default;

ExtensionFunction::ResponseAction HwextActorStopTaskFunction::Run() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx)) {
    return RespondNow(Error("Current platform is not browser."));
  }

  TRACE_EVENT0("actor", "actor::stopTask");
  auto params = api::hwext_actor::StopTask::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  if (!actor_service) {
    return RespondNow(Error("Incognito profile not supported."));
  }

  // Owner-level isolation: only the extension that created the task may stop it.
  actor::TaskId task_id(params->task_id);
  auto* registry = HwextActorOwnerRegistry::Get(browser_context());
  if (!registry || !registry->CheckOwnership(task_id, extension_id())) {
    return RespondNow(Error("Permission denied: not the task owner."));
  }

  actor::ActorTask::StoppedReason reason = ConvertStopReason(params->stop_reason);
  LOG(INFO) << "[ActorTool]: stopTask: calling StopTask, taskId="
            << params->task_id
            << ", internal_reason=" << static_cast<int>(reason);
  actor_service->StopTask(task_id, reason);

  // Ownership mapping cleanup is deferred to HwextActorEventRouter::DispatchTaskRemoved(),
  // which fires when the terminal state notification arrives. Removing here would race
  // with the async state-change callback, causing OnTaskStateChanged/OnTaskRemoved events
  // to be silently dropped (GetOwner returns empty after Remove).
  return RespondNow(
      ArgumentList(api::hwext_actor::StopTask::Results::Create()));
}

// FR-3: pauseTask
HwextActorPauseTaskFunction::HwextActorPauseTaskFunction() = default;
HwextActorPauseTaskFunction::~HwextActorPauseTaskFunction() = default;

ExtensionFunction::ResponseAction HwextActorPauseTaskFunction::Run() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx)) {
    return RespondNow(Error("Current platform is not browser."));
  }

  TRACE_EVENT0("actor", "actor::pauseTask");
  auto params = api::hwext_actor::PauseTask::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  if (!actor_service) {
    return RespondNow(Error("Incognito profile not supported."));
  }

  actor::TaskId task_id(params->task_id);

  // Owner-level isolation: only the extension that created the task may pause it.
  auto* registry = HwextActorOwnerRegistry::Get(browser_context());
  if (!registry || !registry->CheckOwnership(task_id, extension_id())) {
    return RespondNow(Error("Permission denied: not the task owner."));
  }

  actor::ActorTask* task = actor_service->GetTask(task_id);
  if (!task) {
    return RespondNow(Error(
        absl::StrFormat("Task %d not found.", params->task_id)));
  }

  bool from_actor = params->pause_reason ==
                        api::hwext_actor::ActorTaskPauseReason::kByActor ||
                    params->pause_reason ==
                        api::hwext_actor::ActorTaskPauseReason::kNone;
  task->Pause(from_actor);

  return RespondNow(
      ArgumentList(api::hwext_actor::PauseTask::Results::Create()));
}

// FR-4: resumeTask
HwextActorResumeTaskFunction::HwextActorResumeTaskFunction() = default;
HwextActorResumeTaskFunction::~HwextActorResumeTaskFunction() = default;

ExtensionFunction::ResponseAction HwextActorResumeTaskFunction::Run() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx)) {
    return RespondNow(Error("Current platform is not browser."));
  }

  TRACE_EVENT0("actor", "actor::resumeTask");
  auto params = api::hwext_actor::ResumeTask::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  if (!actor_service) {
    return RespondNow(Error("Incognito profile not supported."));
  }

  actor::TaskId task_id(params->task_id);

  // Owner-level isolation: only the extension that created the task may resume it.
  auto* registry = HwextActorOwnerRegistry::Get(browser_context());
  if (!registry || !registry->CheckOwnership(task_id, extension_id())) {
    return RespondNow(Error("Permission denied: not the task owner."));
  }

  actor::ActorTask* task = actor_service->GetTask(task_id);
  if (!task) {
    return RespondNow(Error(
        absl::StrFormat("Task %d not found.", params->task_id)));
  }

  task->Resume();

  return RespondNow(
      ArgumentList(api::hwext_actor::ResumeTask::Results::Create()));
}

// FR-5: getTaskState
HwextActorGetTaskStateFunction::HwextActorGetTaskStateFunction() = default;
HwextActorGetTaskStateFunction::~HwextActorGetTaskStateFunction() = default;

ExtensionFunction::ResponseAction HwextActorGetTaskStateFunction::Run() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx)) {
    return RespondNow(Error("Current platform is not browser."));
  }

  TRACE_EVENT0("actor", "actor::getTaskState");
  auto params = api::hwext_actor::GetTaskState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  if (!actor_service) {
    return RespondNow(Error("Incognito profile not supported."));
  }

  actor::TaskId task_id(params->task_id);

  // Owner-level isolation: only the extension that created the task may query it.
  auto* registry = HwextActorOwnerRegistry::Get(browser_context());
  if (!registry || !registry->CheckOwnership(task_id, extension_id())) {
    return RespondNow(Error("Permission denied: not the task owner."));
  }

  actor::ActorTask* task = actor_service->GetTask(task_id);
  if (!task) {
    return RespondNow(Error(
        absl::StrFormat("Task %d not found.", params->task_id)));
  }

  api::hwext_actor::ActorTaskState state = ActorTaskStateToExtension(task->GetState());
  return RespondNow(ArgumentList(
      api::hwext_actor::GetTaskState::Results::Create(state)));
}

// FR-6: getTasks
HwextActorGetTasksFunction::HwextActorGetTasksFunction() = default;
HwextActorGetTasksFunction::~HwextActorGetTasksFunction() = default;

ExtensionFunction::ResponseAction HwextActorGetTasksFunction::Run() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx)) {
    return RespondNow(Error("Current platform is not browser."));
  }

  TRACE_EVENT0("actor", "actor::getTasks");
  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  if (!actor_service) {
    return RespondNow(Error("Incognito profile not supported."));
  }

  std::vector<api::hwext_actor::TaskInfo> tasks;

  // Owner-level isolation: only return tasks created by this extension.
  auto* registry = HwextActorOwnerRegistry::Get(browser_context());
  if (!registry) {
    return RespondNow(ArgumentList(
        api::hwext_actor::GetTasks::Results::Create(tasks)));
  }
  for (const auto& [task_id, task] : actor_service->GetActiveTasks()) {
    if (registry->CheckOwnership(task_id, extension_id())) {
      tasks.push_back(BuildTaskInfo(*task));
    }
  }

  return RespondNow(
      ArgumentList(api::hwext_actor::GetTasks::Results::Create(tasks)));
}

HwextActorCancelActionsFunction::HwextActorCancelActionsFunction() = default;
HwextActorCancelActionsFunction::~HwextActorCancelActionsFunction() = default;

ExtensionFunction::ResponseAction HwextActorCancelActionsFunction::Run() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebEx)) {
    return RespondNow(Error("Current platform is not browser."));
  }

  TRACE_EVENT0("actor", "actor::cancelActions");
  auto params = api::hwext_actor::CancelActions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  if (!actor_service) {
    return RespondNow(Error("Incognito profile not supported."));
  }

  actor::TaskId task_id(params->task_id);

  // Owner-level isolation: only the extension that created the task may
  // cancel actions on it.
  auto* registry = HwextActorOwnerRegistry::Get(browser_context());
  if (!registry || !registry->CheckOwnership(task_id, extension_id())) {
    return RespondNow(Error("Permission denied: not the task owner."));
  }

  actor::ActorTask* task = actor_service->GetTask(task_id);
  if (!task) {
    return RespondNow(Error(
        absl::StrFormat("Task %d not found.", params->task_id)));
  }

  task->CancelOngoingActions();

  return RespondNow(
      ArgumentList(api::hwext_actor::CancelActions::Results::Create()));
}

}  // namespace extensions
