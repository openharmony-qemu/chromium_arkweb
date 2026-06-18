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

#ifndef CONTENT_BROWSER_AI_ECHO_AI_BASE_OHOS_H_
#define CONTENT_BROWSER_AI_ECHO_AI_BASE_OHOS_H_

#include "arkweb/chromium_ext/content/public/browser/ai_session_types.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {
class WebContents;
class WebContentsDelegate;

class EchoAIBaseOHOS : public WebContentsObserver {
 protected:
  EchoAIBaseOHOS(WebContents* web_contents,
                 base::OnceCallback<void(bool)>,
                 AISessionType type);
  EchoAIBaseOHOS(const EchoAIBaseOHOS&) = delete;
  EchoAIBaseOHOS& operator=(const EchoAIBaseOHOS&) = delete;
  ~EchoAIBaseOHOS();

  WebContentsDelegate* GetDelegate();
  void OnCreateAISession(const std::string& input);
  void HandleCreateSessionCallback(AISessionResultType state,
                                   const std::string& content);
  void OnExecuteAIAction(
      const std::string& input,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder);
  void HandleExecuteActionCallback(mojo::RemoteSetElementId responder_id,
                                   AISessionResultType state,
                                   const std::string& content);

  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  base::OnceCallback<void(bool)> callback_;
  const std::string uuid_;
  bool create_callback_called_ = false;
  AISessionType type_;

  base::WeakPtrFactory<EchoAIBaseOHOS> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_BASE_OHOS_H_
