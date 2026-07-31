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

#include "arkweb/chromium_ext/net/dns/public/resolve_info.h"

#include "arkweb/chromium_ext/net/dns/public/dns_info.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(ResolveInfoTest, DefaultConstructor) {
  ResolveInfo info;
  EXPECT_EQ(info.error_code, net::OK);
  EXPECT_EQ(info.dns_status, kDnsResolvedUndefined);
  EXPECT_EQ(info.dns_transition, 0u);
  EXPECT_TRUE(info.query_host.empty());
  EXPECT_TRUE(info.ip_endpoints.empty());
  EXPECT_TRUE(info.truncation_ips.empty());
}

TEST(ResolveInfoTest, TwoArgConstructor) {
  ResolveInfo info(net::ERR_NAME_NOT_RESOLVED, kDnsResolvedByHttpsDns);
  EXPECT_EQ(info.error_code, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(info.dns_status, kDnsResolvedByHttpsDns);
  EXPECT_EQ(info.dns_transition, 0u);
  EXPECT_TRUE(info.query_host.empty());
}

TEST(ResolveInfoTest, FourArgConstructor) {
  ResolveInfo info(net::ERR_TIMED_OUT, kDnsResolvedByLocalDns, 0x100,
                   "test.com");
  EXPECT_EQ(info.error_code, net::ERR_TIMED_OUT);
  EXPECT_EQ(info.dns_status, kDnsResolvedByLocalDns);
  EXPECT_EQ(info.dns_transition, 0x100u);
  EXPECT_EQ(info.query_host, "test.com");
}

TEST(ResolveInfoTest, CopyConstructor) {
  ResolveInfo info(net::ERR_FAILED, kDnsResolvedFromHosts, 0x01, "a.com");
  ResolveInfo copy(info);
  EXPECT_EQ(copy.error_code, net::ERR_FAILED);
  EXPECT_EQ(copy.dns_status, kDnsResolvedFromHosts);
  EXPECT_EQ(copy.dns_transition, 0x01u);
  EXPECT_EQ(copy.query_host, "a.com");
}

TEST(ResolveInfoTest, MoveConstructor) {
  ResolveInfo info(net::OK, kDnsResolvedBySystemDns, 0x10, "b.com");
  ResolveInfo moved(std::move(info));
  EXPECT_EQ(moved.error_code, net::OK);
  EXPECT_EQ(moved.dns_status, kDnsResolvedBySystemDns);
  EXPECT_EQ(moved.dns_transition, 0x10u);
  EXPECT_EQ(moved.query_host, "b.com");
}

TEST(ResolveInfoTest, CopyAssignment) {
  ResolveInfo info(net::ERR_IO_PENDING, kDnsResolvedFromInsecureCache);
  ResolveInfo assigned;
  assigned = info;
  EXPECT_EQ(assigned.error_code, net::ERR_IO_PENDING);
  EXPECT_EQ(assigned.dns_status, kDnsResolvedFromInsecureCache);
}

TEST(ResolveInfoTest, MoveAssignment) {
  ResolveInfo info(net::ERR_CONNECTION_FAILED, kDnsResolvedFromSecureCache);
  ResolveInfo assigned;
  assigned = std::move(info);
  EXPECT_EQ(assigned.error_code, net::ERR_CONNECTION_FAILED);
  EXPECT_EQ(assigned.dns_status, kDnsResolvedFromSecureCache);
}

TEST(ResolveInfoTest, OperatorEqual) {
  ResolveInfo a(net::OK, kDnsResolvedByHttpsDns);
  ResolveInfo b(net::OK, kDnsResolvedByHttpsDns);
  EXPECT_TRUE(a == b);
}

TEST(ResolveInfoTest, OperatorNotEqual) {
  ResolveInfo a(net::OK, kDnsResolvedByHttpsDns);
  ResolveInfo b(net::ERR_FAILED, kDnsResolvedByHttpsDns);
  EXPECT_TRUE(a != b);
}

TEST(ResolveInfoTest, OperatorNotEqual_DnsStatus) {
  ResolveInfo a(net::OK, kDnsResolvedByHttpsDns);
  ResolveInfo b(net::OK, kDnsResolvedByLocalDns);
  EXPECT_TRUE(a != b);
}

TEST(ResolveInfoTest, IsResolvedByHttpsDns_HttpsDns) {
  EXPECT_TRUE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedByHttpsDns));
}

TEST(ResolveInfoTest, IsResolvedByHttpsDns_SecureCache) {
  EXPECT_TRUE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedFromSecureCache));
}

TEST(ResolveInfoTest, IsResolvedByHttpsDns_OtherStatuses) {
  EXPECT_FALSE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedUndefined));
  EXPECT_FALSE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedFromHosts));
  EXPECT_FALSE(
      ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedFromInsecureCache));
  EXPECT_FALSE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedByLocalDns));
  EXPECT_FALSE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedBySystemDns));
}

TEST(ResolveInfoTest, SetIPEndPoints) {
  IPAddress ip1(192, 168, 1, 1);
  IPAddress ip2(10, 0, 0, 1);
  IPEndPoint ep1(ip1, 443);
  IPEndPoint ep2(ip2, 80);

  ResolveInfo info;
  info.SetIPEndPoints({ep1, ep2});
  EXPECT_EQ(info.ip_endpoints.size(), 2u);
  // ToStringWithoutPort includes the IP only
  EXPECT_NE(info.ip_endpoints[0].find("192.168"), std::string::npos);
  EXPECT_NE(info.ip_endpoints[1].find("10.0"), std::string::npos);
}

