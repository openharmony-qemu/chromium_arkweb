// Copyright 2026 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

// White-box access to private members (DownloadContext, StartDownload,
// OnDownloadComplete, downloads_ map) for branch coverage that cannot be
// driven purely through the public API (e.g. direct OnDownloadComplete on an
// unknown uuid, loader==null state).
#define private public
#define protected public
#include "media/base/ohos/ohos_media_source_download_manager.h"
#undef protected
#undef private

using namespace media;
using namespace testing;
using OHOS::NWeb::IConsumerSurfaceAdapter;
using OHOS::NWeb::PlaybackRateMode;
using OHOS::NWeb::PlayerAdapter;
using OHOS::NWeb::PlayerCallbackAdapter;
using OHOS::NWeb::PlayerSeekMode;

namespace {

// Mirror of the anonymous-namespace error codes in
// ohos_media_source_download_manager.cc (LoadingRequestError as seen by the
// HiStreamer player framework). Kept as plain ints to assert the wire contract.
constexpr int32_t kErrOk = 0;
constexpr int32_t kErrTimeout = -2;    // net::ERR_TIMED_OUT / ERR_CONNECTION_TIMED_OUT
constexpr int32_t kErrNetwork = -3;    // generic / connection / loader==null / http>=400
constexpr int32_t kErrAbort = -4;      // net::ERR_ABORTED
constexpr int32_t kErrForbidden = -5;  // HTTP 403
constexpr int32_t kErrNotFound = -6;   // HTTP 404

constexpr int64_t kUuid = 42;
constexpr const char* kHttpUrl = "https://media.test/segment.ts";
constexpr const char* kFtpUrl = "ftp://media.test/segment.ts";

// Builds a URLResponseHead carrying an HTTP status line (and optional extra
// header lines). |http_status| <= 0 yields a head with null headers, mirroring
// a transport error that produced no response head.
network::mojom::URLResponseHeadPtr BuildHead(int http_status,
                                             const std::string& extra_headers = "") {
  auto head = network::mojom::URLResponseHead::New();
  if (http_status > 0) {
    // Assemble headers via the canonical path used by CreateURLResponseHead()
    // and TestURLLoaderFactoryTest: build a \n-separated block and normalize
    // with net::HttpUtil::AssembleRawHeaders() (which also accepts \r\n line
    // endings from callers). Headers produced this way reliably survive
    // TestURLLoaderFactory's mojom Clone()/serialization round-trips into
    // SimpleURLLoader::ResponseInfo()->headers; feeding a \r\n raw string
    // straight to the HttpResponseHeaders constructor was observed on
    // real-device builds to yield an empty header map in OnDataRespondHeader.
    std::string raw =
        "HTTP/1.1 " + std::to_string(http_status) + " OK\n" + extra_headers + "\n";
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(raw));
  }
  return head;
}

}  // namespace

// Full PlayerAdapter mock. The three OnData* callbacks are non-pure virtuals
// (default empty body) but are exactly what MediaSourceDownloadManager pushes
// back to the player, so they are mocked here.
class MockPlayerAdapter : public PlayerAdapter {
 public:
  MOCK_METHOD(int32_t,
              SetPlayerCallback,
              (std::shared_ptr<PlayerCallbackAdapter>),
              (override));
  MOCK_METHOD(int32_t, SetSource, (const std::string&), (override));
  MOCK_METHOD(int32_t, SetSource, (int32_t, int64_t, int64_t), (override));
  MOCK_METHOD(int32_t,
              SetVideoSurface,
              (std::shared_ptr<IConsumerSurfaceAdapter>),
              (override));
  MOCK_METHOD(int32_t, SetVolume, (float, float), (override));
  MOCK_METHOD(int32_t, Seek, (int32_t, PlayerSeekMode), (override));
  MOCK_METHOD(int32_t, Play, (), (override));
  MOCK_METHOD(int32_t, Pause, (), (override));
  MOCK_METHOD(int32_t, PrepareAsync, (), (override));
  MOCK_METHOD(int32_t, GetCurrentTime, (int32_t&), (override));
  MOCK_METHOD(int32_t, GetDuration, (int32_t&), (override));
  MOCK_METHOD(int32_t, SetPlaybackSpeed, (PlaybackRateMode), (override));
  MOCK_METHOD(int32_t, SetVideoSurfaceNew, (void*), (override));
  MOCK_METHOD(void,
              OnDataRespondHeader,
              (int64_t, (const std::map<std::string, std::string>&), const std::string&),
              (override));
  MOCK_METHOD(void,
              OnDataRespondData,
              (int64_t, int64_t, const std::vector<uint8_t>&),
              (override));
  MOCK_METHOD(void, OnDataFinishLoading, (int64_t, int32_t), (override));
};

