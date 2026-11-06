// Copyright 2026 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/cors/cors.h"
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
// clang-format off
#define private public
#define protected public
#include "media/base/ohos/ohos_media_source_download_manager.h"
#undef protected
#undef private
// clang-format on
 
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
constexpr int32_t kErrTimeout =
    -2;  // net::ERR_TIMED_OUT / ERR_CONNECTION_TIMED_OUT
constexpr int32_t kErrNetwork =
    -3;  // generic / connection / loader==null / http>=400
constexpr int32_t kErrAbort = -4;      // net::ERR_ABORTED
constexpr int32_t kErrForbidden = -5;  // HTTP 403
constexpr int32_t kErrNotFound = -6;   // HTTP 404
 
constexpr int64_t kUuid = 42;
constexpr const char* kHttpUrl = "https://media.test/segment.ts";
constexpr const char* kFtpUrl = "ftp://media.test/segment.ts";
 
// Builds a URLResponseHead carrying an HTTP status line (and optional extra
// header lines). |http_status| <= 0 yields a head with null headers, mirroring
// a transport error that produced no response head.
network::mojom::URLResponseHeadPtr BuildHead(
    int http_status,
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
    std::string raw = "HTTP/1.1 " + std::to_string(http_status) + " OK\n" +
                      extra_headers + "\n";
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
              (int64_t,
               (const std::map<std::string, std::string>&),
               const std::string&),
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
    site_for_cookies_ =
        net::SiteForCookies::FromUrl(GURL("https://example.test/"));
    top_frame_origin_ = url::Origin::Create(GURL("https://example.test/"));
    frame_origin_ = top_frame_origin_;
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
 
  // (Re)creates the manager with default cors_mode=kUnspecified and null
  // taint callback. Used to exercise the null-player / null-sink /
  // null-factory variants without disturbing the shared mocks.
  void NewManager(bool with_player, bool with_devtools) {
    NewManager(with_player, with_devtools, MediaCorsMode::kUnspecified,
               frame_origin_, /*on_taint_changed=*/{});
  }
 
  // Full-control overload: lets a test pick cors_mode, frame_origin, and a
  // taint-changed callback (fire-and-forget exercises).
  using TaintChangedCB =
      base::RepeatingCallback<void(bool tainted)>;
  void NewManager(bool with_player,
                  bool with_devtools,
                  MediaCorsMode cors_mode,
                  const url::Origin& frame_origin,
                  TaintChangedCB on_taint_changed) {
    manager_ = std::make_unique<MediaSourceDownloadManager>(
        with_player ? mock_player_.get() : nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        test_factory_.GetSafeWeakWrapper(),
        with_devtools ? mock_devtools_sink_.get() : nullptr, site_for_cookies_,
        top_frame_origin_, cors_mode, frame_origin,
        std::move(on_taint_changed));
  }
 
  // Presets a 2xx + body success response on the shared test factory.
  void AddOkResponse(const std::string& url,
                     const std::string& body,
                     int http_status = 200,
                     const std::string& extra_headers = "") {
    network::URLLoaderCompletionStatus status;  // default error_code = net::OK
    test_factory_.AddResponse(GURL(url), BuildHead(http_status, extra_headers),
                              body, status);
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
  url::Origin frame_origin_;
  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_taint_cb_;
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

TEST_F(MediaSourceDownloadManagerTest,
       StartDownload_NullFactory_CallsFinishLoadingError) {
  manager_->url_loader_factory_ = nullptr;
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNetwork));
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

// file:// local files are out of scope: this component only ever fetches
// online (HTTP/HTTPS) media URLs. ftp:// below still exercises the non-HTTP
// rejection / failure outcome.
TEST_F(MediaSourceDownloadManagerTest,
       StartDownload_FtpUrl_CallsFinishLoadingError) {
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

TEST_F(MediaSourceDownloadManagerTest,
       Download_Success_RespondsHeaderDataFinish) {
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

TEST_F(MediaSourceDownloadManagerTest,
       Download_Success_NoRangeWhenOffsetZeroAndLengthNeg) {
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

TEST_F(MediaSourceDownloadManagerTest,
       Download_Success_ExtraHeader_RefererSpecial) {
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

TEST_F(MediaSourceDownloadManagerTest,
       Download_Success_ExtraHeader_CookieSkipped) {
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

TEST_F(MediaSourceDownloadManagerTest,
       Download_Success_ExtraHeader_PlainHeader) {
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

TEST_F(MediaSourceDownloadManagerTest,
       Download_NetConnectionTimeout_MapsToTimeout) {
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

TEST_F(MediaSourceDownloadManagerTest,
       OnDownloadComplete_ResponseInfoNull_HttpStatusZero) {
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

TEST_F(MediaSourceDownloadManagerTest,
       OnDownloadComplete_ResponseHeadersDelivered) {
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

// =============================================================================
// (F) Additional branch coverage
// =============================================================================

TEST_F(MediaSourceDownloadManagerTest,
       OnDownloadComplete_FailureNullDevToolsSink_SilentlySwallowed) {
  // Null DevTools sink with a download failure: the devtools notifications are
  // silently skipped (the `if (devtools_sink_)` guards). No crash.
  NewManager(/*with_player=*/true, /*with_devtools=*/false);
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNetwork));
  AddResponseWith(kHttpUrl, net::ERR_FAILED, /*http_status=*/0);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

TEST_F(MediaSourceDownloadManagerTest,
       OnDownloadComplete_FailureNullPlayer_SilentlySwallowed) {
  // Null player with a download failure: the error is silently swallowed
  // (the `if (player_)` guard skips the callback). No crash.
  NewManager(/*with_player=*/false, /*with_devtools=*/true);
  EXPECT_CALL(*mock_devtools_sink_, NotifyLoadingComplete(_, _, _));
  AddResponseWith(kHttpUrl, net::ERR_FAILED, /*http_status=*/0);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
}

// =============================================================================
// (G) ResponseTaintsOrigin — direct static analysis matrix
// =============================================================================
// These tests call the static method directly to exhaust every branch of the
// CORS analysis logic without driving the full download pipeline.

namespace {
// Helpers for building HttpResponseHeaders with specific CORS headers.
scoped_refptr<net::HttpResponseHeaders> MakeHeaders(
    int status,
    const std::vector<std::string>& extra_headers) {
  std::string raw = "HTTP/1.1 " + std::to_string(status) + " OK\n";
  for (const auto& h : extra_headers) {
    raw += h + "\n";
  }
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw));
}
}  // namespace

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_SameOrigin_NeverTaints) {
  auto origin = url::Origin::Create(GURL("https://media.test"));
  auto headers = MakeHeaders(200, {});
  EXPECT_FALSE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUnspecified, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
  EXPECT_FALSE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kAnonymous, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
  EXPECT_FALSE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUseCredentials, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_CrossOrigin_Unspecified_Taints) {
  auto origin = url::Origin::Create(GURL("https://example.test"));
  auto headers = MakeHeaders(200, {});
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUnspecified, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_Anonymous_WithACAO_Wildcard_Clean) {
  auto origin = url::Origin::Create(GURL("https://example.test"));
  auto headers = MakeHeaders(200, {"Access-Control-Allow-Origin: *"});
  EXPECT_FALSE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kAnonymous, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_Anonymous_NoACAO_Taints) {
  auto origin = url::Origin::Create(GURL("https://example.test"));
  auto headers = MakeHeaders(200, {});
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kAnonymous, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_Anonymous_NullHeaders_Taints) {
  auto origin = url::Origin::Create(GURL("https://example.test"));
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kAnonymous, origin, GURL("https://media.test/seg.ts"),
      nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_Anonymous_ACAOReflectingOrigin_Clean) {
  auto origin = url::Origin::Create(GURL("https://example.test"));
  auto headers =
      MakeHeaders(200, {"Access-Control-Allow-Origin: https://example.test"});
  EXPECT_FALSE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kAnonymous, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_UseCredentials_WithACAOAndACAC_Clean) {
  auto origin = url::Origin::Create(GURL("https://example.test"));
  auto headers =
      MakeHeaders(200, {
                           "Access-Control-Allow-Origin: https://example.test",
                           "Access-Control-Allow-Credentials: true",
                       });
  EXPECT_FALSE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUseCredentials, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_UseCredentials_WildcardACAO_Taints) {
  // ACAO:* with credentials mode is NOT allowed per CORS spec.
  auto origin = url::Origin::Create(GURL("https://example.test"));
  auto headers = MakeHeaders(200, {
                                      "Access-Control-Allow-Origin: *",
                                      "Access-Control-Allow-Credentials: true",
                                  });
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUseCredentials, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_UseCredentials_NoACAC_Taints) {
  // ACAO reflecting origin but no ACAC header → taints for use-credentials.
  auto origin = url::Origin::Create(GURL("https://example.test"));
  auto headers =
      MakeHeaders(200, {"Access-Control-Allow-Origin: https://example.test"});
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUseCredentials, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_Anonymous_NoACAC_StillClean) {
  // anonymous mode: ACAC not required, ACAO reflecting origin is enough.
  auto origin = url::Origin::Create(GURL("https://example.test"));
  auto headers =
      MakeHeaders(200, {"Access-Control-Allow-Origin: https://example.test"});
  EXPECT_FALSE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kAnonymous, origin, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_OpaqueFrameOrigin_Anonymous_ACAONull_Clean) {
  // Opaque frame_origin serializes to "null" in CheckAccess, so ACAO: null
  // yields clean (aligns with sandboxed document handling).
  auto opaque = url::Origin();
  auto headers = MakeHeaders(200, {"Access-Control-Allow-Origin: null"});
  EXPECT_FALSE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kAnonymous, opaque, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_OpaqueFrameOrigin_Unspecified_Taints) {
  // Opaque initiator with http URL is always cross-origin for kUnspecified.
  auto opaque = url::Origin();
  auto headers = MakeHeaders(200, {});
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUnspecified, opaque, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_OpaqueFrameOrigin_Anonymous_NoACAO_Taints) {
  auto opaque = url::Origin();
  auto headers = MakeHeaders(200, {});
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kAnonymous, opaque, GURL("https://media.test/seg.ts"),
      headers.get()));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_NonHttpUrl_SameOrigin_Clean) {
  auto origin = url::Origin::Create(GURL("file:///path"));
  EXPECT_FALSE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUnspecified, origin, GURL("file:///path/seg.ts"),
      nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_NonHttpUrl_CrossOrigin_Taints) {
  // file:// URLs share the same origin in Chromium's url::Origin
  // (scheme-only), so use a different non-http scheme to test cross-origin.
  auto origin = url::Origin::Create(GURL("file:///path"));
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUnspecified, origin, GURL("data:text/plain,seg"),
      nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ResponseTaintsOrigin_NonHttpUrl_OpaqueOrigin_Taints) {
  auto opaque = url::Origin();
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUnspecified, opaque, GURL("file:///seg.ts"), nullptr));
}

TEST_F(MediaSourceDownloadManagerTest, ResponseTaintsOrigin_InvalidUrl_Taints) {
  auto origin = url::Origin::Create(GURL("https://example.test"));
  EXPECT_TRUE(MediaSourceDownloadManager::ResponseTaintsOrigin(
      MediaCorsMode::kUnspecified, origin, GURL(""), nullptr));
}

// =============================================================================
// (H) CORS analysis via OnDownloadComplete — integrated taint + delivery
// =============================================================================

TEST_F(MediaSourceDownloadManagerTest,
       CorsAnalysis_SameOriginResponse_DeliveredClean) {
  // frame_origin == response URL origin → clean → verdict change (true→false)
  // → null callback → immediate ack → delivery.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  EXPECT_CALL(*mock_player_, OnDataRespondHeader(kUuid, _, _));
  EXPECT_CALL(*mock_player_, OnDataRespondData(kUuid, 0, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "body");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  // Verdict should have flipped to clean (false).
  EXPECT_FALSE(manager_->any_response_tainted_);
  EXPECT_FALSE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       CorsAnalysis_CrossOriginUnspecified_TaintedNoVerdictSent) {
  // Cross-origin + kUnspecified → taint. any_response_tainted_ = true ==
  // last_sent_verdict_ = true → no verdict change → immediate delivery.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  EXPECT_CALL(*mock_player_, OnDataRespondData(kUuid, 0, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "body");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->any_response_tainted_);
  EXPECT_TRUE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       CorsAnalysis_Anonymous_WithACAO_DeliveredClean) {
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "body", 200,
                "Access-Control-Allow-Origin: https://example.test\r\n");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(manager_->any_response_tainted_);
}

TEST_F(MediaSourceDownloadManagerTest, CorsAnalysis_Anonymous_NoACAO_Tainted) {
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "body");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->any_response_tainted_);
}

