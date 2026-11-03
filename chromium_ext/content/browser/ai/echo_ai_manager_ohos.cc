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

#include "echo_ai_manager_ohos.h"

#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/language/core/common/locale_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "echo_ai_language_model_ohos.h"
#include "echo_ai_proofreader_ohos.h"
#include "echo_ai_rewriter_ohos.h"
#include "translator_ohos.h"
#include "language_detector_ohos.h"
#include "echo_ai_summarizer_ohos.h"
#include "echo_ai_writer_ohos.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"

#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

namespace {

const int kMockDownloadPreparationTimeMillisecond = 300;
const int kMockModelSizeBytes = 0x10000;

using blink::mojom::AILanguageCodePtr;

using blink::mojom::CanCreateTranslatorResult;
using blink::mojom::CreateTranslatorError;
using blink::mojom::CreateTranslatorResult;
using blink::mojom::TranslationManagerCreateTranslatorClient;
using blink::mojom::TranslatorLanguageCode;
using blink::mojom::TranslatorLanguageCodePtr;

// TODO(crbug.com/394109104): This is duplicated from chrome AIManager in order
// to keep the consistent wpt results run from CQ, which currently only supports
// running wpt_internal/ tests on content_shell, using content EchoAIManager.
// If there is enough divergence in two AI Managers' code, it should be
// refactored to share the common code or use subclasses.
auto is_language_supported = [](const AILanguageCodePtr& language) {
  return true;
};

bool IsLanguagesSupported(const std::vector<AILanguageCodePtr>& languages) {
  return std::ranges::all_of(languages, is_language_supported);
}

bool SupportedLanguages(const std::vector<AILanguageCodePtr>& input,
                        const std::vector<AILanguageCodePtr>& context,
                        const AILanguageCodePtr& output) {
  return IsLanguagesSupported(input) && IsLanguagesSupported(context) &&
         is_language_supported(output);
}

// Returns whether optional LanguageModel expected_inputs or expected_outputs
// vectors contain only supported languages. Returns true for absent languages.
bool AreExpectedLanguagesSupported(
    const std::optional<std::vector<blink::mojom::AILanguageModelExpectedPtr>>&
        expected_vector) {
  if (!expected_vector) {
    return true;
  }
  for (const auto& expected_entry : expected_vector.value()) {
    if (expected_entry->languages.has_value() &&
        !IsLanguagesSupported(expected_entry->languages.value())) {
      return false;
    }
  }
  return true;
}

// Returns whether `options` contains any unsupported AILanguageModelPromptType.
bool HasUnsupportedType(
    const blink::mojom::AILanguageModelCreateOptionsPtr& options) {
  bool has_unsupported_type = false;
  if (options) {
    if (options->expected_inputs.has_value()) {
      for (const auto& expected_input : options->expected_inputs.value()) {
        has_unsupported_type |=
            expected_input->type !=
                blink::mojom::AILanguageModelPromptType::kText &&
            !base::FeatureList::IsEnabled(
                blink::features::kAIPromptAPIMultimodalInput);
      }
    }
    if (options->expected_outputs.has_value()) {
      for (const auto& expected_output : options->expected_outputs.value()) {
        has_unsupported_type |= expected_output->type !=
                                blink::mojom::AILanguageModelPromptType::kText;
      }
    }
  }
  return has_unsupported_type;
}

}  // namespace

EchoAIManagerOHOS::EchoAIManagerOHOS(WebContents* web_contents)
    : WebContentsUserData<EchoAIManagerOHOS>(*web_contents) {}

EchoAIManagerOHOS::~EchoAIManagerOHOS() = default;

void EchoAIManagerOHOS::AddReceiver(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void EchoAIManagerOHOS::AddReceiver(
    mojo::PendingReceiver<blink::mojom::TranslationManager> receiver) {
  translation_.Add(this, std::move(receiver));
}

void EchoAIManagerOHOS::CanCreateLanguageModel(
    blink::mojom::AILanguageModelCreateOptionsPtr options,
    CanCreateLanguageModelCallback callback) {
  if (HasUnsupportedType(options)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableModelAdaptationNotAvailable);
    return;
  }
  if (options && (!AreExpectedLanguagesSupported(options->expected_inputs) ||
                  !AreExpectedLanguagesSupported(options->expected_outputs))) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }

  std::move(callback).Run(
      IsModelDownloadedForCurrentReciever()
          ? blink::mojom::ModelAvailabilityCheckResult::kAvailable
          : blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
}

