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

import { AgentBuildConfig } from '../../core/AgentBuildConfig';
import {
  AgentLLMRequest,
  AgentNoteEntry,
  AgentObservation,
  AgentToolCall,
  AgentToolQuery,
  AgentToolResult,
  BrowserAgentAction,
  DetectionResult,
  AgentConstants
} from '../../core/AgentTypes';
import { AgentState } from '../../core/AgentState';
import { AgentLLMScenes } from '../../core/AgentLLMScenes';
import { AgentStateFactory } from '../../core/AgentStateFactory';
import { BrowserAgentTryLLM, BrowserAgentUtils } from '../../core/Utils';
import { BrowserAgentCropImageResult } from '../../ports/AgentElementCropPort';
import { AgentEnvironmentPort } from '../../ports/AgentEnvironmentPort';
import { AgentLLMPort } from '../../ports/AgentLLMPort';
import { AgentSiteTool } from '../../tools/AgentSiteTool';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { HtmlParser } from '../../core/HtmlParser';
import { BrowserPromptConstants } from '../../core/AgentTypes';
import Image from '@ohos.multimedia.image';
import util from '@ohos.util';
import { BrowserAgentGraphContext, AgentToolContext } from '../../ports/AgentContext';
import { BrowserAgentPromptDetectionService } from '../../core/PromptDetectionService';

const DOMAIN = 0x6020;
const TAG = 'BrowserAgentGraph';
const DEBUG_IMAGE_DIR = 'scroll_note/crops';
/** Outputimage format for scroll_note/crops */
const SCROLL_NOTE_IMAGE_FORMAT = 'image/jpeg';
const SCROLL_NOTE_IMAGE_QUALITY = 80;
/** 截图 base64 字符串最大允许长度（10 MB）。 */
const MAX_SCREENSHOT_BASE64_LENGTH = 10 * 1024 * 1024;
/** 单张截图解码像素数上限（4096 × 4096）。 */
const MAX_SCREENSHOT_PIXEL_COUNT = 4096 * 4096;
/** 本次处理所有截图的累计解码像素数上限。 */
const MAX_TOTAL_SCREENSHOT_PIXEL_COUNT = 4096 * 4096;


class ExtractionResponse {
  information: Record<string, string> = {};
  order: ExtractionOrder = new ExtractionOrder();
  progress: string = '';
  status: string = '';
}

class ExtractionOrder {
  judgement: string = '';
  confidence: string = '';
}

class ScrollAndNoteArgs {
  task: string = '';
  region: string = 'window';
}

class ScrollStepObservation {
  observation: AgentObservation;
  dom: string;

  constructor(observation: AgentObservation, dom: string) {
    this.observation = observation;
    this.dom = dom;
  }
}

class ScrollBatchCaptureResult {
  observation: AgentObservation;
  snapshots: ScrollStepObservation[] = [];
  stopReason: string = '';

  constructor(observation: AgentObservation) {
    this.observation = observation;
  }
}

class SharedDiffRegion {
  x: number = 0;
  y: number = 0;
  width: number = 0;
  height: number = 0;
}

export class ScrollAndNoteToolService extends AgentSiteTool {
  static readonly TOOL_ID: string = 'scrollAndNote';
  static readonly TOOL_NAME: string = 'scrollAndNote';
  static readonly TOOL_DESCRIPTION: string =
    `When you need to continue scrolling and collect information, use this tool.
    use "tool": {"toolId":"scrollAndNote","parameters":{"task":"what information to extract from the current page","region":"window or elementId"}}.
    The task should be specific (what to collect, how many entries needed, etc.)
    Specify the region to scroll. If the information you need is located within a scrollable element, you should specify that element instead of the whole window.
    Note: this tool will only perform scroll. If you need other actions (e.g., click into posts to view the content), do not use this tool.`;
  private static readonly STATUS_COMPLETE: string = 'COMPLETE';
  private static readonly STATUS_CONTINUE: string = 'CONTINUE';
  private static readonly STATUS_ABORT: string = 'ABORT';
  private static readonly STATUS_SCROLL_FAILED: string = 'SCROLL_FAIL';
  private static readonly DEFAULT_MAX_ROUNDS: number = 5;
  private static readonly SNAPSHOTS_PER_BATCH: number = 3;

