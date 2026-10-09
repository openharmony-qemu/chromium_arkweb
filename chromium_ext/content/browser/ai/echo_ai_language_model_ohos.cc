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

#include "echo_ai_language_model_ohos.h"

#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "echo_ai_manager_ohos.h"
#include "media/audio/audio_debug_file_writer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/limits.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/gfx/codec/png_codec.h"
namespace content {

std::string WriteBitmapN32ToFile(const ::SkBitmap bitmap, std::string uuid) {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_TEMP, &file_path);
  file_path = file_path.AppendASCII(uuid);
  if (!base::DirectoryExists(file_path)) {
    base::CreateDirectory(file_path);
  }
  file_path = file_path.AppendASCII(
      base::Uuid::GenerateRandomV4().AsLowercaseString() + ".png");

  // Encode SkBitmap to PNG
  std::optional<std::vector<uint8_t>> png_data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                        /*discard_transparency=*/false);

  if (!png_data) {
    LOG(ERROR) << "Failed to encode PNG";
    return "";
  }

  // Write PNG data to file
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create file: " << file_path;
    return "";
  }

  std::optional<size_t> written =
      file.WriteAtCurrentPos(base::as_bytes(base::span(*png_data)));

  if (!written.has_value() || written.value() != png_data->size()) {
    LOG(ERROR) << "Failed to write complete PNG data";
    return "";
  }

  return file_path.value();
}

std::string WriteAudioDataToFile(
    const ::on_device_model::mojom::AudioDataPtr& audio_data,
    std::string uuid) {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_TEMP, &file_path);
  file_path = file_path.AppendASCII(uuid);
  if (!base::DirectoryExists(file_path)) {
    base::CreateDirectory(file_path);
  }
  file_path = file_path.AppendASCII(
      base::Uuid::GenerateRandomV4().AsLowercaseString() + ".wav");

  // Validate audio parameters
  if (audio_data->channel_count <= 0 || audio_data->frame_count <= 0 ||
      audio_data->sample_rate <= 0) {
    LOG(ERROR) << "Invalid audio parameters: channel_count="
               << audio_data->channel_count
               << " frame_count=" << audio_data->frame_count
               << " sample_rate=" << audio_data->sample_rate;
    return "";
  }

  if (audio_data->sample_rate > media::limits::kMaxSampleRate) {
    LOG(ERROR) << "Invalid audio parameters: sample_rate="
               << audio_data->sample_rate << " exceeds maximum";
    return "";
  }

  // Use 64-bit arithmetic to detect integer overflow before multiplication.
  // Casting to uint64_t first makes negative int32 values wrap positively,
  // but line 85's positivity check ensures channel_count > 0, so this is safe.
  auto product = base::CheckMul(audio_data->channel_count, audio_data->frame_count);
  if (!product.IsValid() ||
    product.ValueOrDie() > base::checked_cast<uint64_t>(audio_data->data.size())) {
    LOG(ERROR) << "Audio data size mismatch: channel_count="
               << audio_data->channel_count
               << " frame_count=" << audio_data->frame_count
               << " data.size()=" << audio_data->data.size();
    return "";
  }

  // Create AudioParameters from AudioData
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::Guess(audio_data->channel_count),
      audio_data->sample_rate, audio_data->frame_count);

  if (!params.IsValid()) {
    LOG(ERROR) << "Invalid audio parameters for WAV output";
    return "";
  }

  // Create file
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create file: " << file_path;
    return "";
  }

  // Create AudioDebugFileWriter
  auto writer = media::AudioDebugFileWriter::Create(params, std::move(file));

  // Create AudioBus from the float data
  auto audio_bus = media::AudioBus::Create(audio_data->channel_count,
                                           audio_data->frame_count);

  // Copy data from AudioData to AudioBus
  // Note: AudioData stores interleaved float data, AudioBus stores planar
  audio_bus->FromInterleaved<media::Float32SampleTypeTraits>(
      audio_data->data.data(), audio_data->frame_count);

  // Write to file
  writer->Write(*audio_bus);

  return file_path.value();
  // WAV header is finalized when writer is destroyed
}

std::string ConvertRole(blink::mojom::AILanguageModelPromptRole role) {
  std::string result = "";
  switch (role) {
    case blink::mojom::AILanguageModelPromptRole::kSystem:
      result = "system";
      break;
    case blink::mojom::AILanguageModelPromptRole::kUser:
      result = "user";
      break;
    case blink::mojom::AILanguageModelPromptRole::kAssistant:
      result = "assistant";
      break;
    case blink::mojom::AILanguageModelPromptRole::kToolCall:
      result = "tool-call";
      break;
    case blink::mojom::AILanguageModelPromptRole::kToolResponse:
      result = "tool-response";
      break;
    default:
      NOTREACHED();
  }
  return result;
}

