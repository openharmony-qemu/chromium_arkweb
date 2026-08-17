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

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/buildflag.h"

// clang-format off
#define private public
#include "components/os_crypt/sync/os_crypt.h"
#undef private
// clang-format on

#include "components/os_crypt/sync/key_storage_config_linux.h"

#if BUILDFLAG(ARKWEB_ENCRYPT)
#include "arkweb/chromium_ext/components/os_crypt/sync/ohos_crypto.h"
#endif

namespace {
constexpr char kObfuscationPrefixV10[] = "v10";
constexpr char kMetricDecryptedWithEmptyKey[] =
    "OSCrypt.Linux.DecryptedWithEmptyKey";
constexpr base::FilePath::CharType kNWebAssetHandleDir[] =
    FILE_PATH_LITERAL("migrate_bak");
constexpr char kNewbAssetHandleAlias[] = "asset_data_key";
}  // namespace

#if BUILDFLAG(IS_ARKWEB)
#include "arkweb/chromium_ext/components/os_crypt/sync/os_crypt_linux_for_include.h"
#endif

class CryptLinuxForIncludeTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::string keyHex1 = "000102030405060708090a0b0c0d0e0f";
    std::string keyHex2 = "1a1b1c1d1e1f20212223242526272829";
    std::string ivHex = "000102030405060708090a0b";
    base::HexStringToString(keyHex1, &keyString1);
    base::HexStringToString(keyHex2, &keyString2);
    base::HexStringToString(ivHex, &iv);
    oscryptImpl = std::make_unique<OSCryptImpl>();
  }

  void TearDown() override { oscryptImpl.reset(); }

  std::unique_ptr<crypto::SymmetricKey> CreateTestKey(
      const std::string& key_data) {
    return crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, key_data);
  }

  bool GetEncryptString(const std::string& encryText,
                        std::string* rawCiphertext,
                        crypto::SymmetricKey* encrypKey,
                        std::string& encryIv) {
    crypto::Encryptor encryptor;
    if (!encryptor.Init(encrypKey, crypto::Encryptor::GCM, encryIv)) {
      return false;
    }
    return encryptor.Encrypt(encryText, rawCiphertext);
  }

  std::string keyString1;
  std::string keyString2;
  std::string iv;
  std::unique_ptr<OSCryptImpl> oscryptImpl;
};

TEST_F(CryptLinuxForIncludeTest, TestDecryptWithIvForIncludeSuccess) {
  std::string plaintext;
  std::string testPlaintext = "testPlaintext";
  std::string rawCiphertext;
  bool result = false;
  std::unique_ptr<crypto::SymmetricKey> key1 = CreateTestKey(keyString1);
  std::unique_ptr<crypto::SymmetricKey> key2 = CreateTestKey(keyString2);

  EXPECT_TRUE(GetEncryptString(testPlaintext, &rawCiphertext, key1.get(), iv));
  bool returnValue = DecryptWithIvForInclude(
      rawCiphertext, key1.get(), key2.get(), &plaintext, iv, result);
  EXPECT_TRUE(returnValue);
  EXPECT_TRUE(result);
  EXPECT_EQ(testPlaintext, plaintext);
}

TEST_F(CryptLinuxForIncludeTest, TestDecryptWithIvForIncludeKeyOta) {
  std::string plaintext;
  std::string testPlaintext = "testPlaintextOta";
  std::string rawCiphertext;
  std::unique_ptr<crypto::SymmetricKey> key1 = CreateTestKey(keyString1);
  std::unique_ptr<crypto::SymmetricKey> key2 = CreateTestKey(keyString2);

  EXPECT_TRUE(GetEncryptString(testPlaintext, &rawCiphertext, key2.get(), iv));
  bool result = false;
  bool returnValue = DecryptWithIvForInclude(
      rawCiphertext, key1.get(), key2.get(), &plaintext, iv, result);
  EXPECT_TRUE(returnValue);
  EXPECT_TRUE(result);
}

