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

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "arkweb/build/features/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "ui/gfx/geometry/size.h"

#define private public
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#undef private

namespace blink {

class SystemClipboardForIncludeTest : public testing::Test {
 public:
  SystemClipboardForIncludeTest() {
    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(1, 1));
    clipboard_provider_ =
        std::make_unique<PageTestBase::MockClipboardHostProvider>(
            page_holder_.get()->GetFrame().GetBrowserInterfaceBroker());
  }

 protected:
  SystemClipboard& system_clipboard() {
    return *(page_holder_.get()->GetFrame().GetSystemClipboard());
  }

  void RunUntilIdle() { test::RunPendingTasks(); }

 private:
  test::TaskEnvironment task_environment;
  std::unique_ptr<DummyPageHolder> page_holder_;
  std::unique_ptr<PageTestBase::MockClipboardHostProvider> clipboard_provider_;
};

TEST_F(SystemClipboardForIncludeTest, HandlePasswordVault_Unbound_ReturnsFalse) {
  // Reset mojo remote to unbound so the early-return branch is taken.
  system_clipboard().clipboard_.reset();
  EXPECT_FALSE(system_clipboard().clipboard_.is_bound());
  EXPECT_FALSE(system_clipboard().HandlePasswordVault());
}

TEST_F(SystemClipboardForIncludeTest, HandlePasswordVault_Bound_ReturnsMockResult) {
  // The mock host is bound by the provider, so the remote is bound and the
  // mock HandlePasswordVault returns false.
  EXPECT_TRUE(system_clipboard().clipboard_.is_bound());
  EXPECT_FALSE(system_clipboard().HandlePasswordVault());
}

}  // namespace blink
