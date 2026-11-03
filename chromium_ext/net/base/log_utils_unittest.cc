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

#include "arkweb/chromium_ext/net/base/log_utils.h"

#include <string>

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(LogUtilsTest, AnonymizeIpv4Address) {
  IPAddress ip_addr(192, 168, 1, 100);
  IPEndPoint endpoint(ip_addr, 8080);
  std::string result = LogUtils::AnonymizeIpAddress(endpoint);
  // IPv4: first 2 octets kept, last 2 octets each replaced with single *
  EXPECT_EQ(result, "192.168.**");
}

TEST(LogUtilsTest, AnonymizeIpv4AddressZeros) {
  IPAddress ip_addr(0, 0, 0, 0);
  IPEndPoint endpoint(ip_addr, 0);
  std::string result = LogUtils::AnonymizeIpAddress(endpoint);
  EXPECT_EQ(result, "0.0.**");
}

TEST(LogUtilsTest, AnonymizeIpv4AddressMax) {
  IPAddress ip_addr(255, 255, 255, 255);
  IPEndPoint endpoint(ip_addr, 443);
  std::string result = LogUtils::AnonymizeIpAddress(endpoint);
  EXPECT_EQ(result, "255.255.**");
}

TEST(LogUtilsTest, AnonymizeIpv6Address) {
  // IPv6: 2001:0db8:85a3:0000:0000:8a2e:0370:7334
  uint8_t bytes[16] = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x00, 0x00,
                       0x00, 0x00, 0x8a, 0x2e, 0x03, 0x70, 0x73, 0x34};
  IPAddress ip_addr(bytes);
  IPEndPoint endpoint(ip_addr, 443);
  std::string result = LogUtils::AnonymizeIpAddress(endpoint);
  // IPv6: first 3 groups (bytes 0-5) kept, remaining groups masked with *
  // Groups: 2001:db8:****:****:****:****:****:****
  EXPECT_NE(result.find("2001"), std::string::npos);
  EXPECT_NE(result.find("db8"), std::string::npos);
  // The masked part should contain *
  EXPECT_NE(result.find("*"), std::string::npos);
}

TEST(LogUtilsTest, AnonymizeIpv6Loopback) {
  // ::1 loopback
  uint8_t bytes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  IPAddress ip_addr(bytes);
  IPEndPoint endpoint(ip_addr, 80);
  std::string result = LogUtils::AnonymizeIpAddress(endpoint);
  // All groups after 3rd (i >= 6 bytes) should be masked
  EXPECT_NE(result.find("*"), std::string::npos);
}

TEST(LogUtilsTest, AnonymizeIpv6AllZeros) {
  // All-zeros IPv6 address
  uint8_t bytes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  IPAddress ip_addr(bytes);
  IPEndPoint endpoint(ip_addr, 0);
  std::string result = LogUtils::AnonymizeIpAddress(endpoint);
  // All groups beyond first 3 pairs (6 bytes) should be masked with *
  EXPECT_NE(result.find("*"), std::string::npos);
  // Should contain colons
  EXPECT_NE(result.find(":"), std::string::npos);
}

TEST(LogUtilsTest, AnonymizeIpv4SingleOctetValues) {
  // Test with small single-digit octets to verify no extra chars
  IPAddress ip_addr(1, 2, 3, 4);
  IPEndPoint endpoint(ip_addr, 0);
  std::string result = LogUtils::AnonymizeIpAddress(endpoint);
  // First two octets kept, last two replaced with *
  EXPECT_EQ(result, "1.2.**");
}

TEST(LogUtilsTest, AnonymizeIpv6FirstTwoGroupsPreserved) {
  // IPv6 with distinct first 2 groups so we can verify they appear
  // Groups: 2001:0db8:85a3:...
  // Only the first 2 groups (4 bytes, i<6 before increment) are preserved;
  // the 3rd group (i=6 after increment) is masked.
  uint8_t bytes[16] = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
  IPAddress ip_addr(bytes);
  IPEndPoint endpoint(ip_addr, 443);
  std::string result = LogUtils::AnonymizeIpAddress(endpoint);
  // First 2 groups (4 bytes) should appear as hex
  EXPECT_NE(result.find("2001"), std::string::npos);
  EXPECT_NE(result.find("db8"), std::string::npos);
  // 3rd group and beyond should be masked (85a3 should NOT appear)
  EXPECT_EQ(result.find("85a3"), std::string::npos);
  // Masked groups should show *
  EXPECT_NE(result.find("*"), std::string::npos);
}

TEST(LogUtilsTest, AnonymizeIpv4LargeOctets) {
  // Test with 3-digit octets
  IPAddress ip_addr(100, 200, 50, 25);
  IPEndPoint endpoint(ip_addr, 80);
  std::string result = LogUtils::AnonymizeIpAddress(endpoint);
  EXPECT_EQ(result, "100.200.**");
}

}  // namespace net