TEST(ResolveInfoTest, AppendAddress) {
  IPAddress ip(127, 0, 0, 1);
  IPEndPoint ep(ip, 8080);

  ResolveInfo info;
  info.AppendAddress(ep);
  EXPECT_EQ(info.ip_endpoints.size(), 1u);
}

TEST(ResolveInfoTest, AppendTruncationAddress) {
  IPAddress ip(8, 8, 8, 8);
  IPEndPoint ep(ip, 53);

  ResolveInfo info;
  info.AppendTruncationAddress(ep);
  EXPECT_EQ(info.truncation_ips.size(), 1u);
}

TEST(ResolveInfoTest, AddTransitionType) {
  ResolveInfo info;
  info.AddTransitionType(DnsInfo::SYSTEM);
  EXPECT_EQ(info.dns_transition, DnsInfo::SYSTEM);

  info.AddTransitionType(DnsInfo::DNS);
  EXPECT_EQ(info.dns_transition, DnsInfo::SYSTEM | DnsInfo::DNS);
}

TEST(ResolveInfoTest, AddTransitionTypeDefaultZero) {
  ResolveInfo info;
  info.AddTransitionType(DnsInfo::DEFAULT);
  // DEFAULT == 0, OR with 0 should not change dns_transition
  EXPECT_EQ(info.dns_transition, 0u);
}

TEST(ResolveInfoTest, AddTransitionTypeMultipleBits) {
  ResolveInfo info;
  info.AddTransitionType(DnsInfo::SYSTEM);
  info.AddTransitionType(DnsInfo::SECURE_DNS);
  info.AddTransitionType(DnsInfo::SECURE_DNS_FALLBACK);
  EXPECT_EQ(info.dns_transition,
            static_cast<uint32_t>(DnsInfo::SYSTEM | DnsInfo::SECURE_DNS |
                                  DnsInfo::SECURE_DNS_FALLBACK));
}

TEST(ResolveInfoTest, SetIPEndPointsEmpty) {
  ResolveInfo info;
  info.SetIPEndPoints({});
  EXPECT_TRUE(info.ip_endpoints.empty());
}

TEST(ResolveInfoTest, SetIPEndPointsMultiple) {
  IPAddress ip1(192, 168, 1, 1);
  IPAddress ip2(10, 0, 0, 1);
  IPAddress ip3(172, 16, 0, 1);
  IPEndPoint ep1(ip1, 443);
  IPEndPoint ep2(ip2, 80);
  IPEndPoint ep3(ip3, 8080);

  ResolveInfo info;
  info.SetIPEndPoints({ep1, ep2, ep3});
  EXPECT_EQ(info.ip_endpoints.size(), 3u);
}

TEST(ResolveInfoTest, AppendAddressMultiple) {
  IPAddress ip1(127, 0, 0, 1);
  IPAddress ip2(127, 0, 0, 2);
  IPEndPoint ep1(ip1, 8080);
  IPEndPoint ep2(ip2, 8080);

  ResolveInfo info;
  info.AppendAddress(ep1);
  info.AppendAddress(ep2);
  EXPECT_EQ(info.ip_endpoints.size(), 2u);
}

TEST(ResolveInfoTest, AppendTruncationAddressMultiple) {
  IPAddress ip1(8, 8, 8, 8);
  IPAddress ip2(8, 8, 4, 4);
  IPEndPoint ep1(ip1, 53);
  IPEndPoint ep2(ip2, 53);

  ResolveInfo info;
  info.AppendTruncationAddress(ep1);
  info.AppendTruncationAddress(ep2);
  EXPECT_EQ(info.truncation_ips.size(), 2u);
}

TEST(ResolveInfoTest, OperatorEqualBothFieldsMatch) {
  ResolveInfo a(net::OK, kDnsResolvedByHttpsDns);
  ResolveInfo b(net::OK, kDnsResolvedByHttpsDns);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(ResolveInfoTest, OperatorEqualErrorCodeDiffers) {
  ResolveInfo a(net::OK, kDnsResolvedByHttpsDns);
  ResolveInfo b(net::ERR_FAILED, kDnsResolvedByHttpsDns);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(ResolveInfoTest, CopySelfAssignment) {
  ResolveInfo info(net::ERR_NAME_NOT_RESOLVED, kDnsResolvedFromHosts, 0x01,
                   "self.com");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-assign-overloaded"
  info = info;  // NOLINT(bugprone-unhandled-self-assignment)
#pragma GCC diagnostic pop
  EXPECT_EQ(info.error_code, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(info.dns_status, kDnsResolvedFromHosts);
  EXPECT_EQ(info.query_host, "self.com");
}

TEST(ResolveInfoTest, MoveSelfAssignment) {
  ResolveInfo info(net::ERR_TIMED_OUT, kDnsResolvedByLocalDns, 0x10,
                   "move-self.com");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  info = std::move(info);  // NOLINT(bugprone-unhandled-self-assignment)
#pragma GCC diagnostic pop
  // After self-move, validate object is still usable
  EXPECT_EQ(info.dns_status, kDnsResolvedByLocalDns);
}

TEST(ResolveInfoTest, IsResolvedByHttpsDns_SecureCacheAndHttpsDnsReturnTrue) {
  EXPECT_TRUE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedFromSecureCache));
  EXPECT_TRUE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedByHttpsDns));
  // Verify all other statuses return false
  EXPECT_FALSE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedUndefined));
  EXPECT_FALSE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedFromHosts));
  EXPECT_FALSE(
      ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedFromInsecureCache));
  EXPECT_FALSE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedByLocalDns));
  EXPECT_FALSE(ResolveInfo::IsResolvedByHttpsDns(kDnsResolvedBySystemDns));
}

}  // namespace net
