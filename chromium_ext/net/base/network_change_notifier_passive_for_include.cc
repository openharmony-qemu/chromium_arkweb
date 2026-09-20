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

namespace {

#if BUILDFLAG(ARKWEB_NETWORK_BASE)
net::NetworkChangeNotifier::ConnectionType ConvertOhosConnTypeToNetBaseConnType(
    const OHOS::NWeb::NetConnectType& netConnectType) {
  return static_cast<net::NetworkChangeNotifier::ConnectionType>(
      netConnectType);
}

net::NetworkChangeNotifier::ConnectionSubtype
ConvertOhosConnSubtypeToNetBaseConnSubtype(
    const OHOS::NWeb::NetConnectSubtype& subtype) {
  return static_cast<net::NetworkChangeNotifier::ConnectionSubtype>(subtype);
}

#if BUILDFLAG(IS_ARKWEB_EXT)
class VpnListenerImpl : public OHOS::NWeb::VpnListener {
 public:
  VpnListenerImpl(net::NetworkChangeNotifierPassive* network_change_notifier)
      : network_change_notifier_(network_change_notifier) {}
  virtual ~VpnListenerImpl() = default;
  void OnAvailable() override;
  void OnLost() override;

 private:
  raw_ptr<net::NetworkChangeNotifierPassive> network_change_notifier_ = nullptr;
};

void VpnListenerImpl::OnAvailable() {
  LOG(INFO) << "ohos_network Vpn OnAvailable";
  network_change_notifier_->OnVpnAvailable();
}

void VpnListenerImpl::OnLost() {
  LOG(INFO) << "ohos_network Vpn OnLost";
  network_change_notifier_->OnVpnLost();
}
#endif

class NetConnCallbackImpl : public OHOS::NWeb::NetConnCallback {
 public:
  NetConnCallbackImpl(
      net::NetworkChangeNotifierPassive* network_change_notifier_posix)
      : network_change_notifier_posix_(network_change_notifier_posix) {}
  virtual ~NetConnCallbackImpl() = default;
  int32_t NetAvailable() override;
  int32_t NetCapabilitiesChange(
      const OHOS::NWeb::NetConnectType& netConnectType,
      const OHOS::NWeb::NetConnectSubtype& netConnectSubtype) override;
  int32_t NetConnectionPropertiesChange() override;
  int32_t NetUnavailable() override;
  int32_t OnNetCapabilitiesChanged(
      const std::shared_ptr<OHOS::NWeb::NetCapabilitiesAdapter> capabilities)
      override;
  int32_t OnNetConnectionPropertiesChanged(
      const std::shared_ptr<OHOS::NWeb::NetConnectionPropertiesAdapter>
          properties) override;
  void BindDnsToNetwork(int32_t network_for_dns);

 private:
  void ConnectionTypeChangedTo(int32_t net_id,
                               OHOS::NWeb::NetConnectType type,
                               OHOS::NWeb::NetConnectSubtype subtype);
  bool HasLinkPropertiesChanged();

