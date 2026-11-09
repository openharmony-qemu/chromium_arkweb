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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "arkweb/build/features/features.h"
#include "mock_nweb_delegate.h"
#define private public
#include "nweb_impl.h"
#undef private

namespace OHOS::NWeb {

class NWebNavigationEntryUserDataTest : public testing::Test {
 public:
  void SetUp() override {
    mock_delegate_ = std::make_shared<MockNWebDelegate>();
    nweb_impl_ = std::make_shared<NWebImpl>(id++);
    ASSERT_NE(nweb_impl_, nullptr);
  }

  void TearDown() override {
    nweb_impl_ = nullptr;
    mock_delegate_ = nullptr;
  }

  uint32_t id = 1;
  std::shared_ptr<NWebImpl> nweb_impl_;
  std::shared_ptr<MockNWebDelegate> mock_delegate_;
};

#if BUILDFLAG(ARKWEB_NAVIGATION_ENTRY_USERDATA)

TEST_F(NWebNavigationEntryUserDataTest,
       NavEntryUserData_FlagOn_ForwardsToDelegate) {
  nweb_impl_->nweb_delegate_ = mock_delegate_;
  EXPECT_CALL(*mock_delegate_, SetNavigationEntryUserData("key", "v1"));
  nweb_impl_->SetNavigationEntryUserData("key", "v1");
  EXPECT_CALL(*mock_delegate_, RemoveNavigationEntryUserData("key"));
  nweb_impl_->RemoveNavigationEntryUserData("key");
}

TEST_F(NWebNavigationEntryUserDataTest,
       NavEntryUserData_FlagOn_GetReturnsDelegateValue) {
  nweb_impl_->nweb_delegate_ = mock_delegate_;
  EXPECT_CALL(*mock_delegate_, GetNavigationEntryUserData("key"))
      .WillOnce(testing::Return("v2"));
  EXPECT_EQ(nweb_impl_->GetNavigationEntryUserData("key"), "v2");
}

TEST_F(NWebNavigationEntryUserDataTest,
       NavEntryUserData_FlagOn_NoDelegate_NoCrash) {
  nweb_impl_->nweb_delegate_ = nullptr;
  nweb_impl_->SetNavigationEntryUserData("key", "v1");
  EXPECT_EQ(nweb_impl_->GetNavigationEntryUserData("key"), "");
  nweb_impl_->RemoveNavigationEntryUserData("key");
}

#else  // !BUILDFLAG(ARKWEB_NAVIGATION_ENTRY_USERDATA)

TEST_F(NWebNavigationEntryUserDataTest,
       NavEntryUserData_FlagOff_GetReturnsEmpty) {
  nweb_impl_->nweb_delegate_ = mock_delegate_;
  EXPECT_EQ(nweb_impl_->GetNavigationEntryUserData("key"), "");
}

TEST_F(NWebNavigationEntryUserDataTest,
       NavEntryUserData_FlagOff_SetRemoveNoOp) {
  nweb_impl_->nweb_delegate_ = mock_delegate_;
  nweb_impl_->SetNavigationEntryUserData("key", "v1");
  nweb_impl_->RemoveNavigationEntryUserData("key");
  EXPECT_EQ(nweb_impl_->GetNavigationEntryUserData("key"), "");
}

#endif  // BUILDFLAG(ARKWEB_NAVIGATION_ENTRY_USERDATA)

}  // namespace OHOS::NWeb
