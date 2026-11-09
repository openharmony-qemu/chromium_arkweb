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

export class MemoryEncryptedEnvelope {
  public format: string = MEMORY_ENVELOPE_FORMAT;
  public version: number = MEMORY_ENVELOPE_VERSION;
  public plaintextSchemaVersion: number = MEMORY_PLAINTEXT_SCHEMA_VERSION;
  public algorithm: string = MEMORY_CRYPTO_ALGORITHM;
  public keyVersion: number = MEMORY_KEY_VERSION;
  public nonce: string = '';
  public tag: string = '';
  public ciphertext: string = '';
}

export class MemoryFileCryptoResult {
  public nonce: Uint8Array = new Uint8Array(0);
  public tag: Uint8Array = new Uint8Array(0);
  public ciphertext: Uint8Array = new Uint8Array(0);
}

export const MEMORY_ENVELOPE_FORMAT: string = 'ultron.memory.encrypted';
export const MEMORY_ENVELOPE_VERSION: number = 1;
export const MEMORY_PLAINTEXT_SCHEMA_VERSION: number = 1;
export const MEMORY_CRYPTO_ALGORITHM: string = 'AES-256-GCM';
export const MEMORY_KEY_VERSION: number = 1;
export const MEMORY_KEY_ALIAS: string = 'ultron.browser.memory.primary.v1';
export const MEMORY_GCM_NONCE_SIZE: number = 12;
export const MEMORY_GCM_TAG_SIZE: number = 16;
export const MEMORY_CRYPTO_CHUNK_SIZE: number = 64 * 1024;
export const MEMORY_CRYPTO_AAD_TEXT: string =
  'ultron-browser-agent|memory-store|file=procedural_memory.json|envelope=1|snapshot=1|key=1';
