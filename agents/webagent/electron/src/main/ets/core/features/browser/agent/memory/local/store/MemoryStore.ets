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
import { hilog } from '@kit.PerformanceAnalysisKit';
import { AppContextHolderService } from '../../../../../../adapter/AppContextHolderService';
import { StoredMemoryView, StoredMemoryPayload } from '../domain/MemoryItemModel';
import { MemoryLocalConfig } from '../config/MemoryLocalConfig';
import { MemoryFileCryptoCodec } from '../crypto/MemoryFileCryptoCodec';
import { MemoryCryptoError, MemoryFileCryptoService } from '../crypto/MemoryFileCryptoService';

const DOMAIN = 0x6020;
const TAG = 'MemoryStore';
const STORE_DIR_NAME = 'memory_local';
const STORE_FILE_NAME = 'procedural_memory.json';

interface FileHandle {
  fd: number;
}

class MemoryStoreSnapshot {
  public version: number = 1;
  public items: Record<string, StoredMemoryPayload> = {};
}

class MemoryStoreState {
  public static readonly UNINITIALIZED: string = 'UNINITIALIZED';
  public static readonly INITIALIZING: string = 'INITIALIZING';
  public static readonly READY: string = 'READY';
  public static readonly UNAVAILABLE: string = 'UNAVAILABLE';
}

class LoadEncryptedResult {
  public ok: boolean = false;
  public temporary: boolean = false;
  public errorCode: string = '';
  public items: Map<string, StoredMemoryPayload> = new Map();
  public sizeBytes: number = 0;
}

class SnapshotBytes {
  public bytes: Uint8Array = new Uint8Array(0);
}

export class MemoryStoreFilters {
  public userId: string = '';
  public agentId: string = '';
  public type: string = '';
}

export class MemoryStore {
  private static state: string = MemoryStoreState.UNINITIALIZED;
  private static initPromise: Promise<void> | null = null;
  private static writeQueue: Promise<void> = Promise.resolve();
  private static storePath: string = '';
  private static items: Map<string, StoredMemoryPayload> = new Map();
  private static lastPersistSizeBytes: number = 0;
  private static lastCryptoErrorCode: string = '';

  static async init(): Promise<void> {
    if (MemoryStore.state === MemoryStoreState.READY) {
      return;
    }
    if (MemoryStore.initPromise !== null) {
      hilog.info(DOMAIN, TAG, 'init join existing promise state=%{public}s', MemoryStore.state);
      return MemoryStore.initPromise;
    }
    hilog.info(DOMAIN, TAG, 'init begin state=%{public}s', MemoryStore.state);
    MemoryStore.state = MemoryStoreState.INITIALIZING;
    MemoryStore.initPromise = MemoryStore.doInit().finally(() => {
      MemoryStore.initPromise = null;
    });
    return MemoryStore.initPromise;
  }

  static isInitialized(): boolean {
    return MemoryStore.state === MemoryStoreState.READY;
  }

  static getStorePath(): string {
    return MemoryStore.storePath;
  }

  static count(): number {
    MemoryStore.ensureReady();
    return MemoryStore.items.size;
  }

  static async insert(id: string, payload: StoredMemoryPayload): Promise<void> {
    if (id.length <= 0) {
      throw new Error('MemoryStore.insert: id required');
    }
    if (MemoryStore.items.has(id)) {
      hilog.warn(DOMAIN, TAG, 'insert skipped: id already exists %{public}s, treating as UPDATE', id);
    }
    await MemoryStore.enqueueMutation((draft: Map<string, StoredMemoryPayload>): void => {
      draft.set(id, MemoryStore.clonePayload(payload));
    });
  }

  static async update(id: string, payload: StoredMemoryPayload): Promise<void> {
    await MemoryStore.enqueueMutation((draft: Map<string, StoredMemoryPayload>): void => {
      if (!draft.has(id)) {
        hilog.warn(DOMAIN, TAG, 'update skipped: id not found %{public}s', id);
        return;
      }
      draft.set(id, MemoryStore.clonePayload(payload));
    });
  }

