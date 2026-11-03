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

#include <memory>
#include <ranges>

#include "nweb_common.cc"
#include "nweb_common.h"

#define private public
#include "capi/nweb_notification_options_item.h"
#include "nweb_notification_manager_delegate.cc"
#include "nweb_notification_manager_delegate.h"
#undef private

namespace OHOS::NWeb {

class NWebNotificationManagerDelegateTest : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(NWebNotificationManagerDelegateTest, GetOrCreateDelegateTest) {
  auto& delegate = NWebNotificationManagerDelegate::GetOrCreateDelegate();
  EXPECT_NE(delegate, nullptr);
}

TEST_F(NWebNotificationManagerDelegateTest,
       NWebNotificationManagerDelegateTest) {
  std::unique_ptr<NWebNotificationManagerDelegate> delegate;
  delegate = std::make_unique<NWebNotificationManagerDelegate>();
  EXPECT_NE(delegate, nullptr);
}

TEST_F(NWebNotificationManagerDelegateTest,
       RegisterNotificationManagerDelegateListenerTest_1stIf) {
  auto delegate = std::make_unique<NWebNotificationManagerDelegate>();
  EXPECT_NE(delegate, nullptr);
  auto listener = std::make_shared<NWebNotificationDelegateCallback>();
  delegate->RegisterNotificationManagerDelegateListener(listener);
  EXPECT_NE(NWebNotificationManagerDelegate::notification_delegate_listener_,
            nullptr);
}

TEST_F(NWebNotificationManagerDelegateTest,
       RegisterNotificationManagerDelegateListenerTest_2ndIf) {
  auto delegate = std::make_unique<NWebNotificationManagerDelegate>();
  EXPECT_NE(delegate, nullptr);
  NWebNotificationManagerDelegate::notification_delegate_listener_ =
      std::make_shared<NWebNotificationDelegateCallback>();
  delegate->RegisterNotificationManagerDelegateListener(nullptr);
  EXPECT_NE(NWebNotificationManagerDelegate::notification_delegate_listener_,
            nullptr);
}

TEST_F(NWebNotificationManagerDelegateTest, OnShowNotificationTest_1stIf) {
  std::shared_ptr<NWebNotificationOptionsItem> options;
  NWebNotificationManagerDelegate::OnShowNotification(options);
}

TEST_F(NWebNotificationManagerDelegateTest, OnCloseNotificationTest_1stIf) {
  std::string id = "123";
  NWebNotificationManagerDelegate::OnCloseNotification(id);
}

TEST_F(NWebNotificationManagerDelegateTest, OnShowedTest) {
  const std::string id = "123";
  NWebNotificationManagerDelegate::OnShowed(id);
}

TEST_F(NWebNotificationManagerDelegateTest, OnClosedTest) {
  const std::string id = "123";
  NWebNotificationManagerDelegate::OnClosed(id);
}

TEST_F(NWebNotificationManagerDelegateTest, OnClickedTest) {
  const std::string id = "123";
  int buttonIndex = 123;
  NWebNotificationManagerDelegate::OnClicked(id, buttonIndex);
}
}  // namespace OHOS::NWeb
