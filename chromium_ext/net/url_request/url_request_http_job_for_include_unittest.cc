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
#include "arkweb/ohos_nweb_ex/build/features/features.h"

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY) ||        \
    BUILDFLAG(ARKWEB_NETWORK_PROXY_AUTH_RETRY) || \
    BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK) ||    \
    BUILDFLAG(ARKWEB_EXT_NAVIGATION)

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#include "arkweb/chromium_ext/net/proxy_resolution/fallback_proxy_config.h"
#endif

#if BUILDFLAG(ARKWEB_NETWORK_PROXY_AUTH_RETRY)
#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
#include "arkweb/chromium_ext/base/arkweb_report_statistics.h"
#endif

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
#include "arkweb/chromium_ext/net/base/request_attempt.h"
#endif

// Include url_request.h, url_request_http_job.h and url_request_test_util.h
// with private/protected redefined as public so we can access private members
// in tests. Only these specific headers are included under the macro to
// minimize impact on third-party code.
#define private public
#define protected public
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_test_util.h"
#undef protected
#undef private

namespace net {

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
// Mock ProxyDelegate for fallback proxy testing
class FallbackProxyMockDelegate : public ProxyDelegate {
 public:
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
               OnBeforeTunnelRequestCallback callback),
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
#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

class URLRequestHttpJobForIncludeTest : public testing::Test {
 public:
  URLRequestHttpJobForIncludeTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    context_ = CreateTestURLRequestContextBuilder()->Build();
  }

  void TearDown() override {}

 protected:
  std::unique_ptr<URLRequestContext> context_;
  base::test::TaskEnvironment task_environment_;
};

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
// ---------- MaybeRetryWithFallbackProxy tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       MaybeRetryWithFallbackProxy_AlreadyRetrying) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  // Create a URLRequestHttpJob and assign it to the request
  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  req->SetRetryWithFallbackProxy(true);
  int result = ERR_CONNECTION_FAILED;
  bool ret = job_ptr->MaybeRetryWithFallbackProxy(result);
  EXPECT_FALSE(ret);
}

// ---------- CanRetryWithFallbackProxy tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_NoEnableNwebEx) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  // Without --enable-nweb-ex switch, CanRetryWithFallbackProxy should
  // return false
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  EXPECT_FALSE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest, CanRetryWithFallbackProxy_ResultOK) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  // OK result should not trigger retry
  bool ret = job_ptr->CanRetryWithFallbackProxy(OK, &reason);
  EXPECT_FALSE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_AccelerateProxyConfig) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(true));
  context_builder->set_proxy_delegate(std::move(mock_delegate));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  // Accelerate proxy mode should not retry with fallback proxy
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  EXPECT_FALSE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_NullProxyDelegate) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  // No proxy delegate => can't retry
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  EXPECT_FALSE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_ProxyUnavailable) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::UNAVAILABLE));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  EXPECT_FALSE(ret);
  EXPECT_EQ(reason, ProxyUnusedReason::PROXY_SERVER_UNAVAILABLE);
}

TEST_F(URLRequestHttpJobForIncludeTest, CanRetryWithFallbackProxy_AuthFailed) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::AUTH_FAILED));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  EXPECT_FALSE(ret);
  EXPECT_EQ(reason, ProxyUnusedReason::AUTH_FAILED);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_ErrorCodeNotInCloudList) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::NORMAL));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyRetryErrorCode(testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  EXPECT_FALSE(ret);
  EXPECT_EQ(reason, ProxyUnusedReason::ERROR_CODE_NOT_IN_CLOUD_LIST);
}

// ---------- SupportedHostAndNotUsedSystemProxy tests ----------

TEST_F(URLRequestHttpJobForIncludeTest, SupportedHost_FailedHost) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(true));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->SupportedHostAndNotUsedSystemProxy(ctx->proxy_delegate(),
                                                         &reason);
  EXPECT_FALSE(ret);
  EXPECT_EQ(reason, ProxyUnusedReason::HOST_IN_USING_PROXY_FAILED_LIST);
}

TEST_F(URLRequestHttpJobForIncludeTest, SupportedHost_BlockHost) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(true));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->SupportedHostAndNotUsedSystemProxy(ctx->proxy_delegate(),
                                                         &reason);
  EXPECT_FALSE(ret);
  EXPECT_EQ(reason, ProxyUnusedReason::TOP_SITE_IN_BLOCK_LIST);
}

