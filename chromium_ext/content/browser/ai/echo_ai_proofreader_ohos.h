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

#ifndef CONTENT_BROWSER_AI_ECHO_AI_PROOFREADER_OHOS_H_
#define CONTENT_BROWSER_AI_ECHO_AI_PROOFREADER_OHOS_H_

#include "echo_ai_base_ohos.h"
#include "third_party/blink/public/mojom/ai/ai_proofreader.mojom.h"

namespace content {

// The implementation of `blink::mojom::AIProofreader` which only echoes back
// the prompt text used for testing.
class EchoAIProofreaderOHOS : public EchoAIBaseOHOS,
                              public blink::mojom::AIProofreader {
 public:
  EchoAIProofreaderOHOS(WebContents* web_contents,
                        base::OnceCallback<void(bool)>,
                        blink::mojom::AIProofreaderCreateOptionsPtr options);
  EchoAIProofreaderOHOS(const EchoAIProofreaderOHOS&) = delete;
  EchoAIProofreaderOHOS& operator=(const EchoAIProofreaderOHOS&) = delete;

 private:
  // `blink::mojom::AIProofreader` implementation.
  void Proofread(const std::string& input,
                 mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                     pending_responder) override;
  void GetCorrectionType(
      const std::string& input,
      const std::string& corrected_input,
      const std::string& correction,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_PROOFREADER_OHOS_H_
