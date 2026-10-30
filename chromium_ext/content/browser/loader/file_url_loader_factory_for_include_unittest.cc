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

// This file is included by file_url_loader_factory_unittest.cc via #include.
// Do not add it to chromium_ext.gni or BUILD.gn as a standalone source.

#include "arkweb/build/features/features.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif
#include "arkweb/ohos_adapter_ndk/hiviewdfx_adapter/hitrace_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_ohos_adapter_helper.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"

namespace OHOS::NWeb {
class MockOhosResourceAdapter : public OhosResourceAdapter {
 public:
  MOCK_METHOD(
      bool,
      GetRawFileData,
      (const std::string& rawFile, size_t& len, uint8_t** dest, bool isSys),
      (override));
  MOCK_METHOD(std::shared_ptr<OhosFileMapper>,
              GetRawFileMapper,
              (const std::string& rawFile, bool isSys),
              (override));
  MOCK_METHOD(bool,
              IsRawFileExist,
              (const std::string& rawFile, bool isSys),
              (override));
  MOCK_METHOD(
      bool,
      GetRawFileLastModTime,
      (const std::string& rawFile, uint16_t& date, uint16_t& time, bool isSys),
      (override));
  MOCK_METHOD(bool,
              GetRawFileLastModTime,
              (const std::string& rawFile, time_t& time, bool isSys),
              (override));
  MOCK_METHOD(std::string, GetSystemLanguage, (), (override));
};
}  // namespace OHOS::NWeb

namespace content {
namespace {
static const char kResourceURLPrefix[] = "resource:///";
static const char kTestData[] = "datashare://";
}  // namespace

class ResourceURLLoaderTest : public FileURLLoaderFactoryTest {
 public:
  ResourceURLLoaderTest() : FileURLLoaderFactoryTest() {}
  ResourceURLLoaderTest(const ResourceURLLoaderTest&) = delete;
  ResourceURLLoaderTest& operator=(const ResourceURLLoaderTest&) = delete;
  ~ResourceURLLoaderTest() override = default;

 protected:
  void SetUp() override { FileURLLoaderFactoryTest::SetUp(); }
  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(mock_resource_adapter_.get());
    testing::Mock::VerifyAndClearExpectations(mock_ohos_adapter_helper_.get());
    OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
    mock_resource_adapter_.reset();
    mock_ohos_adapter_helper_.reset();
    FileURLLoaderFactoryTest::TearDown();
  }

  GURL FilePathToResourceURL(const base::FilePath& path,
                             const std::string& scheme = kResourceURLPrefix) {
    std::string url_string(scheme);
    std::string utf8_path = path.AsUTF8Unsafe();
    url_string.reserve(url_string.size() + (3 * utf8_path.size()));

    for (auto c : utf8_path) {
      if (c == '%' || c == ';' || c == '#' || c == '?' ||
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
          c == '\\' ||
#endif
          c <= ' ') {
        url_string += '%';
        base::AppendHexEncodedByte(static_cast<uint8_t>(c), url_string);
      } else {
        url_string += c;
      }
    }

    return GURL(url_string);
  }

  void OpenSwitch() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kOhosAppApiVersion, "9");
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kOhosHapPath, "/data/app/com.example.app/base.apk");
  }

  void SetOhosAdapterHelperStub(const std::string& test_data = kTestData) {
    mock_ohos_adapter_helper_ =
        std::make_unique<OHOS::NWeb::MockOhosAdapterHelper>();
    OHOS::NWeb::OhosAdapterHelper::SetInstance(mock_ohos_adapter_helper_.get());
    mock_resource_adapter_ =
        std::make_unique<OHOS::NWeb::MockOhosResourceAdapter>();
    auto adapter_ptr = mock_resource_adapter_.get();
    EXPECT_CALL(*mock_ohos_adapter_helper_, GetResourceAdapter(testing::_))
        .WillOnce(testing::Return(
            testing::ByMove(std::move(mock_resource_adapter_))));
    EXPECT_CALL(*mock_ohos_adapter_helper_, GetHiTraceAdapterInstance())
        .WillRepeatedly(testing::Invoke([]() -> OHOS::NWeb::HiTraceAdapter& {
          return OHOS::NWeb::HiTraceAdapterImpl::GetInstance();
        }));
    if (test_data.empty()) {
      EXPECT_CALL(*adapter_ptr, GetRawFileData)
          .WillRepeatedly(
              testing::Invoke([](const std::string& rawFile, size_t& len,
                                 uint8_t** dest, bool isSys) -> bool {
                len = 0;
                *dest = nullptr;
                return false;
              }));
    } else {
      EXPECT_CALL(*adapter_ptr, GetRawFileData)
          .WillRepeatedly(testing::Invoke(
              [test_data](const std::string& rawFile, size_t& len,
                          uint8_t** dest, bool isSys) -> bool {
                len = test_data.size();
                *dest = reinterpret_cast<uint8_t*>(strdup(test_data.c_str()));
                return true;
              }));
    }
  }

 protected:
  std::unique_ptr<OHOS::NWeb::MockOhosResourceAdapter> mock_resource_adapter_;
  std::unique_ptr<OHOS::NWeb::MockOhosAdapterHelper> mock_ohos_adapter_helper_;
};

