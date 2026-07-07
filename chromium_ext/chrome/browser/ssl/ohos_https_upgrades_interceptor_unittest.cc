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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

#include "arkweb/build/features/features.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#define private public
#define protected public
#include "arkweb/chromium_ext/chrome/browser/ssl/ohos_https_upgrades_interceptor.h"
#undef private
#undef protected

#include "arkweb/chromium_ext/chrome/browser/ssl/ohos_https_upgrades_interceptor.cc"

class OhosHttpsUpgradesInterceptorTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  static network::ResourceRequest MakeResourceRequest(const std::string& url) {
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL(url);
    return request;
  }

  static content::FrameTreeNodeId MakeFrameTreeNodeId(int32_t value) {
    return content::FrameTreeNodeId::FromUnsafeValue(value);
  }

 protected:
  void SetUp() override { mojo::core::Init(); }
};

TEST_F(OhosHttpsUpgradesInterceptorTest, UpgradeUrlToHttps_DefaultPort) {
  GURL http_url("http://example.com/path?query=1");
  GURL https_url = UpgradeUrlToHttps(http_url);
  EXPECT_TRUE(https_url.SchemeIs(url::kHttpsScheme));
  EXPECT_EQ(https_url.host(), "example.com");
  EXPECT_FALSE(https_url.has_port());
  EXPECT_EQ(https_url.path(), "/path");
  EXPECT_EQ(https_url.query(), "query=1");
}

TEST_F(OhosHttpsUpgradesInterceptorTest, UpgradeUrlToHttps_CustomPort) {
  GURL http_url("http://example.com:8080/path");
  GURL https_url = UpgradeUrlToHttps(http_url);
  EXPECT_TRUE(https_url.SchemeIs(url::kHttpsScheme));
  EXPECT_EQ(https_url.host(), "example.com");
  EXPECT_TRUE(https_url.has_port());
  EXPECT_EQ(https_url.IntPort(), 8080);
  EXPECT_EQ(https_url.path(), "/path");
}

TEST_F(OhosHttpsUpgradesInterceptorTest, UpgradeUrlToHttps_WithFragment) {
  GURL http_url("http://example.com/page#section");
  GURL https_url = UpgradeUrlToHttps(http_url);
  EXPECT_TRUE(https_url.SchemeIs(url::kHttpsScheme));
  EXPECT_EQ(https_url.host(), "example.com");
  EXPECT_EQ(https_url.path(), "/page");
  EXPECT_EQ(https_url.ref(), "section");
}

TEST_F(OhosHttpsUpgradesInterceptorTest, UpgradeUrlToHttps_WithUserInfo) {
  GURL http_url("http://user@example.com/");
  GURL https_url = UpgradeUrlToHttps(http_url);
  EXPECT_TRUE(https_url.SchemeIs(url::kHttpsScheme));
  EXPECT_EQ(https_url.host(), "example.com");
  EXPECT_EQ(https_url.username(), "user");
}

TEST_F(OhosHttpsUpgradesInterceptorTest, SetupRedirect_ConfiguresResponseHead) {
  network::ResourceRequest request = MakeResourceRequest("http://example.com");
  GURL new_url("https://example.com");
  auto response_head = network::mojom::URLResponseHead::New();

  net::RedirectInfo redirect_info =
      SetupRedirect(request, new_url, response_head.get());

  EXPECT_EQ(response_head->encoded_data_length, 0u);
  EXPECT_NE(response_head->headers, nullptr);
  EXPECT_EQ(response_head->headers->response_code(),
            net::HTTP_TEMPORARY_REDIRECT);
  EXPECT_EQ(redirect_info.new_url, new_url);
  EXPECT_EQ(redirect_info.status_code, net::HTTP_TEMPORARY_REDIRECT);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       SetupRedirect_ResponseHeadersContainLocation) {
  network::ResourceRequest request = MakeResourceRequest("http://example.com");
  GURL new_url("https://secure.example.com/redirected");
  auto response_head = network::mojom::URLResponseHead::New();

  SetupRedirect(request, new_url, response_head.get());

  ASSERT_NE(response_head->headers, nullptr);
  std::string location;
  EXPECT_TRUE(
      response_head->headers->EnumerateHeader(nullptr, "Location", &location));
  EXPECT_EQ(location, new_url.spec());
}