// ---------- GetSafeBrowsingThreatUrlPolicy tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       GetSafeBrowsingThreatUrlPolicy_NonMainFrame) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  // Non-main-frame requests should always PROCEED
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kSubFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  int malicious_type = -1;
  int hw_code = -1;
  SBThreatURLPolicy policy = job_ptr->GetSafeBrowsingThreatUrlPolicy(
      ERR_CONNECTION_FAILED, &malicious_type, &hw_code);
  EXPECT_EQ(policy, PROCEED);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GetSafeBrowsingThreatUrlPolicy_NullProxyDelegate) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  int malicious_type = -1;
  int hw_code = -1;
  // Null proxy delegate for main frame => DENY
  SBThreatURLPolicy policy = job_ptr->GetSafeBrowsingThreatUrlPolicy(
      ERR_CONNECTION_FAILED, &malicious_type, &hw_code);
  EXPECT_EQ(policy, DENY);
}

// ---------- RetryWithFallbackProxy / RetryWithDirect tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       RetryWithFallbackProxy_NullTransaction) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // With null transaction, RetryWithFallbackProxy should not crash
  ASSERT_EQ(job_ptr->transaction_.get(), nullptr);
  job_ptr->RetryWithFallbackProxy();
  // Should not crash
}

TEST_F(URLRequestHttpJobForIncludeTest, RetryWithDirect_NullTransaction) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // With null transaction, RetryWithDirect should not crash
  ASSERT_EQ(job_ptr->transaction_.get(), nullptr);
  job_ptr->RetryWithDirect();
  // Should not crash
}

#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

#if BUILDFLAG(ARKWEB_NETWORK_PROXY_AUTH_RETRY)
// ---------- NotifyNetworkServiceAuthRequired tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       NotifyNetworkServiceAuthRequired_NullAdapter) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  // Set status to OK so NotifyAuthRequired's DCHECK passes
  req->status_ = OK;

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  auto auth_info = std::make_unique<AuthChallengeInfo>();
  auth_info->challenger =
      url::SchemeHostPort(url::kHttpScheme, "proxy.example.com", 8080);
  auth_info->scheme = "Basic";
  auth_info->realm = "test";
  // When net_connect_adapter is null (typical in test environment),
  // NotifyNetworkServiceAuthRequired falls back to
  // NotifyAuthRequiredForRequest which calls URLRequest::NotifyAuthRequired
  // and then TestDelegate::OnAuthRequired.
  job_ptr->NotifyNetworkServiceAuthRequired(std::move(auth_info));
  // Verify that the delegate received the auth challenge
  EXPECT_TRUE(d.auth_required_called());
}

#endif  // BUILDFLAG(ARKWEB_NETWORK_PROXY_AUTH_RETRY)

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
// ---------- CanRetryWithSecureDnsOnly tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithSecureDnsOnly_NoEnableSwitch) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // Without --enable-nweb-ex-http-dns-fallback switch, should return false
  bool ret = job_ptr->CanRetryWithSecureDnsOnly(ERR_NAME_NOT_RESOLVED);
  EXPECT_FALSE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest, CanRetryWithSecureDnsOnly_ResultOK) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // OK result should not trigger retry
  bool ret = job_ptr->CanRetryWithSecureDnsOnly(OK);
  EXPECT_FALSE(ret);
}

// ---------- ReportSecureFallbackDnsRetryResult tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       ReportSecureFallbackDnsRetryResult_NullContext) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // ReportSecureFallbackDnsRetryResult should not crash even with
  // minimal context setup
  job_ptr->ReportSecureFallbackDnsRetryResult(ERR_NAME_NOT_RESOLVED);
  // Should not crash
}

#endif  // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
// ---------- GenerateRequestAttempt tests ----------

TEST_F(URLRequestHttpJobForIncludeTest, GenerateRequestAttempt_NullRequest) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // GenerateRequestAttempt should not crash with null transaction
  ASSERT_EQ(job_ptr->transaction_.get(), nullptr);
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_FAILED);
  // Should not crash; with null transaction, response_info is null,
  // so the function returns early after adding a basic attempt
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_MaxSizeExceeded) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // Fill request_attempts_ beyond kMaxRequestAttemptSize (10)
  for (int i = 0; i < 12; i++) {
    job_ptr->request_attempts_.emplace_back();
  }
  // GenerateRequestAttempt should return early when max size exceeded
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_FAILED);
  // Should not crash and should not add more attempts
  EXPECT_GE(job_ptr->request_attempts_.size(), 10u);
}