void EchoAIManagerOHOS::CreateLanguageModel(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    blink::mojom::AILanguageModelCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));

  size_t initial_size = 0;
  for (const auto& initial_prompt : options->initial_prompts) {
    for (const auto& content : initial_prompt->content) {
      if (content->is_text()) {
        initial_size += content->get_text().size();
      } else {
        initial_size += 100;  // TODO(crbug.com/415304330): Improve estimate.
      }
      if (initial_size > kMaxContextSizeInTokens) {
        client_remote->OnError(
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge,
            blink::mojom::QuotaErrorInfo::New(initial_size,
                                              kMaxContextSizeInTokens));
        return;
      }
    }
  }

  if (HasUnsupportedType(options)) {
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession,
        /*quota_error_info=*/nullptr);
    return;
  }
  if (options && (!AreExpectedLanguagesSupported(options->expected_inputs) ||
                  !AreExpectedLanguagesSupported(options->expected_outputs))) {
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage,
        /*quota_error_info=*/nullptr);
    return;
  }
  base::flat_set<blink::mojom::AILanguageModelPromptType> enabled_input_types;
  if (options->expected_inputs.has_value()) {
    for (const auto& expected_input : options->expected_inputs.value()) {
      enabled_input_types.insert(expected_input->type);
    }
  }

  std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_inputs;
  if (options->expected_inputs) {
    expected_inputs = std::move(*options->expected_inputs);
  }
  std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_outputs;
  if (options->expected_outputs) {
    expected_outputs = std::move(*options->expected_outputs);
  }

  auto return_language_model_callback =
      base::BindOnce(&EchoAIManagerOHOS::ReturnAILanguageModelCreationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client_remote),
                     std::move(options->sampling_params), enabled_input_types,
                     std::move(expected_inputs), std::move(expected_outputs),
                     std::move(options->initial_prompts), initial_size);

  if (!IsModelDownloadedForCurrentReciever()) {
    // Simulate downloading the model; cache state for the current receiver.
    model_downloaded_receivers_.insert(receivers_.current_receiver());
    content::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EchoAIManagerOHOS::DoMockDownloadingAndReturn,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(return_language_model_callback)),
        base::Milliseconds(kMockDownloadPreparationTimeMillisecond));
  } else {
    std::move(return_language_model_callback).Run();
  }
}

void EchoAIManagerOHOS::CanCreateSummarizer(
    blink::mojom::AISummarizerCreateOptionsPtr options,
    CanCreateSummarizerCallback callback) {
  CanCreateWritingAssistanceClient<blink::mojom::AISummarizerCreateOptionsPtr,
                                   CanCreateSummarizerCallback>(
      std::move(options), std::move(callback));
}

void EchoAIManagerOHOS::CreateSummarizer(
    mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
    blink::mojom::AISummarizerCreateOptionsPtr options) {
  CreateWritingAssistanceClient<blink::mojom::AISummarizerCreateOptionsPtr,
                                blink::mojom::AIManagerCreateSummarizerClient,
                                blink::mojom::AISummarizer, EchoAISummarizerOHOS>(
      std::move(client), std::move(options));
}

void EchoAIManagerOHOS::GetLanguageModelParams(
    GetLanguageModelParamsCallback callback) {
  std::move(callback).Run(blink::mojom::AILanguageModelParams::New(
      blink::mojom::AILanguageModelSamplingParams::New(
          optimization_guide::features::GetOnDeviceModelDefaultTopK(),
          optimization_guide::features::GetOnDeviceModelDefaultTemperature()),
      blink::mojom::AILanguageModelSamplingParams::New(
          optimization_guide::features::GetOnDeviceModelMaxTopK(),
          /*temperature=*/2.0f)));
}

