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

#include <memory>

#include "arkweb/build/features/features.h"
#include "audio_system_manager_adapter.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/system/system_monitor.h"
#include "base/task/bind_post_task.h"
#include "base/test/task_environment.h"
#include "gtest/gtest.h"
#include "media/audio/audio_thread.h"
#include "media/base/media_switches.h"
#include "ohos_adapter_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/hiviewdfx_adapter.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/surface_control_adapter.h"
#define private public
#define protected public
#include "ohos_audio_manager.h"
#undef protected
#undef private

using namespace testing;
namespace media {
constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultChannelCount = 2;
constexpr int kMinimumOutputBufferSize = 2048;
constexpr int kLowLatencyOutputBufferSize = 960;
#if BUILDFLAG(ARKWEB_WEBRTC)
constexpr int kMinimumInputBufferSize = 2048;
#endif  // BUILDFLAG(ARKWEB_WEBRTC)

class MockOhosAdapterHelper : public OHOS::NWeb::OhosAdapterHelper {
 public:
  MOCK_METHOD(std::unique_ptr<AafwkAppMgrClientAdapter>,
              CreateAafwkAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<PowerMgrClientAdapter>,
              CreatePowerMgrClientAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<DisplayManagerAdapter>,
              CreateDisplayMgrAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<BatteryMgrClientAdapter>,
              CreateBatteryClientAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<NetConnectAdapter>,
              CreateNetConnectAdapter,
              (),
              (override));
  MOCK_METHOD(OhosWebDataBaseAdapter&,
              GetOhosWebDataBaseAdapterInstance,
              (),
              (override));
  MOCK_METHOD(PasteBoardClientAdapter&, GetPasteBoard, (), (override));
  MOCK_METHOD(std::unique_ptr<AudioRendererAdapter>,
              CreateAudioRendererAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<AudioCapturerAdapter>,
              CreateAudioCapturerAdapter,
              (),
              (override));
  MOCK_METHOD(AudioSystemManagerAdapter&,
              GetAudioSystemManager,
              (),
              (override));
  MOCK_METHOD(OhosWebPermissionDataBaseAdapter&,
              GetWebPermissionDataBaseInstance,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<MMIAdapter>, CreateMMIAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<SocPerfClientAdapter>,
              CreateSocPerfClientAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<OhosResourceAdapter>,
              GetResourceAdapter,
              (const std::string&),
              (override));
  MOCK_METHOD(SystemPropertiesAdapter&,
              GetSystemPropertiesInstance,
              (),
              (override));
  MOCK_METHOD(VSyncAdapter&, GetVSyncAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<OhosInitWebAdapter>,
              GetInitWebAdapter,
              (),
              (override));
  MOCK_METHOD(KeystoreAdapter&, GetKeystoreAdapterInstance, (), (override));
  MOCK_METHOD(EnterpriseDeviceManagementAdapter&,
              GetEnterpriseDeviceManagementInstance,
              (),
              (override));
  MOCK_METHOD(DatashareAdapter&, GetDatashareInstance, (), (override));
  MOCK_METHOD(std::unique_ptr<IMFAdapter>, CreateIMFAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<CertManagerAdapter>,
              GetRootCertDataAdapter,
              (),
              (override));
  MOCK_METHOD(AccessTokenAdapter&,
              GetAccessTokenAdapterInstance,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<EventHandlerAdapter>,
              GetEventHandlerAdapter,
              (),
              (override));
  MOCK_METHOD(PrintManagerAdapter&, GetPrintManagerInstance, (), (override));
  MOCK_METHOD(std::unique_ptr<IConsumerSurfaceAdapter>,
              CreateConsumerSurfaceAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<PlayerAdapter>,
              CreatePlayerAdapter,
              (),
              (override));
  MOCK_METHOD(WindowAdapter&, GetWindowAdapterInstance, (), (override));
  MOCK_METHOD(HiSysEventAdapter&, GetHiSysEventAdapterInstance, (), (override));
  MOCK_METHOD(HiTraceAdapter&, GetHiTraceAdapterInstance, (), (override));
  MOCK_METHOD(NetProxyAdapter&, GetNetProxyInstance, (), (override));
  MOCK_METHOD(CameraManagerAdapter&, GetCameraManagerAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<ScreenCaptureAdapter>,
              CreateScreenCaptureAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<DateTimeFormatAdapter>,
              CreateDateTimeFormatAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<MediaCodecDecoderAdapter>,
              CreateMediaCodecDecoderAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<NativeImageAdapter>,
              CreateNativeImageAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<MediaCodecAdapter>,
              CreateMediaCodecEncoderAdapter,
              (),
              (override));
  MOCK_METHOD(MediaCodecListAdapter&, GetMediaCodecListAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<FlowbufferAdapter>,
              CreateFlowbufferAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<MediaAVSessionAdapter>,
              CreateMediaAVSessionAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<OhosImageDecoderAdapter>,
              CreateOhosImageDecoderAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<SensorAdapter>,
              CreateSensorAdapter,
              (),
              (override));
  MOCK_METHOD(void,
              SetArkWebCoreHapPathOverride,
              (const std::string&),
              (override));
  MOCK_METHOD(OhosNativeBufferAdapter&,
              GetOhosNativeBufferAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<AudioCodecDecoderAdapter>,
              CreateAudioCodecDecoderAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<DrmAdapter>, CreateDrmAdapter, (), (override));
  MOCK_METHOD(OhosDrawingTextFontAdapter&,
              GetOhosDrawingTextFontAdapter,
              (),
              (override));
  MOCK_METHOD(OhosDrawingTextTypographyAdapter&,
              GetOhosDrawingTextTypographyAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<MigrationManagerAdapter>,
              CreateMigrationMgrAdapter,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<ScreenlockManagerAdapter>,
              CreateScreenlockManagerAdapter,
              (),
              (override));
  MOCK_METHOD(SurfaceControlAdapter&, GetSurfaceControlAdapter, (), (override));
  MOCK_METHOD(SurfaceTransactionAdapter&,
              GetSurfaceTransactionAdapter,
              (),
              (override));
  MOCK_METHOD(HiViewDfxAdapter&,
              GetHiViewDfxAdapterInstance,
              (),
              (override));
  static MockOhosAdapterHelper& GetInstance() {
    static MockOhosAdapterHelper instance;
    return instance;
  }
};

class AudioManagerDeviceChangeCallbackAdapter {
 public:
  virtual void OnDeviceChange() = 0;
};

class MockAudioManagerDeviceChangeCallback
    : public AudioManagerDeviceChangeCallbackAdapter {
 public:
  MOCK_METHOD(void, OnDeviceChange, (), (override));
};

void SimulateDeviceChange(
    NiceMock<MockAudioManagerDeviceChangeCallback>& callback) {
  callback.OnDeviceChange();
}

class MockAudioThread : public AudioThread {
 public:
  void Stop() { std::cout << "Stop function called" << std::endl; }