  raw_ptr<net::NetworkChangeNotifierPassive> network_change_notifier_posix_ = nullptr;
  std::shared_ptr<OHOS::NWeb::NetCapabilitiesAdapter> capabilities_ = nullptr;
  std::shared_ptr<OHOS::NWeb::NetConnectionPropertiesAdapter> properties_ =
      nullptr;
  int32_t net_id_ = -1;
  std::atomic<int32_t> network_for_dns_{-1};
  OHOS::NWeb::NetConnectType type_ =
      OHOS::NWeb::NetConnectType::CONNECTION_UNKNOWN;
  OHOS::NWeb::NetConnectSubtype subtype_ =
      OHOS::NWeb::NetConnectSubtype::SUBTYPE_UNKNOWN;
  bool IsSameAddrList(const std::vector<std::string>& newList,
                      const std::vector<std::string>& oldList);
  bool IsOnlyIPv6Added(const std::vector<std::string>& new_list,
                       const std::vector<std::string>& old_list);
  bool skip_ip_changed_for_ipv6_slaac_ = false;
  void MaybeNotifyIPAddressChanged();
};

int32_t NetConnCallbackImpl::NetAvailable() {
  LOG_FEEDBACK(INFO, kNetwork) << __func__;
  capabilities_ = nullptr;
  properties_ = nullptr;
  return 0;
}

int32_t NetConnCallbackImpl::NetCapabilitiesChange(
    const OHOS::NWeb::NetConnectType& netConnectType,
    const OHOS::NWeb::NetConnectSubtype& netConnectSubtype) {
  LOG(INFO) << "ohos_network NetCapabilitiesChange "
            << static_cast<int>(netConnectType);
  if (network_change_notifier_posix_) {
    network_change_notifier_posix_->OnConnectionChanged(
        ConvertOhosConnTypeToNetBaseConnType(netConnectType));
    network_change_notifier_posix_->OnConnectionSubtypeChanged(
        ConvertOhosConnTypeToNetBaseConnType(netConnectType),
        ConvertOhosConnSubtypeToNetBaseConnSubtype(netConnectSubtype));
  }
  return 0;
}

int32_t NetConnCallbackImpl::NetConnectionPropertiesChange() {
  if (network_change_notifier_posix_) {
    skip_ip_changed_for_ipv6_slaac_ = false;
#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION) && \
    BUILDFLAG(ARKWEB_NETWORK_IPV6_SLAAC_FILTER)
    auto cur_addr_list = network_change_notifier_posix_->GetDefaultAddrList();
    auto new_addr_list = network_change_notifier_posix_->GetNetAddrListByNetId(net_id_);
    if (!IsSameAddrList(new_addr_list, cur_addr_list) &&
        IsOnlyIPv6Added(new_addr_list, cur_addr_list)) {
      skip_ip_changed_for_ipv6_slaac_ = true;
      LOG(INFO) << "NetConnectionPropertiesChange: only IPv6 added, skip OnIPAddressChanged.";
    }
#endif
    network_change_notifier_posix_->OnDNSChanged();
    MaybeNotifyIPAddressChanged();
  }
  return 0;
}

int32_t NetConnCallbackImpl::NetUnavailable() {
  LOG_FEEDBACK(INFO, kNetwork) << __func__;
  capabilities_ = nullptr;
  properties_ = nullptr;
  if (network_change_notifier_posix_) {
    ConnectionTypeChangedTo(-1, OHOS::NWeb::NetConnectType::CONNECTION_NONE,
                            OHOS::NWeb::NetConnectSubtype::SUBTYPE_NONE);
  }
  return 0;
}

int32_t NetConnCallbackImpl::OnNetCapabilitiesChanged(
    const std::shared_ptr<OHOS::NWeb::NetCapabilitiesAdapter> capabilities) {
  capabilities_ = capabilities;
  if (capabilities_) {
    LOG_FEEDBACK(INFO, kNetwork)
        << __func__ << " netId:" << capabilities_->GetNetId()
        << " connectType:" << (int)capabilities_->GetConnectType()
        << " connectSubType:" << (int)capabilities_->GetConnectSubtype();
  }
  if (network_change_notifier_posix_ && capabilities_ && properties_) {
    ConnectionTypeChangedTo(capabilities_->GetNetId(),
                            capabilities_->GetConnectType(),
                            capabilities_->GetConnectSubtype());
  }
#if BUILDFLAG(ARKWEB_EXT_HTTPS_UPGRADES)
  if (network_change_notifier_posix_) {
    network_change_notifier_posix_->PostUpdateNetworkCapabilityTask();
  }
#endif
  return 0;
}

int32_t NetConnCallbackImpl::OnNetConnectionPropertiesChanged(
    const std::shared_ptr<OHOS::NWeb::NetConnectionPropertiesAdapter>
        properties) {
  properties_ = properties;
  if (properties_ && capabilities_) {
    LOG_FEEDBACK(INFO, kNetwork)
        << __func__ << " netId:" << properties_->GetNetId()
        << " connectType:" << (int)capabilities_->GetConnectType()
        << " connectSubType:" << (int)capabilities_->GetConnectSubtype();
  }
  if (network_change_notifier_posix_ && capabilities_ && properties_) {
    ConnectionTypeChangedTo(properties_->GetNetId(),
                            capabilities_->GetConnectType(),
                            capabilities_->GetConnectSubtype());
  }
  return 0;
}

void NetConnCallbackImpl::BindDnsToNetwork(int32_t network_for_dns) {
  LOG_FEEDBACK(INFO, kNetwork)
      << __func__ << " networkForDns:" << network_for_dns;
  network_for_dns_.store(network_for_dns, std::memory_order_relaxed);
#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
  if (!network_change_notifier_posix_) {
      return;
  }

  int32_t dns_net = network_for_dns_.load(std::memory_order_relaxed);
  if (dns_net == -1) {
      network_change_notifier_posix_->SetAddrListForBindedNetwork(
          std::vector<std::string>());
      return;
  }
  auto new_addr_list =
      network_change_notifier_posix_->GetNetAddrListByNetId(dns_net);
  network_change_notifier_posix_->SetAddrListForBindedNetwork(new_addr_list);
#endif
}

bool NetConnCallbackImpl::IsSameAddrList(const std::vector<std::string>& newList,
                                         const std::vector<std::string>& oldList) {
  if (newList.size() != oldList.size()) {
    return false;
  }
  std::vector<std::string> sortedNewList = newList;
  std::vector<std::string> sortedOldList = oldList;
  std::sort(sortedNewList.begin(), sortedNewList.end());
  std::sort(sortedOldList.begin(), sortedOldList.end());
  return sortedNewList == sortedOldList;
}

bool NetConnCallbackImpl::IsOnlyIPv6Added(
    const std::vector<std::string>& new_list,
    const std::vector<std::string>& old_list) {
  if (new_list.size() <= old_list.size() || old_list.empty()) {
    return false;
  }

  std::vector<std::string> new_v4;
  std::vector<std::string> new_v6;
  std::vector<std::string> old_v4;
  std::vector<std::string> old_v6;

  for (const auto& ip : new_list) {
    (ip.find(':') != std::string::npos ? new_v6 : new_v4).push_back(ip);
  }

  for (const auto& ip : old_list) {
    (ip.find(':') != std::string::npos ? old_v6 : old_v4).push_back(ip);
  }

  std::sort(new_v4.begin(), new_v4.end());
  std::sort(old_v4.begin(), old_v4.end());
  std::sort(new_v6.begin(), new_v6.end());
  std::sort(old_v6.begin(), old_v6.end());

  if (new_v4 != old_v4) {
    return false;
  }

  return std::includes(new_v6.begin(), new_v6.end(),
                       old_v6.begin(), old_v6.end());
}

void NetConnCallbackImpl::MaybeNotifyIPAddressChanged() {
  bool skip = false;
#if BUILDFLAG(ARKWEB_NETWORK_IPV6_SLAAC_FILTER)
  skip = skip_ip_changed_for_ipv6_slaac_ &&
      base::FeatureList::IsEnabled(net::features::kArkWebIPv6SlaacFilter);
  skip_ip_changed_for_ipv6_slaac_ = false;
#endif
  if (!skip) {
    network_change_notifier_posix_->OnIPAddressChanged(
        net::NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL);
  } else {
    LOG(INFO) << "Only IPv6 added, skip OnIPAddressChanged.";
  }
}

// Check if the link properties has changed(IP/Nameserver).
bool NetConnCallbackImpl::HasLinkPropertiesChanged() {
  skip_ip_changed_for_ipv6_slaac_ = false;

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
  std::vector<std::string> cur_addr_list;
  // Use addrlist for binded network if network_for_dns_ is not -1.
  if (network_for_dns_.load(std::memory_order_relaxed) != -1) {
    cur_addr_list = network_change_notifier_posix_->GetAddrListForBindedNetwork();
  } else {
    cur_addr_list = network_change_notifier_posix_->GetDefaultAddrList();
  }
  auto new_addr_list = network_change_notifier_posix_->GetNetAddrListByNetId(net_id_);
  if (!IsSameAddrList(new_addr_list, cur_addr_list)) {
#if BUILDFLAG(ARKWEB_NETWORK_IPV6_SLAAC_FILTER)
    if (IsOnlyIPv6Added(new_addr_list, cur_addr_list)) {
      skip_ip_changed_for_ipv6_slaac_ = true;
      LOG(INFO) << "HasLinkPropertiesChanged: only IPv6 addresses added";
    }
#endif
    return true;
  }

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNwebExHttpDnsFallback)) {
    return false;
  }

  if (!IsSameAddrList(network_change_notifier_posix_->GetCurrentDnsServers(),
      network_change_notifier_posix_->GetActiveDnsServers())) {
#if BUILDFLAG(ARKWEB_NETWORK_IPV6_SLAAC_FILTER)
    if (IsOnlyIPv6Added(network_change_notifier_posix_->GetActiveDnsServers(),
                        network_change_notifier_posix_->GetCurrentDnsServers())) {
      skip_ip_changed_for_ipv6_slaac_ = true;
      LOG(INFO) << "HasLinkPropertiesChanged: only IPv6 DNS servers added";
    }
#endif
    return true;
  }