TEST_F(CryptLinuxForIncludeTest, TestDecryptWithIvForIncludeFail) {
  std::string plaintext;
  std::string testPlaintext = "testPlaintextError";
  std::string rawCiphertext;
  std::unique_ptr<crypto::SymmetricKey> key1 = CreateTestKey(keyString1);

  crypto::SymmetricKey* key2 = nullptr;
  EXPECT_TRUE(GetEncryptString(testPlaintext, &rawCiphertext, key1.get(), iv));
  std::string errorKey;
  std::string keyHexError = "100102030405060708090a0b0c1d1e1f";
  base::HexStringToString(keyHexError, &errorKey);
  std::unique_ptr<crypto::SymmetricKey> errorSKey = CreateTestKey(errorKey);
  bool result = true;
  bool returnValue = DecryptWithIvForInclude(rawCiphertext, errorSKey.get(),
                                             key2, &plaintext, iv, result);
  EXPECT_TRUE(returnValue);
  EXPECT_FALSE(result);
}

TEST_F(CryptLinuxForIncludeTest, TestDecryptWithIvForIncludeWrongkey) {
  std::string plaintext;
  std::string testPlaintext = "testPlaintextError";
  std::string rawCiphertext;
  std::unique_ptr<crypto::SymmetricKey> key1 = CreateTestKey(keyString1);
  std::unique_ptr<crypto::SymmetricKey> key2 = CreateTestKey(keyString2);

  std::string errorKey;
  std::string keyHexError = "100102030405060708090a0b0c1d1e1f";
  base::HexStringToString(keyHexError, &errorKey);
  std::unique_ptr<crypto::SymmetricKey> errorSKey = CreateTestKey(errorKey);
  EXPECT_TRUE(
      GetEncryptString(testPlaintext, &rawCiphertext, errorSKey.get(), iv));
  bool result = true;
  bool returnValue = DecryptWithIvForInclude(
      rawCiphertext, key1.get(), key2.get(), &plaintext, iv, result);
  EXPECT_FALSE(returnValue);
}

#if BUILDFLAG(ARKWEB_ENCRYPT)
TEST_F(CryptLinuxForIncludeTest, TestGetPasswordForOtaFail) {
  crypto::SymmetricKey* key = oscryptImpl->GetPasswordForOtaFail();
  EXPECT_EQ(key, nullptr);
  oscryptImpl->password_ota_cache_ = CreateTestKey(keyString1);
  crypto::SymmetricKey* key2 = oscryptImpl->GetPasswordForOtaFail();
  EXPECT_NE(key2, nullptr);
}
#endif

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
TEST_F(CryptLinuxForIncludeTest, TestDecryptStringForMigrateEmpty) {
  // Empty ciphertext should clear plaintext and return true without
  // touching the encryption key.
  std::string ciphertext = "";
  std::string plaintext = "original";
  // Call on local instance to avoid singleton's GenerateEncryptionKeyForMigrate
  // which requires browser infrastructure (g_browser_process).
  bool result = oscryptImpl->DecryptStringForMigrate(ciphertext, &plaintext);
  EXPECT_TRUE(result);
  EXPECT_TRUE(plaintext.empty());
}

TEST_F(CryptLinuxForIncludeTest, TestDecryptStringForMigrateNullKey) {
  // When no encryption key is available (password_migrate_cache_ is null),
  // DecryptStringForMigrate should return false.
  std::string plaintext;
  std::string testPlaintext = "testPlaintext";
  std::string rawCiphertext;
  std::unique_ptr<crypto::SymmetricKey> encryptionKey =
      CreateTestKey(keyString1);
  EXPECT_TRUE(
      GetEncryptString(testPlaintext, &rawCiphertext, encryptionKey.get(), iv));
  // Prepend V10 prefix + IV to match expected format
  std::string formattedCiphertext =
      std::string(kObfuscationPrefixV10) + iv + rawCiphertext;
  // Ensure the password_migrate_cache_ is null (no key available)
  // Also set migration_count_ >= 1 and is_password_migrate_cached_ = true
  // to prevent GetPasswordV10ForMigrate from triggering
  // GenerateEncryptionKeyForMigrate
  oscryptImpl->password_migrate_cache_.reset();
  oscryptImpl->is_password_migrate_cached_ = true;
  oscryptImpl->migration_count_ = 1;
  bool result =
      oscryptImpl->DecryptStringForMigrate(formattedCiphertext, &plaintext);
  EXPECT_FALSE(result);
}