void EchoAIManagerOHOS::CanCreateWriter(
    blink::mojom::AIWriterCreateOptionsPtr options,
    CanCreateWriterCallback callback) {
  CanCreateWritingAssistanceClient<blink::mojom::AIWriterCreateOptionsPtr,
                                   CanCreateWriterCallback>(
      std::move(options), std::move(callback));
}

void EchoAIManagerOHOS::CreateWriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
    blink::mojom::AIWriterCreateOptionsPtr options) {
  CreateWritingAssistanceClient<blink::mojom::AIWriterCreateOptionsPtr,
                                blink::mojom::AIManagerCreateWriterClient,
                                blink::mojom::AIWriter, EchoAIWriterOHOS>(
      std::move(client), std::move(options));
}

void EchoAIManagerOHOS::CanCreateRewriter(
    blink::mojom::AIRewriterCreateOptionsPtr options,
    CanCreateRewriterCallback callback) {
  CanCreateWritingAssistanceClient<blink::mojom::AIRewriterCreateOptionsPtr,
                                   CanCreateRewriterCallback>(
      std::move(options), std::move(callback));
}

void EchoAIManagerOHOS::CreateRewriter(
    mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
    blink::mojom::AIRewriterCreateOptionsPtr options) {
  CreateWritingAssistanceClient<blink::mojom::AIRewriterCreateOptionsPtr,
                                blink::mojom::AIManagerCreateRewriterClient,
                                blink::mojom::AIRewriter, EchoAIRewriterOHOS>(
      std::move(client), std::move(options));
}

void EchoAIManagerOHOS::CanCreateProofreader(
    blink::mojom::AIProofreaderCreateOptionsPtr options,
    CanCreateProofreaderCallback callback) {
  if (options &&
      !SupportedLanguages(options->expected_input_languages, {},
                          options->correction_explanation_language)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateClient<CanCreateProofreaderCallback>(std::move(callback));
}

void EchoAIManagerOHOS::CreateProofreader(
    mojo::PendingRemote<blink::mojom::AIManagerCreateProofreaderClient> client,
    blink::mojom::AIProofreaderCreateOptionsPtr options) {
  mojo::Remote<blink::mojom::AIManagerCreateProofreaderClient> client_remote(
      std::move(client));
  if (options &&
      !SupportedLanguages(options->expected_input_languages, {},
                          options->correction_explanation_language)) {
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage,
        /*quota_error_info=*/nullptr);
    return;
  }

  CreateClient<blink::mojom::AIProofreaderCreateOptionsPtr,
               blink::mojom::AIManagerCreateProofreaderClient,
               blink::mojom::AIProofreader, EchoAIProofreaderOHOS>(
      std::move(client_remote), std::move(options));
}

void EchoAIManagerOHOS::AddModelDownloadProgressObserver(
    mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
        observer_remote) {
  download_progress_observers_.Add(std::move(observer_remote));
}

template <typename CanCreateCallback>
void EchoAIManagerOHOS::CanCreateClient(CanCreateCallback callback) {
  std::move(callback).Run(
      IsModelDownloadedForCurrentReciever()
          ? blink::mojom::ModelAvailabilityCheckResult::kAvailable
          : blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
}

template <typename AICreateOptions,
          typename AIClientRemote,
          typename AIPendingRemote,
          typename EchoAIClient>
void EchoAIManagerOHOS::CreateClient(
    mojo::Remote<AIClientRemote> client_remote,
    AICreateOptions options) {
  auto return_task = base::BindOnce(
      &EchoAIManagerOHOS::ReturnAIClientCreationResult<
          AICreateOptions, AIClientRemote, AIPendingRemote, EchoAIClient>,
      weak_ptr_factory_.GetWeakPtr(), std::move(client_remote),
      std::move(options));
  if (!IsModelDownloadedForCurrentReciever()) {
    // Simulate downloading the model; cache state for the current receiver.
    model_downloaded_receivers_.insert(receivers_.current_receiver());
    content::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EchoAIManagerOHOS::DoMockDownloadingAndReturn,
                       weak_ptr_factory_.GetWeakPtr(), std::move(return_task)),
        base::Milliseconds(kMockDownloadPreparationTimeMillisecond));
  } else {
    std::move(return_task).Run();
  }
}

