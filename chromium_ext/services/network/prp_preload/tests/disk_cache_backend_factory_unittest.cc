/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "testing/gtest/include/gtest/gtest.h"

#include "arkweb/chromium_ext/services/network/prp_preload/src/disk_cache_backend_factory.h"
#include "base/test/task_environment.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "net/disk_cache/mock/mock_backend_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ohos_prp_preload {

class DiskCacheBackendFactoryTest : public testing::Test {
public:
  static void SetUpTestCase();
  static void TearDownTestCase();
  void SetUp() override;
  void TearDown() override;
protected:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

void DiskCacheBackendFactoryTest::SetUpTestCase() {}

void DiskCacheBackendFactoryTest::TearDownTestCase() {}

void DiskCacheBackendFactoryTest::SetUp() {
  task_environment_ = std::make_unique<base::test::TaskEnvironment>(
    base::test::TaskEnvironment::MainThreadType::DEFAULT);
}

void DiskCacheBackendFactoryTest::TearDown() {
  task_environment_.reset();
}

TEST_F(DiskCacheBackendFactoryTest, CreateBackendTest) {
  DiskCacheBackendFactory factory;
  factory.CreateBackend();
  EXPECT_FALSE(factory.is_inited_.load());
}

TEST_F(DiskCacheBackendFactoryTest, CheckBackendAsyncTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(true);
  bool result = factory.CheckBackendAsync(base::BindOnce([](){}));
  EXPECT_TRUE(result);
  
  factory.is_inited_.store(false);
  result = factory.CheckBackendAsync(base::BindOnce([](){}));
  EXPECT_FALSE(result);
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 1);
}

TEST_F(DiskCacheBackendFactoryTest, CacheCreatedCallbackTest) {
  DiskCacheBackendFactory factory;
  disk_cache::BackendResult result;
  result.net_error = net::OK;
  result.backend = nullptr;
  factory.CacheCreatedCallback(std::move(result));
  EXPECT_TRUE(factory.is_inited_.load());
  
  disk_cache::BackendResult result2;
  result2.net_error = net::ERR_FAILED;
  result2.backend = nullptr;
  factory.CacheCreatedCallback(std::move(result2));
}

