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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <climits>
#include <unistd.h>
#include <vector>
#include <filemanagement/file_uri/oh_file_uri.h>
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api.h"
#define private public
#include "datashare_adapter_impl.h"
#undef private

using namespace MockNdkApi;
using namespace OHOS::NWeb;

class DatashareAdapterImplTest : public ::testing::Test {
  protected:
   void SetAllMockType(bool type) {
      MockDatashareCommonEventSupport::bGetPath = type;
      MockDatashareCommonEventSupport::bGetFileNameMock = type;
   }
    void SetUp() override {
      adapter_ = std::make_unique<DatashareAdapterImpl>();
      SetAllMockType(true);
    }

    void TearDown() override {
      adapter_.reset();
      SetAllMockType(false);
    }

    std::unique_ptr<DatashareAdapterImpl> adapter_;
    const std::string validUri_ = "content://valid/uri";
};


TEST_F(DatashareAdapterImplTest, GetRealPath_Fail) {
  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
              GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                validUri_.length(), testing::_))
    .WillOnce(testing::Return(ERR_ENOMEM));

  std::string result = adapter_->GetRealPath(validUri_);
  EXPECT_TRUE(result.empty());
}

TEST_F(DatashareAdapterImplTest, GetRealPath_Success) {
  const std::string expectedPath = "/data/storage/el2/base/files/valid";

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(expectedPath.c_str());
      return ERR_OK;
    }));

  std::string result = adapter_->GetRealPath(validUri_);
  EXPECT_EQ(result, expectedPath);
}

TEST_F(DatashareAdapterImplTest, GetRealPath_RejectParentPath) {
  const std::string traversalPath =
      "/data/storage/el2/media/../../../../etc/passwd";

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(traversalPath.c_str());
      return ERR_OK;
    }));

  EXPECT_TRUE(adapter_->GetRealPath(validUri_).empty());
}

TEST_F(DatashareAdapterImplTest, GetRealPath_RejectIllegalPathPrefix) {
  const std::string illegalPath = "/etc/passwd";

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(illegalPath.c_str());
      return ERR_OK;
    }));

  EXPECT_TRUE(adapter_->GetRealPath(validUri_).empty());
}

TEST_F(DatashareAdapterImplTest, GetRealPath_RejectNulEncodedUri) {
  const std::string uri =
      "file://docs/storage/Users/currentUser/Documents/a%00b";

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(uri.c_str()), uri.length(),
                                 testing::_))
    .Times(0);

  EXPECT_TRUE(adapter_->GetRealPath(uri).empty());
}

TEST_F(DatashareAdapterImplTest, ResolveSymlinksForExistingPrefix_ExpandsExistingSymlink) {
  std::string tmpl = ::testing::TempDir() + "datashare_link_XXXXXX";
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');
  char* dir = mkdtemp(buffer.data());
  ASSERT_NE(dir, nullptr);

  const std::string targetPath = std::string(dir) + "/target";
  FILE* target = fopen(targetPath.c_str(), "w");
  ASSERT_NE(target, nullptr);
  fclose(target);

  const std::string linkPath = std::string(dir) + "/link";
  ASSERT_EQ(symlink(targetPath.c_str(), linkPath.c_str()), 0);

  char expected[PATH_MAX];
  ASSERT_NE(realpath(targetPath.c_str(), expected), nullptr);
  EXPECT_EQ(DatashareAdapterImpl::ResolveSymlinksForExistingPrefix(linkPath),
            std::string(expected));

  unlink(linkPath.c_str());
  unlink(targetPath.c_str());
  rmdir(dir);
}

TEST_F(DatashareAdapterImplTest, ResolveSymlinksForExistingPrefix_KeepsMissingTail) {
  const std::string base = ::testing::TempDir();
  char resolvedBase[PATH_MAX];
  ASSERT_NE(realpath(base.c_str(), resolvedBase), nullptr);

  const std::string path = base + "missing/tail";
  const std::string expected = std::string(resolvedBase) + "/missing/tail";
  EXPECT_EQ(DatashareAdapterImpl::ResolveSymlinksForExistingPrefix(path), expected);
}