#endif  // ARKWEB_EXT_HTTP_DNS_FALLBACK && IS_ARKWEB_EXT
#endif  // ARKWEB_EXT_NETWORK_CONNECTION
 
  return false;
}

void NetConnCallbackImpl::ConnectionTypeChangedTo(
    int32_t net_id,
    OHOS::NWeb::NetConnectType type,
    OHOS::NWeb::NetConnectSubtype subtype) {
  int32_t network_for_dns = network_for_dns_.load(std::memory_order_relaxed);
  if (network_for_dns != -1 && net_id != network_for_dns) {
    LOG(INFO) << "ohos_network ConnectionTypeChangedTo ret, net_id " << net_id
              << ", network_for_dns_ " << network_for_dns;
    return;
  }

  if (!network_change_notifier_posix_) {
      return;
  }

  if (net_id_ != net_id || type_ != type || HasLinkPropertiesChanged()) {
      LOG(INFO) << "ohos_network ConnectionTypeChangedTo, net_id_ " << net_id_
                << ", net_id " << net_id << ", type_ " << (int)type_
                << ", type " << (int)type << ", network_for_dns_ "
                << network_for_dns;
      net_id_ = net_id;
      type_ = type;
#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
    auto new_addr_list = network_change_notifier_posix_->GetNetAddrListByNetId(net_id_);
    if (network_for_dns != -1) {
      auto addr_list_for_binded_network = network_change_notifier_posix_->GetAddrListForBindedNetwork();
      if (!IsSameAddrList(new_addr_list, addr_list_for_binded_network)) {
        MaybeNotifyIPAddressChanged();
        network_change_notifier_posix_->SetDefaultAddrList(new_addr_list);
      } else {
        LOG(INFO) << "The current addr_list_for_binded_network is same with the new net_addr_list, "
                  << "which will not trigger OnIPAddressChanged event.";
      }
    } else {
      MaybeNotifyIPAddressChanged();
      network_change_notifier_posix_->SetDefaultAddrList(new_addr_list);
    }
    network_change_notifier_posix_->OnConnectionChanged(
        ConvertOhosConnTypeToNetBaseConnType(type));
#else
    MaybeNotifyIPAddressChanged();
    network_change_notifier_posix_->OnConnectionChanged(
                                    ConvertOhosConnTypeToNetBaseConnType(type));
#endif
  }

  if (subtype_ != subtype) {
    LOG(INFO) << "ohos_network ConnectionTypeChangedTo, net_id_ " << net_id_
              << ", subtype_ " << (int)subtype_ << ", subtype " << (int)subtype;
    subtype_ = subtype;
    network_change_notifier_posix_->OnConnectionSubtypeChanged(
        ConvertOhosConnTypeToNetBaseConnType(type),
        ConvertOhosConnSubtypeToNetBaseConnSubtype(subtype));
  }
}

