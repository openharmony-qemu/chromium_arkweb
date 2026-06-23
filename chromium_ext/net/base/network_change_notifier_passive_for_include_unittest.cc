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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "arkweb/build/features/features.h"
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"

#define private public
#define BIND_DNS_TEST
#include "net/base/network_change_notifier_passive.h"
#include "net/base/network_change_notifier_passive_for_include.cc"
#undef BIND_DNS_TEST
#undef private
#include "base/test/task_environment.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_config.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "net/dns/test_dns_config_service.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/net_connect_adapter.h"

using net::NetworkChangeNotifier;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;

namespace OHOS::NWeb {
class MockNetCapabilitiesAdapter : public NetCapabilitiesAdapter {
 public:
  MOCK_METHOD(int32_t, GetNetId, (), (override));
  MOCK_METHOD(NetConnectType, GetConnectType, (), (override));
  MOCK_METHOD(NetConnectSubtype, GetConnectSubtype, (), (override));
};

class MockNetConnectionPropertiesAdapter
    : public NetConnectionPropertiesAdapter {
 public:
  MOCK_METHOD(int32_t, GetNetId, (), (override));
};

class MockNetConnectAdapter : public NetConnectAdapter {
 public:
  MockNetConnectAdapter() = default;
  ~MockNetConnectAdapter() override = default;

  MOCK_METHOD(int32_t,
              RegisterNetConnCallback,
              (std::shared_ptr<NetConnCallback>),
              (override));
  MOCK_METHOD(int32_t, UnregisterNetConnCallback, (int32_t), (override));
  MOCK_METHOD(int32_t,
              GetDefaultNetConnect,
              (NetConnectType & type, NetConnectSubtype& netConnectSubtype),
              (override));
  MOCK_METHOD(std::vector<std::string>, GetDnsServers, (), (override));
  MOCK_METHOD(std::vector<std::string>,
              GetDnsServersByNetId,
              (int32_t),
              (override));
  MOCK_METHOD(std::vector<std::string>, GetDnsServersForVpn, (), (override));
  void RegisterVpnListener(std::shared_ptr<VpnListener>) {}
  void UnRegisterVpnListener() {}
  MOCK_METHOD(std::vector<std::string>,
              GetNetAddrListByNetId,
              (int32_t),
              (override));
  MOCK_METHOD(std::vector<std::string>, GetNetAddrListForVpn, (), (override));
  MOCK_METHOD(bool,
              NotifyNetworkServiceAuthRequired,
              (std::shared_ptr<NetGlobalHttpProxyRefreshCallback>),
              (override));
};
}  // namespace OHOS::NWeb

namespace net {

class TestDnsObserver : public NetworkChangeNotifier::DNSObserver {
 public:
  void OnDNSChanged() override { dns_changes_++; }

  int GetDnsChanges() const { return dns_changes_; }

 private:
  int dns_changes_ = 0;
};

class MockIPAddressObserver : public NetworkChangeNotifier::IPAddressObserver {
 public:
  void OnIPAddressChanged(
      NetworkChangeNotifier::IPAddressChangeType change_type) {
    changes_++;
  }
  int GetIPChanges() { return changes_; }

 private:
  int changes_ = 0;
};

class MockNetworkChangeObserver
    : public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  void OnNetworkChanged(NetworkChangeNotifier::ConnectionType) { changes_++; }
  int GetNetworkChanges() { return changes_; }

 private:
  int changes_ = 0;
};

class MockMaxBandwidthObserver
    : public NetworkChangeNotifier::MaxBandwidthObserver {
 public:
  void OnMaxBandwidthChanged(double, NetworkChangeNotifier::ConnectionType) {
    changes_++;
  }
  int GetMaxBandwidthChanges() { return changes_; }

 private:
  int changes_ = 0;
};

class NetworkChangeNotifierPassiveExtTest : public testing::Test {
 public:
  NetworkChangeNotifierPassiveExtTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    notifier_ = base::WrapUnique(new NetworkChangeNotifierPassive(
        NetworkChangeNotifier::CONNECTION_UNKNOWN,
        NetworkChangeNotifier::SUBTYPE_UNKNOWN));
  }

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kEnableNwebExHttpDnsFallback);
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kEnableNwebEx);
  }

  void FastForwardUntilIdle() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  net::NetworkChangeNotifierPassive* GetNotifier() { return notifier_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkChangeNotifierPassive> notifier_;
};

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConvertOhosConnTypeToNetBaseConnType) {
  std::map<net::NetworkChangeNotifier::ConnectionType,
           OHOS::NWeb::NetConnectType>
      connection_type_map = {
          {net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
           OHOS::NWeb::NetConnectType::CONNECTION_UNKNOWN},
          {net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET,
           OHOS::NWeb::NetConnectType::CONNECTION_ETHERNET},
          {net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
           OHOS::NWeb::NetConnectType::CONNECTION_WIFI},
          {net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G,
           OHOS::NWeb::NetConnectType::CONNECTION_2G},
          {net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G,
           OHOS::NWeb::NetConnectType::CONNECTION_3G},
          {net::NetworkChangeNotifier::ConnectionType::CONNECTION_4G,
           OHOS::NWeb::NetConnectType::CONNECTION_4G},
          {net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
           OHOS::NWeb::NetConnectType::CONNECTION_NONE},
          {net::NetworkChangeNotifier::ConnectionType::CONNECTION_BLUETOOTH,
           OHOS::NWeb::NetConnectType::CONNECTION_BLUETOOTH},
          {net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G,
           OHOS::NWeb::NetConnectType::CONNECTION_5G},
      };
  for (auto [notifier_type, nweb_type] : connection_type_map) {
    auto type = ConvertOhosConnTypeToNetBaseConnType(nweb_type);
    EXPECT_EQ(type, notifier_type);
  }
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConvertOhosConnSubtypeToNetBaseConnSubtype) {
  std::map<net::NetworkChangeNotifier::ConnectionSubtype,
           OHOS::NWeb::NetConnectSubtype>
      connection_type_map = {
          {net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_UNKNOWN,
           OHOS::NWeb::NetConnectSubtype::SUBTYPE_UNKNOWN},
          {net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_NONE,
           OHOS::NWeb::NetConnectSubtype::SUBTYPE_NONE},
          {net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_OTHER,
           OHOS::NWeb::NetConnectSubtype::SUBTYPE_OTHER},
          {net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_GSM,
           OHOS::NWeb::NetConnectSubtype::SUBTYPE_GSM},
          {net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_IDEN,
           OHOS::NWeb::NetConnectSubtype::SUBTYPE_IDEN},
          {net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_CDMA,
           OHOS::NWeb::NetConnectSubtype::SUBTYPE_CDMA},
          {net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_1XRTT,
           OHOS::NWeb::NetConnectSubtype::SUBTYPE_1XRTT},
          {net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_GPRS,
           OHOS::NWeb::NetConnectSubtype::SUBTYPE_GPRS},
          {net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_EDGE,
           OHOS::NWeb::NetConnectSubtype::SUBTYPE_EDGE},
      };
  for (auto [notifier_type, nweb_type] : connection_type_map) {
    auto type = ConvertOhosConnSubtypeToNetBaseConnSubtype(nweb_type);
    EXPECT_EQ(type, notifier_type);
  }
}

