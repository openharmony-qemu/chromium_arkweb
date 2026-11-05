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

#include <memory>

#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/client_socket_pool_manager_impl.h"
#include "net/socket/connect_job.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class ClientSocketPoolManagerImplExtTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a minimal HttpNetworkSession using SpdySessionDependencies
    session_deps_ = std::make_unique<SpdySessionDependencies>();
    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(session_deps_.get());
  }

  void TearDown() override {
    http_network_session_.reset();
    session_deps_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SpdySessionDependencies> session_deps_;
  std::unique_ptr<HttpNetworkSession> http_network_session_;
};

#if BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)
// Test SetConnectTimeout with empty socket_pools_ - should only set
// timeout_override_.
TEST_F(ClientSocketPoolManagerImplExtTest, SetConnectTimeout_EmptyPools) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);

  // SetConnectTimeout should set timeout_override_ without crashing.
  manager.SetConnectTimeout(30);

  // Verify the manager is functional (socket pools are created during
  // construction in this version of Chromium, so SocketPoolInfoToValue
  // returns non-empty data).
  EXPECT_FALSE(manager.SocketPoolInfoToValue().is_none());
}

// Test SetConnectTimeout with zero seconds.
TEST_F(ClientSocketPoolManagerImplExtTest, SetConnectTimeout_Zero) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);

  EXPECT_NO_FATAL_FAILURE(manager.SetConnectTimeout(0));
  // Should not crash with zero timeout
}

// Test SetConnectTimeout with positive value.
TEST_F(ClientSocketPoolManagerImplExtTest, SetConnectTimeout_PositiveValue) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);

  EXPECT_NO_FATAL_FAILURE(manager.SetConnectTimeout(60));
  // Should not crash
}

// Test SetConnectTimeout with a populated socket pool - exercises the loop
// body that propagates the timeout to each pool's utils.
TEST_F(ClientSocketPoolManagerImplExtTest, SetConnectTimeout_WithPools) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);
  EXPECT_NE(manager.GetSocketPool(ProxyChain::Direct()), nullptr);

  EXPECT_NO_FATAL_FAILURE(manager.SetConnectTimeout(60));
}
#endif  // BUILDFLAG(ARKWEB_EXT_NETWORK_CONNECTION)

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
// Test SetConnectJobWithSecureDnsOnlyTimeout with empty pools.
TEST_F(ClientSocketPoolManagerImplExtTest,
       SetConnectJobWithSecureDnsOnlyTimeout_EmptyPools) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);

  EXPECT_NO_FATAL_FAILURE(manager.SetConnectJobWithSecureDnsOnlyTimeout(5));
  // Should not crash, sets connect_job_with_secure_dns_timeout_
}

// Test SetConnectJobWithSecureDnsOnlyTimeout with zero seconds.
TEST_F(ClientSocketPoolManagerImplExtTest,
       SetConnectJobWithSecureDnsOnlyTimeout_Zero) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);

  EXPECT_NO_FATAL_FAILURE(manager.SetConnectJobWithSecureDnsOnlyTimeout(0));
  // Should not crash
}

// Test SetConnectJobWithSecureDnsOnlyTimeout with a populated socket pool -
// exercises the loop body propagating the timeout to each pool's utils.
TEST_F(ClientSocketPoolManagerImplExtTest,
       SetConnectJobWithSecureDnsOnlyTimeout_WithPools) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);
  EXPECT_NE(manager.GetSocketPool(ProxyChain::Direct()), nullptr);

  EXPECT_NO_FATAL_FAILURE(manager.SetConnectJobWithSecureDnsOnlyTimeout(5));
}
#endif  // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

#if BUILDFLAG(ARKWEB_NETWORK_SERVICE)
// Test SetSocketIdleTimeout with empty pools - should iterate but do nothing.
TEST_F(ClientSocketPoolManagerImplExtTest, SetSocketIdleTimeout_EmptyPools) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);

  EXPECT_NO_FATAL_FAILURE(manager.SetSocketIdleTimeout(300));
  // Should not crash with empty pools
}

// Test SetSocketIdleTimeout with zero value.
TEST_F(ClientSocketPoolManagerImplExtTest, SetSocketIdleTimeout_Zero) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);

  EXPECT_NO_FATAL_FAILURE(manager.SetSocketIdleTimeout(0));
  // Should not crash
}

// Test SetSocketIdleTimeout with a populated socket pool - exercises the loop
// body propagating the timeout to each pool.
TEST_F(ClientSocketPoolManagerImplExtTest, SetSocketIdleTimeout_WithPools) {
  auto common_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());
  auto websocket_params = std::make_unique<CommonConnectJobParams>(
      http_network_session_->CreateCommonConnectJobParams());

  ClientSocketPoolManagerImpl manager(*common_params, *websocket_params,
                                      HttpNetworkSession::NORMAL_SOCKET_POOL);
  EXPECT_NE(manager.GetSocketPool(ProxyChain::Direct()), nullptr);

  EXPECT_NO_FATAL_FAILURE(manager.SetSocketIdleTimeout(300));
}
#endif  // BUILDFLAG(ARKWEB_NETWORK_SERVICE)

}  // namespace net