class MockDevToolsNetworkEventSink : public DevToolsNetworkEventSink {
 public:
  MOCK_METHOD(void,
              NotifyRequestWillBeSent,
              (const std::string&, const network::ResourceRequest&),
              (override));
  MOCK_METHOD(void,
              NotifyResponseReceived,
              (const std::string&,
               const GURL&,
               const network::mojom::URLResponseHead&),
              (override));
  MOCK_METHOD(void,
              NotifyLoadingComplete,
              (const std::string&, int, int64_t),
              (override));
};

class MediaSourceDownloadManagerTest : public testing::Test {
 protected:
  // The SingleThreadTaskEnvironment must be declared before |manager_| so that
  // GetCurrentDefault() is valid when the manager is constructed in SetUp().
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  void SetUp() override {
    site_for_cookies_ = net::SiteForCookies::FromUrl(GURL("https://example.test/"));
    top_frame_origin_ = url::Origin::Create(GURL("https://example.test/"));
    mock_player_ = std::make_unique<testing::NiceMock<MockPlayerAdapter>>();
    mock_devtools_sink_ =
        std::make_unique<testing::NiceMock<MockDevToolsNetworkEventSink>>();
    // In this `media_unittests` build, mocks that have had expectations set on
    // them (EXPECT_CALL) are not removed from gmock's leak-tracker on
    // destruction, so gmock's atexit leak check reports a phantom leak even
    // though TearDown() below destroys them. AllowLeak declares these
    // fixture-scoped mocks as intentionally not leak-checked -- they ARE
    // destroyed in TearDown(); this only silences the false positive.
    testing::Mock::AllowLeak(mock_player_.get());
    testing::Mock::AllowLeak(mock_devtools_sink_.get());
    NewManager(/*with_player=*/true, /*with_devtools=*/true);
  }

  // The leak checker of the `media_unittests` driver runs BEFORE fixture member
  // destruction, so a mock still alive at that point is reported as a leak.
  // Destroy the manager (which holds raw pointers into the mocks) first, pump
  // pending tasks, then drop the mocks explicitly -- mirroring
  // OHOSMediaPlayerBridgeTests::TearDown.
  void TearDown() override {
    manager_.reset();
    task_environment_.RunUntilIdle();
    mock_devtools_sink_.reset();
    mock_player_.reset();
  }

  // (Re)creates the manager. Used to exercise the null-player / null-sink /
  // null-factory variants without disturbing the shared mocks.
  void NewManager(bool with_player, bool with_devtools) {
    manager_ = std::make_unique<MediaSourceDownloadManager>(
        with_player ? mock_player_.get() : nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        test_factory_.GetSafeWeakWrapper(),
        with_devtools ? mock_devtools_sink_.get() : nullptr,
        site_for_cookies_,
        top_frame_origin_);
  }