// ---------------------------------------------------------------------------
// End-to-end tests for the physical symlink rejection branch in
// ConvertUriToValidPath. These exercise the security-critical path that the
// lexical-only tests (RejectParentPath / RejectIllegalPathPrefix) cannot
// reach: a path that passes the whitelist + ".." checks lexically but, after
// resolving symlinks, escapes the allowed root.
//
// prefixes /data/storage/ and /storage/ exist, so
// real symlinks are created under /data/storage/el2/base/ (the app sandbox)
// to genuinely exercise the physical realpath check.
// ---------------------------------------------------------------------------

// RAII helper: owns temp directories/files/links under /data/storage/ and
// cleans them up on destruction.
class DataStorageFixture {
 public:
  ~DataStorageFixture() {
    for (const auto& link : createdLinks_) {
      unlink(link.c_str());
    }
    for (const auto& file : createdFiles_) {
      unlink(file.c_str());
    }
    for (auto it = createdDirs_.rbegin(); it != createdDirs_.rend(); ++it) {
      rmdir(it->c_str());
    }
  }

  // Create a temp directory under /data/storage/el2/base/ (app sandbox).
  // Returns the realpath of the created directory.
  std::string MakeSandboxDir() {
    std::string tmpl =
        "/data/storage/el2/base/files/datashare_test_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* dir = mkdtemp(buf.data());
    EXPECT_NE(dir, nullptr);
    createdDirs_.push_back(dir);
    return std::string(dir);
  }

  // Create a temp directory outside the whitelist (under the system temp dir).
  std::string MakeOutsideDir() {
    std::string tmpl = ::testing::TempDir() + "datashare_outside_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* dir = mkdtemp(buf.data());
    EXPECT_NE(dir, nullptr);
    createdDirs_.push_back(dir);
    return std::string(dir);
  }

  void CreateSymlink(const std::string& target, const std::string& link) {
    ASSERT_EQ(symlink(target.c_str(), link.c_str()), 0);
    createdLinks_.push_back(link);
  }

  void CreateFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "w");
    ASSERT_NE(f, nullptr);
    fclose(f);
    createdFiles_.push_back(path);
  }

 private:
  std::vector<std::string> createdDirs_;
  std::vector<std::string> createdFiles_;
  std::vector<std::string> createdLinks_;
};

// Symlink inside /data/storage/ that points outside the sandbox must be
// rejected by the physical realpath check in ConvertUriToValidPath.
TEST_F(DatashareAdapterImplTest, GetRealPath_RejectSymlinkEscapingSandbox) {
  DataStorageFixture fix;
  std::string sandboxDir = fix.MakeSandboxDir();
  std::string outsideDir = fix.MakeOutsideDir();
  fix.CreateFile(outsideDir + "/secret.txt");

  std::string linkPath = sandboxDir + "/escape_link";
  fix.CreateSymlink(outsideDir + "/secret.txt", linkPath);

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(linkPath.c_str());
      return ERR_OK;
    }));

  // The symlink resolves to outsideDir/secret.txt which is not under
  // /data/storage/ — the physical check must reject it.
  EXPECT_TRUE(adapter_->GetRealPath(validUri_).empty());
}

// Symlink inside /data/storage/ that points to another location inside
// /data/storage/ must NOT be rejected (no false positive).
TEST_F(DatashareAdapterImplTest, GetRealPath_AllowSymlinkWithinSandbox) {
  DataStorageFixture fix;
  std::string sandboxDir = fix.MakeSandboxDir();
  fix.CreateFile(sandboxDir + "/real_file.txt");

  std::string linkPath = sandboxDir + "/safe_link";
  fix.CreateSymlink(sandboxDir + "/real_file.txt", linkPath);

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(linkPath.c_str());
      return ERR_OK;
    }));

  // The symlink resolves to sandboxDir/real_file.txt — still under
  // /data/storage/, so it must pass.
  std::string result = adapter_->GetRealPath(validUri_);
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.rfind("/data/storage/", 0), 0u);
}