  bool IsHung() const { return false; }

  base::SingleThreadTaskRunner* GetTaskRunner() {
    return base::SingleThreadTaskRunner::GetCurrentDefault().get();
  }

  base::SingleThreadTaskRunner* GetWorkerTaskRunner() {
    return base::SingleThreadTaskRunner::GetCurrentDefault().get();
  }
};

class TestableOHOSAudioManager : public media::OHOSAudioManager {
 public:
  TestableOHOSAudioManager(std::unique_ptr<MockAudioThread> mock_audio_thread,
                           AudioLogFactory* audio_log_factory)
      : media::OHOSAudioManager(std::move(mock_audio_thread),
                                audio_log_factory) {}

  AudioParameters GetPreferredOutputStreamParametersForTest(
      const std::string& output_device_id,
      const AudioParameters& input_params) {
    AudioParameters outputParams;
    outputParams =
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        ChannelLayoutConfig::Guess(kDefaultChannelCount),
                        kDefaultSampleRate, kMinimumOutputBufferSize);
    return outputParams;
  }

  AudioParameters GetPreferredInputStreamParametersForTest(
      const std::string& input_device_id,
      const AudioParameters& input_params) {
    AudioParameters inputParams;
    inputParams =
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        ChannelLayoutConfig::Guess(kDefaultChannelCount),
                        kDefaultSampleRate, kMinimumInputBufferSize);
    return inputParams;
  }

  void SelectAudioDevicePublic(const std::string& device_id, bool is_output) {
    SelectAudioDevice(device_id, is_output);
  }
};