TEST_F(MediaSourceDownloadManagerTest,
       CorsAnalysis_MonotonicLatching_CleanThenTainted) {
  // First read same-origin (clean), second read cross-origin (tainted).
  // Monotonic OR: once tainted, stays tainted.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(_, kErrOk)).Times(2);
  // First: same-origin → clean.
  AddOkResponse(kHttpUrl, "first");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(manager_->any_response_tainted_);
  // Second: cross-origin URL → tainted. Use a new uuid since Read() reuses
  // the URL stored in DownloadContext.
  constexpr int64_t kUuid2 = 43;
  const char* kCrossUrl = "https://other.test/segment.ts";
  AddOkResponse(kCrossUrl, "second");
  manager_->Open(kUuid2, kCrossUrl, {});
  manager_->Read(kUuid2, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->any_response_tainted_);
}

TEST_F(MediaSourceDownloadManagerTest,
       CorsAnalysis_MonotonicLatching_TaintedStaysTainted) {
  // First read cross-origin (tainted), second read same-origin (clean).
  // Monotonic OR: once tainted, cannot flip back to clean.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk)).Times(2);
  const char* kCrossUrl = "https://other.test/segment.ts";
  AddOkResponse(kCrossUrl, "first");
  manager_->Open(kUuid, kCrossUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(manager_->any_response_tainted_);
  // Second read: same-origin (clean), but aggregate stays tainted.
  AddOkResponse(kHttpUrl, "second");
  // Need to update the URL for the second read.
  manager_->downloads_[kUuid].url = kHttpUrl;
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->any_response_tainted_);
}

