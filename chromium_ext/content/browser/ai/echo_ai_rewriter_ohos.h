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

#ifndef CONTENT_BROWSER_AI_ECHO_AI_REWRITER_OHOS_H_
#define CONTENT_BROWSER_AI_ECHO_AI_REWRITER_OHOS_H_

#include "echo_ai_base_ohos.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"

namespace content {

// The implementation of `blink::mojom::AIRewriter` which only echoes back
// the input text used for testing.
class EchoAIRewriterOHOS : public EchoAIBaseOHOS,
                           public blink::mojom::AIRewriter {
 public:
  EchoAIRewriterOHOS(WebContents* web_contents,
                     base::OnceCallback<void(bool)>,
                     blink::mojom::AIRewriterCreateOptionsPtr options);
  EchoAIRewriterOHOS(const EchoAIRewriterOHOS&) = delete;
  EchoAIRewriterOHOS& operator=(const EchoAIRewriterOHOS&) = delete;

 private:
  // `blink::mojom::AIRewriter` implementation.
  void Rewrite(const std::string& input,
               const std::optional<std::string>& context,
               mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                   pending_responder) override;
  void MeasureUsage(const std::string& input,
                    const std::string& context,
                    MeasureUsageCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_REWRITER_OHOS_H_
