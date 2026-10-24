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

#include "chrome/browser/extensions/api/hwext_agent/hwext_agent_api.h"

#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/extensions/api/hwext_agent.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

// Maximum allowed length for string parameters (trigger_id, type, source).
constexpr size_t kMaxStringLength = 512;

// --- HwextAgentTriggerRouter ---

HwextAgentTriggerRouter::HwextAgentTriggerRouter(
    content::BrowserContext* context)
    : browser_context_(context) {}

HwextAgentTriggerRouter::~HwextAgentTriggerRouter() = default;

size_t HwextAgentTriggerRouter::DispatchTrigger(
    const AgentTriggerData& data) {
  // Validate trigger parameters before dispatch.
  // Reject events with empty / overly long / non-UTF-8 / control-char strings.
  auto is_valid_string = [](const std::string& s) -> bool {
    if (s.empty()) {
      LOG(ERROR) << "HwextAgentTriggerRouter: rejected - empty string parameter";
      return false;
    }
    if (s.size() > kMaxStringLength) {
      LOG(ERROR) << "HwextAgentTriggerRouter: rejected - string too long ("
                 << s.size() << " > " << kMaxStringLength << ")";
      return false;
    }
    if (!base::IsStringUTF8(s)) {
      LOG(ERROR) << "HwextAgentTriggerRouter: rejected - invalid UTF-8";
      return false;
    }
    // Reject control characters (except \t \n \r).
    for (size_t i = 0; i < s.size(); ++i) {
      unsigned char c = static_cast<unsigned char>(s[i]);
      if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
        LOG(ERROR) << "HwextAgentTriggerRouter: rejected - control char at pos "
                   << i << " (0x" << std::hex << static_cast<int>(c) << ")";
        return false;
      }
    }
    return true;
  };

  if (!is_valid_string(data.trigger_id) ||
      !is_valid_string(data.type) ||
      !is_valid_string(data.source)) {
    return 0;
  }

  // Broadcast to all enabled extensions with hwext.agent permission.
  auto* registry = ExtensionRegistry::Get(browser_context_);
  if (!registry) {
    return 0;
  }

  size_t dispatched = 0;
  for (const auto& extension_id :
       registry->enabled_extensions().GetIDs()) {
    const Extension* extension =
        registry->enabled_extensions().GetByID(extension_id);
    if (!extension) {
      continue;
    }
    if (!extension->permissions_data()->HasAPIPermission(
            mojom::APIPermissionID::kHwextAgent)) {
      continue;
    }

    DoDispatch(extension_id, data);
    ++dispatched;
  }
  return dispatched;
}

void HwextAgentTriggerRouter::DoDispatch(
    const std::string& extension_id,
    const AgentTriggerData& data) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  // Build the AgentTriggerEvent for the JS listener.
  api::hwext_agent::AgentTriggerEvent event;
  event.trigger_id = data.trigger_id;
  event.type = data.type;
  event.source = api::hwext_agent::ParseTriggerSource(data.source);
  if (data.tab_id.has_value()) {
    event.tab_id = data.tab_id.value();
  }
  event.timestamp = data.timestamp_ms;

  auto event_args = api::hwext_agent::OnTriggered::Create(event);

  auto event_for_extension = std::make_unique<Event>(
      events::HWEXT_AGENT_ON_TRIGGERED,
      api::hwext_agent::OnTriggered::kEventName,
      std::move(event_args),
      browser_context_);

  event_router->DispatchEventToExtension(extension_id,
                                          std::move(event_for_extension));
}

// --- BrowserContextKeyedAPIFactory ---

static base::LazyInstance<BrowserContextKeyedAPIFactory<HwextAgentTriggerRouter>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<HwextAgentTriggerRouter>*
HwextAgentTriggerRouter::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
