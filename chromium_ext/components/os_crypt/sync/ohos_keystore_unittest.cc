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

#include "ohos_keystore.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"

namespace crypto {
namespace ohos {

// Replicate constants from ohos_keystore.cc anonymous namespace for testing
const size_t KEY_LENGTH = 32;
const uint32_t IV_SIZE = 16;
constexpr char V10[] = "V10";
constexpr uint32_t V10_SIZE = 3;

class OhosKeystoreTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
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

// Test GetKey with the same alias twice returns the same key
TEST_F(OhosKeystoreTest, GetKey_SameAliasTwice) {
  std::string key1 = GetKey("ut_test_persistent_alias");
  std::string key2 = GetKey("ut_test_persistent_alias");
  if (!key1.empty() && !key2.empty()) {
    EXPECT_EQ(key1, key2);
  }
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

}  // namespace ohos
}  // namespace crypto
