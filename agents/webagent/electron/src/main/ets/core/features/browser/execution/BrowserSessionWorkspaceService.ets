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

import { hilog } from '@kit.PerformanceAnalysisKit';
import util from '@ohos.util';
import fs from '@ohos.file.fs';
import { AppContextHolderService } from '../../../adapter/AppContextHolderService';
import { BrowserBinaryPayloadService } from './BrowserBinaryPayloadService';

const DOMAIN = 0x6020;
const TAG = 'BrowserSessionWorkspace';
const SESSION_ROOT_DIR = 'agent_sessions';
const SCREENSHOT_CACHE_DIR = 'screenshot_cache';

interface FileHandle {
  fd: number;
}

export class BrowserSessionWorkspaceService {
  static resolveSessionDir(sessionKey: string, preferredDir: string = ''): string {
    const context = AppContextHolderService.getContext();
    const safeSessionKey = BrowserSessionWorkspaceService.normalizeSessionKey(sessionKey);
    if (!context || safeSessionKey.length <= 0) {
      return '';
    }
    const filesDir = BrowserSessionWorkspaceService.normalizeDirectoryPath(context.filesDir);
    if (filesDir.length <= 0) {
      return '';
    }
    const rootDir = `${filesDir}/${SESSION_ROOT_DIR}`;
    BrowserSessionWorkspaceService.ensureDir(rootDir);
    const sessionDir = `${rootDir}/${safeSessionKey}`;
    const normalizedPreferred = BrowserSessionWorkspaceService.normalizeDirectoryPath(preferredDir);
    if (normalizedPreferred.length > 0 &&
      BrowserSessionWorkspaceService.isPathWithin(rootDir, normalizedPreferred)) {
      BrowserSessionWorkspaceService.ensureDir(normalizedPreferred);
      return normalizedPreferred;
    }
    if (preferredDir.trim().length > 0) {
      hilog.warn(DOMAIN, TAG, 'invalid preferred session dir, using default: %{public}s', preferredDir);
    }
    BrowserSessionWorkspaceService.ensureDir(sessionDir);
    return sessionDir;
  }

  static writeText(sessionDir: string, relativePath: string, content: string): string {
    const targetPath = BrowserSessionWorkspaceService.resolvePath(sessionDir, relativePath);
    if (targetPath.length <= 0) {
      return '';
    }
    const encoder = new util.TextEncoder();
    BrowserSessionWorkspaceService.writeBytes(targetPath, encoder.encode(content));
    return targetPath;
  }

  static appendText(sessionDir: string, relativePath: string, content: string): string {
    const targetPath = BrowserSessionWorkspaceService.resolvePath(sessionDir, relativePath);
    if (targetPath.length <= 0) {
      return '';
    }
    BrowserSessionWorkspaceService.ensureParentDir(targetPath);
    let file: FileHandle | null = null;
    try {
      file = fs.openSync(
        targetPath,
        fs.OpenMode.CREATE | fs.OpenMode.WRITE_ONLY | fs.OpenMode.APPEND
      ) as FileHandle;
      fs.writeSync(file.fd, content);
    } finally {
      if (file) {
        fs.closeSync(file.fd);
      }
    }
    return targetPath;
  }

  static writeJson(sessionDir: string, relativePath: string, value: Object): string {
    return BrowserSessionWorkspaceService.writeText(sessionDir, relativePath, JSON.stringify(value, null, 2));
  }

  static writeBytesFile(sessionDir: string, relativePath: string, bytes: Uint8Array): string {
    const targetPath = BrowserSessionWorkspaceService.resolvePath(sessionDir, relativePath);
    if (targetPath.length <= 0) {
      return '';
    }
    BrowserSessionWorkspaceService.writeBytes(targetPath, bytes);
    return targetPath;
  }

  static writeBase64File(
    sessionDir: string,
    relativePath: string,
    rawValue: string,
    fallbackMimeType: string = 'application/octet-stream'
  ): string {
    const payload = BrowserBinaryPayloadService.parseBase64Payload(rawValue, fallbackMimeType);
    if (payload.base64Data.length <= 0) {
      return '';
    }
    let nextRelativePath = relativePath.trim();
    if (!BrowserSessionWorkspaceService.hasExplicitExtension(nextRelativePath)) {
      const extension = BrowserBinaryPayloadService.resolveFileExtension(payload.mimeType);
      nextRelativePath = `${nextRelativePath}.${extension}`;
    }
    const bytes = BrowserBinaryPayloadService.decodeBase64(payload.base64Data);
    return BrowserSessionWorkspaceService.writeBytesFile(sessionDir, nextRelativePath, bytes);
  }

