/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
TEST_P(HTMLVideoElementTest, TestRequestEnterFullscreen) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(video()->IsFullscreen());
  ASSERT_NO_FATAL_FAILURE(video()->RequestEnterFullscreen());
}

TEST_P(HTMLVideoElementTest, TestRequestExitFullscreen) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(video()->IsFullscreen());
  ASSERT_NO_FATAL_FAILURE(video()->RequestExitFullscreen());
}

#endif  // ARKWEB_CUSTOM_VIDEO_PLAYER

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
TEST_P(HTMLVideoElementTest, TestSetPlaybackRate) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  double playback_rate = 1.01;
  ASSERT_NO_FATAL_FAILURE(video()->SetPlaybackRate(playback_rate));
}

TEST_P(HTMLVideoElementTest, TestRequestDownloadUrl) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NO_FATAL_FAILURE(video()->RequestDownloadUrl());
}
#endif  // ARKWEB_VIDEO_ASSISTANT

#if BUILDFLAG(ARKWEB_PIP)
TEST_P(HTMLVideoElementTest, TestOnPictureInPictureStateChanged) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  uint32_t state = 0;
  int32_t width = 1;
  int32_t height = 1;
  ASSERT_NO_FATAL_FAILURE(video()->OnPictureInPictureStateChanged(state, width, height));
}

TEST_P(HTMLVideoElementTest, TestPipDown) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NO_FATAL_FAILURE(video()->PipDown(true));
  ASSERT_NO_FATAL_FAILURE(video()->PipDown(false));
}

TEST_P(HTMLVideoElementTest, TestRequestExitPictureInPicture) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NO_FATAL_FAILURE(video()->RequestExitPictureInPicture());
}

TEST_P(HTMLVideoElementTest, TestNotifyPipResize) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NO_FATAL_FAILURE(video()->NotifyPipResize());
}

TEST_P(HTMLVideoElementTest, TestPipRequestPlay) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NO_FATAL_FAILURE(video()->PipRequestPlay());
  video()->paused_ = false;
  ASSERT_NO_FATAL_FAILURE(video()->PipRequestPlay());
}
#endif

#if BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
TEST_P(HTMLVideoElementTest, VideoLoadOpt_ConfigDataTest) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(video()->hbsPreloadTime(), 4);
  EXPECT_EQ(video()->hbsMinCacheTime(), 2);
  EXPECT_EQ(video()->hbsMaxCacheTime(), 6);
  EXPECT_EQ(video()->hbsBitrate(), 2000);
  EXPECT_EQ(video()->hbsMoovSize(), 512);

  video()->setHbsPreloadTime(6);
  video()->setHbsMinCacheTime(3);
  video()->setHbsMaxCacheTime(9);
  video()->setHbsBitrate(3000);
  video()->setHbsMoovSize(1024);

  EXPECT_EQ(video()->hbsPreloadTime(), 6);
  EXPECT_EQ(video()->hbsMinCacheTime(), 3);
  EXPECT_EQ(video()->hbsMaxCacheTime(), 9);
  EXPECT_EQ(video()->hbsBitrate(), 3000);
  EXPECT_EQ(video()->hbsMoovSize(), 1024);
}

TEST_P(HTMLVideoElementTest, VideoLoadOpt_GetFrameConfigDataTest) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();

  ASSERT_NO_FATAL_FAILURE(video()->GetVideoPreloadTimeDefault());
  ASSERT_NO_FATAL_FAILURE(video()->GetVideoMinCacheTimeDefault());
  ASSERT_NO_FATAL_FAILURE(video()->GetVideoMaxCacheTimeDefault());
  ASSERT_NO_FATAL_FAILURE(video()->GetVideoMoovSizeDefault());
  ASSERT_NO_FATAL_FAILURE(video()->GetVideoBitrateDefault());
}

TEST_P(HTMLVideoElementTest, VideoLoadOpt_CheckAndSetValueTest) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  video()->setHbsPreloadTime(6);
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();

  uint16_t preloadTime_attr = 0;
  video()->CheckAndSetValue(html_names::kHbspreloadtimeAttr, &preloadTime_attr);
  EXPECT_EQ(video()->hbsPreloadTime(), 6);
}

#endif // ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION

