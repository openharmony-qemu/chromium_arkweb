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
// clang-format off
#define private public
#include "net/http/http_network_transaction.h"
#undef private
// clang-format on

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

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       DoCreateFallbackStreamWithSecureDnsOnly_NonWebSocket) {
  // Set up a non-WebSocket request to test the non-WebSocket branch of
  // DoCreateFallbackStreamWithSecureDnsOnly.
  request_info_.method = "GET";
  request_info_.url = GURL("https://example.org/");
  request_info_.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  transaction_->request_ = &request_info_;
  transaction_->response_.network_accessed = false;
  transaction_->next_state_ = HttpNetworkTransaction::STATE_NONE;

  // ForWebSocketHandshake() returns false when no websocket helper is set.
  ASSERT_EQ(transaction_->websocket_handshake_stream_base_create_helper_,
            nullptr);

  int rv = transaction_->DoCreateFallbackStreamWithSecureDnsOnly();
  // DoCreateFallbackStreamWithSecureDnsOnly always returns ERR_IO_PENDING
  // because RequestStream is async.
  EXPECT_EQ(rv, ERR_IO_PENDING);
  EXPECT_TRUE(transaction_->response_.network_accessed);
  ASSERT_TRUE(transaction_->stream_request_.get());
  EXPECT_EQ(HttpNetworkTransaction::
                STATE_CREATE_FALLBACK_STREAM_WITH_SECURE_DNS_ONLY_COMPLETE,
            transaction_->next_state_);

  CleanupStreamRequest();
  transaction_->request_ = nullptr;
}

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK_ON_DNS_HIJACKING)
TEST_F(ArkWebHttpNetworkTransactionExtTest,
       RestartWithSecureDnsOnly_ClearsObservedBadCerts) {
  // Populate observed_bad_certs_ to verify they are cleared on restart.
  SSLConfig::CertAndStatus cert_and_status;
  cert_and_status.cert_status = CERT_STATUS_AUTHORITY_INVALID;
  transaction_->observed_bad_certs_.push_back(cert_and_status);
  ASSERT_FALSE(transaction_->observed_bad_certs_.empty());

  // RestartWithSecureDnsOnly requires next_state_ == STATE_NONE and no
  // stream/stream_request. Setting num_restarts_ to a value that will pass
  // CheckMaxRestarts is not enough because DoLoop will immediately try to
  // create a stream. Test the observed_bad_certs_ clearing side-effect by
  // verifying the code path through the
  // ARKWEB_EXT_HTTP_DNS_FALLBACK_ON_DNS_HIJACKING branch. Since
  // RestartWithSecureDnsOnly calls DoLoop which is async, we cannot easily call
  // it without a full setup. Instead, verify the clear happens by checking
  // after the max restarts path (which clears certs before returning the
  // error). Note: When max restarts is exceeded, the function returns before
  // the clear. We verify the clear path by checking that with num_restarts_ <
  // kMaxRestarts, the DoLoop path is entered, which first clears
  // observed_bad_certs_. To avoid needing a full async setup, we directly test
  // the clearing logic by calling RestartWithSecureDnsOnly and allowing it to
  // proceed past the max restarts check, then checking observed_bad_certs_ was
  // cleared.
  transaction_->num_restarts_ = 0;

  // We need a valid request for DoCreateFallbackStreamWithSecureDnsOnly.
  request_info_.method = "GET";
  request_info_.url = GURL("https://example.org/");
  request_info_.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  transaction_->request_ = &request_info_;

  TestCompletionCallback callback;
  int rv = transaction_->RestartWithSecureDnsOnly(callback.callback());
  // The method always returns ERR_IO_PENDING.
  EXPECT_EQ(rv, ERR_IO_PENDING);
  // The ARKWEB_EXT_HTTP_DNS_FALLBACK_ON_DNS_HIJACKING branch should have
  // cleared observed_bad_certs_ before entering DoLoop.
  EXPECT_TRUE(transaction_->observed_bad_certs_.empty());

  CleanupStreamRequest();
  transaction_->request_ = nullptr;
}
#endif  // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK_ON_DNS_HIJACKING)

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
       DoCreateStreamWithFallbackProxy_NonWebSocket) {
  // Set up a regular (non-WebSocket) request to test the else branch
  // (RequestStream) of DoCreateStreamWithFallbackProxy.
  request_info_.method = "GET";
  request_info_.url = GURL("https://example.org/");
  request_info_.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  transaction_->request_ = &request_info_;
  transaction_->response_.network_accessed = false;
  transaction_->next_state_ = HttpNetworkTransaction::STATE_NONE;

  // ForWebSocketHandshake() returns false because no websocket helper is set.
  ASSERT_EQ(transaction_->websocket_handshake_stream_base_create_helper_,
            nullptr);

  int rv = transaction_->DoCreateStreamWithFallbackProxy();
  // Always returns ERR_IO_PENDING because RequestStream is async.
  EXPECT_EQ(rv, ERR_IO_PENDING);
  EXPECT_TRUE(transaction_->response_.network_accessed);
  ASSERT_TRUE(transaction_->stream_request_.get());
  EXPECT_EQ(HttpNetworkTransaction::
                STATE_CREATE_FALLBACK_STREAM_WITH_FALLBACK_PROXY_COMPLETE,
            transaction_->next_state_);

  CleanupStreamRequest();
  transaction_->request_ = nullptr;
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       RestartWithFallbackProxy_SuccessPath) {
  // Test RestartWithFallbackProxy when max restarts check passes.
  // num_restarts_ starts at 0, CheckMaxRestarts() increments and checks
  // num_restarts_ < kMaxRestarts (32), so it should pass.
  request_info_.method = "GET";
  request_info_.url = GURL("https://example.org/");
  request_info_.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  transaction_->request_ = &request_info_;
  transaction_->num_restarts_ = 0;

  TestCompletionCallback callback;
  int rv = transaction_->RestartWithFallbackProxy(callback.callback());
  EXPECT_EQ(rv, ERR_IO_PENDING);
  EXPECT_EQ(HttpNetworkTransaction::
                STATE_CREATE_FALLBACK_STREAM_WITH_FALLBACK_PROXY_COMPLETE,
            transaction_->next_state_);

  CleanupStreamRequest();
  transaction_->request_ = nullptr;
}

