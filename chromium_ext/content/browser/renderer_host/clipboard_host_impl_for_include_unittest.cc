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
#include "base/test/test_future.h"

namespace content {

TEST_F(ClipboardHostImplWriteTest, OnClipboardDataGuard) {
  base::test::TestFuture<void> future;
  OnClipboardDataGuard(true, future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(ClipboardHostImplWriteTest, HandlePasswordVaultWithContents) {
  base::test::TestFuture<bool> future;
  HandlePasswordVault(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(ClipboardHostImplWriteTest, HandlePasswordVaultWithoutContents) {
  NavigateAndCommit(GURL("about:blank"));
  base::test::TestFuture<bool> future;
  HandlePasswordVault(future.GetCallback());
  EXPECT_TRUE(future.Take());
}

}  // namespace content
