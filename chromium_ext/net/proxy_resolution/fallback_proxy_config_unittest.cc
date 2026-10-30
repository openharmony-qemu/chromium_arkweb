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

#include "arkweb/chromium_ext/net/proxy_resolution/fallback_proxy_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(FallbackProxyConfigStatusTest, ProxyConfigEnable_True) {
  FallbackProxyConfigStatus::SetStatus(true, false);
  EXPECT_EQ(FallbackProxyConfigStatus::ProxyConfigEnable(), 1);
}

TEST(FallbackProxyConfigStatusTest, ProxyConfigEnable_False) {
  FallbackProxyConfigStatus::SetStatus(false, false);
  EXPECT_EQ(FallbackProxyConfigStatus::ProxyConfigEnable(), 0);
}

TEST(FallbackProxyConfigStatusTest, HasToken_True) {
  FallbackProxyConfigStatus::SetStatus(false, true);
  EXPECT_EQ(FallbackProxyConfigStatus::HasToken(), 1);
}

TEST(FallbackProxyConfigStatusTest, HasToken_False) {
  FallbackProxyConfigStatus::SetStatus(false, false);
  EXPECT_EQ(FallbackProxyConfigStatus::HasToken(), 0);
}

TEST(FallbackProxyConfigStatusTest, SetStatus_ResetsBoth) {
  FallbackProxyConfigStatus::SetStatus(true, true);
  EXPECT_EQ(FallbackProxyConfigStatus::ProxyConfigEnable(), 1);
  EXPECT_EQ(FallbackProxyConfigStatus::HasToken(), 1);

  FallbackProxyConfigStatus::SetStatus(false, false);
  EXPECT_EQ(FallbackProxyConfigStatus::ProxyConfigEnable(), 0);
  EXPECT_EQ(FallbackProxyConfigStatus::HasToken(), 0);
}

TEST(FallbackProxyConfigStatusTest, ActiveOnlyMode_SetAndGet) {
  FallbackProxyConfigStatus::SetActiveOnlyMode(true);
  EXPECT_TRUE(FallbackProxyConfigStatus::IsActiveOnlyMode());

  FallbackProxyConfigStatus::SetActiveOnlyMode(false);
  EXPECT_FALSE(FallbackProxyConfigStatus::IsActiveOnlyMode());
}

}  // namespace net
