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

#include "echo_ai_proofreader_ohos.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace content {

EchoAIProofreaderOHOS::EchoAIProofreaderOHOS(
    WebContents* web_contents,
    base::OnceCallback<void(bool)> callback,
    blink::mojom::AIProofreaderCreateOptionsPtr options)
    : EchoAIBaseOHOS(web_contents,
                     std::move(callback),
                     AISessionType::PROOFREADER) {
  base::Value::Dict dict;
  dict.Set("includeCorrectionTypes", options->include_correction_types);
  dict.Set("includeCorrectionExplanations",
           options->include_correction_explanations);
  dict.Set("correctionExplanationLanguage",
           options->correction_explanation_language->code);
  base::Value::List languages;
  for (auto& expected_input_language : options->expected_input_languages) {
    languages.Append(expected_input_language->code);
  }
  dict.Set("expectedInputLanguages", std::move(languages));
  std::string output;
  base::JSONWriter::Write(dict, &output);

  EchoAIBaseOHOS::OnCreateAISession(output);
}

void EchoAIProofreaderOHOS::Proofread(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  base::Value::Dict input_dict;
  input_dict.Set("input", input);
  std::string input_json;
  base::JSONWriter::Write(input_dict, &input_json);

  EchoAIBaseOHOS::OnExecuteAIAction(input_json, std::move(pending_responder));
}

void EchoAIProofreaderOHOS::GetCorrectionType(
    const std::string& input,
    const std::string& corrected_input,
    const std::string& correction,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  base::Value::Dict input_dict;
  input_dict.Set("input", input);
  input_dict.Set("correctedInput", corrected_input);
  input_dict.Set("correction", correction);
  std::string input_json;
  base::JSONWriter::Write(input_dict, &input_json);

  EchoAIBaseOHOS::OnExecuteAIAction(input_json, std::move(pending_responder));
}

}  // namespace content
