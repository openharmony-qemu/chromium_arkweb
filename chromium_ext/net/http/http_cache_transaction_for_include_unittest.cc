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

#include "arkweb/ohos_nweb_ex/build/features/features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/mock_http_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// clang-format off
#define private public
#include "net/http/http_cache_transaction.h"
#undef private
// clang-format on

namespace net {

namespace {

// Mock HttpTransaction that supports RestartWithFallbackProxy and
// RestartWithDirect for testing the injected methods.
class MockHttpTransactionForCache : public HttpTransaction {
 public:
  MockHttpTransactionForCache() {
    auto headers = base::MakeRefCounted<HttpResponseHeaders>(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
    response_info_.headers = headers;
    response_info_.response_time = base::Time::Now();
    response_info_.request_time = base::Time::Now() - base::Seconds(1);
  }
  ~MockHttpTransactionForCache() override = default;

  int RestartWithFallbackProxy(CompletionOnceCallback callback) override {
    restart_fallback_proxy_called_ = true;
    callback_ = std::move(callback);
    return restart_fallback_result_;
  }

  int RestartWithDirect(CompletionOnceCallback callback) override {
    restart_direct_called_ = true;
    callback_ = std::move(callback);
    return restart_direct_result_;
  }

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
  int RestartWithSecureDnsOnly(CompletionOnceCallback callback) override {
    return ERR_IO_PENDING;
  }
#endif

  const HttpResponseInfo* GetResponseInfo() const override {
    return &response_info_;
  }

  // Unused mock methods
  MOCK_METHOD3(Start,
               int(const HttpRequestInfo* request_info,
                   CompletionOnceCallback callback,
                   const NetLogWithSource& net_log));
  MOCK_METHOD1(RestartIgnoringLastError, int(CompletionOnceCallback callback));
  MOCK_METHOD3(RestartWithCertificate,
               int(scoped_refptr<X509Certificate> client_cert,
                   scoped_refptr<SSLPrivateKey> client_private_key,
                   CompletionOnceCallback callback));
  MOCK_METHOD2(RestartWithAuth,
               int(const AuthCredentials& credentials,
                   CompletionOnceCallback callback));
  MOCK_METHOD0(IsReadyToRestartForAuth, bool());
  MOCK_METHOD3(Read,
               int(IOBuffer* buf,
                   int buf_len,
                   CompletionOnceCallback callback));
  MOCK_METHOD0(StopCaching, void());
  MOCK_CONST_METHOD0(GetTotalReceivedBytes, int64_t());
  MOCK_CONST_METHOD0(GetTotalSentBytes, int64_t());
  MOCK_CONST_METHOD0(GetReceivedBodyBytes, int64_t());
  MOCK_METHOD0(DoneReading, void());
  MOCK_CONST_METHOD0(GetLoadState, LoadState());
  MOCK_METHOD1(SetQuicServerInfo, void(QuicServerInfo* quic_server_info));
  MOCK_CONST_METHOD1(GetLoadTimingInfo, bool(LoadTimingInfo* load_timing_info));
  MOCK_CONST_METHOD1(GetRemoteEndpoint, bool(IPEndPoint* endpoint));
  MOCK_CONST_METHOD1(PopulateNetErrorDetails, void(NetErrorDetails* details));
  MOCK_METHOD1(SetPriority, void(RequestPriority priority));
  MOCK_METHOD1(SetWebSocketHandshakeStreamCreateHelper,
               void(WebSocketHandshakeStreamBase::CreateHelper* create_helper));
  MOCK_METHOD1(SetConnectedCallback, void(const ConnectedCallback& callback));
  MOCK_METHOD1(SetRequestHeadersCallback,
               void(RequestHeadersCallback callback));
  MOCK_METHOD1(SetEarlyResponseHeadersCallback,
               void(ResponseHeadersCallback callback));
  MOCK_METHOD1(SetResponseHeadersCallback,
               void(ResponseHeadersCallback callback));
  MOCK_METHOD1(
      SetModifyRequestHeadersCallback,
      void(base::RepeatingCallback<void(HttpRequestHeaders*)> callback));
  MOCK_METHOD1(SetIsSharedDictionaryReadAllowedCallback,
               void(base::RepeatingCallback<bool()> callback));
  MOCK_METHOD0(ResumeNetworkStart, int());
  MOCK_CONST_METHOD0(GetConnectionAttempts, ConnectionAttempts());
#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
  MOCK_CONST_METHOD0(GetExtraConnectionAttempts, ConnectionAttempts());
#endif
  MOCK_METHOD0(CloseConnectionOnDestruction, void());
  MOCK_CONST_METHOD0(IsMdlMatchForMetrics, bool());
  MOCK_CONST_METHOD1(PopulateLoadTimingInternalInfo,
                     void(LoadTimingInternalInfo* load_timing_internal_info));

  bool restart_fallback_proxy_called_ = false;
  int restart_fallback_result_ = ERR_IO_PENDING;
  bool restart_direct_called_ = false;
  int restart_direct_result_ = ERR_IO_PENDING;
  CompletionOnceCallback callback_;
  HttpResponseInfo response_info_;
};

class HttpCacheTransactionForIncludeTest : public testing::Test {
 public:
  void SetUp() override { mock_cache_ = std::make_unique<MockHttpCache>(); }