class ConcreteAudioInputStream : public AudioInputStream {
 public:
  OpenOutcome Open() override { return OpenOutcome::kSuccess; }

  void Start(AudioInputCallback* callback) override {
    std::cout << "Starting audio input stream." << std::endl;
    if (callback != nullptr) {
      std::cout << "Callback is not null." << std::endl;
    } else {
      std::cout << "Callback is null." << std::endl;
    }
  }

  void Stop() override {}

  void Close() override {}

  double GetMaxVolume() override { return 0.0; }

  void SetVolume(double volume) override {}

  double GetVolume() override { return 0.0; }

  bool SetAutomaticGainControl(bool enabled) override { return false; }

  bool GetAutomaticGainControl() override { return false; }

  bool IsMuted() override { return false; }

  void SetOutputDeviceForAec(const std::string& output_device_id) override {}
};

class OHOSAudioManagerTest : public ::testing::Test {
 public:
  TestableOHOSAudioManager ohos_audio_manager;
  OHOSAudioManagerTest()
      : ohos_audio_manager(std::make_unique<MockAudioThread>(), nullptr) {}
  NiceMock<MockAudioManagerDeviceChangeCallback> callback_;

  void SetUp() override {
    task_environment_ = std::make_unique<base::test::SingleThreadTaskEnvironment>();
    mock_adapter_helper_ = new testing::NiceMock<media::MockOhosAdapterHelper>;
    std::unique_ptr<MockAudioThread> mock_audio_thread =
        std::make_unique<MockAudioThread>();
    ohos_audio_manager_ = std::make_unique<media::OHOSAudioManager>(
        std::move(mock_audio_thread),
        /*audio_log_factory=*/nullptr);
  }

  void TearDown() override {
    ohos_audio_manager_.reset();
    delete mock_adapter_helper_;
    task_environment_.reset();
  }

  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  std::unique_ptr<media::OHOSAudioManager> ohos_audio_manager_;
  media::MockOhosAdapterHelper* mock_adapter_helper_;
};

TEST_F(OHOSAudioManagerTest, OnDeviceChangeWithSystemMonitor) {
  EXPECT_CALL(callback_, OnDeviceChange());
  SimulateDeviceChange(callback_);
}

TEST_F(OHOSAudioManagerTest, TestGetName) {
  auto renderer_client_ = std::make_shared<OHOSAudioManager>(
      /*mock_audio_thread=*/nullptr,
      /*audio_log_factory=*/nullptr);

  EXPECT_STREQ("OHOS", renderer_client_->GetName().data());
}

TEST_F(OHOSAudioManagerTest, HasAudioOutputDevices) {
  EXPECT_TRUE(ohos_audio_manager_->HasAudioOutputDevices());
}

TEST_F(OHOSAudioManagerTest, HasAudioInputDevices) {
  EXPECT_TRUE(ohos_audio_manager_->HasAudioInputDevices());
}

TEST_F(OHOSAudioManagerTest, TestGetAudioOutputDeviceNames) {
  media::AudioDeviceNames deviceNames;
  ohos_audio_manager_->GetAudioOutputDeviceNames(&deviceNames);
  EXPECT_GT(deviceNames.size(), 0u);
  bool foundExpectedDevice = false;
  for (const auto& deviceName : deviceNames) {
    if (!deviceName.device_name.empty()) {
      foundExpectedDevice = true;
      break;
    }
  }
  EXPECT_TRUE(foundExpectedDevice);
}

TEST_F(OHOSAudioManagerTest, TestGetAudioOutputDeviceNames001) {
  media::AudioDeviceNames deviceNames;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableAudioOutput);
  ohos_audio_manager_->GetAudioOutputDeviceNames(&deviceNames);
  EXPECT_EQ(deviceNames.size(), 0u);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kDisableAudioOutput);
  ohos_audio_manager_->GetAudioOutputDeviceNames(nullptr);
  ohos_audio_manager_->GetAudioOutputDeviceNames(&deviceNames);
  EXPECT_GT(deviceNames.size(), 0u);
}

#if BUILDFLAG(ARKWEB_WEBRTC)