  static async delete(id: string): Promise<void> {
    await MemoryStore.enqueueMutation((draft: Map<string, StoredMemoryPayload>): void => {
      draft.delete(id);
    });
  }

  static get(id: string): StoredMemoryPayload | null {
    MemoryStore.ensureReady();
    const v = MemoryStore.items.get(id);
    return v ? MemoryStore.clonePayload(v) : null;
  }

  static list(filters: MemoryStoreFilters, limit: number = -1): StoredMemoryView[] {
    MemoryStore.ensureReady();
    const out: StoredMemoryView[] = [];
    const keys: string[] = Array.from(MemoryStore.items.keys());
    for (let i = 0; i < keys.length; i++) {
      const id = keys[i];
      const payload = MemoryStore.items.get(id);
      if (!payload) {
        continue;
      }
      if (filters.userId.length > 0 && payload.userId !== filters.userId) {
        continue;
      }
      if (filters.agentId.length > 0 && payload.agentId !== filters.agentId) {
        continue;
      }
      if (filters.type.length > 0 && payload.type !== filters.type) {
        continue;
      }
      const view = new StoredMemoryView();
      view.id = id;
      view.payload = MemoryStore.payloadToRecord(payload);
      out.push(view);
      if (limit > 0 && out.length >= limit) {
        break;
      }
    }
    return out;
  }

  static async deleteAll(): Promise<number> {
    let removed = 0;
    await MemoryStore.enqueueMutation((draft: Map<string, StoredMemoryPayload>): void => {
      removed = draft.size;
      draft.clear();
    });
    hilog.info(DOMAIN, TAG, 'deleteAll: removed %{public}d items', removed);
    return removed;
  }

  static async clean(filters: MemoryStoreFilters): Promise<number> {
    let removed = 0;
    await MemoryStore.enqueueMutation((draft: Map<string, StoredMemoryPayload>): void => {
      const toRemove: string[] = [];
      const keys: string[] = Array.from(draft.keys());
      for (let i = 0; i < keys.length; i++) {
        const id = keys[i];
        const payload = draft.get(id);
        if (!payload) {
          continue;
        }
        if (filters.userId.length > 0 && payload.userId !== filters.userId) {
          continue;
        }
        if (filters.agentId.length > 0 && payload.agentId !== filters.agentId) {
          continue;
        }
        if (filters.type.length > 0 && payload.type !== filters.type) {
          continue;
        }
        toRemove.push(id);
      }
      for (let i = 0; i < toRemove.length; i++) {
        draft.delete(toRemove[i]);
      }
      removed = toRemove.length;
    });
    return removed;
  }

  static getStorageSizeBytes(): number {
    return MemoryStore.lastPersistSizeBytes;
  }

  static getSessionMemoryEntryCount(): number {
    MemoryStore.ensureReady();
    let count = 0;
    const keys: string[] = Array.from(MemoryStore.items.keys());
    for (let i = 0; i < keys.length; i++) {
      const p = MemoryStore.items.get(keys[i]);
      if (p && p.type === 'session_memory') {
        count++;
      }
    }
    return count;
  }

  static getLastCryptoErrorCode(): string {
    return MemoryStore.lastCryptoErrorCode;
  }

  private static async doInit(): Promise<void> {
    const context = AppContextHolderService.getContext();
    if (!context) {
      MemoryStore.state = MemoryStoreState.UNINITIALIZED;
      hilog.warn(DOMAIN, TAG, 'init skipped: no app context yet');
      return;
    }
    const rootDir = `${context.filesDir}/${STORE_DIR_NAME}`;
    MemoryStore.ensureDir(rootDir);
    MemoryStore.storePath = `${rootDir}/${STORE_FILE_NAME}`;
    hilog.info(DOMAIN, TAG, 'init resolved paths filesDir=%{public}s rootDir=%{public}s storePath=%{public}s bakPath=%{public}s',
      context.filesDir, rootDir, MemoryStore.storePath, MemoryStore.candidatePath());
    try {
      await MemoryStore.loadEncryptedStore();
      MemoryStore.state = MemoryStoreState.READY;
      MemoryStore.lastCryptoErrorCode = '';
      hilog.info(DOMAIN, TAG, 'memory store initialized path=%{public}s items=%{public}d size=%{public}d',
        MemoryStore.storePath, MemoryStore.items.size, MemoryStore.lastPersistSizeBytes);
    } catch (error) {
      MemoryStore.state = MemoryStoreState.UNAVAILABLE;
      MemoryStore.lastCryptoErrorCode = MemoryStore.errorCode(error);
      hilog.error(DOMAIN, TAG, 'memory store unavailable code=%{public}s msg=%{public}s',
        MemoryStore.lastCryptoErrorCode, MemoryStore.errorMessage(error));
    }
  }