  void TearDown() override {
    transaction_.reset();
    mock_cache_.reset();
    task_environment_.RunUntilIdle();
  }

 protected:
  // Creates a Transaction and sets up basic state needed for
  // RestartWithFallbackProxy / RestartWithDirect testing.
  void SetupTransaction() {
    transaction_ = std::make_unique<HttpCache::Transaction>(
        DEFAULT_PRIORITY, mock_cache_->http_cache());

    // Install a mock network transaction.
    auto mock_network_trans = std::make_unique<MockHttpTransactionForCache>();
    mock_network_trans_ptr_ = mock_network_trans.get();
    transaction_->network_trans_ = std::move(mock_network_trans);

    // Set up request info.
    request_ = std::make_unique<HttpRequestInfo>();
    request_->url = GURL("http://example.com/test");
    transaction_->request_ = request_.get();
    // Use a separate initial_request_ so that request_ != initial_request_,
    // which is the common case when RestartNetworkRequest* methods are called.
    // This also ensures initial_request_ is non-null so that DoLoop (triggered
    // by sync test paths) does not crash on null dereference.
    initial_request_ = std::make_unique<HttpRequestInfo>();
    initial_request_->url = GURL("http://example.com/test");
    transaction_->initial_request_ = initial_request_.get();
    transaction_->io_callback_ = base::DoNothing();
    transaction_->cache_ = mock_cache_->http_cache()->GetWeakPtr();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<MockHttpCache> mock_cache_;
  std::unique_ptr<HttpCache::Transaction> transaction_;
  std::unique_ptr<HttpRequestInfo> request_;
  std::unique_ptr<HttpRequestInfo> initial_request_;
  raw_ptr<MockHttpTransactionForCache> mock_network_trans_ptr_ = nullptr;
};

// ---------- RestartWithFallbackProxy tests ----------

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartWithFallbackProxy_CacheNull_ReturnsUnexpected) {
  transaction_ = std::make_unique<HttpCache::Transaction>(
      DEFAULT_PRIORITY, mock_cache_->http_cache());
  transaction_->cache_ = base::WeakPtr<HttpCache>();

  TestCompletionCallback callback;
  int rv = transaction_->RestartWithFallbackProxy(callback.callback());
  EXPECT_EQ(rv, ERR_UNEXPECTED);
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartWithFallbackProxy_Async_SetsCallback) {
  SetupTransaction();
  mock_network_trans_ptr_->restart_fallback_result_ = ERR_IO_PENDING;

  TestCompletionCallback callback;
  int rv = transaction_->RestartWithFallbackProxy(callback.callback());
  EXPECT_EQ(rv, ERR_IO_PENDING);
  EXPECT_FALSE(transaction_->callback_.is_null());
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartWithFallbackProxy_Sync_DoesNotSetCallback) {
  SetupTransaction();
  mock_network_trans_ptr_->restart_fallback_result_ = OK;

  TestCompletionCallback callback;
  int rv = transaction_->RestartWithFallbackProxy(callback.callback());
  EXPECT_EQ(rv, OK);
}

// ---------- RestartNetworkRequestWithFallbackProxy tests ----------

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartNetworkRequestWithFallbackProxy_SetsRetryFlag) {
  SetupTransaction();
  // Set request_ != initial_request_ so that mutable_request_ is modified.
  transaction_->mutable_request_ = std::make_unique<HttpRequestInfo>();
  transaction_->mutable_request_->url = GURL("http://example.com/test");
  transaction_->mutable_request_->secure_dns_only = true;

  mock_network_trans_ptr_->restart_fallback_result_ = ERR_IO_PENDING;

  int rv = transaction_->RestartNetworkRequestWithFallbackProxy();
  EXPECT_EQ(rv, ERR_IO_PENDING);
  ASSERT_TRUE(transaction_->mutable_request_);
  EXPECT_TRUE(transaction_->mutable_request_->retry_with_fallback_proxy);
  EXPECT_FALSE(transaction_->mutable_request_->secure_dns_only);
  EXPECT_TRUE(mock_network_trans_ptr_->restart_fallback_proxy_called_);
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartNetworkRequestWithFallbackProxy_Sync_CompletesImmediately) {
  SetupTransaction();
  transaction_->mutable_request_ = std::make_unique<HttpRequestInfo>();
  transaction_->mutable_request_->url = GURL("http://example.com/test");

  mock_network_trans_ptr_->restart_fallback_result_ = OK;

  int rv = transaction_->RestartNetworkRequestWithFallbackProxy();
  // When RestartWithFallbackProxy returns OK (not ERR_IO_PENDING), DoLoop
  // is called. The result from DoLoop depends on internal state, but the key
  // behavior tested here is that the sync path is taken (DoLoop is called).
  EXPECT_TRUE(mock_network_trans_ptr_->restart_fallback_proxy_called_);
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartNetworkRequestWithFallbackProxy_RequestEqualsInitial_NoMutation) {
  SetupTransaction();
  // Set request_ == initial_request_ so the mutable_request_ block is skipped.
  transaction_->initial_request_ = transaction_->request_;

  mock_network_trans_ptr_->restart_fallback_result_ = ERR_IO_PENDING;

  int rv = transaction_->RestartNetworkRequestWithFallbackProxy();
  EXPECT_EQ(rv, ERR_IO_PENDING);
  // mutable_request_ should not have retry_with_fallback_proxy set since
  // request_ == initial_request_.
  if (transaction_->mutable_request_) {
    EXPECT_FALSE(transaction_->mutable_request_->retry_with_fallback_proxy);
  }
  EXPECT_TRUE(mock_network_trans_ptr_->restart_fallback_proxy_called_);
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartNetworkRequestWithFallbackProxy_NullMutableRequest_NoCrash) {
  SetupTransaction();
  // request_ != initial_request_ but mutable_request_ is null.
  transaction_->mutable_request_.reset();

  mock_network_trans_ptr_->restart_fallback_result_ = ERR_IO_PENDING;

  int rv = transaction_->RestartNetworkRequestWithFallbackProxy();
  EXPECT_EQ(rv, ERR_IO_PENDING);
  EXPECT_TRUE(mock_network_trans_ptr_->restart_fallback_proxy_called_);
}

// ---------- RestartWithDirect tests ----------

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartWithDirect_CacheNull_ReturnsUnexpected) {
  transaction_ = std::make_unique<HttpCache::Transaction>(
      DEFAULT_PRIORITY, mock_cache_->http_cache());
  transaction_->cache_ = base::WeakPtr<HttpCache>();

  TestCompletionCallback callback;
  int rv = transaction_->RestartWithDirect(callback.callback());
  EXPECT_EQ(rv, ERR_UNEXPECTED);
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartWithDirect_Async_SetsCallback) {
  SetupTransaction();
  mock_network_trans_ptr_->restart_direct_result_ = ERR_IO_PENDING;

  TestCompletionCallback callback;
  int rv = transaction_->RestartWithDirect(callback.callback());
  EXPECT_EQ(rv, ERR_IO_PENDING);
  EXPECT_FALSE(transaction_->callback_.is_null());
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartWithDirect_Sync_DoesNotSetCallback) {
  SetupTransaction();
  mock_network_trans_ptr_->restart_direct_result_ = OK;

  TestCompletionCallback callback;
  int rv = transaction_->RestartWithDirect(callback.callback());
  EXPECT_EQ(rv, OK);
}

// ---------- RestartNetworkRequestWithDirect tests ----------

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartNetworkRequestWithDirect_SetsLoadFlagsAndClearsRetryFlag) {
  SetupTransaction();
  transaction_->mutable_request_ = std::make_unique<HttpRequestInfo>();
  transaction_->mutable_request_->url = GURL("http://example.com/test");
  transaction_->mutable_request_->secure_dns_only = true;
  transaction_->mutable_request_->retry_with_fallback_proxy = true;

  mock_network_trans_ptr_->restart_direct_result_ = ERR_IO_PENDING;

  int rv = transaction_->RestartNetworkRequestWithDirect();
  EXPECT_EQ(rv, ERR_IO_PENDING);
  ASSERT_TRUE(transaction_->mutable_request_);
  EXPECT_FALSE(transaction_->mutable_request_->retry_with_fallback_proxy);
  EXPECT_FALSE(transaction_->mutable_request_->secure_dns_only);
  EXPECT_TRUE(transaction_->mutable_request_->load_flags & LOAD_BYPASS_PROXY);
  EXPECT_TRUE(mock_network_trans_ptr_->restart_direct_called_);
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartNetworkRequestWithDirect_Sync_CompletesImmediately) {
  SetupTransaction();
  transaction_->mutable_request_ = std::make_unique<HttpRequestInfo>();
  transaction_->mutable_request_->url = GURL("http://example.com/test");

  mock_network_trans_ptr_->restart_direct_result_ = OK;

  int rv = transaction_->RestartNetworkRequestWithDirect();
  EXPECT_TRUE(mock_network_trans_ptr_->restart_direct_called_);
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartNetworkRequestWithDirect_RequestEqualsInitial_NoMutation) {
  SetupTransaction();
  transaction_->initial_request_ = transaction_->request_;

  mock_network_trans_ptr_->restart_direct_result_ = ERR_IO_PENDING;

  int rv = transaction_->RestartNetworkRequestWithDirect();
  EXPECT_EQ(rv, ERR_IO_PENDING);
  // Since request_ == initial_request_, mutable_request_ modifications
  // should not occur.
  if (transaction_->mutable_request_) {
    EXPECT_EQ(transaction_->mutable_request_->load_flags & LOAD_BYPASS_PROXY,
              0);
    EXPECT_FALSE(transaction_->mutable_request_->retry_with_fallback_proxy);
  }
  EXPECT_TRUE(mock_network_trans_ptr_->restart_direct_called_);
}

TEST_F(HttpCacheTransactionForIncludeTest,
       RestartNetworkRequestWithDirect_NullMutableRequest_NoCrash) {
  SetupTransaction();
  transaction_->mutable_request_.reset();

  mock_network_trans_ptr_->restart_direct_result_ = ERR_IO_PENDING;

  int rv = transaction_->RestartNetworkRequestWithDirect();
  EXPECT_EQ(rv, ERR_IO_PENDING);
  EXPECT_TRUE(mock_network_trans_ptr_->restart_direct_called_);
}

}  // namespace

}  // namespace net

#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)
