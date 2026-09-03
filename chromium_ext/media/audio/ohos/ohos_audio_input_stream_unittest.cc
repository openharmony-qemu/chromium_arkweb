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

#include <ranges>
#include <memory>
#include "arkweb/chromium_ext/media/base/ohos/ohos_media_player_bridge.h"
#include "base/logging.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#define private public
#define protected public
#include "ohos_audio_input_stream.h"
#undef protected
#undef private

using namespace media;
using namespace testing;
using namespace OHOS::NWeb;
using namespace base;
using namespace content;

namespace media {
namespace {
constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultChannelCount = 2;
constexpr int kMinimumInputBufferSize = 2048;
constexpr double kVolumeEpsilon = 1e-6;
constexpr int kInvalidNWebId = -1;
}  // namespace

class MockAudioThread : public AudioThread {
 public:
  void Stop() { std::cout << "Stop function called" << std::endl; }

  bool IsHung() const { return false; }

  base::SingleThreadTaskRunner* GetTaskRunner() { return nullptr; }

  base::SingleThreadTaskRunner* GetWorkerTaskRunner() { return nullptr; }
};

class OHOSAudioInputStreamTest : public content::RenderViewHostTestHarness {
 public:
  OHOSAudioInputStreamTest() : content::RenderViewHostTestHarness() {}

  OHOSAudioInputStreamTest(const OHOSAudioInputStreamTest&) = delete;
  OHOSAudioInputStreamTest& operator=(const OHOSAudioInputStreamTest&) = delete;

  void SetUp() override {
    std::unique_ptr<MockAudioThread> mock_audio_thread =
        std::make_unique<MockAudioThread>();
    ohos_audio_manager_ = std::make_shared<media::OHOSAudioManager>(
        std::move(mock_audio_thread), nullptr);
    createAudioInputStream();

    content::RenderViewHostTestHarness::SetUp();
  }
  void TearDown() override {
    content::RenderViewHostTestHarness::TearDown();
  }

  void createAudioInputStream() {
    params_ = AudioParameters(AudioParameters::AUDIO_FAKE,
                              ChannelLayoutConfig::Guess(kDefaultChannelCount),
                              kDefaultSampleRate, kMinimumInputBufferSize);
    std::string device_id = "some_device_id";
    audioInputStream_ = static_cast<OHOSAudioInputStream*>(
        ohos_audio_manager_->MakeLowLatencyInputStream(params_, device_id,
                                                       log_callback_));
  }

 public:
  std::shared_ptr<media::OHOSAudioManager> ohos_audio_manager_;
  AudioParameters params_;
  AudioManagerBase::LogCallback log_callback_;
  OHOSAudioInputStream* audioInputStream_;
};
}  // namespace media

TEST_F(OHOSAudioInputStreamTest, Open001) {
  EXPECT_EQ(audioInputStream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
}

TEST_F(OHOSAudioInputStreamTest, Start001) {
#if BUILDFLAG(ARKWEB_WEBRTC)
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:1", true);
#endif
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Start(nullptr));
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, Start002) {
#if BUILDFLAG(ARKWEB_WEBRTC)
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:0", true);
#endif
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Start(nullptr));
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, Stop001) {
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Stop());
}

TEST_F(OHOSAudioInputStreamTest, GetMaxVolume001) {
  EXPECT_EQ(audioInputStream_->GetMaxVolume(), 1.0);
}

TEST_F(OHOSAudioInputStreamTest, SetVolume001) {
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:1", true);
  audioInputStream_->Start(nullptr);
  audioInputStream_->SetVolume(0.5);
  EXPECT_EQ(audioInputStream_->GetVolume(), 0.5);
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetVolume002) {
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:0", true);
  audioInputStream_->Start(nullptr);
  audioInputStream_->SetVolume(0.5);
  EXPECT_EQ(audioInputStream_->GetVolume(), 0.5);
  EXPECT_EQ(audioInputStream_->IsMuted(), false);
  audioInputStream_->SetVolume(0);
  EXPECT_EQ(audioInputStream_->GetVolume(), 0);
  EXPECT_EQ(audioInputStream_->IsMuted(), true);
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetVolume003) {
  audioInputStream_->SetVolume(0);
  EXPECT_EQ(audioInputStream_->GetVolume(), 0);
  EXPECT_EQ(audioInputStream_->IsMuted(), true);
}