  private static ensureReady(): void {
    if (MemoryStore.state !== MemoryStoreState.READY) {
      throw new Error(`MemoryStore not ready: ${MemoryStore.state}`);
    }
  }

  private static async enqueueMutation(work: (draft: Map<string, StoredMemoryPayload>) => void): Promise<void> {
    const run = MemoryStore.writeQueue.then(async (): Promise<void> => {
      await MemoryStore.init();
      MemoryStore.ensureReady();
      hilog.info(DOMAIN, TAG, 'mutation begin path=%{public}s items=%{public}d size=%{public}d',
        MemoryStore.storePath, MemoryStore.items.size, MemoryStore.lastPersistSizeBytes);
      const draft = MemoryStore.cloneItems(MemoryStore.items);
      work(draft);
      const prepared = MemoryStore.prepareSnapshotWithinLimit(draft);
      await MemoryStore.commitPreparedSnapshot(draft, prepared.bytes);
    });
    MemoryStore.writeQueue = run.catch((_error: Error) => {
    });
    return run;
  }

  private static async loadEncryptedStore(): Promise<void> {
    const mainPath = MemoryStore.storePath;
    const bakPath = MemoryStore.candidatePath();
    hilog.info(DOMAIN, TAG, 'load begin main=%{public}s bak=%{public}s', mainPath, bakPath);
    let hadMainFile = false;
    let hadBakFile = false;
    try {
      hadMainFile = fs.accessSync(mainPath);
    } catch (e) {
      hadMainFile = false;
    }
    try {
      hadBakFile = fs.accessSync(bakPath);
    } catch (e) {
      hadBakFile = false;
    }
    if (hadMainFile) {
      hilog.info(DOMAIN, TAG, 'load probing main exists path=%{public}s', mainPath);
      const main = await MemoryStore.tryLoadEncryptedFile(mainPath);
      if (main.ok) {
        MemoryStore.items = main.items;
        MemoryStore.lastPersistSizeBytes = main.sizeBytes;
        hilog.info(DOMAIN, TAG, 'load main ok path=%{public}s size=%{public}d items=%{public}d',
          mainPath, main.sizeBytes, main.items.size);
        if (hadBakFile) {
          hilog.info(DOMAIN, TAG, 'load cleaning stale bak path=%{public}s', bakPath);
          fs.unlinkSync(bakPath);
        }
        return;
      }
      if (main.temporary) {
        throw new MemoryCryptoError(main.errorCode, 'temporary memory store load failure');
      }
      hilog.warn(DOMAIN, TAG, 'main encrypted store rejected code=%{public}s', main.errorCode);
    }
    if (hadBakFile) {
      hilog.info(DOMAIN, TAG, 'load probing bak exists path=%{public}s', bakPath);
      const candidate = await MemoryStore.tryLoadEncryptedFile(bakPath);
      if (candidate.ok) {
        fs.renameSync(bakPath, mainPath);
        MemoryStore.items = candidate.items;
        MemoryStore.lastPersistSizeBytes = candidate.sizeBytes;
        hilog.warn(DOMAIN, TAG, 'load recovered from bak path=%{public}s size=%{public}d items=%{public}d',
          bakPath, candidate.sizeBytes, candidate.items.size);
        return;
      }
      if (candidate.temporary) {
        throw new MemoryCryptoError(candidate.errorCode, 'temporary memory candidate load failure');
      }
      hilog.warn(DOMAIN, TAG, 'candidate encrypted store rejected code=%{public}s', candidate.errorCode);
    }
    hilog.warn(DOMAIN, TAG, 'load no usable encrypted file, writing empty store main=%{public}s', mainPath);
    await MemoryStore.writeEmptyEncryptedStore(!hadMainFile && !hadBakFile);
  }