  // Presets a 2xx + body success response on the shared test factory.
  void AddOkResponse(const std::string& url,
                     const std::string& body,
                     int http_status = 200,
                     const std::string& extra_headers = "") {
    network::URLLoaderCompletionStatus status;  // default error_code = net::OK
    test_factory_.AddResponse(GURL(url), BuildHead(http_status, extra_headers), body,
                              status);
  }

  // Presets a response that completes with a transport/HTTP-level state the
  // production code treats as failure. |net_error| != OK models a transport
  // error (body delivered as null); an HTTP error is modelled by |http_status|
  // with net_error == OK.
  void AddResponseWith(const std::string& url,
                       int net_error,
                       int http_status,
                       const std::string& body = "") {
    network::URLLoaderCompletionStatus status;
    status.error_code = net_error;
    test_factory_.AddResponse(GURL(url), BuildHead(http_status), body, status);
  }

  std::unique_ptr<testing::NiceMock<MockPlayerAdapter>> mock_player_;
  std::unique_ptr<testing::NiceMock<MockDevToolsNetworkEventSink>>
      mock_devtools_sink_;
  network::TestURLLoaderFactory test_factory_;
  net::SiteForCookies site_for_cookies_;
  url::Origin top_frame_origin_;
  std::unique_ptr<MediaSourceDownloadManager> manager_;
};

// =============================================================================
// (A) Lifecycle & state machine
// =============================================================================

TEST_F(MediaSourceDownloadManagerTest, Constructor_AndGetWeakPtr) {
  ASSERT_TRUE(manager_);
  auto weak = manager_->GetWeakPtr();
  EXPECT_TRUE(weak);
  EXPECT_EQ(weak.get(), manager_.get());
}

TEST_F(MediaSourceDownloadManagerTest, Destructor_InvalidatesWeakPtr) {
  auto weak = manager_->GetWeakPtr();
  ASSERT_TRUE(weak);
  manager_.reset();
  EXPECT_FALSE(weak);
}

TEST_F(MediaSourceDownloadManagerTest, Open_InsertsContext) {
  std::map<std::string, std::string> headers = {{"X-A", "1"}};
  manager_->Open(kUuid, kHttpUrl, headers);
  ASSERT_EQ(manager_->downloads_.size(), 1u);
  const auto& ctx = manager_->downloads_[kUuid];
  EXPECT_EQ(ctx.url, kHttpUrl);
  EXPECT_EQ(ctx.header, headers);
  EXPECT_TRUE(ctx.is_active);
  EXPECT_FALSE(ctx.loader);
  EXPECT_FALSE(ctx.header_sent);
}

TEST_F(MediaSourceDownloadManagerTest, Read_UnknownUuid_Noop) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading).Times(0);
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->downloads_.empty());
}

TEST_F(MediaSourceDownloadManagerTest, Read_InactiveUuid_Noop) {
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->downloads_[kUuid].is_active = false;
  EXPECT_CALL(*mock_player_, OnDataFinishLoading).Times(0);
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent).Times(0);
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(manager_->downloads_[kUuid].loader);
}

TEST_F(MediaSourceDownloadManagerTest, Read_ActiveUuid_TriggersDownload) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "ok");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->downloads_[kUuid].header_sent);
}

TEST_F(MediaSourceDownloadManagerTest, Close_UnknownUuid_Noop) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading).Times(0);
  manager_->Close(kUuid);
  EXPECT_TRUE(manager_->downloads_.empty());
}

TEST_F(MediaSourceDownloadManagerTest, Close_ActiveUuid_ResetsLoaderAndErases) {
  AddOkResponse(kHttpUrl, "ok");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  // A loader is created synchronously during Read, before completion runs.
  ASSERT_TRUE(manager_->downloads_[kUuid].loader);
  manager_->Close(kUuid);
  EXPECT_EQ(manager_->downloads_.count(kUuid), 0u);
}

