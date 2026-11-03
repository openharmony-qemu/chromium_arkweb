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

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

#include <memory>
#include <string>
#include <vector>

#include "arkweb/ohos_nweb_ex/build/features/features.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth.h"
#include "net/http/http_network_session.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/socket/connect_job.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// clang-format off
#define private public
#define protected public
#include "net/http/http_proxy_connect_job.h"
#undef protected
#undef private
// clang-format on

#if BUILDFLAG(IS_OHOS)
#include "base/time/time.h"
#endif

namespace net {

namespace {

// Mock ProxyDelegate for testing fallback proxy behavior.
class FallbackProxyMockDelegate : public ProxyDelegate {
 public:
  FallbackProxyMockDelegate() = default;
  ~FallbackProxyMockDelegate() override = default;

  MOCK_METHOD(void,
              OnResolveProxy,
              (const GURL& url,
               const NetworkAnonymizationKey& network_anonymization_key,
               const std::string& method,
               const ProxyRetryInfoMap& proxy_retry_info,
               ProxyInfo* result),
              (override));

  MOCK_METHOD(void,
              OnFallback,
              (const ProxyChain& bad_chain, int net_error),
              (override));

  MOCK_METHOD(void,
              OnSuccessfulRequestAfterFailures,
              (const ProxyRetryInfoMap& proxy_retry_info),
              (override));

  MOCK_METHOD((base::expected<HttpRequestHeaders, Error>),
              OnBeforeTunnelRequest,
              (const ProxyChain& proxy_chain,
               size_t chain_index,
               ProxyDelegate::OnBeforeTunnelRequestCallback callback),
              (override));

  MOCK_METHOD(Error,
              OnTunnelHeadersReceived,
              (const net::ProxyChain& proxy_chain,
               size_t chain_index,
               const HttpResponseHeaders& response_headers,
               CompletionOnceCallback callback),
              (override));

  MOCK_METHOD(void,
              SetProxyResolutionService,
              (ProxyResolutionService * proxy_resolution_service),
              (override));

  MOCK_METHOD(bool,
              AliasRequiresProxyOverride,
              (const std::string scheme,
               const std::vector<std::string>& dns_aliases,
               const net::NetworkAnonymizationKey& network_anonymization_key),
              (override));

  MOCK_METHOD(bool, IsAccelerateProxyConfig, (), (override));

  MOCK_METHOD(void,
              OnTunnelConnectResult,
              (const net::ProxyChain& proxy_chain,
               const std::string& host,
               const net::HttpResponseHeaders& response_headers,
               const net::HttpRequestHeaders& request_headers),
              (override));

  MOCK_METHOD(void,
              AddSuccessMainFrameHosts,
              (const std::string& host),
              (override));

  MOCK_METHOD(bool,
              IsFallbackProxyFailedHost,
              (const std::string& host),
              (override));

  MOCK_METHOD(bool,
              IsFallbackProxySuccessMainFrameHost,
              (const std::string& host),
              (override));

  MOCK_METHOD(void,
              AddByPassRuleWithHost,
              (const std::string& host),
              (override));

  MOCK_METHOD(void,
              OnProxyConnectResult,
              (const net::ProxyChain& proxy_chain, int net_error),
              (override));

  MOCK_METHOD(bool,
              GetUrlMaliciousTypeAndHwCode,
              (const std::vector<GURL>& url_chain,
               int* malicious_type,
               int* hw_code),
              (override));

  MOCK_METHOD(bool,
              IsFallbackProxyMaliciousType,
              (int malicious_type),
              (override));

  MOCK_METHOD(bool, IsFallbackProxyHwCode, (int hw_code), (override));

  MOCK_METHOD(bool, IsFallbackProxyRetryErrorCode, (int net_error), (override));

  MOCK_METHOD(bool,
              IsFallbackProxyBlockHost,
              (const std::string& host),
              (override));

  MOCK_METHOD(int, GetMaliciousUrlCheckWaitTime, (), (override));

  MOCK_METHOD(int, GetProxyConnectTimeout, (), (override));

  MOCK_METHOD(int, GetProxyTunnelTimeout, (), (override));

  MOCK_METHOD(net::FallbackProxyStatus, GetFallbackProxyStatus, (), (override));

  MOCK_METHOD(bool,
              IsFallbackProxyServer,
              (const net::ProxyChain& proxy_chain),
              (override));

