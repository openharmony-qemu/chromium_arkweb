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

import { BusinessError } from '@kit.BasicServicesKit';
import { buffer } from '@kit.ArkTS';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { common } from '@kit.AbilityKit';
import { AppContextHolderService } from '../../adapter/AppContextHolderService';

const TAG: string = 'NavigateRawFile';
const DOMAIN = 0x6020;
const NAVIGATE_SITES_PATH: string = 'navigate/navigate_sites.json';

type NavigateSitesContent = Record<string, string>;

export class RawFileUtil {
  static readJsonSync<T>(context: common.Context, filePath: string): T | null {
    try {
      const data: Uint8Array = context.resourceManager.getRawFileContentSync(filePath);
      const jsonString: string = buffer.from(data.buffer).toString();
      return JSON.parse(jsonString) as T;
    } catch (error) {
      const err: BusinessError = error as BusinessError;
      hilog.error(DOMAIN, TAG, 'read rawfile failed code=%{private}d message=%{private}s',
        err?.code, err?.message);
      return null;
    }
  }

  static getNavigateSites(): NavigateSitesContent {
    const context = AppContextHolderService.getContext();
    if (context === null) {
      hilog.error(DOMAIN, TAG, 'read navigate sites failed: context is null');
      return {};
    }
    return RawFileUtil.readJsonSync<NavigateSitesContent>(context, NAVIGATE_SITES_PATH) ?? {};
  }
}
