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

#include "arkweb/chromium_ext/services/network/public/cpp/navigation_info_mojom_traits.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "arkweb/chromium_ext/net/base/navigation_info.h"
#include "arkweb/chromium_ext/net/dns/public/resolve_info.h"
#include "arkweb/chromium_ext/services/network/public/mojom/navigation_info.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ElementsAreArray;

// ---------------------------------------------------------------------------
// Helpers that build fully-populated structs with distinctive, non-default
// values (multiple vector elements, negative ints, true bools, non-empty
// strings). Used to exercise every field-assignment statement in each
// StructTraits::Read() success branch.
// ---------------------------------------------------------------------------

net::ResolveInfo MakePopulatedResolveInfo() {
  net::ResolveInfo info;
  info.error_code = -42;
  info.dns_status = net::kDnsResolvedByHttpsDns;
  info.dns_transition = 0x01020304u;
  info.query_host = "example.com";
  info.ip_endpoints = {"1.2.3.4", "::1"};
  info.truncation_ips = {"5.6.7.8"};
  return info;
}

net::DnsInfo MakePopulatedDnsInfo() {
  net::DnsInfo info;
  info.result = -7;
  info.dns_transition = net::DnsInfo::SYSTEM | net::DnsInfo::DNS;
  info.dns_status = net::kDnsResolvedBySystemDns;
  info.host = "host.example";
  info.address_list = {"10.0.0.1", "10.0.0.2", "fe80::1"};
  info.truncation_ips = {"9.9.9.9"};
  return info;
}

net::ArkWebSSLInfo MakePopulatedArkWebSSLInfo() {
  net::ArkWebSSLInfo info;
  info.host = "secure.example";
  info.issuer = "CN=Test Issuer";
  info.expired_date = "2026-12-31T23:59:59Z";
  info.is_fatal_cert_error = true;
  info.result = -202;
  return info;
}

net::SocketInfo MakePopulatedSocketInfo() {
  net::SocketInfo info;
  info.host = "socket.example";
  info.result = -105;
  info.address_list = {"1.1.1.1", "2.2.2.2"};
  return info;
}

net::RequestAttempt MakePopulatedRequestAttempt() {
  net::RequestAttempt info;
  info.request_trace_id = "trace-abc-123";
  info.attempt_type = net::AttemptType::kFallbackProxy;
  info.request_result = -303;
  info.was_fetched_via_proxy = true;
  info.dns_info = MakePopulatedDnsInfo();
  info.ssl_info = MakePopulatedArkWebSSLInfo();
  info.socket_info = MakePopulatedSocketInfo();
  return info;
}

net::NavigationInfo MakePopulatedNavigationInfo() {
  net::NavigationInfo info;
  info.request_url = "https://example.com/page";
  info.request_uuid = "uuid-1234";
  info.time_stamp = "2026-07-01T12:00:00Z";
  info.dns_name_servers = "8.8.8.8,1.1.1.1";
  info.local_address = "192.168.1.10";
  info.original_error_code = -501;
  info.error_code = -502;
  info.can_use_secure_dns = true;
  info.is_fallback_proxy_enabled = true;
  info.resolve_info = MakePopulatedResolveInfo();
  info.request_attempts = {MakePopulatedRequestAttempt(),
                           MakePopulatedRequestAttempt()};
  return info;
}

// ---------------------------------------------------------------------------
// Field-by-field comparison helpers. Comparing every field (rather than
// relying on operator==, which only ResolveInfo defines) documents exactly
// which Read() assignments are covered.
// ---------------------------------------------------------------------------

void ExpectResolveInfoEquals(const net::ResolveInfo& expected,
                             const net::ResolveInfo& actual) {
  EXPECT_EQ(expected.error_code, actual.error_code);
  EXPECT_EQ(expected.dns_status, actual.dns_status);
  EXPECT_EQ(expected.dns_transition, actual.dns_transition);
  EXPECT_EQ(expected.query_host, actual.query_host);
  EXPECT_THAT(actual.ip_endpoints, ElementsAreArray(expected.ip_endpoints));
  EXPECT_THAT(actual.truncation_ips, ElementsAreArray(expected.truncation_ips));
}

void ExpectDnsInfoEquals(const net::DnsInfo& expected,
                         const net::DnsInfo& actual) {
  EXPECT_EQ(expected.result, actual.result);
  EXPECT_EQ(expected.dns_transition, actual.dns_transition);
  EXPECT_EQ(expected.dns_status, actual.dns_status);
  EXPECT_EQ(expected.host, actual.host);
  EXPECT_THAT(actual.address_list, ElementsAreArray(expected.address_list));
  EXPECT_THAT(actual.truncation_ips, ElementsAreArray(expected.truncation_ips));
}

