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

export class BrowserBinaryPayload {
  public mimeType: string = 'application/octet-stream';
  public base64Data: string = '';
}

export class BrowserBinaryPayloadService {
  private static readonly BASE64_ALPHABET: string =
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

  static parseBase64Payload(rawValue: string, fallbackMimeType: string = 'application/octet-stream'): BrowserBinaryPayload {
    const payload = new BrowserBinaryPayload();
    payload.mimeType = fallbackMimeType;
    const trimmed = rawValue.trim();
    if (trimmed.indexOf('data:') === 0) {
      const commaIndex = trimmed.indexOf(',');
      const metaPart = commaIndex > 0 ? trimmed.substring(5, commaIndex) : `${fallbackMimeType};base64`;
      const semicolonIndex = metaPart.indexOf(';');
      payload.mimeType = semicolonIndex > 0 ? metaPart.substring(0, semicolonIndex) : metaPart;
      payload.base64Data = commaIndex > 0 ? trimmed.substring(commaIndex + 1) : '';
      return payload;
    }
    payload.base64Data = trimmed;
    return payload;
  }

  static decodeBase64(base64Data: string): Uint8Array {
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
      const value = BrowserBinaryPayloadService.BASE64_ALPHABET.indexOf(charCode);
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

  static resolveFileExtension(mimeType: string, fallbackExtension: string = 'bin'): string {
    const normalized = mimeType.trim().toLowerCase();
    if (normalized === 'image/png') {
      return 'png';
    }
    if (normalized === 'image/jpeg' || normalized === 'image/jpg') {
      return 'jpg';
    }
    if (normalized === 'image/webp') {
      return 'webp';
    }
    if (normalized === 'image/gif') {
      return 'gif';
    }
    if (normalized === 'application/json') {
      return 'json';
    }
    if (normalized === 'text/plain') {
      return 'txt';
    }
    return fallbackExtension;
  }
}
