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

import { NavigateConfigPort } from '../ports/NavigateConfigPort';
import { RawFileUtil } from '../../../../platform/llm/RawFileUtil';
import { hilog } from '@kit.PerformanceAnalysisKit';
const DOMAIN = 0x6020;
const TAG = 'NavigateConfig';

export class NavigateConfigAdapter implements NavigateConfigPort {
  private sites: Record<string, string> | null = null;
  
  private loadSites(): void {
    if (this.sites === null) {
      this.sites = RawFileUtil.getNavigateSites();
    }
  }
  
  getSiteUrl(website: string): string {
    this.loadSites();
    const url = this.sites?.[website];
    if (!url) {
      hilog.error(DOMAIN, TAG, `Cannot find website ${website} from config file.`);
      return '';
    }
    return url;
  }
}
