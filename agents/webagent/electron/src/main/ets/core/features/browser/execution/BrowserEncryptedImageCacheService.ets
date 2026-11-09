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

import fs from '@ohos.file.fs';
import { fileUri } from '@kit.CoreFileKit';
import { MemoryFileCryptoCodec } from '../agent/memory/local/crypto/MemoryFileCryptoCodec';
import { MemoryFileCryptoService } from '../agent/memory/local/crypto/MemoryFileCryptoService';
import {
  MEMORY_GCM_NONCE_SIZE,
  MEMORY_GCM_TAG_SIZE
} from '../agent/memory/local/crypto/MemoryFileCryptoEnvelope';
import { BrowserSessionWorkspaceService } from './BrowserSessionWorkspaceService';

const FILE_URI_PREFIX: string = 'file://';
const ENCRYPTED_FILE_SUFFIX: string = '.enc';
const IMAGE_DATA_URL_PREFIX: string = 'data:image/';
const IMAGE_DATA_URL_BASE64_MARKER: string = ';base64,';
const IMAGE_CACHE_AAD_TEXT: string =
  'ultron-browser-agent|external-llm|image-cache|binary=1|key=1';
const MAX_DATA_URL_BYTES: number = 10 * 1024 * 1024;
const ENCRYPTED_HEADER_BYTES: number = MEMORY_GCM_NONCE_SIZE + MEMORY_GCM_TAG_SIZE;
const MAX_ENCRYPTED_FILE_BYTES: number = ENCRYPTED_HEADER_BYTES + MAX_DATA_URL_BYTES;

interface FileHandle {
  fd: number;
}

interface FileStat {
  size: number;
}

/** Encrypts External LLM image data URLs before persisting them in the app sandbox. */
export class BrowserEncryptedImageCacheService {
  static async writeDataUrl(baseDir: string, relativePath: string, dataUrl: string): Promise<string> {
    if (!relativePath.endsWith(ENCRYPTED_FILE_SUFFIX)) {
      throw new Error('ENCRYPTED_IMAGE_CACHE_PATH_INVALID');
    }
    BrowserEncryptedImageCacheService.assertImageDataUrl(dataUrl);
    const plaintext = MemoryFileCryptoCodec.utf8Encode(dataUrl);
    const encrypted = await MemoryFileCryptoService.encryptWithAad(
      plaintext,
      BrowserEncryptedImageCacheService.imageAadBytes()
    );
    const payload = MemoryFileCryptoCodec.concat([encrypted.nonce, encrypted.tag, encrypted.ciphertext]);
    const filePath = BrowserSessionWorkspaceService.writeBytesFile(baseDir, relativePath, payload);
    if (filePath.length <= 0) {
      throw new Error('ENCRYPTED_IMAGE_CACHE_WRITE_FAILED');
    }
    return filePath;
  }

  /** Returns inline image data unchanged, or decrypts an encrypted cache file URI in memory. */
  static async resolveDataUrl(url: string): Promise<string> {
    if (url.startsWith(IMAGE_DATA_URL_PREFIX)) {
      BrowserEncryptedImageCacheService.assertImageDataUrl(url);
      return url;
    }
    return await BrowserEncryptedImageCacheService.readDataUrl(url);
  }

  private static async readDataUrl(encryptedFileUri: string): Promise<string> {
    const filePath = BrowserEncryptedImageCacheService.resolveEncryptedFilePath(encryptedFileUri);
    const payload = BrowserEncryptedImageCacheService.readEncryptedBytes(filePath);
    const nonce = payload.subarray(0, MEMORY_GCM_NONCE_SIZE);
    const tag = payload.subarray(MEMORY_GCM_NONCE_SIZE, ENCRYPTED_HEADER_BYTES);
    const ciphertext = payload.subarray(ENCRYPTED_HEADER_BYTES);
    const plaintext = await MemoryFileCryptoService.decryptWithAad(
      nonce,
      tag,
      ciphertext,
      BrowserEncryptedImageCacheService.imageAadBytes()
    );
    const dataUrl = MemoryFileCryptoCodec.utf8DecodeStrict(plaintext);
    BrowserEncryptedImageCacheService.assertImageDataUrl(dataUrl);
    return dataUrl;
  }

  private static readEncryptedBytes(filePath: string): Uint8Array {
    let file: FileHandle | null = null;
    try {
      file = fs.openSync(filePath, fs.OpenMode.READ_ONLY) as FileHandle;
      const stat = fs.statSync(file.fd) as FileStat;
      const size = stat.size;
      if (!Number.isSafeInteger(size) || size <= ENCRYPTED_HEADER_BYTES || size > MAX_ENCRYPTED_FILE_BYTES) {
        throw new Error('ENCRYPTED_IMAGE_CACHE_INVALID');
      }
      const buffer = new ArrayBuffer(size);
      if (fs.readSync(file.fd, buffer) !== size) {
        throw new Error('ENCRYPTED_IMAGE_CACHE_READ_FAILED');
      }
      return new Uint8Array(buffer);
    } finally {
      if (file !== null) {
        fs.closeSync(file.fd);
      }
    }
  }

  private static resolveEncryptedFilePath(encryptedFileUri: string): string {
    const filesDir = BrowserSessionWorkspaceService.getFilesDir();
    let filePath = '';
    try {
      filePath = new fileUri.FileUri(encryptedFileUri).path;
    } catch (_error) {
      throw new Error('ENCRYPTED_IMAGE_CACHE_URI_INVALID');
    }
    if (filesDir.length <= 0 || !encryptedFileUri.startsWith(FILE_URI_PREFIX) ||
      !filePath.startsWith(`${filesDir}/`) || !filePath.endsWith(ENCRYPTED_FILE_SUFFIX) ||
      filePath.indexOf('..') >= 0) {
      throw new Error('ENCRYPTED_IMAGE_CACHE_URI_INVALID');
    }
    return filePath;
  }

  private static assertImageDataUrl(dataUrl: string): void {
    const markerIndex = dataUrl.indexOf(IMAGE_DATA_URL_BASE64_MARKER);
    if (!dataUrl.startsWith(IMAGE_DATA_URL_PREFIX) || markerIndex <= IMAGE_DATA_URL_PREFIX.length ||
      markerIndex + IMAGE_DATA_URL_BASE64_MARKER.length >= dataUrl.length ||
      MemoryFileCryptoCodec.utf8Encode(dataUrl).length > MAX_DATA_URL_BYTES) {
      throw new Error('IMAGE_DATA_URL_INVALID');
    }
  }

  private static imageAadBytes(): Uint8Array {
    return MemoryFileCryptoCodec.utf8Encode(IMAGE_CACHE_AAD_TEXT);
  }
}