  private static async tryLoadEncryptedFile(path: string): Promise<LoadEncryptedResult> {
    const out = new LoadEncryptedResult();
    try {
      hilog.info(DOMAIN, TAG, 'try load file path=%{public}s', path);
      const file = fs.openSync(path, fs.OpenMode.READ_ONLY) as FileHandle;
      try {
        const stat = fs.statSync(path);
        const size = (stat as Object as Record<string, Object>).size as number;
        if (size <= 0) {
          out.errorCode = 'ENVELOPE_INVALID';
          return out;
        }
        const buffer = new ArrayBuffer(size);
        fs.readSync(file.fd, buffer);
        const envelopeText = MemoryFileCryptoCodec.utf8DecodeStrict(new Uint8Array(buffer));
        if (!MemoryFileCryptoCodec.isEncryptedEnvelopeText(envelopeText)) {
          out.errorCode = 'PLAINTEXT_STORE_REJECTED';
          return out;
        }
        const config = MemoryLocalConfig.defaults();
        const encrypted = MemoryFileCryptoCodec.decodeEnvelope(envelopeText, config.storageLimitBytes);
        const plaintext = await MemoryFileCryptoService.decrypt(
          encrypted.nonce,
          encrypted.tag,
          encrypted.ciphertext
        );
        const snapshotText = MemoryFileCryptoCodec.utf8DecodeStrict(plaintext);
        out.items = MemoryStore.parseSnapshot(snapshotText);
        out.sizeBytes = size;
        out.ok = true;
        hilog.info(DOMAIN, TAG, 'try load ok path=%{public}s size=%{public}d items=%{public}d',
          path, size, out.items.size);
        return out;
      } finally {
        fs.closeSync(file.fd);
      }
    } catch (error) {
      const err = error instanceof Error ? error : new Error(`${error}`);
      out.errorCode = MemoryStore.errorCode(err);
      out.temporary = MemoryFileCryptoService.isTemporaryError(err);
      return out;
    }
  }

  private static async writeEmptyEncryptedStore(allowKeyRecreate: boolean): Promise<void> {
    const empty = new Map<string, StoredMemoryPayload>();
    const prepared = MemoryStore.prepareSnapshotWithinLimit(empty);
    hilog.info(DOMAIN, TAG, 'write empty encrypted store path=%{public}s allowKeyRecreate=%{public}s',
      MemoryStore.storePath, `${allowKeyRecreate}`);
    try {
      await MemoryStore.commitPreparedSnapshot(empty, prepared.bytes);
    } catch (error) {
      const err = error instanceof Error ? error : new Error(`${error}`);
      const code = MemoryStore.errorCode(err);
      if (!MemoryFileCryptoService.isTemporaryError(err)) {
        throw err;
      }
      if (!allowKeyRecreate) {
        hilog.warn(DOMAIN, TAG,
          'write empty encrypted store failed with crypto code=%{public}s; key recreate blocked because old store files may exist',
          code);
        throw new MemoryCryptoError('CRYPTO_INIT_FAILED',
          'Cannot recreate memory key: existing store files may be recoverable');
      }
      hilog.warn(DOMAIN, TAG, 'write empty encrypted store failed with crypto code=%{public}s; recreating key and retrying once',
        code);
      await MemoryFileCryptoService.recreateKeyForEmptyStoreRecovery();
      await MemoryStore.commitPreparedSnapshot(empty, prepared.bytes);
    }
  }