template <typename AICreateOptions, typename CanCreateCallback>
void EchoAIManagerOHOS::CanCreateWritingAssistanceClient(
    AICreateOptions options,
    CanCreateCallback callback) {
  if (options && !SupportedLanguages(options->expected_input_languages,
                                     options->expected_context_languages,
                                     options->output_language)) {
    std::move(callback).Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage);
    return;
  }
  CanCreateClient<CanCreateCallback>(std::move(callback));
}

template <typename AICreateOptions,
          typename AIClientRemote,
          typename AIPendingRemote,
          typename EchoAIClient>
void EchoAIManagerOHOS::CreateWritingAssistanceClient(
    mojo::PendingRemote<AIClientRemote> client,
    AICreateOptions options) {
  mojo::Remote<AIClientRemote> client_remote(std::move(client));
  if (options && !SupportedLanguages(options->expected_input_languages,
                                     options->expected_context_languages,
                                     options->output_language)) {
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage,
        /*quota_error_info=*/nullptr);
    return;
  }

  CreateClient<AICreateOptions, AIClientRemote, AIPendingRemote, EchoAIClient>(
      std::move(client_remote), std::move(options));
}

template <typename AICreateOptions,
          typename AIClientRemote,
          typename AIPendingRemote,
          typename EchoAIClient>
void EchoAIManagerOHOS::ReturnAIClientCreationResult(
    mojo::Remote<AIClientRemote> client_remote,
    AICreateOptions options) {
  mojo::PendingRemote<AIPendingRemote> pending_remote;
  mojo::PendingReceiver<AIPendingRemote> pending_receiver =
      pending_remote.InitWithNewPipeAndPassReceiver();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<EchoAIClient>(
          &GetWebContents(),
          base::BindOnce(
              [](mojo::Remote<AIClientRemote> client_remote,
                 mojo::PendingRemote<AIPendingRemote> pending_remote,
                 bool success) {
                if (success) {
                  client_remote->OnResult(std::move(pending_remote));
                } else {
                  client_remote->OnError(
                      blink::mojom::AIManagerCreateClientError::
                          kUnableToCreateSession,
                      /*quota_error_info=*/nullptr);
                }
              },
              std::move(client_remote), std::move(pending_remote)),
          std::move(options)),
      std::move(pending_receiver));
}

void EchoAIManagerOHOS::ReturnAILanguageModelCreationResult(
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote,
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
    base::flat_set<blink::mojom::AILanguageModelPromptType> enabled_input_types,
    std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_inputs,
    std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_outputs,
    std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
    uint32_t initial_input_usage) {
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;
  mojo::PendingReceiver<blink::mojom::AILanguageModel> pending_receiver =
      language_model.InitWithNewPipeAndPassReceiver();
  auto model_sampling_params =
      sampling_params
          ? std::move(sampling_params)
          : blink::mojom::AILanguageModelSamplingParams::New(
                optimization_guide::features::GetOnDeviceModelDefaultTopK(),
                optimization_guide::features::
                    GetOnDeviceModelDefaultTemperature());

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<EchoAILanguageModelOHOS>(
          &GetWebContents(),
          base::BindOnce(
              [](mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
                     client_remote,
                 mojo::PendingRemote<blink::mojom::AILanguageModel>
                     language_model,
                 blink::mojom::AILanguageModelSamplingParamsPtr
                     model_sampling_params,
                 base::flat_set<blink::mojom::AILanguageModelPromptType>
                     enabled_input_types,
                 uint32_t initial_input_usage, bool success) {
                if (!success) {
                  client_remote->OnError(
                      blink::mojom::AIManagerCreateClientError::
                          kUnableToCreateSession,
                      /*quota_error_info=*/nullptr);
                  return;
                }
                client_remote->OnResult(
                    std::move(language_model),
                    blink::mojom::AILanguageModelInstanceInfo::New(
                        kMaxContextSizeInTokens, initial_input_usage,
                        std::move(model_sampling_params),
                        std::vector<blink::mojom::AILanguageModelPromptType>(
                            enabled_input_types.begin(),
                            enabled_input_types.end())));
              },
              std::move(client_remote), std::move(language_model),
              model_sampling_params.Clone(), enabled_input_types,
              initial_input_usage),
          std::move(model_sampling_params), enabled_input_types,
          std::move(expected_inputs), std::move(expected_outputs),
          std::move(initial_prompts), initial_input_usage),
      std::move(pending_receiver));
}

