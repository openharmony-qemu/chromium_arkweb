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

import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { huks } from '@kit.UniversalKeystoreKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import {
  MEMORY_CRYPTO_CHUNK_SIZE,
  MemoryFileCryptoResult,
  MEMORY_GCM_NONCE_SIZE,
  MEMORY_GCM_TAG_SIZE,
  MEMORY_KEY_ALIAS
} from './MemoryFileCryptoEnvelope';
import { MemoryFileCryptoCodec } from './MemoryFileCryptoCodec';

const DOMAIN = 0x6020;
const TAG = 'MemoryCrypto';

export class MemoryCryptoError extends Error {
  public code: string;

  constructor(code: string, message: string) {
    super(message);
    this.code = code;
  }
}

class MemoryHuksOptions {
  public properties: huks.HuksParam[] = [];
}

class MemoryHuksDataOptions extends MemoryHuksOptions {
  public inData: Uint8Array = new Uint8Array(0);
}

export class MemoryFileCryptoService {
  static async ensureKey(allowCreate: boolean = true): Promise<void> {
    const properties = MemoryFileCryptoService.keyProperties();
    const options = MemoryFileCryptoService.options(properties);
    hilog.info(DOMAIN, TAG, 'ensureKey begin allowCreate=%{public}s alias=%{public}s',
      `${allowCreate}`, MEMORY_KEY_ALIAS);
    try {
      await huks.hasKeyItem(MEMORY_KEY_ALIAS, options);
      hilog.info(DOMAIN, TAG, 'ensureKey existing key ok');
      return;
    } catch (error) {
      const classified = MemoryFileCryptoService.classifyHuksError(error, 'CRYPTO_KEY_TEMPORARILY_UNAVAILABLE');
      hilog.warn(DOMAIN, TAG, 'ensureKey hasKeyItem failed code=%{public}s huksCode=%{public}s',
        classified.code, MemoryFileCryptoService.rawCode(error));
      if (!allowCreate) {
        throw classified;
      }
    }
    if (!allowCreate) {
      hilog.warn(DOMAIN, TAG, 'ensureKey missing key and create disabled');
      throw new MemoryCryptoError('CRYPTO_KEY_MISSING', 'memory key missing');
    }
    try {
      await huks.generateKeyItem(MEMORY_KEY_ALIAS, options);
      hilog.info(DOMAIN, TAG, 'ensureKey generated key ok');
    } catch (error) {
      const classified = MemoryFileCryptoService.classifyHuksError(error, 'CRYPTO_INIT_FAILED');
      hilog.warn(DOMAIN, TAG, 'ensureKey generateKeyItem failed code=%{public}s huksCode=%{public}s msg=%{public}s',
        classified.code, MemoryFileCryptoService.rawCode(error), MemoryFileCryptoService.rawMessage(error));
      throw classified;
    }
  }

  static async recreateKeyForEmptyStoreRecovery(): Promise<void> {
    const properties = MemoryFileCryptoService.keyProperties();
    const options = MemoryFileCryptoService.options(properties);
    hilog.warn(DOMAIN, TAG, 'recreateKeyForEmptyStoreRecovery begin alias=%{public}s', MEMORY_KEY_ALIAS);
    try {
      await huks.deleteKeyItem(MEMORY_KEY_ALIAS, options);
      hilog.warn(DOMAIN, TAG, 'recreateKeyForEmptyStoreRecovery deleted old key');
    } catch (error) {
      hilog.warn(DOMAIN, TAG, 'recreateKeyForEmptyStoreRecovery delete old key skipped huksCode=%{public}s msg=%{public}s',
        MemoryFileCryptoService.rawCode(error), MemoryFileCryptoService.rawMessage(error));
    }
    try {
      await huks.generateKeyItem(MEMORY_KEY_ALIAS, options);
      hilog.warn(DOMAIN, TAG, 'recreateKeyForEmptyStoreRecovery generated new key ok');
    } catch (error) {
      const classified = MemoryFileCryptoService.classifyHuksError(error, 'CRYPTO_INIT_FAILED');
      hilog.warn(DOMAIN, TAG, 'recreateKeyForEmptyStoreRecovery generate failed code=%{public}s huksCode=%{public}s msg=%{public}s',
        classified.code, MemoryFileCryptoService.rawCode(error), MemoryFileCryptoService.rawMessage(error));
      throw classified;
    }
  }

