/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "ohos_sdk/openharmony/native/llvm/bin/../include/libcxx-ohos/include/c++/v1/__ranges/lazy_split_view.h"
#define private public
#include "arkweb/chromium_ext/third_party/blink/renderer/platform/media/video_common_data_provider.h"
#undef private

#include "build/build_config.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/media_log.h"
#include "media/base/seekable_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/media/url_index.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/media/testing/mock_resource_fetch_context.h"
#include "third_party/blink/renderer/platform/media/testing/mock_web_associated_url_loader.h"

namespace blink {

class WebAssociatedURLLoader;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Truly;

const int kDataSize = 1024;
const int kHttpOK_VideoOpt = 200;
const int kHttpPartialContent_VideoOpt = 206;

const char kHttpUrl[] = "http://foo.bar.com";
const char kHttpRedirect[] = "http://test/ing";
const char kEtag[] = "\"arglebargle glopy-glyf?\"";
const std::string kVideoId = "html5_api";

// Predicate that checks the Accept-Encoding request header.
static bool CorrectAcceptEncoding(const WebURLRequest& request) {
  std::string value = request
                          .HttpHeaderField(WebString::FromUTF8(
                              net::HttpRequestHeaders::kAcceptEncoding))
                          .Utf8();
  return (base::Contains(value, "identity;q=1")) &&
         (base::Contains(value, "*;q=0"));
}

static bool AcceptURLLoadClient(base::WeakPtr<WebAssociatedURLLoaderClient> client) {
    return client != nullptr;
}

class MockVideoCommonDataProvider : public VideoCommonDataProvider {
public:
  MockVideoCommonDataProvider() = default;

  MockVideoCommonDataProvider(UrlData* url_data,
                              MultiBufferBlockId pos,
                              std::string id,
                              bool is_client_audio_element,
                              scoped_refptr<base::SingleThreadTaskRunner> task_runner) 
    : VideoCommonDataProvider(url_data, pos, id, is_client_audio_element, task_runner) {}

  MockVideoCommonDataProvider(const MockVideoCommonDataProvider&) = default;
  MockVideoCommonDataProvider& operator=(const MockVideoCommonDataProvider&) =
      default;
  ~MockVideoCommonDataProvider() override = default;

  MOCK_CONST_METHOD0(Available, bool());
  MOCK_CONST_METHOD0(AvailableBytes, int64_t());
  MOCK_METHOD0(Read, scoped_refptr<media::DataBuffer>());
  MOCK_METHOD1(SetDeferred, void(bool));
  MOCK_METHOD0(Start, void());
};

class VideoCommonDataProviderTest : public ::testing::Test {
public:
  VideoCommonDataProviderTest() {
    ON_CALL(fetch_context_, CreateUrlLoader(_))
        .WillByDefault(Invoke(
            this, &VideoCommonDataProviderTest::CreateUrlLoader));
  }

  void Initialize(const char* url, MultiBufferBlockId pos,
    UrlData::CorsMode corsMode = UrlData::CORS_UNSPECIFIED) {
    url_ = KURL(url);
    url_data_ = url_index_.GetByUrl(url_, UrlData::CORS_UNSPECIFIED, UrlData::kNormal);
    url_data_->set_etag(kEtag);

    std::string id = "html5_api";
    bool is_client_audio_element = false;
    auto loader = std::make_unique<MockVideoCommonDataProvider>(url_data_.get(), pos, id, 
        is_client_audio_element, task_environment_.GetMainThreadTaskRunner());
    provider_ = loader.get();
    url_data_->multibuffer()->AddProvider(std::move(loader));
  }

  void SetUrlData(UrlData* data) {
    if (provider_) {
      provider_->SetUrlData(data);
    }
  }

  UrlData* GetUrlData() {
    if (provider_) {
      return provider_->urlData();
    }
    return nullptr;
  }
  
  const KURL& GetOrigin() {
    return provider_->Origin();
  }
  
  UrlData::CorsMode GetCorsMode() {
    return provider_->CorsMode();
  }

