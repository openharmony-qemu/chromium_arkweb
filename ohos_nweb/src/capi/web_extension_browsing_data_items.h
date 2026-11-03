/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef OHOS_NWEB_EX_BROWSER_WEB_EXTENSION_BROWSING_DATA_ITEMS_H_
#define OHOS_NWEB_EX_BROWSER_WEB_EXTENSION_BROWSING_DATA_ITEMS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct NWebExtensionBrowsingDataOriginTypes {
  std::optional<bool> extension;
  std::optional<bool> protectedWeb;
  std::optional<bool> unprotectedWeb;
};

struct NWebExtensionBrowsingDataRemovalOptions {
  std::optional<std::vector<std::string>> excludeOrigins;
  std::optional<std::vector<std::string>> origins;
  std::optional<int64_t> startTime;
  std::optional<NWebExtensionBrowsingDataOriginTypes> originTypes;
};

struct NWebExtensionBrowsingDataQueryOptions {
  std::optional<std::string> contextType;
  std::optional<bool> includeIncognitoInfo;
};

struct NWebExtensionBrowsingDataSettings {
  std::optional<bool> history;
  std::optional<bool> cache;
  std::optional<bool> siteData;
  std::optional<bool> passwords;
  std::optional<bool> formData;
  std::optional<bool> siteSettings;
  std::optional<bool> downloads;
  std::optional<bool> hostedAppsData;
  std::optional<bool> tabs;
  std::optional<double> sinceTime;
};

#endif  // OHOS_NWEB_EX_BROWSER_WEB_EXTENSION_BROWSING_DATA_ITEMS_H_
