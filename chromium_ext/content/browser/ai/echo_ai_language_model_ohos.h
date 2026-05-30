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

#ifndef CONTENT_BROWSER_AI_ECHO_AI_LANGUAGE_MODEL_OHOS_H_
#define CONTENT_BROWSER_AI_ECHO_AI_LANGUAGE_MODEL_OHOS_H_

#include "echo_ai_base_ohos.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"

namespace content {

// The implementation of `blink::mojom::AILanguageModel` which only echoes
// back the prompt text used for testing.
class EchoAILanguageModelOHOS : public EchoAIBaseOHOS,
                                public blink::mojom::AILanguageModel {
 public:
  explicit EchoAILanguageModelOHOS(
      WebContents*,
      base::OnceCallback<void(bool)>,
      blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
      base::flat_set<blink::mojom::AILanguageModelPromptType> input_types,
      std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_inputs,
      std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_outputs,
      std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
      uint32_t initial_tokens_size);
  EchoAILanguageModelOHOS(const EchoAILanguageModelOHOS&) = delete;
  EchoAILanguageModelOHOS& operator=(const EchoAILanguageModelOHOS&) = delete;

  ~EchoAILanguageModelOHOS() override;

 private:
  friend class EchoAILanguageModelOHOSTest;
  // `blink::mojom::AILanguageModel` implementation.
  void Prompt(std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
              on_device_model::mojom::ResponseConstraintPtr constraint,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Append(std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Fork(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client) override;
  void Destroy() override;
  void MeasureInputUsage(
      std::vector<blink::mojom::AILanguageModelPromptPtr> input,
      MeasureInputUsageCallback callback) override;

  bool is_destroyed_ = false;
  uint64_t current_tokens_ = 0;
  blink::mojom::AILanguageModelSamplingParamsPtr sampling_params_;
  // Prompt types supported by the language model in this session.
  base::flat_set<blink::mojom::AILanguageModelPromptType> input_types_;
  std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_inputs_;
  std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_outputs_;

  // Initial prompts are echoed on the first Prompt call.
  std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts_;

  std::vector<blink::mojom::AILanguageModelPromptPtr> prompt_history_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_LANGUAGE_MODEL_OHOS_H_