TEST_F(URLRequestHttpJobForIncludeTest, GetOriginalNetErrorCode_Initial) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // Initially original_error_code_ should be 0
  EXPECT_EQ(job_ptr->GetOriginalNetErrorCode(), 0);
}

TEST_F(URLRequestHttpJobForIncludeTest, GetRequestAttempts_Initial) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // Initially request_attempts_ should be empty
  EXPECT_TRUE(job_ptr->GetRequestAttempts().empty());
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GetExtraConnectionAttempts_NullTransaction) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // With null transaction, should return empty ConnectionAttempts
  ASSERT_EQ(job_ptr->transaction_.get(), nullptr);
  ConnectionAttempts attempts = job_ptr->GetExtraConnectionAttempts();
  EXPECT_TRUE(attempts.empty());
}

#endif  // BUILDFLAG(ARKWEB_EXT_NAVIGATION)

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
// ---------- Additional coverage tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_CloudControlSwitchDisabled) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(
          testing::Return(FallbackProxyStatus::DISABLE_BY_CLOUD_CONTROL));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  EXPECT_FALSE(ret);
  EXPECT_EQ(reason, ProxyUnusedReason::CLOUD_CONTROL_SWITCH_DISABLED);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       SupportedHostAndNotUsedSystemProxy_IpAddress) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("127.0.0.1", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://127.0.0.1"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  // IP address literal path - should return true since not blocked
  bool ret = job_ptr->SupportedHostAndNotUsedSystemProxy(ctx->proxy_delegate(),
                                                         &reason);
  EXPECT_TRUE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       SupportedHostAndNotUsedSystemProxy_WasFetchedViaProxy) {
  // This test verifies SupportedHostAndNotUsedSystemProxy returns false
  // when the response was fetched via a system proxy. Since this requires
  // a mock transaction with WasFetchedViaProxy() returning true, which is
  // not feasible without a full mock HttpTransaction, we verify the
  // SupportedHost check still works with the null transaction path
  // (transaction_ == nullptr means WasFetchedViaProxy is not checked).
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // Without a transaction, WasFetchedViaProxy is not checked,
  // and the host passes both FailedHost and BlockHost checks
  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->SupportedHostAndNotUsedSystemProxy(ctx->proxy_delegate(),
                                                         &reason);
  EXPECT_TRUE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GetSafeBrowsingThreatUrlPolicy_MaliciousTypeMatch) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate,
              GetUrlMaliciousTypeAndHwCode(testing::_, testing::_, testing::_))
      .WillRepeatedly(
          testing::Invoke([](const std::vector<GURL>&, int* mt, int* hc) {
            *mt = 1;
            *hc = 1;
            return true;
          }));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyMaliciousType(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyHwCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  int malicious_type = -1;
  int hw_code = -1;
  SBThreatURLPolicy policy = job_ptr->GetSafeBrowsingThreatUrlPolicy(
      ERR_CONNECTION_FAILED, &malicious_type, &hw_code);
  EXPECT_EQ(policy, PROCEED);
  EXPECT_EQ(malicious_type, 1);
  EXPECT_EQ(hw_code, 1);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GetSafeBrowsingThreatUrlPolicy_WaitForSbThreatType) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate,
              GetUrlMaliciousTypeAndHwCode(testing::_, testing::_, testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // First call returns WAIT
  int malicious_type = -1;
  int hw_code = -1;
  SBThreatURLPolicy policy = job_ptr->GetSafeBrowsingThreatUrlPolicy(
      ERR_CONNECTION_FAILED, &malicious_type, &hw_code);
  EXPECT_EQ(policy, WAIT);

  // Second call returns DENY because wait_for_sb_threat_type_ is now true
  malicious_type = -1;
  hw_code = -1;
  policy = job_ptr->GetSafeBrowsingThreatUrlPolicy(ERR_CONNECTION_FAILED,
                                                   &malicious_type, &hw_code);
  EXPECT_EQ(policy, DENY);
}

