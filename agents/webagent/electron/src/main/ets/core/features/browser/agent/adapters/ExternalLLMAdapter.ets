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

import { AgentLLMPort } from '../ports/AgentLLMPort';
import { AgentLLMRequest, AgentLLMResponse, BrowserLLMUsageRecord, BrowserPromptConstants, AgentConstants } from '../core/AgentTypes';
import {
  ExternalLLMRequest,
  ExternalLLMResponse,
  ExternalLLMMessage,
  ExternalLLMContentPart,
  ExternalLLMImageUrl,
  ExternalLLMResponseFormat
} from '../core/ExternalLLMTypes';
import { AgentLLMSceneConfigService } from '../core/AgentLLMSceneConfig';
import { BrowserEncryptedImageCacheService } from '../../execution/BrowserEncryptedImageCacheService';
import { BrowserSessionRecorderService } from '../../execution/BrowserSessionRecorderService';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { fileUri } from '@kit.CoreFileKit';
import BuildProfile from 'BuildProfile';

const DOMAIN = 0x6020;
const TAG = 'ExternalLLMAdapter';

export class ExternalLLMAdapter implements AgentLLMPort {
  private readonly callback: (request: ExternalLLMRequest) => Promise<ExternalLLMResponse>;
  private readonly screenshotCacheDir: string;
  private readonly stopChecker: () => boolean;
  private readonly sessionDir: string;
  private imageIndex: number = 0;
  private usageRecords: BrowserLLMUsageRecord[] = [];

  constructor(
    callback: (request: ExternalLLMRequest) => Promise<ExternalLLMResponse>,
    screenshotCacheDir: string,
    stopChecker: () => boolean,
    sessionDir: string = ''
  ) {
    this.callback = callback;
    this.screenshotCacheDir = screenshotCacheDir;
    this.stopChecker = stopChecker;
    this.sessionDir = sessionDir;
  }

  async complete(request: AgentLLMRequest, tag: string = ''): Promise<AgentLLMResponse> {
    const startTime = Date.now();
    let externalRequest: ExternalLLMRequest;
    try {
      externalRequest = await this.buildExternalRequest(request);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'Error building external LLM request: %{public}', e.message);
      throw new Error(e.message);
    }

    if (this.stopChecker()) {
      throw new Error('LLM aborted: task stopped');
    }

