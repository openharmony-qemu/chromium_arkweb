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

#include "translator_ohos.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace content {

TranslatorOHOS::TranslatorOHOS(WebContents* web_contents,
                               base::OnceCallback<void(bool)> callback,
                               blink::mojom::TranslatorCreateOptionsPtr options)
    : EchoAIBaseOHOS(web_contents,
                     std::move(callback),
                     AISessionType::TRANSLATOR) {
  base::Value::Dict dict;
  dict.Set("sourceLanguage", options->source_lang->code);
  dict.Set("targetLanguage", options->target_lang->code);
  std::string output;
  base::JSONWriter::Write(dict, &output);

  EchoAIBaseOHOS::OnCreateAISession(output);
}

void TranslatorOHOS::Translate(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  base::Value::Dict input_dict;
  input_dict.Set("input", input);
  std::string input_json;
  base::JSONWriter::Write(input_dict, &input_json);

  EchoAIBaseOHOS::OnExecuteAIAction(input_json, std::move(pending_responder));
}

void TranslatorOHOS::TranslateStreaming(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  Translate(input, std::move(pending_responder));
}

}  // namespace content