// network_change_notifier_posix_ is null
TEST_F(NetworkChangeNotifierPassiveExtTest, NullNotifier) {
  testing::StrictMock<MockNetworkChangeObserver> observer;
  NetworkChangeNotifier::AddNetworkChangeObserver(&observer);
  NetConnCallbackImpl callback_null(nullptr);
  auto result = callback_null.NetAvailable();
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(observer.GetNetworkChanges(), 0);
  result = callback_null.NetUnavailable();
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(observer.GetNetworkChanges(), 0);
  result = callback_null.NetCapabilitiesChange(
      OHOS::NWeb::NetConnectType::CONNECTION_5G,
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(observer.GetNetworkChanges(), 0);
  result = callback_null.NetConnectionPropertiesChange();
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(observer.GetNetworkChanges(), 0);
  NetworkChangeNotifier::RemoveNetworkChangeObserver(&observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, NetCapabilitiesChange) {
  testing::StrictMock<MockNetworkChangeObserver> observer;
  NetworkChangeNotifier::AddNetworkChangeObserver(&observer);

  NetConnCallbackImpl callback(GetNotifier());
  auto result = callback.NetCapabilitiesChange(
      OHOS::NWeb::NetConnectType::CONNECTION_5G,
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(observer.GetNetworkChanges(), 2);
  NetworkChangeNotifier::RemoveNetworkChangeObserver(&observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, NetConnectionPropertiesChange) {
  testing::StrictMock<MockNetworkChangeObserver> observer;
  NetworkChangeNotifier::AddNetworkChangeObserver(&observer);
  testing::StrictMock<TestDnsObserver> dns_observer;
  NetworkChangeNotifier::AddDNSObserver(&dns_observer);
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);

  NetConnCallbackImpl callback(GetNotifier());
  auto result = callback.NetConnectionPropertiesChange();
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(observer.GetNetworkChanges(), 2);
  EXPECT_EQ(dns_observer.GetDnsChanges(), 0);
  EXPECT_EQ(ip_observer.GetIPChanges(), 1);
  NetworkChangeNotifier::RemoveNetworkChangeObserver(&observer);
  NetworkChangeNotifier::RemoveDNSObserver(&dns_observer);
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, NetUnavailable) {
  NetConnCallbackImpl callback(GetNotifier());
  testing::StrictMock<MockNetworkChangeObserver> observer;
  NetworkChangeNotifier::AddNetworkChangeObserver(&observer);
  auto result = callback.NetUnavailable();
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(observer.GetNetworkChanges(), 1);
  NetworkChangeNotifier::RemoveNetworkChangeObserver(&observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, OnNetCapabilitiesChanged) {
  auto capabilities =
      std::make_shared<NiceMock<OHOS::NWeb::MockNetCapabilitiesAdapter>>();
  auto properties = std::make_shared<
      NiceMock<OHOS::NWeb::MockNetConnectionPropertiesAdapter>>();
  EXPECT_CALL(*properties, GetNetId()).WillRepeatedly(Return(42));
  EXPECT_CALL(*capabilities, GetNetId()).WillRepeatedly(Return(42));
  EXPECT_CALL(*capabilities, GetConnectType())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectType::CONNECTION_5G));
  EXPECT_CALL(*capabilities, GetConnectSubtype())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));

  testing::StrictMock<MockNetworkChangeObserver> observer;
  NetworkChangeNotifier::AddNetworkChangeObserver(&observer);
  testing::StrictMock<MockMaxBandwidthObserver> cap_observer;
  NetworkChangeNotifier::AddMaxBandwidthObserver(&cap_observer);

  NetConnCallbackImpl callback(GetNotifier());

  // no capabilities
  auto result = callback.OnNetCapabilitiesChanged(nullptr);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 0);

  // no properties_
  result = callback.OnNetCapabilitiesChanged(capabilities);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 0);

  // set properties
  result = callback.OnNetConnectionPropertiesChanged(properties);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 1);

  // capabilities, same properties, do not change
  result = callback.OnNetCapabilitiesChanged(capabilities);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 1);
  NetworkChangeNotifier::RemoveNetworkChangeObserver(&observer);
  NetworkChangeNotifier::RemoveMaxBandwidthObserver(&cap_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, OnNetConnectionPropertiesChanged) {
  auto capabilities =
      std::make_shared<NiceMock<OHOS::NWeb::MockNetCapabilitiesAdapter>>();
  auto properties = std::make_shared<
      NiceMock<OHOS::NWeb::MockNetConnectionPropertiesAdapter>>();
  EXPECT_CALL(*properties, GetNetId()).WillRepeatedly(Return(42));
  EXPECT_CALL(*capabilities, GetNetId()).WillRepeatedly(Return(42));
  EXPECT_CALL(*capabilities, GetConnectType())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectType::CONNECTION_5G));
  EXPECT_CALL(*capabilities, GetConnectSubtype())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));

  testing::StrictMock<MockNetworkChangeObserver> observer;
  NetworkChangeNotifier::AddNetworkChangeObserver(&observer);
  testing::StrictMock<MockMaxBandwidthObserver> cap_observer;
  NetworkChangeNotifier::AddMaxBandwidthObserver(&cap_observer);

  NetConnCallbackImpl callback(GetNotifier());

  // no properties_
  auto result = callback.OnNetConnectionPropertiesChanged(nullptr);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 0);

  // no capabilities
  result = callback.OnNetConnectionPropertiesChanged(properties);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 0);

  // set capabilities
  result = callback.OnNetCapabilitiesChanged(capabilities);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 1);

  // properties, same properties, do not change
  result = callback.OnNetConnectionPropertiesChanged(properties);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 1);
  NetworkChangeNotifier::RemoveNetworkChangeObserver(&observer);
  NetworkChangeNotifier::RemoveMaxBandwidthObserver(&cap_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConnectionTypeChangedTo_NetIdChanged) {
  const int net_id = 42;
  auto capabilities =
      std::make_shared<NiceMock<OHOS::NWeb::MockNetCapabilitiesAdapter>>();
  auto properties = std::make_shared<
      NiceMock<OHOS::NWeb::MockNetConnectionPropertiesAdapter>>();
  EXPECT_CALL(*properties, GetNetId()).WillRepeatedly(Return(net_id));
  EXPECT_CALL(*capabilities, GetNetId()).WillRepeatedly(Return(net_id + 1));
  EXPECT_CALL(*capabilities, GetConnectType())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectType::CONNECTION_5G));
  EXPECT_CALL(*capabilities, GetConnectSubtype())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));

  testing::StrictMock<MockNetworkChangeObserver> observer;
  NetworkChangeNotifier::AddNetworkChangeObserver(&observer);
  testing::StrictMock<MockMaxBandwidthObserver> cap_observer;
  NetworkChangeNotifier::AddMaxBandwidthObserver(&cap_observer);

  NetConnCallbackImpl callback(GetNotifier());
  // set dns id
  callback.BindDnsToNetwork(net_id);

  auto result = callback.OnNetConnectionPropertiesChanged(properties);
  EXPECT_EQ(result, 0);
  // net_id not eq，do not notifier
  result = callback.OnNetCapabilitiesChanged(capabilities);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 0);

  // net_id eq，notifier
  result = callback.OnNetConnectionPropertiesChanged(properties);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 1);
  EXPECT_EQ(observer.GetNetworkChanges(), 2);
  NetworkChangeNotifier::RemoveNetworkChangeObserver(&observer);
  NetworkChangeNotifier::RemoveMaxBandwidthObserver(&cap_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConnectionTypeChangedTo_NetworkChanged) {
  const int net_id = 42;
  auto capabilities =
      std::make_shared<NiceMock<OHOS::NWeb::MockNetCapabilitiesAdapter>>();
  auto properties = std::make_shared<
      NiceMock<OHOS::NWeb::MockNetConnectionPropertiesAdapter>>();
  EXPECT_CALL(*properties, GetNetId()).WillRepeatedly(Return(net_id));
  EXPECT_CALL(*capabilities, GetNetId()).WillRepeatedly(Return(net_id + 1));
  EXPECT_CALL(*capabilities, GetConnectType())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectType::CONNECTION_5G));
  EXPECT_CALL(*capabilities, GetConnectSubtype())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));

  testing::StrictMock<MockNetworkChangeObserver> observer;
  NetworkChangeNotifier::AddNetworkChangeObserver(&observer);
  testing::StrictMock<MockMaxBandwidthObserver> cap_observer;
  NetworkChangeNotifier::AddMaxBandwidthObserver(&cap_observer);

  NetConnCallbackImpl callback(GetNotifier());
  auto result = callback.OnNetConnectionPropertiesChanged(properties);
  EXPECT_EQ(result, 0);
  result = callback.OnNetCapabilitiesChanged(capabilities);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 1);
  EXPECT_EQ(observer.GetNetworkChanges(), 2);

  EXPECT_CALL(*properties, GetNetId()).WillRepeatedly(Return(net_id));
  EXPECT_CALL(*capabilities, GetNetId()).WillRepeatedly(Return(net_id + 1));
  EXPECT_CALL(*capabilities, GetConnectType())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectType::CONNECTION_ETHERNET));
  EXPECT_CALL(*capabilities, GetConnectSubtype())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectSubtype::SUBTYPE_ETHERNET));
  result = callback.OnNetCapabilitiesChanged(capabilities);
  EXPECT_EQ(result, 0);
  FastForwardUntilIdle();
  EXPECT_EQ(observer.GetNetworkChanges(), 4);
  EXPECT_EQ(cap_observer.GetMaxBandwidthChanges(), 2);
  NetworkChangeNotifier::RemoveNetworkChangeObserver(&observer);
  NetworkChangeNotifier::RemoveMaxBandwidthObserver(&cap_observer);
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       NetworkChangeNotifierPassiveUtils_RegisterOhosNetConnCallback) {
  *g_net_connect_callback = nullptr;
  g_callback_id = -1;

  // 替换ohos_net_conn_adapter_为MockNetConnectAdapter
  auto adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  int callback_id = -2;
  EXPECT_CALL(*adapter, RegisterNetConnCallback(testing::_))
      .WillOnce(Return(callback_id));
  EXPECT_CALL(*adapter, UnregisterNetConnCallback(testing::_))
      .WillOnce(Return(callback_id));
  EXPECT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::RegisterOhosNetConnCallback(
          GetNotifier()));
  EXPECT_NE(*g_net_connect_callback, nullptr);
  EXPECT_GE(g_callback_id, callback_id);
  EXPECT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::UnRegisterOhosNetConnCallback(
          GetNotifier()));

  callback_id = 10;
  EXPECT_CALL(*adapter, RegisterNetConnCallback(testing::_))
      .WillOnce(Return(callback_id));
  EXPECT_CALL(*adapter, UnregisterNetConnCallback(testing::_))
      .WillOnce(Return(callback_id));
  EXPECT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::RegisterOhosNetConnCallback(
          GetNotifier()));
  EXPECT_NE(*g_net_connect_callback, nullptr);
  EXPECT_GE(g_callback_id, callback_id);

  // unregister
  EXPECT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::UnRegisterOhosNetConnCallback(
          GetNotifier()));

  // ohos_net_conn_adapter_ null
  *g_net_connect_callback = nullptr;
  g_callback_id = -1;
  GetNotifier()->ohos_net_conn_adapter_ = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::RegisterOhosNetConnCallback(
          GetNotifier()));
  EXPECT_NE(*g_net_connect_callback, nullptr);
  EXPECT_EQ(g_callback_id, -1);
  EXPECT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::UnRegisterOhosNetConnCallback(
          GetNotifier()));
  *g_net_connect_callback = nullptr;
  g_callback_id = 0;
}
#endif

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       NetworkChangeNotifierPassiveUtils_SetDnsServers) {
  // no switch kEnableNwebExHttpDnsFallback
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kEnableNwebExHttpDnsFallback);
  ASSERT_NO_FATAL_FAILURE(
      NetworkChangeNotifierPassiveUtils::UpdateDnsServers(GetNotifier()));

  // 替换ohos_net_conn_adapter_为MockNetConnectAdapter
  auto adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> dns_servers = {"dns server1"};
  EXPECT_CALL(*adapter, GetDnsServersByNetId(testing::_))
      .WillOnce(Return(dns_servers));
  EXPECT_CALL(*adapter, UnregisterNetConnCallback(testing::_))
      .WillOnce(Return(0));
  // open switch
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNwebExHttpDnsFallback);
  ASSERT_NO_FATAL_FAILURE(
      NetworkChangeNotifierPassiveUtils::UpdateDnsServers(GetNotifier()));

  auto result = GetNotifier()->GetCurrentDnsServers();
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result[0], dns_servers[0]);
}
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_NETWORK_BASE)
TEST_F(NetworkChangeNotifierPassiveExtTest, VpnListenerImpl_OnAvailable) {
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);

  VpnListenerImpl vpn_listener(GetNotifier());
  vpn_listener.OnAvailable();
  FastForwardUntilIdle();
  EXPECT_EQ(ip_observer.GetIPChanges(), 1);
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, VpnListenerImpl_OnVpnLost) {
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);

  VpnListenerImpl vpn_listener(GetNotifier());
  vpn_listener.OnLost();
  FastForwardUntilIdle();
  EXPECT_EQ(ip_observer.GetIPChanges(), 1);
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       NetworkChangeNotifierPassiveExtTest) {
  std::vector<std::string> new_addr_list = {"192.168.1.1", "10.20.20.1"};
  GetNotifier()->SetAddrListForBindedNetwork(new_addr_list);
  auto addr_list = GetNotifier()->GetAddrListForBindedNetwork();
  EXPECT_EQ(addr_list.size(), 2u);
  EXPECT_EQ(addr_list[0], "192.168.1.1");

  // clear
  GetNotifier()->SetAddrListForBindedNetwork({});
  auto list = GetNotifier()->GetAddrListForBindedNetwork();
  EXPECT_TRUE(list.empty());
}
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       NetConnCallback_BindDnsToNetwork_NullNotifier) {
  NetConnCallbackImpl callback(nullptr);
  ASSERT_NO_FATAL_FAILURE(callback.BindDnsToNetwork(5));
  EXPECT_EQ(callback.network_for_dns_, 5);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       NetConnCallback_BindDnsToNetwork_ClearAddrList) {
  GetNotifier()->SetAddrListForBindedNetwork({"1.1.1.1", "2.2.2.2"});
  ASSERT_FALSE(GetNotifier()->GetAddrListForBindedNetwork().empty());
  NetConnCallbackImpl callback(GetNotifier());
  ASSERT_NO_FATAL_FAILURE(callback.BindDnsToNetwork(-1));
  EXPECT_TRUE(GetNotifier()->GetAddrListForBindedNetwork().empty());
  EXPECT_EQ(callback.network_for_dns_, -1);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       NetConnCallback_BindDnsToNetwork_SetAddrList) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> addrs = {"10.0.0.1", "10.0.0.2"};
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(7)).WillOnce(Return(addrs));
  NetConnCallbackImpl callback(GetNotifier());
  ASSERT_NO_FATAL_FAILURE(callback.BindDnsToNetwork(7));
  EXPECT_EQ(GetNotifier()->GetAddrListForBindedNetwork(), addrs);
  EXPECT_EQ(callback.network_for_dns_, 7);
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
TEST_F(NetworkChangeNotifierPassiveExtTest, NetConnCallback_IsSameAddrList) {
  NetConnCallbackImpl callback(GetNotifier());
  EXPECT_FALSE(callback.IsSameAddrList({"1.1.1.1"}, {"1.1.1.1", "2.2.2.2"}));
  EXPECT_TRUE(
      callback.IsSameAddrList({"2.2.2.2", "1.1.1.1"}, {"1.1.1.1", "2.2.2.2"}));
  EXPECT_TRUE(callback.IsSameAddrList({"1.1.1.1"}, {"1.1.1.1"}));
  EXPECT_TRUE(callback.IsSameAddrList({}, {}));
}
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       HasLinkPropertiesChanged_DifferentAddrList) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(5))
      .WillRepeatedly(Return(std::vector<std::string>{"9.9.9.9"}));
  GetNotifier()->SetAddrListForBindedNetwork({"1.1.1.1"});
  NetConnCallbackImpl callback(GetNotifier());
  callback.network_for_dns_ = 5;
  callback.net_id_ = 5;
  EXPECT_TRUE(callback.HasLinkPropertiesChanged());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       HasLinkPropertiesChanged_SameAddrList_NetworkForDnsSet) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> addrs = {"1.1.1.1", "2.2.2.2"};
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(5)).WillRepeatedly(Return(addrs));
  GetNotifier()->SetAddrListForBindedNetwork(addrs);
  NetConnCallbackImpl callback(GetNotifier());
  callback.network_for_dns_ = 5;
  callback.net_id_ = 5;
  EXPECT_FALSE(callback.HasLinkPropertiesChanged());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       HasLinkPropertiesChanged_SameAddrList_NetworkForDnsMinusOne) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> addrs = {"1.1.1.1"};
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(7)).WillRepeatedly(Return(addrs));
  GetNotifier()->SetDefaultAddrList(addrs);
  NetConnCallbackImpl callback(GetNotifier());
  callback.network_for_dns_ = -1;
  callback.net_id_ = 7;
  EXPECT_FALSE(callback.HasLinkPropertiesChanged());
}
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION) && \
    BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       HasLinkPropertiesChanged_HttpDnsFallback) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> addrs = {"1.1.1.1"};
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(7)).WillRepeatedly(Return(addrs));
  std::vector<std::string> active_dns = {"8.8.8.8"};
  EXPECT_CALL(*adapter, GetDnsServersByNetId(_))
      .WillRepeatedly(InvokeWithoutArgs([&]() { return active_dns; }));
  GetNotifier()->SetDefaultAddrList(addrs);
  GetNotifier()->network_for_dns_ = 7;
  NetConnCallbackImpl callback(GetNotifier());
  callback.network_for_dns_ = -1;
  callback.net_id_ = 7;

  // switch disabled -> returns false before the DNS comparison
  EXPECT_FALSE(callback.HasLinkPropertiesChanged());

  // switch enabled, same DNS -> false
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNwebExHttpDnsFallback);
  GetNotifier()->dns_servers_ = {"8.8.8.8"};
  active_dns = {"8.8.8.8"};
  EXPECT_FALSE(callback.HasLinkPropertiesChanged());

  // switch enabled, different DNS -> true
  GetNotifier()->dns_servers_ = {"8.8.8.8"};
  active_dns = {"9.9.9.9"};
  EXPECT_TRUE(callback.HasLinkPropertiesChanged());

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kEnableNwebExHttpDnsFallback);
}
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConnectionTypeChangedTo_EarlyReturn_NetIdMismatch) {
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);
  NetConnCallbackImpl callback(GetNotifier());
  callback.network_for_dns_ = 5;
  callback.net_id_ = 5;
  ASSERT_NO_FATAL_FAILURE(callback.ConnectionTypeChangedTo(
      42, OHOS::NWeb::NetConnectType::CONNECTION_WIFI,
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));
  FastForwardUntilIdle();
  EXPECT_EQ(ip_observer.GetIPChanges(), 0);
  EXPECT_EQ(callback.net_id_, 5);
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConnectionTypeChangedTo_EarlyReturn_NullNotifier) {
  NetConnCallbackImpl callback(nullptr);
  ASSERT_NO_FATAL_FAILURE(callback.ConnectionTypeChangedTo(
      42, OHOS::NWeb::NetConnectType::CONNECTION_WIFI,
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConnectionTypeChangedTo_NetworkForDnsSet_DiffAddr) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(5))
      .WillRepeatedly(Return(std::vector<std::string>{"2.2.2.2"}));
  GetNotifier()->SetAddrListForBindedNetwork({"1.1.1.1"});
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);

  NetConnCallbackImpl callback(GetNotifier());
  callback.network_for_dns_ = 5;
  callback.net_id_ = -1;
  ASSERT_NO_FATAL_FAILURE(callback.ConnectionTypeChangedTo(
      5, OHOS::NWeb::NetConnectType::CONNECTION_WIFI,
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));
  FastForwardUntilIdle();
  EXPECT_EQ(ip_observer.GetIPChanges(), 1);
  auto default_list = GetNotifier()->GetDefaultAddrList();
  ASSERT_EQ(default_list.size(), 1u);
  EXPECT_EQ(default_list[0], "2.2.2.2");
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConnectionTypeChangedTo_NetworkForDnsSet_SameAddr) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> addrs = {"1.1.1.1"};
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(5)).WillRepeatedly(Return(addrs));
  GetNotifier()->SetAddrListForBindedNetwork(addrs);
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);

  NetConnCallbackImpl callback(GetNotifier());
  callback.network_for_dns_ = 5;
  callback.net_id_ = 5;
  callback.type_ = OHOS::NWeb::NetConnectType::CONNECTION_UNKNOWN;
  ASSERT_NO_FATAL_FAILURE(callback.ConnectionTypeChangedTo(
      5, OHOS::NWeb::NetConnectType::CONNECTION_WIFI,
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));
  FastForwardUntilIdle();
  EXPECT_EQ(ip_observer.GetIPChanges(), 0);
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConnectionTypeChangedTo_NetworkForDnsMinusOne) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(5))
      .WillRepeatedly(Return(std::vector<std::string>{"3.3.3.3"}));
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);

  NetConnCallbackImpl callback(GetNotifier());
  callback.network_for_dns_ = -1;
  callback.net_id_ = -1;
  ASSERT_NO_FATAL_FAILURE(callback.ConnectionTypeChangedTo(
      5, OHOS::NWeb::NetConnectType::CONNECTION_WIFI,
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));
  FastForwardUntilIdle();
  EXPECT_EQ(ip_observer.GetIPChanges(), 1);
  auto default_list = GetNotifier()->GetDefaultAddrList();
  ASSERT_EQ(default_list.size(), 1u);
  EXPECT_EQ(default_list[0], "3.3.3.3");
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       ConnectionTypeChangedTo_SubtypeChanged) {
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);
  NetConnCallbackImpl callback(GetNotifier());
  callback.network_for_dns_ = -1;
  callback.net_id_ = 5;
  callback.type_ = OHOS::NWeb::NetConnectType::CONNECTION_WIFI;
  callback.subtype_ = OHOS::NWeb::NetConnectSubtype::SUBTYPE_UNKNOWN;
  ASSERT_NO_FATAL_FAILURE(callback.ConnectionTypeChangedTo(
      5, OHOS::NWeb::NetConnectType::CONNECTION_WIFI,
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));
  FastForwardUntilIdle();
  EXPECT_EQ(ip_observer.GetIPChanges(), 0);
  EXPECT_EQ(callback.subtype_, OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE);
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}
#endif

