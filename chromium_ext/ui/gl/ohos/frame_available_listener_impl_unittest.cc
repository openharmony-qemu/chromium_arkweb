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

#include "arkweb/chromium_ext/ui/gl/ohos/frame_available_listener_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace OHOS::NWeb {
namespace {

// The FrameAvailableListenerImpl business logic only invokes the callback
// synchronously on the calling thread. No TaskEnvironment is needed (same as
// other tests in this target).
class FrameAvailableListenerImplTest : public testing::Test {};

// Verifies that the callback is invoked when OnFrameAvailableListener is
// called. The callback runs synchronously on the calling thread.
TEST_F(FrameAvailableListenerImplTest, CallbackInvokedOnSameThread) {
  int call_count = 0;
  auto cb = base::BindRepeating([](int* count) { ++(*count); },
                                base::Unretained(&call_count));
  auto listener = std::make_unique<FrameAvailableListenerImpl>(cb);
  ASSERT_NE(listener, nullptr);
  listener->OnFrameAvailableListener();
  EXPECT_EQ(call_count, 1);
}

// Verifies that OnFrameAvailableListener can be called multiple times, and
// the RepeatingClosure is invoked for each call.
TEST_F(FrameAvailableListenerImplTest, CallbackInvokedMultipleTimes) {
  int call_count = 0;
  auto cb = base::BindRepeating([](int* count) { ++(*count); },
                                base::Unretained(&call_count));
  auto listener = std::make_unique<FrameAvailableListenerImpl>(cb);
  ASSERT_NE(listener, nullptr);
  listener->OnFrameAvailableListener();
  listener->OnFrameAvailableListener();
  listener->OnFrameAvailableListener();
  EXPECT_EQ(call_count, 3);
}

// Verifies that constructing the listener with a null callback does not
// crash when OnFrameAvailableListener is invoked. The implementation guards
// against null callbacks via the `if (frame_available_cb_)` check.
TEST_F(FrameAvailableListenerImplTest, NullCallbackDoesNotCrash) {
  base::RepeatingClosure null_cb;
  auto listener =
      std::make_unique<FrameAvailableListenerImpl>(std::move(null_cb));
  ASSERT_NE(listener, nullptr);
  // Should not crash.
  listener->OnFrameAvailableListener();
  SUCCEED() << "Null callback handled gracefully";
}

// Verifies that the listener can be destroyed while a callback is pending
// without causing use-after-free. This exercises the lifetime semantics of
// the FrameAvailableListenerImpl with respect to its captured callback.
TEST_F(FrameAvailableListenerImplTest, DestroyListenerWithoutInvocation) {
  int call_count = 0;
  auto cb = base::BindRepeating([](int* count) { ++(*count); },
                                base::Unretained(&call_count));
  auto listener = std::make_unique<FrameAvailableListenerImpl>(cb);
  ASSERT_NE(listener, nullptr);
  // Destroy the listener without ever calling OnFrameAvailableListener.
  listener.reset();
  EXPECT_EQ(call_count, 0);
}

// Verifies that the listener can be constructed and immediately destroyed.
TEST_F(FrameAvailableListenerImplTest, ConstructAndDestroy) {
  base::RepeatingClosure cb = base::DoNothing();
  auto listener = std::make_unique<FrameAvailableListenerImpl>(cb);
  ASSERT_NE(listener, nullptr);
  listener.reset();
}

// Verifies that the callback is invoked with the correct closure semantics:
// the RepeatingCallback can be invoked many times across multiple listener
// instances sharing the same callback.
TEST_F(FrameAvailableListenerImplTest, SharedCallbackAcrossInstances) {
  int call_count = 0;
  auto cb = base::BindRepeating([](int* count) { ++(*count); },
                                base::Unretained(&call_count));
  {
    auto listener1 = std::make_unique<FrameAvailableListenerImpl>(cb);
    listener1->OnFrameAvailableListener();
  }
  {
    auto listener2 = std::make_unique<FrameAvailableListenerImpl>(cb);
    listener2->OnFrameAvailableListener();
    listener2->OnFrameAvailableListener();
  }
  EXPECT_EQ(call_count, 3);
}

}  // namespace
}  // namespace OHOS::NWeb
