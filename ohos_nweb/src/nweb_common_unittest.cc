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

#include "nweb_common.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace testing;

class NWebCommonTest : public ::testing::Test {
 public:
  NWebCommonTest() = default;
  ~NWebCommonTest() override = default;

  void SetUp() override {
    freed_addr_ = nullptr;
    SetFreeMemoryFunc(nullptr);
  }

  void TearDown() override {
    SetFreeMemoryFunc(nullptr);
  }

  static void FreeMemoryCallback(void* addr) {
    freed_addr_ = addr;
  }

  static void* freed_addr_;
};

void* NWebCommonTest::freed_addr_ = nullptr;

TEST_F(NWebCommonTest, FreeExternalMemory_NullAddr_NoCallback) {
  SetFreeMemoryFunc(FreeMemoryCallback);
  FreeExternalMemory(nullptr);
  EXPECT_EQ(freed_addr_, nullptr);
}

TEST_F(NWebCommonTest, FreeExternalMemory_NoCallback_NoCrash) {
  SetFreeMemoryFunc(nullptr);
  char dummy = 'x';
  FreeExternalMemory(&dummy);
  EXPECT_EQ(freed_addr_, nullptr);
}

TEST_F(NWebCommonTest, FreeExternalMemory_WithCallback_Invoked) {
  SetFreeMemoryFunc(FreeMemoryCallback);
  char dummy = 'x';
  FreeExternalMemory(&dummy);
  EXPECT_EQ(freed_addr_, &dummy);
}

TEST_F(NWebCommonTest, FreeExternalMemory_ReplaceCallback_UsesNewest) {
  SetFreeMemoryFunc(FreeMemoryCallback);
  char dummy = 'x';
  SetFreeMemoryFunc(nullptr);
  FreeExternalMemory(&dummy);
  EXPECT_EQ(freed_addr_, nullptr);
}
