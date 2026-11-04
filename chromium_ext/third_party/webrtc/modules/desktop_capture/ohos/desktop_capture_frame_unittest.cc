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

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Expose private members (factory_, frame_) for white-box assertions.
// clang-format off
#define private public
#include "modules/desktop_capture/ohos/desktop_capture_frame.h"
#undef private
// clang-format on

#include "modules/desktop_capture/shared_memory.h"

using ::testing::Return;

namespace webrtc {

// Concrete SharedMemory subclass for testing the factory success path.
class FakeSharedMemory : public SharedMemory {
 public:
  explicit FakeSharedMemory(size_t size)
      : SharedMemory(new char[size](), size, 0, 0), buffer_(data()) {}

  ~FakeSharedMemory() override { delete[] static_cast<char*>(buffer_); }

 private:
  void* buffer_;
};

// SharedMemoryFactory stand-in whose CreateSharedMemory() yields nullptr, used
// to drive the "cannot create a new DesktopFrame" failure branch in Prepare().
class MockSharedMemoryFactory : public SharedMemoryFactory {
 public:
  MOCK_METHOD(std::unique_ptr<SharedMemory>,
              CreateSharedMemory,
              (size_t size),
              (override));
};

// Concrete factory that returns real SharedMemory objects (success path).
class FakeSharedMemoryFactory : public SharedMemoryFactory {
 public:
  std::unique_ptr<SharedMemory> CreateSharedMemory(size_t size) override {
    return std::make_unique<FakeSharedMemory>(size);
  }
};

// ---------------------------------------------------------------------------
// Constructor tests
// ---------------------------------------------------------------------------

TEST(DesktopCaptureFrameTest, Constructor_StoresNullFactory) {
  DesktopCaptureFrame frame(nullptr);
  EXPECT_EQ(frame.factory_, nullptr);
}

TEST(DesktopCaptureFrameTest, Constructor_StoresNonNullFactory) {
  MockSharedMemoryFactory factory;
  DesktopCaptureFrame frame(&factory);
  EXPECT_EQ(frame.factory_, &factory);
}

// ---------------------------------------------------------------------------
// Prepare tests — factory_ == nullptr (BasicDesktopFrame branch)
// ---------------------------------------------------------------------------

TEST(DesktopCaptureFrameTest, Prepare_FactoryNull_CreatesBasicDesktopFrame) {
  DesktopCaptureFrame frame(nullptr);
  EXPECT_TRUE(frame.Prepare(DesktopSize(10, 10)));
  EXPECT_NE(frame.frame(), nullptr);
  EXPECT_EQ(frame.frame()->size().width(), 10);
  EXPECT_EQ(frame.frame()->size().height(), 10);
}

TEST(DesktopCaptureFrameTest, Prepare_SameSize_ReusesFrame) {
  DesktopCaptureFrame frame(nullptr);
  ASSERT_TRUE(frame.Prepare(DesktopSize(10, 10)));
  SharedDesktopFrame* first = frame.frame();
  ASSERT_NE(first, nullptr);

  // Same resolution: resolution_tracker_ does not signal a change, so the
  // underlying SharedDesktopFrame is reused.
  EXPECT_TRUE(frame.Prepare(DesktopSize(10, 10)));
  EXPECT_EQ(frame.frame(), first);
}

TEST(DesktopCaptureFrameTest, Prepare_DifferentSize_RecreatesFrame) {
  DesktopCaptureFrame frame(nullptr);
  ASSERT_TRUE(frame.Prepare(DesktopSize(10, 10)));
  ASSERT_NE(frame.frame(), nullptr);

  // Resolution change triggers frame_.reset() and rebuilds the frame.
  EXPECT_TRUE(frame.Prepare(DesktopSize(20, 20)));
  EXPECT_NE(frame.frame(), nullptr);
  EXPECT_EQ(frame.frame()->size().width(), 20);
  EXPECT_EQ(frame.frame()->size().height(), 20);
}

// ---------------------------------------------------------------------------
// Prepare tests — factory_ != nullptr, Create returns nullptr
// ---------------------------------------------------------------------------

TEST(DesktopCaptureFrameTest, Prepare_FactoryCreateReturnsNull_ReturnsFalse) {
  MockSharedMemoryFactory factory;
  // SharedMemoryDesktopFrame::Create forwards to CreateSharedMemory(); a
  // nullptr result makes Create() return nullptr, exercising the failure path.
  EXPECT_CALL(factory, CreateSharedMemory(::testing::_))
      .WillOnce(Return(nullptr));

  DesktopCaptureFrame frame(&factory);
  EXPECT_FALSE(frame.Prepare(DesktopSize(10, 10)));
  // frame_ is never assigned because creation failed.
  EXPECT_EQ(frame.frame_, nullptr);
}

// ---------------------------------------------------------------------------
// Prepare tests — factory_ != nullptr, Create succeeds (SharedMemory path)
// ---------------------------------------------------------------------------

TEST(DesktopCaptureFrameTest, Prepare_FactorySuccess_CreatesSharedMemoryFrame) {
  FakeSharedMemoryFactory factory;
  DesktopCaptureFrame frame(&factory);
  EXPECT_TRUE(frame.Prepare(DesktopSize(10, 10)));
  EXPECT_NE(frame.frame(), nullptr);
  EXPECT_EQ(frame.frame()->size().width(), 10);
  EXPECT_EQ(frame.frame()->size().height(), 10);
}

TEST(DesktopCaptureFrameTest, Prepare_FactorySuccess_DifferentSize_Recreates) {
  FakeSharedMemoryFactory factory;
  DesktopCaptureFrame frame(&factory);
  ASSERT_TRUE(frame.Prepare(DesktopSize(10, 10)));
  SharedDesktopFrame* first = frame.frame();
  ASSERT_NE(first, nullptr);

  // Resolution change triggers rebuild even with a working factory.
  EXPECT_TRUE(frame.Prepare(DesktopSize(20, 20)));
  EXPECT_NE(frame.frame(), nullptr);
  EXPECT_EQ(frame.frame()->size().width(), 20);
}

}  // namespace webrtc
