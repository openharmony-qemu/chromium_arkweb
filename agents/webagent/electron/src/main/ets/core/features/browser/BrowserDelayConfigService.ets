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

// 浏览器自动化延迟配置服务
// 管理各阶段等待时间，确保浏览器操作的时序正确

export class BrowserDelayConfig {
  public postReadyDelayMs: number = 0;
  public initialPerceptionDelayMs: number = 0;
  public postActionDelayMs: number = 0;
  public postLoadedDelayMs: number = 0;
  public toolStepDelayMs: number = 0;
  public initialWaitRetryCount: number = 0;
  public initialWaitRetryDelayMs: number = 0;
}

export class BrowserDelayConfigService {
  private static readonly config: BrowserDelayConfig = BrowserDelayConfigService.buildDefaultConfig();

  static getConfig(): BrowserDelayConfig {
    const nextConfig = new BrowserDelayConfig();
    nextConfig.postReadyDelayMs = BrowserDelayConfigService.config.postReadyDelayMs;
    nextConfig.initialPerceptionDelayMs = BrowserDelayConfigService.config.initialPerceptionDelayMs;
    nextConfig.postActionDelayMs = BrowserDelayConfigService.config.postActionDelayMs;
    nextConfig.postLoadedDelayMs = BrowserDelayConfigService.config.postLoadedDelayMs;
    nextConfig.toolStepDelayMs = BrowserDelayConfigService.config.toolStepDelayMs;
    nextConfig.initialWaitRetryCount = BrowserDelayConfigService.config.initialWaitRetryCount;
    nextConfig.initialWaitRetryDelayMs = BrowserDelayConfigService.config.initialWaitRetryDelayMs;
    return nextConfig;
  }

  static getPostReadyDelayMs(): number {
    return BrowserDelayConfigService.config.postReadyDelayMs;
  }

  static getInitialPerceptionDelayMs(): number {
    return BrowserDelayConfigService.config.initialPerceptionDelayMs;
  }

  static getPostActionDelayMs(): number {
    return BrowserDelayConfigService.config.postActionDelayMs;
  }

  static getPostLoadedDelayMs(): number {
    return BrowserDelayConfigService.config.postLoadedDelayMs;
  }

  static getToolStepDelayMs(): number {
    return BrowserDelayConfigService.config.toolStepDelayMs;
  }

  static getInitialWaitRetryCount(): number {
    return BrowserDelayConfigService.config.initialWaitRetryCount;
  }

  static getInitialWaitRetryDelayMs(): number {
    return BrowserDelayConfigService.config.initialWaitRetryDelayMs;
  }

  static waitInitialPerceptionDelay(): Promise<void> {
    return BrowserDelayConfigService.wait(BrowserDelayConfigService.getInitialPerceptionDelayMs());
  }

  static waitPostReadyDelay(): Promise<void> {
    return BrowserDelayConfigService.wait(BrowserDelayConfigService.getPostReadyDelayMs());
  }

  static waitPostActionDelay(): Promise<void> {
    return BrowserDelayConfigService.wait(BrowserDelayConfigService.getPostActionDelayMs());
  }

  static waitPostLoadedDelay(): Promise<void> {
    return BrowserDelayConfigService.wait(BrowserDelayConfigService.getPostLoadedDelayMs());
  }

  static waitToolStepDelay(): Promise<void> {
    return BrowserDelayConfigService.wait(BrowserDelayConfigService.getToolStepDelayMs());
  }

  static waitInitialWaitRetryDelay(): Promise<void> {
    return BrowserDelayConfigService.wait(BrowserDelayConfigService.getInitialWaitRetryDelayMs());
  }

  private static buildDefaultConfig(): BrowserDelayConfig {
    const config = new BrowserDelayConfig();
    config.postReadyDelayMs = 2000;
    config.initialPerceptionDelayMs = 1000;
    config.postActionDelayMs = 1500;
    config.postLoadedDelayMs = 500;
    config.toolStepDelayMs = 800;
    config.initialWaitRetryCount = 3;
    config.initialWaitRetryDelayMs = 2000;
    return config;
  }

  private static wait(delayMs: number): Promise<void> {
    if (delayMs <= 0) {
      return Promise.resolve();
    }
    return new Promise<void>((resolve: () => void): void => {
      setTimeout((): void => {
        resolve();
      }, delayMs);
    });
  }
}
