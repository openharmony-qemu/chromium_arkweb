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

import { AgentLiteDomRect, AgentObservation } from '../core/AgentTypes';
import { AgentElementCropPort, BrowserAgentCropImageResult } from '../ports/AgentElementCropPort';
import { BrowserBinaryPayloadService } from '../../execution/BrowserBinaryPayloadService';
import { BrowserVisionCoordinateService, BrowserVisionImageSize } from '../../vision/BrowserVisionCoordinateService';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { image } from '@kit.ImageKit';

const DOMAIN = 0x6020;
const TAG = 'BrowserAgentCrop';
const MAX_RECTS_PER_ELEMENT = 1024;
/** base64 payload 最大允许字符串长度（10 MB），超出拒绝解码。 */
const MAX_BASE64_PAYLOAD_LENGTH = 10 * 1024 * 1024;
/** 源图片解码像素数上限（4096 × 4096），超出拒绝解码。 */
const MAX_SOURCE_IMAGE_PIXEL_COUNT = 4096 * 4096;
/** crop 输出像素数上限（4096 × 4096 = ~16M 像素），RGBA_8888 下约 64 MB。 */
const MAX_CROP_PIXEL_COUNT = 4096 * 4096;

class BrowserAgentCropBounds {
  public left: number = 0;
  public top: number = 0;
  public width: number = 0;
  public height: number = 0;
}

class BrowserAgentCropResult {
  public dataUrl: string = '';
  public mimeType: string = '';
}

export class BrowserElementCropService implements AgentElementCropPort {
  async cropByElementId(
    observation: AgentObservation | null,
    elementIdText: string
  ): Promise<BrowserAgentCropImageResult> {
    return this.cropByElementIdWithPadding(observation, elementIdText, 0);
  }

  async cropByElementIdWithPadding(
    observation: AgentObservation | null,
    elementIdText: string,
    paddingCssPx: number
  ): Promise<BrowserAgentCropImageResult> {
    const result = new BrowserAgentCropImageResult();
    result.elementId = elementIdText.trim();
    if (!observation) {
      result.errorMessage = 'observation unavailable';
      return result;
    }
    if (result.elementId.length <= 0) {
      result.errorMessage = 'elementId is empty';
      return result;
    }
    if (!BrowserElementCropService.isElementId(result.elementId)) {
      result.errorMessage = `invalid elementId: ${result.elementId}`;
      return result;
    }
    if (observation.originalScreenshot.trim().length <= 0) {
      result.errorMessage = 'originalScreenshot unavailable';
      return result;
    }
    if (observation.originalScreenshot.length > MAX_BASE64_PAYLOAD_LENGTH) {
      result.errorMessage =
        `base64 payload too large: ${observation.originalScreenshot.length} characters (max ${MAX_BASE64_PAYLOAD_LENGTH})`;
      return result;
    }
    if (observation.viewport.width <= 0 || observation.viewport.height <= 0) {
      result.errorMessage = 'viewport unavailable';
      return result;
    }
    const elementIndex = Number(result.elementId);
    if (elementIndex < 0 || elementIndex >= observation.liteDomTree.length) {
      result.errorMessage = `elementId out of range: ${result.elementId}`;
      return result;
    }
    const node = observation.liteDomTree[elementIndex];
    const cssBounds = BrowserElementCropService.resolveCssBounds(
      node.rects,
      observation.viewport.width,
      observation.viewport.height
    );
    if (!cssBounds) {
      result.errorMessage = `element ${result.elementId} has no visible rects`;
      return result;
    }
    const paddedCssBounds = BrowserElementCropService.applyCssPadding(
      cssBounds,
      paddingCssPx,
      observation.viewport.width,
      observation.viewport.height
    );
    const imageSize = BrowserVisionCoordinateService.resolveImageSize(observation.originalScreenshot);
    if (!imageSize) {
      result.errorMessage = 'unable to resolve screenshot size';
      return result;
    }
    if (!BrowserElementCropService.isSourceImageSizeValid(imageSize.width, imageSize.height)) {
      result.errorMessage =
        `source image too large or invalid: ${imageSize.width}x${imageSize.height} pixels ` +
        `(max ${MAX_SOURCE_IMAGE_PIXEL_COUNT})`;
      return result;
    }
    const pixelBounds = BrowserElementCropService.resolvePixelBounds(
      paddedCssBounds,
      observation.viewport.width,
      observation.viewport.height,
      imageSize
    );
    if (!pixelBounds) {
      result.errorMessage = 'invalid crop bounds';
      return result;
    }
    try {
      const cropResult = await BrowserElementCropService.cropImageRegion(
        observation.originalScreenshot,
        pixelBounds
      );
      result.success = cropResult.dataUrl.length > 0;
      result.dataUrl = cropResult.dataUrl;
      result.width = pixelBounds.width;
      result.height = pixelBounds.height;
      result.mimeType = cropResult.mimeType;
      if (!result.success) {
        result.errorMessage = 'crop image failed';
      }
      return result;
    } catch (error) {
      result.errorMessage = `${error}`;
      hilog.warn(DOMAIN, TAG, 'crop element image failed elementId=%{public}s error=%{public}s', result.elementId, result.errorMessage);
      return result;
    }
  }