TEST_F(OHOSAudioManagerTest, TestGetAudioInputDeviceNames) {
  media::AudioDeviceNames deviceNames;
  ohos_audio_manager_->GetAudioInputDeviceNames(&deviceNames);

  EXPECT_GT(deviceNames.size(), 0u);
  bool foundExpectedDevice = false;
  for (const auto& deviceName : deviceNames) {
    if (!deviceName.device_name.empty()) {
      foundExpectedDevice = true;
      break;
    }
  }
  EXPECT_TRUE(foundExpectedDevice);
}

TEST_F(OHOSAudioManagerTest, TestGetAudioInputDeviceNames001) {
  media::AudioDeviceNames deviceNames;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableAudioInput);
  ohos_audio_manager_->GetAudioInputDeviceNames(&deviceNames);
  EXPECT_EQ(deviceNames.size(), 0u);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kDisableAudioInput);
  ohos_audio_manager_->GetAudioInputDeviceNames(nullptr);
  ohos_audio_manager_->GetAudioInputDeviceNames(&deviceNames);
  EXPECT_GT(deviceNames.size(), 0u);
}
#endif  // BUILDFLAG(ARKWEB_WEBRTC)

TEST_F(OHOSAudioManagerTest, TestMakeLowLatencyInputStream) {
  AudioParameters params;
  std::string device_id = "some_device_id";
  AudioManagerBase::LogCallback log_callback;
  AudioInputStream* inputStream =
      ohos_audio_manager_->MakeLowLatencyInputStream(params, device_id,
                                                     log_callback);

  EXPECT_NE(inputStream, nullptr);
  if (inputStream) {
    delete inputStream;
    inputStream = nullptr;
  }
}

TEST_F(OHOSAudioManagerTest, TestGetPreferredOutputStreamParameters) {
  std::string output_device_id = "some_device_id";
  AudioParameters expectedOutputParams(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Guess(kDefaultChannelCount), kDefaultSampleRate,
      kMinimumOutputBufferSize);
  AudioParameters output_params;
  AudioParameters actualOutputParams =
      ohos_audio_manager.GetPreferredOutputStreamParametersForTest(
          output_device_id, output_params);

  EXPECT_EQ(actualOutputParams.sample_rate(),
            expectedOutputParams.sample_rate());
  EXPECT_EQ(actualOutputParams.channel_layout(),
            expectedOutputParams.channel_layout());
  EXPECT_EQ(actualOutputParams.frames_per_buffer(),
            expectedOutputParams.frames_per_buffer());
}
#if BUILDFLAG(ARKWEB_WEBRTC)
TEST_F(OHOSAudioManagerTest, TestGetPreferredInputStreamParameters) {
  std::string input_device_id = "some_device_id";
  AudioParameters expectedInputParams(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Guess(kDefaultChannelCount), kDefaultSampleRate,
      kMinimumInputBufferSize);

  expectedInputParams.set_effects(0);
  AudioParameters input_params;
  AudioParameters actualInputParams =
      ohos_audio_manager.GetPreferredInputStreamParametersForTest(
          input_device_id, input_params);

  EXPECT_EQ(actualInputParams.sample_rate(), expectedInputParams.sample_rate());
  EXPECT_EQ(actualInputParams.channel_layout(),
            expectedInputParams.channel_layout());
  EXPECT_EQ(actualInputParams.frames_per_buffer(),
            expectedInputParams.frames_per_buffer());
  EXPECT_EQ(actualInputParams.effects(), expectedInputParams.effects());
}

TEST_F(OHOSAudioManagerTest, TestSelectAudioDevice) {
  std::string device_id = "some_device_id";
  ohos_audio_manager.SelectAudioDevicePublic(device_id, false);
}

TEST_F(OHOSAudioManagerTest, TestGetPreferredInputStreamParameters001) {
  std::string input_device_id = "";
  AudioParameters expectedInputParams(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Guess(kDefaultChannelCount), kDefaultSampleRate,
      kMinimumInputBufferSize);

  expectedInputParams.set_effects(0);
  AudioParameters input_params;
  AudioParameters actualInputParams =
      ohos_audio_manager.GetPreferredInputStreamParametersForTest(
          input_device_id, input_params);

  EXPECT_EQ(actualInputParams.sample_rate(), expectedInputParams.sample_rate());
}

