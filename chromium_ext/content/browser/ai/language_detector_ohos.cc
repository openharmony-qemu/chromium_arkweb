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

#include "language_detector_ohos.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace content {

LanguageDetectorOHOS::LanguageDetectorOHOS(
    WebContents* web_contents,
    base::OnceCallback<void(bool)> callback)
    : EchoAIBaseOHOS(web_contents,
                     std::move(callback),
                     AISessionType::LANGUAGE_DETECTOR) {
  EchoAIBaseOHOS::OnCreateAISession("");
}

void LanguageDetectorOHOS::Detect(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  base::Value::Dict input_dict;
  input_dict.Set("input", input);
  std::string input_json;
  base::JSONWriter::Write(input_dict, &input_json);

  EchoAIBaseOHOS::OnExecuteAIAction(input_json, std::move(pending_responder));
}

}  // namespace content