  private static isElementId(value: string): boolean {
    return /^\d+$/.test(value);
  }

  private static isSourceImageSizeValid(width: number, height: number): boolean {
    return Number.isFinite(width) &&
      Number.isFinite(height) &&
      width > 0 &&
      height > 0 &&
      width * height <= MAX_SOURCE_IMAGE_PIXEL_COUNT;
  }

  private static resolveCssBounds(
    rects: AgentLiteDomRect[],
    viewportWidth: number,
    viewportHeight: number
  ): BrowserAgentCropBounds | null {
    if (rects.length <= 0 || viewportWidth <= 0 || viewportHeight <= 0) {
      return null;
    }
    const rectLimit = Math.min(rects.length, MAX_RECTS_PER_ELEMENT);
    if (rectLimit < rects.length) {
      hilog.warn(DOMAIN, TAG,
        'element rects exceeded cap, truncating: %{public}d -> %{public}d',
        rects.length, rectLimit);
    }
    let minLeft = Number.POSITIVE_INFINITY;
    let minTop = Number.POSITIVE_INFINITY;
    let maxRight = Number.NEGATIVE_INFINITY;
    let maxBottom = Number.NEGATIVE_INFINITY;
    for (let i = 0; i < rectLimit; i++) {
      const rect = rects[i];
      const left = Number.isFinite(rect.left) ? rect.left : 0;
      const top = Number.isFinite(rect.top) ? rect.top : 0;
      const right = BrowserElementCropService.resolveRectRight(rect, left);
      const bottom = BrowserElementCropService.resolveRectBottom(rect, top);
      minLeft = Math.min(minLeft, left, right);
      minTop = Math.min(minTop, top, bottom);
      maxRight = Math.max(maxRight, left, right);
      maxBottom = Math.max(maxBottom, top, bottom);
    }
    const clampedLeft = BrowserElementCropService.clamp(Math.floor(minLeft), 0, viewportWidth);
    const clampedTop = BrowserElementCropService.clamp(Math.floor(minTop), 0, viewportHeight);
    const clampedRight = BrowserElementCropService.clamp(Math.ceil(maxRight), 0, viewportWidth);
    const clampedBottom = BrowserElementCropService.clamp(Math.ceil(maxBottom), 0, viewportHeight);
    if (clampedRight <= clampedLeft || clampedBottom <= clampedTop) {
      return null;
    }
    const bounds = new BrowserAgentCropBounds();
    bounds.left = clampedLeft;
    bounds.top = clampedTop;
    bounds.width = clampedRight - clampedLeft;
    bounds.height = clampedBottom - clampedTop;
    return bounds;
  }