base::NoDestructor<std::shared_ptr<NetConnCallbackImpl>> g_net_connect_callback(nullptr);
#if BUILDFLAG(IS_ARKWEB_EXT)
base::NoDestructor<std::shared_ptr<VpnListenerImpl>> g_vpn_listener(nullptr);
#endif
std::atomic<int32_t> g_callback_id{-1};
#endif

}  // namespace

namespace net {

class NetworkChangeNotifierPassiveUtils {
 public:
#if BUILDFLAG(ARKWEB_NETWORK_BASE)
  static void RegisterOhosNetConnCallback(
      raw_ptr<NetworkChangeNotifierPassive> obj) {
    *g_net_connect_callback = std::make_shared<NetConnCallbackImpl>(obj);
#if BUILDFLAG(IS_ARKWEB_EXT)
    *g_vpn_listener = std::make_shared<VpnListenerImpl>(obj);
#endif
    if (obj->ohos_net_conn_adapter_) {
      g_callback_id.store(obj->ohos_net_conn_adapter_->RegisterNetConnCallback(
                              *g_net_connect_callback),
                          std::memory_order_relaxed);
      if (g_callback_id.load(std::memory_order_relaxed) < 0) {
        LOG(ERROR) << "register ohos net connect callback failed.";
      }
#if BUILDFLAG(IS_ARKWEB_EXT)
      obj->ohos_net_conn_adapter_->RegisterVpnListener(*g_vpn_listener);
#endif
    }
  }