TEST_F(OHOSAudioInputStreamTest, SetAutomaticGainControl001) {
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:1", true);
  audioInputStream_->Start(nullptr);
  audioInputStream_->SetAutomaticGainControl(true);
  EXPECT_EQ(audioInputStream_->GetAutomaticGainControl(), true);
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetAutomaticGainControl002) {
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:0", true);
  audioInputStream_->Start(nullptr);
  audioInputStream_->SetAutomaticGainControl(false);
  EXPECT_EQ(audioInputStream_->GetAutomaticGainControl(), false);
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetAutomaticGainControl003) {
  audioInputStream_->SetAutomaticGainControl(false);
  EXPECT_EQ(audioInputStream_->GetAutomaticGainControl(), false);
}

TEST_F(OHOSAudioInputStreamTest, SetOutputDeviceForAec001) {
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:1", true);
  audioInputStream_->Start(nullptr);
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->SetOutputDeviceForAec("deviceId"));
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetOutputDeviceForAec002) {
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:0", true);
  audioInputStream_->Start(nullptr);
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->SetOutputDeviceForAec("someDeviceId"));
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetOutputDeviceForAec003) {
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->SetOutputDeviceForAec("someDeviceId"));
}

TEST_F(OHOSAudioInputStreamTest, GetNWebId001) {
  EXPECT_EQ(audioInputStream_->GetNWebId(params_), -1);
}

TEST_F(OHOSAudioInputStreamTest, GetNWebIdOnUIThread001) {
  EXPECT_EQ(audioInputStream_->GetNWebIdOnUIThread(params_), -1);
}

TEST_F(OHOSAudioInputStreamTest, GetNWebIdOnUIThread002) {
  auto frameHost = main_rfh();
  ASSERT_NE(frameHost, nullptr);
  params_.set_render_process_id(static_cast<int>(frameHost->GetProcess()->GetID()));
  params_.set_render_frame_id(frameHost->GetRoutingID());

  EXPECT_NE(audioInputStream_->GetNWebIdOnUIThread(params_), -1);
}

TEST_F(OHOSAudioInputStreamTest, Pause001) {
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Pause());
}

TEST_F(OHOSAudioInputStreamTest, Pause002) {
  audioInputStream_->Start(nullptr);
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Pause());
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, Resume001) {
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Resume());
}

TEST_F(OHOSAudioInputStreamTest, Resume002) {
  audioInputStream_->Start(nullptr);
  audioInputStream_->Pause();
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Resume());
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, ResumeMicrophone001) {
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->ResumeMicrophone());
}

TEST_F(OHOSAudioInputStreamTest, ResumeMicrophone002) {
  audioInputStream_->Start(nullptr);
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->ResumeMicrophone());
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, PauseMicrophone001) {
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->PauseMicrophone());
}

TEST_F(OHOSAudioInputStreamTest, PauseMicrophone002) {
  audioInputStream_->Start(nullptr);
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->PauseMicrophone());
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetPauseStateChangedCallback001) {
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->SetPauseStateChangedCallback(
      base::BindRepeating([](bool paused) {})));
}

TEST_F(OHOSAudioInputStreamTest, OnMicrophoneCaptureStateChanged001) {
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->OnMicrophoneCaptureStateChanged(
          AudioInputStream::AudioCaptureState::ACTIVE));
}

TEST_F(OHOSAudioInputStreamTest, OnMicrophoneCaptureStateChanged002) {
  audioInputStream_->Start(nullptr);
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->OnMicrophoneCaptureStateChanged(
          AudioInputStream::AudioCaptureState::PAUSE));
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, OnMicrophoneCaptureStateChanged003) {
  audioInputStream_->Start(nullptr);
  audioInputStream_->Pause();
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->OnMicrophoneCaptureStateChanged(
          AudioInputStream::AudioCaptureState::NONE));
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, OnMicrophoneCaptureStateChanged004_SameState) {
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->OnMicrophoneCaptureStateChanged(
          AudioInputStream::AudioCaptureState::NONE));
}

TEST_F(OHOSAudioInputStreamTest, OnMicrophoneCaptureStateChangedBind001_SameState) {
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->OnMicrophoneCaptureStateChangedBind(
          AudioInputStream::AudioCaptureState::NONE, AudioInputStream::AudioCaptureState::NONE, params_));
}

TEST_F(OHOSAudioInputStreamTest, OnMicrophoneCaptureStateChangedBind002_NullRenderFrameHost) {
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->OnMicrophoneCaptureStateChangedBind(
          AudioInputStream::AudioCaptureState::NONE, AudioInputStream::AudioCaptureState::ACTIVE, params_));
}

