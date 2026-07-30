// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "arkweb/build/features/features.h"
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_connection_info.h"
#include "net/socket/connection_attempts.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Access private members of HttpNetworkTransaction for testing.
// We bracket only the target header so other headers are unaffected.
#define private public
#include "net/http/http_network_transaction.h"
#undef private

// The ext header is included by http_network_transaction.h at the end,
// so ArkWebHttpNetworkTransactionExt is already fully defined here.

namespace net {

namespace {

class ArkWebHttpNetworkTransactionExtTest : public ::testing::Test {
 protected:
  void SetUp() override {
    session_deps_ = std::make_unique<SpdySessionDependencies>();
    session_ = SpdySessionDependencies::SpdyCreateSession(session_deps_.get());

    transaction_ = std::make_unique<ArkWebHttpNetworkTransactionExt>(
        DEFAULT_PRIORITY, session_.get());
  }

  void TearDown() override {
    transaction_.reset();
    session_.reset();
    session_deps_.reset();
    task_environment_.RunUntilIdle();
  }

  // Creates a real HttpStreamRequest and assigns it to
  // transaction_->stream_request_ so that
  // CopyConnectionAttemptsFromStreamRequest() can be called safely.
  void SetupStreamRequest() {
    request_info_.method = "GET";
    request_info_.url = GURL("https://example.org/");
    request_info_.traffic_annotation =
        MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

    transaction_->request_ = &request_info_;
    transaction_->stream_request_ =
        session_->http_stream_factory()->RequestStream(
            request_info_, DEFAULT_PRIORITY,
            /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>(),
            transaction_.get(),
            /*enable_ip_based_pooling_for_h2=*/true,
            /*enable_alternative_services=*/true, NetLogWithSource());
  }

