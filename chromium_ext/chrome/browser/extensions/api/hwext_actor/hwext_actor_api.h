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

#ifndef CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_API_H_

#include "arkweb/build/features/features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/common/actor/task_id.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "extensions/browser/extension_function.h"

namespace base {
class TimeTicks;
}

namespace extensions {

// Base class for all hwext.actor functions. See idl for more details.
class HwextActorApiFunction : public ExtensionFunction {
 public:
  HwextActorApiFunction();

  HwextActorApiFunction(const HwextActorApiFunction&) = delete;
  HwextActorApiFunction& operator=(const HwextActorApiFunction&) = delete;

 protected:
  ~HwextActorApiFunction() override;
  bool PreRunValidation(std::string* error) override;
};

// Creates an actor task (chrome.hwext.actor.createTask).
class HwextActorCreateTaskFunction : public HwextActorApiFunction {
 public:
  HwextActorCreateTaskFunction();

  HwextActorCreateTaskFunction(const HwextActorCreateTaskFunction&) = delete;
  HwextActorCreateTaskFunction& operator=(
      const HwextActorCreateTaskFunction&) = delete;

 protected:
  ~HwextActorCreateTaskFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("hwext.actor.createTask", HWEXT_ACTOR_CREATETASK)
};

// Stops an actor task (chrome.hwext.actor.stopTask).
class HwextActorStopTaskFunction : public HwextActorApiFunction {
 public:
  HwextActorStopTaskFunction();

  HwextActorStopTaskFunction(const HwextActorStopTaskFunction&) = delete;
  HwextActorStopTaskFunction& operator=(
      const HwextActorStopTaskFunction&) = delete;

 protected:
  ~HwextActorStopTaskFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("hwext.actor.stopTask", HWEXT_ACTOR_STOPTASK)
};

// Pauses an actor task (chrome.hwext.actor.pauseTask).
class HwextActorPauseTaskFunction : public HwextActorApiFunction {
 public:
  HwextActorPauseTaskFunction();

  HwextActorPauseTaskFunction(const HwextActorPauseTaskFunction&) = delete;
  HwextActorPauseTaskFunction& operator=(
      const HwextActorPauseTaskFunction&) = delete;

 protected:
  ~HwextActorPauseTaskFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("hwext.actor.pauseTask", HWEXT_ACTOR_PAUSETASK)
};

// Resumes a paused actor task (chrome.hwext.actor.resumeTask).
class HwextActorResumeTaskFunction : public HwextActorApiFunction {
 public:
  HwextActorResumeTaskFunction();

  HwextActorResumeTaskFunction(const HwextActorResumeTaskFunction&) = delete;
  HwextActorResumeTaskFunction& operator=(
      const HwextActorResumeTaskFunction&) = delete;

 protected:
  ~HwextActorResumeTaskFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("hwext.actor.resumeTask", HWEXT_ACTOR_RESUMETASK)
};

// Gets the state of an actor task (chrome.hwext.actor.getTaskState).
class HwextActorGetTaskStateFunction : public HwextActorApiFunction {
 public:
  HwextActorGetTaskStateFunction();

  HwextActorGetTaskStateFunction(const HwextActorGetTaskStateFunction&) = delete;
  HwextActorGetTaskStateFunction& operator=(
      const HwextActorGetTaskStateFunction&) = delete;

 protected:
  ~HwextActorGetTaskStateFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("hwext.actor.getTaskState",
                             HWEXT_ACTOR_GETTASKSTATE)
};

// Gets all active actor tasks (chrome.hwext.actor.getTasks).
class HwextActorGetTasksFunction : public HwextActorApiFunction {
 public:
  HwextActorGetTasksFunction();

  HwextActorGetTasksFunction(const HwextActorGetTasksFunction&) = delete;
  HwextActorGetTasksFunction& operator=(
      const HwextActorGetTasksFunction&) = delete;

 protected:
  ~HwextActorGetTasksFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("hwext.actor.getTasks", HWEXT_ACTOR_GETTASKS)
};

// Cancels in-progress actions for a task (chrome.hwext.actor.cancelActions).
class HwextActorCancelActionsFunction : public HwextActorApiFunction {
 public:
  HwextActorCancelActionsFunction();

  HwextActorCancelActionsFunction(const HwextActorCancelActionsFunction&) =
      delete;
  HwextActorCancelActionsFunction& operator=(
      const HwextActorCancelActionsFunction&) = delete;

 protected:
  ~HwextActorCancelActionsFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("hwext.actor.cancelActions",
                             HWEXT_ACTOR_CANCELACTIONS)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_API_H_