  private static readonly SYSTEM_PROMPT:string = `
**Role**: You are a specialized *Information Extraction Module* within a browser agent. Your task is to extract **specific, non-redundant data** from webpage screenshots and their corresponding DOM trees, based on user goals and subtasks.
#### **Inputs**:
1. **User Goal**: the instruction of the user to the browser agent (e.g., *"Scrape 10 positive and 10 negative reviews for laptops within a week from Tmall"*)
2. **Task**: the task you were given by the browser agent. This is your primary objective (e.g., *"Extract 5 positive reviews within a week from this website"*)
3. **Task progress**: the progress of the Task summarized by yourself before the current step. (e.g., *"4/5 positive reviews collected, one being partial."*)
4. **Browser Agent Notes**: Data already collected by the browser agent. You **MUST NOT** output information in the Browser Agent Notes.
5. **Module History**: your task-specific history including:
  - Memory: Data collected by yourself previously during this task. Organized as a Dict with keys as entry ID starting from 1.
  - Order: Previous judgement of the page's content sorting pattern (unsorted, date descending, etc.)
6. **Screenshots & DOM**: Newly obtained visual and textual data of the current page / an scrollable element. There are multiple images and only one DOM. The DOM may contain more information than the screenshot as it has information outside the screen.

#### **Output**:
- **Information**: extracted information (a Dict, keys as serial number strings starting from 1, and values as plain text strings)
- **Progress**: Summarize your current progress of the Task (not the user goal; including the information you just output).
- **Order**: Judge the sorting state of items in the page based on previous judgement and new webpage(s), and your confidence of the judgement (low, medium, high). Judgement must include one of:
  - Not applicable: the Task does not require attribute range (e.g., date, price) or the page does not contain multiple items
  - Unsorted: The items are not sorted based on the attribute requirement of the Task.
  - Sorted: No evidence that suggests the items are not sorted. In this case, must also output the attribute and direction, and values of the items in this case.
- **Status Code**: Must include one of:
  - ${ScrollAndNoteToolService.STATUS_COMPLETE}: Already collected sufficient task-relevant info (e.g., 5/5 reviews collected).
  - ${ScrollAndNoteToolService.STATUS_CONTINUE}: Found partial info or scrolling would reveal needed info, need to scroll/load more content  (e.g., 2/5 reviews).
  - ${ScrollAndNoteToolService.STATUS_ABORT}: No or little task-relevant info found, and continue scolling does not seem promising.
  - ${ScrollAndNoteToolService.STATUS_SCROLL_FAILED}: The scroll failed to reveal more information (either task-relevant or non task-relevant) in your region of interest. This could be due to an execute error or a wrong scroll target being specified when you were called.

#### **Rules**:
1. **Avoid Deduplication**: Never output information already existing in Browser Agent Notes or your Memory. Only output NEW data to append to/replace the already existing data.
2. **Record Immediately**: When you encounter relevant information during browsing (even if incomplete), record it immediately. Mark incomplete entries with [Partial].
3. **Partial norm**: Mark content as [Partial] only when it's visibly truncated due to screen size constraints (e.g., cut off by viewport edges). Do NOT mark content as partial if it's intentionally collapsed by UI elements (e.g., 'Show more' buttons, dropdowns, expandable sections) — treat these as complete but hidden.
4. **Complete when possible**: When you see new information that can complete partial information in the Memory, output the new information (including the old) as a new entry with key "Replace <partial entry id>".
5. **Notice order if any**: When the items are sorted and you have go **beyond the task's required range**, you should abort. (e.g., requires collect reviews from March 2024 to April 2024, page sorted by date DESC, currently on February 2024, should abort; however, if currently on May 2024, should continue)
6. **Strict format**: Strictly follow the format of keys in output according to the rules and example.
7. **Judge if scroll failed**: If the multiple screenshots provided are identical (or identical in the part containing your desired information), it means the scroll failed. This might be due to the agent called you with a wrong parameter (e.g., should scroll an element but specified the window). Output ${ScrollAndNoteToolService.STATUS_SCROLL_FAILED} in such case, unless the task is already completed.

#### **Example Input**:
Task: Extract 5 positive reviews from Mar.13 to Mar.20 from this website.
Notes: (folded, already has 5 positive comments)
Memory: {
  "1": "Positive review 6: The screen is big enough for a movie, Mar 20".
  "2": "Positive review 7: This product is very thin, and the feel is so... [Partial]".
}
Order: {
  "judgement": "The task has a requirement on date. Sorted by date DESC. items: Mar 20, Mar 19, Mar 17",
  "confidence": "Medium as only 3 items have been observed"
}
(Other input information is omitted.)

#### **Example Output**:
{
  "information": {
    "Replace 2": "Positive review 7: This product is very thin, and the feel is so good. It also comes with a stand. Mar 16",
    "3": "Positive review 8: The battery is good. Mar 16",
    "4": "Positive review 9: The price ...[Partial].",
  },
  "order": {
    "judgement": "The task has a requirement on date. Items are sorted by date DESC. items: Mar 20, ... Mar 17, Mar 16, Mar 16",
    "confidence": "High",
  },
  "progress": "4/5 positive reviews collected, one being partial.",
  "status": "${ScrollAndNoteToolService.STATUS_CONTINUE}",
}
`;

  readonly toolId: string = ScrollAndNoteToolService.TOOL_ID;
  readonly displayName: string = ScrollAndNoteToolService.TOOL_NAME;
  readonly description: string = ScrollAndNoteToolService.TOOL_DESCRIPTION;
  readonly domain: string = 'all';

  private readonly memory: ExtractionResponse = new ExtractionResponse();
  private readonly base64Helper: util.Base64Helper = new util.Base64Helper();
  private nextId: number = 1;
  private url = '';

  supports(query: AgentToolQuery): boolean {
    void query;
    return true;
  }

