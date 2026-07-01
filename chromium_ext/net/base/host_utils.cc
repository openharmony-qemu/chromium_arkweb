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

#include "arkweb/chromium_ext/net/base/host_utils.h"

#include <string_view>
#include "base/strings/string_util.h"
#include "net/base/ip_address.h"
#include "net/base/url_util.h"

namespace net {

// static
bool HostUtils::IsValidIP(const std::string& host) {
  net::IPAddress ip_address;
  return ip_address.AssignFromIPLiteral(host);
}

// static
bool HostUtils::IsValidDomain(const std::string& host) {
  if (IsValidIP(host)) {
    return false;
  }

  if (!net::IsCanonicalizedHostCompliant(host)) {
    return false;
  }

  std::string_view host_view(host);
  if (base::EndsWith(host_view, ".", base::CompareCase::SENSITIVE)) {
    host_view.remove_suffix(1);
  }
  return host_view.find('.') != std::string_view::npos;
}

// static
bool HostUtils::IsHostMatchInList(const std::string& host,
                                  const std::vector<std::string>& host_list) {
  if (host.empty() || host_list.empty()) {
    return false;
  }

  for (const auto& pattern : host_list) {
    if (host == pattern) {
      return true;
    }

    if (IsValidDomain(host) && host.length() > pattern.length() &&
        host[host.length() - pattern.length() - 1] == '.' &&
        host.compare(host.length() - pattern.length(),
                     pattern.length(), pattern) == 0) {
      return true;
    }
  }

  return false;
}

}  // namespace net
