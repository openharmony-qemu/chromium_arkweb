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

#ifndef ARKWEB_CHROMIUM_EXT_CONTENT_PUBLIC_BROWSER_AI_SESSION_TYPES_H_
#define ARKWEB_CHROMIUM_EXT_CONTENT_PUBLIC_BROWSER_AI_SESSION_TYPES_H_

#include <string>
#include "base/functional/callback.h"

namespace content {

enum class AISessionType {
  TRANSLATOR = 0,
  LANGUAGE_DETECTOR,
  SUMMARIZER,
  WRITER,
  REWRITER,
  PROMPT,
  PROOFREADER,
};

enum class AISessionResultType {
  SUCCESS = 0,
  FAILURE = 1,
  RUNNING = 2,
};

using AISessionCallback = base::RepeatingCallback<void(AISessionResultType state, const std::string& content)>;

}  // namespace content

#endif  // ARKWEB_CHROMIUM_EXT_CONTENT_PUBLIC_BROWSER_AI_SESSION_TYPES_H_