// GetApplicationApiVersion - no switch set
TEST_F(ResourceURLLoaderTest, Invalid_AppApiVersion) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto command_line = base::CommandLine::ForCurrentProcess();
  // not set kOhosAppApiVersion
  command_line->RemoveSwitch(switches::kOhosAppApiVersion);

  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  request->url = FilePathToResourceURL(file);
  EXPECT_EQ(net::ERR_FAILED, CreateLoaderAndRun(std::move(request)));

  command_line->AppendSwitchASCII(switches::kOhosAppApiVersion, "10");
  auto request1 = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request1);
  request1->url = FilePathToResourceURL(file);
  EXPECT_EQ(net::ERR_FAILED, CreateLoaderAndRun(std::move(request1)));

  auto request2 = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request2);
  request2->url = FilePathToResourceURL(file);
  command_line->AppendSwitchASCII(switches::kOhosAppApiVersion,
                                  "0xffffffffffffffffff");
  EXPECT_EQ(net::ERR_FAILED, CreateLoaderAndRun(std::move(request2)));
}

// Invalid resource scheme
TEST_F(ResourceURLLoaderTest, Invalid_ResourcesScheme) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));

  auto request = std::make_unique<network::ResourceRequest>();
  OpenSwitch();
  // invalid scheme
  request->url = FilePathToResourceURL(file, "invalid_resources://");
  ASSERT_NE(net::OK, CreateLoaderAndRun(std::move(request)));
}

// ResourceURLLoader success path
TEST_F(ResourceURLLoaderTest, Response_Success) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  OpenSwitch();
  SetOhosAdapterHelperStub();
  request->url = FilePathToResourceURL(file);
  ASSERT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));

  ASSERT_NE(ResponseInfo(), nullptr);
  ASSERT_NE(ResponseInfo()->headers, nullptr);
  std::string mine_type;
  ASSERT_TRUE(ResponseInfo()->headers->GetMimeType(&mine_type));
  ASSERT_EQ("text/plain", mine_type);
  ASSERT_EQ(200, ResponseInfo()->headers->response_code());
  ASSERT_EQ("OK", ResponseInfo()->headers->GetStatusText());
}

// ResourceURLLoader GetRawFileData fails
TEST_F(ResourceURLLoaderTest, GetRawFileData_Fail) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  OpenSwitch();
  SetOhosAdapterHelperStub("");
  request->url = FilePathToResourceURL(file);
  ASSERT_EQ(net::ERR_FILE_NOT_FOUND, CreateLoaderAndRun(std::move(request)));
}

// ResourceURLLoader with data larger than kMaxBytesToSniff
TEST_F(ResourceURLLoaderTest, MaxBytesToSniff) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  std::string large_data(net::kMaxBytesToSniff + 1, 'a');
  OpenSwitch();
  SetOhosAdapterHelperStub(large_data);
  request->url = FilePathToResourceURL(file);
  ASSERT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));

  ASSERT_NE(ResponseInfo(), nullptr);
  ASSERT_NE(ResponseInfo()->headers, nullptr);
  std::string mine_type;
  ASSERT_TRUE(ResponseInfo()->headers->GetMimeType(&mine_type));
  ASSERT_EQ("text/plain", mine_type);
  ASSERT_EQ(200, ResponseInfo()->headers->response_code());
  ASSERT_EQ("OK", ResponseInfo()->headers->GetStatusText());
}

// GetApplicationApiVersion - empty string returns -1
TEST_F(ResourceURLLoaderTest, Invalid_AppApiVersionEmptyString) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto command_line = base::CommandLine::ForCurrentProcess();
  command_line->RemoveSwitch(switches::kOhosAppApiVersion);
  command_line->AppendSwitchASCII(switches::kOhosAppApiVersion, "");

  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  request->url = FilePathToResourceURL(file);
  EXPECT_EQ(net::ERR_FAILED, CreateLoaderAndRun(std::move(request)));
}

// FileURLLoaderUtils::FileDataSourceExt via file:// URL with regular file
TEST_F(ResourceURLLoaderTest, FileURLLoader_NormalFile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  std::string test_content = "hello file url loader";
  ASSERT_TRUE(base::WriteFile(file, test_content));

  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  request->url = net::FilePathToFileURL(file);
  ASSERT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));
  ASSERT_NE(ResponseInfo(), nullptr);
  ASSERT_NE(ResponseInfo()->headers, nullptr);
  ASSERT_EQ(200, ResponseInfo()->headers->response_code());
}

// FileURLLoaderUtils::FileDataSourceExt with non-existent file returns error
TEST_F(ResourceURLLoaderTest, FileURLLoader_NonExistentFile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath non_existent =
      base::FilePath(FILE_PATH_LITERAL("/non/existent/file.txt"));

  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  request->url = net::FilePathToFileURL(non_existent);
  ASSERT_NE(net::OK, CreateLoaderAndRun(std::move(request)));
}