TEST_F(CryptLinuxForIncludeTest, TestDecryptStringForMigrateShortText) {
  // Ciphertext shorter than prefix + IV length should return true
  // (but with empty plaintext from early return).
  std::string shortCiphertext = std::string(kObfuscationPrefixV10) + "short";
  std::string plaintext = "existing";
  // Set up a migrate key so we reach the length check
  oscryptImpl->password_migrate_cache_ = CreateTestKey(keyString1);
  oscryptImpl->is_password_migrate_cached_ = true;
  oscryptImpl->migration_count_ = 1;
  bool result =
      oscryptImpl->DecryptStringForMigrate(shortCiphertext, &plaintext);
  // With short text (< prefix.length() + kIVSizeAESGCM), early return true
  EXPECT_TRUE(result);
}

TEST_F(CryptLinuxForIncludeTest, TestDecryptStringForMigrateDecryptionFail) {
  // Ciphertext with wrong key should fail decryption.
  std::string testPlaintext = "HelloTest";
  std::string rawCiphertext;
  std::unique_ptr<crypto::SymmetricKey> encryptionKey =
      CreateTestKey(keyString1);
  EXPECT_TRUE(
      GetEncryptString(testPlaintext, &rawCiphertext, encryptionKey.get(), iv));
  std::string formattedCiphertext =
      std::string(kObfuscationPrefixV10) + iv + rawCiphertext;
  // Set a different key as the migrate key
  oscryptImpl->password_migrate_cache_ = CreateTestKey(keyString2);
  oscryptImpl->is_password_migrate_cached_ = true;
  oscryptImpl->migration_count_ = 1;
  std::string plaintext;
  bool result =
      oscryptImpl->DecryptStringForMigrate(formattedCiphertext, &plaintext);
  EXPECT_FALSE(result);
}

TEST_F(CryptLinuxForIncludeTest, TestDecryptString16ForMigrateDecryptionFail) {
  // DecryptString16ForMigrate should return false when inner Decrypt fails.
  std::string testPlaintext = "testPlaintext";
  std::string rawCiphertext;
  std::unique_ptr<crypto::SymmetricKey> encryptionKey =
      CreateTestKey(keyString1);
  EXPECT_TRUE(
      GetEncryptString(testPlaintext, &rawCiphertext, encryptionKey.get(), iv));
  std::string formattedCiphertext =
      std::string(kObfuscationPrefixV10) + iv + rawCiphertext;
  // Set a different key so decryption fails
  oscryptImpl->password_migrate_cache_ = CreateTestKey(keyString2);
  oscryptImpl->is_password_migrate_cached_ = true;
  oscryptImpl->migration_count_ = 1;
  std::u16string plaintext;
  bool result =
      oscryptImpl->DecryptString16ForMigrate(formattedCiphertext, &plaintext);
  EXPECT_FALSE(result);
}

TEST_F(CryptLinuxForIncludeTest, TestSetMigrationCountCurrent) {
  oscryptImpl->SetMigrationCountCurrent(5);
  EXPECT_EQ(oscryptImpl->migration_count_current_, 5);
}
#endif

#if BUILDFLAG(ARKWEB_ENCRYPT)

// Test SymmetricKey::Import with invalid key size (not 16 or 32)
TEST_F(CryptLinuxForIncludeTest, TestSymmetricKeyImportInvalidSize) {
  std::string badKey8(8, 'A');
  auto key = crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, badKey8);
  EXPECT_EQ(key, nullptr);

  std::string badKey24(24, 'B');
  auto key24 =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, badKey24);
  EXPECT_EQ(key24, nullptr);
}

// Test Encryptor::Init with wrong IV size for CBC mode
TEST_F(CryptLinuxForIncludeTest, TestEncryptorInitCBCWrongIvSize) {
  auto key = CreateTestKey(keyString1);
  crypto::Encryptor encryptor;
  std::string badIv = "short";
  EXPECT_FALSE(encryptor.Init(key.get(), crypto::Encryptor::CBC, badIv));
}

// Test Encryptor::Init with non-empty IV for CTR mode
TEST_F(CryptLinuxForIncludeTest, TestEncryptorInitCTRWithIv) {
  auto key = CreateTestKey(keyString1);
  crypto::Encryptor encryptor;
  std::string someIv = "someiv";
  EXPECT_FALSE(encryptor.Init(key.get(), crypto::Encryptor::CTR, someIv));
}