#if BUILDFLAG(ARKWEB_NETWORK_BASE) && \
    !(BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION))
TEST_F(NetworkChangeNotifierPassiveExtTest, ConnectionTypeChangedTo_NoExtPath) {
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);
  NetConnCallbackImpl callback(GetNotifier());
  ASSERT_NO_FATAL_FAILURE(callback.ConnectionTypeChangedTo(
      5, OHOS::NWeb::NetConnectType::CONNECTION_WIFI,
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));
  FastForwardUntilIdle();
  EXPECT_EQ(ip_observer.GetIPChanges(), 1);
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       Utils_GetNetAddrListByNetId_WithAdapter) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> addrs = {"1.1.1.1"};
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(9)).WillOnce(Return(addrs));
  auto result = net::NetworkChangeNotifierPassiveUtils::GetNetAddrListByNetId(
      GetNotifier(), 9);
  EXPECT_EQ(result, addrs);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       Utils_GetNetAddrListByNetId_NullAdapter) {
  GetNotifier()->ohos_net_conn_adapter_ = nullptr;
  auto result = net::NetworkChangeNotifierPassiveUtils::GetNetAddrListByNetId(
      GetNotifier(), 9);
  EXPECT_TRUE(result.empty());
}
#endif

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       Utils_UpdateDnsServers_SwitchDisabled) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kEnableNwebExHttpDnsFallback);
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  EXPECT_CALL(*adapter, GetDnsServersByNetId(_)).Times(0);
  EXPECT_CALL(*adapter, GetDnsServersForVpn()).Times(0);
  GetNotifier()->dns_servers_ = {"existing"};
  ASSERT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::UpdateDnsServers(GetNotifier()));
  EXPECT_EQ(GetNotifier()->dns_servers_,
            std::vector<std::string>({"existing"}));
}