TEST_F(DiskCacheBackendFactoryTest, CacheCreatedCallbackWithCallbackListTest) {
  DiskCacheBackendFactory factory;
  factory.backend_complete_callback_list_.push_back(base::BindOnce([](){}));
  factory.backend_complete_callback_list_.push_back(base::BindOnce([](){}));
  
  disk_cache::BackendResult result;
  result.net_error = net::OK;
  result.backend = nullptr;
  factory.CacheCreatedCallback(std::move(result));
  EXPECT_TRUE(factory.is_inited_.load());
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

TEST_F(DiskCacheBackendFactoryTest, RemoveCacheTest) {
  DiskCacheBackendFactory factory;
  factory.RemoveCache(base::Time::Now(), base::Time::Max());
  EXPECT_FALSE(factory.is_inited_.load());
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

TEST_F(DiskCacheBackendFactoryTest, RemoveCacheWithBackendTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(true);
  factory.backend_complete_callback_list_.push_back(base::BindOnce([](){}));
  factory.RemoveCache(base::Time::Now(), base::Time::Max());
  EXPECT_FALSE(factory.is_inited_.load());
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

TEST_F(DiskCacheBackendFactoryTest, RemoveCacheWithTimeRangeTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(true);
  factory.RemoveCache(base::Time::Now() - base::Days(10), base::Time::Now());
  EXPECT_FALSE(factory.is_inited_.load());
}

TEST_F(DiskCacheBackendFactoryTest, ClearPreloadCacheDoneTest) {
  DiskCacheBackendFactory factory;
  factory.backend_complete_callback_list_.push_back(base::BindOnce([](){}));
  factory.backend_complete_callback_list_.push_back(base::BindOnce([](){}));
  factory.ClearPreloadCacheDone(net::OK);
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

TEST_F(DiskCacheBackendFactoryTest, RemoveCacheNullStartTimeTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(true);
  factory.RemoveCache(base::Time(), base::Time::Max());
  EXPECT_FALSE(factory.is_inited_.load());
}

TEST_F(DiskCacheBackendFactoryTest, ClearPreloadCacheDoneWithErrorTest) {
  DiskCacheBackendFactory factory;
  factory.backend_complete_callback_list_.push_back(base::BindOnce([](){}));
  factory.ClearPreloadCacheDone(net::ERR_FAILED);
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

TEST_F(DiskCacheBackendFactoryTest, IsInitedAtomicTest) {
  DiskCacheBackendFactory factory;
  EXPECT_FALSE(factory.is_inited_.load());
  
  factory.is_inited_.store(true);
  EXPECT_TRUE(factory.is_inited_.load());
  
  factory.is_inited_.store(false);
  EXPECT_FALSE(factory.is_inited_.load());
}

TEST_F(DiskCacheBackendFactoryTest, CacheCreatedCallbackErrorTest) {
  DiskCacheBackendFactory factory;
  factory.backend_complete_callback_list_.push_back(base::BindOnce([](){}));

  disk_cache::BackendResult result;
  result.net_error = net::ERR_FAILED;
  result.backend = nullptr;
  factory.CacheCreatedCallback(std::move(result));
  EXPECT_FALSE(factory.is_inited_.load());
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 1);
}

TEST_F(DiskCacheBackendFactoryTest, CheckBackendAsyncMultipleCallbacksTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(false);

  int callback_count = 0;

  EXPECT_FALSE(factory.CheckBackendAsync(base::BindOnce(
      [](int* count) { (*count)++; }, &callback_count)));
  EXPECT_FALSE(factory.CheckBackendAsync(base::BindOnce(
      [](int* count) { (*count)++; }, &callback_count)));
  EXPECT_FALSE(factory.CheckBackendAsync(base::BindOnce(
      [](int* count) { (*count)++; }, &callback_count)));

  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 3);
  EXPECT_EQ(callback_count, 0);
}

TEST_F(DiskCacheBackendFactoryTest, CacheCreatedCallbackRunsMultipleCallbacksTest) {
  DiskCacheBackendFactory factory;

  int callback_count = 0;

  factory.backend_complete_callback_list_.push_back(base::BindOnce(
      [](int* count) { (*count)++; }, &callback_count));
  factory.backend_complete_callback_list_.push_back(base::BindOnce(
      [](int* count) { (*count)++; }, &callback_count));
  factory.backend_complete_callback_list_.push_back(base::BindOnce(
      [](int* count) { (*count)++; }, &callback_count));

  disk_cache::BackendResult result;
  result.net_error = net::OK;
  result.backend = nullptr;
  factory.CacheCreatedCallback(std::move(result));

  EXPECT_TRUE(factory.is_inited_.load());
  EXPECT_EQ(callback_count, 3);
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

TEST_F(DiskCacheBackendFactoryTest, RemoveCacheWithEmptyTimeRangeTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(true);

  base::Time now = base::Time::Now();
  factory.RemoveCache(now, now);
  EXPECT_FALSE(factory.is_inited_.load());
}

TEST_F(DiskCacheBackendFactoryTest, RemoveCacheWithFutureEndTimeTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(true);

  base::Time now = base::Time::Now();
  base::Time future = now + base::Days(30);
  factory.RemoveCache(now, future);
  EXPECT_FALSE(factory.is_inited_.load());
}

TEST_F(DiskCacheBackendFactoryTest, BackendReturnsCorrectValueTest) {
  DiskCacheBackendFactory factory;
  EXPECT_EQ(factory.Backend(), nullptr);

  factory.is_inited_.store(true);
  EXPECT_EQ(factory.Backend(), nullptr);
}

TEST_F(DiskCacheBackendFactoryTest, CreateBackendNotInitedInitiallyTest) {
  DiskCacheBackendFactory factory;
  EXPECT_FALSE(factory.is_inited_.load());
  EXPECT_EQ(factory.cache_path_.value().size(), 0);

  factory.CreateBackend();
  EXPECT_FALSE(factory.is_inited_.load());
}

TEST_F(DiskCacheBackendFactoryTest, CheckBackendAsyncNotInitedStillReturnsFalseTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(false);

  bool result = factory.CheckBackendAsync(base::BindOnce([]() {}));
  EXPECT_FALSE(result);
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 1);

  result = factory.CheckBackendAsync(base::BindOnce([]() {}));
  EXPECT_FALSE(result);
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 2);
}

TEST_F(DiskCacheBackendFactoryTest, RemoveCacheNotInitedClearsCallbacksImmediatelyTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(false);
  factory.backend_complete_callback_list_.push_back(base::BindOnce([]() {}));
  factory.backend_complete_callback_list_.push_back(base::BindOnce([]() {}));

  factory.RemoveCache(base::Time::Now(), base::Time::Max());

  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

TEST_F(DiskCacheBackendFactoryTest, CallbackListHoldsMoveOnlyCallbacksTest) {
  DiskCacheBackendFactory factory;

  int value = 0;
  auto callback = base::BindOnce([](int* ptr) { (*ptr)++; }, &value);

  factory.backend_complete_callback_list_.push_back(std::move(callback));
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 1);

  disk_cache::BackendResult result;
  result.net_error = net::OK;
  result.backend = nullptr;
  factory.CacheCreatedCallback(std::move(result));

  EXPECT_EQ(value, 1);
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

TEST_F(DiskCacheBackendFactoryTest, CheckBackendAsyncInitedReturnsTrueImmediatelyTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(true);

  bool result = factory.CheckBackendAsync(base::BindOnce([]() {}));
  EXPECT_TRUE(result);
  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

TEST_F(DiskCacheBackendFactoryTest, RemoveCacheClearsCallbackListAfterDoomAllEntriesTest) {
  DiskCacheBackendFactory factory;
  factory.is_inited_.store(true);
  factory.backend_complete_callback_list_.push_back(base::BindOnce([]() {}));
  factory.backend_complete_callback_list_.push_back(base::BindOnce([]() {}));

  factory.RemoveCache(base::Time(), base::Time::Max());

  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

// Inject a real Backend so RemoveCache exercises the DoomAllEntries /
// DoomEntriesBetween paths instead of the null-backend early return.
TEST_F(DiskCacheBackendFactoryTest, RemoveCacheWithInjectedBackendDoomAllEntriesTest) {
  DiskCacheBackendFactory factory;
  auto backend = std::make_unique<disk_cache::BackendMock>(net::DISK_CACHE);
  disk_cache::BackendMock* mock_backend = backend.get();
  factory.backend_ = std::move(backend);
  factory.is_inited_.store(true);

  EXPECT_CALL(*mock_backend, DoomAllEntries(testing::_))
      .WillOnce(testing::Return(net::ERR_IO_PENDING));

  factory.RemoveCache(base::Time(), base::Time::Max());

  EXPECT_TRUE(factory.is_inited_.load());
}

TEST_F(DiskCacheBackendFactoryTest, RemoveCacheWithInjectedBackendDoomEntriesBetweenTest) {
  DiskCacheBackendFactory factory;
  auto backend = std::make_unique<disk_cache::BackendMock>(net::DISK_CACHE);
  disk_cache::BackendMock* mock_backend = backend.get();
  factory.backend_ = std::move(backend);
  factory.is_inited_.store(true);

  const base::Time start = base::Time::Now() - base::Days(10);
  const base::Time end = base::Time::Now();
  EXPECT_CALL(*mock_backend, DoomEntriesBetween(start, end, testing::_))
      .WillOnce(testing::Return(net::ERR_IO_PENDING));

  factory.RemoveCache(start, end);

  EXPECT_TRUE(factory.is_inited_.load());
}

// A synchronous Doom result must fall through to the posted
// ClearPreloadCacheDone task and drop the queued callbacks.
TEST_F(DiskCacheBackendFactoryTest, RemoveCacheSyncResultClearsCallbacksTest) {
  DiskCacheBackendFactory factory;
  auto backend = std::make_unique<disk_cache::BackendMock>(net::DISK_CACHE);
  disk_cache::BackendMock* mock_backend = backend.get();
  factory.backend_ = std::move(backend);
  factory.is_inited_.store(true);
  factory.backend_complete_callback_list_.push_back(base::BindOnce([]() {}));

  EXPECT_CALL(*mock_backend, DoomAllEntries(testing::_))
      .WillOnce(testing::Return(net::OK));

  factory.RemoveCache(base::Time(), base::Time::Max());
  task_environment_->RunUntilIdle();

  EXPECT_EQ(factory.backend_complete_callback_list_.size(), 0);
}

// CreateBackend must clear an index file older than 7 days before creating
// the backend.
TEST_F(DiskCacheBackendFactoryTest, CreateBackendRemovesStaleIndexTest) {
  base::FilePath cache_root;
  ASSERT_TRUE(base::PathService::Get(base::DIR_CACHE, &cache_root));
  const base::FilePath cache_dir = cache_root.AppendASCII("PreloadCache");
  const base::FilePath index_file = cache_dir.AppendASCII("index");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  {
    base::File index(index_file, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(index.IsValid());
  }
  ASSERT_TRUE(base::TouchFile(index_file,
                             base::Time::Now() - base::Days(10),
                             base::Time::Now() - base::Days(10)));

  DiskCacheBackendFactory factory;
  factory.CreateBackend();

  EXPECT_FALSE(base::PathExists(index_file));
}

} // namespace ohos_prp_preload
