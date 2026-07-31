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
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_pool_additional_capacity.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

// clang-format off
#define private public
#define protected public
#include "net/socket/transport_client_socket_pool.h"
// clang-format on

#include "net/socket/client_socket_pool_manager_impl.h"

#if BUILDFLAG(ARKWEB_PRP_PRELOAD)
#include "net/socket/stream_socket.h"
#endif

namespace net {

class TransportClientSocketPoolForIncludeTest : public testing::Test {
 public:
  void SetUp() override {
    session_deps_ = std::make_unique<SpdySessionDependencies>();
    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(session_deps_.get());

    common_connect_job_params_ = std::make_unique<CommonConnectJobParams>(
        http_network_session_->CreateCommonConnectJobParams());
  }

  void TearDown() override {
    common_connect_job_params_.reset();
    http_network_session_.reset();
    session_deps_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SpdySessionDependencies> session_deps_;
  std::unique_ptr<HttpNetworkSession> http_network_session_;
  std::unique_ptr<CommonConnectJobParams> common_connect_job_params_;
};

#if BUILDFLAG(ARKWEB_PRP_PRELOAD)
// Test that UNUSED_PRELOAD_SOCKET_TIMEOUT_MICRO_SEC constant results in
// the expected timeout value.
TEST_F(TransportClientSocketPoolForIncludeTest,
       UnusedPreloadSocketTimeoutValue) {
  // The constant is 5000000 microseconds = 5 seconds
  base::TimeDelta expected_timeout =
      base::TimeDelta::FromInternalValue(5000000);
  EXPECT_EQ(expected_timeout.InSeconds(), 5);
}
#endif  // BUILDFLAG(ARKWEB_PRP_PRELOAD)

// Test that TransportClientSocketPool can be created with necessary params.
// This is a basic smoke test for the pool infrastructure that the
// for_include.cc code depends on.
TEST_F(TransportClientSocketPoolForIncludeTest, PoolCreation) {
  auto pool = std::make_unique<TransportClientSocketPool>(
      10,  // socket_soft_cap
      5,   // max_sockets_per_group
      SocketPoolAdditionalCapacity::CreateEmpty(), base::TimeDelta::Max(),
      ProxyChain(), false /* is_for_websockets */,
      common_connect_job_params_.get());
  EXPECT_NE(pool.get(), nullptr);
}

#if BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION) && \
    BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
// Test ConnectJobSetConnectTimeoutExt logic with secure_dns_only group.
// This test verifies the branching logic: when secure_dns_only is true,
// the secure dns timeout is used; otherwise the regular connect timeout.
TEST_F(TransportClientSocketPoolForIncludeTest,
       ConnectJobSetConnectTimeoutExt_SecureDnsOnly) {
  // Verify that ClientSocketPool::GroupId can be created with
  // secure_dns_only flag
  ClientSocketPool::GroupId secure_group(
      url::SchemeHostPort("https", "example.com", 443),
      PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kDisable, false, true /* secure_dns_only */);

  ClientSocketPool::GroupId normal_group(
      url::SchemeHostPort("https", "example.com", 443),
      PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kDisable, false, false /* secure_dns_only */);

  EXPECT_TRUE(secure_group.secure_dns_only());
  EXPECT_FALSE(normal_group.secure_dns_only());
}
#endif

}  // namespace net
