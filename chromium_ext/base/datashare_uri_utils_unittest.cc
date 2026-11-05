/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "base/datashare_uri_utils.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/files/file.h"
#include "base/strings/utf_string_conversions.h"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"

namespace base {
namespace testing {

using MockNdkApi::MockDatashareCommonEventSupport;

class DatashareUriUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MockDatashareCommonEventSupport::bGetPath = false;
    MockDatashareCommonEventSupport::bGetFileNameMock = false;
  }
  void TearDown() override {
    MockDatashareCommonEventSupport::bGetPath = false;
    MockDatashareCommonEventSupport::bGetFileNameMock = false;
  }
};

TEST_F(DatashareUriUtilsTest, OpenDatashareUriForRead001) {
  FilePath invalid_uri("invalid_datashare_uri");
  File file = OpenDatashareUriForRead(invalid_uri);
  EXPECT_FALSE(file.IsValid());
}

TEST_F(DatashareUriUtilsTest, GetFileDisplayName001) {
  FilePath uri("valid_uri");
  std::u16string result = GetFileDisplayName(uri);
  EXPECT_FALSE(result.empty());
}

TEST_F(DatashareUriUtilsTest, GetRealPath001) {
  FilePath uri("invalid_datashare_uri");
  std::string result = GetRealPath(uri);
  EXPECT_FALSE(result.empty());
}

TEST_F(DatashareUriUtilsTest, OpenDatashareUriForRead002) {
  const char kTempFile[] = "/data/storage/test_datashare_uri_utils_002";
  FILE* fp = fopen(kTempFile, "w");
  ASSERT_NE(fp, nullptr);
  fputs("test", fp);
  fclose(fp);

  MockDatashareCommonEventSupport::bGetPath = true;
  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(), GetPathFromUriMock)
      .WillOnce([kTempFile](const char* uri, unsigned int length, char** uriResult) {
        std::string path = kTempFile;
        *uriResult = static_cast<char*>(malloc(path.size() + 1));
        strcpy(*uriResult, path.c_str());
        return ERR_OK;
      });

  FilePath uri("datashare://test");
  File file = OpenDatashareUriForRead(uri);
  EXPECT_TRUE(file.IsValid());

  file.Close();
  remove(kTempFile);
}

}  // namespace testing
}  // namespace base
