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

import { BrowserRuntimeAccessor } from './BrowserRuntimeBridge';
import { AgentWorkspacePort } from '../ports/AgentWorkspacePort';
import { BrowserSessionWorkspaceService } from '../../execution/BrowserSessionWorkspaceService';
import { hilog } from '@kit.PerformanceAnalysisKit';
import BuildProfile from 'BuildProfile';
const DOMAIN = 0x6020;
const TAG = 'BrowserAgentGraph';

export class BrowserWorkspaceAdapter implements AgentWorkspacePort {
  private readonly runtimeAccessor: BrowserRuntimeAccessor;

  constructor(runtimeAccessor: BrowserRuntimeAccessor) {
    this.runtimeAccessor = runtimeAccessor;
  }

  getSessionDir(): string {
    return this.runtimeAccessor.getRuntime().sessionDir;
  }

  getScreenshotDir(): string {
    const runtime = this.runtimeAccessor.getRuntime();
    if (runtime.cacheScreenshot && runtime.screenshotCacheDir.length > 0) {
      return runtime.screenshotCacheDir;
    }
    return runtime.sessionDir;
  }

  async writeText(relativePath: string, content: string): Promise<string> {
    return BrowserSessionWorkspaceService.writeText(this.getSessionDir(), relativePath, content);
  }

  async appendText(relativePath: string, content: string): Promise<string> {
    return BrowserSessionWorkspaceService.appendText(this.getSessionDir(), relativePath, content);
  }

  async writeJson(relativePath: string, value: Object): Promise<string> {
    return BrowserSessionWorkspaceService.writeJson(this.getSessionDir(), relativePath, value);
  }

  async writeBase64File(relativePath: string, rawValue: string, fallbackMimeType: string = 'application/octet-stream'): Promise<string> {
    const screenshotDir = this.getScreenshotDir();
    return BrowserSessionWorkspaceService.writeBase64File(
      screenshotDir,
      relativePath,
      rawValue,
      fallbackMimeType
    );
  }

  isDebug(): boolean {
    if (BuildProfile.DEBUG) {
      return true;
    }
    return false;
  }
}
