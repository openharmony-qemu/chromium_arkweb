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

#include <optional>
#include <string>

#include "arkweb/build/features/features.h"  // nogncheck

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

#include "arkweb/chromium_ext/net/base/fallback_proxy_constants.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
// clang-format off
#define private public
#include "net/http/http_response_headers.h"
#include "services/network/network_service_proxy_delegate.h"
#undef private
// clang-format on
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class TestProxyConnectionObserver
    : public mojom::CustomProxyConnectionObserver {
 public:
  TestProxyConnectionObserver() = default;
  ~TestProxyConnectionObserver() override = default;

  void OnFallback(const net::ProxyChain& bad_chain, int net_error) override {}
  void OnTunnelHeadersReceived(const net::ProxyChain& proxy_chain,
                               uint64_t chain_index,
                               const scoped_refptr<net::HttpResponseHeaders>&
                                   response_headers) override {}

  void OnTunnelHeadersReceivedWithToken(
      const net::ProxyChain& proxy_chain,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers,
      const std::string& token) override {}
  void OnProxyConnectResult(const net::ProxyChain& proxy_chain,
                            int error_code) override {}
};

}  // namespace

class NetworkServiceProxyDelegateExtTest : public testing::Test {
 public:
  NetworkServiceProxyDelegateExtTest() = default;

  void SetUp() override {
    context_ = net::CreateTestURLRequestContextBuilder()->Build();
  }

 protected:
  std::unique_ptr<NetworkServiceProxyDelegate> CreateDelegate(
      mojom::CustomProxyConfigPtr config) {
    auto observer = std::make_unique<TestProxyConnectionObserver>();

    mojo::PendingRemote<mojom::CustomProxyConnectionObserver> observer_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(observer), observer_remote.InitWithNewPipeAndPassReceiver());

    auto delegate = std::make_unique<NetworkServiceProxyDelegate>(
        mojom::CustomProxyConfig::New(), client_.BindNewPipeAndPassReceiver(),
        std::move(observer_remote));
    SetConfig(std::move(config));
    return delegate;
  }

  void SetConfig(mojom::CustomProxyConfigPtr config) {
    base::RunLoop loop;
    client_->OnCustomProxyConfigUpdated(std::move(config), loop.QuitClosure());
    loop.Run();
  }

  mojo::Remote<mojom::CustomProxyConfigClient> client_;
  std::unique_ptr<net::URLRequestContext> context_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NetworkServiceProxyDelegateExtTest, IsFallbackProxyServerTest) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  auto proxy_chain =
      net::ProxyChain(net::PacResultElementToProxyServer("HTTPS proxy"));
  bool result = delegate->IsFallbackProxyServer(proxy_chain);
  EXPECT_FALSE(result);

  delegate->fallback_proxy_status_ = net::FallbackProxyStatus::NORMAL;
  result = delegate->IsFallbackProxyServer(proxy_chain);
  EXPECT_TRUE(result);

  net::ProxyChain proxy_chain_null;
  result = delegate->IsFallbackProxyServer(proxy_chain_null);
  EXPECT_FALSE(result);
}

TEST_F(NetworkServiceProxyDelegateExtTest, AddSuccessMainFrameHostsTest) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  const std::string test_host = "www.example.com";
  delegate->AddSuccessMainFrameHosts(test_host);
  delegate->AddSuccessMainFrameHosts(test_host);
  EXPECT_EQ(delegate->main_frame_using_proxy_success_list_.size(), 1u);
  EXPECT_NE(delegate->main_frame_using_proxy_success_list_.find(test_host),
            delegate->main_frame_using_proxy_success_list_.end());
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       ProcessFailedResult_RemovesFromSuccessList) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));
  const std::string test_host = "www.example.com";

  delegate->AddSuccessMainFrameHosts(test_host);
  ASSERT_EQ(delegate->main_frame_using_proxy_success_list_.size(), 1u);

  delegate->ProcessFailedResult(test_host);

  EXPECT_EQ(delegate->main_frame_using_proxy_success_list_.size(), 0u);
  EXPECT_EQ(delegate->main_frame_using_proxy_success_list_.find(test_host),
            delegate->main_frame_using_proxy_success_list_.end());
}