TEST_F(OHOSAudioInputStreamTest, OnMicrophoneCaptureStateChangedBind003_WithValidRenderFrameHost) {
  auto frameHost = main_rfh();
  ASSERT_NE(frameHost, nullptr);
  AudioParameters params = params_;
  params.set_render_process_id(static_cast<int>(frameHost->GetProcess()->GetID()));
  params.set_render_frame_id(frameHost->GetRoutingID());
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->OnMicrophoneCaptureStateChangedBind(
          AudioInputStream::AudioCaptureState::NONE, AudioInputStream::AudioCaptureState::ACTIVE, params));
}

TEST_F(OHOSAudioInputStreamTest, IsMuted001) {
  audioInputStream_->Start(nullptr);
  audioInputStream_->SetVolume(0.5);
  EXPECT_EQ(audioInputStream_->IsMuted(), false);
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, IsMuted002) {
  audioInputStream_->Start(nullptr);
  audioInputStream_->SetVolume(0);
  EXPECT_EQ(audioInputStream_->IsMuted(), true);
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, GetAudioParameters001) {
  AudioParameters result = audioInputStream_->GetAudioParameters();
  EXPECT_EQ(result.sample_rate(), kDefaultSampleRate);
  EXPECT_EQ(result.channels(), kDefaultChannelCount);
}

TEST_F(OHOSAudioInputStreamTest, OnMicrophoneCaptureStateChangedBind004_PauseToActive) {
  audioInputStream_->Start(nullptr);
  audioInputStream_->Pause();
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->OnMicrophoneCaptureStateChanged(
          AudioInputStream::AudioCaptureState::ACTIVE));
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetVolumeWithBaseCapturer) {
#if BUILDFLAG(ARKWEB_WEBRTC)
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:0", true);
#endif
  audioInputStream_->Start(nullptr);
  audioInputStream_->SetVolume(0.3);
  EXPECT_EQ(audioInputStream_->GetVolume(), 0.3);
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetAutomaticGainControlWithBaseCapturer) {
#if BUILDFLAG(ARKWEB_WEBRTC)
  ohos_audio_manager_->SelectAudioDevice("screen:systemAudio:-2:0", true);
#endif
  audioInputStream_->Start(nullptr);
  audioInputStream_->SetAutomaticGainControl(true);
  EXPECT_EQ(audioInputStream_->GetAutomaticGainControl(), true);
  audioInputStream_->Stop();
}

TEST_F(OHOSAudioInputStreamTest, SetOutputDeviceForAecNotStarted) {
  ASSERT_NO_FATAL_FAILURE(
      audioInputStream_->SetOutputDeviceForAec("someDeviceId"));
}

TEST_F(OHOSAudioInputStreamTest, PauseNotStarted) {
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Pause());
}

TEST_F(OHOSAudioInputStreamTest, ResumeNotStarted) {
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Resume());
}

TEST_F(OHOSAudioInputStreamTest, OnCapturerPaused001_NoCallback) {
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->OnCapturerPaused(true));
}

TEST_F(OHOSAudioInputStreamTest, OnCapturerPaused002_WithCallback) {
  static bool paused_received = false;
  paused_received = false;
  audioInputStream_->SetPauseStateChangedCallback(
      base::BindRepeating([](bool paused) { paused_received = paused; }));
  audioInputStream_->OnCapturerPaused(true);
  EXPECT_EQ(paused_received, true);
}

TEST_F(OHOSAudioInputStreamTest, OnCapturerPaused003_CallbackFalse) {
  static bool paused_received = false;
  paused_received = true;
  audioInputStream_->SetPauseStateChangedCallback(
      base::BindRepeating([](bool paused) { paused_received = paused; }));
  audioInputStream_->OnCapturerPaused(false);
  EXPECT_EQ(paused_received, false);
}

TEST_F(OHOSAudioInputStreamTest, Close001_StopReleasesCapturerSources) {
  audioInputStream_->Start(nullptr);
  EXPECT_TRUE(audioInputStream_->capturer_source_ != nullptr ||
              audioInputStream_->base_capturer_source_ != nullptr);

  audioInputStream_->Stop();
  EXPECT_EQ(audioInputStream_->capturer_source_, nullptr);
  EXPECT_EQ(audioInputStream_->base_capturer_source_, nullptr);
}

TEST_F(OHOSAudioInputStreamTest, Close002_DoubleStopSafe) {
  audioInputStream_->Start(nullptr);
  audioInputStream_->Stop();
  ASSERT_NO_FATAL_FAILURE(audioInputStream_->Stop());
}
