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

// 浏览器自动化操控 Agent 对外入口
// 样例只接受调试环境中的本机 WebSocket JSON 连接。
import { BrowserExecutionHostService } from '../execution/BrowserExecutionHostService';
import { BrowserExecutionRequest } from '../execution/BrowserExecutionTypes';
import { BrowserRuntimeState, BrowserPhase, BrowserTransportConstants } from '../../../shared/domain/BrowserAutomationTypes';
import { BrowserTransportHostService } from '../transport/BrowserTransportHostService';
import { BrowserAgentEventCallbacks } from '../agent/ports/AgentContext';
import { vConfig } from '../agent/VersionConfig';
import BuildProfile from 'BuildProfile';
import { hilog } from '@kit.PerformanceAnalysisKit';
import fs from '@ohos.file.fs';
import { AppContextHolderService } from '../../../adapter/AppContextHolderService';
const DOMAIN = 0x6020;
const TAG = 'BrowserRunAutomation';

export class BrowserAutomationFacadeService {
  private static runningTask: boolean = false;

  /**
   * 启动浏览器自动化任务。
   * 调试客户端通过本机 WebSocket 连接到样例通道。
   *
   * @param task 用户任务描述
   * @param events 事件、LLM 与安全检测回调
   * @returns 最终运行时状态
   */
  static async runAutomation(
    task: string,
    sessionKey: string = '',
    sessionDir: string = '',
    needClearBrowser: boolean = false,
    events: BrowserAgentEventCallbacks | null = null,
    cacheScreenshot: boolean = true,
    modal: string = 'vl',
    groupName: string = '',
  ): Promise<BrowserRuntimeState> {
    const calledAt = Date.now();
    hilog.info(DOMAIN, TAG, 'runAutomation called! %{public}d', Date.now());
    if (groupName === '') {
      groupName = task;
    }
    if (BrowserAutomationFacadeService.runningTask) {
      const ret = new BrowserRuntimeState();
      ret.errorMessage = 'browser automation is already running';
      ret.phase = BrowserPhase.FAILED;
      return ret;
    }
    if (modal !== 'vl' && modal !== 'text') {
      const ret = new BrowserRuntimeState();
      ret.errorMessage = `modal of LLM model should be "vl" or "text", got ${modal}`;
      ret.phase = BrowserPhase.FAILED;
      return ret;
    }
    vConfig.setTextOnly(modal === 'text');

    if (events?.checkText && !await events.checkText(groupName)) {
      groupName = '浏览器任务';
    }

    if (!BuildProfile.DEBUG) {
      /*
      release模式下，不往agent_sessions下写东西
        cache          debug                                          release
        true      往agent_sessions写path，往cache路径写图片        无agent_sessions，往cache写图片
        false           往agent_sessions写路径和图片                      不允许
       */
      hilog.warn(DOMAIN, TAG, 'In release mode, should use cache for screenshot.');
      cacheScreenshot = true;
    }

    BrowserAutomationFacadeService.runningTask = true;
    try {
      const request = new BrowserExecutionRequest(task);
      request.sessionKey = sessionKey;
      request.sessionDir = sessionDir;
      request.needClearBrowser = needClearBrowser;
      request.events = events;
      request.cacheScreenshot = cacheScreenshot;
      request.groupName = groupName;
      request.calledAt = calledAt;
      return await BrowserExecutionHostService.run(request);
    } finally {
      hilog.info(DOMAIN, TAG, 'runAutomation ready for shutdown');
      BrowserAutomationFacadeService.shutdown();
      BrowserAutomationFacadeService.runningTask = false;
      AppStorage.set<boolean>(BrowserTransportConstants.HAS_REQUESTED_TASK_STOP, false);
    }
  }

  /**
   * 停止当前自动化任务
   */
  static shutdown(): void {
    BrowserExecutionHostService.requestStop();
    BrowserTransportHostService.shutdown();
  }

  /**
   * 检查浏览器插件是否已连接。
   */
  static isExtensionConnected(): boolean {
    return BrowserTransportHostService.isConnected();
  }

  /**
   * 检查样例 WebSocket 客户端是否已连接。
   */
  static async checkExtensionConnected(browserMode: string): Promise<boolean> {
    void browserMode;
    return BrowserTransportHostService.isConnected();
  }

  static isTaskRunning(): boolean {
    return BrowserAutomationFacadeService.runningTask;
  }

  /**
   * 删除应用 files 目录下由浏览器自动化功能使用的本地数据目录。
   *
   * 不存在的目录视为已清理成功。任一目录删除失败时返回 false，
   * 同时继续尝试删除其余目录。
   */
  static clearLocalAppCacheDirectories(): boolean {
    const localAppDirectories: string[] = ['memory_local', 'models', 'screenshot_cache'];
    const context = AppContextHolderService.getContext();
    if (!context || context.filesDir.length <= 0) {
      hilog.error(DOMAIN, TAG, "clearLocalAppCacheDirectories: current application's files dir unavailable");
      return false;
    }

    let succeeded = true;
    for (let i = 0; i < localAppDirectories.length; i++) {
      const dirPath = `${context.filesDir}/${localAppDirectories[i]}`;
      try {
        if (!fs.accessSync(dirPath)) {
          hilog.info(DOMAIN, TAG, 'clearLocalAppCacheDirectories: not found: %{private}s', dirPath);
          continue;
        }
        hilog.info(DOMAIN, TAG, 'clearLocalAppCacheDirectories: found: %{private}s', dirPath);
        fs.rmdirSync(dirPath);
        hilog.info(DOMAIN, TAG, 'clearLocalAppCacheDirectories: removed: %{private}s', dirPath);
      } catch (error) {
        succeeded = false;
        hilog.error(DOMAIN, TAG, 'clearLocalAppCacheDirectories: failed path=%{private}s error=%{private}s',
          dirPath, error instanceof Error ? error.message : `${error}`);
      }
    }
    return succeeded;
  }
}
