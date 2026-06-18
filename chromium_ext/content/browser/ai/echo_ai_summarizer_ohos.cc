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

#include "echo_ai_summarizer_ohos.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace content {

EchoAISummarizerOHOS::EchoAISummarizerOHOS(
    WebContents* web_contents,
    base::OnceCallback<void(bool)> callback,
    blink::mojom::AISummarizerCreateOptionsPtr options)
    : EchoAIBaseOHOS(web_contents,
                     std::move(callback),
                     AISessionType::SUMMARIZER) {
  base::Value::Dict dict;
  if (options->shared_context) {
    dict.Set("sharedContext", options->shared_context.value());
  } else {
    dict.Set("sharedContext", base::Value());
  }
  switch (options->type) {
    case blink::mojom::AISummarizerType::kTLDR:
      dict.Set("type", "tldr");
      break;
    case blink::mojom::AISummarizerType::kKeyPoints:
      dict.Set("type", "key-points");
      break;
    case blink::mojom::AISummarizerType::kTeaser:
      dict.Set("type", "teaser");
      break;
    case blink::mojom::AISummarizerType::kHeadline:
      dict.Set("type", "headline");
      break;
    default:
      NOTREACHED();
  }

  switch (options->format) {
    case blink::mojom::AISummarizerFormat::kPlainText:
      dict.Set("format", "plain-text");
      break;
    case blink::mojom::AISummarizerFormat::kMarkDown:
      dict.Set("format", "markdown");
      break;
    default:
      NOTREACHED();
  }

  switch (options->length) {
    case blink::mojom::AISummarizerLength::kShort:
      dict.Set("length", "short");
      break;
    case blink::mojom::AISummarizerLength::kMedium:
      dict.Set("length", "medium");
      break;
    case blink::mojom::AISummarizerLength::kLong:
      dict.Set("length", "long");
      break;
    default:
      NOTREACHED();
  }
  base::Value::List languages;
  for (auto& expected_input_language : options->expected_input_languages) {
    languages.Append(expected_input_language->code);
  }
  dict.Set("expectedInputLanguages", std::move(languages));
  for (auto& expected_context_languages : options->expected_context_languages) {
    languages.Append(expected_context_languages->code);
  }
  dict.Set("expectedContextLanguages", std::move(languages));
  dict.Set("outputLanguage", options->output_language->code);
  std::string output;
  base::JSONWriter::Write(dict, &output);

  EchoAIBaseOHOS::OnCreateAISession(output);
}

void EchoAISummarizerOHOS::Summarize(
    const std::string& input,
    const std::string& context,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  base::Value::Dict dict;
  dict.Set("input", input);
  base::Value::Dict options_dict;
  options_dict.Set("context", context);
  dict.Set("options", std::move(options_dict));
  std::string input_json;
  base::JSONWriter::Write(dict, &input_json);

  EchoAIBaseOHOS::OnExecuteAIAction(input_json, std::move(pending_responder));
}

void EchoAISummarizerOHOS::MeasureUsage(const std::string& input,
                                        const std::string& context,
                                        MeasureUsageCallback callback) {
  std::move(callback).Run(input.size() + context.size());
}

}  // namespace content