  static getFilesDir(): string {
    const context = AppContextHolderService.getContext();
    return context ? context.filesDir : '';
  }

  private static writeBytes(targetPath: string, bytes: Uint8Array): void {
    let file: FileHandle | null = null;
    try {
      BrowserSessionWorkspaceService.ensureParentDir(targetPath);
      file = fs.openSync(
        targetPath,
        fs.OpenMode.CREATE | fs.OpenMode.WRITE_ONLY | fs.OpenMode.TRUNC
      ) as FileHandle;
      fs.writeSync(file.fd, bytes.buffer as ArrayBuffer);
    } catch (error) {
      hilog.error(DOMAIN, TAG, 'writeBytes failed path=%{public}s error=%{public}s',
        targetPath, `${error.message}`);
      throw new Error('workspace_file_write_failed');
    } finally {
      if (file) {
        try {
          fs.closeSync(file.fd);
        } catch (error) {
          hilog.warn(DOMAIN, TAG, 'writeBytes close failed path=%{public}s error=%{public}s',
            targetPath, `${error.message}`);
        }
      }
    }
  }

  private static resolvePath(sessionDir: string, relativePath: string): string {
    const normalizedSessionDir = BrowserSessionWorkspaceService.normalizeDirectoryPath(sessionDir);
    const normalizedRelative = relativePath.trim().replace(/\\/g, '/');
    if (normalizedSessionDir.length <= 0 || normalizedRelative.length <= 0 ||
      !BrowserSessionWorkspaceService.isPathWithin(
        BrowserSessionWorkspaceService.normalizeDirectoryPath(BrowserSessionWorkspaceService.getFilesDir()),
        normalizedSessionDir
      )) {
      hilog.warn(DOMAIN, TAG, 'session dir is outside filesDir: %{public}s', sessionDir);
      return '';
    }
    if (normalizedRelative.indexOf('\0') >= 0 || /%00/i.test(normalizedRelative) ||
      normalizedRelative.indexOf('*') >= 0 || normalizedRelative.indexOf('?') >= 0) {
      hilog.warn(DOMAIN, TAG, 'unsafe path characters blocked: %{public}s', normalizedRelative);
      return '';
    }
    if (normalizedRelative.startsWith('/') || normalizedRelative.indexOf(':') >= 0) {
      hilog.warn(DOMAIN, TAG, 'absolute path is not allowed: %{private}s', normalizedRelative);
      return '';
    }
    const segments = normalizedRelative.split('/').filter((segment) => segment.length > 0);
    const sanitized: string[] = [];
    for (let i = 0; i < segments.length; i++) {
      if (segments[i] === '.' || segments[i] === '..') {
        hilog.warn(DOMAIN, TAG, 'unsafe path segment blocked: %{private}s', normalizedRelative);
        return '';
      }
      sanitized.push(segments[i]);
    }
    if (sanitized.length <= 0) {
      return '';
    }
    return `${normalizedSessionDir}/${sanitized.join('/')}`;
  }

  private static ensureParentDir(targetPath: string): void {
    const lastSlash = targetPath.lastIndexOf('/');
    if (lastSlash <= 0) {
      return;
    }
    const parentDir = targetPath.substring(0, lastSlash);
    BrowserSessionWorkspaceService.ensureDir(parentDir);
  }