// =============================================================================
// (I) Fire-and-forget taint push — value-change dedup + delivery independence
// =============================================================================

TEST_F(MediaSourceDownloadManagerTest,
       FireAndForget_FirstClean_CallbackInvokedOnceAndDelivered) {
  // Same-origin response → verdict changes true→false → callback fired exactly
  // once. Data delivery happens immediately after (not gated on any ack).
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  EXPECT_CALL(mock_taint_cb_, Run(false)).Times(1);
  EXPECT_CALL(*mock_player_, OnDataRespondHeader(kUuid, _, _));
  EXPECT_CALL(*mock_player_, OnDataRespondData(kUuid, 0, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "body");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(manager_->any_response_tainted_);
  EXPECT_FALSE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       FireAndForget_AlwaysTainted_NoCallbackAndDelivered) {
  // Cross-origin + kUnspecified → taint. any_response_tainted_ = true ==
  // last_sent_verdict_ = true → no verdict change → no callback, but data
  // is still delivered.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://example.test")), mock_taint_cb_.Get());
  EXPECT_CALL(mock_taint_cb_, Run).Times(0);
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "body");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->any_response_tainted_);
  EXPECT_TRUE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       FireAndForget_MonotonicLatch_CleanThenTainted_TwoCallbacks) {
  // First read same-origin (clean) → callback(false).
  // Second read cross-origin (tainted) → callback(true).
  // Monotonic OR: aggregate flips clean→tainted, two callbacks total.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(_, kErrOk)).Times(2);
  // First: same-origin → clean → callback(false).
  EXPECT_CALL(mock_taint_cb_, Run(false));
  AddOkResponse(kHttpUrl, "first");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(manager_->any_response_tainted_);
  ASSERT_FALSE(manager_->last_sent_verdict_);
  // Second: cross-origin URL → tainted → callback(true).
  constexpr int64_t kUuid2 = 43;
  const char* kCrossUrl = "https://other.test/segment.ts";
  AddOkResponse(kCrossUrl, "second");
  EXPECT_CALL(mock_taint_cb_, Run(true));
  manager_->Open(kUuid2, kCrossUrl, {});
  manager_->Read(kUuid2, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->any_response_tainted_);
  EXPECT_TRUE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       FireAndForget_MonotonicLatch_TaintedStays_NoThirdCallback) {
  // First read cross-origin (tainted) → no callback (true == true).
  // Second read same-origin (clean) → aggregate stays tainted → no callback.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  EXPECT_CALL(mock_taint_cb_, Run).Times(0);
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk)).Times(2);
  const char* kCrossUrl = "https://other.test/segment.ts";
  AddOkResponse(kCrossUrl, "first");
  manager_->Open(kUuid, kCrossUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(manager_->any_response_tainted_);
  // Second read: same-origin (clean), aggregate stays tainted.
  AddOkResponse(kHttpUrl, "second");
  manager_->downloads_[kUuid].url = kHttpUrl;
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager_->any_response_tainted_);
  EXPECT_TRUE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       FireAndForget_NullCallback_NoCrashAndDelivered) {
  // Null on_taint_changed_ → verdict changes but callback is null → no crash,
  // data delivered normally.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  EXPECT_CALL(*mock_player_, OnDataRespondHeader(kUuid, _, _));
  EXPECT_CALL(*mock_player_, OnDataRespondData(kUuid, 0, _));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "body");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(manager_->any_response_tainted_);
  EXPECT_FALSE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       FireAndForget_FailedDownload_NoVerdictNoDelivery) {
  // Transport error → no taint analysis, no verdict, no delivery.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  EXPECT_CALL(mock_taint_cb_, Run).Times(0);
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrNetwork));
  AddResponseWith(kHttpUrl, net::ERR_FAILED, 0);
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  // last_sent_verdict_ stays at initial true (no change sent).
  EXPECT_TRUE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       FireAndForget_DestroyManagerAfterCallback_NoCrash) {
  // Destroy the manager after a callback has fired. The weak factory
  // should prevent any use-after-free.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  EXPECT_CALL(mock_taint_cb_, Run(false));
  EXPECT_CALL(*mock_player_, OnDataFinishLoading(kUuid, kErrOk));
  AddOkResponse(kHttpUrl, "body");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  // Destroy — no crash, no pending state to leak.
  manager_.reset();
  task_environment_.RunUntilIdle();
}