TEST_F(OhosHttpsUpgradesInterceptorTest, SetupRedirect_PreservesRequestMethod) {
  network::ResourceRequest request = MakeResourceRequest("http://example.com");
  GURL new_url("https://example.com");
  auto response_head = network::mojom::URLResponseHead::New();

  net::RedirectInfo redirect_info =
      SetupRedirect(request, new_url, response_head.get());

  EXPECT_EQ(redirect_info.new_method, request.method);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       ShouldExcludeNavigationFromUpgrades_NullFrameTreeNode) {
  content::FrameTreeNodeId frame_id = MakeFrameTreeNodeId(1);
  EXPECT_TRUE(ShouldExcludeNavigationFromUpgrades(frame_id));
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       ShouldExcludeNavigationFromUpgrades_DefaultId) {
  content::FrameTreeNodeId frame_id;
  EXPECT_TRUE(ShouldExcludeNavigationFromUpgrades(frame_id));
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       ShouldProceedWithHttpsUpgrades_NullFrameTreeNode) {
  content::FrameTreeNodeId frame_id = MakeFrameTreeNodeId(1);
  EXPECT_FALSE(ShouldProceedWithHttpsUpgrades(frame_id));
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       ShouldProceedWithHttpsUpgrades_DefaultId) {
  content::FrameTreeNodeId frame_id;
  EXPECT_FALSE(ShouldProceedWithHttpsUpgrades(frame_id));
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateInterceptor_NullWebContents) {
  auto interceptor = OhosHttpsUpgradesInterceptor::MaybeCreateInterceptor(
      MakeFrameTreeNodeId(1));
  EXPECT_EQ(interceptor, nullptr);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateInterceptor_InvalidFrameTreeNodeId) {
  content::FrameTreeNodeId invalid_id;
  auto interceptor =
      OhosHttpsUpgradesInterceptor::MaybeCreateInterceptor(invalid_id);
  EXPECT_EQ(interceptor, nullptr);
}

TEST_F(OhosHttpsUpgradesInterceptorTest, Constructor_StoresFrameTreeNodeId) {
  auto id = MakeFrameTreeNodeId(42);
  auto interceptor = std::make_unique<OhosHttpsUpgradesInterceptor>(id);
  EXPECT_EQ(interceptor->frame_tree_node_id_.GetUnsafeValue(), 42);
}

TEST_F(OhosHttpsUpgradesInterceptorTest, Constructor_DefaultFrameTreeNodeId) {
  auto interceptor = std::make_unique<OhosHttpsUpgradesInterceptor>(
      content::FrameTreeNodeId());
  EXPECT_TRUE(interceptor->frame_tree_node_id_.is_null());
}

TEST_F(OhosHttpsUpgradesInterceptorTest, Destructor_NoCrash) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  EXPECT_NO_FATAL_FAILURE(interceptor.reset());
}