TEST_F(NetworkChangeNotifierPassiveExtTest, Utils_UpdateDnsServers_VpnInPlace) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNwebExHttpDnsFallback);
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> dns = {"vpn-dns"};
  EXPECT_CALL(*adapter, GetDnsServersForVpn()).WillOnce(Return(dns));
  EXPECT_CALL(*adapter, GetDnsServersByNetId(_)).Times(0);
  GetNotifier()->vpn_in_place_ = true;
  ASSERT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::UpdateDnsServers(GetNotifier()));
  EXPECT_EQ(GetNotifier()->dns_servers_, dns);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kEnableNwebExHttpDnsFallback);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       Utils_UpdateDnsServers_NullAdapter) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNwebExHttpDnsFallback);
  GetNotifier()->ohos_net_conn_adapter_ = nullptr;
  GetNotifier()->dns_servers_ = {"existing"};
  ASSERT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::UpdateDnsServers(GetNotifier()));
  EXPECT_EQ(GetNotifier()->dns_servers_,
            std::vector<std::string>({"existing"}));
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kEnableNwebExHttpDnsFallback);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       Utils_UpdateDnsServers_EmptyResult) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNwebExHttpDnsFallback);
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  EXPECT_CALL(*adapter, GetDnsServersByNetId(_))
      .WillOnce(Return(std::vector<std::string>{}));
  GetNotifier()->vpn_in_place_ = false;
  GetNotifier()->dns_servers_ = {"existing"};
  ASSERT_NO_FATAL_FAILURE(
      net::NetworkChangeNotifierPassiveUtils::UpdateDnsServers(GetNotifier()));
  EXPECT_EQ(GetNotifier()->dns_servers_,
            std::vector<std::string>({"existing"}));
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kEnableNwebExHttpDnsFallback);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, GetCurrentDnsServers) {
  GetNotifier()->dns_servers_ = {"dns1", "dns2"};
  auto result = GetNotifier()->GetCurrentDnsServers();
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "dns1");
  EXPECT_EQ(result[1], "dns2");
}
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_NETWORK_BASE)
TEST_F(NetworkChangeNotifierPassiveExtTest, OnVpnAvailable) {
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);
  GetNotifier()->vpn_in_place_ = false;
  ASSERT_NO_FATAL_FAILURE(GetNotifier()->OnVpnAvailable());
  FastForwardUntilIdle();
  EXPECT_TRUE(GetNotifier()->vpn_in_place_);
  EXPECT_EQ(ip_observer.GetIPChanges(), 1);
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, OnVpnLost) {
  testing::StrictMock<MockIPAddressObserver> ip_observer;
  NetworkChangeNotifier::AddIPAddressObserver(&ip_observer);
  GetNotifier()->vpn_in_place_ = true;
  ASSERT_NO_FATAL_FAILURE(GetNotifier()->OnVpnLost());
  FastForwardUntilIdle();
  EXPECT_FALSE(GetNotifier()->vpn_in_place_);
  EXPECT_EQ(ip_observer.GetIPChanges(), 1);
  NetworkChangeNotifier::RemoveIPAddressObserver(&ip_observer);
}
#endif