// =============================================================================
// (J) Request construction — cors_mode → Origin header + credentials_mode
// =============================================================================

TEST_F(MediaSourceDownloadManagerTest,
       RequestConstruction_Anonymous_SetsOriginAndSameOriginCreds) {
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "ok");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  // Origin header should be set to frame_origin.
  auto origin = captured.headers.GetHeader(net::HttpRequestHeaders::kOrigin);
  ASSERT_TRUE(origin.has_value());
  EXPECT_EQ(*origin, "https://example.test");
  // credentials_mode should be kSameOrigin for anonymous.
  EXPECT_EQ(captured.credentials_mode,
            network::mojom::CredentialsMode::kSameOrigin);
}

TEST_F(MediaSourceDownloadManagerTest,
       RequestConstruction_UseCredentials_SetsOriginAndIncludeCreds) {
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUseCredentials,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "ok");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  auto origin = captured.headers.GetHeader(net::HttpRequestHeaders::kOrigin);
  ASSERT_TRUE(origin.has_value());
  EXPECT_EQ(*origin, "https://example.test");
  // credentials_mode should be kInclude (default) for use-credentials.
  EXPECT_EQ(captured.credentials_mode,
            network::mojom::CredentialsMode::kInclude);
}

TEST_F(MediaSourceDownloadManagerTest,
       RequestConstruction_Unspecified_NoOriginHeader) {
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "ok");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  // No Origin header for kUnspecified.
  EXPECT_FALSE(captured.headers.HasHeader(net::HttpRequestHeaders::kOrigin));
  // credentials_mode stays kInclude (default).
  EXPECT_EQ(captured.credentials_mode,
            network::mojom::CredentialsMode::kInclude);
}