  static async encrypt(plaintext: Uint8Array): Promise<MemoryFileCryptoResult> {
    return await MemoryFileCryptoService.encryptWithAad(plaintext, MemoryFileCryptoCodec.aadBytes());
  }

  static async encryptWithAad(plaintext: Uint8Array, aad: Uint8Array): Promise<MemoryFileCryptoResult> {
    MemoryFileCryptoService.assertNonEmptyAad(aad);
    hilog.info(DOMAIN, TAG, 'encrypt begin plaintextBytes=%{public}d', plaintext.length);
    await MemoryFileCryptoService.ensureKey(true);
    const nonce = cryptoFramework.createRandom().generateRandomSync(MEMORY_GCM_NONCE_SIZE).data;
    const resultBytes = await MemoryFileCryptoService.runSession(
      huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_ENCRYPT,
      nonce,
      null,
      plaintext,
      aad
    );
    if (resultBytes.length < MEMORY_GCM_TAG_SIZE) {
      throw new MemoryCryptoError('CRYPTO_FINISH_FAILED', 'invalid encrypted output');
    }
    const tagOffset = resultBytes.length - MEMORY_GCM_TAG_SIZE;
    const result = new MemoryFileCryptoResult();
    result.nonce = nonce;
    result.ciphertext = resultBytes.subarray(0, tagOffset);
    result.tag = resultBytes.subarray(tagOffset);
    hilog.info(DOMAIN, TAG, 'encrypt ok ciphertextBytes=%{public}d', result.ciphertext.length);
    return result;
  }

  static async decrypt(nonce: Uint8Array, tag: Uint8Array, ciphertext: Uint8Array): Promise<Uint8Array> {
    return await MemoryFileCryptoService.decryptWithAad(nonce, tag, ciphertext, MemoryFileCryptoCodec.aadBytes());
  }

  static async decryptWithAad(
    nonce: Uint8Array,
    tag: Uint8Array,
    ciphertext: Uint8Array,
    aad: Uint8Array
  ): Promise<Uint8Array> {
    MemoryFileCryptoService.assertNonEmptyAad(aad);
    await MemoryFileCryptoService.ensureKey(false);
    return await MemoryFileCryptoService.runSession(
      huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_DECRYPT,
      nonce,
      tag,
      ciphertext,
      aad
    );
  }

  static isTemporaryError(error: Error): boolean {
    const code = MemoryFileCryptoService.errorCode(error);
    return code === 'CRYPTO_KEY_TEMPORARILY_UNAVAILABLE' ||
      code === 'CRYPTO_INIT_FAILED' ||
      code === 'CRYPTO_UPDATE_FAILED' ||
      code === 'CRYPTO_FINISH_FAILED';
  }

  static isAuthError(error: Error): boolean {
    return MemoryFileCryptoService.errorCode(error) === 'CRYPTO_AUTH_FAILED';
  }

  static errorCode(error: Object): string {
    if (error instanceof MemoryCryptoError) {
      return error.code;
    }
    if (typeof error === 'object' && error !== null) {
      const record = error as Record<string, Object>;
      const code = record.code;
      if (typeof code === 'string') {
        return code;
      }
    }
    return '';
  }