TEST_F(MediaSourceDownloadManagerTest, Close_AfterComplete) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "ok");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  // Loader was reset inside OnDownloadComplete after a successful read.
  ASSERT_FALSE(manager_->downloads_[kUuid].loader);
  manager_->Close(kUuid);  // Must not crash; just erases.
  EXPECT_EQ(manager_->downloads_.count(kUuid), 0u);
}

// =============================================================================
// (B) StartDownload pre-flight validation
// =============================================================================

TEST_F(MediaSourceDownloadManagerTest, StartDownload_NullFactory_CallsFinishLoadingError) {
  manager_->url_loader_factory_ = nullptr;
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNetwork));
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

// file:// local files are out of scope: this component only ever fetches
// online (HTTP/HTTPS) media URLs. ftp:// below still exercises the non-HTTP
// rejection / failure outcome.
TEST_F(MediaSourceDownloadManagerTest, StartDownload_FtpUrl_CallsFinishLoadingError) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNetwork));
  manager_->Open(kUuid, kFtpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, StartDownload_NonActiveUuid_Noop) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading).Times(0);
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent).Times(0);
  // Private entry: uuid not present in the map at all.
  manager_->StartDownload(kUuid, kHttpUrl, 0, -1, {});
}

TEST_F(MediaSourceDownloadManagerTest, StartDownload_InactiveUuidInMap_Noop) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading).Times(0);
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent).Times(0);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->downloads_[kUuid].is_active = false;
  manager_->StartDownload(kUuid, kHttpUrl, 0, -1, {});
}

// =============================================================================
// (C) Success path (driven through TestURLLoaderFactory)
// =============================================================================