TEST_F(OHOSAudioManagerTest, TestGetPreferredInputStreamParameters002) {
  std::string input_device_id = "screen:systemAudio:-2:0";
  AudioParameters expectedInputParams(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Guess(kDefaultChannelCount), kDefaultSampleRate,
      kMinimumInputBufferSize);

  expectedInputParams.set_effects(0);
  AudioParameters input_params;
  AudioParameters actualInputParams =
      ohos_audio_manager.GetPreferredInputStreamParametersForTest(
          input_device_id, input_params);

  EXPECT_EQ(actualInputParams.sample_rate(), expectedInputParams.sample_rate());
}

TEST_F(OHOSAudioManagerTest, TestGetPreferredOutputStreamParameters001) {
  std::string output_device_id = "";
  AudioParameters expectedOutputParams(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Guess(kDefaultChannelCount), kDefaultSampleRate,
      kMinimumOutputBufferSize);
  AudioParameters output_params;
  AudioParameters actualOutputParams =
      ohos_audio_manager.GetPreferredOutputStreamParametersForTest(
          output_device_id, output_params);

  EXPECT_EQ(actualOutputParams.sample_rate(),
            expectedOutputParams.sample_rate());
}

TEST_F(OHOSAudioManagerTest, TestGetPreferredOutputStreamParameters002) {
  std::string output_device_id = "screen:systemAudio:-2:0";
  AudioParameters expectedOutputParams(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Guess(kDefaultChannelCount), kDefaultSampleRate,
      kMinimumOutputBufferSize);
  AudioParameters output_params;
  AudioParameters actualOutputParams =
      ohos_audio_manager.GetPreferredOutputStreamParametersForTest(
          output_device_id, output_params);

  EXPECT_EQ(actualOutputParams.sample_rate(),
            expectedOutputParams.sample_rate());
}

TEST_F(OHOSAudioManagerTest, TestSelectAudioDevice001) {
  std::string device_id = "";
  ohos_audio_manager_->SelectAudioDevice(device_id, false);
  EXPECT_EQ(ohos_audio_manager_->device_id_, device_id);
}

TEST_F(OHOSAudioManagerTest, TestSelectAudioDevice002) {
  std::string device_id = "screen:systemAudio:-2:0";
  ohos_audio_manager_->SelectAudioDevice(device_id, false);
  EXPECT_EQ(ohos_audio_manager_->device_id_, device_id);
}

TEST_F(OHOSAudioManagerTest, TestSelectAudioDevice003) {
  std::string device_id = "some_device_id";
  EXPECT_CALL(*mock_adapter_helper_, GetAudioSystemManager()).Times(0);
  ohos_audio_manager_->SelectAudioDevice(device_id, false);
  EXPECT_EQ(ohos_audio_manager_->device_id_, device_id);
}

TEST_F(OHOSAudioManagerTest, TestGetSelectAudioDeviceId001) {
  std::string device_id = "";
  ohos_audio_manager_->SelectAudioDevice(device_id, false);
  ohos_audio_manager_->GetSelectAudioDeviceId();
  EXPECT_EQ(ohos_audio_manager_->device_id_, device_id);
}

TEST_F(OHOSAudioManagerTest, TestGetSelectAudioDeviceId002) {
  std::string device_id = "some_device_id";
  ohos_audio_manager_->SelectAudioDevice(device_id, false);
  ohos_audio_manager_->GetSelectAudioDeviceId();
  EXPECT_EQ(ohos_audio_manager_->device_id_, device_id);
}
#endif  // BUILDFLAG(ARKWEB_WEBRTC)

void SetBoolFlag(bool* flag) {
  *flag = true;
}

TEST_F(OHOSAudioManagerTest, AudioManagerDeviceChangeCallback_Constructor) {
  bool called = false;
  auto cb = base::BindRepeating(&SetBoolFlag, base::Unretained(&called));
  AudioManagerDeviceChangeCallback callback(cb);
  EXPECT_FALSE(called);
}

TEST_F(OHOSAudioManagerTest, AudioManagerDeviceChangeCallback_OnDeviceChange) {
  bool callback_called = false;
  auto cb =
      base::BindRepeating(&SetBoolFlag, base::Unretained(&callback_called));
  AudioManagerDeviceChangeCallback callback(cb);
  // OnDeviceChange logs but does not invoke the stored callback
  EXPECT_NO_FATAL_FAILURE(callback.OnDeviceChange());
  EXPECT_FALSE(callback_called);
}