  private static async runSession(
    purpose: number,
    nonce: Uint8Array,
    tag: Uint8Array | null,
    input: Uint8Array,
    aad: Uint8Array
  ): Promise<Uint8Array> {
    const sessionProperties = MemoryFileCryptoService.sessionProperties(purpose, nonce, tag, aad);
    const options = MemoryFileCryptoService.options(sessionProperties);
    const dataOptions = MemoryFileCryptoService.dataOptions(sessionProperties);
    let handle: number = 0;
    try {
      hilog.info(DOMAIN, TAG, 'huks initSession begin purpose=%{public}d inputBytes=%{public}d',
        purpose, input.length);
      const initResult = await huks.initSession(MEMORY_KEY_ALIAS, options);
      handle = initResult.handle;
      hilog.info(DOMAIN, TAG, 'huks initSession ok handle=%{public}d', handle);
      const outputs: Uint8Array[] = [];
      let offset = 0;
      while (offset < input.length) {
        const nextOffset = Math.min(offset + MEMORY_CRYPTO_CHUNK_SIZE, input.length);
        const chunk = input.subarray(offset, nextOffset);
        dataOptions.inData = chunk;
        const updated = await huks.updateSession(handle, dataOptions);
        hilog.info(DOMAIN, TAG, 'huks updateSession ok purpose=%{public}d inputBytes=%{public}d outputBytes=%{public}d',
          purpose, chunk.length, updated && updated.outData ? updated.outData.length : 0);
        if (updated && updated.outData && updated.outData.length > 0) {
          outputs.push(updated.outData);
        }
        offset = nextOffset;
      }
      const finished = await huks.finishSession(handle, options);
      hilog.info(DOMAIN, TAG, 'huks finishSession raw ok purpose=%{public}d outputBytes=%{public}d',
        purpose, finished && finished.outData ? finished.outData.length : 0);
      if (finished && finished.outData && finished.outData.length > 0) {
        outputs.push(finished.outData);
      }
      const result = MemoryFileCryptoCodec.concat(outputs);
      hilog.info(DOMAIN, TAG, 'huks finishSession ok purpose=%{public}d outputBytes=%{public}d',
        purpose, result.length);
      return result;
    } catch (error) {
      if (handle !== 0) {
        await MemoryFileCryptoService.abortSessionSafe(handle);
      }
      const fallback = purpose === huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_DECRYPT
        ? 'CRYPTO_AUTH_FAILED'
        : 'CRYPTO_FINISH_FAILED';
      const classified = MemoryFileCryptoService.classifyHuksError(error, fallback);
      hilog.warn(DOMAIN, TAG, 'huks session failed code=%{public}s huksCode=%{public}s msg=%{public}s',
        classified.code, MemoryFileCryptoService.rawCode(error), MemoryFileCryptoService.rawMessage(error));
      throw classified;
    }
  }

  private static async abortSessionSafe(handle: number): Promise<void> {
    try {
      await huks.abortSession(handle, MemoryFileCryptoService.options([]));
      hilog.info(DOMAIN, TAG, 'huks abortSession ok handle=%{public}d', handle);
    } catch (_error) {
      hilog.warn(DOMAIN, TAG, 'huks abortSession failed handle=%{public}d msg=%{public}s',
        handle, MemoryFileCryptoService.rawMessage(_error));
    }
  }

  private static options(properties: huks.HuksParam[]): MemoryHuksOptions {
    const options = new MemoryHuksOptions();
    options.properties = properties;
    return options;
  }

  private static dataOptions(properties: huks.HuksParam[]): MemoryHuksDataOptions {
    const options = new MemoryHuksDataOptions();
    options.properties = properties;
    return options;
  }

