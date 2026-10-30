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

// The arkweb mock adapter MUST be included before ohos_keystore.h: the mock
// header and the third_party NDK ohos_adapter_helper.h share the same include
// guard (OHOS_ADAPTER_HELPER_H), and ohos_keystore.h pulls in the NDK version
// transitively. Include the mock first so the arkweb interfaces version wins.
// clang-format off
#include "arkweb/build/features/features.h"  // nogncheck
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_ohos_adapter_helper.h"
// clang-format on

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"

#include "ohos_keystore.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace crypto {
namespace ohos {

// Replicate constants from ohos_keystore.cc anonymous namespace for testing
const size_t KEY_LENGTH = 32;
const uint32_t IV_SIZE = 16;
const int COUNT_FOR_RETRY = 3;
constexpr char V10[] = "V10";
constexpr uint32_t V10_SIZE = 3;
constexpr base::FilePath::CharType kNWebKeyStoreDir[] =
    FILE_PATH_LITERAL("nwebks");
constexpr char kTestAlias[] = "ut_test_keystore_alias";

class MockKeystoreAdapter : public OHOS::NWeb::KeystoreAdapter {
 public:
  MOCK_METHOD(std::string,
              EncryptKey,
              (const std::string alias, const std::string plainData),
              (override));
  MOCK_METHOD(std::string,
              DecryptKey,
              (const std::string alis, const std::string encryptedData),
              (override));
  MOCK_METHOD(std::string,
              AssetQuery,
              (const std::string assetHandle),
              (override));
};

class OhosKeystoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Force GetPath() to resolve DIR_USER_DATA to the temp dir.
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_USER_DATA, temp_dir_.GetPath());

    helper_ = std::make_unique<OHOS::NWeb::MockOhosAdapterHelper>();
    OHOS::NWeb::OhosAdapterHelper::SetInstance(helper_.get());
    ON_CALL(*helper_, GetKeystoreAdapterInstance())
        .WillByDefault(ReturnRef(keystore_));
  }

  void TearDown() override {
    OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
    path_override_.reset();
    if (base::PathExists(KeyDir())) {
      base::DeletePathRecursively(KeyDir());
    }
  }

  base::FilePath KeyDir() const {
    return temp_dir_.GetPath().Append(kNWebKeyStoreDir);
  }

  base::FilePath KeyFile(const std::string& alias) const {
    return KeyDir().AppendASCII(alias);
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  std::unique_ptr<OHOS::NWeb::MockOhosAdapterHelper> helper_;
  MockKeystoreAdapter keystore_;
};

// Test GenerateLocalKey with various sizes
TEST_F(OhosKeystoreTest, GenerateLocalKey_32) {
  std::string key = GenerateLocalKey(32);
  EXPECT_EQ(key.size(), 32u);
}

TEST_F(OhosKeystoreTest, GenerateLocalKey_16) {
  std::string key = GenerateLocalKey(16);
  EXPECT_EQ(key.size(), 16u);
}

TEST_F(OhosKeystoreTest, GenerateLocalKey_1) {
  std::string key = GenerateLocalKey(1);
  EXPECT_EQ(key.size(), 1u);
}

TEST_F(OhosKeystoreTest, GenerateLocalKey_0) {
  std::string key = GenerateLocalKey(0);
  EXPECT_TRUE(key.empty());
}

// Test GenerateLocalKey produces different keys
TEST_F(OhosKeystoreTest, GenerateLocalKey_Uniqueness) {
  std::string key1 = GenerateLocalKey(32);
  std::string key2 = GenerateLocalKey(32);
  EXPECT_NE(key1, key2);
}

// Test GenerateLocalKey with size 32 produces 32-byte key
TEST_F(OhosKeystoreTest, GenerateLocalKey_32_Detailed) {
  std::string key = GenerateLocalKey(32);
  EXPECT_EQ(key.size(), 32u);
  // Verify key is not all zeros
  bool all_zero = true;
  for (char c : key) {
    if (c != 0) {
      all_zero = false;
      break;
    }
  }
  EXPECT_FALSE(all_zero);
}

// Replicate ValidateKeyAndGetEncryptedData logic for testing
// (The function is in anonymous namespace in ohos_keystore.cc and not
// directly accessible)
static size_t TestGetEncryptSize() {
  return V10_SIZE + IV_SIZE + KEY_LENGTH;
}

TEST_F(OhosKeystoreTest, EncryptSizeCalculation) {
  EXPECT_EQ(TestGetEncryptSize(), V10_SIZE + IV_SIZE + KEY_LENGTH);
  EXPECT_EQ(TestGetEncryptSize(), 3u + 16u + 32u);
  EXPECT_EQ(TestGetEncryptSize(), 51u);
}

// --- GetKey tests driven by mock keystore adapter + controlled file system ---

