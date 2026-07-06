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

#ifndef ARKWEB_NWEB_EX_NET_BASE_HOST_UTILS_H_
#define ARKWEB_NWEB_EX_NET_BASE_HOST_UTILS_H_

#include <string>
#include <vector>
#include "base/component_export.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT HostUtils {
 public:
  // Validate whether the host name is a valid IP address
  static bool IsValidIP(const std::string& host);

  // Validate whether the host name is a valid domain
  static bool IsValidDomain(const std::string& host);

  // Check if the host name matches the specified host list
  // Supports exact match, IPv4 address match and subdomain match
  static bool IsHostMatchInList(const std::string& host,
                                const std::vector<std::string>& host_list);
};

}  // namespace net

#endif  // ARKWEB_NWEB_EX_NET_BASE_HOST_UTILS_H_