TEST_F(MediaSourceDownloadManagerTest, Download_Success_RespondsHeaderDataFinish) {
  const std::string kBody = "segment-bytes";
  EXPECT_CALL(*mock_player_, OnDataRespondHeader(kUuid, _, _));
  EXPECT_CALL(*mock_player_, OnDataRespondData(kUuid, 0, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  EXPECT_CALL(*mock_devtools_sink_, NotifyResponseReceived(_, _, _));
  EXPECT_CALL(*mock_devtools_sink_, NotifyLoadingComplete(_, net::OK, _));
  AddOkResponse(kHttpUrl, kBody);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_WithRange) {
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "rng");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, /*offset=*/10, /*length=*/20);
  task_environment_.RunUntilIdle();
  auto range = captured.headers.GetHeader(net::HttpRequestHeaders::kRange);
  ASSERT_TRUE(range.has_value());
  EXPECT_EQ(*range, "bytes=10-29");
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_OpenEndedRange) {
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "rng");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, /*offset=*/10, /*length=*/-1);
  task_environment_.RunUntilIdle();
  auto range = captured.headers.GetHeader(net::HttpRequestHeaders::kRange);
  ASSERT_TRUE(range.has_value());
  EXPECT_EQ(*range, "bytes=10-");
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_NoRangeWhenOffsetZeroAndLengthNeg) {
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "full");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, /*offset=*/0, /*length=*/-1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(captured.headers.HasHeader(net::HttpRequestHeaders::kRange));
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_HeaderSentOnlyOnce) {
  EXPECT_CALL(*mock_player_, OnDataRespondHeader(kUuid, _, _)).Times(1);
  EXPECT_CALL(*mock_player_, OnDataRespondData).Times(2);
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk)).Times(2);
  AddOkResponse(kHttpUrl, "first");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(manager_->downloads_[kUuid].header_sent);
  // Second read on the same handle: header must NOT be resent.
  AddOkResponse(kHttpUrl, "second");
  manager_->Read(kUuid, 100, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_ExtraHeader_RefererSpecial) {
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "ok");
  std::map<std::string, std::string> headers = {
      {net::HttpRequestHeaders::kReferer, "https://referer.test/"},
  };
  manager_->Open(kUuid, kHttpUrl, headers);
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  // Referer is routed to request->referrer, never copied as a plain header.
  EXPECT_EQ(captured.referrer, GURL("https://referer.test/"));
  EXPECT_FALSE(captured.headers.HasHeader(net::HttpRequestHeaders::kReferer));
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_ExtraHeader_CookieSkipped) {
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "ok");
  std::map<std::string, std::string> headers = {
      {net::HttpRequestHeaders::kCookie, "id=abc; token=xyz"},
      {"X-Keep", "yes"},
  };
  manager_->Open(kUuid, kHttpUrl, headers);
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  // Cookie header is deliberately dropped (page cookies come via the cookie
  // context, not a literal header) so it never reaches the request.
  EXPECT_FALSE(captured.headers.HasHeader(net::HttpRequestHeaders::kCookie));
  auto keep = captured.headers.GetHeader("X-Keep");
  ASSERT_TRUE(keep.has_value());
  EXPECT_EQ(*keep, "yes");
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_ExtraHeader_PlainHeader) {
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "ok");
  std::map<std::string, std::string> headers = {
      {"X-Custom", "value1"},
      {"User-Agent", "TestUA/1.0"},
  };
  manager_->Open(kUuid, kHttpUrl, headers);
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  auto custom = captured.headers.GetHeader("X-Custom");
  ASSERT_TRUE(custom.has_value());
  EXPECT_EQ(*custom, "value1");
  auto ua = captured.headers.GetHeader("User-Agent");
  ASSERT_TRUE(ua.has_value());
  EXPECT_EQ(*ua, "TestUA/1.0");
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_NullPlayer) {
  // No player callbacks; DevTools still emits, no crash.
  EXPECT_CALL(*mock_devtools_sink_, NotifyResponseReceived(_, _, _));
  EXPECT_CALL(*mock_devtools_sink_, NotifyLoadingComplete(_, net::OK, _));
  NewManager(/*with_player=*/false, /*with_devtools=*/true);
  AddOkResponse(kHttpUrl, "ok");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_NullDevToolsSink) {
  // Player still receives data; DevTools is a no-op, no crash.
  EXPECT_CALL(*mock_player_, OnDataRespondHeader(kUuid, _, _));
  EXPECT_CALL(*mock_player_, OnDataRespondData(kUuid, 0, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  NewManager(/*with_player=*/true, /*with_devtools=*/false);
  AddOkResponse(kHttpUrl, "ok");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_Success_EmptyBody) {
  // Empty (but non-null) body is still success: header + data(empty) + finish.
  EXPECT_CALL(*mock_player_, OnDataRespondHeader(kUuid, _, _));
  EXPECT_CALL(*mock_player_, OnDataRespondData(kUuid, 0, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

// =============================================================================
// (D) Failure path (covers MapNetErrorToLoadingRequestError branches)
// =============================================================================

TEST_F(MediaSourceDownloadManagerTest, Download_Http403_MapsToForbidden) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrForbidden));
  AddResponseWith(kHttpUrl, net::OK, 403, "forbidden-body");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_Http404_MapsToNotFound) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNotFound));
  AddResponseWith(kHttpUrl, net::OK, 404, "missing");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_Http500_MapsToNetwork) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNetwork));
  AddResponseWith(kHttpUrl, net::OK, 500, "err");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_NetTimeout_MapsToTimeout) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrTimeout));
  AddResponseWith(kHttpUrl, net::ERR_TIMED_OUT, /*http_status=*/0);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_NetConnectionTimeout_MapsToTimeout) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrTimeout));
  AddResponseWith(kHttpUrl, net::ERR_CONNECTION_TIMED_OUT, /*http_status=*/0);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_NetAborted_MapsToAbort) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrAbort));
  AddResponseWith(kHttpUrl, net::ERR_ABORTED, /*http_status=*/0);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_NetGeneric_MapsToNetwork) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNetwork));
  AddResponseWith(kHttpUrl, net::ERR_FAILED, /*http_status=*/0);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest, Download_DevToolsNotifiedOnError) {
  // HTTP error: a response head exists (SetAllowHttpErrorResults) so
  // NotifyResponseReceived fires alongside NotifyLoadingComplete.
  EXPECT_CALL(*mock_devtools_sink_, NotifyResponseReceived(_, _, _));
  EXPECT_CALL(*mock_devtools_sink_, NotifyLoadingComplete(_, net::OK, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrForbidden));
  AddResponseWith(kHttpUrl, net::OK, 403, "b");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  // Loader is reset after the failure branch.
  EXPECT_FALSE(manager_->downloads_[kUuid].loader);
}

