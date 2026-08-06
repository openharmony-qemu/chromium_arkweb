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

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Expose callback_map_ for white-box injection/inspection. The header pulls in
// native_avscreen_capture.h transitively; C OH headers contain no `private`
// token, so the macro is safe around this include.
#define private public
#include "modules/desktop_capture/ohos/window_capturer_notify.h"
#undef private

namespace webrtc {

// The three dispatchers are file-local in window_capturer_notify.cc (not
// declared in any header). Forward-declare them here so the linker resolves
// them to the production definitions.
void OnStateChange(OH_AVScreenCapture* capture,
                   OH_AVScreenCaptureStateCode stateCode,
                   void* userData);
void OnError(OH_AVScreenCapture* capture, int32_t errorCode, void* userData);
void OnBufferAvailable(OH_AVScreenCapture* capture,
                       OH_AVBuffer* buffer,
                       OH_AVScreenCaptureBufferType bufferType,
                       int64_t timestamp,
                       void* userData);

// CapturerEventCallBack stand-in recording dispatcher deliveries.
class MockCapturerEventCallBack : public CapturerEventCallBack {
 public:
  virtual ~MockCapturerEventCallBack() = default;
  MOCK_METHOD(void,
              OnStateChange,
              (OH_AVScreenCaptureStateCode stateCode, void* userData),
              (override));
  MOCK_METHOD(void,
              OnError,
              (int32_t errorCode, void* userData),
              (override));
  MOCK_METHOD(void,
              OnBufferAvailable,
              (OH_AVBuffer* buffer,
               OH_AVScreenCaptureBufferType bufferType,
               int64_t timestamp,
               void* userData),
              (override));
};

// WindowCapturerNotify is a base::Singleton, so its internal callback_map_
// persists across tests in the same process. TaskEnvironment provides the
// AtExitManager the Singleton needs; TearDown clears the map to isolate tests.
class WindowCapturerNotifyTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;

  void TearDown() override {
    WindowCapturerNotify::GetInstance()->callback_map_.clear();
  }
};

// ---------------------------------------------------------------------------
// GetInstance / GetEventCallBack
// ---------------------------------------------------------------------------

TEST_F(WindowCapturerNotifyTest, GetInstance_ReturnsNonNull) {
  EXPECT_NE(WindowCapturerNotify::GetInstance(), nullptr);
}

TEST_F(WindowCapturerNotifyTest, GetEventCallBack_Miss_ReturnsNull) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x1234);
  EXPECT_EQ(WindowCapturerNotify::GetInstance()->GetEventCallBack(dummy),
            nullptr);
}

TEST_F(WindowCapturerNotifyTest, GetEventCallBack_Hit_ReturnsCallback) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x2345);
  auto mock_cb = std::make_shared<MockCapturerEventCallBack>();
  WindowCapturerNotify::GetInstance()->callback_map_[dummy] = mock_cb;

  EXPECT_EQ(WindowCapturerNotify::GetInstance()->GetEventCallBack(dummy),
            mock_cb.get());
}

// ---------------------------------------------------------------------------
// UnRegisterEventCallback
// ---------------------------------------------------------------------------

TEST_F(WindowCapturerNotifyTest, UnRegisterEventCallback_NotInMap_NoOp) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x3456);
  EXPECT_EQ(WindowCapturerNotify::GetInstance()->callback_map_.count(dummy),
            0u);
  // No crash, map untouched.
  ASSERT_NO_FATAL_FAILURE(
      WindowCapturerNotify::GetInstance()->UnRegisterEventCallback(dummy));
  EXPECT_EQ(WindowCapturerNotify::GetInstance()->callback_map_.count(dummy),
            0u);
}

TEST_F(WindowCapturerNotifyTest, UnRegisterEventCallback_InMap_Erases) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x4567);
  auto mock_cb = std::make_shared<MockCapturerEventCallBack>();
  WindowCapturerNotify::GetInstance()->callback_map_[dummy] = mock_cb;
  ASSERT_EQ(WindowCapturerNotify::GetInstance()->callback_map_.count(dummy),
            1u);

  WindowCapturerNotify::GetInstance()->UnRegisterEventCallback(dummy);
  EXPECT_EQ(WindowCapturerNotify::GetInstance()->callback_map_.count(dummy),
            0u);
}

