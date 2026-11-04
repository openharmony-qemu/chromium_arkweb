// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/ohos/mock_native_image_texture_owner.h"

namespace gpu {

using testing::_;
using testing::Return;

MockNativeImageTextureOwner::MockNativeImageTextureOwner()
    : NativeImageTextureOwner() {
  ON_CALL(*this, UpdateNativeImage())
      .WillByDefault([this]() {
        if (frame_available_callback_) {
          frame_available_callback_.Run();
        }
      });
  ON_CALL(*this, SetFrameAvailableCallback(_))
      .WillByDefault([this](const base::RepeatingClosure& cb) {
        frame_available_callback_ = cb;
        if (cb) {
          cb.Run();
        }
      });
  ON_CALL(*this, RunWhenBufferIsAvailable(_))
      .WillByDefault([](base::OnceClosure cb) { std::move(cb).Run(); });
}

MockNativeImageTextureOwner::~MockNativeImageTextureOwner() {};

}  // namespace gpu