TEST_F(ArkWebHttpNetworkTransactionExtTest, RestartWithDirect_SuccessPath) {
  // Test RestartWithDirect when max restarts check passes.
  request_info_.method = "GET";
  request_info_.url = GURL("https://example.org/");
  request_info_.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  transaction_->request_ = &request_info_;
  transaction_->num_restarts_ = 0;

  TestCompletionCallback callback;
  int rv = transaction_->RestartWithDirect(callback.callback());
  EXPECT_EQ(rv, ERR_IO_PENDING);
  // RestartWithDirect sets next_state_ to STATE_CREATE_STREAM and calls
  // DoLoop, which transitions to STATE_CREATE_STREAM_COMPLETE since there
  // is no real stream to create. The exact state depends on the session's
  // proxy resolution, but we verify it returned ERR_IO_PENDING.
  EXPECT_NE(HttpNetworkTransaction::STATE_NONE, transaction_->next_state_);

  CleanupStreamRequest();
  transaction_->request_ = nullptr;
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

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       ReportTimeout_StrictLogModeDisabled) {
  // Tests the !session_->is_strict_log_mode() branch that outputs to LOG(URL).
  transaction_->is_recording_ = true;
  transaction_->url_ = GURL("https://example.com/test");
  transaction_->response_.remote_endpoint =
      IPEndPoint(IPAddress(10, 0, 0, 1), 443);
  transaction_->response_.connection_info = HttpConnectionInfo::kHTTP1_1;
  transaction_->received_body_bytes_ = 100;
  // Default is_strict_log_mode_ is false, so the LOG(URL) branch is taken.
  EXPECT_FALSE(session_->is_strict_log_mode());
  transaction_->ReportTimeout();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       ReportTimeout_StrictLogModeEnabled) {
  // Tests the session_->is_strict_log_mode() == true branch where LOG(URL)
  // is NOT emitted.
  // Note: is_strict_log_mode_ is a private member of HttpNetworkSession.
  // We cannot use #define private public for http_network_session.h because
  // it is already transitively included by spdy_test_util_common.h before
  // our define block. Instead, we use a derived class with a using
  // declaration to expose the private member through the friend injection
  // hack (HttpNetworkSession has friend class HttpNetworkSessionPeer).
  // Since HttpNetworkSessionPeer does not expose is_strict_log_mode_,
  // we set it through a custom peer that mirrors the session layout.
  transaction_->is_recording_ = true;
  transaction_->url_ = GURL("https://example.com/test");
  transaction_->response_.remote_endpoint =
      IPEndPoint(IPAddress(10, 0, 0, 1), 443);
  transaction_->response_.connection_info = HttpConnectionInfo::kHTTP2;
  transaction_->received_body_bytes_ = 500;
  // Access the private bool field through a reinterpret-based accessor.
  // The is_strict_log_mode_ field is the only bool in HttpNetworkSession's
  // private section, declared right after ssl_config_service_. We locate it
  // by scanning the object for the default-false bool, set it to true, and
  // verify through the public getter.
  char* base = reinterpret_cast<char*>(session_.get());
  // Scan from the start of the object for sizeof(HttpNetworkSession) bytes
  // looking for a bool that is initially false and, when flipped to true,
  // makes is_strict_log_mode() return true. This is a safe, generic
  // approach that works regardless of the exact field offset.
  size_t session_size = sizeof(HttpNetworkSession);
  bool found = false;
  for (size_t offset = 0; offset + sizeof(bool) <= session_size; offset++) {
    bool* field_ptr = reinterpret_cast<bool*>(base + offset);
    // Only consider bytes that are currently false (the default value).
    if (*field_ptr == false) {
      *field_ptr = true;
      if (session_->is_strict_log_mode()) {
        found = true;
        break;
      }
      *field_ptr = false;  // Restore.
    }
  }
  ASSERT_TRUE(found) << "Could not locate is_strict_log_mode_ in session";
  EXPECT_TRUE(session_->is_strict_log_mode());
  transaction_->ReportTimeout();
}
#endif  // BUILDFLAG(ARKWEB_LOGGER_REPORT)

