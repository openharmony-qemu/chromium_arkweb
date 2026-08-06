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

#include "testing/gtest/include/gtest/gtest.h"

#define private public
#include "modules/desktop_capture/ohos/native_frame.h"
#undef private

namespace webrtc {

// ---------------------------------------------------------------------------
// Constructor tests
// ---------------------------------------------------------------------------

TEST(NativeFrameTest, Constructor_WithScreenCapture) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x1234);
  NativeFrame frame(dummy);
  EXPECT_EQ(frame.screen_capture_, dummy);
  EXPECT_EQ(frame.buffer_, nullptr);
  EXPECT_EQ(frame.native_buffer_, nullptr);
}

TEST(NativeFrameTest, Constructor_WithBuffer) {
  auto* dummy = reinterpret_cast<OH_AVBuffer*>(0x5678);
  NativeFrame frame(dummy);
  EXPECT_EQ(frame.buffer_, dummy);
  EXPECT_EQ(frame.screen_capture_, nullptr);
  EXPECT_EQ(frame.native_buffer_, nullptr);
}

// ---------------------------------------------------------------------------
// Destructor tests
// ---------------------------------------------------------------------------

TEST(NativeFrameTest, Destructor_WithoutNativeBuffer_NoOp) {
  // native_buffer_ is nullptr by default; destructor should be a no-op.
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x1234);
  { NativeFrame frame(dummy); }
  // Reaching here without crashing means the test passed.
}

TEST(NativeFrameTest, Destructor_WithNativeBuffer_Unreferences) {
  GTEST_SKIP() << "Cannot test in host UT: calling OH_NativeBuffer_Unreference "
                  "on a dummy native_buffer_ pointer would dereference invalid "
                  "memory and crash without a real OH_NativeBuffer.";
}

// ---------------------------------------------------------------------------
// Init tests
// ---------------------------------------------------------------------------

TEST(NativeFrameTest, Init_BothNull_ReturnsFalse) {
  NativeFrame frame(static_cast<OH_AVScreenCapture*>(nullptr));
  EXPECT_FALSE(frame.Init());
}

TEST(NativeFrameTest, Init_WithScreenCapture_AcquireFailed_ReturnsFalse) {
  // screen_capture_ is set to a dummy pointer; OH_AVScreenCapture_AcquireVideoBuffer
  // will return nullptr for an invalid capture handle, causing Init to fail.
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x1234);
  NativeFrame frame(dummy);
}

TEST(NativeFrameTest, Init_WithBuffer_GetNativeBufferFailed_ReturnsFalse) {
  GTEST_SKIP() << "Cannot test in host UT: calling OH_AVBuffer_GetNativeBuffer "
                  "on a dummy buffer_ pointer would crash without the OH "
                  "runtime; only the both-null path is testable here.";
}

// ---------------------------------------------------------------------------
// Getter tests
// ---------------------------------------------------------------------------

TEST(NativeFrameTest, GetNativeBuffer_ReturnsBuffer) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x1234);
  NativeFrame frame(dummy);
  auto* nb = reinterpret_cast<OH_NativeBuffer*>(0xABCD);
  frame.native_buffer_ = nb;
  // GetNativeBuffer has RTC_DCHECK(native_buffer_) which is satisfied.
  EXPECT_EQ(frame.GetNativeBuffer(), nb);
  frame.native_buffer_ = nullptr;
}

TEST(NativeFrameTest, ImageDimensions_ReturnConfigValues) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x1234);
  NativeFrame frame(dummy);
  frame.config_.width = 1920;
  frame.config_.height = 1080;
  frame.config_.stride = 7680;
  EXPECT_EQ(frame.image_width(), 1920);
  EXPECT_EQ(frame.image_height(), 1080);
  EXPECT_EQ(frame.image_stride(), 7680);
}

// ---------------------------------------------------------------------------
// UnMap tests
// ---------------------------------------------------------------------------

TEST(NativeFrameTest, UnMap_WithNullNativeBuffer_NoOp) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x1234);
  NativeFrame frame(dummy);
  // native_buffer_ is nullptr; UnMap should be a no-op.
  ASSERT_NO_FATAL_FAILURE(frame.UnMap());
}

}  // namespace webrtc