  private static ensureDir(dirPath: string): void {
    const normalized = BrowserSessionWorkspaceService.normalizeDirectoryPath(dirPath);
    if (normalized.length <= 0) {
      return;
    }
    const filesDir = BrowserSessionWorkspaceService.normalizeDirectoryPath(
      BrowserSessionWorkspaceService.getFilesDir()
    );
    if (filesDir.length <= 0 || !BrowserSessionWorkspaceService.isPathWithin(filesDir, normalized)) {
      hilog.warn(DOMAIN, TAG, 'skip creating dir outside filesDir: %{public}s', normalized);
      return;
    }
    const segments = normalized.split('/').filter((segment) => segment.length > 0);
    let current = normalized.startsWith('/') ? '/' : '';
    let startIndex = 0;
    if (!normalized.startsWith('/') &&
      segments.length > 0 &&
      /^[A-Za-z]:$/.test(segments[0])) {
      current = segments[0];
      startIndex = 1;
    }
    try {
      for (let i = startIndex; i < segments.length; i++) {
        current = current.length > 1 || !current.endsWith('/') ?
          `${current}${current.length > 0 && !current.endsWith('/') ? '/' : ''}${segments[i]}` :
          `${current}${segments[i]}`;
        if (!fs.accessSync(current)) {
          fs.mkdirSync(current);
        }
      }
    } catch (error) {
      hilog.error(DOMAIN, TAG, 'ensureDir failed path=%{public}s error=%{public}s',
        current, `${error.message}`);
      throw new Error('workspace_directory_unavailable');
    }
  }

  private static normalizeSessionKey(sessionKey: string): string {
    const normalized = sessionKey.trim();
    if (normalized.length <= 0 || !/^[A-Za-z0-9_-]+$/.test(normalized)) {
      hilog.warn(DOMAIN, TAG, 'invalid session key rejected');
      return '';
    }
    return normalized;
  }

  private static normalizeDirectoryPath(path: string): string {
    const normalized = path.trim().replace(/\\/g, '/');
    if (normalized.length <= 0 || normalized.indexOf('\0') >= 0 || /%00/i.test(normalized)) {
      return '';
    }
    const segments = normalized.split('/');
    const safeSegments: string[] = [];
    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];
      if (segment.length <= 0) {
        continue;
      }
      if (segment === '.' || segment === '..' || segment.indexOf('*') >= 0 || segment.indexOf('?') >= 0) {
        return '';
      }
      safeSegments.push(segment);
    }
    if (safeSegments.length <= 0) {
      return '';
    }
    const prefix = normalized.startsWith('/') ? '/' : '';
    return `${prefix}${safeSegments.join('/')}`;
  }

  private static isPathWithin(rootDir: string, candidatePath: string): boolean {
    if (rootDir.length <= 0 || candidatePath.length <= 0) {
      return false;
    }
    return candidatePath === rootDir || candidatePath.startsWith(`${rootDir}/`);
  }

  private static hasExplicitExtension(relativePath: string): boolean {
    const lastSlash = relativePath.lastIndexOf('/');
    const fileName = lastSlash >= 0 ? relativePath.substring(lastSlash + 1) : relativePath;
    return fileName.indexOf('.') >= 0;
  }

  static getScreenshotCacheDir(): string {
    const context = AppContextHolderService.getContext();
    if (!context) {
      return '';
    }
    const cacheDir = `${context.filesDir}/${SCREENSHOT_CACHE_DIR}`;
    BrowserSessionWorkspaceService.ensureDir(cacheDir);
    return cacheDir;
  }

  static clearDir(dir: string): void {
    const normalizedDir = BrowserSessionWorkspaceService.normalizeDirectoryPath(dir);
    const filesDir = BrowserSessionWorkspaceService.normalizeDirectoryPath(
      BrowserSessionWorkspaceService.getFilesDir()
    );
    const screenshotCacheDir = filesDir.length > 0 ?
      `${filesDir}/${SCREENSHOT_CACHE_DIR}` : '';
    if (normalizedDir.length <= 0 || normalizedDir !== screenshotCacheDir) {
      hilog.warn(DOMAIN, TAG, 'skip clearing unexpected directory: %{public}s', dir);
      return;
    }
    try {
      if (!fs.accessSync(normalizedDir)) {
        return;
      }
      const files = fs.listFileSync(normalizedDir);
      for (let i = 0; i < files.length; i++) {
        const filePath = `${normalizedDir}/${files[i]}`;
        try {
          fs.unlinkSync(filePath);
        } catch (e) {
          hilog.warn(DOMAIN, TAG, 'clearDir unlinkSync failed: %{public}s', e.message);
        }
      }
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'clearDir failed: %{public}s', e.message);
    }
  }
}