// ---------------------------------------------------------------------------
// RegisterEventCallback — already-registered early return (does NOT touch the
// OH NDK setters, which would crash on a dummy capture pointer).
// ---------------------------------------------------------------------------

TEST_F(WindowCapturerNotifyTest,
       RegisterEventCallback_AlreadyRegistered_ReturnsTrueWithoutNdk) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x5678);
  auto existing = std::make_shared<MockCapturerEventCallBack>();
  WindowCapturerNotify::GetInstance()->callback_map_[dummy] = existing;

  auto second = std::make_shared<MockCapturerEventCallBack>();
  // Pre-registered: must short-circuit and return true without invoking any
  // OH_AVScreenCapture_Set*Callback NDK call.
  int dummy_user = 0;
  bool result = WindowCapturerNotify::GetInstance()->RegisterEventCallback(
      dummy, second, &dummy_user);
  EXPECT_TRUE(result);
  // The original callback must remain (not overwritten).
  EXPECT_EQ(WindowCapturerNotify::GetInstance()->GetEventCallBack(dummy),
            existing.get());
}

// ---------------------------------------------------------------------------
// Free-function dispatchers: null-callback early return (LOG + return).
// Each uses a capture key that is absent from the map, so GetEventCallBack
// yields nullptr and no delivery occurs.
// ---------------------------------------------------------------------------

TEST_F(WindowCapturerNotifyTest, OnStateChange_NullCallback_EarlyReturn) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x6789);
  ASSERT_NO_FATAL_FAILURE(
      OnStateChange(dummy, OH_SCREEN_CAPTURE_STATE_STARTED, nullptr));
}

TEST_F(WindowCapturerNotifyTest, OnError_NullCallback_EarlyReturn) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x789a);
  ASSERT_NO_FATAL_FAILURE(OnError(dummy, 42, nullptr));
}

TEST_F(WindowCapturerNotifyTest, OnBufferAvailable_NullCallback_EarlyReturn) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x89ab);
  auto* dummy_buffer = reinterpret_cast<OH_AVBuffer*>(0x9999);
  ASSERT_NO_FATAL_FAILURE(OnBufferAvailable(
      dummy, dummy_buffer, OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO, 1234, nullptr));
}

// ---------------------------------------------------------------------------
// Free-function dispatchers: hit path — the registered mock receives the call.
// ---------------------------------------------------------------------------

TEST_F(WindowCapturerNotifyTest, OnStateChange_Hit_Dispatches) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0x9abc);
  auto mock_cb = std::make_shared<MockCapturerEventCallBack>();
  WindowCapturerNotify::GetInstance()->callback_map_[dummy] = mock_cb;

  int ud_value = 7;
  void* ud = &ud_value;
  EXPECT_CALL(*mock_cb,
              OnStateChange(OH_SCREEN_CAPTURE_STATE_STARTED, ud))
      .Times(1);
  OnStateChange(dummy, OH_SCREEN_CAPTURE_STATE_STARTED, ud);
}

TEST_F(WindowCapturerNotifyTest, OnError_Hit_Dispatches) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0xabcd);
  auto mock_cb = std::make_shared<MockCapturerEventCallBack>();
  WindowCapturerNotify::GetInstance()->callback_map_[dummy] = mock_cb;

  int ud_value = 8;
  void* ud = &ud_value;
  EXPECT_CALL(*mock_cb, OnError(42, ud)).Times(1);
  OnError(dummy, 42, ud);
}

TEST_F(WindowCapturerNotifyTest, OnBufferAvailable_Hit_Dispatches) {
  auto* dummy = reinterpret_cast<OH_AVScreenCapture*>(0xbcde);
  auto mock_cb = std::make_shared<MockCapturerEventCallBack>();
  WindowCapturerNotify::GetInstance()->callback_map_[dummy] = mock_cb;

  auto* dummy_buffer = reinterpret_cast<OH_AVBuffer*>(0x8888);
  int ud_value = 9;
  void* ud = &ud_value;
  EXPECT_CALL(*mock_cb,
              OnBufferAvailable(dummy_buffer, OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO,
                                /*timestamp=*/1234, ud))
      .Times(1);
  OnBufferAvailable(dummy, dummy_buffer, OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO,
                    1234, ud);
}

}  // namespace webrtc