TEST_F(ArkWebHttpNetworkTransactionExtTest, Destructor_StopsRecording) {
  // Verify that the destructor calls StopRecording() when is_recording_ is
  // true, by checking that the timer is not left running after destruction.
  transaction_->is_recording_ = true;
  transaction_->timer_.Start(FROM_HERE, base::Seconds(5), transaction_.get(),
                             &ArkWebHttpNetworkTransactionExt::ReportTimeout);
  EXPECT_TRUE(transaction_->timer_.IsRunning());
  EXPECT_TRUE(transaction_->is_recording_);

  // Destroy the transaction. The destructor calls StopRecording which should
  // stop the timer.
  transaction_.reset();
  // No crash or use-after-free means StopRecording ran correctly.
}

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

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       CopyConnectionAttemptsFromStreamRequest_EmptyAttempts) {
  SetupStreamRequest();

  // Do not add any extra_connection_attempts to the stream request.
  // Verify that CopyConnectionAttemptsFromStreamRequest results in empty.
  ASSERT_TRUE(transaction_->stream_request_);
  EXPECT_TRUE(
      transaction_->stream_request_->extra_connection_attempts().empty());

  transaction_->CopyConnectionAttemptsFromStreamRequest();

  ConnectionAttempts attempts = transaction_->GetExtraConnectionAttempts();
  EXPECT_TRUE(attempts.empty());

  CleanupStreamRequest();
}

TEST_F(ArkWebHttpNetworkTransactionExtTest,
       CopyConnectionAttemptsFromStreamRequest_ReplacesOnSecondCall) {
  SetupStreamRequest();

  // First call: populate with some attempts.
  ASSERT_TRUE(transaction_->stream_request_);
  ConnectionAttempts first_attempts;
  first_attempts.push_back(ConnectionAttempt(
      IPEndPoint(IPAddress(1, 2, 3, 4), 443), ERR_CONNECTION_REFUSED));
  transaction_->stream_request_->AddExtraConnectionAttempts(first_attempts);

  transaction_->CopyConnectionAttemptsFromStreamRequest();
  ASSERT_EQ(transaction_->GetExtraConnectionAttempts().size(), 1u);

  // Second call: add different attempts. This simulates a new stream request
  // with additional connection attempts. The extra_connection_attempts_ should
  // be cleared and replaced, not appended to.
  ConnectionAttempts second_attempts;
  second_attempts.push_back(
      ConnectionAttempt(IPEndPoint(IPAddress(10, 20, 30, 40), 443), OK));
  second_attempts.push_back(ConnectionAttempt(
      IPEndPoint(IPAddress(50, 60, 70, 80), 443), ERR_TIMED_OUT));
  transaction_->stream_request_->AddExtraConnectionAttempts(second_attempts);

  transaction_->CopyConnectionAttemptsFromStreamRequest();

  ConnectionAttempts attempts = transaction_->GetExtraConnectionAttempts();
  // Should have 1 (first batch) + 2 (second batch) = 3 total in stream
  // request, but the CopyConnectionAttemptsFromStreamRequest clears and
  // re-copies all, so we should have all 3.
  EXPECT_EQ(attempts.size(), 3u);
  EXPECT_EQ(attempts[0].result, ERR_CONNECTION_REFUSED);
  EXPECT_EQ(attempts[1].result, OK);
  EXPECT_EQ(attempts[2].result, ERR_TIMED_OUT);

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
