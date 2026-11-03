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

#ifndef CONTENT_BROWSER_AI_TRANSLATOR_OHOS_H_
#define CONTENT_BROWSER_AI_TRANSLATOR_OHOS_H_

#include "echo_ai_base_ohos.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"

namespace content {

// The implementation of `blink::mojom::Translator` which not only echoes back
// the input text used for testing.
class TranslatorOHOS : public EchoAIBaseOHOS, public blink::mojom::Translator {
 public:
  TranslatorOHOS(WebContents* web_contents,
                 base::OnceCallback<void(bool)>,
                 blink::mojom::TranslatorCreateOptionsPtr options);
  TranslatorOHOS(const TranslatorOHOS&) = delete;
  TranslatorOHOS& operator=(const TranslatorOHOS&) = delete;

 private:
  // `blink::mojom::Translator` implementation.
  void Translate(const std::string& input,
                 mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                     pending_responder) override;
  void TranslateStreaming(
      const std::string& input,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_TRANSLATOR_OHOS_H_
