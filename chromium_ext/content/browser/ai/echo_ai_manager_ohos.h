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

#ifndef CONTENT_BROWSER_AI_ECHO_AI_MANAGER_OHOS_H_
#define CONTENT_BROWSER_AI_ECHO_AI_MANAGER_OHOS_H_

#include <variant>

#include "base/containers/flat_set.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"

#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

// The implementation of `blink::mojom::AIManager` that creates session which
// only echoes back the prompt text used for testing, and all the parameters
// will be set using the default value.
class EchoAIManagerOHOS : public blink::mojom::AIManager,
                          public blink::mojom::TranslationManager,
                          public WebContentsUserData<EchoAIManagerOHOS> {
 public:
  // The context size for EchoAIManagerOHOS is intentionally set to a small
  // value so we can easily simulate the context overflow scenario.
  static constexpr int kMaxContextSizeInTokens = 1000;

  EchoAIManagerOHOS(const EchoAIManagerOHOS&) = delete;
  EchoAIManagerOHOS& operator=(const EchoAIManagerOHOS&) = delete;

  ~EchoAIManagerOHOS() override;

  void AddReceiver(mojo::PendingReceiver<blink::mojom::AIManager> receiver);

  void AddReceiver(mojo::PendingReceiver<blink::mojom::TranslationManager> receiver);
 private:
  friend class WebContentsUserData<EchoAIManagerOHOS>;
  friend class EchoAIManagerOHOSTest;

  explicit EchoAIManagerOHOS(WebContents* web_contents);

  // `blink::mojom::AIManager` implementation.
  void CanCreateLanguageModel(
      blink::mojom::AILanguageModelCreateOptionsPtr options,
      CanCreateLanguageModelCallback callback) override;
  void CreateLanguageModel(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client,
      blink::mojom::AILanguageModelCreateOptionsPtr options) override;
  void CanCreateSummarizer(blink::mojom::AISummarizerCreateOptionsPtr options,
                           CanCreateSummarizerCallback callback) override;
  void CreateSummarizer(
      mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
      blink::mojom::AISummarizerCreateOptionsPtr options) override;
  void GetLanguageModelParams(GetLanguageModelParamsCallback callback) override;
  void CanCreateWriter(blink::mojom::AIWriterCreateOptionsPtr options,
                       CanCreateWriterCallback callback) override;
  void CreateWriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
      blink::mojom::AIWriterCreateOptionsPtr options) override;
  void CanCreateRewriter(blink::mojom::AIRewriterCreateOptionsPtr options,
                         CanCreateRewriterCallback callback) override;
  void CreateRewriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
      blink::mojom::AIRewriterCreateOptionsPtr options) override;
  void CanCreateProofreader(blink::mojom::AIProofreaderCreateOptionsPtr options,
                            CanCreateProofreaderCallback callback) override;
  void CreateProofreader(
      mojo::PendingRemote<blink::mojom::AIManagerCreateProofreaderClient>
          client,
      blink::mojom::AIProofreaderCreateOptionsPtr options) override;
  void AddModelDownloadProgressObserver(
      mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
          observer_remote) override;

  template <typename CanCreateCallback>
  void CanCreateClient(CanCreateCallback callback);

  template <typename AICreateOptions,
            typename AIClientRemote,
            typename AIPendingRemote,
            typename EchoAIClient>
  void CreateClient(mojo::Remote<AIClientRemote> client_remote,
                    AICreateOptions options);

  template <typename AICreateOptions, typename CanCreateCallback>
  void CanCreateWritingAssistanceClient(AICreateOptions options,
                                        CanCreateCallback callback);

  template <typename AICreateOptions,
            typename AIClientRemote,
            typename AIPendingRemote,
            typename EchoAIClient>
  void CreateWritingAssistanceClient(mojo::PendingRemote<AIClientRemote> client,
                                     AICreateOptions options);

  template <typename AICreateOptions,
            typename AIClientRemote,
            typename AIPendingRemote,
            typename EchoAIClient>
  void ReturnAIClientCreationResult(mojo::Remote<AIClientRemote> client_remote,
                                    AICreateOptions options);

  void ReturnAILanguageModelCreationResult(
      mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
          client_remote,
      blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
      base::flat_set<blink::mojom::AILanguageModelPromptType>
          enabled_input_types,
      std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_inputs,
      std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_outputs,
      std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
      uint32_t initial_input_usage);

  void DoMockDownloadingAndReturn(base::OnceClosure callback);

  // Returns whether the current mojo receiver triggered mock model download.
  bool IsModelDownloadedForCurrentReciever() const;

  // `blink::mojom::TranslationManager` implementation.
  void CreateTranslator(
      mojo::PendingRemote<
          blink::mojom::TranslationManagerCreateTranslatorClient> client,
      blink::mojom::TranslatorCreateOptionsPtr options,
      bool) override;

  void TranslationAvailable(blink::mojom::TranslatorLanguageCodePtr source_lang,
                            blink::mojom::TranslatorLanguageCodePtr target_lang,
                            TranslationAvailableCallback callback) override;
  void CreateLanguageDetector(CreateLanguageDetectorCallback callback) override;

  // The set of mojo receivers that have triggered mock model download.
  base::flat_set<mojo::ReceiverId> model_downloaded_receivers_;

  mojo::RemoteSet<blink::mojom::ModelDownloadProgressObserver>
      download_progress_observers_;

  mojo::ReceiverSet<blink::mojom::AIManager> receivers_;
  mojo::ReceiverSet<blink::mojom::TranslationManager> translation_;

  base::WeakPtrFactory<EchoAIManagerOHOS> weak_ptr_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_MANAGER_OHOS_H_