  MultiBufferBlockId GetTell() {
    return provider_ ? provider_->Tell() : 0;
  }

  void Fallback()  {
    return provider_->Fallback();
  }

  int64_t BlockSize() {
    return provider_->block_size();
  }

  int32_t blockSizeShift() {
    return provider_->block_size_shift();
  }

  WebURLRequest GetRequest(int64_t start, int64_t end) {
    return provider_->PrepareRequest(start, end);
  }

protected:
  std::unique_ptr<WebAssociatedURLLoader> CreateUrlLoader(
      const WebAssociatedURLLoaderOptions& options) {
    auto url_loader = std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
    return url_loader;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  KURL url_;
  NiceMock<MockResourceFetchContext> fetch_context_;
  UrlIndex url_index_{&fetch_context_, task_environment_.GetMainThreadTaskRunner()};
  scoped_refptr<UrlData> url_data_;
  raw_ptr<MockVideoCommonDataProvider> provider_;
};

class VideoRangeURLLoaderClientTest : public ::testing::Test {
public:
  VideoRangeURLLoaderClientTest() {
    for (int i = 0; i < kDataSize; ++i) {
      data_[i] = i;
    }

    ON_CALL(fetch_context_, CreateVideoUrlLoader(_))
        .WillByDefault(Invoke(
            this, &VideoRangeURLLoaderClientTest::CreateVideoUrlLoader));
    ON_CALL(fetch_context_, CreateUrlLoader(_))
        .WillByDefault(Invoke(
            this, &VideoRangeURLLoaderClientTest::CreateUrlLoader));
  }

  void Initialize(const char* url, int32_t pos, int64_t start, int64_t end,
      UrlData::CorsMode corsMode = UrlData::CORS_UNSPECIFIED) {
    url_ = KURL(url);
    url_index_.SetNewsFeedPageFitted(true);
    url_data_ = url_index_.GetByUrl(url_, corsMode, UrlData::kNormal);

    url_data_->OnRedirect(
        base::BindOnce(&VideoRangeURLLoaderClientTest::RedirectCallback,
                       base::Unretained(this)));

    auto provider = std::make_unique<MockVideoCommonDataProvider>(url_data_.get(), pos, kVideoId,
        false, task_environment_.GetMainThreadTaskRunner());
    provider_ = provider.get();
    url_data_->multibuffer()->AddProvider(std::move(provider));

    loader_client_ = std::make_unique<VideoRangeURLLoaderClient>(provider_, start, end, kVideoId,
        task_environment_.GetMainThreadTaskRunner());
    start_ = start;
  }
  
  std::pair<int64_t, int64_t> GetRange() {
    return loader_client_->Range();
  }

  bool ProviderInRange() {
    return loader_client_->ProviderInThisRange();
  }

  void FullResponse(int64_t instance_size) {
    WebURLResponse response(url_);
    response.SetHttpHeaderField(
        WebString::FromUTF8("Content-Length"),
        WebString::FromUTF8(base::StringPrintf("%" PRId64, instance_size)));
    response.SetExpectedContentLength(instance_size);
    response.SetHttpStatusCode(kHttpOK_VideoOpt);
    loader_client_->DidReceiveResponse(response);
  }


  void PartialResponse(int64_t first_position,
                       int64_t last_position,
                       int64_t instance_size) {
    PartialResponse(first_position, last_position, instance_size, false, true);
  }

  void PartialResponse(int64_t first_position,
                       int64_t last_position,
                       int64_t instance_size,
                       bool chunked,
                       bool accept_ranges) {
    WebURLResponse response(url_);
    response.SetHttpHeaderField(
        WebString::FromUTF8("Content-Range"),
        WebString::FromUTF8(
            base::StringPrintf("bytes "
                               "%" PRId64 "-%" PRId64 "/%" PRId64,
                               first_position, last_position, instance_size)));

    // HTTP 1.1 doesn't permit Content-Length with Transfer-Encoding: chunked.
    int64_t content_length = -1;
    if (chunked) {
      response.SetHttpHeaderField(WebString::FromUTF8("Transfer-Encoding"),
                                  WebString::FromUTF8("chunked"));
    } else {
      content_length = last_position - first_position + 1;
    }
    response.SetExpectedContentLength(content_length);

    // A server isn't required to return Accept-Ranges even though it might.
    if (accept_ranges) {
      response.SetHttpHeaderField(WebString::FromUTF8("Accept-Ranges"),
                                  WebString::FromUTF8("bytes"));
    }

    response.SetHttpStatusCode(kHttpPartialContent_VideoOpt);
    loader_client_->DidReceiveResponse(response);

    EXPECT_EQ(instance_size, url_data_->length());
  }