// Test Encryptor::Init with wrong IV size for GCM mode
TEST_F(CryptLinuxForIncludeTest, TestEncryptorInitGCMWrongIvSize) {
  auto key = CreateTestKey(keyString1);
  crypto::Encryptor encryptor;
  std::string badIv = "short";
  EXPECT_FALSE(encryptor.Init(key.get(), crypto::Encryptor::GCM, badIv));
}

// Test Encryptor GCM decrypt with input size <= GCM_TAG_SIZE (16)
TEST_F(CryptLinuxForIncludeTest, TestEncryptorGCMDecryptShortInput) {
  auto key = CreateTestKey(keyString1);
  crypto::Encryptor encryptor;
  EXPECT_TRUE(encryptor.Init(key.get(), crypto::Encryptor::GCM, iv));

  std::string shortCiphertext(16, '\0');  // Exactly GCM_TAG_SIZE
  std::string plaintext;
  EXPECT_FALSE(encryptor.Decrypt(shortCiphertext, &plaintext));

  std::string shorterCiphertext(10, '\0');  // Less than GCM_TAG_SIZE
  EXPECT_FALSE(encryptor.Decrypt(shorterCiphertext, &plaintext));
}

// Test Encryptor CBC mode returns false (not supported)
TEST_F(CryptLinuxForIncludeTest, TestEncryptorCBCDecryptNotSupported) {
  auto key = CreateTestKey(keyString1);
  crypto::Encryptor encryptor;
  std::string cbcIv(16, '\0');
  ASSERT_TRUE(encryptor.Init(key.get(), crypto::Encryptor::CBC, cbcIv));

  std::string plaintext = "test";
  std::string ciphertext;
  EXPECT_FALSE(encryptor.Encrypt(plaintext, &ciphertext));
}

// Test GetPasswordV10 caches the key
TEST_F(CryptLinuxForIncludeTest, TestGetPasswordV10Cached) {
  crypto::SymmetricKey* key1 = oscryptImpl->GetPasswordV10();
  EXPECT_NE(key1, nullptr);
  crypto::SymmetricKey* key2 = oscryptImpl->GetPasswordV10();
  EXPECT_EQ(key1, key2);  // Same cached pointer
}

// Test GetPasswordV11 with config set
TEST_F(CryptLinuxForIncludeTest, TestGetPasswordV11WithConfig) {
  auto config = std::make_unique<os_crypt::Config>();
  config->store = "kwallet";
  oscryptImpl->SetConfig(std::move(config));
  // GetPasswordV11 with probe=true when config is set.
  // First call consumes config_ and tries to create KeyStorageLinux.
  // On device this may or may not succeed.
  crypto::SymmetricKey* key1 = oscryptImpl->GetPasswordV11(true);
  crypto::SymmetricKey* key2 = oscryptImpl->GetPasswordV11(true);
  // Second call should return same cached result
  EXPECT_EQ(key1, key2);
}

// Test GetPasswordV11 cached with null config returns nullptr
TEST_F(CryptLinuxForIncludeTest, TestGetPasswordV11CachedNullConfig) {
  // Call once to cache
  crypto::SymmetricKey* key1 = oscryptImpl->GetPasswordV11(true);
  // Without config v11 cache is null
  // Subsequent call hits cached path where is_password_v11_cached_ = true
  // but password_v11_cache_ is null and config_ is null => returns nullptr
  crypto::SymmetricKey* key2 = oscryptImpl->GetPasswordV11(true);
  EXPECT_EQ(key2, nullptr);
}

// Test DecryptWithIvForInclude with ota key also failing
TEST_F(CryptLinuxForIncludeTest, TestDecryptWithIvForIncludeBothKeysFail) {
  std::string plaintext;
  std::string testPlaintext = "testBothFail";
  std::string rawCiphertext;
  std::string errorKey;
  std::string keyHexError = "100102030405060708090a0b0c1d1e1f";
  base::HexStringToString(keyHexError, &errorKey);
  auto errorSKey = CreateTestKey(errorKey);

  EXPECT_TRUE(
      GetEncryptString(testPlaintext, &rawCiphertext, errorSKey.get(), iv));

  auto key1 = CreateTestKey(keyString1);
  auto key2 = CreateTestKey(keyString2);
  bool result = true;
  // Both key1 and key2 are wrong for this ciphertext
  bool returnValue = DecryptWithIvForInclude(
      rawCiphertext, key1.get(), key2.get(), &plaintext, iv, result);
  EXPECT_FALSE(returnValue);
}

#endif