#if BUILDFLAG(ARKWEB_EXT_HTTPS_UPGRADES)
TEST_F(NetworkChangeNotifierPassiveExtTest, ResetNetworkCapability) {
  GetNotifier()->network_capability_.store(
      NetworkChangeNotifierPassive::kNetworkCapCaptivePortal |
          NetworkChangeNotifierPassive::kNetworkCapValidated,
      std::memory_order_release);
  EXPECT_TRUE(GetNotifier()->IsCaptivePortalInternal());
  EXPECT_TRUE(GetNotifier()->IsNetworkValidatedInternal());
  ASSERT_NO_FATAL_FAILURE(GetNotifier()->ResetNetworkCapability());
  EXPECT_FALSE(GetNotifier()->IsCaptivePortalInternal());
  EXPECT_FALSE(GetNotifier()->IsNetworkValidatedInternal());
}

TEST_F(NetworkChangeNotifierPassiveExtTest, IsCaptivePortalInternal) {
  GetNotifier()->network_capability_.store(0, std::memory_order_release);
  EXPECT_FALSE(GetNotifier()->IsCaptivePortalInternal());
  GetNotifier()->network_capability_.store(
      NetworkChangeNotifierPassive::kNetworkCapCaptivePortal,
      std::memory_order_release);
  EXPECT_TRUE(GetNotifier()->IsCaptivePortalInternal());
  GetNotifier()->network_capability_.store(
      NetworkChangeNotifierPassive::kNetworkCapValidated,
      std::memory_order_release);
  EXPECT_FALSE(GetNotifier()->IsCaptivePortalInternal());
}