TEST_F(NetworkServiceProxyDelegateExtTest, ProcessFailedResult_IncrementCount) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));
  const std::string test_host = "www.example.com";

  ASSERT_TRUE(delegate->using_proxy_failed_hosts_map_.empty());
  delegate->ProcessFailedResult(test_host);

  EXPECT_EQ(delegate->using_proxy_failed_hosts_map_.size(), 1u);
  EXPECT_EQ(delegate->using_proxy_failed_hosts_map_[test_host], 1);

  delegate->ProcessFailedResult(test_host);
  EXPECT_EQ(delegate->using_proxy_failed_hosts_map_[test_host], 2);
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       ProcessSuccessResult_ClearsFailedHosts) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  const std::string host1 = "www.example.com";
  const std::string host2 = "www.test.com";
  const std::string host3 = "www.another.com";
  delegate->using_proxy_failed_hosts_map_[host1] = 2;
  delegate->using_proxy_failed_hosts_map_[host3] = 1;
  delegate->ProcessSuccessResult(host1);
  delegate->ProcessSuccessResult(host2);
  delegate->ProcessSuccessResult(host3);

  EXPECT_EQ(delegate->using_proxy_failed_hosts_map_.size(), 0u);
}

TEST_F(NetworkServiceProxyDelegateExtTest, IsFallbackProxyFailedHostTest) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  const std::string host1 = "www.example.com";
  const std::string host2 = "www.test.com";
  const std::string host3 = "www.another.com";

  delegate->using_proxy_failed_hosts_map_[host1] = 1;
  delegate->using_proxy_failed_hosts_map_[host2] = 3;
  delegate->using_proxy_failed_hosts_map_[host3] = 5;

  EXPECT_FALSE(delegate->IsFallbackProxyFailedHost(host1));
  EXPECT_TRUE(delegate->IsFallbackProxyFailedHost(host2));
  EXPECT_TRUE(delegate->IsFallbackProxyFailedHost(host3));
  EXPECT_FALSE(delegate->IsFallbackProxyFailedHost("www.new.com"));
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       IsFallbackProxySuccessMainFrameHostTest) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  const std::string host1 = "www.example.com";
  const std::string host2 = "www.test.com";
  const std::string host3 = "www.another.com";

  delegate->AddSuccessMainFrameHosts(host1);
  delegate->AddSuccessMainFrameHosts(host3);

  EXPECT_TRUE(delegate->IsFallbackProxySuccessMainFrameHost(host1));
  EXPECT_FALSE(delegate->IsFallbackProxySuccessMainFrameHost(host2));
  EXPECT_TRUE(delegate->IsFallbackProxySuccessMainFrameHost(host3));
  EXPECT_FALSE(delegate->IsFallbackProxySuccessMainFrameHost("www.new.com"));
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       GetUrlMaliciousTypeAndHwCode_EmptyChain) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  std::vector<GURL> empty_url_chain;
  int malicious_type = 0;
  int hw_code = 0;
  auto proxy_chain =
      net::ProxyChain(net::PacResultElementToProxyServer("HTTPS proxy"));
  delegate->OnProxyConnectResult(proxy_chain, 0);
  EXPECT_FALSE(delegate->GetUrlMaliciousTypeAndHwCode(
      empty_url_chain, &malicious_type, &hw_code));
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       GetUrlMaliciousTypeAndHwCode_NotFound) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  std::vector<GURL> url_chain = {GURL("http://www.example.com")};
  int malicious_type = 0;
  int hw_code = 0;

  ASSERT_TRUE(delegate->url_sb_info_map_.empty());
  EXPECT_FALSE(delegate->GetUrlMaliciousTypeAndHwCode(
      url_chain, &malicious_type, &hw_code));
}

TEST_F(NetworkServiceProxyDelegateExtTest, GetUrlMaliciousTypeAndHwCode_Found) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  const std::string url1 = "http://www.example.com";
  const std::string url2 = "http://www.test.com";
  std::vector<GURL> url_chain = {GURL(url1), GURL(url2)};
  int malicious_type = 0;
  int hw_code = 0;
  int test_type = 3;
  int test_code = 300;
  GURL gurl(url2);
  delegate->url_sb_info_map_.Put(
      gurl.spec(),
      NetworkServiceProxyDelegate::PageSafeBrowsingInfo(test_type, test_code));

  EXPECT_TRUE(delegate->GetUrlMaliciousTypeAndHwCode(url_chain, &malicious_type,
                                                     &hw_code));
  EXPECT_EQ(test_type, malicious_type);
  EXPECT_EQ(test_code, hw_code);
}

TEST_F(NetworkServiceProxyDelegateExtTest, IsFallbackProxyMaliciousTypeTest) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  int test_type = 100;
  ASSERT_TRUE(delegate->safe_browsing_malicious_type_list_.empty());
  EXPECT_FALSE(delegate->IsFallbackProxyMaliciousType(test_type));

  delegate->safe_browsing_malicious_type_list_.insert(test_type);
  EXPECT_TRUE(delegate->IsFallbackProxyMaliciousType(test_type));
}

