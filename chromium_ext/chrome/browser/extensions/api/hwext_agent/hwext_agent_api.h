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

#ifndef CHROME_BROWSER_EXTENSIONS_API_HWEXT_AGENT_HWEXT_AGENT_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HWEXT_AGENT_HWEXT_AGENT_API_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_registry_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// AgentTriggerEvent data for internal dispatch.
struct AgentTriggerData {
  std::string trigger_id;
  std::string type;
  std::string source;  // "browser" or "os"
  std::optional<int> tab_id;
  double timestamp_ms = 0;
};

// Per-extension trigger router.
class HwextAgentTriggerRouter : public BrowserContextKeyedAPI {
 public:
  explicit HwextAgentTriggerRouter(content::BrowserContext* context);
  ~HwextAgentTriggerRouter() override;

  // Broadcasts a trigger event to all enabled extensions with hwext.agent
  // permission. Returns count of extensions dispatched to.
  size_t DispatchTrigger(const AgentTriggerData& data);

  static BrowserContextKeyedAPIFactory<HwextAgentTriggerRouter>*
      GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<HwextAgentTriggerRouter>;

  void DoDispatch(const std::string& extension_id,
                  const AgentTriggerData& data);

  raw_ptr<content::BrowserContext> browser_context_;

  static const char* service_name() { return "HwextAgentTriggerRouter"; }

  HwextAgentTriggerRouter(const HwextAgentTriggerRouter&) = delete;
  HwextAgentTriggerRouter& operator=(const HwextAgentTriggerRouter&) = delete;
};

template <>
struct BrowserContextFactoryDependencies<HwextAgentTriggerRouter> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<HwextAgentTriggerRouter>* factory) {
    factory->DependsOn(EventRouterFactory::GetInstance());
    factory->DependsOn(ExtensionRegistryFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HWEXT_AGENT_HWEXT_AGENT_API_H_
