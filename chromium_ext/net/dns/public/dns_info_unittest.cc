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

#include "arkweb/chromium_ext/net/dns/public/dns_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(DnsInfoTest, DefaultConstructor) {
  DnsInfo info;
  EXPECT_EQ(info.result, net::OK);
  EXPECT_EQ(info.dns_status, kDnsResolvedUndefined);
  EXPECT_EQ(info.dns_transition, 0u);
  EXPECT_TRUE(info.host.empty());
  EXPECT_TRUE(info.address_list.empty());
  EXPECT_TRUE(info.truncation_ips.empty());
}

TEST(DnsInfoTest, CopyConstructor) {
  DnsInfo info;
  info.result = net::ERR_NAME_NOT_RESOLVED;
  info.dns_status = kDnsResolvedByHttpsDns;
  info.dns_transition = DnsInfo::SECURE_DNS;
  info.host = "example.com";
  info.address_list = {"1.2.3.4"};
  info.truncation_ips = {"5.6.7.8"};

  DnsInfo copy(info);
  EXPECT_EQ(copy.result, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(copy.dns_status, kDnsResolvedByHttpsDns);
  EXPECT_EQ(copy.dns_transition, DnsInfo::SECURE_DNS);
  EXPECT_EQ(copy.host, "example.com");
  EXPECT_EQ(copy.address_list.size(), 1u);
  EXPECT_EQ(copy.truncation_ips.size(), 1u);
}

TEST(DnsInfoTest, MoveConstructor) {
  DnsInfo info;
  info.result = net::ERR_TIMED_OUT;
  info.dns_status = kDnsResolvedByLocalDns;
  info.host = "test.org";

  DnsInfo moved(std::move(info));
  EXPECT_EQ(moved.result, net::ERR_TIMED_OUT);
  EXPECT_EQ(moved.dns_status, kDnsResolvedByLocalDns);
  EXPECT_EQ(moved.host, "test.org");
}

TEST(DnsInfoTest, CopyAssignment) {
  DnsInfo info;
  info.dns_status = kDnsResolvedFromHosts;
  info.host = "host.test";

  DnsInfo assigned;
  assigned = info;
  EXPECT_EQ(assigned.dns_status, kDnsResolvedFromHosts);
  EXPECT_EQ(assigned.host, "host.test");
}

TEST(DnsInfoTest, MoveAssignment) {
  DnsInfo info;
  info.dns_status = kDnsResolvedBySystemDns;
  info.host = "system.test";

  DnsInfo assigned;
  assigned = std::move(info);
  EXPECT_EQ(assigned.dns_status, kDnsResolvedBySystemDns);
  EXPECT_EQ(assigned.host, "system.test");
}

TEST(DnsInfoTest, UsedHttpDns_Undefined) {
  DnsInfo info;
  info.dns_status = kDnsResolvedUndefined;
  EXPECT_FALSE(info.UsedHttpDns());
}

TEST(DnsInfoTest, UsedHttpDns_SecureCache) {
  DnsInfo info;
  info.dns_status = kDnsResolvedFromSecureCache;
  EXPECT_TRUE(info.UsedHttpDns());
}

TEST(DnsInfoTest, UsedHttpDns_HttpsDns) {
  DnsInfo info;
  info.dns_status = kDnsResolvedByHttpsDns;
  EXPECT_TRUE(info.UsedHttpDns());
}

TEST(DnsInfoTest, UsedHttpDns_OtherStatuses) {
  DnsInfo info;
  info.dns_status = kDnsResolvedFromHosts;
  EXPECT_FALSE(info.UsedHttpDns());
  info.dns_status = kDnsResolvedFromInsecureCache;
  EXPECT_FALSE(info.UsedHttpDns());
  info.dns_status = kDnsResolvedByLocalDns;
  EXPECT_FALSE(info.UsedHttpDns());
  info.dns_status = kDnsResolvedBySystemDns;
  EXPECT_FALSE(info.UsedHttpDns());
}

TEST(DnsInfoTest, UsedInsecureDns_Undefined) {
  DnsInfo info;
  info.dns_status = kDnsResolvedUndefined;
  EXPECT_FALSE(info.UsedInsecureDns());
}

TEST(DnsInfoTest, UsedInsecureDns_Hosts) {
  DnsInfo info;
  info.dns_status = kDnsResolvedFromHosts;
  EXPECT_TRUE(info.UsedInsecureDns());
}

TEST(DnsInfoTest, UsedInsecureDns_InsecureCache) {
  DnsInfo info;
  info.dns_status = kDnsResolvedFromInsecureCache;
  EXPECT_TRUE(info.UsedInsecureDns());
}

TEST(DnsInfoTest, UsedInsecureDns_LocalDns) {
  DnsInfo info;
  info.dns_status = kDnsResolvedByLocalDns;
  EXPECT_TRUE(info.UsedInsecureDns());
}

TEST(DnsInfoTest, UsedInsecureDns_SystemDns) {
  DnsInfo info;
  info.dns_status = kDnsResolvedBySystemDns;
  EXPECT_TRUE(info.UsedInsecureDns());
}

TEST(DnsInfoTest, UsedInsecureDns_SecureStatuses) {
  DnsInfo info;
  info.dns_status = kDnsResolvedFromSecureCache;
  EXPECT_FALSE(info.UsedInsecureDns());
  info.dns_status = kDnsResolvedByHttpsDns;
  EXPECT_FALSE(info.UsedInsecureDns());
}

TEST(DnsInfoTest, TransitionTypeEnumValues) {
  EXPECT_EQ(DnsInfo::DEFAULT, 0);
  EXPECT_EQ(DnsInfo::SYSTEM, 0x00000001);
  EXPECT_EQ(DnsInfo::DNS, 0x00000010);
  EXPECT_EQ(DnsInfo::SECURE_DNS, 0x00000100);
  EXPECT_EQ(DnsInfo::CACHE_LOOKUP, 0x00001000);
  EXPECT_EQ(DnsInfo::INSECURE_CACHE, 0x00010000);
  EXPECT_EQ(DnsInfo::SECURE_CACHE, 0x00100000);
  EXPECT_EQ(DnsInfo::SECURE_DNS_FALLBACK, 0x01000000);
}

TEST(DnsInfoTest, UsedHttpDns_SecureCacheBoundary) {
  DnsInfo info;
  // Only secure cache and https dns return true for UsedHttpDns
  info.dns_status = kDnsResolvedFromSecureCache;
  EXPECT_TRUE(info.UsedHttpDns());
  info.dns_status = kDnsResolvedByHttpsDns;
  EXPECT_TRUE(info.UsedHttpDns());
  // All other statuses return false
  info.dns_status = kDnsResolvedUndefined;
  EXPECT_FALSE(info.UsedHttpDns());
}

TEST(DnsInfoTest, UsedInsecureDns_AllInsecureStatuses) {
  DnsInfo info;
  // Each insecure status returns true
  info.dns_status = kDnsResolvedFromHosts;
  EXPECT_TRUE(info.UsedInsecureDns());
  info.dns_status = kDnsResolvedFromInsecureCache;
  EXPECT_TRUE(info.UsedInsecureDns());
  info.dns_status = kDnsResolvedByLocalDns;
  EXPECT_TRUE(info.UsedInsecureDns());
  info.dns_status = kDnsResolvedBySystemDns;
  EXPECT_TRUE(info.UsedInsecureDns());
}

TEST(DnsInfoTest, CopySelfAssignment) {
  DnsInfo info;
  info.result = net::ERR_NAME_NOT_RESOLVED;
  info.dns_status = kDnsResolvedByHttpsDns;
  info.host = "self.com";
  info.address_list = {"10.0.0.1"};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-assign-overloaded"
  info = info;  // NOLINT(bugprone-unhandled-self-assignment)
#pragma GCC diagnostic pop
  EXPECT_EQ(info.result, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(info.dns_status, kDnsResolvedByHttpsDns);
  EXPECT_EQ(info.host, "self.com");
}

TEST(DnsInfoTest, MoveSelfAssignment) {
  DnsInfo info;
  info.result = net::ERR_TIMED_OUT;
  info.dns_status = kDnsResolvedByLocalDns;
  info.host = "move-self.com";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  info = std::move(info);  // NOLINT(bugprone-unhandled-self-assignment)
#pragma GCC diagnostic pop
  // After self-move, validate object is still in a usable state
  EXPECT_EQ(info.dns_status, kDnsResolvedByLocalDns);
}

TEST(DnsInfoTest, AddressListAndTruncationIps) {
  DnsInfo info;
  info.address_list = {"1.2.3.4", "5.6.7.8"};
  info.truncation_ips = {"10.0.0.1"};
  EXPECT_EQ(info.address_list.size(), 2u);
  EXPECT_EQ(info.truncation_ips.size(), 1u);

  DnsInfo copy(info);
  EXPECT_EQ(copy.address_list.size(), 2u);
  EXPECT_EQ(copy.truncation_ips.size(), 1u);
}

TEST(DnsInfoTest, DnsTransitionCombined) {
  DnsInfo info;
  info.dns_transition =
      DnsInfo::SYSTEM | DnsInfo::DNS | DnsInfo::SECURE_DNS_FALLBACK;
  EXPECT_TRUE(info.dns_transition & DnsInfo::SYSTEM);
  EXPECT_TRUE(info.dns_transition & DnsInfo::DNS);
  EXPECT_TRUE(info.dns_transition & DnsInfo::SECURE_DNS_FALLBACK);
  EXPECT_FALSE(info.dns_transition & DnsInfo::SECURE_DNS);
}

}  // namespace net
