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

#include "arkweb/build/features/features.h"
#include "base/unguessable_token.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
// NOTE: do NOT #include video_capture_host.h (nor anything that pulls in
// video_capture_host_utils.h) before the #define private public below.
// video_capture_host.h includes video_capture_host_utils.h, which would set
// its include guard and turn the private->public rewrite into a no-op.

// clang-format off
#define private public
#include "arkweb/chromium_ext/content/browser/renderer_host/media/video_capture_host_utils.h"
#undef private
// clang-format on
// video_capture_host_utils.h transitively brings in global_routing_id.h,
// video_capture_controller_event_handler.h and video_capture_host.h.

using content::GlobalRenderFrameHostId;
using content::VideoCaptureHostUtils;

#if BUILDFLAG(ARKWEB_WEBRTC)
using content::CameraCaptureState;
using content::VideoCaptureControllerID;
using content::VideoCaptureHost;
#endif

class VideoCaptureHostUtilsTest : public testing::Test {
 protected:
  // content::BrowserTaskEnvironment initializes content::BrowserThread with the
  // UI thread bound to the main test thread. This makes
  // OnCameraCaptureStateChangedBind take the real UI-thread path
  // (BrowserThread::CurrentlyOn(UI) == true) and skip the off-UI
  // GetUIThreadTaskRunner({})->PostTask re-dispatch branch entirely.
  content::BrowserTaskEnvironment task_environment_;
};

// ---------------------------------------------------------------------------
// Constructor tests
// ---------------------------------------------------------------------------

TEST_F(VideoCaptureHostUtilsTest, Constructor_NullHost) {
  VideoCaptureHostUtils utils(nullptr);
  EXPECT_TRUE(utils.videoCaptureHost == nullptr);
}

// ---------------------------------------------------------------------------
// Setter tests (require ARKWEB_RENDER_PROCESS_MODE)
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
TEST_F(VideoCaptureHostUtilsTest, SetRenderFrameHostId_SetsValue) {
  VideoCaptureHostUtils utils(nullptr);
  GlobalRenderFrameHostId id(42, 99);
  utils.SetRenderFrameHostId(id);
  EXPECT_EQ(utils.render_frame_host_id_.child_id, 42);
  EXPECT_EQ(utils.render_frame_host_id_.frame_routing_id, 99);
}
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)

// ---------------------------------------------------------------------------
// OnCameraCaptureStateChangedBind early-return (requires ARKWEB_WEBRTC)
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_WEBRTC)
TEST_F(VideoCaptureHostUtilsTest,
       OnCameraCaptureStateChangedBind_SameState_ReturnsEarly) {
  GlobalRenderFrameHostId id;
  // When camera_state == new_state the function returns immediately.
  ASSERT_NO_FATAL_FAILURE(
      VideoCaptureHostUtils::OnCameraCaptureStateChangedBind(
          CameraCaptureState::NONE, CameraCaptureState::NONE, id));
}

TEST_F(VideoCaptureHostUtilsTest,
       OnCameraCaptureStateChangedBind_DifferentState_NullHost_ReturnsEarly) {
  // Different states but invalid GlobalRenderFrameHostId {0,0}.
  // Under content::BrowserTaskEnvironment the main test thread IS the UI
  // thread, so BrowserThread::CurrentlyOn(UI) is true and the function skips
  // the off-UI PostTask branch. It calls RenderFrameHost::FromID({0,0}), which
  // returns null -> early return. Deterministic, no tasks posted.
  GlobalRenderFrameHostId id;
  ASSERT_NO_FATAL_FAILURE(
      VideoCaptureHostUtils::OnCameraCaptureStateChangedBind(
          CameraCaptureState::NONE, CameraCaptureState::ACTIVE, id));
  // Drain, in case any task ever gets posted; should be a no-op today.
  task_environment_.RunUntilIdle();
}
#endif  // BUILDFLAG(ARKWEB_WEBRTC)

// ---------------------------------------------------------------------------
// OnCameraCaptureStarted early-return (requires ARKWEB_WEBRTC)
// ---------------------------------------------------------------------------

#if BUILDFLAG(ARKWEB_WEBRTC)
TEST_F(VideoCaptureHostUtilsTest,
       OnCameraCaptureStarted_NullHost_ReturnsEarly) {
  VideoCaptureHostUtils utils(nullptr);
  VideoCaptureControllerID controller_id = base::UnguessableToken::Create();
  // videoCaptureHost is null -> early return, no crash.
  ASSERT_NO_FATAL_FAILURE(utils.OnCameraCaptureStarted(controller_id));
}

TEST_F(VideoCaptureHostUtilsTest,
       OnCameraCaptureStateChanged_UpdatesCameraState) {
  VideoCaptureHostUtils utils(nullptr);
  EXPECT_EQ(utils.camera_state_, CameraCaptureState::NONE);
  utils.OnCameraCaptureStateChanged(CameraCaptureState::ACTIVE);
  EXPECT_EQ(utils.camera_state_, CameraCaptureState::ACTIVE);
  utils.OnCameraCaptureStateChanged(CameraCaptureState::PAUSED);
  EXPECT_EQ(utils.camera_state_, CameraCaptureState::PAUSED);
}

// OnCameraCaptureStarted_ControllerNotFound_ReturnsEarly is skipped:
// it requires a real VideoCaptureHost object with an empty controllers_ map.
// VideoCaptureHost cannot be instantiated in a unit test without the full
// content browser infrastructure (media service, render process host, etc.).
#endif  // BUILDFLAG(ARKWEB_WEBRTC)
