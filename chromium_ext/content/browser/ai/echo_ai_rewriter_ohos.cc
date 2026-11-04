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

#include "echo_ai_rewriter_ohos.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace content {

EchoAIRewriterOHOS::EchoAIRewriterOHOS(
    WebContents* web_contents,
    base::OnceCallback<void(bool)> callback,
    blink::mojom::AIRewriterCreateOptionsPtr options)
    : EchoAIBaseOHOS(web_contents,
                     std::move(callback),
                     AISessionType::REWRITER) {
  base::Value::Dict dict;
  if (options->shared_context) {
    dict.Set("sharedContext", options->shared_context.value());
  } else {
    dict.Set("sharedContext", base::Value());
  }
  switch (options->tone) {
    case blink::mojom::AIRewriterTone::kAsIs:
      dict.Set("tone", "as-is");
      break;
    case blink::mojom::AIRewriterTone::kMoreFormal:
      dict.Set("tone", "more-formal");
      break;
    case blink::mojom::AIRewriterTone::kMoreCasual:
      dict.Set("tone", "more-casual");
      break;
    default:
      NOTREACHED();
  }

  switch (options->format) {
    case blink::mojom::AIRewriterFormat::kAsIs:
      dict.Set("format", "as-is");
      break;
    case blink::mojom::AIRewriterFormat::kPlainText:
      dict.Set("format", "plain-text");
      break;
    case blink::mojom::AIRewriterFormat::kMarkdown:
      dict.Set("format", "markdown");
      break;
    default:
      NOTREACHED();
  }

  switch (options->length) {
    case blink::mojom::AIRewriterLength::kAsIs:
      dict.Set("length", "as-is");
      break;
    case blink::mojom::AIRewriterLength::kShorter:
      dict.Set("length", "shorter");
      break;
    case blink::mojom::AIRewriterLength::kLonger:
      dict.Set("length", "longer");
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

void EchoAIRewriterOHOS::Rewrite(
    const std::string& input,
    const std::optional<std::string>& context,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  base::Value::Dict dict;
  dict.Set("input", input);
  if (context) {
    base::Value::Dict options_dict;
    options_dict.Set("context", *context);
    dict.Set("options", std::move(options_dict));
  }
  std::string input_json;
  base::JSONWriter::Write(dict, &input_json);

  EchoAIBaseOHOS::OnExecuteAIAction(input_json, std::move(pending_responder));
}

void EchoAIRewriterOHOS::MeasureUsage(const std::string& input,
                                      const std::string& context,
                                      MeasureUsageCallback callback) {
  std::move(callback).Run(input.size() + context.size());
}

}  // namespace content