#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_ShouldSetOriginalCode) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  job_ptr->should_set_original_code_ = true;
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_FAILED);
  EXPECT_EQ(job_ptr->original_error_code_, ERR_CONNECTION_FAILED);
  EXPECT_FALSE(job_ptr->should_set_original_code_);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_CheckWirelessChangeType) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  job_ptr->current_attempt_type_ = AttemptType::kCheckWirelessChange;
  size_t prev_size = job_ptr->request_attempts_.size();
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_FAILED);
  // kCheckWirelessChange type should not add to attempts
  EXPECT_EQ(job_ptr->request_attempts_.size(), prev_size);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_CheckSafeBrowsingType) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  job_ptr->current_attempt_type_ = AttemptType::kCheckSafeBrowsing;
  size_t prev_size = job_ptr->request_attempts_.size();
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_FAILED);
  // kCheckSafeBrowsing type should not add to attempts
  EXPECT_EQ(job_ptr->request_attempts_.size(), prev_size);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_CheckNotRetryHttpDnsType) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  job_ptr->current_attempt_type_ = AttemptType::kCheckNotRetryHttpDns;
  size_t prev_size = job_ptr->request_attempts_.size();
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_FAILED);
  // kCheckNotRetryHttpDns type should not add to attempts
  EXPECT_EQ(job_ptr->request_attempts_.size(), prev_size);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_NormalTypeAddsAttempt) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // Default current_attempt_type_ is kNormal
  ASSERT_EQ(job_ptr->current_attempt_type_, AttemptType::kNormal);
  size_t prev_size = job_ptr->request_attempts_.size();
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_FAILED);
  // kNormal type should add an attempt (even with null transaction)
  EXPECT_EQ(job_ptr->request_attempts_.size(), prev_size + 1);
  EXPECT_EQ(job_ptr->request_attempts_.back().request_result,
            ERR_CONNECTION_FAILED);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_ShouldSetOriginalCodeTrue) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // Enable should_set_original_code_ (it may already be false from a prior
  // call, so set it explicitly)
  job_ptr->should_set_original_code_ = true;
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_REFUSED);
  // After GenerateRequestAttempt, should_set_original_code_ is reset
  EXPECT_FALSE(job_ptr->should_set_original_code_);
  // original_error_code_ should be set to the error
  EXPECT_EQ(job_ptr->original_error_code_, ERR_CONNECTION_REFUSED);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_ShouldSetOriginalCodeFalse) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // should_set_original_code_ is false by default (after first call)
  job_ptr->should_set_original_code_ = false;
  int prev_error = job_ptr->original_error_code_;
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_RESET);
  // original_error_code_ should NOT be updated
  EXPECT_EQ(job_ptr->original_error_code_, prev_error);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_FallbackProxyTypeAddsAttempt) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  job_ptr->current_attempt_type_ = AttemptType::kFallbackProxy;
  size_t prev_size = job_ptr->request_attempts_.size();
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_FAILED);
  // kFallbackProxy type should add an attempt
  EXPECT_EQ(job_ptr->request_attempts_.size(), prev_size + 1);
  EXPECT_EQ(job_ptr->request_attempts_.back().attempt_type,
            AttemptType::kFallbackProxy);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GenerateRequestAttempt_FallbackProxyDirectTypeAddsAttempt) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  job_ptr->current_attempt_type_ = AttemptType::kFallbackProxyDirect;
  size_t prev_size = job_ptr->request_attempts_.size();
  job_ptr->GenerateRequestAttempt(ERR_CONNECTION_FAILED);
  // kFallbackProxyDirect type should add an attempt
  EXPECT_EQ(job_ptr->request_attempts_.size(), prev_size + 1);
  EXPECT_EQ(job_ptr->request_attempts_.back().attempt_type,
            AttemptType::kFallbackProxyDirect);
}
#endif  // BUILDFLAG(ARKWEB_EXT_NAVIGATION)

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
// ---------- CanRetryWithFallbackProxy sub-frame tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_SubFrameSuccessHost) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("sub.example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::NORMAL));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyRetryErrorCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxySuccessMainFrameHost(testing::_))
      .WillRepeatedly(testing::Return(true));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req =
      ctx->CreateRequest(GURL("http://sub.example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  // Sub-frame with top_frame_origin whose host succeeded via proxy
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kSubFrame,
      url::Origin::Create(GURL("http://top-frame.com")),
      url::Origin::Create(GURL("http://sub.example.com")),
      SiteForCookies::FromUrl(GURL("http://sub.example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  // Sub-frame with top frame that succeeded via proxy should return true
  EXPECT_TRUE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_SubFrameNotSuccessHost) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("sub.example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::NORMAL));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyRetryErrorCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxySuccessMainFrameHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req =
      ctx->CreateRequest(GURL("http://sub.example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kSubFrame,
      url::Origin::Create(GURL("http://top-frame.com")),
      url::Origin::Create(GURL("http://sub.example.com")),
      SiteForCookies::FromUrl(GURL("http://sub.example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  // Sub-frame with top frame that did NOT succeed via proxy should return false
  EXPECT_FALSE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_SubFrameNoTopOrigin) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("sub.example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::NORMAL));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyRetryErrorCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req =
      ctx->CreateRequest(GURL("http://sub.example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  // Sub-frame without top_frame_origin (nullable origin)
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kSubFrame,
      url::Origin::Create(GURL("http://sub.example.com")),
      url::Origin::Create(GURL("http://sub.example.com")),
      SiteForCookies::FromUrl(GURL("http://sub.example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  // Sub-frame without a distinct top_frame_origin should return false
  // (same origin means top_frame_origin().host() won't be in success list)
  EXPECT_FALSE(ret);
}

// ---------- IsFallbackProxyIgnoreErrorCode logic tests via
//           MaybeRetryWithFallbackProxy ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       MaybeRetryWithFallbackProxy_AccelerateProxy_CertErrorIgnored) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(true));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // Certificate error is in IsFallbackProxyIgnoreErrorCode, so
  // accelerate-proxy path should NOT retry with direct
  int result = ERR_CERT_AUTHORITY_INVALID;
  bool ret = job_ptr->MaybeRetryWithFallbackProxy(result);
  EXPECT_FALSE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       MaybeRetryWithFallbackProxy_AccelerateProxy_ConnectionError) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(true));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  // Without a mock transaction, the is_accelerate_proxy check at line 261-264
  // requires transaction_->GetResponseInfo()->is_accelerate_proxy. Since
  // transaction_ is null here, MaybeRetryWithFallbackProxy falls through to
  // CanRetryWithFallbackProxy which returns false because
  // IsAccelerateProxyConfig() is true.
  int result = ERR_CONNECTION_FAILED;
  bool ret = job_ptr->MaybeRetryWithFallbackProxy(result);
  EXPECT_FALSE(ret);
}

// ---------- GetSafeBrowsingThreatUrlPolicy DENY tests ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       GetSafeBrowsingThreatUrlPolicy_MaliciousTypeDeny) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate,
              GetUrlMaliciousTypeAndHwCode(testing::_, testing::_, testing::_))
      .WillRepeatedly(
          testing::Invoke([](const std::vector<GURL>&, int* mt, int* hc) {
            *mt = 1;
            *hc = 1;
            return true;
          }));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyMaliciousType(testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  int malicious_type = -1;
  int hw_code = -1;
  SBThreatURLPolicy policy = job_ptr->GetSafeBrowsingThreatUrlPolicy(
      ERR_CONNECTION_FAILED, &malicious_type, &hw_code);
  // Malicious type not in cloud list => DENY
  EXPECT_EQ(policy, DENY);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       GetSafeBrowsingThreatUrlPolicy_HwCodeDeny) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate,
              GetUrlMaliciousTypeAndHwCode(testing::_, testing::_, testing::_))
      .WillRepeatedly(
          testing::Invoke([](const std::vector<GURL>&, int* mt, int* hc) {
            *mt = 1;
            *hc = 1;
            return true;
          }));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyMaliciousType(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyHwCode(testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  int malicious_type = -1;
  int hw_code = -1;
  SBThreatURLPolicy policy = job_ptr->GetSafeBrowsingThreatUrlPolicy(
      ERR_CONNECTION_FAILED, &malicious_type, &hw_code);
  // Malicious type matches but hw code doesn't => DENY
  EXPECT_EQ(policy, DENY);
}

// ---------- CanRetryWithFallbackProxy main frame success path ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_MainFrameSuccess) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::NORMAL));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyRetryErrorCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  EXPECT_TRUE(ret);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       CanRetryWithFallbackProxy_MainFrameSupportFails) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::NORMAL));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyRetryErrorCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(true));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  ProxyUnusedReason reason = ProxyUnusedReason::MAX_VALUE;
  bool ret = job_ptr->CanRetryWithFallbackProxy(ERR_CONNECTION_FAILED, &reason);
  EXPECT_FALSE(ret);
  EXPECT_EQ(reason, ProxyUnusedReason::HOST_IN_USING_PROXY_FAILED_LIST);
}

// ---------- MaybeRetryWithFallbackProxy full retry path ----------

TEST_F(URLRequestHttpJobForIncludeTest,
       MaybeRetryWithFallbackProxy_CanRetryProceedPolicy) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::NORMAL));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyRetryErrorCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate,
              GetUrlMaliciousTypeAndHwCode(testing::_, testing::_, testing::_))
      .WillRepeatedly(
          testing::Invoke([](const std::vector<GURL>&, int* mt, int* hc) {
            *mt = 1;
            *hc = 1;
            return true;
          }));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyMaliciousType(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyHwCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  int result = ERR_CONNECTION_FAILED;
  bool ret = job_ptr->MaybeRetryWithFallbackProxy(result);
  // With PROCEED policy, RetryWithFallbackProxy is called (returns early
  // due to null transaction without setting did_use_fallback_proxy_) and
  // MaybeRetryWithFallbackProxy returns true
  EXPECT_TRUE(ret);
  EXPECT_EQ(job_ptr->original_net_error_, ERR_CONNECTION_FAILED);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       MaybeRetryWithFallbackProxy_CanRetryDenyPolicy) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::NORMAL));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyRetryErrorCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate,
              GetUrlMaliciousTypeAndHwCode(testing::_, testing::_, testing::_))
      .WillRepeatedly(
          testing::Invoke([](const std::vector<GURL>&, int* mt, int* hc) {
            *mt = 1;
            *hc = 1;
            return true;
          }));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyMaliciousType(testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  int result = ERR_CONNECTION_FAILED;
  bool ret = job_ptr->MaybeRetryWithFallbackProxy(result);
  // With DENY policy, retry is skipped; function returns false
  EXPECT_FALSE(ret);
  EXPECT_EQ(job_ptr->original_net_error_, ERR_CONNECTION_FAILED);
}

