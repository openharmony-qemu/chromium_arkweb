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

#include "content/renderer/media/ohos/arkweb_media_factory_ext.h"

#include <memory>
#include <utility>

#include "arkweb/build/features/features.h"
#include "arkweb/chromium_ext/content/renderer/media/renderer_web_native_delegate.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// ArkwebMediaFactoryExt inherits from MediaFactory, whose constructor only
// stores the RenderFrameImpl pointer and constructs an ArkwebMediaFactoryUtils.
// The MediaFactory destructor accesses RenderThreadImpl::current() only when
// decoder_factory_ is non-null; since we never call EnsureDecoderFactory(),
// constructing and destructing with a null render_frame is safe in UT.
class ArkwebMediaFactoryExtTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;

  static void DeleteExt(ArkwebMediaFactoryExt* ext) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
    delete ext;
#pragma clang diagnostic pop
  }

  static void CleanupDelegate(media::RendererWebNativeDelegate* delegate) {
    if (delegate) {
      delegate->OnDestruct();  // deletes delegate
    }
  }
};

// AsArkwebMediaFactoryExt() returns `this` for an ArkwebMediaFactoryExt
// instance, and nullptr for a plain MediaFactory. Verify the override returns
// the derived instance.
TEST_F(ArkwebMediaFactoryExtTest, AsArkwebMediaFactoryExt_ReturnsThis) {
  auto* ext =
      new ArkwebMediaFactoryExt(nullptr, media::RequestRoutingTokenCallback());
  EXPECT_EQ(ext->AsArkwebMediaFactoryExt(), ext);
  DeleteExt(ext);
}

// GetDeviceScaleFactor() returns 1.0f when render_frame_ is null. Otherwise it
// forwards to render_frame_->GetDeviceScaleFactor(), which requires a real
// RenderFrameImpl. The null-branch is the only branch testable in UT.
TEST_F(ArkwebMediaFactoryExtTest, GetDeviceScaleFactor_NullRenderFrame) {
  auto* ext =
      new ArkwebMediaFactoryExt(nullptr, media::RequestRoutingTokenCallback());
  EXPECT_FLOAT_EQ(ext->GetDeviceScaleFactor(), 1.0f);
  DeleteExt(ext);
}

// GetWebNativeDelegate() lazily constructs a RendererWebNativeDelegate on the
// first call and returns the same weak pointer on subsequent calls. The
// RendererWebNativeDelegate constructor accepts a null RenderFrame (it only
// stores the pointer for later IsHidden checks), so this is safe in UT.
TEST_F(ArkwebMediaFactoryExtTest, GetWebNativeDelegate_LazilyCreates) {
  auto* ext =
      new ArkwebMediaFactoryExt(nullptr, media::RequestRoutingTokenCallback());
  auto weak1 = ext->GetWebNativeDelegate();
  EXPECT_TRUE(weak1);
  // Second call returns the same delegate instance (not a new one).
  auto weak2 = ext->GetWebNativeDelegate();
  EXPECT_EQ(weak1.get(), weak2.get());
  CleanupDelegate(weak1.get());
  DeleteExt(ext);
}

// The WeakPtr returned by GetWebNativeDelegate is valid before the delegate is
// destroyed and invalidated after OnDestruct (self-delete).
TEST_F(ArkwebMediaFactoryExtTest, GetWebNativeDelegate_WeakPtrInvalidated) {
  auto* ext =
      new ArkwebMediaFactoryExt(nullptr, media::RequestRoutingTokenCallback());
  auto weak = ext->GetWebNativeDelegate();
  ASSERT_TRUE(weak);
  auto* delegate = weak.get();
  delegate->OnDestruct();  // deletes delegate
  EXPECT_FALSE(weak);
  DeleteExt(ext);
}

// Destroying ArkwebMediaFactoryExt after GetWebNativeDelegate was called (but
// delegate cleaned up) must not crash. The MediaFactory destructor only touches
// decoder_factory_, which is null here.
TEST_F(ArkwebMediaFactoryExtTest, Destructor_AfterDelegateCleanup_NoCrash) {
  auto* ext =
      new ArkwebMediaFactoryExt(nullptr, media::RequestRoutingTokenCallback());
  auto weak = ext->GetWebNativeDelegate();
  ASSERT_TRUE(weak);
  CleanupDelegate(weak.get());
  EXPECT_NO_FATAL_FAILURE(DeleteExt(ext));
}
}  // namespace content
