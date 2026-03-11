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

#include "echo_ai_base_ohos.h"

#include "base/uuid.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"

namespace content {

EchoAIBaseOHOS::EchoAIBaseOHOS(WebContents* web_contents,
                               base::OnceCallback<void(bool)> callback,
                               AISessionType type)
    : WebContentsObserver(web_contents),
      callback_(std::move(callback)),
      uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      type_(type) {}

EchoAIBaseOHOS::~EchoAIBaseOHOS() {
  if (WebContentsDelegate* delegate = GetDelegate()) {
    delegate->OnDestroyAISession(type_, uuid_);
  }
}

WebContentsDelegate* EchoAIBaseOHOS::GetDelegate() {
  if (web_contents()) {
    return web_contents()->GetDelegate();
  }
  return nullptr;
}

void EchoAIBaseOHOS::OnCreateAISession(const std::string& input) {
  if (WebContentsDelegate* delegate = GetDelegate()) {
    delegate->OnCreateAISession(
        type_, uuid_, input,
        base::BindRepeating(&EchoAIBaseOHOS::HandleCreateSessionCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    std::move(callback_).Run(false);
  }
}

void EchoAIBaseOHOS::HandleCreateSessionCallback(AISessionResultType state,
                                                 const std::string& content) {
  if (!callback_ || create_callback_called_) {
    return;
  }
  create_callback_called_ = true;
  if (state == AISessionResultType::SUCCESS) {
    std::move(callback_).Run(true);
  } else {
    std::move(callback_).Run(false);
  }
}

void EchoAIBaseOHOS::OnExecuteAIAction(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  if (WebContentsDelegate* delegate = GetDelegate()) {
    delegate->OnExecuteAIAction(
        type_, uuid_, input,
        base::BindRepeating(&EchoAIBaseOHOS::HandleExecuteActionCallback,
                            weak_ptr_factory_.GetWeakPtr(), responder_id));
  } else {
    blink::mojom::ModelStreamingResponder* responder =
        responder_set_.Get(responder_id);
    if (responder) {
      responder->OnError(
          blink::mojom::ModelStreamingResponseStatus::kErrorUnknown,
          /*quota_error_info=*/nullptr);
    }
  }
}

void EchoAIBaseOHOS::HandleExecuteActionCallback(
    mojo::RemoteSetElementId responder_id,
    AISessionResultType state,
    const std::string& content) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  switch (state) {
    case AISessionResultType::RUNNING:
      responder->OnStreaming(content);
      break;
    case AISessionResultType::SUCCESS:
      responder->OnStreaming(content);
      responder->OnCompletion(/*context_info=*/nullptr);
      break;
    case AISessionResultType::FAILURE:
      responder->OnError(
          blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
          /*quota_error_info=*/nullptr);
      break;
  }
}

}  // namespace content
