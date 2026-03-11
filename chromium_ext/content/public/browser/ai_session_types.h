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