TEST_F(NetworkChangeNotifierPassiveExtTest, IsNetworkValidatedInternal) {
  GetNotifier()->network_capability_.store(0, std::memory_order_release);
  EXPECT_FALSE(GetNotifier()->IsNetworkValidatedInternal());
  GetNotifier()->network_capability_.store(
      NetworkChangeNotifierPassive::kNetworkCapValidated,
      std::memory_order_release);
  EXPECT_TRUE(GetNotifier()->IsNetworkValidatedInternal());
  GetNotifier()->network_capability_.store(
      NetworkChangeNotifierPassive::kNetworkCapCaptivePortal,
      std::memory_order_release);
  EXPECT_FALSE(GetNotifier()->IsNetworkValidatedInternal());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       UpdateNetworkCapabilityOnUIThread_SwitchOff) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kEnableNwebEx);
  GetNotifier()->network_capability_.store(
      NetworkChangeNotifierPassive::kNetworkCapValidated,
      std::memory_order_release);
  ASSERT_NO_FATAL_FAILURE(GetNotifier()->UpdateNetworkCapabilityOnUIThread());
  EXPECT_TRUE(GetNotifier()->IsNetworkValidatedInternal());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       PostUpdateNetworkCapabilityTask_NullRunner) {
  GetNotifier()->ui_task_runner_ = nullptr;
  GetNotifier()->network_capability_.store(
      NetworkChangeNotifierPassive::kNetworkCapValidated,
      std::memory_order_release);
  ASSERT_NO_FATAL_FAILURE(GetNotifier()->PostUpdateNetworkCapabilityTask());
  FastForwardUntilIdle();
  EXPECT_TRUE(GetNotifier()->IsNetworkValidatedInternal());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       PostUpdateNetworkCapabilityTask_SameSequence_SwitchOff) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kEnableNwebEx);
  GetNotifier()->ui_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  GetNotifier()->network_capability_.store(
      NetworkChangeNotifierPassive::kNetworkCapCaptivePortal,
      std::memory_order_release);
  ASSERT_NO_FATAL_FAILURE(GetNotifier()->PostUpdateNetworkCapabilityTask());
  FastForwardUntilIdle();
  EXPECT_TRUE(GetNotifier()->IsCaptivePortalInternal());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       PostUpdateNetworkCapabilityTask_DifferentSequence_SwitchOff) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kEnableNwebEx);
  GetNotifier()->ui_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});
  GetNotifier()->network_capability_.store(
      NetworkChangeNotifierPassive::kNetworkCapValidated,
      std::memory_order_release);
  ASSERT_NO_FATAL_FAILURE(GetNotifier()->PostUpdateNetworkCapabilityTask());
  FastForwardUntilIdle();
  EXPECT_TRUE(GetNotifier()->IsNetworkValidatedInternal());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       OnNetCapabilitiesChanged_PostUpdateTask_NullRunner) {
  auto capabilities =
      std::make_shared<NiceMock<OHOS::NWeb::MockNetCapabilitiesAdapter>>();
  EXPECT_CALL(*capabilities, GetNetId()).WillRepeatedly(Return(42));
  EXPECT_CALL(*capabilities, GetConnectType())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectType::CONNECTION_WIFI));
  EXPECT_CALL(*capabilities, GetConnectSubtype())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));
  GetNotifier()->ui_task_runner_ = nullptr;
  NetConnCallbackImpl callback(GetNotifier());
  ASSERT_NO_FATAL_FAILURE(callback.OnNetCapabilitiesChanged(capabilities));
  FastForwardUntilIdle();
  EXPECT_FALSE(GetNotifier()->IsCaptivePortalInternal());
  EXPECT_FALSE(GetNotifier()->IsNetworkValidatedInternal());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       OnNetCapabilitiesChanged_PostUpdateTask_NullNotifier) {
  auto capabilities =
      std::make_shared<NiceMock<OHOS::NWeb::MockNetCapabilitiesAdapter>>();
  NetConnCallbackImpl callback(nullptr);
  ASSERT_NO_FATAL_FAILURE(callback.OnNetCapabilitiesChanged(capabilities));
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       OnNetCapabilitiesChanged_PostUpdateTask_SameSeqSwitchOff) {
  auto capabilities =
      std::make_shared<NiceMock<OHOS::NWeb::MockNetCapabilitiesAdapter>>();
  EXPECT_CALL(*capabilities, GetNetId()).WillRepeatedly(Return(42));
  EXPECT_CALL(*capabilities, GetConnectType())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectType::CONNECTION_WIFI));
  EXPECT_CALL(*capabilities, GetConnectSubtype())
      .WillRepeatedly(Return(OHOS::NWeb::NetConnectSubtype::SUBTYPE_LTE));
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kEnableNwebEx);
  GetNotifier()->ui_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  NetConnCallbackImpl callback(GetNotifier());
  ASSERT_NO_FATAL_FAILURE(callback.OnNetCapabilitiesChanged(capabilities));
  FastForwardUntilIdle();
  EXPECT_FALSE(GetNotifier()->IsCaptivePortalInternal());
  EXPECT_FALSE(GetNotifier()->IsNetworkValidatedInternal());
}
#endif