  private static applyCssPadding(
    bounds: BrowserAgentCropBounds,
    paddingCssPx: number,
    viewportWidth: number,
    viewportHeight: number
  ): BrowserAgentCropBounds {
    const padding = Number.isFinite(paddingCssPx) && paddingCssPx > 0 ? Math.floor(paddingCssPx) : 0;
    if (padding <= 0) {
      return bounds;
    }
    const left = BrowserElementCropService.clamp(bounds.left - padding, 0, viewportWidth);
    const top = BrowserElementCropService.clamp(bounds.top - padding, 0, viewportHeight);
    const right = BrowserElementCropService.clamp(bounds.left + bounds.width + padding, 0, viewportWidth);
    const bottom = BrowserElementCropService.clamp(bounds.top + bounds.height + padding, 0, viewportHeight);
    const padded = new BrowserAgentCropBounds();
    padded.left = left;
    padded.top = top;
    padded.width = Math.max(1, right - left);
    padded.height = Math.max(1, bottom - top);
    return padded;
  }

  private static resolveRectRight(
    rect: AgentLiteDomRect,
    fallbackLeft: number
  ): number {
    if (Number.isFinite(rect.right) && rect.right > fallbackLeft) {
      return rect.right;
    }
    if (Number.isFinite(rect.width) && rect.width > 0) {
      return fallbackLeft + rect.width;
    }
    return fallbackLeft;
  }

  private static resolveRectBottom(
    rect: AgentLiteDomRect,
    fallbackTop: number
  ): number {
    if (Number.isFinite(rect.bottom) && rect.bottom > fallbackTop) {
      return rect.bottom;
    }
    if (Number.isFinite(rect.height) && rect.height > 0) {
      return fallbackTop + rect.height;
    }
    return fallbackTop;
  }

  private static resolvePixelBounds(
    cssBounds: BrowserAgentCropBounds,
    viewportWidth: number,
    viewportHeight: number,
    imageSize: BrowserVisionImageSize
  ): BrowserAgentCropBounds | null {
    if (viewportWidth <= 0 || viewportHeight <= 0 || imageSize.width <= 0 || imageSize.height <= 0) {
      return null;
    }
    const scaleX = imageSize.width / viewportWidth;
    const scaleY = imageSize.height / viewportHeight;
    const left = BrowserElementCropService.clamp(Math.floor(cssBounds.left * scaleX), 0, imageSize.width);
    const top = BrowserElementCropService.clamp(Math.floor(cssBounds.top * scaleY), 0, imageSize.height);
    const right = BrowserElementCropService.clamp(
      Math.ceil((cssBounds.left + cssBounds.width) * scaleX),
      0,
      imageSize.width
    );
    const bottom = BrowserElementCropService.clamp(
      Math.ceil((cssBounds.top + cssBounds.height) * scaleY),
      0,
      imageSize.height
    );
    if (right <= left || bottom <= top) {
      return null;
    }
    const bounds = new BrowserAgentCropBounds();
    bounds.left = left;
    bounds.top = top;
    bounds.width = right - left;
    bounds.height = bottom - top;
    return bounds;
  }