// GetKey with no existing key file: generates a local key, encrypts via the
// adapter (success on first retry), writes the encrypted blob, returns key.
TEST_F(OhosKeystoreTest, GetKey_NewKeyEncryptsAndWrites) {
  EXPECT_CALL(keystore_, EncryptKey(_, _))
      .WillOnce(Return(std::string(V10) + std::string(IV_SIZE, '\xab') +
                       std::string(KEY_LENGTH, '\xcd')));

  std::string key = GetKey(kTestAlias);

  EXPECT_EQ(key.size(), KEY_LENGTH);
  // Encrypted data was persisted.
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(KeyFile(kTestAlias), &content));
  EXPECT_FALSE(content.empty());
}

// GetKey: encrypt returns empty on every retry -> empty key returned. The key
// file itself is still created by the FLAG_CREATE open in the else branch,
// but no encrypted payload is ever written to it.
TEST_F(OhosKeystoreTest, GetKey_EncryptAlwaysFailsReturnsEmpty) {
  EXPECT_CALL(keystore_, EncryptKey(_, _))
      .Times(COUNT_FOR_RETRY)
      .WillRepeatedly(Return(std::string()));

  std::string key = GetKey(kTestAlias);
  EXPECT_TRUE(key.empty());
  // The empty key file is created by the FLAG_CREATE open, with no content.
  EXPECT_TRUE(base::PathExists(KeyFile(kTestAlias)));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(KeyFile(kTestAlias), &content));
  EXPECT_TRUE(content.empty());
}

// GetKey with an existing encrypted key file whose content is a valid
// V10-prefixed blob -> DecryptKey succeeds and returns the key.
TEST_F(OhosKeystoreTest, GetKey_ExistingKeyDecrypts) {
  std::string encrypted = std::string(V10) + std::string(IV_SIZE, '\xaa') +
                          std::string(KEY_LENGTH, '\xbb');
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::WriteFile(KeyFile(kTestAlias), encrypted));

  EXPECT_CALL(keystore_, DecryptKey(kTestAlias, encrypted))
      .WillOnce(Return(std::string(KEY_LENGTH, 'K')));

  std::string key = GetKey(kTestAlias);
  EXPECT_EQ(key, std::string(KEY_LENGTH, 'K'));
}

// GetKey with existing key file, but DecryptKey fails on first retry and
// succeeds on the second retry.
TEST_F(OhosKeystoreTest, GetKey_ExistingKeyDecryptRetry) {
  std::string encrypted = std::string(KEY_LENGTH, '\x11');
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::WriteFile(KeyFile(kTestAlias), encrypted));

  EXPECT_CALL(keystore_, DecryptKey(kTestAlias, encrypted))
      .WillOnce(Return(std::string()))
      .WillOnce(Return(std::string(KEY_LENGTH, 'L')));

  std::string key = GetKey(kTestAlias);
  EXPECT_EQ(key, std::string(KEY_LENGTH, 'L'));
}

// GetKey with existing key file, DecryptKey always fails -> empty.
TEST_F(OhosKeystoreTest, GetKey_ExistingKeyDecryptAlwaysFails) {
  std::string encrypted = std::string(V10) + std::string(IV_SIZE, '\xaa') +
                          std::string(KEY_LENGTH, '\xbb');
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::WriteFile(KeyFile(kTestAlias), encrypted));

  EXPECT_CALL(keystore_, DecryptKey(kTestAlias, encrypted))
      .Times(COUNT_FOR_RETRY)
      .WillRepeatedly(Return(std::string()));

  std::string key = GetKey(kTestAlias);
  EXPECT_TRUE(key.empty());
}

// GetKey with existing key file that is NOT valid (too short) -> falls into
// the else branch: generates a new key and re-encrypts.
TEST_F(OhosKeystoreTest, GetKey_InvalidExistingFileReencrypts) {
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::WriteFile(KeyFile(kTestAlias), "short"));

  EXPECT_CALL(keystore_, EncryptKey(_, _))
      .WillOnce(Return(std::string(KEY_LENGTH, '\x01')));

  std::string key = GetKey(kTestAlias);
  EXPECT_EQ(key.size(), KEY_LENGTH);
}

// GetKeyForOta: key dir missing -> early return empty (no crash).
TEST_F(OhosKeystoreTest, GetKeyForOta_MissingDirReturnsEmpty) {
  std::string key = GetKeyForOta(kTestAlias);
  EXPECT_TRUE(key.empty());
}

// GetKeyForOta: key file missing -> empty.
TEST_F(OhosKeystoreTest, GetKeyForOta_NoKeyFile) {
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  std::string key = GetKeyForOta(kTestAlias);
  EXPECT_TRUE(key.empty());
}

