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

#ifndef OHOS_NWEB_SAFE_BROWSING_DETECTION_RESULT_ITEM_H_
#define OHOS_NWEB_SAFE_BROWSING_DETECTION_RESULT_ITEM_H_

#include <string>

struct SafeBrowsingDetectionResult {
  int req_id{-1};
  int code{-1};
  int policy{-1};
  std::string mapping_type;
  std::string desensitized_url;
  bool is_content_check{false};
  std::string access_url;
  std::string content_check_parameters;
  std::string uuid;
  std::string appname;
  bool is_from_af_detect{false};
  bool is_minor_protection{false};

  SafeBrowsingDetectionResult(
      int code,
      int policy,
      const std::string& mapping_type,
      const std::string& desensitized_url,
      bool is_content_check,
      const std::string& access_url,
      const std::string& content_check_parameters,
      bool is_from_af_detect = false,
      bool is_minor_protection = false
  ) : 
      req_id(-1),
      code(code),
      policy(policy),
      mapping_type(mapping_type),
      desensitized_url(desensitized_url),
      is_content_check(is_content_check),
      access_url(access_url),
      content_check_parameters(content_check_parameters),
      uuid(""), 
      appname(""),
      is_from_af_detect(is_from_af_detect),
      is_minor_protection(is_minor_protection)
  {}

  SafeBrowsingDetectionResult() : 
      mapping_type(""), 
      desensitized_url(""), 
      access_url(""), 
      content_check_parameters(""), 
      uuid(""), 
      appname("")
  {}
};

#endif  // OHOS_NWEB_SAFE_BROWSING_DETECTION_RESULT_ITEM_H_
