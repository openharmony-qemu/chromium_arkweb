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

export class ExternalLLMMessage {
  role: string = '';
  content: string | ExternalLLMContentPart[] = '';
}

export class ExternalLLMContentPart {
  type: string = '';
  text?: string = '';
  image_url?: ExternalLLMImageUrl = new ExternalLLMImageUrl();
}

export class ExternalLLMImageUrl {
  url: string = '';
}

export class ExternalLLMRequest {
  messages: ExternalLLMMessage[] = [];
  sceneId: string = '';
  stream: boolean = false;
  temperature?: number;
  max_tokens?: number;
  response_format?: ExternalLLMResponseFormat;
  enable_thinking?: boolean;
  uris: string[] = [];
}

export class ExternalLLMResponseFormat {
  type: string = '';
}

export class ExternalLLMChoice {
  finish_reason: string = '';
  index: number = 0;
  message: ExternalLLMResponseMessage = new ExternalLLMResponseMessage();
}

export class ExternalLLMResponseMessage {
  role: string = '';
  content: string = '';
}

export class ExternalLLMUsage {
  prompt_tokens: number = 0;
  completion_tokens: number = 0;
  total_tokens: number = 0;
  prompt_cache_hit_tokens: number = 0;
  prompt_cache_miss_tokens: number = 0;
}

export class ExternalLLMResponse {
  choices: ExternalLLMChoice[] = [];
  model: string = '';
  usage: ExternalLLMUsage = new ExternalLLMUsage();
  errorCode: string = '';
  errorMessage: string = '';
}
