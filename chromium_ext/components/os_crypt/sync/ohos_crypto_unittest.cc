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

#include "base/lazy_instance.h"
#define private public
#include "ohos_crypto.cc"
#undef private
#include "ohos_keystore.h"

namespace crypto {
namespace ohos {
class KeyCacheTest : public ::testing::Test {
 protected:
  void SetUp() { key_cache_ = std::make_shared<KeyCache>(); }
  void TearDown() { key_cache_.reset(); }
  std::shared_ptr<KeyCache> key_cache_ = nullptr;
};

TEST_F(KeyCacheTest, get_key) {
  std::string key_name = "name";
  std::shared_ptr<std::string> key_value =
      std::make_shared<std::string>("value");
  ASSERT_NE(key_cache_, nullptr);
  auto result = key_cache_->get_key(key_name, key_value.get());
  EXPECT_FALSE(result);
  KeyCache::key_map_entry_t map_entry_("1", 1);
  key_cache_->key_map_.clear();
  key_cache_->key_map_.emplace(key_name, map_entry_);
  result = key_cache_->get_key(key_name, key_value.get());
  EXPECT_TRUE(result);
  result = key_cache_->get_key(key_name, nullptr);
  EXPECT_TRUE(result);
}

TEST_F(KeyCacheTest, add_key_001) {
  std::string key_name = "name";
  std::string key_value = "value";
  ASSERT_NE(key_cache_, nullptr);
  key_cache_->key_map_.clear();
  key_cache_->add_key(key_name, key_value);
  EXPECT_EQ(key_cache_->key_map_[key_name].time_, 0);
}

TEST_F(KeyCacheTest, add_key_002) {
  std::string key_name = "name";
  std::string key_value = "value";
  ASSERT_NE(key_cache_, nullptr);
  key_cache_->key_map_.clear();
  for (int i = 0; i < 256; i++) {
    key_cache_->key_map_.emplace(key_name + std::to_string(i), key_value);
  }

  key_cache_->add_key(key_name, key_value);
  EXPECT_EQ(key_cache_->key_map_[key_name].time_, 0);
}

TEST_F(KeyCacheTest, AddKeyTimeIncrement) {
  std::string key_name = "test_key";
  std::string key_value = "test_value";
  ASSERT_NE(key_cache_, nullptr);
  key_cache_->key_map_.clear();
  key_cache_->add_key(key_name, key_value);
  uint64_t first_time = key_cache_->key_map_[key_name].time_;
  key_cache_->add_key(key_name, key_value);
  EXPECT_GT(key_cache_->key_map_[key_name].time_, first_time);
}

TEST_F(KeyCacheTest, AddKeyRemoveOldest) {
  std::string key_name = "name";
  std::string key_value = "value";
  ASSERT_NE(key_cache_, nullptr);
  key_cache_->key_map_.clear();
  for (int i = 0; i < 256; i++) {
    key_cache_->key_map_.emplace(key_name + std::to_string(i), key_value);
  }
  std::string oldest_key;
  uint64_t min_time = UINT64_MAX;
  for (const auto& pair : key_cache_->key_map_) {
    if (pair.second.time_ < min_time) {
      min_time = pair.second.time_;
      oldest_key = pair.first;
    }
  }
  key_cache_->add_key("new_key", "new_value");
  EXPECT_EQ(key_cache_->key_map_.count(oldest_key), 0);
  EXPECT_EQ(key_cache_->key_map_.size(), MAX_KEY_MAP_SIZE);
}

TEST_F(KeyCacheTest, hex_repr) {
  auto result = _hex_repr("1");
  EXPECT_EQ(result, "31");
}

TEST_F(KeyCacheTest, hex_repr_empty) {
  auto result = _hex_repr("");
  EXPECT_EQ(result, "");
}

TEST_F(KeyCacheTest, hex_repr_multichar) {
  auto result = _hex_repr("ABC");
  EXPECT_EQ(result, "414243");
}

TEST_F(KeyCacheTest, generate_key) {
  std::string key_name = "name";
  auto result = _generate_key(key_name);
  EXPECT_FALSE(result.empty());
}

TEST_F(KeyCacheTest, get_random) {
  auto result = _get_random(0);
  EXPECT_TRUE(result.empty());
}

TEST_F(KeyCacheTest, get_random_success) {
  auto result = _get_random(16);
  EXPECT_EQ(result.size(), 16U);
}

TEST_F(KeyCacheTest, generate_key_for_ota) {
  std::string key_name = "name";
  auto result = _generate_key_for_ota(key_name);
  EXPECT_FALSE(result.empty());
}

TEST_F(KeyCacheTest, get_symmetric_key_256) {
  std::string key_name = "name";
  auto result = get_symmetric_key_256(key_name);
  EXPECT_FALSE(result.empty());
}

TEST_F(KeyCacheTest, get_symmetric_key_256_FromCache) {
  std::string key_name = "test_key";
  std::string key_value = "test_value";
  inst.Get().add_key(key_name, key_value);
  auto result = get_symmetric_key_256(key_name);
  EXPECT_EQ(result, key_value);
}

TEST_F(KeyCacheTest, get_symmetric_key_256_noAdd) {
  std::string key_name = "test_new_key";
  std::string key_value = _generate_key(key_name);
  auto result = get_symmetric_key_256(key_name);
  EXPECT_EQ(result, key_value);
}

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
TEST_F(KeyCacheTest, get_asset_handle_file_256) {
  std::string key_name = "name";
  auto result = get_asset_handle_file_256(key_name);
  EXPECT_FALSE(result.empty());
}
#endif

TEST_F(KeyCacheTest, get_iv) {
  auto result = get_iv(0);
  EXPECT_TRUE(result.empty());
}

TEST_F(KeyCacheTest, get_iv_success) {
  auto result = get_iv(12);
  EXPECT_EQ(result.size(), 12U);
}

TEST_F(KeyCacheTest, get_symmetric_key_256_for_ota_cached) {
  std::string key_name = "ota_test_key";
  std::string key_value = "ota_test_value";
  inst_ota.Get().add_key(key_name, key_value);
  auto result = get_symmetric_key_256_for_ota(key_name);
  EXPECT_EQ(result, key_value);
}

TEST_F(KeyCacheTest, get_symmetric_key_256_for_ota_not_cached) {
  std::string key_name = "ota_new_key";
  auto result = get_symmetric_key_256_for_ota(key_name);
  EXPECT_TRUE(result.empty());
}

TEST_F(KeyCacheTest, generate_key_for_ota_not_empty) {
  std::string key_name = "ota_gen_key";
  auto result = _generate_key_for_ota(key_name);
  EXPECT_TRUE(result.empty());
}

TEST_F(KeyCacheTest, generate_key_not_empty) {
  std::string key_name = "gen_key";
  auto result = _generate_key(key_name);
  EXPECT_FALSE(result.empty());
}

TEST_F(KeyCacheTest, hex_repr_long_string) {
  std::string input = "This is a longer string for testing";
  auto result = _hex_repr(input);
  EXPECT_EQ(result.size(), input.size() * 2);
}

TEST_F(KeyCacheTest, get_random_failure) {
  auto result = _get_random(16);
}

TEST_F(KeyCacheTest, GenerateLocalKey) {
  auto result = crypto::ohos::GenerateLocalKey(32);
  EXPECT_EQ(result.size(), 32U);
}

TEST_F(KeyCacheTest, GenerateLocalKey_failure) {
  auto result = crypto::ohos::GenerateLocalKey(0);
}

TEST_F(KeyCacheTest, GetKey_empty_path) {
  crypto::ohos::inst.Get().key_map_.clear();
  auto result = crypto::ohos::GetKey("");
}

TEST_F(KeyCacheTest, GetKeyForOta_empty_path) {
  crypto::ohos::inst_ota.Get().key_map_.clear();
  auto result = crypto::ohos::GetKeyForOta("");
}

TEST_F(KeyCacheTest, get_symmetric_key_256_for_ota_FromCache) {
  std::string key_name = "ota_cached_key";
  std::string key_value = "ota_cached_value";
  crypto::ohos::inst_ota.Get().add_key(key_name, key_value);
  auto result = crypto::ohos::get_symmetric_key_256_for_ota(key_name);
  EXPECT_EQ(result, key_value);
}

// Test KeyCache::get_key returns false when key not found and key_value is
// nullptr
TEST_F(KeyCacheTest, get_key_not_found_nullptr) {
  ASSERT_NE(key_cache_, nullptr);
  key_cache_->key_map_.clear();
  auto result = key_cache_->get_key("nonexistent", nullptr);
  EXPECT_FALSE(result);
}

// Test KeyCache LRU eviction: verify the newest key survives after overflow
TEST_F(KeyCacheTest, AddKeyLRUPreservesNewest) {
  std::string key_name = "lrutest";
  std::string key_value = "value";
  ASSERT_NE(key_cache_, nullptr);
  key_cache_->key_map_.clear();

  // Add 256 entries
  for (int i = 0; i < 256; i++) {
    key_cache_->key_map_.emplace(key_name + std::to_string(i), key_value);
  }

  // Access the last key to refresh its time
  key_cache_->get_key(key_name + "255", nullptr);

  // Add one more key to trigger eviction
  key_cache_->add_key("overflow_key", "overflow_value");

  // The overflow_key should exist
  EXPECT_EQ(key_cache_->key_map_.count("overflow_key"), 1u);
  // The recently accessed key should survive
  EXPECT_EQ(key_cache_->key_map_.count(key_name + "255"), 1u);
  // Map should remain at max size
  EXPECT_EQ(key_cache_->key_map_.size(), MAX_KEY_MAP_SIZE);
}

// Test get_iv with various sizes
TEST_F(KeyCacheTest, get_iv_various_sizes) {
  auto result8 = get_iv(8);
  EXPECT_EQ(result8.size(), 8U);

  auto result16 = get_iv(16);
  EXPECT_EQ(result16.size(), 16U);
}

// Test get_symmetric_key_256 returns consistent results on repeated calls
TEST_F(KeyCacheTest, get_symmetric_key_256_consistent) {
  std::string key_name = "consistent_test_key";
  std::string result1 = get_symmetric_key_256(key_name);
  std::string result2 = get_symmetric_key_256(key_name);
  EXPECT_EQ(result1, result2);
}

// Test that add_key overwrites existing key with updated time
TEST_F(KeyCacheTest, add_key_overwrite_updates_time) {
  std::string key_name = "overwrite_key";
  std::string key_value1 = "value1";
  std::string key_value2 = "value2";
  ASSERT_NE(key_cache_, nullptr);
  key_cache_->key_map_.clear();

  key_cache_->add_key(key_name, key_value1);
  uint64_t time1 = key_cache_->key_map_[key_name].time_;

  key_cache_->add_key(key_name, key_value2);
  uint64_t time2 = key_cache_->key_map_[key_name].time_;

  EXPECT_GT(time2, time1);
  EXPECT_EQ(key_cache_->key_map_[key_name].value_, key_value2);
}

}  // namespace ohos
}  // namespace crypto