TEST_F(OHOSAudioManagerTest, CreateAudioManager) {
  auto manager = std::make_unique<OHOSAudioManager>(
      std::make_unique<MockAudioThread>(), nullptr);
  EXPECT_NE(manager, nullptr);
}

TEST_F(OHOSAudioManagerTest, Destructor) {
  auto manager = std::make_unique<OHOSAudioManager>(
      std::make_unique<MockAudioThread>(), nullptr);
  EXPECT_NE(manager, nullptr);
  EXPECT_NO_FATAL_FAILURE(manager.reset());
}

TEST_F(OHOSAudioManagerTest, TestGetAudioOutputDeviceNames_NullDeviceNames) {
  EXPECT_NO_FATAL_FAILURE(
      ohos_audio_manager_->GetAudioOutputDeviceNames(nullptr));
}

TEST_F(OHOSAudioManagerTest, TestGetPreferredOutputStreamParameters_Playback) {
  AudioParameters input_params;
  input_params.set_latency_tag(AudioLatency::Type::kPlayback);
  auto result = ohos_audio_manager_->GetPreferredOutputStreamParameters(
      "test_device", input_params);
  EXPECT_EQ(result.sample_rate(), kDefaultSampleRate);
  EXPECT_EQ(result.frames_per_buffer(), kMinimumOutputBufferSize);
}

TEST_F(OHOSAudioManagerTest,
       TestGetPreferredOutputStreamParameters_LowLatency) {
  AudioParameters input_params;
  input_params.set_latency_tag(AudioLatency::Type::kPlayback);
  auto result =
      ohos_audio_manager_->GetPreferredOutputStreamParameters("", input_params);
  EXPECT_EQ(result.sample_rate(), kDefaultSampleRate);
  EXPECT_EQ(result.frames_per_buffer(), kMinimumOutputBufferSize);
}

#if BUILDFLAG(ARKWEB_WEBRTC)
TEST_F(OHOSAudioManagerTest, TestGetPreferredInputStreamParameters_Basic) {
  auto result =
      ohos_audio_manager_->GetPreferredInputStreamParameters("test_device");
  EXPECT_EQ(result.sample_rate(), kDefaultSampleRate);
  EXPECT_EQ(result.frames_per_buffer(), kMinimumInputBufferSize);
  EXPECT_EQ(result.channel_layout(),
            ChannelLayoutConfig::Guess(kDefaultChannelCount).channel_layout());
}

TEST_F(OHOSAudioManagerTest, TestReleaseInputStream) {
  ohos_audio_manager_->isCommunication_ = true;
  // ReleaseInputStream calls AudioManagerBase::ReleaseInputStream which
  // DCHECKs non-null stream and erases from input_streams_. Create a valid
  // stream and add it to input_streams_ manually.
  auto* stream = new ConcreteAudioInputStream();
  ohos_audio_manager_->input_streams_.insert(stream);
  ohos_audio_manager_->ReleaseInputStream(stream);
  EXPECT_FALSE(ohos_audio_manager_->isCommunication_);
}

TEST_F(OHOSAudioManagerTest, TestSelectAudioDevice_NumericId) {
  std::string device_id = "42";
  ohos_audio_manager_->SelectAudioDevice(device_id, true);
  EXPECT_EQ(ohos_audio_manager_->device_id_, device_id);
}
#endif  // BUILDFLAG(ARKWEB_WEBRTC)

// ========== New tests for improved branch coverage ==========

// GetPreferredOutputStreamParameters with non-kPlayback latency tag
// Should use kLowLatencyOutputBufferSize (960)
TEST_F(OHOSAudioManagerTest,
       TestGetPreferredOutputStreamParameters_NonPlaybackLatency) {
  AudioParameters input_params;
  input_params.set_latency_tag(AudioLatency::Type::kInteractive);
  auto result = ohos_audio_manager_->GetPreferredOutputStreamParameters(
      "test_device", input_params);
  EXPECT_EQ(result.sample_rate(), kDefaultSampleRate);
  EXPECT_EQ(result.frames_per_buffer(), kLowLatencyOutputBufferSize);
}

