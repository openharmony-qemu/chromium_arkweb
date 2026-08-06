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
 
#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_OHOS_NAVIGATION_ENTRY_SAFE_BROWSING_DATA_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_OHOS_NAVIGATION_ENTRY_SAFE_BROWSING_DATA_H_
 
#include "ohos_sb_blocking_page_info.h"
 
#include "base/containers/flat_map.h"
#include "content/public/browser/navigation_entry.h"
 
namespace ohos_safe_browsing {
 
class NavigationEntrySafeBrowsingData : public base::SupportsUserData::Data {
 public:
  NavigationEntrySafeBrowsingData() = default;
  ~NavigationEntrySafeBrowsingData() = default;
 
  NavigationEntrySafeBrowsingData(const NavigationEntrySafeBrowsingData&) = delete;
  NavigationEntrySafeBrowsingData& operator=(const NavigationEntrySafeBrowsingData&) = delete;
 
  static NavigationEntrySafeBrowsingData* GetOrCreate(content::NavigationEntry* entry);
 
  bool is_js_redirect() const { return is_js_redirect_; }
  void set_is_js_redirect(bool is_js_redirect) { is_js_redirect_ = is_js_redirect; }
 
  void AppendRedirectChain(const std::vector<GURL>& redirect_chain);

  bool UrlInRedirectChain(const GURL& url);

  void ClearSbBlockingPageInfoCache();

  std::vector<GURL> GetRedirectChain() { return redirect_chain_; }
  void SetRedirectChain(const std::vector<GURL>& redirect_chain) {
    redirect_chain_ = redirect_chain;
  }

  std::vector<GURL> GetStoredRedirectChain() { return stored_redirect_chain_; }
  void SetStoredRedirectChain(const std::vector<GURL>& redirect_chain) {
    stored_redirect_chain_ = redirect_chain;
  }

  const OhosSbBlockingPageInfo& GetHighestThreatInChain() const {
     return highest_blocking_page_info_;
  }
  void SetHighestThreatInChain(const OhosSbBlockingPageInfo& info);

  const OhosSbBlockingPageInfo& GetPreviouslyShownBlockingPageInfo() const {
     return previously_shown_blocking_page_info_;
  }
  void SetPreviouslyShownBlockingPageInfo(const OhosSbBlockingPageInfo& info) {
    previously_shown_blocking_page_info_ = info;
  }

  void AddSbBlockingPageInfo(const OhosSbBlockingPageInfo& info);

  const base::flat_map<GURL, OhosSbBlockingPageInfo>& GetSbBlockingPageInfoCache() const {
    return ohos_sb_blocking_page_info_cache_;
  }
  void SetSbBlockingPageInfoCache(
      base::flat_map<GURL, OhosSbBlockingPageInfo> ohos_sb_blocking_page_info_cache) {
    ohos_sb_blocking_page_info_cache_ = ohos_sb_blocking_page_info_cache;
  }
 
  OhosSbBlockingPageInfo GetBlockingPageInfo(const GURL& url) const;
 
 private:
  static const int kUserDataKey;
 
  bool is_js_redirect_{false};
  OhosSbBlockingPageInfo highest_blocking_page_info_;
  OhosSbBlockingPageInfo previously_shown_blocking_page_info_;
  std::vector<GURL> redirect_chain_;
  std::vector<GURL> stored_redirect_chain_;
  base::flat_map<GURL, OhosSbBlockingPageInfo> ohos_sb_blocking_page_info_cache_{};
};
 
}  // namespace ohos_safe_browsing
 
#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_OHOS_NAVIGATION_ENTRY_SAFE_BROWSING_DATA_H_