// GetKeyForOta with a V10-prefixed encrypted blob: strips V10 + IV prefix,
// calls EncryptKey with the stripped payload, returns the re-encrypted key.
TEST_F(OhosKeystoreTest, GetKeyForOta_V10BlobStripsPrefix) {
  std::string payload = std::string(KEY_LENGTH, '\x99');
  std::string encrypted =
      std::string(V10) + std::string(IV_SIZE, '\xaa') + payload;
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::WriteFile(KeyFile(kTestAlias), encrypted));

  EXPECT_CALL(keystore_, EncryptKey(kTestAlias, payload))
      .WillOnce(Return(std::string(KEY_LENGTH, 'R')));

  std::string key = GetKeyForOta(kTestAlias);
  EXPECT_EQ(key, std::string(KEY_LENGTH, 'R'));
}

// GetKeyForOta with a raw 32-byte blob (no V10 prefix): passes it through
// unchanged to EncryptKey.
TEST_F(OhosKeystoreTest, GetKeyForOta_RawBlobPassedThrough) {
  std::string encrypted = std::string(KEY_LENGTH, '\x77');
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::WriteFile(KeyFile(kTestAlias), encrypted));

  EXPECT_CALL(keystore_, EncryptKey(kTestAlias, encrypted))
      .WillOnce(Return(std::string(KEY_LENGTH, 'S')));

  std::string key = GetKeyForOta(kTestAlias);
  EXPECT_EQ(key, std::string(KEY_LENGTH, 'S'));
}

// GetKeyForOta: EncryptKey always fails -> empty after retries.
TEST_F(OhosKeystoreTest, GetKeyForOta_EncryptAlwaysFails) {
  std::string encrypted = std::string(KEY_LENGTH, '\x77');
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::WriteFile(KeyFile(kTestAlias), encrypted));

  EXPECT_CALL(keystore_, EncryptKey(_, _))
      .Times(COUNT_FOR_RETRY)
      .WillRepeatedly(Return(std::string()));

  std::string key = GetKeyForOta(kTestAlias);
  EXPECT_TRUE(key.empty());
}

// GetKeyForOta: EncryptKey fails on first retry then succeeds on second.
TEST_F(OhosKeystoreTest, GetKeyForOta_EncryptRetry) {
  std::string encrypted = std::string(KEY_LENGTH, '\x77');
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::WriteFile(KeyFile(kTestAlias), encrypted));

  EXPECT_CALL(keystore_, EncryptKey(_, _))
      .WillOnce(Return(std::string()))
      .WillOnce(Return(std::string(KEY_LENGTH, 'T')));

  std::string key = GetKeyForOta(kTestAlias);
  EXPECT_EQ(key, std::string(KEY_LENGTH, 'T'));
}

// GetKey with existing key file that is a directory: ReadFileToString fails
// -> ValidateKeyAndGetEncryptedData returns false, goes to the else branch
// and re-encrypts.
TEST_F(OhosKeystoreTest, GetKey_UnreadableExistingFileReencrypts) {
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  // A directory is not readable as a file.
  ASSERT_TRUE(base::CreateDirectory(KeyFile(kTestAlias)));

  EXPECT_CALL(keystore_, EncryptKey(_, _))
      .WillOnce(Return(std::string(KEY_LENGTH, '\x02')));

  std::string key = GetKey(kTestAlias);
  EXPECT_EQ(key.size(), KEY_LENGTH);
}

// GetKeyForOta with a V10-prefixed blob whose total length is shorter than
// V10_SIZE + IV_SIZE: the prefix-strip condition short-circuits, so the raw
// content is passed through to EncryptKey unchanged.
TEST_F(OhosKeystoreTest, GetKeyForOta_ShortV10BlobPassedThrough) {
  // "V10abc" -> starts_with(V10) true, but length 6 < 3 + 16 = 19.
  std::string encrypted = "V10abc";
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::WriteFile(KeyFile(kTestAlias), encrypted));

  EXPECT_CALL(keystore_, EncryptKey(kTestAlias, encrypted))
      .WillOnce(Return(std::string(KEY_LENGTH, 'U')));

  std::string key = GetKeyForOta(kTestAlias);
  EXPECT_EQ(key, std::string(KEY_LENGTH, 'U'));
}

// GetKeyForOta with a key file that cannot be read (a directory):
// ReadFileToString fails -> returns empty.
TEST_F(OhosKeystoreTest, GetKeyForOta_UnreadableFileReturnsEmpty) {
  ASSERT_TRUE(base::CreateDirectory(KeyDir()));
  ASSERT_TRUE(base::CreateDirectory(KeyFile(kTestAlias)));

  std::string key = GetKeyForOta(kTestAlias);
  EXPECT_TRUE(key.empty());
}

}  // namespace ohos
}  // namespace crypto
