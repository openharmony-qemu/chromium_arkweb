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

import { util } from '@kit.ArkTS';
import {
  MemoryEncryptedEnvelope,
  MEMORY_CRYPTO_AAD_TEXT,
  MEMORY_CRYPTO_ALGORITHM,
  MEMORY_ENVELOPE_FORMAT,
  MEMORY_ENVELOPE_VERSION,
  MEMORY_GCM_NONCE_SIZE,
  MEMORY_GCM_TAG_SIZE,
  MemoryFileCryptoResult,
  MEMORY_KEY_VERSION,
  MEMORY_PLAINTEXT_SCHEMA_VERSION
} from './MemoryFileCryptoEnvelope';

const BASE64_PATTERN: RegExp = /^[A-Za-z0-9+/]*={0,2}$/;

export class MemoryFileCryptoCodec {
  private static readonly encoder = new util.TextEncoder('utf-8');
  private static readonly decoder = util.TextDecoder.create('utf-8');
  private static readonly base64Helper = new util.Base64Helper();

  static encodeEnvelope(result: MemoryFileCryptoResult): string {
    const envelope = new MemoryEncryptedEnvelope();
    envelope.nonce = MemoryFileCryptoCodec.encodeBase64(result.nonce);
    envelope.tag = MemoryFileCryptoCodec.encodeBase64(result.tag);
    envelope.ciphertext = MemoryFileCryptoCodec.encodeBase64(result.ciphertext);
    return JSON.stringify(envelope);
  }

  static decodeEnvelope(text: string, storageLimitBytes: number): MemoryFileCryptoResult {
    if (text.length <= 0 || MemoryFileCryptoCodec.utf8Encode(text).length > storageLimitBytes) {
      throw new Error('ENVELOPE_INVALID');
    }
    let parsed: Record<string, Object>;
    try {
      parsed = JSON.parse(text) as Record<string, Object>;
    } catch (_error) {
      throw new Error('ENVELOPE_INVALID');
    }
    if (parsed.format !== MEMORY_ENVELOPE_FORMAT ||
      parsed.version !== MEMORY_ENVELOPE_VERSION ||
      parsed.plaintextSchemaVersion !== MEMORY_PLAINTEXT_SCHEMA_VERSION ||
      parsed.algorithm !== MEMORY_CRYPTO_ALGORITHM ||
      parsed.keyVersion !== MEMORY_KEY_VERSION ||
      typeof parsed.nonce !== 'string' ||
      typeof parsed.tag !== 'string' ||
      typeof parsed.ciphertext !== 'string') {
      throw new Error('ENVELOPE_INVALID');
    }
    const nonce = MemoryFileCryptoCodec.decodeBase64Strict(parsed.nonce as string);
    const tag = MemoryFileCryptoCodec.decodeBase64Strict(parsed.tag as string);
    const ciphertext = MemoryFileCryptoCodec.decodeBase64Strict(parsed.ciphertext as string);
    if (nonce.length !== MEMORY_GCM_NONCE_SIZE ||
      tag.length !== MEMORY_GCM_TAG_SIZE ||
      ciphertext.length > storageLimitBytes) {
      throw new Error('ENVELOPE_INVALID');
    }
    const result = new MemoryFileCryptoResult();
    result.nonce = nonce;
    result.tag = tag;
    result.ciphertext = ciphertext;
    return result;
  }

  static isEncryptedEnvelopeText(text: string): boolean {
    try {
      const parsed = JSON.parse(text) as Record<string, Object>;
      return parsed.format === MEMORY_ENVELOPE_FORMAT;
    } catch (_error) {
      return false;
    }
  }

  static aadBytes(): Uint8Array {
    return MemoryFileCryptoCodec.utf8Encode(MEMORY_CRYPTO_AAD_TEXT);
  }

  static utf8Encode(text: string): Uint8Array {
    return MemoryFileCryptoCodec.encoder.encodeInto(text);
  }

  static utf8DecodeStrict(bytes: Uint8Array): string {
    if (!MemoryFileCryptoCodec.isValidUtf8(bytes)) {
      throw new Error('invalid_utf8');
    }
    return MemoryFileCryptoCodec.decoder.decodeWithStream(bytes, { stream: false });
  }