#if BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
TEST_P(HTMLVideoElementTest, VideoLoadOpt_CheckAndSetValueInvalidInput) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();

  // Test with non-numeric attribute value - !ok branch
  video()->setAttribute(html_names::kHbspreloadtimeAttr,
                        AtomicString("not_a_number"));
  uint16_t preload_time = 100;
  video()->CheckAndSetValue(html_names::kHbspreloadtimeAttr, &preload_time);
  EXPECT_EQ(preload_time, 100);  // Should not change - !ok returns early
 
  // Test with zero value - result <= 0 branch
  video()->setAttribute(html_names::kHbspreloadtimeAttr, AtomicString("0"));
  preload_time = 100;
  video()->CheckAndSetValue(html_names::kHbspreloadtimeAttr, &preload_time);
  EXPECT_EQ(preload_time, 100);  // Should not change - result <= 0

  // Test with negative value - result <= 0 branch
  video()->setAttribute(html_names::kHbspreloadtimeAttr, AtomicString("-5"));
  preload_time = 100;
  video()->CheckAndSetValue(html_names::kHbspreloadtimeAttr, &preload_time);
  EXPECT_EQ(preload_time, 100);  // Should not change - result <= 0

  // Test with value exceeding UINT16_MAX - result > UINT16_MAX branch
  video()->setAttribute(html_names::kHbspreloadtimeAttr, AtomicString("70000"));
  preload_time = 100;
  video()->CheckAndSetValue(html_names::kHbspreloadtimeAttr, &preload_time);
  EXPECT_EQ(preload_time, 100);  // Should not change - result > UINT16_MAX

  // Test with valid value - should update
  video()->setAttribute(html_names::kHbspreloadtimeAttr, AtomicString("10"));
  preload_time = 100;
  video()->CheckAndSetValue(html_names::kHbspreloadtimeAttr, &preload_time);
  EXPECT_EQ(preload_time, 10);  // Should update
}
#endif  // ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION

#if BUILDFLAG(ARKWEB_PIP)
TEST_P(HTMLVideoElementTest, TestOnPictureInPictureStateChanged_PipDownTrue) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  // When pip_down_ is true, the function should set pip_down_ to true and
  // return
  uint32_t state = 0;
  int32_t width = 1;
  int32_t height = 1;
  ASSERT_NO_FATAL_FAILURE(
      video()->OnPictureInPictureStateChanged(state, width, height));
}

TEST_P(HTMLVideoElementTest, TestPipDown_NoMediaPlayer) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  // When web_media_player_ is null, PipDown should return early
  video()->web_media_player_ = nullptr;
  ASSERT_NO_FATAL_FAILURE(video()->PipDown(true));
}

TEST_P(HTMLVideoElementTest, TestOnPictureInPictureStateChanged_ObserverPath) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  // Set pip_down_ to true so the observer path is taken
  video()->pip_down_ = true;
  uint32_t state = 1;
  int32_t width = 100;
  int32_t height = 100;
  // Call with pip_down_=true and web_media_player_ present
  ASSERT_NO_FATAL_FAILURE(
      video()->OnPictureInPictureStateChanged(state, width, height));
}
#endif

#if BUILDFLAG(ARKWEB_MEDIA_CAST)
TEST_P(HTMLVideoElementTest, TestUpdateUiPlayState) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  // With remoting_interstitial_ null (default)
  ASSERT_NO_FATAL_FAILURE(video()->UpdateUiPlayState(true));
  ASSERT_NO_FATAL_FAILURE(video()->UpdateUiPlayState(false));
}

TEST_P(HTMLVideoElementTest, TestUpdateUiPlayPosition) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  // With remoting_interstitial_ null (default)
  ASSERT_NO_FATAL_FAILURE(video()->UpdateUiPlayPosition(0));
  ASSERT_NO_FATAL_FAILURE(video()->UpdateUiPlayPosition(1000));
}

TEST_P(HTMLVideoElementTest, TestUpdateRemotePlayState) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NO_FATAL_FAILURE(video()->UpdateRemotePlayState(true));
  ASSERT_NO_FATAL_FAILURE(video()->UpdateRemotePlayState(false));
}

TEST_P(HTMLVideoElementTest, TestUpdateRemotePlayPosition) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NO_FATAL_FAILURE(video()->UpdateRemotePlayPosition(0));
  ASSERT_NO_FATAL_FAILURE(video()->UpdateRemotePlayPosition(5000));
}

TEST_P(HTMLVideoElementTest, TestNotifyRemoteInterstitial) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  // With remoting_interstitial_ null (default)
  ASSERT_NO_FATAL_FAILURE(
      video()->NotifyRemoteInterstitial(MediaControlsSizingClass::kLarge));
  ASSERT_NO_FATAL_FAILURE(
      video()->NotifyRemoteInterstitial(MediaControlsSizingClass::kSmall));
}

TEST_P(HTMLVideoElementTest, TestUpdateRemoteFullScreenCss) {
  video()->SetSrc(AtomicString("http://example.com/foo.mp4"));
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  // With remoting_interstitial_ null (default)
  ASSERT_NO_FATAL_FAILURE(video()->UpdateRemoteFullScreenCss());
}
#endif  // BUILDFLAG(ARKWEB_MEDIA_CAST)