base::Value::List ConvertContent(
    const std::vector<blink::mojom::AILanguageModelPromptContentPtr>& contents,
    std::string uuid) {
  base::Value::List list;
  for (auto& content : contents) {
    base::Value::Dict dict;
    if (content->is_text()) {
      dict.Set("type", "text");
      dict.Set("value", content->get_text());
    } else if (content->is_bitmap()) {
      dict.Set("type", "image");
      dict.Set("value", WriteBitmapN32ToFile(content->get_bitmap(), uuid));
    } else if (content->is_audio()) {
      dict.Set("type", "audio");
      dict.Set("value", WriteAudioDataToFile(content->get_audio(), uuid));
    } else {
      NOTIMPLEMENTED_LOG_ONCE();
    }
    list.Append(std::move(dict));
  }
  return list;
}

base::Value::List ConvertExpected(
    const std::vector<blink::mojom::AILanguageModelExpectedPtr>&
        expected_array) {
  base::Value::List list;
  for (const auto& expected : expected_array) {
    base::Value::Dict dict;
    switch (expected->type) {
      case blink::mojom::AILanguageModelPromptType::kText:
        dict.Set("type", "text");
        break;
      case blink::mojom::AILanguageModelPromptType::kImage:
        dict.Set("type", "image");
        break;
      case blink::mojom::AILanguageModelPromptType::kAudio:
        dict.Set("type", "audio");
        break;
      case blink::mojom::AILanguageModelPromptType::kToolCall:
        dict.Set("type", "tool-call");
        break;
      case blink::mojom::AILanguageModelPromptType::kToolResponse:
        dict.Set("type", "tool-response");
        break;
      default:
        NOTREACHED();
    }
    base::Value::List languages;
    if (expected->languages.has_value()) {
      for (const auto& language : expected->languages.value()) {
        languages.Append(language->code);
      }
      dict.Set("languages", std::move(languages));
      list.Append(std::move(dict));
    }
  }
  return list;
}

EchoAILanguageModelOHOS::EchoAILanguageModelOHOS(
    WebContents* web_contents,
    base::OnceCallback<void(bool)> callback,
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
    base::flat_set<blink::mojom::AILanguageModelPromptType> input_types,
    std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_inputs,
    std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_outputs,
    std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
    uint32_t initial_tokens_size)
    : EchoAIBaseOHOS(web_contents, std::move(callback), AISessionType::PROMPT),
      current_tokens_(initial_tokens_size),
      sampling_params_(std::move(sampling_params)),
      input_types_(input_types),
      expected_inputs_(std::move(expected_inputs)),
      expected_outputs_(std::move(expected_outputs)),
      initial_prompts_(std::move(initial_prompts)) {
  base::Value::Dict dict;
  if (sampling_params_) {
    dict.Set("temperature", sampling_params_->temperature);
    dict.Set("topK", static_cast<int>(sampling_params_->top_k));
  }
  base::Value::List list;
  for (const auto& prompt : initial_prompts_) {
    base::Value::Dict dict_prompt;
    dict_prompt.Set("role", ConvertRole(prompt->role));
    dict_prompt.Set("content", ConvertContent(prompt->content, uuid_));
    dict_prompt.Set("prefix", prompt->is_prefix);
    list.Append(std::move(dict_prompt));
  }
  dict.Set("initialPrompts", std::move(list));
  dict.Set("expectedInputs", ConvertExpected(expected_inputs_));
  dict.Set("expectedOutputs", ConvertExpected(expected_outputs_));
  std::string output;
  base::JSONWriter::Write(dict, &output);

  EchoAIBaseOHOS::OnCreateAISession(output);
}

EchoAILanguageModelOHOS::~EchoAILanguageModelOHOS() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_TEMP, &file_path);
  file_path = file_path.AppendASCII(uuid_);
  if (base::DirectoryExists(file_path)) {
    base::DeletePathRecursively(file_path);
  }
}

void EchoAILanguageModelOHOS::Prompt(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    on_device_model::mojom::ResponseConstraintPtr constraint,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (is_destroyed_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*quota_error_info=*/nullptr);
    return;
  }

  size_t new_tokens = 0;
  blink::mojom::ModelStreamingResponseStatus error_status;
  if (!ValidatePromptInput(prompts, error_status, new_tokens)) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnError(
        error_status,
        error_status == blink::mojom::ModelStreamingResponseStatus::
                            kErrorInputTooLarge
            ? blink::mojom::QuotaErrorInfo::New(
                  static_cast<uint32_t>(new_tokens),
                  static_cast<uint32_t>(
                      EchoAIManagerOHOS::kMaxContextSizeInTokens))
            : nullptr);
    return;
  }

  for (const auto& prompt : prompts) {
    prompt_history_.push_back(prompt.Clone());
  }

  base::Value::List prompts_list;
  for (const auto& prompt : prompt_history_) {
    base::Value::Dict dict_prompt;
    dict_prompt.Set("role", ConvertRole(prompt->role));
    dict_prompt.Set("content", ConvertContent(prompt->content, uuid_));
    dict_prompt.Set("prefix", prompt->is_prefix);
    prompts_list.Append(std::move(dict_prompt));
  }

  prompt_history_.clear();
  current_tokens_ += new_tokens;

  std::string input_json;
  base::JSONWriter::Write(prompts_list, &input_json);
  EchoAIBaseOHOS::OnExecuteAIAction(input_json, std::move(pending_responder));
}

