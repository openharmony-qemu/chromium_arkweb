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

#include "nweb_extension_agent_handler_delegate.h"

#include "base/logging.h"
#include "chrome/browser/extensions/api/hwext_agent/hwext_agent_api.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "nweb_extension_utils.h"

namespace OHOS::NWeb {

// static
void NWebExtensionAgentHandlerDelegate::onTriggered(
    const std::string& trigger_id,
    const std::string& type,
    const std::string& source,
    int tab_id,
    double timestamp_ms) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    LOG(ERROR) << "Failed to get browser context for agent trigger dispatch";
    return;
  }

  auto* router = extensions::HwextAgentTriggerRouter::GetFactoryInstance()->Get(
      browser_context);
  if (!router) {
    LOG(ERROR) << "Failed to get HwextAgentTriggerRouter";
    return;
  }

  extensions::AgentTriggerData data;
  data.trigger_id = trigger_id;
  data.type = type;
  data.source = source;
  if (tab_id >= 0) {
    data.tab_id = tab_id;
  }
  data.timestamp_ms = timestamp_ms;

  router->DispatchTrigger(data);
}

}  // namespace OHOS::NWeb