// Same escaping-symlink scenario through OpenDataShareUriForRead.
TEST_F(DatashareAdapterImplTest,
       OpenDataShareUriForRead_RejectSymlinkEscapingSandbox) {
  DataStorageFixture fix;
  std::string sandboxDir = fix.MakeSandboxDir();
  std::string outsideDir = fix.MakeOutsideDir();
  fix.CreateFile(outsideDir + "/secret.txt");

  std::string linkPath = sandboxDir + "/escape_link";
  fix.CreateSymlink(outsideDir + "/secret.txt", linkPath);

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(linkPath.c_str());
      return ERR_OK;
    }));

  EXPECT_EQ(adapter_->OpenDataShareUriForRead(validUri_), -1);
}

// Same within-sandbox symlink scenario through OpenDataShareUriForRead.
TEST_F(DatashareAdapterImplTest,
       OpenDataShareUriForRead_AllowSymlinkWithinSandbox) {
  DataStorageFixture fix;
  std::string sandboxDir = fix.MakeSandboxDir();
  fix.CreateFile(sandboxDir + "/real_file.txt");

  std::string linkPath = sandboxDir + "/safe_link";
  fix.CreateSymlink(sandboxDir + "/real_file.txt", linkPath);

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(linkPath.c_str());
      return ERR_OK;
    }));

  int fd = adapter_->OpenDataShareUriForRead(validUri_);
  EXPECT_GE(fd, 0);
  if (fd >= 0) {
    close(fd);
  }
}

TEST_F(DatashareAdapterImplTest, GetFileDisplayName_Success) {
  const std::string expectedFileName = "valid_file.txt";

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
              GetFileNameMock(testing::StrEq(validUri_.c_str()),
                             testing::Eq(validUri_.length()), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(expectedFileName.c_str());
      return ERR_OK;
    }));

  std::string result = adapter_->GetFileDisplayName(validUri_);
  EXPECT_EQ(result, expectedFileName);
}

TEST_F(DatashareAdapterImplTest, GetFileDisplayName_Failure) {
  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
              GetFileNameMock(testing::StrEq(validUri_.c_str()),
                             testing::Eq(validUri_.length()), testing::_))
    .WillOnce(testing::Return(ERR_ENOMEM));

  std::string result = adapter_->GetFileDisplayName(validUri_);
  EXPECT_TRUE(result.empty());
}

TEST_F(DatashareAdapterImplTest, OpenDataShareUriForRead_Failure_GetPath) {
  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Return(ERR_ENOMEM));

  int fd = adapter_->OpenDataShareUriForRead(validUri_);
  EXPECT_EQ(fd, -1);
}

TEST_F(DatashareAdapterImplTest, OpenDataShareUriForRead_RejectParentPath) {
  const std::string traversalPath =
      "/data/storage/el2/media/../../../../etc/passwd";

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(traversalPath.c_str());
      return ERR_OK;
    }));

  EXPECT_EQ(adapter_->OpenDataShareUriForRead(validUri_), -1);
}

TEST_F(DatashareAdapterImplTest, OpenDataShareUriForRead_RejectIllegalPathPrefix) {
  const std::string illegalPath = "/etc/passwd";

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(validUri_.c_str()),
                                 validUri_.length(), testing::_))
    .WillOnce(testing::Invoke([&](const char*, unsigned int, char** out) {
      *out = strdup(illegalPath.c_str());
      return ERR_OK;
    }));

  EXPECT_EQ(adapter_->OpenDataShareUriForRead(validUri_), -1);
}

TEST_F(DatashareAdapterImplTest, OpenDataShareUriForRead_RejectNulEncodedUri) {
  const std::string uri =
      "file://docs/storage/Users/currentUser/Documents/a%00b";

  EXPECT_CALL(MockDatashareCommonEventSupport::GetInstance(),
               GetPathFromUriMock(testing::StrEq(uri.c_str()), uri.length(),
                                 testing::_))
    .Times(0);

  EXPECT_EQ(adapter_->OpenDataShareUriForRead(uri), -1);
}