TEST_F(NetworkServiceProxyDelegateExtTest, IsFallbackProxyHwCodeTest) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));
  int test_code = 100;
  ASSERT_TRUE(delegate->safe_browsing_hw_code_list_.empty());
  EXPECT_FALSE(delegate->IsFallbackProxyHwCode(test_code));

  delegate->safe_browsing_hw_code_list_.insert(test_code);
  EXPECT_TRUE(delegate->IsFallbackProxyHwCode(test_code));
}

TEST_F(NetworkServiceProxyDelegateExtTest, OnTunnelConnectResult_NullChain) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  std::string host = "www.example.com";
  auto proxy_chain =
      net::ProxyChain(net::PacResultElementToProxyServer("HTTPS proxy"));
  net::ProxyChain proxy_chain_null;
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 200 OK\nHello: World\n\n");
  net::HttpRequestHeaders request_headers;
  EXPECT_NO_FATAL_FAILURE(delegate->OnTunnelConnectResult(
      proxy_chain_null, host, *response_headers, request_headers));
  EXPECT_NO_FATAL_FAILURE(delegate->OnTunnelConnectResult(
      proxy_chain, host, *response_headers, request_headers));
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       OnTunnelConnectResult_FailedAndSuccessCodes) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  std::string host = "www.example.com";
  auto proxy_chain =
      net::ProxyChain(net::PacResultElementToProxyServer("HTTPS proxy"));

  // Construct headers with specific response codes that trigger
  // ProcessFailedResult
  int times = 1;
  scoped_refptr<net::HttpResponseHeaders> headers_timeout =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 " +
          std::to_string(net::FallbackProxyResponseCode::DEST_SERVER_TIME_OUT) +
          " Timeout\n\n");
  delegate->OnTunnelConnectResult(proxy_chain, host, *headers_timeout,
                                  net::HttpRequestHeaders());
  EXPECT_EQ(delegate->using_proxy_failed_hosts_map_[host], times);

  scoped_refptr<net::HttpResponseHeaders> headers_disconnect =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 " +
          std::to_string(
              net::FallbackProxyResponseCode::DEST_SERVER_DISCONNECT) +
          " Disconnect\n\n");
  delegate->OnTunnelConnectResult(proxy_chain, host, *headers_disconnect,
                                  net::HttpRequestHeaders());
  EXPECT_EQ(delegate->using_proxy_failed_hosts_map_[host], ++times);

  scoped_refptr<net::HttpResponseHeaders> headers_resolved =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 " +
          std::to_string(
              net::FallbackProxyResponseCode::DEST_HOST_RESOLVED_FAILED) +
          " ResolvedFailed\n\n");
  delegate->OnTunnelConnectResult(proxy_chain, host, *headers_resolved,
                                  net::HttpRequestHeaders());
  EXPECT_EQ(delegate->using_proxy_failed_hosts_map_[host], ++times);

  scoped_refptr<net::HttpResponseHeaders> headers_port_deny =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 " +
          std::to_string(
              net::FallbackProxyResponseCode::DEST_PORT_DENY_BY_PROXY) +
          " PortDeny\n\n");
  delegate->OnTunnelConnectResult(proxy_chain, host, *headers_port_deny,
                                  net::HttpRequestHeaders());
  EXPECT_EQ(delegate->using_proxy_failed_hosts_map_[host], ++times);

  // HTTP_OK triggers ProcessSuccessResult which clears failed hosts map
  scoped_refptr<net::HttpResponseHeaders> headers_ok =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n\n");
  delegate->OnTunnelConnectResult(proxy_chain, host, *headers_ok,
                                  net::HttpRequestHeaders());
  EXPECT_EQ(delegate->using_proxy_failed_hosts_map_.size(), 0u);
}

TEST_F(NetworkServiceProxyDelegateExtTest, SaveAndCheckRetryErrorCode) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  auto delegate = CreateDelegate(std::move(config));

  EXPECT_FALSE(
      delegate->IsFallbackProxyRetryErrorCode(net::ERR_CONNECTION_RESET));
  delegate->SaveUsingFallbackProxyErrorCodes(
      {net::ERR_CONNECTION_RESET, net::ERR_TIMED_OUT});
  EXPECT_TRUE(
      delegate->IsFallbackProxyRetryErrorCode(net::ERR_CONNECTION_RESET));
  EXPECT_TRUE(delegate->IsFallbackProxyRetryErrorCode(net::ERR_TIMED_OUT));
  EXPECT_FALSE(delegate->IsFallbackProxyRetryErrorCode(net::ERR_FAILED));
}

