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

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HostUtilsTest, IsValidIP_ValidIpv4) {
  EXPECT_TRUE(HostUtils::IsValidIP("192.168.1.1"));
}

TEST(HostUtilsTest, IsValidIP_ValidIpv6) {
  EXPECT_TRUE(HostUtils::IsValidIP("::1"));
}

TEST(HostUtilsTest, IsValidIP_Invalid) {
  EXPECT_FALSE(HostUtils::IsValidIP("not-an-ip"));
}

TEST(HostUtilsTest, IsValidIP_Empty) {
  EXPECT_FALSE(HostUtils::IsValidIP(""));
}

TEST(HostUtilsTest, IsValidDomain_IsIpReturnsFalse) {
  EXPECT_FALSE(HostUtils::IsValidDomain("192.168.1.1"));
}

TEST(HostUtilsTest, IsValidDomain_InvalidCharReturnsFalse) {
  // Space is not allowed in canonical host names.
  EXPECT_FALSE(HostUtils::IsValidDomain("bad host.com"));
}

TEST(HostUtilsTest, IsValidDomain_UnderscoreAllowed) {
  // IsCanonicalizedHostCompliant explicitly allows '_'.
  EXPECT_TRUE(HostUtils::IsValidDomain("bad_host.com"));
}

TEST(HostUtilsTest, IsValidDomain_SimpleDomain) {
  EXPECT_TRUE(HostUtils::IsValidDomain("example.com"));
}

TEST(HostUtilsTest, IsValidDomain_Subdomain) {
  EXPECT_TRUE(HostUtils::IsValidDomain("sub.example.com"));
}

TEST(HostUtilsTest, IsValidDomain_NoDotReturnsFalse) {
  // No '.' in host after canonicalization -> false.
  EXPECT_FALSE(HostUtils::IsValidDomain("localhost"));
}

TEST(HostUtilsTest, IsHostMatchInList_EmptyHost) {
  std::vector<std::string> list = {"example.com"};
  EXPECT_FALSE(HostUtils::IsHostMatchInList("", list));
}

TEST(HostUtilsTest, IsHostMatchInList_EmptyList) {
  std::vector<std::string> list;
  EXPECT_FALSE(HostUtils::IsHostMatchInList("example.com", list));
}

TEST(HostUtilsTest, IsHostMatchInList_ExactMatch) {
  std::vector<std::string> list = {"example.com", "foo.org"};
  EXPECT_TRUE(HostUtils::IsHostMatchInList("example.com", list));
}

TEST(HostUtilsTest, IsHostMatchInList_SubdomainMatch) {
  std::vector<std::string> list = {"example.com"};
  EXPECT_TRUE(HostUtils::IsHostMatchInList("sub.example.com", list));
}

TEST(HostUtilsTest, IsHostMatchInList_MultiLevelSubdomainMatch) {
  std::vector<std::string> list = {"example.com"};
  EXPECT_TRUE(HostUtils::IsHostMatchInList("a.b.example.com", list));
}

TEST(HostUtilsTest, IsHostMatchInList_NoMatch) {
  std::vector<std::string> list = {"example.com"};
  EXPECT_FALSE(HostUtils::IsHostMatchInList("other.org", list));
}

TEST(HostUtilsTest, IsHostMatchInList_SimilarSuffixNoMatch) {
  // host is longer than pattern and ends with pattern, but the char before the
  // pattern is not '.', so it is NOT a subdomain match.
  std::vector<std::string> list = {"example.com"};
  EXPECT_FALSE(HostUtils::IsHostMatchInList("notexample.com", list));
}

TEST(HostUtilsTest, IsHostMatchInList_IpHostNoMatch) {
  // host is an IP, IsValidDomain returns false so the subdomain branch is
  // skipped; falls through to false.
  std::vector<std::string> list = {"192.168.1.1"};
  EXPECT_TRUE(HostUtils::IsHostMatchInList("192.168.1.1", list));
}

TEST(HostUtilsTest, IsHostMatchInList_IpHostSubdomainSkipped) {
  // host is an IP, IsValidDomain returns false so subdomain branch is skipped.
  std::vector<std::string> list = {"1.1"};
  EXPECT_FALSE(HostUtils::IsHostMatchInList("192.168.1.1", list));
}

TEST(HostUtilsTest, IsHostMatchInList_SecondPatternMatches) {
  std::vector<std::string> list = {"first.org", "target.com"};
  EXPECT_TRUE(HostUtils::IsHostMatchInList("sub.target.com", list));
}

TEST(HostUtilsTest, IsHostMatchInList_HostShorterThanPattern) {
  // host length <= pattern length -> the subdomain branch is skipped.
  std::vector<std::string> list = {"longer.example.com"};
  EXPECT_FALSE(HostUtils::IsHostMatchInList("example.com", list));
}

TEST(HostUtilsTest, IsHostMatchInList_TrailingDotHostNoMatch) {
  // "sub.example.com." is a valid domain (trailing dot stripped), but the
  // subdomain check indexes host[host.length()-pattern.length()-1] which lands
  // on 'e' (not '.') because of the trailing dot, so it does NOT match.
  std::vector<std::string> list = {"example.com"};
  EXPECT_FALSE(HostUtils::IsHostMatchInList("sub.example.com.", list));
}

TEST(HostUtilsTest, IsValidDomain_TrailingDot) {
  // Ends with '.', which is stripped before checking for '.'.
  EXPECT_TRUE(HostUtils::IsValidDomain("example.com."));
}

}  // namespace net