TEST_F(OhosHttpsUpgradesInterceptorTest, MaybeCreateLoader_NullBrowserContext) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  network::ResourceRequest request = MakeResourceRequest("http://example.com");

  bool callback_called = false;
  interceptor->MaybeCreateLoader(
      request, nullptr,
      base::BindOnce([](bool* called, RequestHandler) { *called = true; },
                     base::Unretained(&callback_called)));

  EXPECT_TRUE(callback_called);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoader_NullBrowserContextWithHttpsUrl) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  network::ResourceRequest request = MakeResourceRequest("https://example.com");

  bool callback_called = false;
  interceptor->MaybeCreateLoader(
      request, nullptr,
      base::BindOnce([](bool* called, RequestHandler) { *called = true; },
                     base::Unretained(&callback_called)));

  EXPECT_TRUE(callback_called);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoaderOnHstsQueryCompleted_NullWebContents) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  network::ResourceRequest request = MakeResourceRequest("http://example.com");

  bool callback_called = false;
  interceptor->MaybeCreateLoaderOnHstsQueryCompleted(
      request,
      base::BindOnce([](bool* called, RequestHandler) { *called = true; },
                     base::Unretained(&callback_called)),
      false);

  EXPECT_TRUE(callback_called);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoaderOnHstsQueryCompleted_HstsActive_NullWebContents) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  network::ResourceRequest request = MakeResourceRequest("http://example.com");

  bool callback_called = false;
  interceptor->MaybeCreateLoaderOnHstsQueryCompleted(
      request,
      base::BindOnce([](bool* called, RequestHandler) { *called = true; },
                     base::Unretained(&callback_called)),
      true);

  EXPECT_TRUE(callback_called);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoaderForResponse_NullWebContents) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_FAILED;
  network::ResourceRequest request = MakeResourceRequest("http://example.com");
  auto response_head = network::mojom::URLResponseHead::New();
  mojo::ScopedDataPipeConsumerHandle response_body;
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver;

  EXPECT_FALSE(interceptor->MaybeCreateLoaderForResponse(
      status, request, &response_head, &response_body, &loader,
      &client_receiver, nullptr));
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoaderForResponse_NetOkAndNullWebContents) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;
  network::ResourceRequest request = MakeResourceRequest("https://example.com");
  auto response_head = network::mojom::URLResponseHead::New();
  mojo::ScopedDataPipeConsumerHandle response_body;
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver;

  EXPECT_FALSE(interceptor->MaybeCreateLoaderForResponse(
      status, request, &response_head, &response_body, &loader,
      &client_receiver, nullptr));
}

TEST_F(OhosHttpsUpgradesInterceptorTest, CreateRedirectHandler_ReturnsNonNull) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  auto handler =
      interceptor->CreateRedirectHandler(GURL("https://example.com"));
  EXPECT_FALSE(handler.is_null());
}

TEST_F(OhosHttpsUpgradesInterceptorTest, CreateRedirectHandler_DifferentUrls) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  auto handler1 =
      interceptor->CreateRedirectHandler(GURL("https://example.com"));
  auto handler2 =
      interceptor->CreateRedirectHandler(GURL("https://other.com:443"));
  EXPECT_FALSE(handler1.is_null());
  EXPECT_FALSE(handler2.is_null());
}

