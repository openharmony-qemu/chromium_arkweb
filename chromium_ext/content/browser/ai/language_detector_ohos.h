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

#ifndef CONTENT_BROWSER_AI_LANGUAGE_DETECTOR_OHOS_H_
#define CONTENT_BROWSER_AI_LANGUAGE_DETECTOR_OHOS_H_

#include "arkweb/chromium_ext/third_party/blink/public/mojom/ai/ai_language_detector.mojom.h"
#include "echo_ai_base_ohos.h"

namespace content {

class LanguageDetectorOHOS : public EchoAIBaseOHOS,
                             public blink::mojom::AILanguageDetector {
 public:
  LanguageDetectorOHOS(WebContents* web_contents,
                       base::OnceCallback<void(bool)>);
  LanguageDetectorOHOS(const LanguageDetectorOHOS&) = delete;
  LanguageDetectorOHOS& operator=(const LanguageDetectorOHOS&) = delete;

 private:
  // `blink::mojom::Translator` implementation.
  void Detect(const std::string& input,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_LANGUAGE_DETECTOR_OHOS_H_