TEST_F(MediaSourceDownloadManagerTest,
       RequestConstruction_Anonymous_OpaqueFrameOrigin_NoOriginHeader) {
  // Opaque frame_origin → no Origin header (opaque can't serialize
  // meaningfully).
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin(),  // opaque
             /*on_taint_changed=*/{});
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "ok");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(captured.headers.HasHeader(net::HttpRequestHeaders::kOrigin));
}

TEST_F(MediaSourceDownloadManagerTest,
       RequestConstruction_PageOriginDoesNotOverrideAnalysisOrigin) {
  // Page-supplied Origin header in extra_header must NOT override the
  // analysis initiator (the CORS Origin is set AFTER the header copy loop).
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  network::ResourceRequest captured;
  EXPECT_CALL(*mock_devtools_sink_, NotifyRequestWillBeSent(_, _))
      .WillOnce(SaveArg<1>(&captured));
  AddOkResponse(kHttpUrl, "ok");
  std::map<std::string, std::string> headers = {
      {net::HttpRequestHeaders::kOrigin, "https://evil.test"},
  };
  manager_->Open(kUuid, kHttpUrl, headers);
  manager_->Read(kUuid, 0, -1);
  task_environment_.RunUntilIdle();
  auto origin = captured.headers.GetHeader(net::HttpRequestHeaders::kOrigin);
  ASSERT_TRUE(origin.has_value());
  // Must be the analysis frame_origin, not the page-supplied evil origin.
  EXPECT_EQ(*origin, "https://example.test");
}

