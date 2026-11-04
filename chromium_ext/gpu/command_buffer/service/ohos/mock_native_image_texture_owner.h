// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_OHOS_MOCK_NATIVE_IMAGE_TEXTURE_OWNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_OHOS_MOCK_NATIVE_IMAGE_TEXTURE_OWNER_H_

#include "gpu/command_buffer/service/ohos/native_image_texture_owner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

// Mock for NativeImageTextureOwner, similar to Android's MockTextureOwner.
// Uses the mock-only no-arg constructor that skips context_state_ and texture_
// initialization, avoiding null dereference in the base class destructor.
class MockNativeImageTextureOwner : public NativeImageTextureOwner {
 public:
  MockNativeImageTextureOwner();
  ~MockNativeImageTextureOwner() override;

  MOCK_METHOD(gl::GLContext*, GetContext, (), (const, override));
  MOCK_METHOD(gl::GLSurface*, GetSurface, (), (const, override));
  MOCK_METHOD(void*, AquireOhosNativeWindow, (), (const, override));
  MOCK_METHOD(void, UpdateNativeImage, (), (override));
  MOCK_METHOD(void, GetSurfaceId, (uint64_t*), (override));
  MOCK_METHOD(void, EnsureNativeImageBound, (GLuint), (override));
  MOCK_METHOD(void, ReleaseNativeImage, (), (override));
  MOCK_METHOD(bool,
              GetCodedSizeAndVisibleRect,
              (gfx::Size, gfx::Size*, gfx::Rect*),
              (override));
  MOCK_METHOD(void,
              SetFrameAvailableCallback,
              (const base::RepeatingClosure&),
              (override));
  MOCK_METHOD(void, RunWhenBufferIsAvailable, (base::OnceClosure), (override));
  MOCK_METHOD(void, ReleaseResources, (), (override));
  std::unique_ptr<ScopedNativeBufferFenceSync> GetNativeBuffer() override {
    get_native_buffer_count++;
    return nullptr;
  }
  int get_native_buffer_count = 0;

  base::RepeatingClosure frame_available_callback_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_OHOS_MOCK_NATIVE_IMAGE_TEXTURE_OWNER_H_