// ResourceURLLoader with kOhosHapPath not set
TEST_F(ResourceURLLoaderTest, NoHapPath) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kOhosAppApiVersion, "10");
  // kOhosHapPath not set

  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  request->url = FilePathToResourceURL(file);
  EXPECT_EQ(net::ERR_FAILED, CreateLoaderAndRun(std::move(request)));
}

// ResourceURLLoader with a null resource adapter (GetResourceAdapter returns
// nullptr) completes with ERR_FAILED.
TEST_F(ResourceURLLoaderTest, ResourceAdapter_Null) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  OpenSwitch();
  mock_ohos_adapter_helper_ =
      std::make_unique<OHOS::NWeb::MockOhosAdapterHelper>();
  OHOS::NWeb::OhosAdapterHelper::SetInstance(mock_ohos_adapter_helper_.get());
  EXPECT_CALL(*mock_ohos_adapter_helper_, GetHiTraceAdapterInstance())
      .WillRepeatedly(testing::Invoke([]() -> OHOS::NWeb::HiTraceAdapter& {
        return OHOS::NWeb::HiTraceAdapterImpl::GetInstance();
      }));
  EXPECT_CALL(*mock_ohos_adapter_helper_, GetResourceAdapter(testing::_))
      .WillOnce(testing::Return(
          testing::ByMove(std::unique_ptr<OHOS::NWeb::OhosResourceAdapter>())));
  request->url = FilePathToResourceURL(file);
  EXPECT_EQ(net::ERR_FAILED, CreateLoaderAndRun(std::move(request)));
}

// ResourceURLLoader with an empty data source (GetRawFileData succeeds with
// zero length) exercises the total_bytes_to_send == 0 completion path.
TEST_F(ResourceURLLoaderTest, Response_EmptyData) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  OpenSwitch();
  mock_ohos_adapter_helper_ =
      std::make_unique<OHOS::NWeb::MockOhosAdapterHelper>();
  OHOS::NWeb::OhosAdapterHelper::SetInstance(mock_ohos_adapter_helper_.get());
  mock_resource_adapter_ =
      std::make_unique<OHOS::NWeb::MockOhosResourceAdapter>();
  auto adapter_ptr = mock_resource_adapter_.get();
  EXPECT_CALL(*mock_ohos_adapter_helper_, GetResourceAdapter(testing::_))
      .WillOnce(
          testing::Return(testing::ByMove(std::move(mock_resource_adapter_))));
  EXPECT_CALL(*mock_ohos_adapter_helper_, GetHiTraceAdapterInstance())
      .WillRepeatedly(testing::Invoke([]() -> OHOS::NWeb::HiTraceAdapter& {
        return OHOS::NWeb::HiTraceAdapterImpl::GetInstance();
      }));
  EXPECT_CALL(*adapter_ptr, GetRawFileData)
      .WillRepeatedly(
          testing::Invoke([](const std::string& rawFile, size_t& len,
                             uint8_t** dest, bool isSys) -> bool {
            len = 0;
            *dest = nullptr;
            return true;
          }));
  request->url = FilePathToResourceURL(file);
  EXPECT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));
}

// ResourceURLLoader with valid Range header
TEST_F(ResourceURLLoaderTest, Response_WithRangeHeader) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  std::string test_data = "0123456789ABCDEF";
  OpenSwitch();
  SetOhosAdapterHelperStub(test_data);
  request->url = FilePathToResourceURL(file);
  request->headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=0-3");
  ASSERT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));

  ASSERT_NE(ResponseInfo(), nullptr);
  ASSERT_NE(ResponseInfo()->headers, nullptr);
}

// ResourceURLLoader with invalid Range header (out of range)
TEST_F(ResourceURLLoaderTest, Response_InvalidRangeHeader) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  std::string test_data = "0123456789ABCDEF";
  OpenSwitch();
  SetOhosAdapterHelperStub(test_data);
  request->url = FilePathToResourceURL(file);
  request->headers.SetHeader(net::HttpRequestHeaders::kRange,
                             "bytes=99999-999999");
  ASSERT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            CreateLoaderAndRun(std::move(request)));
}

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
// FileURLLoaderUtils::FileDataSourceExt with datashare URI
TEST_F(ResourceURLLoaderTest, FileURLLoader_DataShareUri) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath datashare_path(FILE_PATH_LITERAL(
      "datashare://com.example.data/entry/main/storage/test.txt"));

  auto request = std::make_unique<network::ResourceRequest>();
  ASSERT_TRUE(request);
  request->url = net::FilePathToFileURL(datashare_path);
  // datashare URI without an accessible file will fail, but exercises
  // FileDataSourceExt with IsDataShareUri path
  ASSERT_NE(net::OK, CreateLoaderAndRun(std::move(request)));
}
#endif

}  // namespace content