  private static prepareSnapshotWithinLimit(source: Map<string, StoredMemoryPayload>): SnapshotBytes {
    const config = MemoryLocalConfig.defaults();
    let snapshotText = MemoryStore.serializeSnapshot(source);
    let bytes = MemoryFileCryptoCodec.utf8Encode(snapshotText);
    if (!config.storageAgingEnabled) {
      if (MemoryFileCryptoCodec.estimateEnvelopeSize(bytes.length) > config.storageLimitBytes) {
        throw new Error('STORAGE_LIMIT_EXCEEDED');
      }
      const out = new SnapshotBytes();
      out.bytes = bytes;
      return out;
    }
    let iterations = 0;
    while (MemoryFileCryptoCodec.estimateEnvelopeSize(bytes.length) > config.storageLimitBytes && iterations < 50) {
      const evicted = MemoryStore.evictOldestSessionFrom(source);
      if (!evicted) {
        throw new Error('STORAGE_LIMIT_EXCEEDED');
      }
      iterations++;
      snapshotText = MemoryStore.serializeSnapshot(source);
      bytes = MemoryFileCryptoCodec.utf8Encode(snapshotText);
    }
    if (iterations > 0) {
      hilog.info(DOMAIN, TAG, 'Aging done before encrypt: %{public}d sessions evicted', iterations);
    }
    if (MemoryFileCryptoCodec.estimateEnvelopeSize(bytes.length) > config.storageLimitBytes) {
      throw new Error('STORAGE_LIMIT_EXCEEDED');
    }
    const out = new SnapshotBytes();
    out.bytes = bytes;
    return out;
  }

  private static async commitPreparedSnapshot(nextItems: Map<string, StoredMemoryPayload>, plaintextBytes: Uint8Array):
    Promise<void> {
    const startedAt = Date.now();
    hilog.info(DOMAIN, TAG, 'commit begin path=%{public}s nextItems=%{public}d plaintextBytes=%{public}d',
      MemoryStore.storePath, nextItems.size, plaintextBytes.length);
    const encrypted = await MemoryFileCryptoService.encrypt(plaintextBytes);
    const envelopeText = MemoryFileCryptoCodec.encodeEnvelope(encrypted);
    const config = MemoryLocalConfig.defaults();
    const envelopeBytes = MemoryFileCryptoCodec.utf8Encode(envelopeText);
    if (envelopeBytes.length > config.storageLimitBytes) {
      throw new Error('STORAGE_LIMIT_EXCEEDED');
    }
    MemoryStore.writeCandidateAndRename(envelopeBytes);
    MemoryStore.items = nextItems;
    MemoryStore.lastPersistSizeBytes = envelopeBytes.length;
    hilog.info(DOMAIN, TAG, 'persist encrypted ok size=%{public}d plaintextBytes=%{public}d elapsedMs=%{public}d',
      envelopeBytes.length, plaintextBytes.length, Date.now() - startedAt);
  }

  private static writeCandidateAndRename(envelopeBytes: Uint8Array): void {
    if (MemoryStore.storePath.length <= 0) {
      throw new Error('FILE_COMMIT_FAILED');
    }
    const bakPath = MemoryStore.candidatePath();
    let fileWrite: FileHandle | null = null;
    try {
      hilog.info(DOMAIN, TAG, 'commit writing candidate bak=%{public}s main=%{public}s bytes=%{public}d',
        bakPath, MemoryStore.storePath, envelopeBytes.length);
      if (fs.accessSync(bakPath)) {
        hilog.info(DOMAIN, TAG, 'commit removing stale bak path=%{public}s', bakPath);
        fs.unlinkSync(bakPath);
      }
      fileWrite = fs.openSync(
        bakPath,
        fs.OpenMode.CREATE | fs.OpenMode.WRITE_ONLY | fs.OpenMode.TRUNC
      ) as FileHandle;
      try {
        const written = fs.writeSync(fileWrite.fd, envelopeBytes.buffer);
        if (typeof written === 'number' && written !== envelopeBytes.length) {
          throw new Error('FILE_COMMIT_FAILED');
        }
        fs.fsyncSync(fileWrite.fd);
      } finally {
        if (fileWrite !== null) {
          const openedFile = fileWrite;
          fileWrite = null;
          fs.closeSync(openedFile.fd);
        }
      }
      hilog.info(DOMAIN, TAG, 'commit renaming bak->main bak=%{public}s main=%{public}s', bakPath, MemoryStore.storePath);
      fs.renameSync(bakPath, MemoryStore.storePath);
      hilog.info(DOMAIN, TAG, 'commit done main=%{public}s bytes=%{public}d', MemoryStore.storePath, envelopeBytes.length);
    } catch (error) {
      throw new Error(`FILE_COMMIT_FAILED:${error instanceof Error ? error.message : error}`);
    }
  }

