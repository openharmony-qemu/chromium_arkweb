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

#include "arkweb/build/features/features.h"  // nogncheck
#if BUILDFLAG(IS_ARKWEB_EXT)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#include "arkweb/ohos_nweb_ex/build/features/features.h"  // nogncheck
#pragma clang diagnostic pop
#endif

#include "base/test/task_environment.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/dns_query_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

// clang-format off
#define private public
#define protected public
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_manager_service_endpoint_request_impl.h"
// clang-format on
#include "arkweb/chromium_ext/net/dns/public/resolve_info.h"
#include "net/dns/public/host_resolver_results.h"

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
#include "arkweb/chromium_ext/net/dns/secure_dns_fallback_utils.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#include "arkweb/chromium_ext/net/dns/secure_dns_fallback_utils.h"
#include "base/command_line.h"
#endif

namespace net {

class ServiceEndpointRequestImplExtTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a ServiceEndpointRequestImpl with minimal setup.
    // The manager_ is null, which is fine for testing the ext functions
    // that only need resolve_info_ and host_ fields.
    request_ =
        std::make_unique<HostResolverManager::ServiceEndpointRequestImpl>(
            url::SchemeHostPort("https", "example.com", 443),
            NetworkAnonymizationKey(), NetLogWithSource(),
            HostResolver::ResolveHostParameters(),
            base::WeakPtr<ResolveContext>(),
            base::WeakPtr<HostResolverManager>(), nullptr /* tick_clock */);
  }

  void TearDown() override { request_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<HostResolverManager::ServiceEndpointRequestImpl> request_;
};

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