void ExpectArkWebSSLInfoEquals(const net::ArkWebSSLInfo& expected,
                               const net::ArkWebSSLInfo& actual) {
  EXPECT_EQ(expected.host, actual.host);
  EXPECT_EQ(expected.issuer, actual.issuer);
  EXPECT_EQ(expected.expired_date, actual.expired_date);
  EXPECT_EQ(expected.is_fatal_cert_error, actual.is_fatal_cert_error);
  EXPECT_EQ(expected.result, actual.result);
}

void ExpectSocketInfoEquals(const net::SocketInfo& expected,
                            const net::SocketInfo& actual) {
  EXPECT_EQ(expected.host, actual.host);
  EXPECT_EQ(expected.result, actual.result);
  EXPECT_THAT(actual.address_list, ElementsAreArray(expected.address_list));
}

void ExpectRequestAttemptEquals(const net::RequestAttempt& expected,
                                const net::RequestAttempt& actual) {
  EXPECT_EQ(expected.request_trace_id, actual.request_trace_id);
  EXPECT_EQ(expected.attempt_type, actual.attempt_type);
  EXPECT_EQ(expected.request_result, actual.request_result);
  EXPECT_EQ(expected.was_fetched_via_proxy, actual.was_fetched_via_proxy);
  ExpectDnsInfoEquals(expected.dns_info, actual.dns_info);
  ExpectArkWebSSLInfoEquals(expected.ssl_info, actual.ssl_info);
  ExpectSocketInfoEquals(expected.socket_info, actual.socket_info);
}

void ExpectNavigationInfoEquals(const net::NavigationInfo& expected,
                                const net::NavigationInfo& actual) {
  EXPECT_EQ(expected.request_url, actual.request_url);
  EXPECT_EQ(expected.request_uuid, actual.request_uuid);
  EXPECT_EQ(expected.time_stamp, actual.time_stamp);
  EXPECT_EQ(expected.dns_name_servers, actual.dns_name_servers);
  EXPECT_EQ(expected.local_address, actual.local_address);
  EXPECT_EQ(expected.original_error_code, actual.original_error_code);
  EXPECT_EQ(expected.error_code, actual.error_code);
  EXPECT_EQ(expected.can_use_secure_dns, actual.can_use_secure_dns);
  EXPECT_EQ(expected.is_fallback_proxy_enabled,
            actual.is_fallback_proxy_enabled);
  ExpectResolveInfoEquals(expected.resolve_info, actual.resolve_info);
  EXPECT_EQ(expected.request_attempts.size(), actual.request_attempts.size());
  for (size_t i = 0; i < expected.request_attempts.size(); ++i) {
    ExpectRequestAttemptEquals(expected.request_attempts[i],
                               actual.request_attempts[i]);
  }
}

// For the leaf structs (ResolveInfo, DnsInfo, ArkWebSSLInfo, SocketInfo) every
// mojo field is a non-nullable string/array of strings or a primitive. Because
// StringTraits<std::string>::Read and the std::vector array deserializer always
// succeed for a structurally-valid message, the only way to make
// DeserializeFromMessage() return false is to feed it a structurally-malformed
// message. This helper serializes a valid struct, round-trips it through a
// message handle (mirroring mojo::test::SerializeAndDeserialize), then zeroes
// the payload so the struct header (num_bytes == 0) is rejected by
// ValidateStructHeaderAndClaimMemory() before Read() runs.
template <typename MojomType, typename UserType>
void ExpectCorruptPayloadFailsDeserialization(UserType* input) {
  mojo::Message message = MojomType::SerializeAsMessage(input);
  mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
  message = mojo::Message::CreateFromMessageHandle(&handle);
  ASSERT_NE(0u, message.payload_num_bytes());
  uint8_t* mutable_payload = message.mutable_payload();
  base::span<uint8_t> mutable_payload_span(mutable_payload,
                                           message.payload_num_bytes());
  std::ranges::fill(mutable_payload_span, 0);
  UserType output;
  EXPECT_FALSE(MojomType::DeserializeFromMessage(std::move(message), &output));
}

class NavigationInfoMojomTraitsTest : public ::testing::Test {};

// ---------------------------------------------------------------------------
// ResolveInfo
// ---------------------------------------------------------------------------

TEST_F(NavigationInfoMojomTraitsTest, ResolveInfoRoundTrip) {
  {
    net::ResolveInfo input = MakePopulatedResolveInfo();
    net::ResolveInfo output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::ResolveInfo>(
            input, output));
    ExpectResolveInfoEquals(input, output);
  }
  {
    net::ResolveInfo input;
    net::ResolveInfo output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::ResolveInfo>(
            input, output));
    ExpectResolveInfoEquals(input, output);
  }
}

