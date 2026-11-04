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
 
#include "ohos_navigation_entry_safe_browsing_data.h"

namespace ohos_safe_browsing {
 
const int NavigationEntrySafeBrowsingData::kUserDataKey = 0;
 
// static
NavigationEntrySafeBrowsingData* NavigationEntrySafeBrowsingData::GetOrCreate(content::NavigationEntry* entry) {
  auto* data = static_cast<NavigationEntrySafeBrowsingData*>(
      entry->GetUserData(&kUserDataKey));
  if (data) {
    return data;
  }

  auto data_object = std::make_unique<NavigationEntrySafeBrowsingData>();
  data = data_object.get();
  entry->SetUserData(&kUserDataKey, std::move(data_object));
  return data;
}

void NavigationEntrySafeBrowsingData::AppendRedirectChain(
    const std::vector<GURL>& redirect_chain) {
  for (auto url : redirect_chain) {
    if (std::find(redirect_chain_.begin(), redirect_chain_.end(), url) == redirect_chain_.end()) {
      redirect_chain_.emplace_back(url);
    }
  }
}

bool NavigationEntrySafeBrowsingData::UrlInRedirectChain(const GURL& url) {
  if (redirect_chain_.empty()) {
    return false;
  }

  auto it = std::find(redirect_chain_.begin(), redirect_chain_.end(), url);
  return it != redirect_chain_.end();
}

void NavigationEntrySafeBrowsingData::SetHighestThreatInChain(const OhosSbBlockingPageInfo& info) {
  if (info.info_policy_level > highest_blocking_page_info_.info_policy_level) {
    highest_blocking_page_info_ = info;
  }
}

void NavigationEntrySafeBrowsingData::ClearSbBlockingPageInfoCache() {
  ohos_sb_blocking_page_info_cache_.clear();
  highest_blocking_page_info_ = OhosSbBlockingPageInfo();
}
 
void NavigationEntrySafeBrowsingData::AddSbBlockingPageInfo(const OhosSbBlockingPageInfo& info) {
  // Check if URL already exists in results, update if so.
  ohos_sb_blocking_page_info_cache_[info.info_url] = info;
  if (info.info_policy_level > highest_blocking_page_info_.info_policy_level) {
    highest_blocking_page_info_ = info;
  }
}

OhosSbBlockingPageInfo NavigationEntrySafeBrowsingData::GetBlockingPageInfo(const GURL& url) const {
  auto it = ohos_sb_blocking_page_info_cache_.find(url);
  if (it != ohos_sb_blocking_page_info_cache_.end()) {
    return it->second;
  }
  return OhosSbBlockingPageInfo();
}

void NavigationEntrySafeBrowsingData::SetSbBlockingPageInfoCache(
    base::flat_map<GURL, OhosSbBlockingPageInfo> ohos_sb_blocking_page_info_cache) {
  ohos_sb_blocking_page_info_cache_ = ohos_sb_blocking_page_info_cache;
  highest_blocking_page_info_ = OhosSbBlockingPageInfo();
  for (const auto& [url, info] : ohos_sb_blocking_page_info_cache_) {
    if (info.info_policy_level > highest_blocking_page_info_.info_policy_level) {
      highest_blocking_page_info_ = info;
    }
  }
}
} // ohos_safe_browsing