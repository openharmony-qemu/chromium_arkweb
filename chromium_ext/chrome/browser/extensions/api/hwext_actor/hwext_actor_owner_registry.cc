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

#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_owner_registry.h"

#include <utility>

#include "arkweb/chromium_ext/chrome/browser/extensions/api/hwext_actor/hwext_actor_owner_registry_factory.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

HwextActorOwnerRegistry::HwextActorOwnerRegistry(content::BrowserContext* context)
    : owner_map_() {}

HwextActorOwnerRegistry::~HwextActorOwnerRegistry() = default;

// static
HwextActorOwnerRegistry* HwextActorOwnerRegistry::Get(
    content::BrowserContext* context) {
  return HwextActorOwnerRegistryFactory::GetForBrowserContext(context);
}

void HwextActorOwnerRegistry::Register(const actor::TaskId& task_id,
                                       const std::string& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  owner_map_[task_id] = extension_id;
}

void HwextActorOwnerRegistry::Remove(const actor::TaskId& task_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  owner_map_.erase(task_id);
}

std::string HwextActorOwnerRegistry::GetOwner(
    const actor::TaskId& task_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = owner_map_.find(task_id);
  if (it == owner_map_.end()) {
    return std::string();
  }
  return it->second;
}

bool HwextActorOwnerRegistry::CheckOwnership(
    const actor::TaskId& task_id,
    const std::string& extension_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = owner_map_.find(task_id);
  if (it == owner_map_.end()) {
    return false;
  }
  return it->second == extension_id;
}

std::vector<actor::TaskId> HwextActorOwnerRegistry::GetTasksForOwner(
    const std::string& extension_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<actor::TaskId> result;
  for (const auto& [task_id, owner] : owner_map_) {
    if (owner == extension_id) {
      result.push_back(task_id);
    }
  }
  return result;
}

size_t HwextActorOwnerRegistry::GetTaskCountForOwner(
    const std::string& extension_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t count = 0;
  for (const auto& [task_id, owner] : owner_map_) {
    if (owner == extension_id) {
      ++count;
    }
  }
  return count;
}

}  // namespace extensions
