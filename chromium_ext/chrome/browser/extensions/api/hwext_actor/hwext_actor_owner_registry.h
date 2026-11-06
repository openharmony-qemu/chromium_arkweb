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

#ifndef CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_OWNER_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_OWNER_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

#include "arkweb/build/features/features.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "chrome/common/actor/task_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// HwextActorOwnerRegistry maintains the TaskId -> ExtensionId mapping that
// implements owner-level isolation for the hwext.actor extension API.
//
// Every API call that takes a taskId (stopTask, pauseTask, resumeTask,
// getTaskState, performActions, getTaskTrace) must first call
// CheckOwnership() to verify the caller is the task owner.  getTasks() must
// filter its result through GetTasksForOwner().  Events (onTaskRemoved,
// onTaskStateChanged) must be dispatched only to the owner extension returned
// by GetOwner().
class HwextActorOwnerRegistry : public KeyedService {
 public:
  explicit HwextActorOwnerRegistry(content::BrowserContext* context);
  HwextActorOwnerRegistry(const HwextActorOwnerRegistry&) = delete;
  HwextActorOwnerRegistry& operator=(const HwextActorOwnerRegistry&) = delete;
  ~HwextActorOwnerRegistry() override;

  // Returns the registry for |context|, or nullptr if not available.
  static HwextActorOwnerRegistry* Get(content::BrowserContext* context);

  // Registers a TaskId -> ExtensionId mapping. Called right after createTask
  // succeeds.
  void Register(const actor::TaskId& task_id, const std::string& extension_id);

  // Removes the mapping for |task_id|. Called when the task is removed
  // (terminal state reached or explicit stopTask).
  void Remove(const actor::TaskId& task_id);

  // Returns the owner extension id for |task_id|, or empty string if not
  // registered.
  std::string GetOwner(const actor::TaskId& task_id) const;

  // Returns true if |extension_id| is the owner of |task_id|.
  bool CheckOwnership(const actor::TaskId& task_id,
                      const std::string& extension_id) const;

  // Returns all task ids owned by |extension_id|.
  std::vector<actor::TaskId> GetTasksForOwner(
      const std::string& extension_id) const;

  // Returns the number of active tasks owned by |extension_id|.
  size_t GetTaskCountForOwner(const std::string& extension_id) const;

  // Maximum number of concurrent tasks per extension.
  static constexpr size_t kMaxTasksPerExtension = 10;

 private:
  std::map<actor::TaskId, std::string> owner_map_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HWEXT_ACTOR_HWEXT_ACTOR_OWNER_REGISTRY_H_