  void Redirect(const char* url) {
    WebURL new_url{KURL(url)};
    WebURLResponse redirect_response(url_);

    loader_client_->WillFollowRedirect(new_url, redirect_response);

    base::RunLoop().RunUntilIdle();
  }

  void StopWhenLoad() {
    provider_ = nullptr;
    loader_client_ = nullptr;
    url_data_ = nullptr;
  }

  void WriteData(int size) {
    auto data = base::HeapArray<char>::Uninit(size);
    loader_client_->DidReceiveData(data);
  }

  MOCK_METHOD1(RedirectCallback, void(const scoped_refptr<UrlData>&));

  // Verifies that data in buffer[0...size] is equal to data_[pos...pos+size].
  void VerifyBuffer(uint8_t* buffer, int pos, int size) {
    EXPECT_EQ(0, memcmp(buffer, data_ + pos, size));
  }

  void SetUrlData(const scoped_refptr<UrlData>& new_url_data) {
    url_data_ = new_url_data;
  }

protected:
  std::unique_ptr<WebAssociatedURLLoader> CreateVideoUrlLoader(
      const WebAssociatedURLLoaderOptions& options) {
    auto url_loader = std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
    return url_loader;
  }

  std::unique_ptr<WebAssociatedURLLoader> CreateUrlLoader(
      const WebAssociatedURLLoaderOptions& options) {
    auto url_loader = std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
    return url_loader;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  KURL url_;
  NiceMock<MockResourceFetchContext> fetch_context_;
  UrlIndex url_index_{&fetch_context_, task_environment_.GetMainThreadTaskRunner()};
  scoped_refptr<UrlData> url_data_;
  
  raw_ptr<MockVideoCommonDataProvider> provider_;
  std::unique_ptr<VideoRangeURLLoaderClient> loader_client_;

  int64_t start_ = 0;
  uint8_t data_[kDataSize];
};

// VideoCommonDataProviderTest
TEST_F(VideoCommonDataProviderTest, VideoOpt_UrlDataTest) {
  Initialize(kHttpUrl, 0);

  UrlData* urlDataPtr = GetUrlData();
  EXPECT_NE(urlDataPtr, nullptr);

  KURL url1("http://foo.bar.com");
  UrlIndex url_index_local{nullptr, task_environment_.GetMainThreadTaskRunner()};
  scoped_refptr<UrlData> url_data_local =
    url_index_local.GetByUrl(url1, UrlData::CORS_UNSPECIFIED, UrlData::kNormal);
  SetUrlData(url_data_local.get());
  EXPECT_NE(url_data_local.get(), nullptr);
}

TEST_F(VideoCommonDataProviderTest, VideoOpt_OriginCorsModeTellTest) {
  Initialize(kHttpUrl, 0);

  const KURL& urlOrigin = GetOrigin();
  UrlData::CorsMode corsMode = GetCorsMode();
  MultiBufferBlockId blockId = GetTell();

  KURL kurl("http://foo.bar.com");

  EXPECT_EQ(corsMode, UrlData::CORS_UNSPECIFIED);
  EXPECT_EQ(blockId, 0);
}

TEST_F(VideoCommonDataProviderTest, VideoOpt_BlockSizeTest) {
  Initialize(kHttpUrl, 0);

  UrlData* urlDataPtr = GetUrlData();
  EXPECT_NE(urlDataPtr, nullptr);

  ResourceMultiBuffer* multiBufferPtr = urlDataPtr->multibuffer();
  EXPECT_NE(multiBufferPtr, nullptr);

  int64_t block_size  = BlockSize();
  int32_t block_size_shift = blockSizeShift();

  int64_t expect_block_size = 32768;
  int32_t expect_block_size_shift = 15;

  EXPECT_EQ(block_size, expect_block_size);
  EXPECT_EQ(block_size_shift, expect_block_size_shift);
}

TEST_F(VideoCommonDataProviderTest, VideoOpt_FallbackTest) {
  Initialize(kHttpUrl, 0);

  UrlData* urlData = GetUrlData();
  if (urlData) {
    bool is_need_fallback = urlData->IsNeedFallback();
    EXPECT_EQ(is_need_fallback, false);

    Fallback();
    is_need_fallback = urlData->IsNeedFallback();
    EXPECT_EQ(is_need_fallback, true);
  }
}

TEST_F(VideoCommonDataProviderTest, VideoOpt_StartRequestTest) {
  Initialize(kHttpUrl, 0);

  WebURLRequest endRequest = GetRequest(0, -1); // end
  WebString rangeStr = WebString::FromUTF8(net::HttpRequestHeaders::kRange);
  WebString endRangeStr = endRequest.HttpHeaderField(rangeStr);
  EXPECT_EQ(endRangeStr.length(), 8);

  WebURLRequest blockRequest = GetRequest(0, 10); // block
  WebString blockRangeStr = blockRequest.HttpHeaderField(rangeStr);
  EXPECT_EQ(blockRangeStr.length(), 10);
}

TEST_F(VideoCommonDataProviderTest, VideoOpt_StartRequest002Test) {
  Initialize(kHttpUrl, 0, UrlData::CORS_UNSPECIFIED);

  WebString rangeStr = WebString::FromUTF8(net::HttpRequestHeaders::kRange);
  WebURLRequest blockRequest = GetRequest(0, 10); // block
  WebString blockRangeStr = blockRequest.HttpHeaderField(rangeStr);
  EXPECT_EQ(blockRangeStr.length(), 10);
}

TEST_F(VideoCommonDataProviderTest, VideoOpt_StartRequest003Test) {
  Initialize(kHttpUrl, 0, UrlData::CORS_USE_CREDENTIALS);

  WebString rangeStr = WebString::FromUTF8(net::HttpRequestHeaders::kRange);
  WebURLRequest blockRequest = GetRequest(0, 10); // block
  WebString blockRangeStr = blockRequest.HttpHeaderField(rangeStr);
  EXPECT_EQ(blockRangeStr.length(), 10);
}

// VideoRangeURLLoaderClientTest
TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_RangeTest) {
  Initialize(kHttpUrl, 0,  0, 100);
  std::pair<int64_t, int64_t> pairData = GetRange();
  EXPECT_EQ(pairData.second, 100);
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_ProviderInRangeTest1) {
  Initialize(kHttpUrl, 0, 0, 100);
  bool isInRange = ProviderInRange();
  EXPECT_TRUE(isInRange);
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_ProviderInRangeTest2) {
  Initialize(kHttpUrl, 0, 2, 100);
  bool isInRange = ProviderInRange();
  EXPECT_FALSE(isInRange);
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_ProviderInRangeTest3) {
  Initialize(kHttpUrl, 2, 2, -1);
  bool isInRange = ProviderInRange();
  EXPECT_TRUE(isInRange);
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_ProviderInRangeTest4) {
  Initialize(kHttpUrl, 101, 2, 100);
  bool isInRange = ProviderInRange();
  EXPECT_FALSE(isInRange);
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_StartStop) {
  Initialize(kHttpUrl, 0, 0, 100);
  loader_client_->Start();
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_PartialResponse) {
  Initialize(kHttpUrl, 0, 0, 100);
  loader_client_->Start();

  PartialResponse(100, 200, 1024);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_PartialResponse_Chunked) {
  Initialize(kHttpUrl, 0, 0, 100);
  loader_client_->Start();

  PartialResponse(100, 200, 1024, true, true);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_InvalidPartialResponse) {
  Initialize(kHttpUrl, 0, 0, 100);
  loader_client_->Start();

  WebURLResponse response(url_);
  response.SetHttpHeaderField(
      WebString::FromUTF8("Content-Range"),
      WebString::FromUTF8(base::StringPrintf("bytes "
                                             "%d-%d/%d",
                                             1, 10, 1024)));
  response.SetExpectedContentLength(10);
  response.SetHttpStatusCode(kHttpPartialContent_VideoOpt);
  loader_client_->DidReceiveResponse(response);
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_InvalidPartialResponse002) {
  Initialize(kHttpUrl, 10, 0, 100);
  loader_client_->Start();

  PartialResponse(0, 8, 100, true, true);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_TestRedirects) {
  // Test redirect.
  Initialize(kHttpUrl, 0, 0, 100);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);

