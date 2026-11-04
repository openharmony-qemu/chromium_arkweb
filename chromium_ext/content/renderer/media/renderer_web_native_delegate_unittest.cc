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

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "arkweb/chromium_ext/content/renderer/media/renderer_web_native_delegate.h"

namespace media {

using ::testing::StrictMock;

// blink::WebNativeDelegate::Observer implementation that records fan-out
// callbacks. The interface is declared in
// third_party/blink/public/platform/media/web_native_delegate.h.
class MockObserver : public blink::WebNativeDelegate::Observer {
 public:
  MOCK_METHOD(void, OnFrameHidden, ());
  MOCK_METHOD(void, OnFrameShown, ());
};

class RendererWebNativeDelegateTest : public testing::Test {
 protected:
  // TaskEnvironment provides the main-thread sequence required by
  // base::WeakPtrFactory's sequence checker.
  base::test::TaskEnvironment task_environment_;
};

// RenderFrameObserver's constructor explicitly documents that |render_frame|
// can be NULL on unit testing, so constructing with nullptr is safe. With a
// null render_frame, IsFrameHidden() short-circuits via the
// `render_frame() && ...` check and returns false.

TEST_F(RendererWebNativeDelegateTest, IsFrameHidden_NullRenderFrame_ReturnsFalse) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  EXPECT_FALSE(delegate->IsFrameHidden());
}

TEST_F(RendererWebNativeDelegateTest, AddObserver_ReturnsIncrementingIds) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  StrictMock<MockObserver> obs1;
  StrictMock<MockObserver> obs2;
  int id1 = delegate->AddObserver(&obs1);
  int id2 = delegate->AddObserver(&obs2);
  // base::IDMap generates ids starting from 1 and strictly increasing.
  EXPECT_GE(id1, 1);
  EXPECT_GT(id2, id1);
}

TEST_F(RendererWebNativeDelegateTest, RemoveObserver_ValidId_Removes) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  StrictMock<MockObserver> obs;
  int id = delegate->AddObserver(&obs);
  delegate->RemoveObserver(id);
  // After removal, WasHidden/WasShown must not dispatch to |obs|.
  EXPECT_CALL(obs, OnFrameHidden()).Times(0);
  EXPECT_CALL(obs, OnFrameShown()).Times(0);
  delegate->WasHidden();
  delegate->WasShown();
}

TEST_F(RendererWebNativeDelegateTest, WasHidden_FansOutToAllObservers) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  StrictMock<MockObserver> obs1;
  StrictMock<MockObserver> obs2;
  delegate->AddObserver(&obs1);
  delegate->AddObserver(&obs2);
  EXPECT_CALL(obs1, OnFrameHidden()).Times(1);
  EXPECT_CALL(obs2, OnFrameHidden()).Times(1);
  delegate->WasHidden();
}

TEST_F(RendererWebNativeDelegateTest, WasShown_FansOutToAllObservers) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  StrictMock<MockObserver> obs1;
  StrictMock<MockObserver> obs2;
  delegate->AddObserver(&obs1);
  delegate->AddObserver(&obs2);
  EXPECT_CALL(obs1, OnFrameShown()).Times(1);
  EXPECT_CALL(obs2, OnFrameShown()).Times(1);
  delegate->WasShown();
}

TEST_F(RendererWebNativeDelegateTest, WasHidden_NoObservers_NoCrash) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  EXPECT_NO_FATAL_FAILURE(delegate->WasHidden());
}

TEST_F(RendererWebNativeDelegateTest, WasShown_NoObservers_NoCrash) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  EXPECT_NO_FATAL_FAILURE(delegate->WasShown());
}

TEST_F(RendererWebNativeDelegateTest, GetWeakPtr_ValidBeforeDestroy) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  auto weak = delegate->GetWeakPtr();
  EXPECT_TRUE(weak);
  // Destroying the delegate invalidates the weak pointer.
  delegate.reset();
  EXPECT_FALSE(weak);
}

// OnDestruct() performs `delete this`, so the object must be heap-allocated
// and must not be touched afterwards. A WeakPtr captured before destruction
// is invalidated once OnDestruct runs.

TEST_F(RendererWebNativeDelegateTest,
       GetWeakPtr_InvalidatedAfterOnDestruct) {
  auto* delegate = new RendererWebNativeDelegate(nullptr);
  auto weak = delegate->GetWeakPtr();
  ASSERT_TRUE(weak);
  delegate->OnDestruct();  // deletes |delegate|
  EXPECT_FALSE(weak);
}

TEST_F(RendererWebNativeDelegateTest, OnDestruct_SelfDeletes) {
  auto* delegate = new RendererWebNativeDelegate(nullptr);
  auto weak = delegate->GetWeakPtr();
  ASSERT_TRUE(weak);
  // OnDestruct deletes itself; the WeakPtr must report invalidation proving
  // the destructor (and thus the self-delete) ran.
  EXPECT_NO_FATAL_FAILURE(delegate->OnDestruct());
  EXPECT_FALSE(weak);
}

// WasHidden/WasShown guard against nullptr entries in the observer map via
// `if (it.GetCurrentValue())`. base::IDMap defaults to allowing null data, so
// AddObserver(nullptr) injects a null entry that the fan-out must skip while
// still dispatching to real observers.

TEST_F(RendererWebNativeDelegateTest, WasHidden_NullObserverInMap_Skips) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  StrictMock<MockObserver> obs;
  delegate->AddObserver(&obs);
  delegate->AddObserver(nullptr);
  // Only the real observer is notified; the null entry is skipped.
  EXPECT_CALL(obs, OnFrameHidden()).Times(1);
  EXPECT_NO_FATAL_FAILURE(delegate->WasHidden());
}

TEST_F(RendererWebNativeDelegateTest, WasShown_NullObserverInMap_Skips) {
  auto delegate = std::make_unique<RendererWebNativeDelegate>(nullptr);
  StrictMock<MockObserver> obs;
  delegate->AddObserver(nullptr);
  delegate->AddObserver(&obs);
  EXPECT_CALL(obs, OnFrameShown()).Times(1);
  EXPECT_NO_FATAL_FAILURE(delegate->WasShown());
}

}  // namespace media