void EchoAIManagerOHOS::DoMockDownloadingAndReturn(base::OnceClosure callback) {
  // Mock the downloading process update for testing.
  for (auto& observer : download_progress_observers_) {
    observer->OnDownloadProgressUpdate(0, kMockModelSizeBytes);
    observer->OnDownloadProgressUpdate(kMockModelSizeBytes / 3,
                                       kMockModelSizeBytes);
    observer->OnDownloadProgressUpdate(kMockModelSizeBytes / 3 * 2,
                                       kMockModelSizeBytes);
    observer->OnDownloadProgressUpdate(kMockModelSizeBytes,
                                       kMockModelSizeBytes);
  }

  std::move(callback).Run();
}

bool EchoAIManagerOHOS::IsModelDownloadedForCurrentReciever() const {
  return model_downloaded_receivers_.contains(receivers_.current_receiver());
}

void EchoAIManagerOHOS::CreateTranslator(
    mojo::PendingRemote<TranslationManagerCreateTranslatorClient> client,
    blink::mojom::TranslatorCreateOptionsPtr options,
    bool) {
  mojo::Remote<TranslationManagerCreateTranslatorClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::Translator> pending_remote;
  mojo::PendingReceiver<blink::mojom::Translator> pending_receiver =
      pending_remote.InitWithNewPipeAndPassReceiver();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<TranslatorOHOS>(
          &GetWebContents(),
          base::BindOnce(
              [](mojo::Remote<TranslationManagerCreateTranslatorClient>
                     client_remote,
                 mojo::PendingRemote<blink::mojom::Translator> pending_remote,
                 TranslatorLanguageCodePtr source_language,
                 TranslatorLanguageCodePtr target_language, bool success) {
                if (!success) {
                  pending_remote.reset();
                  client_remote->OnResult(
                      CreateTranslatorResult::NewError(
                          CreateTranslatorError::kFailedToCreateTranslator),
                      nullptr, nullptr);
                } else {
                  client_remote->OnResult(CreateTranslatorResult::NewTranslator(
                                              std::move(pending_remote)),
                                          std::move(source_language),
                                          std::move(target_language));
                }
              },
              std::move(client_remote), std::move(pending_remote),
              TranslatorLanguageCode::New(options->source_lang->code),
              TranslatorLanguageCode::New(options->target_lang->code)),
          std::move(options)),
      std::move(pending_receiver));
}

void EchoAIManagerOHOS::TranslationAvailable(
    TranslatorLanguageCodePtr source_lang,
    TranslatorLanguageCodePtr target_lang,
    TranslationAvailableCallback callback) {
  std::move(callback).Run(CanCreateTranslatorResult::kReadily);
}

void EchoAIManagerOHOS::CreateLanguageDetector(
    CreateLanguageDetectorCallback callback) {
  mojo::PendingRemote<blink::mojom::AILanguageDetector> pending_remote;
  mojo::PendingReceiver<blink::mojom::AILanguageDetector> pending_receiver =
      pending_remote.InitWithNewPipeAndPassReceiver();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<LanguageDetectorOHOS>(
          &GetWebContents(),
          base::BindOnce(
              [](CreateLanguageDetectorCallback callback,
                 mojo::PendingRemote<blink::mojom::AILanguageDetector> pending_remote,
                 bool success) {
                if (!success) {
                  pending_remote.reset();
                }
                std::move(callback).Run(std::move(pending_remote));
              },
              std::move(callback), std::move(pending_remote))),
      std::move(pending_receiver));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(EchoAIManagerOHOS);

}  // namespace content