  loader_client_->Start();
  Redirect(kHttpRedirect);
  FullResponse(1024);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_SetFifoTest) {
  Initialize(kHttpUrl, 0,  0, 100);
  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_Start001) {
  Initialize(kHttpUrl, 0, 100, 120);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_Start002) {
  Initialize(kHttpUrl, 0, 100, 0, UrlData::CORS_ANONYMOUS);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_ReceiveData001) {
  int64_t kDataSize_test = 3276800;
  Initialize(kHttpUrl, 0, 0, kDataSize_test -1);
  url_data_->set_length(kDataSize_test);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  std::vector<char> buffer(kDataSize_test, 0);
  base::span<const char> data_span(buffer.data(), buffer.size());
  loader_client_->DidReceiveData(data_span);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_ReceiveData002) {
  int64_t kDataSize_test = 3276800;
  Initialize(kHttpUrl, 0, 0, kDataSize_test -1);
  url_data_->set_length(kDataSize_test);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  // Use a NON-empty span: DidReceiveData has DCHECK_GT(data.size(), 0u) at
  // source L452, which BRK-kills the test binary under dcheck_always_on if the
  // span is empty. With 5 bytes and bytes_to_discard_=10, the partial-discard
  // path runs (discards all 5, data exhausted -> early return at L458), leaving
  // bytes_to_discard_=5.
  std::vector<char> buffer(5, 0);
  base::span<const char> data_span(buffer.data(), buffer.size());

  loader_client_->bytes_to_discard_ = 10;
  loader_client_->DidReceiveData(data_span);
  EXPECT_EQ(loader_client_->bytes_to_discard_, 5);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_Terminate001) {
  Initialize(kHttpUrl, 0, 0, 100);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  loader_client_->Terminate();
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidFinishLoading001) {
  Initialize(kHttpUrl, 10, 0, 100);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  loader_client_->DidFinishLoading();
  EXPECT_EQ(request_buffers.size(), 1);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidFinishLoading002) {
  int64_t kDataSize_test = 3276800;
  Initialize(kHttpUrl, 0, 0, kDataSize_test -1);
  url_data_->set_length(kDataSize_test);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  loader_client_->DidFinishLoading();
  EXPECT_GT(loader_client_->retries_, 0);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidFinishLoading003) {
  int64_t kDataSize_test = 3276800;
  Initialize(kHttpUrl, 0, 0, kDataSize_test -1);
  url_data_->set_length(kDataSize_test);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  loader_client_->retries_ = 50;
  loader_client_->DidFinishLoading();
  EXPECT_EQ(request_buffers.size(), 0);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidFail001) {
  Initialize(kHttpUrl, 10, 0, 100);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  WebURL web_url(url_);
  blink::WebURLError error(net::ERR_FAILED, web_url);
  loader_client_->DidFail(error);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidFail002) {
  Initialize(kHttpUrl, 10, 0, 100);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  WebURL web_url(url_);
  blink::WebURLError error(net::ERR_FAILED, web_url);
  loader_client_->retries_  = 5;
  loader_client_->DidFail(error);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidReStart001) {
  Initialize(kHttpUrl, 0, 0, 100);

  std::list<scoped_refptr<media::DataBuffer>> request_buffers;
  loader_client_->SetFifo(&request_buffers);
  loader_client_->Start();

  loader_client_->DidReStart();
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_ParseContentRange_ValidCases) {
  int64_t first = -1;
  int64_t last = -1;
  int64_t instance = -1;
  EXPECT_TRUE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 0-99/100", &first, &last, &instance));
  EXPECT_EQ(first, 0);
  EXPECT_EQ(last, 99);
  EXPECT_EQ(instance, 100);

  first = last = instance = -1;
  EXPECT_TRUE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 0-10/*", &first, &last, &instance));
  EXPECT_EQ(first, 0);
  EXPECT_EQ(last, 10);
  EXPECT_EQ(instance, kPositionNotSpecified);
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_ParseContentRange_FormatErrors) {
  int64_t first = -1;
  int64_t last = -1;
  int64_t instance = -1;
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "range 0-9/100", &first, &last, &instance));   // wrong prefix
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 01234/100", &first, &last, &instance));  // no dash
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 0-10", &first, &last, &instance));       // no slash
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 0/10-20", &first, &last, &instance));    // slash before dash
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 0-10/", &first, &last, &instance));      // trailing slash only
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_ParseContentRange_NumberErrors) {
  int64_t first = -1;
  int64_t last = -1;
  int64_t instance = -1;
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes x-10/100", &first, &last, &instance));
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 0-x/100", &first, &last, &instance));
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 0-10/xx", &first, &last, &instance));
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_ParseContentRange_RangeErrors) {
  int64_t first = -1;
  int64_t last = -1;
  int64_t instance = -1;
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 20-10/100", &first, &last, &instance));
  EXPECT_FALSE(VideoRangeURLLoaderClient::ParseContentRange(
      "bytes 0-2000/100", &first, &last, &instance));
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_VerifyPartialResponse_GarbageRange) {
  Initialize(kHttpUrl, 0, 0, 100);
  WebURLResponse response(url_);
  response.SetHttpHeaderField(WebString::FromUTF8("Content-Range"),
                              WebString::FromUTF8("garbage"));

  EXPECT_FALSE(loader_client_->VerifyPartialResponse(response, url_data_));
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_VerifyPartialResponse_LengthPreset) {
  Initialize(kHttpUrl, 0, 0, 100);
  url_data_->set_length(1000);      // already known -> L556 branch skipped
  WebURLResponse response(url_);
  response.SetHttpHeaderField(
      WebString::FromUTF8("Content-Range"),
      WebString::FromUTF8("bytes 0-99/2048"));
  EXPECT_TRUE(loader_client_->VerifyPartialResponse(response, url_data_));

  EXPECT_EQ(url_data_->length(), 1000);
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_VerifyPartialResponse_FirstGtBytePos) {
  Initialize(kHttpUrl, 0, 0, 100);
  WebURLResponse response(url_);
  response.SetHttpHeaderField(
      WebString::FromUTF8("Content-Range"),
      WebString::FromUTF8("bytes 10-20/100"));

  EXPECT_FALSE(loader_client_->VerifyPartialResponse(response, url_data_));
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_VerifyPartialResponse_LastLtBytePos) {
  Initialize(kHttpUrl, 0, 100, 200);
  WebURLResponse response(url_);
  response.SetHttpHeaderField(
      WebString::FromUTF8("Content-Range"),
      WebString::FromUTF8("bytes 0-50/200"));

  EXPECT_FALSE(loader_client_->VerifyPartialResponse(response, url_data_));
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_VerifyPartialResponse_Success) {
  Initialize(kHttpUrl, 0, 0, 100);  // byte_pos() = 0, length unknown
  EXPECT_EQ(url_data_->length(), kPositionNotSpecified);
  WebURLResponse response(url_);
  response.SetHttpHeaderField(
      WebString::FromUTF8("Content-Range"),
      WebString::FromUTF8("bytes 0-99/100"));
  EXPECT_TRUE(loader_client_->VerifyPartialResponse(response, url_data_));
  EXPECT_EQ(url_data_->length(), 100);  // L556 set length from instance size.
  EXPECT_EQ(loader_client_->bytes_to_discard_, 0u);  // byte_pos - first = 0.
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_WillFollowRedirect_CorsSpecified) {
  Initialize(kHttpUrl, 0, 0, 100, UrlData::CORS_ANONYMOUS);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  WebURL new_url{KURL(kHttpRedirect)};
  WebURLResponse redirect_response(url_);
  EXPECT_TRUE(loader_client_->WillFollowRedirect(new_url, redirect_response));
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_WillFollowRedirect_SameOrigin) {
  Initialize(kHttpUrl, 0, 0, 100);  // CORS_UNSPECIFIED
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  WebURL new_url{KURL("http://foo.bar.com/elsewhere")};  // same origin
  WebURLResponse redirect_response(url_);
  EXPECT_TRUE(loader_client_->WillFollowRedirect(new_url, redirect_response));
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_WillFollowRedirect_CrossOriginWithData) {
  Initialize(kHttpUrl, 0, 0, 100);  // CORS_UNSPECIFIED
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  bufs.push_back(media::DataBuffer::CreateEOSBuffer());  // fifo non-empty
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  WebURL new_url{KURL(kHttpRedirect)};  // cross origin
  WebURLResponse redirect_response(url_);
  // Cross-origin  (map non-empty OR fifo non-empty) -> Fail  return false.
  EXPECT_FALSE(loader_client_->WillFollowRedirect(new_url, redirect_response));
  // "this" may be invalidated by Fail(); reset and do not reuse loader_client_.
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_DidReceiveResponse_HttpVersions) {
  Initialize(kHttpUrl, 0, 0, 100);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  // Exercise every arm of the DCHECK_IS_ON HttpVersion switch.
  const WebURLResponse::HTTPVersion versions[] = {
      WebURLResponse::kHTTPVersion_0_9, WebURLResponse::kHTTPVersion_1_0,
      WebURLResponse::kHTTPVersion_1_1, WebURLResponse::kHTTPVersion_2_0,
      WebURLResponse::kHTTPVersionUnknown};
  for (auto version : versions) {
    WebURLResponse response(url_);
    response.SetHttpVersion(version);
    response.SetHttpStatusCode(kHttpPartialContent_VideoOpt);
    response.SetHttpHeaderField(WebString::FromUTF8("Content-Range"),
                                WebString::FromUTF8("bytes 0-99/100"));
    response.SetExpectedContentLength(100);
    loader_client_->DidReceiveResponse(response);
  }
  EXPECT_EQ(url_data_->length(), 100);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_DidReceiveResponse_LastModifiedHeader) {
  Initialize(kHttpUrl, 0, 0, 100);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpPartialContent_VideoOpt);
  response.SetHttpHeaderField(WebString::FromUTF8("Content-Range"),
                              WebString::FromUTF8("bytes 0-99/100"));
  response.SetHttpHeaderField(
      WebString::FromUTF8("Last-Modified"),
      WebString::FromUTF8("Wed, 21 Oct 2015 07:28:00 GMT"));
  loader_client_->DidReceiveResponse(response);  // parses Last-Modified
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_DidReceiveResponse_AcaoHeader) {
  Initialize(kHttpUrl, 0, 0, 100);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpPartialContent_VideoOpt);
  response.SetHttpHeaderField(WebString::FromUTF8("Content-Range"),
                              WebString::FromUTF8("bytes 0-99/100"));
  response.SetHttpHeaderField(
      WebString::FromUTF8("Access-Control-Allow-Origin"),
      WebString::FromUTF8("*"));
  loader_client_->DidReceiveResponse(response);  // set_has_access_control()
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_DidReceiveResponse_ServiceWorker) {
  Initialize(kHttpUrl, 0, 0, 100);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpPartialContent_VideoOpt);
  response.SetHttpHeaderField(WebString::FromUTF8("Content-Range"),
                              WebString::FromUTF8("bytes 0-99/100"));
  response.SetWasFetchedViaServiceWorker(true);
  std::vector<WebURL> url_list{WebURL{url_}};
  response.SetUrlListViaServiceWorker(url_list);  // HasUrlList -> use ResponseUrl
  loader_client_->DidReceiveResponse(response);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidReceiveResponse_416) {
  Initialize(kHttpUrl, 0, 0, 100);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  WebURLResponse response(url_);
  response.SetHttpStatusCode(416);  // kHttpRangeNotSatisfiable -> end_of_file
  loader_client_->DidReceiveResponse(response);
  EXPECT_EQ(bufs.size(), 1u);  // EOS buffer pushed at end of DidReceiveResponse.
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_DidReceiveData_DiscardThenProcess) {
  Initialize(kHttpUrl, 0, 0, 3276800 - 1);
  url_data_->set_length(3276800);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  loader_client_->bytes_to_discard_ = 10;
  std::vector<char> buffer(100, 0);  // size >= 1 (DCHECK_GT at L452)
  base::span<const char> data_span(buffer.data(), buffer.size());
  loader_client_->DidReceiveData(data_span);
  EXPECT_EQ(loader_client_->bytes_to_discard_, 0u);  // 10 consumed
  EXPECT_EQ(bufs.size(), 1u);                         // 90 bytes buffered
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_DidReceiveData_ProviderNotInRange) {
  Initialize(kHttpUrl, 2, 0, 100);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  std::vector<char> buffer(50, 0);  // size >= 1
  base::span<const char> data_span(buffer.data(), buffer.size());
  loader_client_->DidReceiveData(data_span);
  // Data is still buffered; only OnDataProviderEvent is skipped.
  EXPECT_EQ(bufs.size(), 1u);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidFail_BytePosZero) {
  Initialize(kHttpUrl, 0, 0, 100);  // byte_pos() = 0 -> "&& byte_pos()!=0" false
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  WebURL web_url(url_);
  blink::WebURLError error(net::ERR_FAILED, web_url);
  loader_client_->DidFail(error);  // NotifyMediaWebURLError (null-cb no-op)Fail
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidFail_MaxRetries) {
  Initialize(kHttpUrl, 0, 100, 200);  // byte_pos() = 100 != 0
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  loader_client_->retries_ = VideoRangeURLLoaderClient::kMaxRetries;
  WebURL web_url(url_);
  blink::WebURLError error(net::ERR_FAILED, web_url);
  loader_client_->DidFail(error);  // retries < kMax false -> else branch
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_DidFinishLoading_EndUnboundedRetry) {
  Initialize(kHttpUrl, 0, 0, -1);  // end_ = -1
  url_data_->set_length(1024);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  loader_client_->DidFinishLoading();  // pos(0) < length(1024), end_ == -1
  EXPECT_EQ(loader_client_->retries_, 1);  // retry scheduled
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_Start_LengthGuardTerminate) {
  Initialize(kHttpUrl, 0, 100, -1);  // byte_pos() = 100
  url_data_->set_length(50);         // 100 >= 50 -> right conjunct of Start guard
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);

  loader_client_->Start();  // posts Terminate (active_loader_ left null)
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(bufs.size(), 1u);  // Terminate pushed an EOS buffer.
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_SetDefersLoading_ActiveLoader) {
  Initialize(kHttpUrl, 0, 0, 100);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();  // sets active_loader_

  loader_client_->SetDefersLoading(true);   // forwarded to active_loader_
  loader_client_->SetDefersLoading(false);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest,
       VideoOpt_SetDefersLoading_NoLoader) {
  Initialize(kHttpUrl, 0, 0, 100);  // no Start -> active_loader_ is null
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);

  loader_client_->SetDefersLoading(true);  // null guard -> no-op
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidSendData) {
  Initialize(kHttpUrl, 0, 0, 100);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  loader_client_->DidSendData(0, 0);
  StopWhenLoad();
}

TEST_F(VideoRangeURLLoaderClientTest, VideoOpt_DidDownloadData) {
  Initialize(kHttpUrl, 0, 0, 100);
  std::list<scoped_refptr<media::DataBuffer>> bufs;
  loader_client_->SetFifo(&bufs);
  loader_client_->Start();

  loader_client_->DidDownloadData(0);
  StopWhenLoad();
}

}