  async invoke(call: AgentToolCall, state: AgentState, context: AgentToolContext): Promise<AgentToolResult> {
    const result = new AgentToolResult();
    result.handled = true;
    const args = this.parseArgs(call.inputText);
    if (args.task.length <= 0) {
      result.success = false;
      result.summary = 'scrollAndNote requires a non-empty task parameter.';
      result.returnValue = result.summary;
      return result;
    }
    let regionSelector = '';
    if (args.region.trim().toLowerCase() !== 'window') {
      if (!state.observation) {
        result.success = false;
        result.summary = 'scrollAndNote skipped: observation unavailable.';
        result.returnValue = result.summary;
        return result;
      }
      regionSelector = this.resolveRegionSelector(state.observation, args.region);
      if (regionSelector.length <= 0) {
        result.success = false;
        result.summary = `scrollAndNote requires region to be "window" or a valid elementId.`;
        result.returnValue = result.summary;
        return result;
      }
    }
    this.url = state.observation?.currentUrl; //防止中途用户接管导致网页变化
    this.resetMemory();
    try {
      return await this.scrollAndNote(args.task, regionSelector, state, context.env, context.llm, context);
    } catch (error) {
      result.success = false;
      result.summary = `scrollAndNote failed: ${error.message}`;
      return result;
    }
  }