  static estimateEnvelopeSize(plaintextBytesLength: number): number {
    const nonceBase64 = MemoryFileCryptoCodec.base64Length(MEMORY_GCM_NONCE_SIZE);
    const tagBase64 = MemoryFileCryptoCodec.base64Length(MEMORY_GCM_TAG_SIZE);
    const ciphertextBase64 = MemoryFileCryptoCodec.base64Length(plaintextBytesLength);
    const fixedEnvelopeOverhead = 180;
    return fixedEnvelopeOverhead + nonceBase64 + tagBase64 + ciphertextBase64;
  }

  static encodeBase64(bytes: Uint8Array): string {
    return MemoryFileCryptoCodec.base64Helper.encodeToStringSync(bytes);
  }

  static decodeBase64Strict(value: string): Uint8Array {
    if (value.length <= 0 || value.length % 4 !== 0 || !BASE64_PATTERN.test(value)) {
      throw new Error('invalid_base64');
    }
    try {
      const decoded = MemoryFileCryptoCodec.base64Helper.decodeSync(value);
      return decoded;
    } catch (_error) {
      throw new Error('invalid_base64: ' + _error.message);
    }
  }

  static concat(parts: Uint8Array[]): Uint8Array {
    let totalLength = 0;
    for (let i = 0; i < parts.length; i++) {
      totalLength += parts[i].length;
    }
    const output = new Uint8Array(totalLength);
    let offset = 0;
    for (let i = 0; i < parts.length; i++) {
      output.set(parts[i], offset);
      offset += parts[i].length;
    }
    return output;
  }

  private static base64Length(byteLength: number): number {
    return Math.ceil(byteLength / 3) * 4;
  }

  private static isValidUtf8(bytes: Uint8Array): boolean {
    let index = 0;
    while (index < bytes.length) {
      const first = bytes[index];
      if (first <= 0x7f) {
        index += 1;
        continue;
      }
      if (first >= 0xc2 && first <= 0xdf) {
        if (!MemoryFileCryptoCodec.validateUtf8TwoByte(bytes, index)) {
          return false;
        }
        index += 2;
        continue;
      }
      if (first >= 0xe0 && first <= 0xef) {
        if (!MemoryFileCryptoCodec.validateUtf8ThreeByte(bytes, index)) {
          return false;
        }
        index += 3;
        continue;
      }
      if (first >= 0xf0 && first <= 0xf4) {
        if (!MemoryFileCryptoCodec.validateUtf8FourByte(bytes, index)) {
          return false;
        }
        index += 4;
        continue;
      }
      return false;
    }
    return true;
  }

  private static isUtf8Continuation(value: number): boolean {
    return value >= 0x80 && value <= 0xbf;
  }

  private static validateUtf8TwoByte(bytes: Uint8Array, index: number): boolean {
    if (index + 1 >= bytes.length) {
      return false;
    }
    return MemoryFileCryptoCodec.isUtf8Continuation(bytes[index + 1]);
  }

  private static validateUtf8ThreeByte(bytes: Uint8Array, index: number): boolean {
    if (index + 2 >= bytes.length) {
      return false;
    }
    const first = bytes[index];
    const second = bytes[index + 1];
    const third = bytes[index + 2];
    return MemoryFileCryptoCodec.isUtf8Continuation(second) &&
      MemoryFileCryptoCodec.isUtf8Continuation(third) &&
      !MemoryFileCryptoCodec.isInvalidUtf8ThreeByteStart(first, second);
  }

  private static isInvalidUtf8ThreeByteStart(first: number, second: number): boolean {
    return (first === 0xe0 && second < 0xa0) ||
      (first === 0xed && second >= 0xa0);
  }

  private static validateUtf8FourByte(bytes: Uint8Array, index: number): boolean {
    if (index + 3 >= bytes.length) {
      return false;
    }
    const first = bytes[index];
    const second = bytes[index + 1];
    return MemoryFileCryptoCodec.isUtf8Continuation(second) &&
      MemoryFileCryptoCodec.isUtf8Continuation(bytes[index + 2]) &&
      MemoryFileCryptoCodec.isUtf8Continuation(bytes[index + 3]) &&
      !MemoryFileCryptoCodec.isInvalidUtf8FourByteStart(first, second);
  }

  private static isInvalidUtf8FourByteStart(first: number, second: number): boolean {
    return (first === 0xf0 && second < 0x90) ||
      (first === 0xf4 && second >= 0x90);
  }
}
