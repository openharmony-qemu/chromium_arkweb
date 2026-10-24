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

#ifndef NWEB_EXTENSION_AGENT_HANDLER_DELEGATE_H_
#define NWEB_EXTENSION_AGENT_HANDLER_DELEGATE_H_

#include <string>

namespace OHOS::NWeb {

class NWebExtensionAgentHandlerDelegate {
 public:
  // Broadcasts a trigger event to all enabled extensions with hwext.agent
  // permission via HwextAgentTriggerRouter. Each qualifying extension
  // receives the event independently with its own dedup/rate-limit.
  // |trigger_id|: opaque unique trigger ID
  // |type|: trigger type identifier
  // |source|: "browser" or "os"
  // |tab_id|: tab reference, -1 if not present
  // |timestamp_ms|: Unix epoch milliseconds
  static void onTriggered(const std::string& trigger_id,
                              const std::string& type,
                              const std::string& source,
                              int tab_id,
                              double timestamp_ms);
};

}  // namespace OHOS::NWeb

#endif  // NWEB_EXTENSION_AGENT_HANDLER_DELEGATE_H_