  void CleanupStreamRequest() {
    transaction_->stream_request_.reset();
    transaction_->request_ = nullptr;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<SpdySessionDependencies> session_deps_;
  std::unique_ptr<HttpNetworkSession> session_;
  std::unique_ptr<ArkWebHttpNetworkTransactionExt> transaction_;
  HttpRequestInfo request_info_;
};

// ---------- ARKWEB_EXT_HTTP_DNS_FALLBACK tests ----------

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       RestartWithSecureDnsOnly_MaxRestartsExceeded) {
  transaction_->num_restarts_ = 32;  // kMaxRestarts = 32
  TestCompletionCallback callback;
  int rv = transaction_->RestartWithSecureDnsOnly(callback.callback());
  EXPECT_EQ(rv, ERR_TOO_MANY_RETRIES);
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateFallbackStreamWithSecureDnsOnlyComplete_OK) {
  SetupStreamRequest();
  int rv = transaction_->DoCreateFallbackStreamWithSecureDnsOnlyComplete(OK);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(HttpNetworkTransaction::STATE_CONNECTED_CALLBACK,
            transaction_->next_state_);
  CleanupStreamRequest();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateFallbackStreamWithSecureDnsOnlyComplete_Http11Required) {
  SetupStreamRequest();
  // HandleHttp11Required() calls ResetConnectionAndRequestForResend() and
  // returns OK.
  int rv = transaction_->DoCreateFallbackStreamWithSecureDnsOnlyComplete(
      ERR_HTTP_1_1_REQUIRED);
  EXPECT_EQ(OK, rv);
  CleanupStreamRequest();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateFallbackStreamWithSecureDnsOnlyComplete_ProxyHttp11Required) {
  SetupStreamRequest();
  // HandleHttp11Required() calls ResetConnectionAndRequestForResend() and
  // returns OK.
  int rv = transaction_->DoCreateFallbackStreamWithSecureDnsOnlyComplete(
      ERR_PROXY_HTTP_1_1_REQUIRED);
  EXPECT_EQ(OK, rv);
  CleanupStreamRequest();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateFallbackStreamWithSecureDnsOnlyComplete_OtherError) {
  SetupStreamRequest();
  int rv = transaction_->DoCreateFallbackStreamWithSecureDnsOnlyComplete(
      ERR_CONNECTION_REFUSED);
  EXPECT_EQ(ERR_CONNECTION_REFUSED, rv);
  CleanupStreamRequest();
}

#endif  // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

// ---------- ARKWEB_EX_FALLBACK_PROXY tests ----------

#if BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       RestartWithFallbackProxy_MaxRestartsExceeded) {
  transaction_->num_restarts_ = 32;
  TestCompletionCallback callback;
  int rv = transaction_->RestartWithFallbackProxy(callback.callback());
  EXPECT_EQ(rv, ERR_TOO_MANY_RETRIES);
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateStreamWithFallbackProxy_NullRequest) {
  ASSERT_EQ(transaction_->request_, nullptr);
  int rv = transaction_->DoCreateStreamWithFallbackProxy();
  EXPECT_EQ(rv, ERR_ABORTED);
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateStreamWithFallbackProxyComplete_OK) {
  SetupStreamRequest();
  int rv = transaction_->DoCreateStreamWithFallbackProxyComplete(OK);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(HttpNetworkTransaction::STATE_CONNECTED_CALLBACK,
            transaction_->next_state_);
  CleanupStreamRequest();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateStreamWithFallbackProxyComplete_Http11Required) {
  SetupStreamRequest();
  // HandleHttp11Required() calls ResetConnectionAndRequestForResend() and
  // returns OK.
  int rv = transaction_->DoCreateStreamWithFallbackProxyComplete(
      ERR_HTTP_1_1_REQUIRED);
  EXPECT_EQ(OK, rv);
  CleanupStreamRequest();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateStreamWithFallbackProxyComplete_ProxyHttp11Required) {
  SetupStreamRequest();
  // HandleHttp11Required() calls ResetConnectionAndRequestForResend() and
  // returns OK.
  int rv = transaction_->DoCreateStreamWithFallbackProxyComplete(
      ERR_PROXY_HTTP_1_1_REQUIRED);
  EXPECT_EQ(OK, rv);
  CleanupStreamRequest();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateStreamWithFallbackProxyComplete_OtherError) {
  SetupStreamRequest();
  int rv = transaction_->DoCreateStreamWithFallbackProxyComplete(
      ERR_CONNECTION_REFUSED);
  EXPECT_EQ(ERR_CONNECTION_REFUSED, rv);
  CleanupStreamRequest();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       RestartWithDirect_MaxRestartsExceeded) {
  transaction_->num_restarts_ = 32;
  TestCompletionCallback callback;
  int rv = transaction_->RestartWithDirect(callback.callback());
  EXPECT_EQ(rv, ERR_TOO_MANY_RETRIES);
}

#endif  // BUILDFLAG(ARKWEB_EX_FALLBACK_PROXY)

// ---------- ARKWEB_EXT_LOG_MESSAGE tests ----------

#if BUILDFLAG(ARKWEB_EXT_LOG_MESSAGE)

TEST_F(ArkWebHttpNetworkTransactionExtTest, StartRecording_FirstTime) {
  transaction_->is_recording_ = false;
  transaction_->StartRecording();
  EXPECT_TRUE(transaction_->is_recording_);
  EXPECT_TRUE(transaction_->timer_.IsRunning());
}

TEST_F(ArkWebHttpNetworkTransactionExtTest, StartRecording_AlreadyRecording) {
  transaction_->is_recording_ = true;
  transaction_->timer_.Start(FROM_HERE, base::Seconds(5), transaction_.get(),
                             &ArkWebHttpNetworkTransactionExt::ReportTimeout);
  EXPECT_TRUE(transaction_->timer_.IsRunning());
  transaction_->StartRecording();
  EXPECT_TRUE(transaction_->is_recording_);
  EXPECT_TRUE(transaction_->timer_.IsRunning());
}

TEST_F(ArkWebHttpNetworkTransactionExtTest, StopRecording_NotRecording) {
  transaction_->is_recording_ = false;
  transaction_->StopRecording();
  EXPECT_FALSE(transaction_->is_recording_);
}

TEST_F(ArkWebHttpNetworkTransactionExtTest, StopRecording_WhileRecording) {
  transaction_->is_recording_ = true;
  transaction_->timer_.Start(FROM_HERE, base::Seconds(5), transaction_.get(),
                             &ArkWebHttpNetworkTransactionExt::ReportTimeout);
  transaction_->StopRecording();
  EXPECT_FALSE(transaction_->is_recording_);
  EXPECT_FALSE(transaction_->timer_.IsRunning());
}

#if BUILDFLAG(ARKWEB_LOGGER_REPORT)
TEST_F(ArkWebHttpNetworkTransactionExtTest, ReportTimeout_WithResponseInfo) {
  transaction_->is_recording_ = true;
  transaction_->url_ = GURL("https://example.com/test");
  transaction_->response_.remote_endpoint =
      IPEndPoint(IPAddress(192, 168, 1, 1), 443);
  transaction_->response_.connection_info = HttpConnectionInfo::kHTTP1_1;
  transaction_->received_body_bytes_ = 12345;

  transaction_->ReportTimeout();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest, ReportTimeout_NullResponseInfo) {
  transaction_->is_recording_ = true;
  transaction_->url_ = GURL("https://example.com/test");
  transaction_->ReportTimeout();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest, ReportTimeout_StrictLogMode) {
  transaction_->is_recording_ = true;
  transaction_->url_ = GURL("https://example.com/test");
  transaction_->ReportTimeout();
}
#endif  // BUILDFLAG(ARKWEB_LOGGER_REPORT)

#endif  // BUILDFLAG(ARKWEB_EXT_LOG_MESSAGE)

// ---------- ARKWEB_EXT_NAVIGATION tests ----------

#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       GetExtraConnectionAttempts_InitiallyEmpty) {
  ConnectionAttempts attempts = transaction_->GetExtraConnectionAttempts();
  EXPECT_TRUE(attempts.empty());
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       CopyConnectionAttemptsFromStreamRequest) {
  SetupStreamRequest();

  // Populate extra_connection_attempts in the stream request directly.
  ASSERT_TRUE(transaction_->stream_request_);
  ConnectionAttempts test_attempts;
  test_attempts.push_back(ConnectionAttempt(
      IPEndPoint(IPAddress(1, 2, 3, 4), 443), ERR_CONNECTION_REFUSED));
  test_attempts.push_back(
      ConnectionAttempt(IPEndPoint(IPAddress(5, 6, 7, 8), 443), OK));
  transaction_->stream_request_->AddExtraConnectionAttempts(test_attempts);

  transaction_->CopyConnectionAttemptsFromStreamRequest();

  ConnectionAttempts attempts = transaction_->GetExtraConnectionAttempts();
  ASSERT_EQ(attempts.size(), 2u);
  EXPECT_EQ(attempts[0].result, ERR_CONNECTION_REFUSED);
  EXPECT_EQ(attempts[1].result, OK);

  CleanupStreamRequest();
}

#endif  // BUILDFLAG(ARKWEB_EXT_NAVIGATION)

// ---------- Constructor / Destructor tests ----------

TEST_F(ArkWebHttpNetworkTransactionExtTest, AsArkWebExt_ReturnsThis) {
  ArkWebHttpNetworkTransactionExt* ptr =
      transaction_->AsArkWebHttpNetworkTransactionExt();
  EXPECT_EQ(ptr, transaction_.get());
}

TEST_F(ArkWebHttpNetworkTransactionExtTest, BaseClassReturnsNull) {
  HttpNetworkTransaction base_transaction(DEFAULT_PRIORITY, session_.get());
  ArkWebHttpNetworkTransactionExt* ptr =
      base_transaction.AsArkWebHttpNetworkTransactionExt();
  EXPECT_EQ(ptr, nullptr);
}

}  // namespace

}  // namespace net
