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

// 这个文件负责 browser 功能 中 Browser Vision Coordinate Service 的核心服务。
import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN = 0x6020;
const TAG = 'BrowserVisionCoord';

export class BrowserVisionImageSize {
  public width: number = 0;
  public height: number = 0;
}

export class BrowserVisionCoordinatePoint {
  public x: number = 0;
  public y: number = 0;
}

class BrowserVisionImagePayload {
  public mimeType: string = 'image/png';
  public base64Data: string = '';
}

export class BrowserVisionCoordinateService {
  private static readonly NORMALIZED_RANGE: number = 1000;
  private static readonly BASE64_ALPHABET: string =
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

  static convertQwenNormalizedCoordinate(rawX: number, rawY: number, rawImage: string): BrowserVisionCoordinatePoint {
    const imageSize = BrowserVisionCoordinateService.resolveImageSize(rawImage);
    if (!imageSize) {
      const fallback = new BrowserVisionCoordinatePoint();
      fallback.x = Math.max(0, Math.round(rawX));
      fallback.y = Math.max(0, Math.round(rawY));
      hilog.warn(DOMAIN, TAG,
        'vision coordinate fallback rawX=%{public}d rawY=%{public}d reason=image_size_missing',
        fallback.x,
        fallback.y);
      return fallback;
    }

    const point = new BrowserVisionCoordinatePoint();
    if (BrowserVisionCoordinateService.isNormalizedCoordinate(rawX, rawY)) {
      point.x = BrowserVisionCoordinateService.clamp(
        Math.round(rawX * imageSize.width / BrowserVisionCoordinateService.NORMALIZED_RANGE),
        0,
        Math.max(0, imageSize.width - 1)
      );
      point.y = BrowserVisionCoordinateService.clamp(
        Math.round(rawY * imageSize.height / BrowserVisionCoordinateService.NORMALIZED_RANGE),
        0,
        Math.max(0, imageSize.height - 1)
      );
    } else {
      point.x = BrowserVisionCoordinateService.clamp(Math.round(rawX), 0, Math.max(0, imageSize.width - 1));
      point.y = BrowserVisionCoordinateService.clamp(Math.round(rawY), 0, Math.max(0, imageSize.height - 1));
    }
    hilog.info(DOMAIN, TAG,
      'vision coordinate converted raw=(%{public}d,%{public}d) actual=(%{public}d,%{public}d) image=%{public}dx%{public}d normalized=%{public}s',
      Math.round(rawX),
      Math.round(rawY),
      point.x,
      point.y,
      imageSize.width,
      imageSize.height,
      BrowserVisionCoordinateService.isNormalizedCoordinate(rawX, rawY) ? 'true' : 'false');
    return point;
  }

  static resolveImageSize(rawImage: string): BrowserVisionImageSize | null {
    const payload = BrowserVisionCoordinateService.parseImagePayload(rawImage);
    if (payload.base64Data.length <= 0) {
      return null;
    }
    const bytes = BrowserVisionCoordinateService.decodeBase64(payload.base64Data);
    if (bytes.length <= 0) {
      return null;
    }
    const pngSize = BrowserVisionCoordinateService.readPngSize(bytes);
    if (pngSize) {
      return pngSize;
    }
    const jpegSize = BrowserVisionCoordinateService.readJpegSize(bytes);
    if (jpegSize) {
      return jpegSize;
    }
    hilog.warn(DOMAIN, TAG, 'vision image size unsupported mime=%{public}s bytes=%{public}d', payload.mimeType, bytes.length);
    return null;
  }

  private static isNormalizedCoordinate(rawX: number, rawY: number): boolean {
    return rawX >= 0 && rawY >= 0 &&
      rawX <= BrowserVisionCoordinateService.NORMALIZED_RANGE &&
      rawY <= BrowserVisionCoordinateService.NORMALIZED_RANGE;
  }

  private static parseImagePayload(rawImage: string): BrowserVisionImagePayload {
    const payload = new BrowserVisionImagePayload();
    const trimmed = rawImage.trim();
    if (trimmed.indexOf('data:') === 0) {
      const commaIndex = trimmed.indexOf(',');
      const metaPart = commaIndex > 0 ? trimmed.substring(5, commaIndex) : 'image/png;base64';
      const semicolonIndex = metaPart.indexOf(';');
      payload.mimeType = semicolonIndex > 0 ? metaPart.substring(0, semicolonIndex) : metaPart;
      payload.base64Data = commaIndex > 0 ? trimmed.substring(commaIndex + 1) : '';
      return payload;
    }
    payload.base64Data = trimmed;
    return payload;
  }