TEST_F(MediaSourceDownloadManagerTest, Download_LoaderNull_MapsToNetwork) {
  // Drive OnDownloadComplete directly with no loader set: NetError falls back
  // to ERR_FAILED and http_status stays 0 -> kErrNetwork.
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNetwork));
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->OnDownloadComplete(kUuid, nullptr);
}

// =============================================================================
// (E) OnDownloadComplete boundaries
// =============================================================================

TEST_F(MediaSourceDownloadManagerTest, OnDownloadComplete_UnknownUuid_Noop) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading).Times(0);
  EXPECT_CALL(*mock_devtools_sink_, NotifyLoadingComplete).Times(0);
  manager_->OnDownloadComplete(kUuid, std::make_unique<std::string>("late"));
}

TEST_F(MediaSourceDownloadManagerTest, OnDownloadComplete_InactiveUuid_Noop) {
  EXPECT_CALL(*mock_player_, OnDataFinishLoading).Times(0);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->downloads_[kUuid].is_active = false;
  manager_->OnDownloadComplete(kUuid, std::make_unique<std::string>("late"));
}

TEST_F(MediaSourceDownloadManagerTest, OnDownloadComplete_ResponseInfoNull_HttpStatusZero) {
  // Loader absent: ResponseInfo()/headers cannot be read, so http_status stays
  // 0 and NetError() defaults to net::ERR_FAILED. The failure-branch guard
  // (`net_error != net::OK`) fires even though a body was delivered.
  EXPECT_CALL(*mock_player_, OnDataRespondHeader).Times(0);
  EXPECT_CALL(*mock_player_, OnDataRespondData).Times(0);
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNetwork));
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->downloads_[kUuid].pending_offset = 0;
  manager_->OnDownloadComplete(kUuid, std::make_unique<std::string>("body"));
}

TEST_F(MediaSourceDownloadManagerTest, OnDownloadComplete_HeadersNotPresent) {
  // A successful body whose head has no headers: the response-header map
  // delivered to the player is empty, and the header_sent latch still flips.
  EXPECT_CALL(*mock_player_, OnDataRespondHeader(kUuid, IsEmpty(), _));
  EXPECT_CALL(*mock_player_, OnDataRespondData(kUuid, 5, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddResponseWith(kHttpUrl, net::OK, /*http_status=*/0, "body");
  // http_status==0 + net::OK + non-empty body is a success with empty headers.
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 5, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->downloads_[kUuid].header_sent);
}

TEST_F(MediaSourceDownloadManagerTest, OnDownloadComplete_ResponseHeadersDelivered) {
  // Deterministic counterpart of the empty-headers case: a 200 response with
  // real header lines must surface them verbatim via OnDataRespondHeader
  // (exercises HttpResponseHeaders::EnumerateHeaderLines).
  std::map<std::string, std::string> captured;
  EXPECT_CALL(*mock_player_, OnDataRespondHeader(kUuid, _, _))
      .WillOnce(SaveArg<1>(&captured));
  EXPECT_CALL(*mock_player_, OnDataRespondData(kUuid, 0, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "body", 200,
                "Content-Type: video/mp2t\r\nX-Id: 7\r\n");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(captured["Content-Type"], "video/mp2t");
  EXPECT_EQ(captured["X-Id"], "7");
}
