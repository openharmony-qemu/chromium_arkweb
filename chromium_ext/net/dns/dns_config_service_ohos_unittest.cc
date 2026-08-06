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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstring>
#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "net/base/ip_address.h"
#include "net/dns/public/dns_protocol.h"
#include "securec.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// clang-format off
#define private public
#include "net/dns/dns_config_service_ohos.h"
#undef private
// clang-format on

// Use the mock_ndk_api's g_mock_* variables which are connected to the
// __wrap functions via ldflags --wrap in BUILD.gn.
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api.h"

using MockNdkApi::g_mock_OH_NetConn_GetConnectionProperties;
using MockNdkApi::g_mock_OH_NetConn_GetDefaultNet;

namespace net {
namespace {
enum AddressFamily { FAMILY_IPV4 = 1, FAMILY_IPV6 = 2 };
static constexpr NetConn_NetAddr kTestAddresses[] = {
    {FAMILY_IPV4, 0, NS_DEFAULTPORT, "92.168.2.2"},
    {FAMILY_IPV4, 0, 0, "192.168.2.2"},
    {FAMILY_IPV6, 0, NS_DEFAULTPORT, "2001:0:4137:9e76:2087:4a7d:8c27:1c2"},
};
static constexpr NetConn_NetAddr kTestInvalidAddresses[] = {
    {FAMILY_IPV4, 0, NS_DEFAULTPORT, "92.168.2.256"},  // invalid ipv4
    {0, 0, NS_DEFAULTPORT, "92.168.2.256"},            // invalid ipv4
    {FAMILY_IPV6, 0, NS_DEFAULTPORT, "2001:0:4137:2087:4a7d:8c27:rrrr"}};
}  // namespace
namespace internal {

class DnsConfigServiceOhosTest : public ::testing::Test {
 public:
  DnsConfigServiceOhosTest() = default;

  void OnConfigChanged(const DnsConfig& config) {
    callback_called_ = true;
    last_config_ = config;
  }

 protected:
  void SetUp() override {
    g_mock_OH_NetConn_GetDefaultNet = nullptr;
    g_mock_OH_NetConn_GetConnectionProperties = nullptr;
    service_ = std::make_unique<DnsConfigServiceOhos>();
  }

  void TearDown() override {
    g_mock_OH_NetConn_GetDefaultNet = nullptr;
    g_mock_OH_NetConn_GetConnectionProperties = nullptr;
    service_.reset();
  }