// Test MaybeModifyResolveLocallyResults with error result - should return
// early without modification.
TEST_F(ServiceEndpointRequestImplExtTest,
       MaybeModifyResolveLocallyResults_ErrorResult) {
  HostCache::Entry entry(ERR_NAME_NOT_RESOLVED,
                         HostCache::Entry::SOURCE_UNKNOWN);
  std::vector<IPEndPoint> truncation_results;

  request_->MaybeModifyResolveLocallyResults(entry, truncation_results);

  // No modification should happen for error results
  EXPECT_EQ(entry.error(), ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(truncation_results.empty());
}

// Test MaybeModifyResolveLocallyResults with empty ip_endpoints - should
// return early.
TEST_F(ServiceEndpointRequestImplExtTest,
       MaybeModifyResolveLocallyResults_EmptyEndpoints) {
  HostCache::Entry entry(OK, HostCache::Entry::SOURCE_UNKNOWN);
  std::vector<IPEndPoint> truncation_results;

  request_->MaybeModifyResolveLocallyResults(entry, truncation_results);

  // No modification should happen for empty endpoints
  EXPECT_EQ(entry.error(), OK);
  EXPECT_TRUE(entry.ip_endpoints().empty());
  EXPECT_TRUE(truncation_results.empty());
}

// Test MaybeModifyResolveLocallyResultsAndUpdateResolveInfo with error result
// - resolve_info should still get error code and dns_status.
TEST_F(ServiceEndpointRequestImplExtTest,
       MaybeModifyResolveLocallyResultsAndUpdateResolveInfo_ErrorResult) {
  HostCache::Entry entry(ERR_NAME_NOT_RESOLVED,
                         HostCache::Entry::SOURCE_UNKNOWN);
  int dns_status = kDnsResolvedBySystemDns;

  request_->MaybeModifyResolveLocallyResultsAndUpdateResolveInfo(entry,
                                                                 dns_status);

  // resolve_info_ should have error_code and dns_status set even for errors
  EXPECT_EQ(request_->resolve_info_.error_code, ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(request_->resolve_info_.dns_status, dns_status);
}

// Test MaybeModifyResolveLocallyResultsAndUpdateResolveInfo with OK result
// but empty endpoints - resolve_info should get status but no ip_endpoints.
TEST_F(ServiceEndpointRequestImplExtTest,
       MaybeModifyResolveLocallyResultsAndUpdateResolveInfo_EmptyEndpoints) {
  HostCache::Entry entry(OK, HostCache::Entry::SOURCE_UNKNOWN);
  int dns_status = kDnsResolvedByLocalDns;

  request_->MaybeModifyResolveLocallyResultsAndUpdateResolveInfo(entry,
                                                                 dns_status);

  EXPECT_EQ(request_->resolve_info_.error_code, OK);
  EXPECT_EQ(request_->resolve_info_.dns_status, dns_status);
  EXPECT_TRUE(request_->resolve_info_.ip_endpoints.empty());
  EXPECT_TRUE(request_->resolve_info_.truncation_ips.empty());
}

// Test MaybeModifyResolveLocallyResultsAndUpdateResolveInfo with valid
// endpoints that don't need modification (non-suspect IPs).
TEST_F(ServiceEndpointRequestImplExtTest,
       MaybeModifyResolveLocallyResultsAndUpdateResolveInfo_ValidEndpoints) {
  IPAddress ip1;
  ASSERT_TRUE(ip1.AssignFromIPLiteral("192.168.1.1"));
  IPEndPoint endpoint1(ip1, 443);
  HostCache::Entry entry(OK, std::vector<IPEndPoint>{endpoint1},
                         std::set<std::string>{}, HostCache::Entry::SOURCE_DNS);

  int dns_status = kDnsResolvedBySystemDns;
  request_->MaybeModifyResolveLocallyResultsAndUpdateResolveInfo(entry,
                                                                 dns_status);

  EXPECT_EQ(request_->resolve_info_.error_code, OK);
  EXPECT_EQ(request_->resolve_info_.dns_status, dns_status);
  // ip_endpoints should be populated from the entry
  EXPECT_EQ(request_->resolve_info_.ip_endpoints.size(), 1u);
  // truncation_ips should be empty since IPs are not suspect
  EXPECT_TRUE(request_->resolve_info_.truncation_ips.empty());
}

// Test MaybeModifyResolveLocallyResultsAndUpdateResolveInfo with
// multiple endpoints.
TEST_F(ServiceEndpointRequestImplExtTest,
       MaybeModifyResolveLocallyResultsAndUpdateResolveInfo_MultipleEndpoints) {
  IPAddress ip1;
  IPAddress ip2;
  ASSERT_TRUE(ip1.AssignFromIPLiteral("10.0.0.1"));
  ASSERT_TRUE(ip2.AssignFromIPLiteral("10.0.0.2"));
  IPEndPoint endpoint1(ip1, 443);
  IPEndPoint endpoint2(ip2, 80);
  HostCache::Entry entry(OK, std::vector<IPEndPoint>{endpoint1, endpoint2},
                         std::set<std::string>{}, HostCache::Entry::SOURCE_DNS);

  int dns_status = kDnsResolvedByHttpsDns;
  request_->MaybeModifyResolveLocallyResultsAndUpdateResolveInfo(entry,
                                                                 dns_status);

  EXPECT_EQ(request_->resolve_info_.error_code, OK);
  EXPECT_EQ(request_->resolve_info_.dns_status, dns_status);
  EXPECT_EQ(request_->resolve_info_.ip_endpoints.size(), 2u);
}

// Test MaybeModifyResolveLocallyResults with null manager_ - secure dns
// fallback should be false by default.
TEST_F(ServiceEndpointRequestImplExtTest,
       MaybeModifyResolveLocallyResults_NullManager) {
  IPAddress ip;
  ASSERT_TRUE(ip.AssignFromIPLiteral("8.8.8.8"));
  IPEndPoint endpoint(ip, 443);
  HostCache::Entry entry(OK, std::vector<IPEndPoint>{endpoint},
                         std::set<std::string>{}, HostCache::Entry::SOURCE_DNS);
  std::vector<IPEndPoint> truncation_results;

  // manager_ is null by default - CanUseSecureDnsFallback cannot be called
  request_->MaybeModifyResolveLocallyResults(entry, truncation_results);

  // With no manager, secure_dns_fallback_available is false.
  // MaybeNeedToProcessAddressList should return false since no suspect IPs
  // are configured for the host.
  EXPECT_EQ(entry.error(), OK);
}

#endif  // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)

// Test set_resolve_info with basic error code and dns_status.
TEST_F(ServiceEndpointRequestImplExtTest, SetResolveInfo_BasicInfo) {
  std::vector<HostResolverManager::TaskType> finished_tasks = {
      HostResolverManager::TaskType::DNS};
  std::vector<IPEndPoint> truncation_results;

  request_->set_resolve_info(ERR_NAME_NOT_RESOLVED, kDnsResolvedBySystemDns,
                             finished_tasks, truncation_results);

  EXPECT_EQ(request_->resolve_info_.error_code, ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(request_->resolve_info_.dns_status, kDnsResolvedBySystemDns);
  // Transition type should be added from DNS task
  EXPECT_NE(request_->resolve_info_.dns_transition, 0u);
}

// Test set_resolve_info with empty finished_tasks.
TEST_F(ServiceEndpointRequestImplExtTest, SetResolveInfo_EmptyTasks) {
  std::vector<HostResolverManager::TaskType> finished_tasks;
  std::vector<IPEndPoint> truncation_results;

  request_->set_resolve_info(OK, kDnsResolvedByLocalDns, finished_tasks,
                             truncation_results);

  EXPECT_EQ(request_->resolve_info_.error_code, OK);
  EXPECT_EQ(request_->resolve_info_.dns_status, kDnsResolvedByLocalDns);
  // No transition type should be added
  EXPECT_EQ(request_->resolve_info_.dns_transition, 0u);
}

// Test set_resolve_info with multiple finished tasks (DNS + MDNS).
TEST_F(ServiceEndpointRequestImplExtTest, SetResolveInfo_MultipleTasks) {
  std::vector<HostResolverManager::TaskType> finished_tasks = {
      HostResolverManager::TaskType::DNS, HostResolverManager::TaskType::MDNS};
  std::vector<IPEndPoint> truncation_results;

  request_->set_resolve_info(OK, kDnsResolvedByHttpsDns, finished_tasks,
                             truncation_results);

  EXPECT_EQ(request_->resolve_info_.error_code, OK);
  EXPECT_EQ(request_->resolve_info_.dns_status, kDnsResolvedByHttpsDns);
  // Two transition types should be added
  EXPECT_NE(request_->resolve_info_.dns_transition, 0u);
}

// Test set_resolve_info with truncation results.
TEST_F(ServiceEndpointRequestImplExtTest, SetResolveInfo_TruncationResults) {
  std::vector<HostResolverManager::TaskType> finished_tasks;
  IPAddress ip1;
  IPAddress ip2;
  ASSERT_TRUE(ip1.AssignFromIPLiteral("1.2.3.4"));
  ASSERT_TRUE(ip2.AssignFromIPLiteral("5.6.7.8"));
  std::vector<IPEndPoint> truncation_results = {IPEndPoint(ip1, 443),
                                                IPEndPoint(ip2, 443)};

  request_->set_resolve_info(OK, kDnsResolvedBySystemDns, finished_tasks,
                             truncation_results);

  EXPECT_EQ(request_->resolve_info_.truncation_ips.size(), 2u);
}

// Test set_resolve_info with empty truncation_results.
TEST_F(ServiceEndpointRequestImplExtTest, SetResolveInfo_NoTruncation) {
  std::vector<HostResolverManager::TaskType> finished_tasks;
  std::vector<IPEndPoint> truncation_results;

  request_->set_resolve_info(OK, kDnsResolvedUndefined, finished_tasks,
                             truncation_results);

  EXPECT_TRUE(request_->resolve_info_.truncation_ips.empty());
}

// Test set_resolve_info with finalized_result_ containing endpoints.
TEST_F(ServiceEndpointRequestImplExtTest, SetResolveInfo_WithFinalizedResult) {
  IPAddress ipv4;
  IPAddress ipv6;
  ASSERT_TRUE(ipv4.AssignFromIPLiteral("10.0.0.1"));
  ASSERT_TRUE(ipv6.AssignFromIPLiteral("::1"));

  // Create a finalized result with both IPv4 and IPv6 endpoints
  ServiceEndpoint se;
  se.ipv4_endpoints = {IPEndPoint(ipv4, 443)};
  se.ipv6_endpoints = {IPEndPoint(ipv6, 443)};
  std::set<std::string> dns_aliases;

  request_->finalized_result_.emplace(std::vector<ServiceEndpoint>{se},
                                      dns_aliases);

  std::vector<HostResolverManager::TaskType> finished_tasks = {
      HostResolverManager::TaskType::DNS};
  std::vector<IPEndPoint> truncation_results;

  request_->set_resolve_info(OK, kDnsResolvedBySystemDns, finished_tasks,
                             truncation_results);

  // Should have both IPv4 and IPv6 addresses
  EXPECT_EQ(request_->resolve_info_.ip_endpoints.size(), 2u);
}

// Test set_resolve_info with finalized_result_ that has no value.
TEST_F(ServiceEndpointRequestImplExtTest, SetResolveInfo_NoFinalizedResult) {
  ASSERT_FALSE(request_->finalized_result_.has_value());

  std::vector<HostResolverManager::TaskType> finished_tasks = {
      HostResolverManager::TaskType::DNS};
  std::vector<IPEndPoint> truncation_results;

  request_->set_resolve_info(ERR_FAILED, kDnsResolvedUndefined, finished_tasks,
                             truncation_results);

  EXPECT_EQ(request_->resolve_info_.error_code, ERR_FAILED);
  EXPECT_TRUE(request_->resolve_info_.ip_endpoints.empty());
}

// Test GetResolveInfo returns correct data after set_resolve_info.
TEST_F(ServiceEndpointRequestImplExtTest, GetResolveInfo_AfterSet) {
  std::vector<HostResolverManager::TaskType> finished_tasks = {
      HostResolverManager::TaskType::DNS};
  std::vector<IPEndPoint> truncation_results;

  request_->set_resolve_info(OK, kDnsResolvedByHttpsDns, finished_tasks,
                             truncation_results);

  ResolveInfo info = request_->GetResolveInfo();
  EXPECT_EQ(info.error_code, OK);
  EXPECT_EQ(info.dns_status, kDnsResolvedByHttpsDns);
  EXPECT_NE(info.dns_transition, 0u);
}

// Test SecureDnsFallbackResolved default value.
TEST_F(ServiceEndpointRequestImplExtTest,
       SecureDnsFallbackResolved_DefaultFalse) {
  EXPECT_FALSE(request_->SecureDnsFallbackResolved());
}

#endif  // BUILDFLAG(ARKWEB_EXT_NAVIGATION)

}  // namespace net