  private static candidatePath(): string {
    return `${MemoryStore.storePath}.bak`;
  }

  private static parseSnapshot(text: string): Map<string, StoredMemoryPayload> {
    let parsed: Record<string, Object>;
    try {
      parsed = JSON.parse(text) as Record<string, Object>;
    } catch (_error) {
      throw new Error('SNAPSHOT_INVALID');
    }
    if (parsed.version !== 1 || typeof parsed.items !== 'object' || parsed.items === null ||
      Array.isArray(parsed.items)) {
      throw new Error('SNAPSHOT_INVALID');
    }
    const loaded: Map<string, StoredMemoryPayload> = new Map();
    const obj = parsed.items as Record<string, Object>;
    const keys: string[] = Object.keys(obj);
    for (let i = 0; i < keys.length; i++) {
      const k = keys[i];
      const v = obj[k];
      if (typeof v !== 'object' || v === null || Array.isArray(v)) {
        throw new Error('SNAPSHOT_INVALID');
      }
      loaded.set(k, MemoryStore.fromRecord(v as Record<string, Object>));
    }
    return loaded;
  }

  private static serializeSnapshot(items: Map<string, StoredMemoryPayload>): string {
    const snapshot = new MemoryStoreSnapshot();
    const keys: string[] = Array.from(items.keys());
    for (let i = 0; i < keys.length; i++) {
      const k = keys[i];
      const v = items.get(k);
      if (v) {
        snapshot.items[k] = v;
      }
    }
    return JSON.stringify(snapshot);
  }

  private static evictOldestSessionFrom(items: Map<string, StoredMemoryPayload>): boolean {
    const keys: string[] = Array.from(items.keys());
    let oldestKey = '';
    let oldestTime = '';
    for (let i = 0; i < keys.length; i++) {
      const p = items.get(keys[i]);
      if (!p || p.type !== 'session_memory') {
        continue;
      }
      if (oldestKey.length <= 0 || p.createdAt < oldestTime) {
        oldestKey = keys[i];
        oldestTime = p.createdAt;
      }
    }
    if (oldestKey.length <= 0) {
      return false;
    }
    items.delete(oldestKey);
    return true;
  }

  private static ensureDir(dirPath: string): void {
    if (!fs.accessSync(dirPath)) {
      try {
        fs.mkdirSync(dirPath);
      } catch (error) {
        hilog.warn(DOMAIN, TAG, 'mkdir failed: %{public}s',
          error instanceof Error ? error.message : `${error}`);
      }
    }
  }

  private static cloneItems(items: Map<string, StoredMemoryPayload>): Map<string, StoredMemoryPayload> {
    const cloned: Map<string, StoredMemoryPayload> = new Map();
    const keys: string[] = Array.from(items.keys());
    for (let i = 0; i < keys.length; i++) {
      const payload = items.get(keys[i]);
      if (payload) {
        cloned.set(keys[i], MemoryStore.clonePayload(payload));
      }
    }
    return cloned;
  }

  private static clonePayload(payload: StoredMemoryPayload): StoredMemoryPayload {
    const out = new StoredMemoryPayload();
    out.userId = payload.userId;
    out.agentId = payload.agentId;
    out.type = payload.type;
    out.subType = payload.subType;
    out.subSubType = payload.subSubType;
    out.title = payload.title;
    out.content = payload.content;
    out.summarizedTitle = payload.summarizedTitle;
    out.createdAt = payload.createdAt;
    out.updatedAt = payload.updatedAt;
    out.oldMemory = payload.oldMemory;
    out.scenarioId = payload.scenarioId;
    out.toolsJson = payload.toolsJson;
    out.historyJson = payload.historyJson;
    out.sourceTraceJson = payload.sourceTraceJson;
    return out;
  }