  private static async cropImageRegion(
    rawImage: string,
    bounds: BrowserAgentCropBounds
  ): Promise<BrowserAgentCropResult> {
    const mimeType = BrowserElementCropService.resolveOutputMimeType(rawImage);
    const payload = BrowserBinaryPayloadService.parseBase64Payload(rawImage, 'image/png');

    if (payload.base64Data.length > MAX_BASE64_PAYLOAD_LENGTH) {
      throw new Error(
        `base64 payload too large: ${payload.base64Data.length} characters (max ${MAX_BASE64_PAYLOAD_LENGTH})`
      );
    }

    const bytes = BrowserBinaryPayloadService.decodeBase64(payload.base64Data);
    const sourceBuffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    let imageSource: image.ImageSource | null = null;
    let pixelMap: image.PixelMap | null = null;
    let packer: image.ImagePacker | null = null;
    try {
      imageSource = image.createImageSource(sourceBuffer);

      const imageInfo = await imageSource.getImageInfo();
      const sourceWidth = imageInfo.size.width;
      const sourceHeight = imageInfo.size.height;

      if (!BrowserElementCropService.isSourceImageSizeValid(sourceWidth, sourceHeight)) {
        throw new Error(
          `source image too large or invalid: ${sourceWidth}x${sourceHeight} pixels (max ${MAX_SOURCE_IMAGE_PIXEL_COUNT})`
        );
      }

      if (
        !Number.isFinite(bounds.left) ||
        !Number.isFinite(bounds.top) ||
        !Number.isFinite(bounds.width) ||
        !Number.isFinite(bounds.height) ||
        bounds.left < 0 ||
        bounds.top < 0 ||
        bounds.width <= 0 ||
        bounds.height <= 0
      ) {
        throw new Error(
          `crop region invalid: ${bounds.left},${bounds.top},${bounds.width},${bounds.height}`
        );
      }

      const cropLeft = Math.floor(bounds.left);
      const cropTop = Math.floor(bounds.top);
      const cropRight = Math.ceil(bounds.left + bounds.width);
      const cropBottom = Math.ceil(bounds.top + bounds.height);
      const cropWidth = cropRight - cropLeft;
      const cropHeight = cropBottom - cropTop;

      if (cropWidth * cropHeight > MAX_CROP_PIXEL_COUNT) {
        throw new Error(
          `crop region too large: ${cropWidth}x${cropHeight} pixels (max ${MAX_CROP_PIXEL_COUNT})`
        );
      }

      if (cropRight > sourceWidth || cropBottom > sourceHeight) {
        throw new Error(
          `crop region out of source image bounds: ${cropLeft},${cropTop},${cropWidth},${cropHeight}; source=${sourceWidth}x${sourceHeight}`
        );
      }

      const region: image.Region = {
        x: cropLeft,
        y: cropTop,
        size: {
          width: cropWidth,
          height: cropHeight
        }
      };
      const decodingOptions: image.DecodingOptions = {
        desiredRegion: region,
        desiredPixelFormat: image.PixelMapFormat.RGBA_8888
      };
      pixelMap = await imageSource.createPixelMap(decodingOptions);
      packer = image.createImagePacker();
      const packOptions: image.PackingOption = {
        format: mimeType,
        quality: 100
      };
      const packedBuffer = await packer.packToData(pixelMap, packOptions);
      const base64Data = BrowserElementCropService.encodeBase64(packedBuffer);
      const result = new BrowserAgentCropResult();
      result.dataUrl = `data:${mimeType};base64,${base64Data}`;
      result.mimeType = mimeType;
      return result;
    } finally {
      try {
        if (pixelMap) {
          await pixelMap.release();
        }
      } finally {
        try {
          if (packer) {
            await packer.release();
          }
        } finally {
          if (imageSource) {
            await imageSource.release();
          }
        }
      }
    }
  }

  private static resolveOutputMimeType(rawImage: string): string {
    const payload = BrowserBinaryPayloadService.parseBase64Payload(rawImage, 'image/png');
    const mimeType = payload.mimeType.trim().toLowerCase();
    if (mimeType === 'image/png' || mimeType === 'image/jpeg' || mimeType === 'image/webp') {
      return mimeType;
    }
    return 'image/png';
  }

  private static encodeBase64(buffer: ArrayBuffer): string {
    const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    const bytes = new Uint8Array(buffer);
    let output = '';
    for (let i = 0; i < bytes.length; i += 3) {
      const a = bytes[i];
      const b = i + 1 < bytes.length ? bytes[i + 1] : 0;
      const c = i + 2 < bytes.length ? bytes[i + 2] : 0;
      const triple = (a << 16) | (b << 8) | c;
      output += BrowserElementCropService.readBase64Char(alphabet, (triple >> 18) & 0x3f);
      output += BrowserElementCropService.readBase64Char(alphabet, (triple >> 12) & 0x3f);
      output += i + 1 < bytes.length ? BrowserElementCropService.readBase64Char(alphabet, (triple >> 6) & 0x3f) : '=';
      output += i + 2 < bytes.length ? BrowserElementCropService.readBase64Char(alphabet, triple & 0x3f) : '=';
    }
    return output;
  }

  private static readBase64Char(alphabet: string, index: number): string {
    return alphabet.substring(index, index + 1);
  }

  private static clamp(value: number, min: number, max: number): number {
    if (value < min) {
      return min;
    }
    if (value > max) {
      return max;
    }
    return value;
  }
}