  static void UnRegisterOhosNetConnCallback(
      raw_ptr<NetworkChangeNotifierPassive> obj) {
    if (obj->ohos_net_conn_adapter_) {
      int32_t ret = obj->ohos_net_conn_adapter_->UnregisterNetConnCallback(
          g_callback_id.load(std::memory_order_relaxed));
      if (ret != 0) {
        LOG(ERROR) << "unregister ohos net connect callback failed.";
      }
      obj->ohos_net_conn_adapter_->UnRegisterVpnListener();
    }
  }
#endif

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  static void UpdateDnsServers(raw_ptr<NetworkChangeNotifierPassive> obj) {
    std::vector<std::string> dns_servers;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableNwebExHttpDnsFallback)) {
      if (obj->ohos_net_conn_adapter_) {
        LOG(INFO) << "OnConnectionChanged, network_for_dns_ "
                  << obj->network_for_dns_;
        if (obj->vpn_in_place_.load(std::memory_order_relaxed)) {
          dns_servers = obj->ohos_net_conn_adapter_->GetDnsServersForVpn();
        } else {
          dns_servers = obj->ohos_net_conn_adapter_->GetDnsServersByNetId(obj->network_for_dns_);
        }
      }
    }
    {
      base::AutoLock scoped_lock(obj->dns_server_lock_);
      if (!dns_servers.empty()) {
      obj->dns_servers_ = std::move(dns_servers);
      } else {
        LOG(ERROR) << "OnConnectionChanged, ohos_network dns server is empty.";
      }
    }
  }
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
  static std::vector<std::string> GetNetAddrListByNetId(raw_ptr<NetworkChangeNotifierPassive> obj, int32_t netId) {
    std::vector<std::string> net_addr_list;
      if (obj->ohos_net_conn_adapter_) {
        LOG(INFO) << "OnConnectionChanged, GetNetAddrListByNetId, netId "
                  << netId;
        net_addr_list = obj->ohos_net_conn_adapter_->GetNetAddrListByNetId(netId);
    }
   return net_addr_list;
  }
#endif
};

#ifndef BIND_DNS_TEST
#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
const std::vector<std::string>
NetworkChangeNotifierPassive::GetCurrentDnsServers() {
  base::AutoLock scoped_lock(dns_server_lock_);
  return dns_servers_;
}
#endif