  private static fromRecord(record: Record<string, Object>): StoredMemoryPayload {
    const p = new StoredMemoryPayload();
    p.userId = MemoryStore.readString(record, 'userId');
    p.agentId = MemoryStore.readString(record, 'agentId');
    p.type = MemoryStore.readString(record, 'type');
    p.subType = MemoryStore.readString(record, 'subType');
    p.subSubType = MemoryStore.readString(record, 'subSubType');
    p.title = MemoryStore.readString(record, 'title');
    p.content = MemoryStore.readString(record, 'content');
    p.summarizedTitle = MemoryStore.readString(record, 'summarizedTitle');
    p.createdAt = MemoryStore.readString(record, 'createdAt');
    p.updatedAt = MemoryStore.readString(record, 'updatedAt');
    p.oldMemory = MemoryStore.readString(record, 'oldMemory');
    p.scenarioId = MemoryStore.readString(record, 'scenarioId');
    p.toolsJson = MemoryStore.readStringDefault(record, 'toolsJson', '[]');
    p.historyJson = MemoryStore.readStringDefault(record, 'historyJson', '{}');
    p.sourceTraceJson = MemoryStore.readStringDefault(record, 'sourceTraceJson', '[]');
    return p;
  }

  private static payloadToRecord(payload: StoredMemoryPayload): Record<string, Object> {
    const record: Record<string, Object> = {};
    record.userId = payload.userId;
    record.agentId = payload.agentId;
    record.type = payload.type;
    record.subType = payload.subType;
    record.subSubType = payload.subSubType;
    record.title = payload.title;
    record.content = payload.content;
    record.summarizedTitle = payload.summarizedTitle;
    record.createdAt = payload.createdAt;
    record.updatedAt = payload.updatedAt;
    record.oldMemory = payload.oldMemory;
    record.scenarioId = payload.scenarioId;
    record.tools = MemoryStore.parseJsonArray(payload.toolsJson);
    record.history = MemoryStore.parseJsonObject(payload.historyJson);
    record.sourceTrace = MemoryStore.parseJsonArray(payload.sourceTraceJson);
    return record;
  }

  private static readString(record: Record<string, Object>, key: string): string {
    const v = record[key];
    return typeof v === 'string' ? (v as string) : '';
  }

  private static readStringDefault(record: Record<string, Object>, key: string, fallback: string): string {
    const v = record[key];
    return typeof v === 'string' ? (v as string) : fallback;
  }

  private static parseJsonArray(text: string): Object[] {
    if (!text || text.length <= 0) {
      return [];
    }
    try {
      const parsed: Object = JSON.parse(text) as Object;
      return Array.isArray(parsed) ? parsed as Object[] : [];
    } catch (_e) {
      return [];
    }
  }

  private static parseJsonObject(text: string): Record<string, Object> {
    if (!text || text.length <= 0) {
      return {};
    }
    try {
      const parsed: Object = JSON.parse(text) as Object;
      return (typeof parsed === 'object' && parsed !== null && !Array.isArray(parsed)) ?
        parsed as Record<string, Object> : {};
    } catch (_e) {
      return {};
    }
  }

  private static errorCode(error: Object): string {
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
    if (error instanceof Error) {
      const message = error.message;
      const knownCodes: string[] = [
        'ENVELOPE_INVALID',
        'SNAPSHOT_INVALID',
        'PLAINTEXT_STORE_REJECTED',
        'STORAGE_LIMIT_EXCEEDED',
        'FILE_COMMIT_FAILED'
      ];
      for (let i = 0; i < knownCodes.length; i++) {
        if (message.indexOf(knownCodes[i]) >= 0) {
          return knownCodes[i];
        }
      }
    }
    return 'MEMORY_STORE_ERROR';
  }

  private static errorMessage(error: Object): string {
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