TEST_F(OhosHttpsUpgradesInterceptorTest, CreateRedirectHandler_AndCallHandler) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  auto handler =
      interceptor->CreateRedirectHandler(GURL("https://example.com"));
  ASSERT_FALSE(handler.is_null());

  mojo::MessagePipe loader_pipe;
  mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver(
      std::move(loader_pipe.handle0));
  mojo::ScopedMessagePipeHandle loader_other_end =
      std::move(loader_pipe.handle1);

  mojo::PendingRemote<network::mojom::URLLoaderClient> client_pending_remote;
  auto unused_client_receiver =
      client_pending_remote.InitWithNewPipeAndPassReceiver();

  network::ResourceRequest request = MakeResourceRequest("http://example.com");

  std::move(handler).Run(request, std::move(loader_receiver),
                         std::move(client_pending_remote));

  EXPECT_TRUE(interceptor->receiver_.is_bound());
  EXPECT_TRUE(interceptor->client_.is_bound());
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       RedirectHandler_BindsReceiverAndClient) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  mojo::MessagePipe loader_pipe;
  mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver(
      std::move(loader_pipe.handle0));
  mojo::ScopedMessagePipeHandle loader_other_end =
      std::move(loader_pipe.handle1);

  mojo::PendingRemote<network::mojom::URLLoaderClient> client_pending_remote;
  auto unused_client_receiver =
      client_pending_remote.InitWithNewPipeAndPassReceiver();

  network::ResourceRequest request = MakeResourceRequest("http://example.com");

  interceptor->RedirectHandler(GURL("https://example.com"), request,
                               std::move(loader_receiver),
                               std::move(client_pending_remote));

  EXPECT_TRUE(interceptor->receiver_.is_bound());
  EXPECT_TRUE(interceptor->client_.is_bound());
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       RedirectHandler_ResetsPreviousBindings) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  mojo::MessagePipe pipe1;
  mojo::PendingReceiver<network::mojom::URLLoader> receiver1(
      std::move(pipe1.handle0));
  mojo::ScopedMessagePipeHandle other1 = std::move(pipe1.handle1);
  mojo::PendingRemote<network::mojom::URLLoaderClient> remote1;
  auto unused1 = remote1.InitWithNewPipeAndPassReceiver();

  network::ResourceRequest request = MakeResourceRequest("http://example.com");
  interceptor->RedirectHandler(GURL("https://example.com"), request,
                               std::move(receiver1), std::move(remote1));
  EXPECT_TRUE(interceptor->receiver_.is_bound());
  EXPECT_TRUE(interceptor->client_.is_bound());

  mojo::MessagePipe pipe2;
  mojo::PendingReceiver<network::mojom::URLLoader> receiver2(
      std::move(pipe2.handle0));
  mojo::ScopedMessagePipeHandle other2 = std::move(pipe2.handle1);
  mojo::PendingRemote<network::mojom::URLLoaderClient> remote2;
  auto unused2 = remote2.InitWithNewPipeAndPassReceiver();

  interceptor->RedirectHandler(GURL("https://other.com"), request,
                               std::move(receiver2), std::move(remote2));
  EXPECT_TRUE(interceptor->receiver_.is_bound());
  EXPECT_TRUE(interceptor->client_.is_bound());
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       OnConnectionClosed_ResetsReceiverAndClient) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  mojo::MessagePipe loader_pipe;
  mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver(
      std::move(loader_pipe.handle0));
  mojo::ScopedMessagePipeHandle loader_other_end =
      std::move(loader_pipe.handle1);

  mojo::PendingRemote<network::mojom::URLLoaderClient> client_pending_remote;
  auto unused_client_receiver =
      client_pending_remote.InitWithNewPipeAndPassReceiver();

  network::ResourceRequest request = MakeResourceRequest("http://example.com");
  interceptor->RedirectHandler(GURL("https://example.com"), request,
                               std::move(loader_receiver),
                               std::move(client_pending_remote));

  EXPECT_TRUE(interceptor->receiver_.is_bound());
  EXPECT_TRUE(interceptor->client_.is_bound());

  interceptor->OnConnectionClosed();

  EXPECT_FALSE(interceptor->receiver_.is_bound());
  EXPECT_FALSE(interceptor->client_.is_bound());
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       OnConnectionClosed_WithoutPriorBinding) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  EXPECT_FALSE(interceptor->receiver_.is_bound());
  EXPECT_FALSE(interceptor->client_.is_bound());

  EXPECT_NO_FATAL_FAILURE(interceptor->OnConnectionClosed());

  EXPECT_FALSE(interceptor->receiver_.is_bound());
  EXPECT_FALSE(interceptor->client_.is_bound());
}

TEST_F(OhosHttpsUpgradesInterceptorTest, RedirectLoop_UrlSeenTracking) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  GURL url1("http://example.com");
  GURL url2("http://other.com");

  EXPECT_FALSE(base::Contains(interceptor->urls_seen_, url1));
  EXPECT_FALSE(base::Contains(interceptor->urls_seen_, url2));

  interceptor->urls_seen_.insert(url1);
  EXPECT_TRUE(base::Contains(interceptor->urls_seen_, url1));
  EXPECT_FALSE(base::Contains(interceptor->urls_seen_, url2));

  interceptor->urls_seen_.insert(url2);
  EXPECT_TRUE(base::Contains(interceptor->urls_seen_, url1));
  EXPECT_TRUE(base::Contains(interceptor->urls_seen_, url2));
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       RedirectLoop_SameUrlDifferentSchemeNotContained) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  GURL http_url("http://example.com");
  GURL https_url("https://example.com");

  interceptor->urls_seen_.insert(http_url);
  EXPECT_TRUE(base::Contains(interceptor->urls_seen_, http_url));
  EXPECT_FALSE(base::Contains(interceptor->urls_seen_, https_url));
}

TEST_F(OhosHttpsUpgradesInterceptorTest, RedirectLoop_UrlsSeenSize) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  EXPECT_EQ(interceptor->urls_seen_.size(), 0u);

  interceptor->urls_seen_.insert(GURL("http://a.com"));
  EXPECT_EQ(interceptor->urls_seen_.size(), 1u);

  interceptor->urls_seen_.insert(GURL("http://b.com"));
  EXPECT_EQ(interceptor->urls_seen_.size(), 2u);

  interceptor->urls_seen_.insert(GURL("http://a.com"));
  EXPECT_EQ(interceptor->urls_seen_.size(), 2u);
}