#if BUILDFLAG(IS_ARKWEB_EXT) && BUILDFLAG(ARKWEB_NETWORK_BASE)
void NetworkChangeNotifierPassive::OnVpnAvailable() {
  LOG(INFO) << "ohos_network on vpn available";
  vpn_in_place_.store(true, std::memory_order_relaxed);
  OnIPAddressChanged(net::NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL);
  OnConnectionChanged(connection_type_);
}

void NetworkChangeNotifierPassive::OnVpnLost() {
  LOG(INFO) << "ohos_network on vpn lost";
  vpn_in_place_.store(false, std::memory_order_relaxed);
  OnIPAddressChanged(net::NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL);
  OnConnectionChanged(connection_type_);
}
#endif

#if BUILDFLAG(ARKWEB_EXT_HTTPS_UPGRADES)
void NetworkChangeNotifierPassive::ResetNetworkCapability() {
  network_capability_.store(0, std::memory_order_release);
}

void NetworkChangeNotifierPassive::PostUpdateNetworkCapabilityTask() {
  if (!ui_task_runner_) {
    return;
  }

  if (ui_task_runner_->RunsTasksInCurrentSequence()) {
    UpdateNetworkCapabilityOnUIThread();
    return;
  }
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NetworkChangeNotifierPassive::UpdateNetworkCapabilityOnUIThread,
          weak_ptr_factory_.GetWeakPtr()));
}

void NetworkChangeNotifierPassive::UpdateNetworkCapabilityOnUIThread() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableNwebEx)) {
    return;
  }

  NetConn_NetHandle net_handle = {};
  int32_t ret = OH_NetConn_GetDefaultNet(&net_handle);
  if (ret != 0) {
    LOG_FEEDBACK(WARNING, kNetwork)
        << "UpdateNetworkCapability ret:" << ret
        << " message:getDefaultNetFailed";
    ResetNetworkCapability();
    return;
  }
  NetConn_NetCapabilities netCapabilities;
  ret = OH_NetConn_GetNetCapabilities(&net_handle, &netCapabilities);
  if (ret != 0) {
    LOG_FEEDBACK(WARNING, kNetwork) << "UpdateNetworkCapability ret:" << ret
                                    << " netHandle:" << net_handle.netId
                                    << " message:getNetCapabilitiesFailed";
    ResetNetworkCapability();
    return;
  }

  uint8_t old_caps = network_capability_.load(std::memory_order_acquire);

  uint8_t new_caps = 0;
  if (netCapabilities.netCapsSize < 0 ||
      netCapabilities.netCapsSize > NETCONN_MAX_CAP_SIZE) {
    LOG_FEEDBACK(ERROR, kNetwork)
        << "Invalid netCapsSize:" << netCapabilities.netCapsSize;
    return;
  }
  if (netCapabilities.bearerTypesSize < 0 ||
      netCapabilities.bearerTypesSize > NETCONN_MAX_BEARER_TYPE_SIZE) {
    LOG_FEEDBACK(ERROR, kNetwork)
        << "Invalid bearerTypesSize:" << netCapabilities.bearerTypesSize;
    return;
  }

  for (int32_t i = 0; i < netCapabilities.netCapsSize; i++) {
    if (netCapabilities.netCaps[i] == NETCONN_NET_CAPABILITY_PORTAL) {
      new_caps |= kNetworkCapCaptivePortal;
    } else if (netCapabilities.netCaps[i] == NETCONN_NET_CAPABILITY_VALIDATED) {
      new_caps |= kNetworkCapValidated;
    }
  }

  network_capability_.store(new_caps, std::memory_order_release);

  auto convert_array_to_string = [](const auto* array, int32_t size) {
    std::string result = "[";
    for (int32_t i = 0; i < size; ++i) {
      if (i > 0) {
        result += ",";
      }
      result += std::to_string(array[i]);
    }
    result += "]";
    return result;
  };

  std::string net_caps_str = convert_array_to_string(
      netCapabilities.netCaps, netCapabilities.netCapsSize);
  std::string bearer_types_str = convert_array_to_string(
      netCapabilities.bearerTypes, netCapabilities.bearerTypesSize);

  bool bearer_types_changed = (last_bearer_types_str_ != bearer_types_str);
  if (old_caps != new_caps || bearer_types_changed) {
    LOG_FEEDBACK(INFO, kNetwork)
        << "UpdateNetworkCapability isCaptivePortal:"
        << static_cast<bool>(old_caps & kNetworkCapCaptivePortal) << "->"
        << static_cast<bool>(new_caps & kNetworkCapCaptivePortal)
        << " isNetworkValidated:"
        << static_cast<bool>(old_caps & kNetworkCapValidated) << "->"
        << static_cast<bool>(new_caps & kNetworkCapValidated)
        << " netHandle:" << net_handle.netId << " bearType:"
        << last_bearer_types_str_ << "->" << bearer_types_str
        << " netCaps:" << net_caps_str;
    last_bearer_types_str_ = bearer_types_str;
  }
}