TEST_F(NetworkServiceProxyDelegateExtTest, SaveAndCheckBlockHost) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  auto delegate = CreateDelegate(std::move(config));

  EXPECT_FALSE(delegate->IsFallbackProxyBlockHost("blocked.com"));
  delegate->OnHostBlockListUpdated({"blocked.com", "evil.com"});
  EXPECT_TRUE(delegate->IsFallbackProxyBlockHost("blocked.com"));
  EXPECT_TRUE(delegate->IsFallbackProxyBlockHost("evil.com"));
  EXPECT_FALSE(delegate->IsFallbackProxyBlockHost("safe.com"));
}

TEST_F(NetworkServiceProxyDelegateExtTest, UpdateFallbackProxyStatus) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  auto delegate = CreateDelegate(std::move(config));

  delegate->UpdateFallbackProxyStatus(
      static_cast<int>(net::FallbackProxyStatus::NORMAL));
  EXPECT_EQ(delegate->GetFallbackProxyStatus(),
            net::FallbackProxyStatus::NORMAL);

  delegate->UpdateFallbackProxyStatus(
      static_cast<int>(net::FallbackProxyStatus::DISABLE_BY_CLOUD_CONTROL));
  EXPECT_EQ(delegate->GetFallbackProxyStatus(),
            net::FallbackProxyStatus::DISABLE_BY_CLOUD_CONTROL);
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       IsDomainInProxyWhitelist_EmptyList_ReturnsFalse) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  auto delegate = CreateDelegate(std::move(config));

  EXPECT_FALSE(delegate->IsDomainInProxyWhitelist("example.com"));
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       IsDomainInProxyWhitelist_ExactMatch) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  auto delegate = CreateDelegate(std::move(config));
  delegate->proxy_whitelist_domains_ = {"example.com"};

  EXPECT_TRUE(delegate->IsDomainInProxyWhitelist("example.com"));
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       IsDomainInProxyWhitelist_SubdomainMatch) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  auto delegate = CreateDelegate(std::move(config));
  delegate->proxy_whitelist_domains_ = {"example.com"};

  // sub.example.com ends with ".example.com" and the preceding char is '.'
  EXPECT_TRUE(delegate->IsDomainInProxyWhitelist("sub.example.com"));
}

TEST_F(NetworkServiceProxyDelegateExtTest, IsDomainInProxyWhitelist_NoMatch) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  auto delegate = CreateDelegate(std::move(config));
  delegate->proxy_whitelist_domains_ = {"example.com"};

  // Different domain, same suffix but no leading '.' boundary.
  EXPECT_FALSE(delegate->IsDomainInProxyWhitelist("notexample.com"));
  EXPECT_FALSE(delegate->IsDomainInProxyWhitelist("evil.org"));
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       IsDomainInProxyWhitelist_ShorterDomain) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  auto delegate = CreateDelegate(std::move(config));
  delegate->proxy_whitelist_domains_ = {"example.com"};

  // domain shorter than the whitelist entry - the length check fails.
  EXPECT_FALSE(delegate->IsDomainInProxyWhitelist("com"));
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       OnTunnelConnectResult_TokenAuthFailed_WithToken) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  std::string host = "www.example.com";
  auto proxy_chain =
      net::ProxyChain(net::PacResultElementToProxyServer("HTTPS proxy"));
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 " +
          std::to_string(net::FallbackProxyResponseCode::TOKEN_AUTH_FAILED) +
          " TokenAuthFailed\n\n");
  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(net::kFallbackProxyTunnelHeaderKey, "test-token");

  EXPECT_NO_FATAL_FAILURE(delegate->OnTunnelConnectResult(
      proxy_chain, host, *response_headers, request_headers));
}

TEST_F(NetworkServiceProxyDelegateExtTest,
       OnTunnelConnectResult_TokenAuthFailed_NoToken) {
  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString("https://proxy");
  config->connect_tunnel_headers.SetHeader("connect", "baz");
  auto delegate = CreateDelegate(std::move(config));

  std::string host = "www.example.com";
  auto proxy_chain =
      net::ProxyChain(net::PacResultElementToProxyServer("HTTPS proxy"));
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 " +
          std::to_string(net::FallbackProxyResponseCode::TOKEN_AUTH_FAILED) +
          " TokenAuthFailed\n\n");
  net::HttpRequestHeaders request_headers;

  EXPECT_NO_FATAL_FAILURE(delegate->OnTunnelConnectResult(
      proxy_chain, host, *response_headers, request_headers));
}

}  // namespace network

#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