// =============================================================================
// (K) ComputeResponseTaint — direct unit tests
// =============================================================================
// These tests construct a DownloadContext directly and call ComputeResponseTaint
// to exhaust branch coverage that is hard to drive through the full pipeline
// (redirect chains, null loader, cross-origin hop detection).

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_EmptyRedirectChain_SameOrigin_NullLoader_False) {
  // K1: no redirect chain, final URL same-origin, loader=nullptr → false.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  MediaSourceDownloadManager::DownloadContext ctx;
  ctx.url = kHttpUrl;
  ctx.redirect_chain = {};
  EXPECT_FALSE(manager_->ComputeResponseTaint(ctx, /*loader=*/nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_EmptyRedirectChain_CrossOrigin_Unspecified_True) {
  // K2: no redirect chain, final URL cross-origin, kUnspecified → true.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  MediaSourceDownloadManager::DownloadContext ctx;
  ctx.url = kHttpUrl;
  ctx.redirect_chain = {};
  EXPECT_TRUE(manager_->ComputeResponseTaint(ctx, /*loader=*/nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_CrossOrigin_Anonymous_WithACAO_False) {
  // K3: cross-origin final URL, kAnonymous, ACAO:* → false.
  // Drive a real download with ACAO:* header. After completion, the internal
  // ComputeResponseTaint call should have found the response clean (ACAO:*
  // allows cross-origin for kAnonymous). We verify via any_response_tainted_.
  // Note: we cannot call ComputeResponseTaint directly with the loader here
  // because SimpleURLLoader::ResponseInfo() is not populated until the
  // completion callback runs, and the loader is reset after completion.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  AddOkResponse(kHttpUrl, "body", 200, "Access-Control-Allow-Origin: *\r\n");
  manager_->Open(kUuid, kHttpUrl, {});
  manager_->Read(kUuid, 0, -1);
  // Loader exists synchronously after Read, before completion runs.
  ASSERT_TRUE(manager_->downloads_[kUuid].loader);
  task_environment_.RunUntilIdle();
  // After a successful download with ACAO:*, the aggregated taint should be
  // false (clean). Verify via the manager's internal state.
  EXPECT_FALSE(manager_->any_response_tainted_);
}

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_AllHopsSameOrigin_FinalSameOrigin_Unspecified_False) {
  // K4: redirect chain with all same-origin hops, final URL same-origin,
  // kUnspecified → false.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  MediaSourceDownloadManager::DownloadContext ctx;
  ctx.url = "https://media.test/seg.ts";
  ctx.redirect_chain = {
      {GURL("https://media.test/redirect1"), MakeHeaders(302, {})},
      {GURL("https://media.test/redirect2"), MakeHeaders(302, {})},
  };
  EXPECT_FALSE(manager_->ComputeResponseTaint(ctx, /*loader=*/nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_CrossOriginHop_Unspecified_True) {
  // K5: redirect chain with a cross-origin hop (first hop), kUnspecified →
  // true (cross_origin_hop detection fires on first hop).
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  MediaSourceDownloadManager::DownloadContext ctx;
  ctx.url = "https://media.test/seg.ts";
  ctx.redirect_chain = {
      {GURL("https://other.test/redirect"), MakeHeaders(302, {})},
  };
  EXPECT_TRUE(manager_->ComputeResponseTaint(ctx, /*loader=*/nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_CrossOriginRedirect_ResponseTaints_True) {
  // K6: kUnspecified, ctx.url and redirect hops all from a cross-origin
  // (other.test), frame_origin = media.test. The redirect hops are same-origin
  // with each other and with ctx.url, so cross_origin_hop = false. But
  // ResponseTaintsOrigin on each hop (cross-origin vs frame_origin) returns
  // true for kUnspecified → response_taints = true.
  // Note: the "all hops same-origin but final URL cross-origin via
  // loader->GetFinalURL()" branch cannot be tested without a real loader
  // that has a different GetFinalURL(), which SimpleURLLoader does not
  // expose for mocking.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  MediaSourceDownloadManager::DownloadContext ctx;
  ctx.url = "https://other.test/seg.ts";
  ctx.redirect_chain = {
      {GURL("https://other.test/redirect"), MakeHeaders(302, {})},
  };
  EXPECT_TRUE(manager_->ComputeResponseTaint(ctx, /*loader=*/nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_Anonymous_SkipsCrossOriginHopCheck_False) {
  // K7: kAnonymous mode skips the cross-origin hop check. Redirect hop is
  // cross-origin but has ACAO header → ResponseTaintsOrigin returns false.
  // Final URL (ctx.url, cross-origin) also needs ACAO. With loader=nullptr,
  // final_headers = nullptr → ResponseTaintsOrigin fail-closes → true.
  // So to get false, the final URL must be same-origin.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  MediaSourceDownloadManager::DownloadContext ctx;
  ctx.url = "https://media.test/seg.ts";
  ctx.redirect_chain = {
      {GURL("https://other.test/redirect"),
       MakeHeaders(302, {"Access-Control-Allow-Origin: https://media.test"})},
  };
  // Redirect hop: cross-origin, but ACAO reflects frame_origin → clean.
  // Final URL: same-origin → clean.
  // kAnonymous skips cross_origin_hop check entirely.
  EXPECT_FALSE(manager_->ComputeResponseTaint(ctx, /*loader=*/nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_Anonymous_CrossOriginRedirectNoACAO_True) {
  // K8: kAnonymous, redirect hop cross-origin with no ACAO →
  // ResponseTaintsOrigin returns true → response_taints = true.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  MediaSourceDownloadManager::DownloadContext ctx;
  ctx.url = "https://media.test/seg.ts";
  ctx.redirect_chain = {
      {GURL("https://other.test/redirect"), MakeHeaders(302, {})},
  };
  // Redirect hop: cross-origin, no ACAO → taints.
  // Final URL: same-origin → clean. But response_taints already true.
  EXPECT_TRUE(manager_->ComputeResponseTaint(ctx, /*loader=*/nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_Anonymous_CrossOriginNullHeaders_True) {
  // K9: kAnonymous, no redirect chain, final URL cross-origin, loader=nullptr
  // → final_headers = nullptr → ResponseTaintsOrigin fail-closes → true.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin::Create(GURL("https://example.test")),
             /*on_taint_changed=*/{});
  MediaSourceDownloadManager::DownloadContext ctx;
  ctx.url = kHttpUrl;
  ctx.redirect_chain = {};
  EXPECT_TRUE(manager_->ComputeResponseTaint(ctx, /*loader=*/nullptr));
}

TEST_F(MediaSourceDownloadManagerTest,
       ComputeResponseTaint_Anonymous_AllHopsSameOrigin_FinalSameOrigin_False) {
  // K10: kAnonymous, all hops same-origin, final URL same-origin → false.
  // This tests the kAnonymous path where cross_origin_hop check is skipped
  // and all ResponseTaintsOrigin calls return false (same-origin is always
  // clean regardless of headers).
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kAnonymous,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  MediaSourceDownloadManager::DownloadContext ctx;
  ctx.url = "https://media.test/seg.ts";
  ctx.redirect_chain = {
      {GURL("https://media.test/redirect1"), MakeHeaders(302, {})},
      {GURL("https://media.test/redirect2"), MakeHeaders(302, {})},
  };
  EXPECT_FALSE(manager_->ComputeResponseTaint(ctx, /*loader=*/nullptr));
}

// =============================================================================
// (L) MaybePushTaint — direct unit tests
// =============================================================================
// These tests call MaybePushTaint directly and inspect member state, covering
// the monotonic OR aggregation, dedup via last_sent_verdict_, and null
// callback safety.

TEST_F(MediaSourceDownloadManagerTest,
       MaybePushTaint_InitialTrue_InputFalse_StaysTrue_NoCallback) {
  // L1: initial any_response_tainted_=true, last_sent_verdict_=true.
  // MaybePushTaint(false) → true|=false → true. true==true → no push.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  // Default state: any_response_tainted_=false, last_sent_verdict_=true.
  // Set to match the "initial true" scenario described in the spec.
  manager_->any_response_tainted_ = true;
  manager_->last_sent_verdict_ = true;
  EXPECT_CALL(mock_taint_cb_, Run).Times(0);
  manager_->MaybePushTaint(false);
  EXPECT_TRUE(manager_->any_response_tainted_);
  EXPECT_TRUE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       MaybePushTaint_InitialTrue_InputTrue_StaysTrue_NoCallback) {
  // L2: initial true, input true → no change, no callback.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  manager_->any_response_tainted_ = true;
  manager_->last_sent_verdict_ = true;
  EXPECT_CALL(mock_taint_cb_, Run).Times(0);
  manager_->MaybePushTaint(true);
  EXPECT_TRUE(manager_->any_response_tainted_);
  EXPECT_TRUE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       MaybePushTaint_ManualFalse_InputFalse_FlipsToFalse_CallbackOnce) {
  // L3: manually set any_response_tainted_=false, last_sent_verdict_=true.
  // MaybePushTaint(false) → false|=false → false. false!=true → push false.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  manager_->any_response_tainted_ = false;
  manager_->last_sent_verdict_ = true;
  EXPECT_CALL(mock_taint_cb_, Run(false)).Times(1);
  manager_->MaybePushTaint(false);
  EXPECT_FALSE(manager_->any_response_tainted_);
  EXPECT_FALSE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       MaybePushTaint_ManualFalse_InputTrue_FlipsToTrue_CallbackOnce) {
  // L4: manually set any_response_tainted_=false, last_sent_verdict_=false.
  // MaybePushTaint(true) → false|=true → true. true!=false → push true.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  manager_->any_response_tainted_ = false;
  manager_->last_sent_verdict_ = false;
  EXPECT_CALL(mock_taint_cb_, Run(true)).Times(1);
  manager_->MaybePushTaint(true);
  EXPECT_TRUE(manager_->any_response_tainted_);
  EXPECT_TRUE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       MaybePushTaint_NullCallback_NoCrash) {
  // L5: null callback → verdict changes but callback is null, no crash.
  NewManager(/*with_player=*/true, /*with_devtools=*/true,
             MediaCorsMode::kUnspecified,
             url::Origin::Create(GURL("https://media.test")),
             /*on_taint_changed=*/{});
  manager_->any_response_tainted_ = false;
  manager_->last_sent_verdict_ = true;
  // Should not crash even though on_taint_changed_ is null.
  manager_->MaybePushTaint(false);
  EXPECT_FALSE(manager_->any_response_tainted_);
  EXPECT_FALSE(manager_->last_sent_verdict_);
}

TEST_F(MediaSourceDownloadManagerTest,
       MaybePushTaint_Monotonic_TrueThenFalse_StaysTrue_NoSecondCallback) {
  // L6: monotonicity — MaybePushTaint(true) flips to true and pushes.
  // Subsequent MaybePushTaint(false) → true|=false → true. true==true →
  // no second push.
  NewManager(
      /*with_player=*/true, /*with_devtools=*/true, MediaCorsMode::kUnspecified,
      url::Origin::Create(GURL("https://media.test")), mock_taint_cb_.Get());
  manager_->any_response_tainted_ = false;
  manager_->last_sent_verdict_ = false;
  EXPECT_CALL(mock_taint_cb_, Run(true)).Times(1);
  manager_->MaybePushTaint(true);
  EXPECT_TRUE(manager_->any_response_tainted_);
  EXPECT_TRUE(manager_->last_sent_verdict_);
  // Second call with false — no additional callback.
  EXPECT_CALL(mock_taint_cb_, Run).Times(0);
  manager_->MaybePushTaint(false);
  EXPECT_TRUE(manager_->any_response_tainted_);
  EXPECT_TRUE(manager_->last_sent_verdict_);
}