  private static decodeBase64(base64Data: string): Uint8Array {
    const sanitized = base64Data.replace(/\s+/g, '');
    let padding = 0;
    if (sanitized.endsWith('==')) {
      padding = 2;
    } else if (sanitized.endsWith('=')) {
      padding = 1;
    }
    const outputLength = Math.max(0, Math.floor(sanitized.length * 3 / 4) - padding);
    const bytes = new Uint8Array(outputLength);
    let buffer = 0;
    let bitCount = 0;
    let offset = 0;
    for (let i = 0; i < sanitized.length; i++) {
      const charCode = sanitized.charAt(i);
      if (charCode === '=') {
        break;
      }
      const value = BrowserVisionCoordinateService.BASE64_ALPHABET.indexOf(charCode);
      if (value < 0) {
        continue;
      }
      buffer = (buffer << 6) | value;
      bitCount += 6;
      if (bitCount >= 8) {
        bitCount -= 8;
        if (offset < outputLength) {
          bytes[offset] = (buffer >> bitCount) & 0xff;
          offset += 1;
        }
      }
    }
    return bytes;
  }

  private static readPngSize(bytes: Uint8Array): BrowserVisionImageSize | null {
    if (bytes.length < 24) {
      return null;
    }
    if (bytes[0] !== 0x89 || bytes[1] !== 0x50 || bytes[2] !== 0x4e || bytes[3] !== 0x47 ||
      bytes[4] !== 0x0d || bytes[5] !== 0x0a || bytes[6] !== 0x1a || bytes[7] !== 0x0a) {
      return null;
    }
    const size = new BrowserVisionImageSize();
    size.width = BrowserVisionCoordinateService.readUint32BE(bytes, 16);
    size.height = BrowserVisionCoordinateService.readUint32BE(bytes, 20);
    return size.width > 0 && size.height > 0 ? size : null;
  }

  private static readJpegSize(bytes: Uint8Array): BrowserVisionImageSize | null {
    if (bytes.length < 4 || bytes[0] !== 0xff || bytes[1] !== 0xd8) {
      return null;
    }
    let offset = 2;
    while (offset + 8 < bytes.length) {
      while (offset < bytes.length && bytes[offset] !== 0xff) {
        offset += 1;
      }
      while (offset < bytes.length && bytes[offset] === 0xff) {
        offset += 1;
      }
      if (offset >= bytes.length) {
        break;
      }
      const marker = bytes[offset];
      offset += 1;
      if (marker === 0xd9 || marker === 0xda) {
        break;
      }
      if (offset + 1 >= bytes.length) {
        break;
      }
      const segmentLength = BrowserVisionCoordinateService.readUint16BE(bytes, offset);
      if (segmentLength < 2 || offset + segmentLength > bytes.length) {
        break;
      }
      if (BrowserVisionCoordinateService.isStartOfFrameMarker(marker)) {
        const size = new BrowserVisionImageSize();
        size.height = BrowserVisionCoordinateService.readUint16BE(bytes, offset + 3);
        size.width = BrowserVisionCoordinateService.readUint16BE(bytes, offset + 5);
        return size.width > 0 && size.height > 0 ? size : null;
      }
      offset += segmentLength;
    }
    return null;
  }

  private static isStartOfFrameMarker(marker: number): boolean {
    return marker === 0xc0 || marker === 0xc1 || marker === 0xc2 || marker === 0xc3 ||
      marker === 0xc5 || marker === 0xc6 || marker === 0xc7 ||
      marker === 0xc9 || marker === 0xca || marker === 0xcb ||
      marker === 0xcd || marker === 0xce || marker === 0xcf;
  }

  private static readUint16BE(bytes: Uint8Array, offset: number): number {
    return ((bytes[offset] & 0xff) << 8) | (bytes[offset + 1] & 0xff);
  }

  private static readUint32BE(bytes: Uint8Array, offset: number): number {
    return ((bytes[offset] & 0xff) * 0x1000000) +
      ((bytes[offset + 1] & 0xff) << 16) +
      ((bytes[offset + 2] & 0xff) << 8) +
      (bytes[offset + 3] & 0xff);
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