void EchoAILanguageModelOHOS::Append(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
      std::move(pending_responder));

  size_t new_tokens = 0;
  blink::mojom::ModelStreamingResponseStatus error_status;
  if (!ValidatePromptInput(prompts, error_status, new_tokens)) {
    responder->OnError(
        error_status,
        error_status == blink::mojom::ModelStreamingResponseStatus::
                            kErrorInputTooLarge
            ? blink::mojom::QuotaErrorInfo::New(
                  static_cast<uint32_t>(new_tokens),
                  static_cast<uint32_t>(
                      EchoAIManagerOHOS::kMaxContextSizeInTokens))
            : nullptr);
    return;
  }

  for (const auto& prompt : prompts) {
    prompt_history_.push_back(prompt.Clone());
  }
  current_tokens_ += new_tokens;
  responder->OnCompletion(
      blink::mojom::ModelExecutionContextInfo::New(current_tokens_));
}

void EchoAILanguageModelOHOS::Fork(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;
  mojo::PendingReceiver<blink::mojom::AILanguageModel> pending_receiver =
      language_model.InitWithNewPipeAndPassReceiver();

  // This sessions initial prompts + history is copied into the
  // forked session as initial prompts.
  std::vector<blink::mojom::AILanguageModelPromptPtr> prompts_copy;
  for (const auto& prompt : initial_prompts_) {
    prompts_copy.push_back(prompt.Clone());
  }
  for (const auto& prompt : prompt_history_) {
    prompts_copy.push_back(prompt.Clone());
  }
  std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_inputs_copy;
  for (const auto& expected : expected_inputs_) {
    expected_inputs_copy.push_back(expected.Clone());
  }
  std::vector<blink::mojom::AILanguageModelExpectedPtr> expected_outputs_copy;
  for (const auto& expected : expected_outputs_) {
    expected_outputs_copy.push_back(expected.Clone());
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<EchoAILanguageModelOHOS>(
          web_contents(),
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
                        EchoAIManagerOHOS::kMaxContextSizeInTokens,
                        initial_input_usage, std::move(model_sampling_params),
                        base::ToVector(enabled_input_types)));
              },
              std::move(client_remote), std::move(language_model),
              sampling_params_.Clone(), input_types_, current_tokens_),
          sampling_params_.Clone(), input_types_,
          std::move(expected_inputs_copy), std::move(expected_outputs_copy),
          std::move(prompts_copy), current_tokens_),
      std::move(pending_receiver));
}

void EchoAILanguageModelOHOS::Destroy() {
  is_destroyed_ = true;

  for (auto& responder : responder_set_) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*quota_error_info=*/nullptr);
  }
  responder_set_.Clear();
}

void EchoAILanguageModelOHOS::MeasureInputUsage(
    std::vector<blink::mojom::AILanguageModelPromptPtr> input,
    MeasureInputUsageCallback callback) {
  size_t total = 0;
  for (const auto& prompt : input) {
    for (const auto& content : prompt->content) {
      if (content->is_text()) {
        total += content->get_text().size();
      } else {
        total += 100;  // TODO(crbug.com/415304330): Improve estimate.
      }
    }
  }
  std::move(callback).Run(total);
}

bool EchoAILanguageModelOHOS::ValidatePromptInput(
    const std::vector<blink::mojom::AILanguageModelPromptPtr>& prompts,
    blink::mojom::ModelStreamingResponseStatus& error_status,
    size_t& new_tokens) {
  new_tokens = 0;
  for (const auto& prompt : prompts) {
    for (const auto& content : prompt->content) {
      auto type =
          content->is_text()
              ? blink::mojom::AILanguageModelPromptType::kText
              : content->is_bitmap()
                    ? blink::mojom::AILanguageModelPromptType::kImage
                    : content->is_audio()
                          ? blink::mojom::AILanguageModelPromptType::kAudio
                          : blink::mojom::AILanguageModelPromptType::kText;
      if (!input_types_.contains(type)) {
        error_status =
            blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure;
        return false;
      }
      new_tokens += content->is_text() ? content->get_text().size() : 100;
    }
  }
  if (current_tokens_ + new_tokens >
      static_cast<uint64_t>(EchoAIManagerOHOS::kMaxContextSizeInTokens)) {
    error_status =
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge;
    return false;
  }
  return true;
}

}  // namespace content