  private static keyProperties(): huks.HuksParam[] {
    return [
      { tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: huks.HuksKeyAlg.HUKS_ALG_AES },
      { tag: huks.HuksTag.HUKS_TAG_KEY_SIZE, value: huks.HuksKeySize.HUKS_AES_KEY_SIZE_256 },
      {
        tag: huks.HuksTag.HUKS_TAG_PURPOSE,
        value: huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_ENCRYPT | huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_DECRYPT
      },
      { tag: huks.HuksTag.HUKS_TAG_BLOCK_MODE, value: huks.HuksCipherMode.HUKS_MODE_GCM },
      { tag: huks.HuksTag.HUKS_TAG_PADDING, value: huks.HuksKeyPadding.HUKS_PADDING_NONE },
      { tag: huks.HuksTag.HUKS_TAG_AUTH_STORAGE_LEVEL, value: huks.HuksAuthStorageLevel.HUKS_AUTH_STORAGE_LEVEL_CE }
    ];
  }

  private static sessionProperties(
    purpose: number,
    nonce: Uint8Array,
    tag: Uint8Array | null,
    aad: Uint8Array
  ): huks.HuksParam[] {
    const params: huks.HuksParam[] = [
      { tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: huks.HuksKeyAlg.HUKS_ALG_AES },
      { tag: huks.HuksTag.HUKS_TAG_KEY_SIZE, value: huks.HuksKeySize.HUKS_AES_KEY_SIZE_256 },
      { tag: huks.HuksTag.HUKS_TAG_PURPOSE, value: purpose },
      { tag: huks.HuksTag.HUKS_TAG_BLOCK_MODE, value: huks.HuksCipherMode.HUKS_MODE_GCM },
      { tag: huks.HuksTag.HUKS_TAG_PADDING, value: huks.HuksKeyPadding.HUKS_PADDING_NONE },
      { tag: huks.HuksTag.HUKS_TAG_NONCE, value: nonce },
      { tag: huks.HuksTag.HUKS_TAG_ASSOCIATED_DATA, value: aad },
      { tag: huks.HuksTag.HUKS_TAG_AUTH_STORAGE_LEVEL, value: huks.HuksAuthStorageLevel.HUKS_AUTH_STORAGE_LEVEL_CE }
    ];
    if (tag !== null) {
      params.push({ tag: huks.HuksTag.HUKS_TAG_AE_TAG, value: tag });
    }
    return params;
  }

  private static assertNonEmptyAad(aad: Uint8Array): void {
    if (aad.length <= 0) {
      throw new MemoryCryptoError('CRYPTO_INVALID_AAD', 'aad must be non-empty');
    }
  }

  private static classifyHuksError(error: Object, fallbackCode: string): MemoryCryptoError {
    const message = MemoryFileCryptoService.rawMessage(error);
    const lower = message.toLowerCase();
    if (lower.indexOf('auth') >= 0 || lower.indexOf('verify') >= 0 || lower.indexOf('tag') >= 0) {
      return new MemoryCryptoError('CRYPTO_AUTH_FAILED', message);
    }
    if (lower.indexOf('not exist') >= 0 || lower.indexOf('not found') >= 0 || lower.indexOf('missing') >= 0) {
      return new MemoryCryptoError('CRYPTO_KEY_MISSING', message);
    }
    if (lower.indexOf('busy') >= 0 || lower.indexOf('tempor') >= 0 || lower.indexOf('unavailable') >= 0) {
      return new MemoryCryptoError('CRYPTO_KEY_TEMPORARILY_UNAVAILABLE', message);
    }
    return new MemoryCryptoError(fallbackCode, message);
  }

  private static rawCode(error: Object): string {
    if (typeof error === 'object' && error !== null) {
      const record = error as Record<string, Object>;
      const code = record.code;
      if (typeof code === 'number' || typeof code === 'string') {
        return `${code}`;
      }
    }
    return '';
  }

  private static rawMessage(error: Object): string {
    if (error instanceof Error) {
      return error.message;
    }
    if (typeof error === 'object' && error !== null) {
      const record = error as Record<string, Object>;
      const message = record.message;
      if (typeof message === 'string') {
        return message;
      }
      const code = record.code;
      if (typeof code === 'number' || typeof code === 'string') {
        return `code=${code}`;
      }
    }
    return `${error}`;
  }
}