TEST_F(OhosHttpsUpgradesInterceptorTest, FollowRedirect_NoOp) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders modified_headers;
  net::HttpRequestHeaders modified_cors_exempt_headers;
  std::optional<GURL> new_url;

  EXPECT_NO_FATAL_FAILURE(
      interceptor->FollowRedirect(removed_headers, modified_headers,
                                  modified_cors_exempt_headers, new_url));
}

TEST_F(OhosHttpsUpgradesInterceptorTest, SetPriority_NoOp) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));

  EXPECT_NO_FATAL_FAILURE(
      interceptor->SetPriority(net::RequestPriority::IDLE, 0));
  EXPECT_NO_FATAL_FAILURE(
      interceptor->SetPriority(net::RequestPriority::MAXIMUM_PRIORITY, 10));
}

TEST_F(OhosHttpsUpgradesInterceptorTest, Receiver_InitiallyUnbound) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  EXPECT_FALSE(interceptor->receiver_.is_bound());
  EXPECT_FALSE(interceptor->client_.is_bound());
}

TEST_F(OhosHttpsUpgradesInterceptorTest, WeakFactory_ValidWeakPtr) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  auto weak_ptr = interceptor->weak_factory_.GetWeakPtr();
  EXPECT_NE(weak_ptr.get(), nullptr);
  interceptor.reset();
  EXPECT_EQ(weak_ptr.get(), nullptr);
}

#if BUILDFLAG(ARKWEB_EXT_HTTPS_UPGRADES)
TEST_F(OhosHttpsUpgradesInterceptorTest,
       ShouldProceedWithHttpsUpgrades_GuardedNullFrameTreeNode) {
  content::FrameTreeNodeId frame_id = MakeFrameTreeNodeId(1);
  EXPECT_FALSE(ShouldProceedWithHttpsUpgrades(frame_id));
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       ShouldProceedWithHttpsUpgrades_GuardedDefaultId) {
  content::FrameTreeNodeId frame_id;
  EXPECT_FALSE(ShouldProceedWithHttpsUpgrades(frame_id));
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoader_GuardedNullBrowserContext) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  network::ResourceRequest request = MakeResourceRequest("http://example.com");

  bool callback_called = false;
  interceptor->MaybeCreateLoader(
      request, nullptr,
      base::BindOnce([](bool* called, RequestHandler) { *called = true; },
                     base::Unretained(&callback_called)));

  EXPECT_TRUE(callback_called);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoader_GuardedNonHttpSchemeNullProfile) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("ftp://example.com");

  bool callback_called = false;
  interceptor->MaybeCreateLoader(
      request, nullptr,
      base::BindOnce([](bool* called, RequestHandler) { *called = true; },
                     base::Unretained(&callback_called)));

  EXPECT_TRUE(callback_called);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoader_GuardedIpHostNullProfile) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("http://192.168.1.1/");

  bool callback_called = false;
  interceptor->MaybeCreateLoader(
      request, nullptr,
      base::BindOnce([](bool* called, RequestHandler) { *called = true; },
                     base::Unretained(&callback_called)));

  EXPECT_TRUE(callback_called);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoader_GuardedLocalhostNullProfile) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("http://localhost/");

  bool callback_called = false;
  interceptor->MaybeCreateLoader(
      request, nullptr,
      base::BindOnce([](bool* called, RequestHandler) { *called = true; },
                     base::Unretained(&callback_called)));

  EXPECT_TRUE(callback_called);
}

TEST_F(OhosHttpsUpgradesInterceptorTest,
       MaybeCreateLoader_GuardedNonUniqueHostNullProfile) {
  auto interceptor =
      std::make_unique<OhosHttpsUpgradesInterceptor>(MakeFrameTreeNodeId(1));
  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("http://intranet/");

  bool callback_called = false;
  interceptor->MaybeCreateLoader(
      request, nullptr,
      base::BindOnce([](bool* called, RequestHandler) { *called = true; },
                     base::Unretained(&callback_called)));

  EXPECT_TRUE(callback_called);
}
#endif  // BUILDFLAG(ARKWEB_EXT_HTTPS_UPGRADES)