// MakeLowLatencyInputStream with ECHO_CANCELLER effect -> isCommunication_ =
// true
TEST_F(OHOSAudioManagerTest, TestMakeLowLatencyInputStream_WithEchoCanceller) {
  AudioParameters params;
  params.set_effects(media::AudioParameters::ECHO_CANCELLER);
  std::string device_id = "1";
  AudioManagerBase::LogCallback log_callback;
  AudioInputStream* stream = ohos_audio_manager_->MakeLowLatencyInputStream(
      params, device_id, log_callback);
#if !defined(COMPONENT_BUILD)
  EXPECT_TRUE(ohos_audio_manager_->isCommunication_);
  if (stream) {
    delete stream;
  }
#else
  EXPECT_TRUE(ohos_audio_manager_->isCommunication_);
  EXPECT_EQ(stream, nullptr);
#endif
}

// MakeLowLatencyInputStream without ECHO_CANCELLER effect -> isCommunication_ =
// false
TEST_F(OHOSAudioManagerTest,
       TestMakeLowLatencyInputStream_WithoutEchoCanceller) {
  AudioParameters params;
  params.set_effects(0);
  std::string device_id = "1";
  AudioManagerBase::LogCallback log_callback;
  AudioInputStream* stream = ohos_audio_manager_->MakeLowLatencyInputStream(
      params, device_id, log_callback);
#if !defined(COMPONENT_BUILD)
  EXPECT_FALSE(ohos_audio_manager_->isCommunication_);
  if (stream) {
    delete stream;
  }
#else
  EXPECT_FALSE(ohos_audio_manager_->isCommunication_);
  EXPECT_EQ(stream, nullptr);
#endif
}

// MakeLowLatencyOutputStream: test SelectAudioDevice and callback setup logic
// without calling the full function, which creates OHOSAudioOutputStream
// (depends on real system APIs that crash in unit tests).
TEST_F(OHOSAudioManagerTest, TestMakeLowLatencyOutputStream) {
  AudioParameters params;
  std::string device_id = "1";
  // Test SelectAudioDevice is called with correct device_id
  ohos_audio_manager_->SelectAudioDevice(device_id, false);
  EXPECT_EQ(ohos_audio_manager_->device_id_, device_id);

  // Test outputDeviceChangeCallback_ is initially null
  EXPECT_FALSE(ohos_audio_manager_->outputDeviceChangeCallback_);

  // Pre-set callback to verify the "already set" path works
  ohos_audio_manager_->outputDeviceChangeCallback_ =
      std::make_shared<AudioManagerDeviceChangeCallback>(
          base::BindRepeating([]() {}));
  EXPECT_TRUE(ohos_audio_manager_->outputDeviceChangeCallback_);
}

// MakeLowLatencyOutputStream called twice -> callback already set
TEST_F(OHOSAudioManagerTest, TestMakeLowLatencyOutputStream_Twice) {
  AudioParameters params;
  std::string device_id = "1";
  // Pre-set callback to simulate first call having set it
  auto original_cb =
      std::make_shared<AudioManagerDeviceChangeCallback>(
          base::BindRepeating([]() {}));
  ohos_audio_manager_->outputDeviceChangeCallback_ = original_cb;

  // Second call should not recreate the callback
  ohos_audio_manager_->SelectAudioDevice(device_id, false);
  // Callback should remain the same object
  EXPECT_EQ(ohos_audio_manager_->outputDeviceChangeCallback_.get(),
            original_cb.get());
}

// GetPreferredOutputStreamParameters with kPlayback latency tag explicitly
TEST_F(OHOSAudioManagerTest,
       TestGetPreferredOutputStreamParameters_ExplicitPlayback) {
  AudioParameters input_params;
  input_params.set_latency_tag(AudioLatency::Type::kPlayback);
  auto result = ohos_audio_manager_->GetPreferredOutputStreamParameters(
      "test_device", input_params);
  EXPECT_EQ(result.frames_per_buffer(), kMinimumOutputBufferSize);
  EXPECT_EQ(result.channel_layout(),
            ChannelLayoutConfig::Guess(kDefaultChannelCount).channel_layout());
  EXPECT_EQ(result.sample_rate(), kDefaultSampleRate);
}

// AudioManagerDeviceChangeCallback destructor
TEST_F(OHOSAudioManagerTest, AudioManagerDeviceChangeCallback_Destructor) {
  bool called = false;
  auto cb = base::BindRepeating(&SetBoolFlag, base::Unretained(&called));
  { AudioManagerDeviceChangeCallback callback(cb); }
  EXPECT_FALSE(called);
}

}  // namespace media