  processResponse(responseJson: ExtractionResponse): void {
    // Process the LLM response and update the memory accordingly.
    // Args:
    // response_json (dict): The JSON response from the LLM containing:
    //   - "information": dict of new information to add/replace
    const newInfo = responseJson.information;
    const keys = Object.keys(newInfo);
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      if (key.startsWith('Replace ')) {
        this.replaceMemoryEntry(key, newInfo[key]);

      } else {
        this.appendMemoryEntry(key, newInfo[key]);
      }
    }
    this.memory.order = responseJson.order;
    this.memory.progress = responseJson.progress;
    this.memory.status = responseJson.status;
  }

  buildRequest(
    instruction: string,
    task: string,
    notes: string,
    images: string[],
    dom: string
  ): AgentLLMRequest {
    const request = new AgentLLMRequest();
    request.systemPrompt = ScrollAndNoteToolService.SYSTEM_PROMPT;
    const promptParts: string[] = [
      '## User goal:',
      instruction,
      '## Task:',
      task,
      '## Task progress:',
      this.memory.progress.length > 0 ? this.memory.progress : 'None',
      '## Browser Agent Notes:',
      notes,
      '## Memory (information previously extracted by yourself in this task):',
      JSON.stringify(this.memory.information, null, 2),
      '## Previous judged order:',
      JSON.stringify(this.memory.order, null, 2),
      '## Webpage screenshots for this batch:',
      `${BrowserPromptConstants.IMAGE_PLACEHOLDER} `.repeat(images.length),
      '## DOM of the last screenshot in this batch:',
      dom.length > 0 ? dom : 'None'
    ];
    request.userPrompt = promptParts.join('\n');
    request.images = images;
    request.scene = request.images.length > 0 ?
      AgentLLMScenes.BROWSER_DECISION_VISION : AgentLLMScenes.BROWSER_DECISION;
    BrowserAgentUtils.debugLongText(DOMAIN, TAG, 'information extractor user prompt=', request.userPrompt);
    return request;
  }

  private readonly scrollAndNote = async (
    task: string,
    regionSelector: string,
    state: AgentState,
    env: AgentEnvironmentPort,
    llm: AgentLLMPort,
    context: AgentToolContext
  ): Promise<AgentToolResult> => {
    //  当前处理response时出错易任务崩掉，需catch住。增加多次滚动+单步收集。筛除相容的dom和图片。
    const nextResult = new AgentToolResult();
    nextResult.handled = true;
    if (!state.observation) {
      nextResult.success = false;
      nextResult.summary = 'scrollAndNote skipped: observation unavailable.';
      nextResult.returnValue = nextResult.summary;
      return nextResult;
    }
    let observation = AgentStateFactory.cloneObservation(state.observation);
    let stopReason: string = '';
    const maxRounds = state.stepBudget > ScrollAndNoteToolService.DEFAULT_MAX_ROUNDS ? ScrollAndNoteToolService.DEFAULT_MAX_ROUNDS : 1;
    let totalRounds = 0;
    while (totalRounds < maxRounds) {
      const batchResult = await this.captureSnapshotBatch(
        observation,
        regionSelector,
        context,
        Math.min(ScrollAndNoteToolService.SNAPSHOTS_PER_BATCH, maxRounds - totalRounds)
      );
      observation = batchResult.observation;
      stopReason = batchResult.stopReason;
      if (batchResult.snapshots.length <= 0) {
        break;
      }

      totalRounds += batchResult.snapshots.length;
      let llmImages: string[] = [];
      try {
        llmImages = await this.resolveImagesForSnapshots(batchResult.snapshots, regionSelector, context);
      } catch (error) {
        hilog.warn(
          DOMAIN,
          TAG,
          'resolveImagesForSnapshots failed, abort scroll-and-note batch: %{public}s',
          error instanceof Error ? error.message : String(error)
        );
        break;
      }
      const treeText = this.formatSnapshotDom(batchResult.snapshots);
      const request = this.buildRequest(
        state.task,
        task,
        this.formatGlobalNotes(state.note),
        llmImages,
        treeText
      );
      const promptDetectionText = BrowserAgentPromptDetectionService.build(
        treeText, batchResult.observation);
      const checkPrompt = context.events.checkPrompt;

      const promptCheckPromise = checkPrompt ?
        BrowserAgentUtils.base64ToPixelMaps(request.images).then(pixelMaps =>
        checkPrompt(pixelMaps, promptDetectionText)
        ).catch((error: unknown): Error => {
          const err = error instanceof Error ? error : new Error(String(error));
          hilog.error(DOMAIN, TAG, 'checkPrompt failed: %{private}s', err.message);
          return err;
        }) :
        Promise.resolve<DetectionResult | Error | null>(null);

      const tryLLM = new BrowserAgentTryLLM<ExtractionResponse>(
        async (nextRequest: AgentLLMRequest) => {
          return await llm.complete(nextRequest);
        },
        (response) => {
          const content = response.text.length > 0 ? response.text : response.rawText;
          BrowserAgentUtils.debugLongText(DOMAIN, TAG, 'information extractor response', content);
          return this.parseResponse(content);
        },
      );
      const responseJson = await tryLLM.run(
        request,
        (_nextRequest, _reasonResponse, errorMessage) => {
          hilog.warn(DOMAIN, TAG, 'information extractor parse result error %{private}s', errorMessage);
        }
      );

      const promptCheckResult = await promptCheckPromise;
      if (promptCheckResult && !(promptCheckResult instanceof Error) && promptCheckResult.riskCode === AgentConstants.INJECTED) {
        const stopReason = '安全模块在tool执行过程中在网页检测到疑似提示注入，因此tool被终止。';
        const toolResult = this.buildToolResult(state.note, totalRounds, stopReason);
        toolResult.success = false;
        toolResult.summary = stopReason;
        return toolResult;
      }

      this.processResponse(responseJson);
      if (this.memory.status !== ScrollAndNoteToolService.STATUS_CONTINUE || stopReason.length > 0) {
        break;
      }
    }
    return this.buildToolResult(state.note, totalRounds, stopReason);
  }

  private async captureSnapshotBatch(
    observation: AgentObservation,
    regionSelector: string,
    context: AgentToolContext,
    maxCaptures: number
  ): Promise<ScrollBatchCaptureResult> {
    const result = new ScrollBatchCaptureResult(AgentStateFactory.cloneObservation(observation));
    for (let round = 0; round < maxCaptures; round++) {
      const action = this.buildScrollAction(result.observation, regionSelector);
      if (!action) {
        result.stopReason = regionSelector.length > 0 ?
          'The scroll and note tool stopped because the target scroll region is no longer available.' :
          '';
        break;
      }
      context.events.onScreenshot?.('', 0, '向下滚动');
      await BrowserAgentUtils.contextInteract(context, () => context.env.execute(action));
      let userControlled: boolean;
      [result.observation, userControlled] = await BrowserAgentUtils.contextInteract(context, () => context.env.perceive());
      result.snapshots.push(this.buildSnapshotObservation(result.observation));
    }
    return result;
  }

  private buildSnapshotObservation(observation: AgentObservation): ScrollStepObservation {
    const parser = new HtmlParser(observation.html, false, [], ['a']);
    const textTree = parser.printWholeTree({
      includeNoneText: true,
      includeHref: false
    });
    return new ScrollStepObservation(AgentStateFactory.cloneObservation(observation), textTree);
  }

  private replaceMemoryEntry(key: string, value: string): void {
    const parts = key.split(' ');
    if (parts.length <= 1) {
      throw new Error(`Invalid key in information: ${key}`);
    }
    const id = Number(parts[1]);
    if (Number.isNaN(id) || !this.memory.information[String(id)]) {
      throw new Error(`Found key '${key}' but cannot find entry with key '${parts[1]}' in Memory`);
    }
    this.memory.information[String(id)] = value;
  }

  private appendMemoryEntry(key: string, value: string): void {
    const id = Number(key);
    if (Number.isNaN(id)) {
      throw new Error(`Found invalid key '${key}'`);
    }
    if (id !== this.nextId) {
      if (this.nextId === 1) {
        this.nextId = id;
      } else {
        throw new Error(`Found invalid key '${key}', should be sequential`);
      }
    }
    this.memory.information[String(id)] = value;
    this.nextId += 1;
  }

  private parseResponse(content: string): ExtractionResponse {
    const normalized = this.extractObjectText(this.stripCodeFence(content));
    const parsed = JSON.parse(normalized) as Record<string, Object>;
    const response = new ExtractionResponse();
    response.information = this.readStringMap(parsed, 'information');
    response.order = this.readOrder(parsed.order);
    response.progress = this.readString(parsed, 'progress');
    response.status = this.readString(parsed, 'status').toUpperCase();
    if (!this.isSupportedStatus(response.status)) {
      throw new Error(`Invalid status in extraction response: ${response.status}`);
    }
    return response;
  }

  private readStringMap(source: Record<string, Object>, key: string): Record<string, string> {
    const result: Record<string, string> = {};
    const value = source[key];
    if (typeof value !== 'object' || value === null || Array.isArray(value)) {
      return result;
    }
    const record = value as Record<string, Object>;
    const keys = Object.keys(record);
    for (let i = 0; i < keys.length; i++) {
      const entryValue = record[keys[i]];
      result[keys[i]] = typeof entryValue === 'string' ? entryValue : '';
    }
    return result;
  }

  private readOrder(value: Object): ExtractionOrder {
    const order = new ExtractionOrder();
    if (typeof value !== 'object' || value === null || Array.isArray(value)) {
      return order;
    }
    const record = value as Record<string, Object>;
    order.judgement = this.readString(record, 'judgement');
    order.confidence = this.readString(record, 'confidence');
    return order;
  }

  private readString(source: Record<string, Object>, key: string): string {
    const value = source[key];
    return typeof value === 'string' ? value.trim() : '';
  }

  private isSupportedStatus(status: string): boolean {
    return status === ScrollAndNoteToolService.STATUS_COMPLETE ||
      status === ScrollAndNoteToolService.STATUS_CONTINUE ||
      status === ScrollAndNoteToolService.STATUS_ABORT ||
      status === ScrollAndNoteToolService.STATUS_SCROLL_FAILED;
  }

  private formatGlobalNotes(notes: AgentNoteEntry[]): string {
    const values: string[] = [];
    for (let i = 0; i < notes.length; i++) {
      values.push(
        `{"webpage": ${JSON.stringify(notes[i].webpageTitle)}, "information": ${JSON.stringify(notes[i].information)}}`
      );
    }
    return values.length > 0 ? `[${values.join(', ')}]` : '[]';
  }

  private async resolveImagesForSnapshots(
    snapshots: ScrollStepObservation[],
    regionSelector: string,
    context: AgentToolContext
  ): Promise<string[]> {
    if (snapshots.length <= 0) {
      return [];
    }
    if (regionSelector.length > 0) {
      const images: string[] = [];
      for (let i = 0; i < snapshots.length; i++) {
        const roundImages = await this.resolveImages(
          snapshots[i].observation,
          regionSelector,
          i,
          context
        );
        for (let j = 0; j < roundImages.length; j++) {
          images.push(roundImages[j]);
        }
      }
      return images;
    }
    return await this.cropImagesBySharedDifference(snapshots, context);
  }

  private formatSnapshotDom(snapshots: ScrollStepObservation[]): string {
    const sections: string[] = [];
    sections.push(
      `### DOM of the last screenshot:`,
      snapshots[snapshots.length - 1].dom.length > 0 ? snapshots[snapshots.length - 1].dom : 'None'
    );
    return sections.join('\n');
  }

  private async cropImagesBySharedDifference(
    snapshots: ScrollStepObservation[],
    context: AgentToolContext
  ): Promise<string[]> {
    const screenshots: string[] = [];
    for (let i = 0; i < snapshots.length; i++) {
      const screenshot = snapshots[i].observation.originalScreenshot;
      if (screenshot.length > MAX_SCREENSHOT_BASE64_LENGTH) {
        throw new Error(
          `screenshot base64 too large: ${screenshot.length} characters (max ${MAX_SCREENSHOT_BASE64_LENGTH})`
        );
      }
      if (screenshot.length > 0) {
        screenshots.push(screenshot);
      }
    }
    if (screenshots.length <= 0) {
      return [];
    }
    const imageSources: Image.ImageSource[] = [];
    const pixelMaps: Image.PixelMap[] = [];
    try {
      let totalPixelCount = 0;
      for (let i = 0; i < screenshots.length; i++) {
        const imageSource = Image.createImageSource(this.decodeScreenshotBuffer(screenshots[i]));
        imageSources.push(imageSource);

        const imageInfo = await imageSource.getImageInfo();
        const sourceWidth = imageInfo.size.width;
        const sourceHeight = imageInfo.size.height;

        if (
          !Number.isFinite(sourceWidth) ||
          !Number.isFinite(sourceHeight) ||
          sourceWidth <= 0 ||
          sourceHeight <= 0
        ) {
          throw new Error(
            `screenshot invalid: ${sourceWidth}x${sourceHeight} pixels`
          );
        }

        const pixelCount = sourceWidth * sourceHeight;
        if (pixelCount > MAX_SCREENSHOT_PIXEL_COUNT) {
          throw new Error(
            `screenshot too large: ${sourceWidth}x${sourceHeight} pixels (max ${MAX_SCREENSHOT_PIXEL_COUNT})`
          );
        }

        totalPixelCount += pixelCount;
        if (totalPixelCount > MAX_TOTAL_SCREENSHOT_PIXEL_COUNT) {
          throw new Error(
            `total screenshot pixels too large: ${totalPixelCount} (max ${MAX_TOTAL_SCREENSHOT_PIXEL_COUNT})`
          );
        }
      }

      for (let i = 0; i < imageSources.length; i++) {
        const pixelMap = await imageSources[i].createPixelMap({
          desiredPixelFormat: Image.PixelMapFormat.RGBA_8888
        });
        pixelMaps.push(pixelMap);
      }
      const cropRegion = this.resolveSharedDiffRegion(pixelMaps);
      const croppedImages: string[] = [];
      for (let i = 0; i < pixelMaps.length; i++) {
        await pixelMaps[i].crop({
          x: cropRegion.x,
          y: cropRegion.y,
          size: {
            width: cropRegion.width,
            height: cropRegion.height
          }
        });
        const imagePacker = Image.createImagePacker();
        try {
          const packedBuffer = await imagePacker.packToData(pixelMaps[i], {
            format: SCROLL_NOTE_IMAGE_FORMAT,
            quality: SCROLL_NOTE_IMAGE_QUALITY,
          });
          await this.saveDebugImage(packedBuffer, i + 1, SCROLL_NOTE_IMAGE_FORMAT, context);
          croppedImages.push(this.toDataUrl(packedBuffer, SCROLL_NOTE_IMAGE_FORMAT));
        } finally {
          await imagePacker.release();
        }
      }
      return croppedImages;
    } finally {
      for (let i = 0; i < pixelMaps.length; i++) {
        await pixelMaps[i].release();
      }
      for (let i = 0; i < imageSources.length; i++) {
        await imageSources[i].release();
      }
    }
  }

  private resolveSharedDiffRegion(pixelMaps: Image.PixelMap[]): SharedDiffRegion {
    const region = new SharedDiffRegion();
    if (pixelMaps.length <= 0) {
      return region;
    }
    const imageInfos = pixelMaps.map((pixelMap) => pixelMap.getImageInfoSync());
    const width = imageInfos[0].size.width;
    const height = imageInfos[0].size.height;
    region.width = width;
    region.height = height;
    for (let i = 1; i < imageInfos.length; i++) {
      if (imageInfos[i].size.width !== width ||
        imageInfos[i].size.height !== height ||
        imageInfos[i].stride !== imageInfos[0].stride) {
        return region;
      }
    }
    const rowBytes = pixelMaps[0].getBytesNumberPerRow();
    const pixelBytes = pixelMaps.map((pixelMap) => {
      const buffer = new ArrayBuffer(pixelMap.getPixelBytesNumber());
      pixelMap.readPixelsToBufferSync(buffer);
      return buffer;
    });
    const bytesPerPixel = this.resolveBytesPerPixel(rowBytes, width);
    let minX = width;
    let minY = height;
    let maxX = -1;
    let maxY = -1;
    const byteViews = pixelBytes.map((buffer) => new Uint8Array(buffer));
    const wordViews = bytesPerPixel === 4 && rowBytes % 4 === 0 ?
      pixelBytes.map((buffer) => new Uint32Array(buffer)) :
      [];
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const offset = y * rowBytes + x * bytesPerPixel;
        if (this.hasPixelDifferenceAt(byteViews, wordViews, offset, bytesPerPixel)) {
          minX = Math.min(minX, x);
          minY = Math.min(minY, y);
          maxX = Math.max(maxX, x);
          maxY = Math.max(maxY, y);
        }
      }
    }
    if (maxX < 0 || maxY < 0) {
      return region;
    }
    region.x = minX;
    region.y = minY;
    region.width = maxX - minX + 1;
    region.height = maxY - minY + 1;
    return region;
  }

  private hasPixelDifferenceAt(
    pixelBytes: Uint8Array[],
    pixelWords: Uint32Array[],
    offset: number,
    bytesPerPixel: number
  ): boolean {
    if (pixelWords.length === pixelBytes.length && bytesPerPixel === 4) {
      const wordIndex = offset / 4;
      const basePixel = pixelWords[0][wordIndex];
      for (let imageIndex = 1; imageIndex < pixelWords.length; imageIndex++) {
        if (pixelWords[imageIndex][wordIndex] !== basePixel) {
          return true;
        }
      }
      return false;
    }
    for (let imageIndex = 1; imageIndex < pixelBytes.length; imageIndex++) {
      for (let channelIndex = 0; channelIndex < bytesPerPixel; channelIndex++) {
        if (pixelBytes[imageIndex][offset + channelIndex] !== pixelBytes[0][offset + channelIndex]) {
          return true;
        }
      }
    }
    return false;
  }

  private resolveBytesPerPixel(rowBytes: number, width: number): number {
    if (width <= 0 || rowBytes <= 0) {
      throw new Error('Unable to crop screenshots because the image size is invalid.');
    }
    if (rowBytes % width !== 0) {
      throw new Error('Unable to crop screenshots because the decoded pixel buffer is invalid.');
    }
    const bytesPerPixel = rowBytes / width;
    if (bytesPerPixel <= 0) {
      throw new Error('Unable to crop screenshots because the decoded pixel format is invalid.');
    }
    return bytesPerPixel;
  }

  private decodeScreenshotBuffer(screenshot: string): ArrayBuffer {
    const base64Text = screenshot.startsWith('data:') ?
      screenshot.substring(screenshot.indexOf(',') + 1) :
      screenshot;
    const decoded = this.base64Helper.decodeSync(base64Text);
    const bytes = new Uint8Array(decoded.length);
    bytes.set(decoded);
    return bytes.buffer;
  }

  private toDataUrl(buffer: ArrayBuffer, mimeType: string): string {
    const encoded = this.base64Helper.encodeToStringSync(new Uint8Array(buffer));
    return `data:${mimeType};base64,${encoded}`;
  }

  private async saveDebugImage(
    buffer: ArrayBuffer,
    index: number,
    mimeType: string,
    context: AgentToolContext
  ): Promise<void> {
    if (AgentBuildConfig.DEBUG) {
      const extension = this.resolveDebugImageExtension(mimeType);
      await this.writeDebugImage(this.toDataUrl(buffer, mimeType), index, extension, mimeType, context);
    }
  }

  private resolveDebugImageExtension(mimeType: string): string {
    const normalized = mimeType.trim().toLowerCase();
    if (normalized === 'image/jpeg' || normalized === 'image/jpg') {
      return 'jpg';
    }
    if (normalized === 'image/webp') {
      return 'webp';
    }
    if (normalized === 'image/gif') {
      return 'gif';
    }
    return 'png';
  }

  private async writeDebugImage(
    rawValue: string,
    index: number,
    extension: string,
    mimeType: string,
    context: AgentToolContext
  ): Promise<void> {
    const relativePath = `${DEBUG_IMAGE_DIR}/scroll_note_crop_${Date.now()}_${index}.${extension}`;
    await context.workspace.writeBase64File(relativePath, rawValue, mimeType);
  }
  
  private async resolveImages(
    observation: AgentObservation,
    regionSelector: string,
    round: number,
    context: AgentToolContext
  ): Promise<string[]> {
    const images: string[] = [];
    if (regionSelector.length > 0) {
      const regionElementId = this.resolveCurrentRegionElementId(observation, regionSelector);
      if (regionElementId.length > 0) {
        const cropResult = await context.cropper.cropByElementId(observation, regionElementId);
        const croppedImage = await this.persistCropIfAvailable(cropResult, round, context);
        if (croppedImage.length > 0) {
          images.push(croppedImage);
          return images;
        }
      }
    }
    if (observation.originalScreenshot.length > 0) {
      images.push(observation.originalScreenshot);
    }
    return images;
  }

  private async persistCropIfAvailable(
    cropResult: BrowserAgentCropImageResult,
    round: number,
    context: AgentToolContext
  ): Promise<string> {
    if (!cropResult.success || cropResult.dataUrl.length <= 0) {
      return '';
    }
    const safeElementId = cropResult.elementId.length > 0 ? cropResult.elementId : 'unknown';
    const relativePath = `${DEBUG_IMAGE_DIR}/round-${(round + 1).toString().padStart(4, '0')}-element-${safeElementId}.png`;
    if (AgentBuildConfig.DEBUG) {
      try {
        await context.workspace.writeBase64File(relativePath, cropResult.dataUrl, cropResult.mimeType);
      } catch (error) {
        hilog.warn(DOMAIN, TAG, 'scroll and note persist crop failed path=%{public}s error=%{public}s',
          relativePath,
          `${error}`);
      }
    }
    return cropResult.dataUrl;
  }

  private buildToolResult(existingNotes: AgentNoteEntry[], maxRounds: number, stopReason: string): AgentToolResult {
    const result = new AgentToolResult();
    result.handled = true;
    result.success = true;
    result.note = this.buildNoteText(existingNotes);
    result.returnValue = this.buildReturnValue(result.note, maxRounds, stopReason);
    result.summary = this.memory.progress.length > 0 ? this.memory.progress : this.buildStatusText(maxRounds, stopReason);
    return result;
  }

  private buildNoteText(existingNotes: AgentNoteEntry[]): string {
    const keys = Object.keys(this.memory.information);
    const values: string[] = [];
    for (let i = 0; i < keys.length; i++) {
      const information = this.memory.information[keys[i]];
      if (information.length <= 0 || this.hasExistingNote(existingNotes, information)) {
        continue;
      }
      values.push(information);
    }
    return values.join('\n');
  }

  private buildReturnValue(noteText: string, maxRounds: number, stopReason: string): string {
    const noteValue = noteText.length > 0 ? noteText : 'None';
    return `${this.buildStatusText(maxRounds, stopReason)}. \n Its summary of progress was: ` +
      `${this.memory.progress.length > 0 ? this.memory.progress : 'None'}. \n ` +
      `The following information was retrieved and has been written in the notes: ${noteValue}`;
  }

  private buildStatusText(maxRounds: number, stopReason: string): string {
    if (this.memory.status === ScrollAndNoteToolService.STATUS_COMPLETE) {
      return 'The scroll and note tool judged itself as having completed the task';
    }
    if (this.memory.status === ScrollAndNoteToolService.STATUS_ABORT) {
      return 'The scroll and note tool aborted the task as continual scrolling would not reveal sufficient information in the current page';
    }
    if (this.memory.status === ScrollAndNoteToolService.STATUS_SCROLL_FAILED) {
      return 'The scroll and note tool aborted the task as the scrolling did not changed its region of interest. This might be because you specified a wrong scroll target.';
    }
    if (stopReason.length > 0) {
      return stopReason;
    }
    if (this.memory.status === ScrollAndNoteToolService.STATUS_CONTINUE) {
      return `The scroll and note tool was terminated after reaching the max scroll limit (${maxRounds})`;
    }
    return 'The scroll and note tool finished without a final status';
  }

  private hasExistingNote(notes: AgentNoteEntry[], information: string): boolean {
    for (let i = 0; i < notes.length; i++) {
      if (notes[i].information === information) {
        return true;
      }
    }
    return false;
  }

  private buildScrollAction(observation: AgentObservation, regionSelector: string): BrowserAgentAction | null {
    const action = new BrowserAgentAction();
    if (regionSelector.length <= 0) {
      if (observation.currentUrl !== this.url) {
        return null;
      }
      action.action = 'SCROLL [DOWN]';
      action.actionType = 'SCROLL';
      action.params = ['DOWN'];
      action.requiresLoadingWait = true;
      return action;
    }
    const regionElementId = this.resolveCurrentRegionElementId(observation, regionSelector);
    if (regionElementId.length <= 0) {
      return null;
    }
    action.action = `SCROLL [DOWN] [${regionElementId}]`;
    action.actionType = 'SCROLL';
    action.element = Number(regionElementId);
    action.params = ['DOWN'];
    action.requiresLoadingWait = true;
    return action;
  }

  private stripCodeFence(content: string): string {
    let normalized = content.trim();
    if (!normalized.startsWith('```')) {
      return normalized;
    }
    const firstLineEnd = normalized.indexOf('\n');
    if (firstLineEnd >= 0) {
      normalized = normalized.substring(firstLineEnd + 1);
    }
    if (normalized.endsWith('```')) {
      normalized = normalized.substring(0, normalized.length - 3);
    }
    return normalized.trim();
  }

  private extractObjectText(content: string): string {
    const start = content.indexOf('{');
    if (start < 0) {
      return content;
    }
    let depth = 0;
    let inString = false;
    let escaped = false;
    for (let i = start; i < content.length; i++) {
      const currentChar = content.charAt(i);
      if (inString) {
        if (escaped) {
          escaped = false;
          continue;
        }
        if (currentChar === '\\') {
          escaped = true;
          continue;
        }
        if (currentChar === '"') {
          inString = false;
        }
        continue;
      }
      if (currentChar === '"') {
        inString = true;
        continue;
      }
      if (currentChar === '{') {
        depth += 1;
      } else if (currentChar === '}') {
        depth -= 1;
        if (depth === 0) {
          return content.substring(start, i + 1);
        }
      }
    }
    return content.substring(start);
  }

  private resetMemory(): void {
    this.memory.information = {};
    this.memory.order = new ExtractionOrder();
    this.memory.progress = '';
    this.memory.status = '';
    this.nextId = 1;
  }

  private parseArgs(inputText: string): ScrollAndNoteArgs {
    const args = new ScrollAndNoteArgs();
    const normalized = inputText.trim();
    if (normalized.length <= 0) {
      return args;
    }
    try {
      const parsed = JSON.parse(normalized) as Record<string, Object>;
      args.task = this.readString(parsed, 'task');
      const region = this.readString(parsed, 'region');
      args.region = region.length > 0 ? region : 'window';
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'Scroll note parse args error: %{private}s, using default: window', e.message);
      args.task = normalized;
      args.region = 'window';
    }
    return args;
  }

  private resolveRegionSelector(observation: AgentObservation, region: string): string {
    const normalizedRegion = region.trim();
    if (normalizedRegion.length <= 0 || normalizedRegion.toLowerCase() === 'window') {
      return '';
    }
    if (!/^\d+$/.test(normalizedRegion)) {
      return '';
    }
    const regionIndex = Number(normalizedRegion);
    if (Number.isNaN(regionIndex) || regionIndex < 0 || regionIndex >= observation.liteDomTree.length) {
      return '';
    }
    const selector = observation.liteDomTree[regionIndex].selector.trim();
    return selector;
  }

  private resolveCurrentRegionElementId(observation: AgentObservation, regionSelector: string): string {
    for (let i = 0; i < observation.liteDomTree.length; i++) {
      if (observation.liteDomTree[i].selector.trim() === regionSelector &&
        this.isScrollableNode(observation, i)) {
        return `${i}`;
      }
    }
    return '';
  }

  private isScrollableNode(observation: AgentObservation, index: number): boolean {
    const node = observation.liteDomTree[index];
    const nodeType = node.type.trim().toLowerCase();
    const elementType = node.element.type.trim().toLowerCase();
    return nodeType === 'scrollable' || elementType === 'scrollable';
  }
}