bool NetworkChangeNotifierPassive::IsCaptivePortalInternal() {
  return (network_capability_.load(std::memory_order_acquire) &
          kNetworkCapCaptivePortal) != 0;
}

bool NetworkChangeNotifierPassive::IsNetworkValidatedInternal() {
  return (network_capability_.load(std::memory_order_acquire) &
          kNetworkCapValidated) != 0;
}
#endif

#if BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
void NetworkChangeNotifierPassive::BindDnsToNetwork(int32_t network_for_dns) {
  if (*g_net_connect_callback) {
    (*g_net_connect_callback)->BindDnsToNetwork(network_for_dns);
  }
#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  network_for_dns_ = network_for_dns;
#endif
}

#if BUILDFLAG(IS_ARKWEB_EXT)
const std::vector<std::string> NetworkChangeNotifierPassive::GetNetAddrListByNetId(int32_t netId) {
  std::vector<std::string> net_addr_list = NetworkChangeNotifierPassiveUtils::GetNetAddrListByNetId(this, netId);
  if (net_addr_list.empty()) {
    LOG(INFO) << "Get net addrList by netId failed, net_addr_list is empty.";
  }
  return net_addr_list;
}

void NetworkChangeNotifierPassive::SetDefaultAddrList(std::vector<std::string> new_addr_list) {
  base::AutoLock scoped_lock(lock_);
  if (!new_addr_list.empty()) {
    default_address_list_ = std::move(new_addr_list);
    LOG(INFO) << "set new default addrlist size is " << default_address_list_.size();
  } else {
    default_address_list_.clear();
    LOG(INFO) << "clear default addrlist";
  }
}

const std::vector<std::string>
NetworkChangeNotifierPassive::GetDefaultAddrList() {
  base::AutoLock scoped_lock(lock_);
  return default_address_list_;
}

void NetworkChangeNotifierPassive::SetAddrListForBindedNetwork(std::vector<std::string> new_addr_list) {
  base::AutoLock scoped_lock(lock_);
  if (!new_addr_list.empty()) {
    addr_list_for_binded_network_ = std::move(new_addr_list);
    LOG(INFO) << "set new AddrListForBindedNetwork, size is " << addr_list_for_binded_network_.size();
  } else {
    addr_list_for_binded_network_.clear();
    LOG(INFO) << "clear AddrListForBindedNetwork";
  }
}

const std::vector<std::string>
NetworkChangeNotifierPassive::GetAddrListForBindedNetwork() {
  base::AutoLock scoped_lock(lock_);
  return addr_list_for_binded_network_;
}

const std::vector<std::string>
NetworkChangeNotifierPassive::GetActiveDnsServers() {
  std::vector<std::string> dns_servers;
  if (ohos_net_conn_adapter_) {
    if (vpn_in_place_.load(std::memory_order_relaxed)) {
      dns_servers = ohos_net_conn_adapter_->GetDnsServersForVpn();
    } else {
      dns_servers = ohos_net_conn_adapter_->GetDnsServersByNetId(network_for_dns_);
    }
  }
  return dns_servers;
}
#endif // IS_ARKWEB_EXT
#endif // ARKWEB_EXT_NETWORK_CONNECTION
#endif // BIND_DNS_TEST
}  // namespace net