    let timeoutHandle: number = -1;
    let stopHandle: number = -1;
    try {
      const externalResponse = await Promise.race([
        this.callback(externalRequest),
        new Promise<never>((_, reject) => {
          timeoutHandle = setTimeout(() => reject(new Error(`LLM request timeout after ${AgentConstants.LLM_TIMEOUT_MS}ms`)), AgentConstants.LLM_TIMEOUT_MS);
        }),
        new Promise<never>((_, reject) => {
          stopHandle = setInterval(() => {
            if (this.stopChecker()) {
              reject(new Error('LLM aborted: task stopped'));
            }
          }, 200);
        })
      ]);
      const agentResponse = this.parseExternalResponse(externalResponse);

      const record = new BrowserLLMUsageRecord();
      record.tag = tag;
      record.promptTokens = agentResponse.promptTokens;
      record.completionTokens = agentResponse.completionTokens;
      record.totalTokens = agentResponse.totalTokens;
      record.promptCacheHitTokens = agentResponse.promptCacheHitTokens;
      record.promptCacheMissTokens = agentResponse.promptCacheMissTokens;
      record.durationMs = Date.now() - startTime;
      this.usageRecords.push(record);
      if (BuildProfile.DEBUG) {
        BrowserSessionRecorderService.recordLlmCall({
          sessionDir: this.sessionDir,
          request: request,
          response: agentResponse,
          tag: tag,
          scene: externalRequest.sceneId,
          model: externalResponse.model.length > 0 ? externalResponse.model : 'external',
          durationMs: Date.now() - startTime,
          externalRequest: externalRequest
        });
      }
      hilog.info(DOMAIN, TAG, 'llm complete %{public}s, time = %{public}d', tag, Date.now() - startTime);
      return agentResponse;
    } finally {
      if (timeoutHandle >= 0) {
        clearTimeout(timeoutHandle);
      }
      if (stopHandle >= 0) {
        clearInterval(stopHandle);
      }
    }
  }

  getUsage(): BrowserLLMUsageRecord[] {
    const records = this.usageRecords.slice();
    this.usageRecords = [];
    return records;
  }

  private async buildExternalRequest(request: AgentLLMRequest): Promise<ExternalLLMRequest> {
    const externalRequest = new ExternalLLMRequest();
    externalRequest.messages = [];
    externalRequest.sceneId = request.scene;
    externalRequest.uris = [];

    if (request.systemPrompt.length > 0) {
      const systemMessage = new ExternalLLMMessage();
      systemMessage.role = 'system';
      systemMessage.content = request.systemPrompt;
      externalRequest.messages.push(systemMessage);
    }

    if (request.messages.length > 0) {
      for (let i = 0; i < request.messages.length; i++) {
        const msg = request.messages[i];
        const externalMessage = new ExternalLLMMessage();
        externalMessage.role = msg.role;
        if (request.images.length > 0 && msg.role === 'user') {
          externalMessage.content = await this.buildUserContentWithImages(
            msg.content, request.images, externalRequest.uris);
        } else {
          externalMessage.content = msg.content;
        }
        externalRequest.messages.push(externalMessage);
      }
    } else if (request.userPrompt.length > 0 || request.images.length > 0) {
      const userMessage = new ExternalLLMMessage();
      userMessage.role = 'user';
      userMessage.content = await this.buildUserContentWithImages(
        request.userPrompt, request.images, externalRequest.uris);
      externalRequest.messages.push(userMessage);
    }

    const sceneConfig = AgentLLMSceneConfigService.get(request.scene);
    if (sceneConfig) {
      externalRequest.stream = sceneConfig.stream;
      externalRequest.temperature = sceneConfig.temperature;
      externalRequest.max_tokens = sceneConfig.maxTokens;
      externalRequest.enable_thinking = sceneConfig.enableThinking;
    }

    if (request.responseFormat.length > 0) {
      const format = new ExternalLLMResponseFormat();
      format.type = request.responseFormat;
      externalRequest.response_format = format;
    } else if (sceneConfig && sceneConfig.responseFormat.length > 0) {
      const format = new ExternalLLMResponseFormat();
      format.type = sceneConfig.responseFormat;
      externalRequest.response_format = format;
    }

    return externalRequest;
  }

  private async buildUserContentWithImages(
    prompt: string, images: string[], uris: string[]):
  Promise<ExternalLLMContentPart[] | string> {
    if (images.length === 0) {
      return prompt;
    }

    this.imageIndex = 0;
    const hasPlaceholders = prompt.includes(BrowserPromptConstants.IMAGE_PLACEHOLDER);
    if (!hasPlaceholders) {
      const parts: ExternalLLMContentPart[] = [];
      if (prompt.length > 0) {
        const textPart = new ExternalLLMContentPart();
        textPart.type = 'text';
        textPart.text = prompt;
        parts.push(textPart);
      }
      for (let i = 0; i < images.length; i++) {
        parts.push(await this.buildImagePart(images[i], uris));
      }
      return parts;
    }

    const segments = prompt.split(BrowserPromptConstants.IMAGE_PLACEHOLDER);
    const placeholderCount = segments.length - 1;
    if (placeholderCount !== images.length) {
      throw new Error(
        `Interweave prompt mismatch: found ${placeholderCount} image placeholders but ${images.length} images provided`
      );
    }

    const parts: ExternalLLMContentPart[] = [];
    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];
      if (segment.length > 0) {
        const textPart = new ExternalLLMContentPart();
        textPart.type = 'text';
        textPart.text = segment;
        parts.push(textPart);
      }
      if (i < images.length) {
        parts.push(await this.buildImagePart(images[i], uris));
      }
    }
    return parts;
  }

  private async buildImagePart(
    imageData: string, uris: string[]):
  Promise<ExternalLLMContentPart> {
    const imagePart = new ExternalLLMContentPart();
    imagePart.type = 'image_url';
    imagePart.image_url = new ExternalLLMImageUrl();

    let imageDataWithPrefix: string = imageData;
    if (!imageDataWithPrefix.startsWith('data')) {
      imageDataWithPrefix = 'data:image/png;base64,' + imageData;
    }
    if (this.screenshotCacheDir.length > 0) {
      const fileName = `image_${this.imageIndex}.enc`;
      const filePath = await BrowserEncryptedImageCacheService.writeDataUrl(
        this.screenshotCacheDir,
        fileName,
        imageDataWithPrefix
      );
      const fileUriValue = fileUri.getUriFromPath(filePath);
      imagePart.image_url.url = fileUriValue;
      uris.push(fileUriValue);
    } else {
      imagePart.image_url.url = imageData.startsWith('data:') ? imageData : `data:image/jpeg;base64,${imageData}`;
    }

    this.imageIndex += 1;
    return imagePart;
  }

  private parseExternalResponse(response: ExternalLLMResponse): AgentLLMResponse {
    const agentResponse = new AgentLLMResponse();
    if (typeof response === 'string') {
      throw new Error(response);
    }
    if (!response.choices) {
      throw new Error(`Call llm failed, the response does not have "choices" field`);
    }
    if (
      (response.errorCode && response.errorCode.length > 0) ||
      (response.errorMessage && response.errorMessage.length > 0)
    ) {
      throw new Error(`Call llm failed, code = ${response.errorCode}, message = ${response.errorMessage}`);
    }
    if (response.choices.length === 0) {
      throw new Error(`Call llm failed, the "choices" field of response is empty.`);
    }
    if (response.choices.length > 0) {
      const choice = response.choices[0];
      agentResponse.text = choice.message.content;
      agentResponse.rawText = JSON.stringify(response);
    }

    if (response.usage) {
      agentResponse.promptTokens = response.usage.prompt_tokens;
      agentResponse.completionTokens = response.usage.completion_tokens;
      agentResponse.totalTokens = response.usage.total_tokens;
      agentResponse.promptCacheHitTokens = response.usage.prompt_cache_hit_tokens;
      agentResponse.promptCacheMissTokens = response.usage.prompt_cache_miss_tokens;
    }

    return agentResponse;
  }
}
