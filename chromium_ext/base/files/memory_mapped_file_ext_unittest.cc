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

#include "arkweb/build/features/features.h"
#if BUILDFLAG(ARKWEB_UNITTESTS)
#define private public
#include "base/files/memory_mapped_file.h"
#include "arkweb/chromium_ext/base/files/memory_mapped_file_ext.h"
#undef private
#else  // ARKWEB_UNITTESTS
#include "base/files/memory_mapped_file.h"
#include "arkweb/chromium_ext/base/files/memory_mapped_file_ext.h"
#endif  // ARKWEB_UNITTESTS

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {

namespace {

#if BUILDFLAG(ARKWEB_UNITTESTS)

class MemoryMappedFileExtTest : public PlatformTest {
 protected:
  void SetUp() override { PlatformTest::SetUp(); }

  void TearDown() override { PlatformTest::TearDown(); }
};

class MockOhosFileMapper : public OHOS::NWeb::OhosFileMapper {
 public:
  int32_t GetFd() override { return 1; }
  int32_t GetOffset() override { return 1; }
  std::string GetFileName() override { return "name"; }
  bool IsCompressed() override { return true; }
  void* GetDataPtr() override { return nullptr; }
  size_t GetDataLen() override { return 1; }
  bool UnzipData(uint8_t** dest, size_t& len) override { return false; }
};

class MockOhosFileMapperUncompressed : public OHOS::NWeb::OhosFileMapper {
 public:
  int32_t GetFd() override { return 1; }
  int32_t GetOffset() override { return 1; }
  std::string GetFileName() override { return "name"; }
  bool IsCompressed() override { return false; }
  void* GetDataPtr() override { return data_; }
  size_t GetDataLen() override { return 2; }
  bool UnzipData(uint8_t** dest, size_t& len) override { return false; }

 private:
  uint8_t data_[2] = {0x01, 0x02};
};

TEST_F(MemoryMappedFileExtTest, SetOhosFileMapper_001)
{
  MemoryMappedFile memory_mapped_file_test;
  MockOhosFileMapper mock_ohos_file_mapper;
  std::shared_ptr<OHOS::NWeb::OhosFileMapper> mapper =
      std::make_shared<MockOhosFileMapper>(mock_ohos_file_mapper);
  uint8_t test[2] = {0x01, 0x02};
  memory_mapped_file_test.bytes_ = span<uint8_t>{test};
  memory_mapped_file_test.mapper_file_ext_.customizeData_ = true;
  memory_mapped_file_test.mapper_file_ext_.mapper_ = nullptr;
  memory_mapped_file_test.SetOhosFileMapper(mapper);
  EXPECT_EQ(memory_mapped_file_test.bytes_.empty(), false);
}

TEST_F(MemoryMappedFileExtTest, SetOhosFileMapper_002)
{
  MemoryMappedFile memory_mapped_file_test;
  MockOhosFileMapper mock_ohos_file_mapper;
  std::shared_ptr<OHOS::NWeb::OhosFileMapper> mapper =
      std::make_shared<MockOhosFileMapper>(mock_ohos_file_mapper);
  uint8_t test[2] = {0x01, 0x02};
  memory_mapped_file_test.bytes_ = span<uint8_t>{test};
  memory_mapped_file_test.mapper_file_ext_.customizeData_ = false;
  memory_mapped_file_test.mapper_file_ext_.mapper_ = nullptr;
  memory_mapped_file_test.SetOhosFileMapper(mapper);
  EXPECT_EQ(memory_mapped_file_test.bytes_.empty(), true);
}

TEST_F(MemoryMappedFileExtTest, SetOhosFileMapper_003)
{
  MemoryMappedFile memory_mapped_file_test;
  MockOhosFileMapper mock_ohos_file_mapper;
  std::shared_ptr<OHOS::NWeb::OhosFileMapper> mapper =
      std::make_shared<MockOhosFileMapper>(mock_ohos_file_mapper);
  uint8_t test[2] = {0x01, 0x02};
  memory_mapped_file_test.bytes_ = span<uint8_t>{test};
  memory_mapped_file_test.mapper_file_ext_.customizeData_ = true;
  memory_mapped_file_test.mapper_file_ext_.mapper_ = mapper;
  memory_mapped_file_test.SetOhosFileMapper(mapper);
  EXPECT_EQ(memory_mapped_file_test.bytes_.empty(), false);
}

TEST_F(MemoryMappedFileExtTest, ClearData_NotCustomized_CallsCallback)
{
  MemoryMappedFileExt memory_mapped_file_ext_test;
  bool callback_called = false;
  memory_mapped_file_ext_test.ClearData([&callback_called]() { callback_called = true; });
  EXPECT_TRUE(callback_called);
}

TEST_F(MemoryMappedFileExtTest, SetOhosFileMapper_EmptyBytes_NoCallback)
{
  MemoryMappedFileExt memory_mapped_file_ext_test;
  MockOhosFileMapper mock_ohos_file_mapper;
  std::shared_ptr<OHOS::NWeb::OhosFileMapper> mapper =
      std::make_shared<MockOhosFileMapper>(mock_ohos_file_mapper);
  bool callback_called = false;
  span<uint8_t> bytes;
  memory_mapped_file_ext_test.SetOhosFileMapper(mapper,
      [&callback_called]() { callback_called = true; }, bytes);
  EXPECT_FALSE(callback_called);
  EXPECT_EQ(memory_mapped_file_ext_test.customizeData_, true);
}

TEST_F(MemoryMappedFileExtTest, SetOhosFileMapper_NullMapper_ReturnsEarly)
{
  MemoryMappedFileExt memory_mapped_file_ext_test;
  std::shared_ptr<OHOS::NWeb::OhosFileMapper> mapper = nullptr;
  uint8_t test[2] = {0x01, 0x02};
  span<uint8_t> bytes{test};
  bool callback_called = false;
  memory_mapped_file_ext_test.SetOhosFileMapper(mapper,
      [&callback_called]() { callback_called = true; }, bytes);
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(memory_mapped_file_ext_test.customizeData_, false);
}

TEST_F(MemoryMappedFileExtTest, SetOhosFileMapper_NotCompressed_SetsBytes)
{
  MemoryMappedFileExt memory_mapped_file_ext_test;
  MockOhosFileMapperUncompressed mock_ohos_file_mapper;
  std::shared_ptr<OHOS::NWeb::OhosFileMapper> mapper =
      std::make_shared<MockOhosFileMapperUncompressed>(mock_ohos_file_mapper);
  uint8_t test[2] = {0x01, 0x02};
  span<uint8_t> bytes{test};
  memory_mapped_file_ext_test.SetOhosFileMapper(mapper,
      []() {}, bytes);
  EXPECT_EQ(bytes.size(), 2u);
}
#endif  // ARKWEB_UNITTESTS

}  // namespace

}  // namespace base