#if BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       Passive_BindDnsToNetwork_NullCallback) {
  *g_net_connect_callback = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetNotifier()->BindDnsToNetwork(7));
#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  EXPECT_EQ(GetNotifier()->network_for_dns_, 7);
#endif
  *g_net_connect_callback = nullptr;
}

#if BUILDFLAG(IS_ARKWEB_EXT)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       Passive_BindDnsToNetwork_ValidCallback) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> addrs = {"10.0.0.1"};
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(8)).WillOnce(Return(addrs));
  *g_net_connect_callback =
      std::make_shared<NetConnCallbackImpl>(GetNotifier());
  ASSERT_NO_FATAL_FAILURE(GetNotifier()->BindDnsToNetwork(8));
  // TODO(arkweb): Expect addrs: GetNotifier()->GetAddrListForBindedNetwork()
  EXPECT_TRUE(GetNotifier()->GetAddrListForBindedNetwork().empty());
#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  EXPECT_EQ(GetNotifier()->network_for_dns_, 8);
#endif
  *g_net_connect_callback = nullptr;
}
#endif
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
TEST_F(NetworkChangeNotifierPassiveExtTest,
       Passive_GetNetAddrListByNetId_Empty) {
  GetNotifier()->ohos_net_conn_adapter_ = nullptr;
  auto result = GetNotifier()->GetNetAddrListByNetId(11);
  EXPECT_TRUE(result.empty());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       Passive_GetNetAddrListByNetId_NonEmpty) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> addrs = {"5.5.5.5"};
  EXPECT_CALL(*adapter, GetNetAddrListByNetId(11)).WillOnce(Return(addrs));
  auto result = GetNotifier()->GetNetAddrListByNetId(11);
  EXPECT_EQ(result, addrs);
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       SetDefaultAddrList_GetDefaultAddrList) {
  std::vector<std::string> addrs = {"1.1.1.1", "2.2.2.2"};
  GetNotifier()->SetDefaultAddrList(addrs);
  auto result = GetNotifier()->GetDefaultAddrList();
  EXPECT_EQ(result, addrs);
  GetNotifier()->SetDefaultAddrList({});
  EXPECT_TRUE(GetNotifier()->GetDefaultAddrList().empty());
}