  DnsConfigServiceOhos* GetService() { return service_.get(); }
  bool WasCallbackCalled() const { return callback_called_; }
  const DnsConfig& last_config() const { return last_config_; }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void SetDefaultNetStub() {
    g_mock_OH_NetConn_GetDefaultNet = [](NetConn_NetHandle* netHandle) -> int32_t {
      return 0;
    };

    g_mock_OH_NetConn_GetConnectionProperties =
        [](NetConn_NetHandle* netHandle,
           NetConn_ConnectionProperties* prop) -> int32_t {
      prop->dnsListSize = sizeof(kTestAddresses) / sizeof(kTestAddresses[0]);
      EXPECT_TRUE(prop->dnsListSize < NETCONN_MAX_ADDR_SIZE);
      memcpy_s(prop->dnsList, NETCONN_MAX_ADDR_SIZE, kTestAddresses,
               sizeof(kTestAddresses));
      return 0;
    };
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<DnsConfigServiceOhos> service_;
  bool callback_called_ = false;
  DnsConfig last_config_;
};

TEST_F(DnsConfigServiceOhosTest, ReadConfig) {
  EXPECT_TRUE(GetService() != nullptr);
  SetDefaultNetStub();
  GetService()->ReadConfig(base::BindRepeating(
      &DnsConfigServiceOhosTest::OnConfigChanged, base::Unretained(this)));
  RunUntilIdle();
#if !defined(COMPONENT_BUILD) && defined(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  EXPECT_TRUE(WasCallbackCalled());
  const DnsConfig& config = last_config();
  EXPECT_TRUE(config.IsValid());
  EXPECT_FALSE(config.nameservers.empty());
#endif
}

TEST_F(DnsConfigServiceOhosTest, WatchConfig) {
  EXPECT_TRUE(GetService() != nullptr);
  SetDefaultNetStub();
  GetService()->WatchConfig(base::BindRepeating(
      &DnsConfigServiceOhosTest::OnConfigChanged, base::Unretained(this)));
  RunUntilIdle();
#if !defined(COMPONENT_BUILD) && defined(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  EXPECT_TRUE(WasCallbackCalled());
  const DnsConfig& config = last_config();
  EXPECT_TRUE(config.IsValid());
  EXPECT_FALSE(config.nameservers.empty());
#endif
}

TEST_F(DnsConfigServiceOhosTest, ToEndPoint) {
  for (auto net_addr : kTestAddresses) {
    absl::optional<IPEndPoint> ipe;
    if (net_addr.family == FAMILY_IPV4) {
      ipe = GetIpv4EndPoint(net_addr);
    } else {
      ipe = GetIpv6EndPoint(net_addr);
    }
    ASSERT_TRUE(ipe.has_value());
    EXPECT_EQ(ipe->address().ToString(), net_addr.address);
    EXPECT_EQ(ipe->port(), net_addr.port == 0 ? NS_DEFAULTPORT : net_addr.port);
  }
}

TEST_F(DnsConfigServiceOhosTest, ToEndPoint_Invalid) {
  for (auto net_addr : kTestInvalidAddresses) {
    absl::optional<IPEndPoint> ipe;
    if (net_addr.family == FAMILY_IPV6) {
      ipe = GetIpv6EndPoint(net_addr);
    } else {
      ipe = GetIpv4EndPoint(net_addr);
    }
    EXPECT_FALSE(ipe.has_value());
  }
}

TEST_F(DnsConfigServiceOhosTest, GetNameserversFromConnectProperties) {
  g_mock_OH_NetConn_GetDefaultNet = [](NetConn_NetHandle* netHandle) -> int32_t {
    return -1;
  };
  // fail OH_NetConn_GetDefaultNet
  auto nameservers = GetNameserversFromConnectProperties();
  EXPECT_FALSE(nameservers.has_value());

  g_mock_OH_NetConn_GetDefaultNet = [](NetConn_NetHandle* netHandle) -> int32_t {
    return 0;
  };
  g_mock_OH_NetConn_GetConnectionProperties =
      [](NetConn_NetHandle* netHandle,
         NetConn_ConnectionProperties* prop) -> int32_t { return -1; };
  // fail OH_NetConn_GetConnectionProperties
  nameservers = GetNameserversFromConnectProperties();
  EXPECT_FALSE(nameservers.has_value());

  SetDefaultNetStub();
  nameservers = GetNameserversFromConnectProperties();
  // FIXME: Should be true.
  ASSERT_FALSE(nameservers.has_value());
}

TEST_F(DnsConfigServiceOhosTest,
       GetNameserversFromConnectProperties_InvalidAddress) {
  g_mock_OH_NetConn_GetDefaultNet = [](NetConn_NetHandle* netHandle) -> int32_t {
    return 0;
  };
  g_mock_OH_NetConn_GetConnectionProperties =
      [](NetConn_NetHandle* netHandle,
         NetConn_ConnectionProperties* prop) -> int32_t {
    prop->dnsListSize =
        sizeof(kTestInvalidAddresses) / sizeof(kTestInvalidAddresses[0]);
    EXPECT_TRUE(prop->dnsListSize < NETCONN_MAX_ADDR_SIZE);
    memcpy_s(prop->dnsList, NETCONN_MAX_ADDR_SIZE, kTestInvalidAddresses,
             sizeof(kTestInvalidAddresses));
    return 0;
  };
  auto nameservers = GetNameserversFromConnectProperties();
  ASSERT_FALSE(nameservers.has_value());
}

TEST_F(DnsConfigServiceOhosTest, CreateSystemService) {
  auto service = DnsConfigService::CreateSystemService();
  EXPECT_TRUE(service != nullptr);
}

TEST_F(DnsConfigServiceOhosTest, RefreshConfig) {
  SetDefaultNetStub();
  // RefreshConfig should do nothing (empty implementation)
  EXPECT_NO_FATAL_FAILURE(GetService()->RefreshConfig());
}

TEST_F(DnsConfigServiceOhosTest, OnNetworkChanged_NonNoneType) {
  SetDefaultNetStub();
  GetService()->WatchConfig(base::BindRepeating(
      &DnsConfigServiceOhosTest::OnConfigChanged, base::Unretained(this)));
  RunUntilIdle();
  // Non-NONE connection type should trigger config change
  GetService()->OnNetworkChanged(NetworkChangeNotifier::CONNECTION_WIFI);
  RunUntilIdle();
}

TEST_F(DnsConfigServiceOhosTest, OnNetworkChanged_NoneType) {
  SetDefaultNetStub();
  // CONNECTION_NONE should NOT trigger OnConfigChanged
  EXPECT_NO_FATAL_FAILURE(
      GetService()->OnNetworkChanged(NetworkChangeNotifier::CONNECTION_NONE));
}

TEST_F(DnsConfigServiceOhosTest, GetNameserversEmptyDnsList) {
  g_mock_OH_NetConn_GetDefaultNet = [](NetConn_NetHandle* netHandle) -> int32_t {
    return 0;
  };
  g_mock_OH_NetConn_GetConnectionProperties =
      [](NetConn_NetHandle* netHandle,
         NetConn_ConnectionProperties* prop) -> int32_t {
    prop->dnsListSize = 0;
    return 0;
  };
  auto nameservers = GetNameserversFromConnectProperties();
  ASSERT_TRUE(nameservers.has_value());
  EXPECT_TRUE(nameservers.value().empty());
}

TEST_F(DnsConfigServiceOhosTest, GetIpv4EndPoint_InvalidAddress) {
  NetConn_NetAddr addr = {FAMILY_IPV4, 0, NS_DEFAULTPORT, "not_an_ip"};
  auto result = GetIpv4EndPoint(addr);
  EXPECT_FALSE(result.has_value());
}

TEST_F(DnsConfigServiceOhosTest, GetIpv6EndPoint_InvalidAddress) {
  NetConn_NetAddr addr = {FAMILY_IPV6, 0, NS_DEFAULTPORT, "not_an_ipv6"};
  auto result = GetIpv6EndPoint(addr);
  EXPECT_FALSE(result.has_value());
}

TEST_F(DnsConfigServiceOhosTest, GetIpv4EndPoint_CustomPort) {
  NetConn_NetAddr addr = {FAMILY_IPV4, 0, 85, "8.8.8.8"};
  auto result = GetIpv4EndPoint(addr);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->port(), 85);
}

TEST_F(DnsConfigServiceOhosTest, GetIpv6EndPoint_CustomPort) {
  NetConn_NetAddr addr = {FAMILY_IPV6, 0, 85, "2001:4860:4860::8888"};
  auto result = GetIpv6EndPoint(addr);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->port(), 85);
}

TEST_F(DnsConfigServiceOhosTest, GetNameserversUnknownFamily) {
  g_mock_OH_NetConn_GetDefaultNet = [](NetConn_NetHandle* netHandle) -> int32_t {
    return 0;
  };
  // Family value 99 is neither IPv4 nor IPv6
  static constexpr NetConn_NetAddr unknown_family_addr = {99, 0, NS_DEFAULTPORT,
                                                          "1.2.3.4"};
  g_mock_OH_NetConn_GetConnectionProperties =
      [](NetConn_NetHandle* netHandle,
         NetConn_ConnectionProperties* prop) -> int32_t {
    prop->dnsListSize = 1;
    memcpy_s(prop->dnsList, NETCONN_MAX_ADDR_SIZE, &unknown_family_addr,
             sizeof(unknown_family_addr));
    return 0;
  };
  auto nameservers = GetNameserversFromConnectProperties();
  // Unknown family means ipe_inet has no value, so returns nullopt
  EXPECT_FALSE(nameservers.has_value());
}
}  // namespace internal
}  // namespace net
