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

#include <sys/mman.h>

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#include "arkweb/ohos_adapter_ndk/utils/include/ashmem_adapter.h"
#undef private
using namespace OHOS::NWeb;

namespace OHOS::NWeb {

class AshmemTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(AshmemTest, ShmemAdapterClose001)
{
  ASSERT_NO_FATAL_FAILURE(ShmemAdapterClose(0));

  char* name = nullptr;
  int fd = ShmemAdapterCreate(name, 1);
  ASSERT_NO_FATAL_FAILURE(ShmemAdapterClose(fd));
}

TEST_F(AshmemTest, ShmemAdapterCreate001)
{
  char name[] = "abc";
  int result = ShmemAdapterCreate(name, -1);
  EXPECT_EQ(result, -1);
}

TEST_F(AshmemTest, ShmemAdapterCreate002) {
  char* name = nullptr;
  int result = ShmemAdapterCreate(name, 1);
  ASSERT_GT(result, 0);
  ShmemAdapterClose(result);
}

TEST_F(AshmemTest, ShmemAdapterCreate003) {
  char name[] = "abc";
  int result = ShmemAdapterCreate(name, 1);
  ASSERT_GT(result, 0);
  ShmemAdapterClose(result);
}

TEST_F(AshmemTest, ShmemAdapterMap001) {
  constexpr int kRegionSize = 4096;
  int fd = ShmemAdapterCreate("ashmem-map-test", kRegionSize);
  ASSERT_GT(fd, 0);
  EXPECT_EQ(ShmemAdapterGetSize(fd), kRegionSize);
  EXPECT_EQ(ShmemAdapterSetProt(fd, PROT_READ | PROT_WRITE), 0);
  EXPECT_EQ(ShmemAdapterGetProt(fd), PROT_READ | PROT_WRITE);

  void* result = ShmemAdapterMap(fd, PROT_READ | PROT_WRITE);
  ASSERT_NE(result, MAP_FAILED);
  static_cast<char*>(result)[0] = 'A';
  EXPECT_EQ(static_cast<char*>(result)[0], 'A');
  ShmemAdapterUnmap(result, kRegionSize);
  ShmemAdapterClose(fd);
}

TEST_F(AshmemTest, ShmemAdapterCreateRejectsOverlongName) {
  const std::string name(1024, 'a');
  EXPECT_EQ(ShmemAdapterCreate(name.c_str(), 1), -1);
}

TEST_F(AshmemTest, ShmemAdapterUnmap001) {
  char* name = nullptr;
  ASSERT_NO_FATAL_FAILURE(ShmemAdapterUnmap(name, 1));

  char str[] = "abc";
  ASSERT_NO_FATAL_FAILURE(ShmemAdapterUnmap(str, 1));
}

TEST_F(AshmemTest, InvalidFileDescriptorOperations) {
  EXPECT_EQ(ShmemAdapterSetProt(-1, PROT_READ), -1);
  EXPECT_EQ(ShmemAdapterGetSize(-1), -1);
  EXPECT_EQ(ShmemAdapterGetProt(-1), -1);
  EXPECT_EQ(ShmemAdapterMap(-1, PROT_READ), MAP_FAILED);
  EXPECT_NO_FATAL_FAILURE(ShmemAdapterClose(-1));
}
}