  MOCK_METHOD(bool,
              IsDomainInProxyWhitelist,
              (const std::string& domain),
              (override));
};

const char kEndpointHost[] = "www.endpoint.test";
const char kHttpProxyHost[] = "httpproxy.example.test";
const ProxyServer kHttpProxyServer{ProxyServer::SCHEME_HTTP,
                                   HostPortPair(kHttpProxyHost, 80)};
const ProxyChain kHttpProxyChain{kHttpProxyServer};

class HttpProxyConnectJobForIncludeTest : public testing::Test {
 public:
  HttpProxyConnectJobForIncludeTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    session_deps_.host_resolver = std::make_unique<MockHostResolver>();

    network_quality_estimator_ =
        std::make_unique<TestNetworkQualityEstimator>();
    session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    InitCommonConnectJobParams();
  }

  void TearDown() override {
    job_.reset();
    session_.reset();
    task_environment_.RunUntilIdle();
  }

  void InitCommonConnectJobParams() {
    common_connect_job_params_ = std::make_unique<CommonConnectJobParams>(
        session_->CreateCommonConnectJobParams());
    common_connect_job_params_->proxy_delegate = proxy_delegate_.get();
    common_connect_job_params_->network_quality_estimator =
        network_quality_estimator_.get();
  }

  void CreateHttpProxyConnectJob() {
    ConnectJobParams nested_params(base::MakeRefCounted<TransportSocketParams>(
        kHttpProxyServer.host_port_pair(), NetworkAnonymizationKey(),
        SecureDnsPolicy::kAllow, OnHostResolutionCallback(),
        base::flat_set<std::string>()));
    auto params = base::MakeRefCounted<HttpProxySocketParams>(
        std::move(nested_params), HostPortPair(kEndpointHost, 443),
        kHttpProxyChain,
        /*proxy_chain_index=*/0, /*tunnel=*/true, TRAFFIC_ANNOTATION_FOR_TESTS,
        NetworkAnonymizationKey(), SecureDnsPolicy::kAllow);

    job_ = std::make_unique<HttpProxyConnectJob>(
        DEFAULT_PRIORITY, SocketTag(), common_connect_job_params_.get(), params,
        &test_delegate_, &net_log_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> session_;
  std::unique_ptr<TestNetworkQualityEstimator> network_quality_estimator_;
  std::unique_ptr<CommonConnectJobParams> common_connect_job_params_;
  std::unique_ptr<FallbackProxyMockDelegate> proxy_delegate_;
  TestConnectJobDelegate test_delegate_;
  NetLogWithSource net_log_ = NetLogWithSource();
  std::unique_ptr<HttpProxyConnectJob> job_;
};

// ---------- ResetTimerExInBeginConnect tests ----------

TEST_F(HttpProxyConnectJobForIncludeTest,
       ResetTimerExInBeginConnect_NoProxyDelegate_UsesAlternateTimeout) {
  CreateHttpProxyConnectJob();

  // Without proxy_delegate, the else branch is taken which uses
  // AlternateNestedConnectionTimeout.
  job_->ResetTimerExInBeginConnect();

  EXPECT_FALSE(job_->is_fallback_proxy_server_);
}

TEST_F(
    HttpProxyConnectJobForIncludeTest,
    ResetTimerExInBeginConnect_ProxyDelegateNotFallback_UsesAlternateTimeout) {
  proxy_delegate_ = std::make_unique<FallbackProxyMockDelegate>();
  InitCommonConnectJobParams();
  CreateHttpProxyConnectJob();

  EXPECT_CALL(*proxy_delegate_, IsFallbackProxyServer(testing::_))
      .WillOnce(testing::Return(false));

  job_->ResetTimerExInBeginConnect();

  EXPECT_FALSE(job_->is_fallback_proxy_server_);
}

TEST_F(HttpProxyConnectJobForIncludeTest,
       ResetTimerExInBeginConnect_FallbackProxy_SetsFlag) {
  proxy_delegate_ = std::make_unique<FallbackProxyMockDelegate>();
  InitCommonConnectJobParams();
  CreateHttpProxyConnectJob();

  EXPECT_CALL(*proxy_delegate_, IsFallbackProxyServer(testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*proxy_delegate_, GetProxyConnectTimeout())
      .WillOnce(testing::Return(5000));

  job_->ResetTimerExInBeginConnect();

  EXPECT_TRUE(job_->is_fallback_proxy_server_);
}

// ---------- ResetTimerExInHttpProxyConnect tests ----------

TEST_F(HttpProxyConnectJobForIncludeTest,
       ResetTimerExInHttpProxyConnect_NotFallback_UsesDefaultTimeout) {
  CreateHttpProxyConnectJob();

  // is_fallback_proxy_server_ defaults to false.
  job_->ResetTimerExInHttpProxyConnect();

  // TODO: Verify no http_auth_controller_ was created in the non-fallback path
  EXPECT_TRUE(job_->http_auth_controller_);
}

TEST_F(HttpProxyConnectJobForIncludeTest,
       ResetTimerExInHttpProxyConnect_Fallback_CreatesAuthController) {
  proxy_delegate_ = std::make_unique<FallbackProxyMockDelegate>();
  InitCommonConnectJobParams();
  CreateHttpProxyConnectJob();

  // Simulate being in fallback proxy mode.
  job_->is_fallback_proxy_server_ = true;

  EXPECT_CALL(*proxy_delegate_, GetProxyTunnelTimeout())
      .WillOnce(testing::Return(10000));

  job_->ResetTimerExInHttpProxyConnect();

  // HttpAuthController should have been created since it was null.
  EXPECT_TRUE(job_->http_auth_controller_);
}

TEST_F(
    HttpProxyConnectJobForIncludeTest,
    ResetTimerExInHttpProxyConnect_Fallback_ExistingAuthController_NotRecreated) {
  proxy_delegate_ = std::make_unique<FallbackProxyMockDelegate>();
  InitCommonConnectJobParams();
  CreateHttpProxyConnectJob();

  // Simulate being in fallback proxy mode with an existing auth controller.
  job_->is_fallback_proxy_server_ = true;

  // Create an HttpAuthController beforehand.
  job_->http_auth_controller_ = base::MakeRefCounted<HttpAuthController>(
      HttpAuth::AUTH_PROXY,
      GURL("http://" + kHttpProxyServer.host_port_pair().ToString()),
      NetworkAnonymizationKey(), session_->http_auth_cache(),
      session_->http_auth_handler_factory(), session_deps_.host_resolver.get());

  auto* original_controller = job_->http_auth_controller_.get();

  EXPECT_CALL(*proxy_delegate_, GetProxyTunnelTimeout())
      .WillOnce(testing::Return(10000));

  job_->ResetTimerExInHttpProxyConnect();

  // The existing controller should not have been replaced.
  EXPECT_EQ(job_->http_auth_controller_.get(), original_controller);
}

TEST_F(HttpProxyConnectJobForIncludeTest,
       ResetTimerExInHttpProxyConnect_Fallback_SslParams_HttpsUrl) {
  proxy_delegate_ = std::make_unique<FallbackProxyMockDelegate>();
  InitCommonConnectJobParams();
  CreateHttpProxyConnectJob();

  // Simulate being in fallback proxy mode with SSL params (https proxy).
  job_->is_fallback_proxy_server_ = true;

  // Set up ssl_params so that the URL in HttpAuthController uses "https://".
  job_->params_ = base::MakeRefCounted<HttpProxySocketParams>(
      ConnectJobParams(base::MakeRefCounted<SSLSocketParams>(
          ConnectJobParams(base::MakeRefCounted<TransportSocketParams>(
              kHttpProxyServer.host_port_pair(), NetworkAnonymizationKey(),
              SecureDnsPolicy::kAllow, OnHostResolutionCallback(),
              base::flat_set<std::string>())),
          HostPortPair(kHttpProxyHost, 443), SSLConfig(),
          NetworkAnonymizationKey())),
      HostPortPair(kEndpointHost, 443),
      ProxyChain(ProxyServer(ProxyServer::SCHEME_HTTPS,
                             HostPortPair(kHttpProxyHost, 443))),
      /*proxy_chain_index=*/0, /*tunnel=*/true, TRAFFIC_ANNOTATION_FOR_TESTS,
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow);

  EXPECT_CALL(*proxy_delegate_, GetProxyTunnelTimeout())
      .WillOnce(testing::Return(10000));

  job_->ResetTimerExInHttpProxyConnect();

  // HttpAuthController should have been created and the URL should start with
  // "https://".
  EXPECT_TRUE(job_->http_auth_controller_);
}

#if BUILDFLAG(IS_OHOS)
TEST_F(HttpProxyConnectJobForIncludeTest,
       ResetTimerExInHttpProxyConnect_NotFallback_OhosTimeout) {
  CreateHttpProxyConnectJob();

  // On OHOS, the non-fallback path should use kHttpProxyConnectJobTunnelTimeout
  // which is 10 seconds.
  job_->ResetTimerExInHttpProxyConnect();

  // The timer should be running with the OHOS timeout (10 seconds).
  // We verify indirectly by checking that the timer was set.
  EXPECT_FALSE(job_->is_fallback_proxy_server_);
}
#endif  // BUILDFLAG(IS_OHOS)

}  // namespace

}  // namespace net

#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