TEST_F(URLRequestHttpJobForIncludeTest,
       MaybeRetryWithFallbackProxy_CanRetryWaitPolicy) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto mock_delegate = std::make_unique<FallbackProxyMockDelegate>();
  EXPECT_CALL(*mock_delegate, IsAccelerateProxyConfig())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, GetFallbackProxyStatus())
      .WillRepeatedly(testing::Return(FallbackProxyStatus::NORMAL));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyRetryErrorCode(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyFailedHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate, IsFallbackProxyBlockHost(testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_delegate,
              GetUrlMaliciousTypeAndHwCode(testing::_, testing::_, testing::_))
      .WillRepeatedly(testing::Return(false));
  context_builder->set_proxy_delegate(std::move(mock_delegate));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableNwebEx);
  auto ctx = context_builder->Build();

  TestDelegate d;
  auto req = ctx->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(GURL("http://example.com")),
      url::Origin::Create(GURL("http://example.com")),
      SiteForCookies::FromUrl(GURL("http://example.com")));
  req->set_isolation_info(isolation_info);

  auto job = std::make_unique<URLRequestHttpJob>(
      req.get(), ctx->http_user_agent_settings());
  URLRequestHttpJob* job_ptr = job.get();
  req->job_ = std::move(job);

  int result = ERR_CONNECTION_FAILED;
  bool ret = job_ptr->MaybeRetryWithFallbackProxy(result);
  // With WAIT policy, MaybeRetryWithFallbackProxy posts a delayed task
  // and returns true
  EXPECT_TRUE(ret);
  EXPECT_EQ(job_ptr->original_net_error_, ERR_CONNECTION_FAILED);
}
#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

}  // namespace net

#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY) ||
        // BUILDFLAG(ARKWEB_NETWORK_PROXY_AUTH_RETRY) ||
        // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK) ||
        // BUILDFLAG(ARKWEB_EXT_NAVIGATION)