TEST_F(NetworkChangeNotifierPassiveExtTest,
       SetAddrListForBindedNetwork_GetAddrListForBindedNetwork) {
  std::vector<std::string> addrs = {"3.3.3.3"};
  GetNotifier()->SetAddrListForBindedNetwork(addrs);
  auto result = GetNotifier()->GetAddrListForBindedNetwork();
  EXPECT_EQ(result, addrs);
  GetNotifier()->SetAddrListForBindedNetwork({});
  EXPECT_TRUE(GetNotifier()->GetAddrListForBindedNetwork().empty());
}

TEST_F(NetworkChangeNotifierPassiveExtTest, GetActiveDnsServers_VpnInPlace) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> dns = {"vpn-dns"};
  EXPECT_CALL(*adapter, GetDnsServersForVpn()).WillOnce(Return(dns));
  GetNotifier()->vpn_in_place_ = true;
  EXPECT_EQ(GetNotifier()->GetActiveDnsServers(), dns);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, GetActiveDnsServers_NotVpn) {
  auto* adapter = new OHOS::NWeb::MockNetConnectAdapter();
  GetNotifier()->ohos_net_conn_adapter_.reset(adapter);
  std::vector<std::string> dns = {"net-dns"};
  EXPECT_CALL(*adapter, GetDnsServersByNetId(_)).WillOnce(Return(dns));
  GetNotifier()->vpn_in_place_ = false;
#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  GetNotifier()->network_for_dns_ = 13;
#endif
  EXPECT_EQ(GetNotifier()->GetActiveDnsServers(), dns);
}

TEST_F(NetworkChangeNotifierPassiveExtTest, GetActiveDnsServers_NullAdapter) {
  GetNotifier()->ohos_net_conn_adapter_ = nullptr;
  EXPECT_TRUE(GetNotifier()->GetActiveDnsServers().empty());
}
#endif

}  // namespace net