TEST_F(NavigationInfoMojomTraitsTest, ResolveInfoFailsWithCorruptPayload) {
  net::ResolveInfo input = MakePopulatedResolveInfo();
  ExpectCorruptPayloadFailsDeserialization<network::mojom::ResolveInfo>(&input);
}

// ---------------------------------------------------------------------------
// DnsInfo
// ---------------------------------------------------------------------------

TEST_F(NavigationInfoMojomTraitsTest, DnsInfoRoundTrip) {
  {
    net::DnsInfo input = MakePopulatedDnsInfo();
    net::DnsInfo output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::DnsInfo>(
        input, output));
    ExpectDnsInfoEquals(input, output);
  }
  {
    net::DnsInfo input;
    net::DnsInfo output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::DnsInfo>(
        input, output));
    ExpectDnsInfoEquals(input, output);
  }
}

TEST_F(NavigationInfoMojomTraitsTest, DnsInfoFailsWithCorruptPayload) {
  net::DnsInfo input = MakePopulatedDnsInfo();
  ExpectCorruptPayloadFailsDeserialization<network::mojom::DnsInfo>(&input);
}

// ---------------------------------------------------------------------------
// ArkWebSSLInfo
// ---------------------------------------------------------------------------

TEST_F(NavigationInfoMojomTraitsTest, ArkWebSSLInfoRoundTrip) {
  {
    net::ArkWebSSLInfo input = MakePopulatedArkWebSSLInfo();
    net::ArkWebSSLInfo output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::ArkWebSSLInfo>(
            input, output));
    ExpectArkWebSSLInfoEquals(input, output);
  }
  {
    net::ArkWebSSLInfo input;
    net::ArkWebSSLInfo output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::ArkWebSSLInfo>(
            input, output));
    ExpectArkWebSSLInfoEquals(input, output);
  }
}

TEST_F(NavigationInfoMojomTraitsTest, ArkWebSSLInfoFailsWithCorruptPayload) {
  net::ArkWebSSLInfo input = MakePopulatedArkWebSSLInfo();
  ExpectCorruptPayloadFailsDeserialization<network::mojom::ArkWebSSLInfo>(
      &input);
}

// ---------------------------------------------------------------------------
// SocketInfo
// ---------------------------------------------------------------------------

TEST_F(NavigationInfoMojomTraitsTest, SocketInfoRoundTrip) {
  {
    net::SocketInfo input = MakePopulatedSocketInfo();
    net::SocketInfo output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::SocketInfo>(
        input, output));
    ExpectSocketInfoEquals(input, output);
  }
  {
    net::SocketInfo input;
    net::SocketInfo output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<network::mojom::SocketInfo>(
        input, output));
    ExpectSocketInfoEquals(input, output);
  }
}

TEST_F(NavigationInfoMojomTraitsTest, SocketInfoFailsWithCorruptPayload) {
  net::SocketInfo input = MakePopulatedSocketInfo();
  ExpectCorruptPayloadFailsDeserialization<network::mojom::SocketInfo>(&input);
}

// ---------------------------------------------------------------------------
// RequestAttempt (contains nested DnsInfo / ArkWebSSLInfo / SocketInfo)
// ---------------------------------------------------------------------------

TEST_F(NavigationInfoMojomTraitsTest, RequestAttemptRoundTrip) {
  {
    net::RequestAttempt input = MakePopulatedRequestAttempt();
    net::RequestAttempt output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::RequestAttempt>(
            input, output));
    ExpectRequestAttemptEquals(input, output);
  }
  {
    net::RequestAttempt input;
    net::RequestAttempt output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::RequestAttempt>(
            input, output));
    ExpectRequestAttemptEquals(input, output);
  }
}

// ---------------------------------------------------------------------------
// NavigationInfo (contains nested ResolveInfo + vector<RequestAttempt>)
// ---------------------------------------------------------------------------

TEST_F(NavigationInfoMojomTraitsTest, NavigationInfoRoundTrip) {
  {
    net::NavigationInfo input = MakePopulatedNavigationInfo();
    net::NavigationInfo output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::NavigationInfo>(
            input, output));
    ExpectNavigationInfoEquals(input, output);
  }
  {
    net::NavigationInfo input;
    net::NavigationInfo output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::NavigationInfo>(
            input, output));
    ExpectNavigationInfoEquals(input, output);
  }
}

TEST_F(NavigationInfoMojomTraitsTest, NavigationInfoFailsWithNullResolveInfo) {
  // Construct a mojom NavigationInfo whose non-nullable nested ResolveInfo is
  // left as a null mojo::StructPtr. Deserialization must be rejected.
  mojo::StructPtr<network::mojom::NavigationInfo> input(std::in_place);
  input->request_url = "https://example.com/page";
  net::NavigationInfo output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<network::mojom::NavigationInfo>(
          input, output));
}

}  // namespace
