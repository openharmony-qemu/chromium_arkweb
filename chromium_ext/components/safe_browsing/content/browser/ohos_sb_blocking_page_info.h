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
 
#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_OHOS_SB_BLOCKING_PAGE_INFO_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_OHOS_SB_BLOCKING_PAGE_INFO_H_
 
#include "ohos_sb_threat_type.h"
#include "url/gurl.h"
#include "arkweb/ohos_nweb/src/capi/nweb_safe_browsing_detection_result_item.h"
 
namespace ohos_safe_browsing {
struct OhosSbBlockingPageInfo {
  OhosSbBlockingPageInfo() = default;
 
  OhosSbBlockingPageInfo(
      const GURL& url,
      OHSBThreatType block_type,
      int hw_code,
      int policy,
      OHSBPolicyLevel policy_level,
      const GURL& redirect_url)
      : info_url(url),
        info_hw_code(hw_code),
        info_block_type(block_type),
        info_original_block_type(block_type),
        info_policy(policy),
        info_policy_level(policy_level),
        info_redirect_url(redirect_url) {}
 
  ~OhosSbBlockingPageInfo() = default;
 
  bool is_valid() {
    return info_url.is_valid() && !info_url.is_empty();
  }
 
  int req_id{-1};
  GURL info_url = GURL("");
  int info_hw_code{-1};
  std::string mapping_type;
  OHSBThreatType info_block_type = OHSBThreatType::THREAT_DEFAULT;
  OHSBThreatType info_original_block_type = OHSBThreatType::THREAT_DEFAULT;
  int info_policy{-1};
  OHSBPolicyLevel info_policy_level = OHSBPolicyLevel::POLICY_LEVEL_DEFAULT;
  GURL info_redirect_url = GURL("");
  std::string desensitized_url;
  bool is_content_check{false};
  std::string content_check_parameters;
  std::string uuid;
  std::string appname;
  bool is_from_af_detect{false};
  bool is_minor_protection{false};
};
}
#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_OHOS_SB_BLOCKING_PAGE_INFO_H_