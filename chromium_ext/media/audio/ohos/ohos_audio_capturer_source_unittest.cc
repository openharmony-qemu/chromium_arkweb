/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "base/files/file_util.h"
#define private public
#include "base/task/single_thread_task_runner.h"
#include "media/audio/ohos/ohos_audio_capturer_source.h"
#undef private
#include "audio_capturer_adapter.h"
#include "base/logging.h"
#include "buffer_desc_adapter_impl.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "ohos_adapter_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace media;
using namespace testing;

namespace {
constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultChannelCount = 2;
constexpr int kMinimumInputBufferSize = 2048;
}  // namespace

class MockCaptureCallback : public media::AudioCapturerSource::CaptureCallback {
 public:
  MockCaptureCallback() = default;
  virtual ~MockCaptureCallback() = default;

  // Mock 所有虚函数
  MOCK_METHOD(void, OnCaptureStarted, (), (override));

  MOCK_METHOD(void,
              Capture,
              (const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               const media::AudioGlitchInfo& glitch_info,
               double volume),
              (override));

  MOCK_METHOD(void,
              OnCaptureError,
              (media::AudioCapturerSource::ErrorCode code,
               const std::string& message),
              (override));

  MOCK_METHOD(void, OnCaptureMuted, (bool is_muted), (override));

  MOCK_METHOD(void, OnCapturePaused, (bool is_paused), (override));

  MOCK_METHOD(void,
              OnCaptureProcessorCreated,
              (media::AudioProcessorControls * controls),
              (override));
};

class MockAudioCapturerAdapter : public OHOS::NWeb::AudioCapturerAdapter {
 public:
  MockAudioCapturerAdapter() = default;
  virtual ~MockAudioCapturerAdapter() = default;

  // Mock 所有纯虚函数
  MOCK_METHOD(
      int32_t,
      Create,
      (const std::shared_ptr<AudioCapturerOptionsAdapter> capturerOptions,
       std::string cachePath),
      (override));

  MOCK_METHOD(bool, Start, (), (override));
  MOCK_METHOD(bool, Stop, (), (override));
  MOCK_METHOD(bool, Release, (), (override));

  MOCK_METHOD(int32_t,
              SetCapturerReadCallback,
              (std::shared_ptr<AudioCapturerReadCallbackAdapter> callback),
              (override));

  MOCK_METHOD(int32_t,
              GetBufferDesc,
              (std::shared_ptr<BufferDescAdapter> bufferDesc),
              (override));

  MOCK_METHOD(int32_t,
              Enqueue,
              (const std::shared_ptr<BufferDescAdapter> bufferDesc),
              (override));

  MOCK_METHOD(int32_t, GetFrameCount, (uint32_t & frameCount), (override));

  MOCK_METHOD(int64_t, GetAudioTime, (), (override));

  MOCK_METHOD(bool, Pause, (), (override));
};

class MockReadDataCallback {
 public:
  MOCK_METHOD(void, Run, (), ());
};

class AudioCapturerReadCallbackTest : public ::testing::Test {
 public:
  AudioCapturerReadCallbackTest()
      : mock_callback(),
        read_callback(base::BindRepeating(&MockReadDataCallback::Run,
                                          base::Unretained(&mock_callback))) {}

 protected:
  MockReadDataCallback mock_callback;
  AudioCapturerReadCallback read_callback;
};

TEST_F(AudioCapturerReadCallbackTest, OnReadDataTest) {
  size_t length = 10;
  bool result = read_callback.readDataCallback_.is_null();
  EXPECT_CALL(mock_callback, Run()).Times(1);
  read_callback.OnReadData(length);
  EXPECT_FALSE(result);
}

TEST_F(AudioCapturerReadCallbackTest, OnReadDataNoCallbackTest) {
  size_t length = 10;
  read_callback.readDataCallback_ = base::RepeatingCallback<void()>();
  bool result = read_callback.readDataCallback_.is_null();
  EXPECT_CALL(mock_callback, Run()).Times(0);
  read_callback = AudioCapturerReadCallback(base::RepeatingCallback<void()>());
  read_callback.OnReadData(length);
  EXPECT_TRUE(result);
}

namespace base {

class OHOSAudioCapturerSourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    audio_capturer_source_ = new OHOSAudioCapturerSource(capturer_task_runner_);
    mock_callback_ = std::make_unique<MockCaptureCallback>();
  }

  void TearDown() override {
    delete audio_capturer_source_;
    audio_capturer_source_ = nullptr;
  }

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> capturer_task_runner_;
  OHOSAudioCapturerSource* audio_capturer_source_;
  std::unique_ptr<MockCaptureCallback> mock_callback_;
};

TEST_F(OHOSAudioCapturerSourceTest, Initialize01) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);

  EXPECT_CALL(*mock_capturer_ptr, Create(testing::_, testing::_)).Times(1);

  auto params =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);

  audio_capturer_source_->Initialize(params, mock_callback_.get());
}

TEST_F(OHOSAudioCapturerSourceTest, Initialize02) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);

  EXPECT_CALL(*mock_capturer_ptr, Create(testing::_, testing::_)).Times(0);

  auto params =
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);

  audio_capturer_source_->Initialize(params, mock_callback_.get());
}

TEST_F(OHOSAudioCapturerSourceTest, Start01) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  EXPECT_CALL(*mock_capturer_ptr, Start()).Times(1).WillOnce(Return(true));
  audio_capturer_source_->Start();
}

TEST_F(OHOSAudioCapturerSourceTest, Start02) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  EXPECT_CALL(*mock_capturer_ptr, Start()).Times(1).WillOnce(Return(false));
  audio_capturer_source_->Start();
}

TEST_F(OHOSAudioCapturerSourceTest, Stop01) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_.get();

  EXPECT_CALL(*mock_capturer_ptr, Stop()).Times(1).WillOnce(Return(false));
  audio_capturer_source_->Stop();
  EXPECT_EQ(audio_capturer_source_->callback_, nullptr);
}

TEST_F(OHOSAudioCapturerSourceTest, Stop02) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_.get();

  EXPECT_CALL(*mock_capturer_ptr, Stop()).Times(1).WillOnce(Return(true));
  audio_capturer_source_->Stop();
  EXPECT_EQ(audio_capturer_source_->callback_, nullptr);
}

TEST_F(OHOSAudioCapturerSourceTest, Stop03) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = nullptr;

  EXPECT_CALL(*mock_capturer_ptr, Stop()).Times(1).WillOnce(Return(false));
  audio_capturer_source_->Stop();
  EXPECT_EQ(audio_capturer_source_->callback_, nullptr);
}

TEST_F(OHOSAudioCapturerSourceTest, Stop04) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = nullptr;

  EXPECT_CALL(*mock_capturer_ptr, Stop()).Times(1).WillOnce(Return(true));
  audio_capturer_source_->Stop();
  EXPECT_EQ(audio_capturer_source_->callback_, nullptr);
}

TEST_F(OHOSAudioCapturerSourceTest, ReadData01) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = nullptr;
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_.get();
  audio_capturer_source_->is_stopped_ = false;
  audio_capturer_source_->params_ =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);
  audio_capturer_source_->frameCount_ = 2 * kDefaultSampleRate / 100;

  EXPECT_CALL(*mock_capturer_ptr, GetBufferDesc(testing::_)).Times(0);
  EXPECT_CALL(*mock_capturer_ptr, Enqueue(testing::_)).Times(0);
  audio_capturer_source_->ReadData();
}

TEST_F(OHOSAudioCapturerSourceTest, ReadData02) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_.get();
  audio_capturer_source_->is_stopped_ = true;
  audio_capturer_source_->params_ =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);
  audio_capturer_source_->frameCount_ = 2 * kDefaultSampleRate / 100;

  EXPECT_CALL(*mock_capturer_ptr, GetBufferDesc(testing::_)).Times(0);
  EXPECT_CALL(*mock_capturer_ptr, Enqueue(testing::_)).Times(0);
  audio_capturer_source_->ReadData();
}

TEST_F(OHOSAudioCapturerSourceTest, ReadData03) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
  EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockCaptureCallback* mock_callback_ptr = mock_callback_.get();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_ptr;
  audio_capturer_source_->is_stopped_ = false;
  audio_capturer_source_->params_ =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);
  audio_capturer_source_->frameCount_ = 2 * kDefaultSampleRate / 100 + 1;

  EXPECT_CALL(*mock_capturer_ptr, GetBufferDesc(testing::_)).Times(1);
  EXPECT_CALL(*mock_capturer_ptr, Enqueue(testing::_)).Times(1);
  EXPECT_CALL(*mock_callback_ptr,
              Capture(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  audio_capturer_source_->ReadData();
}

TEST_F(OHOSAudioCapturerSourceTest, NotifyCaptureError01) {
  MockCaptureCallback* mock_callback_ptr = mock_callback_.get();
  audio_capturer_source_->callback_ = nullptr;

  EXPECT_CALL(*mock_callback_ptr, OnCaptureError(testing::_, testing::_))
      .Times(0);
  const std::string& message = "test";
  audio_capturer_source_->NotifyCaptureError(message);
}

TEST_F(OHOSAudioCapturerSourceTest, NotifyCaptureError02) {
  MockCaptureCallback* mock_callback_ptr = mock_callback_.get();
  audio_capturer_source_->callback_ = mock_callback_ptr;

  EXPECT_CALL(*mock_callback_ptr, OnCaptureError(testing::_, testing::_))
      .Times(1);
  const std::string& message = "test";
  audio_capturer_source_->NotifyCaptureError(message);
}

TEST_F(OHOSAudioCapturerSourceTest, NotifyCaptureStarted01) {
  MockCaptureCallback* mock_callback_ptr = mock_callback_.get();
  audio_capturer_source_->callback_ = nullptr;

  EXPECT_CALL(*mock_callback_ptr, OnCaptureStarted()).Times(0);
  audio_capturer_source_->NotifyCaptureStarted();
}

TEST_F(OHOSAudioCapturerSourceTest, NotifyCaptureStarted02) {
  MockCaptureCallback* mock_callback_ptr = mock_callback_.get();
  audio_capturer_source_->callback_ = mock_callback_ptr;

  EXPECT_CALL(*mock_callback_ptr, OnCaptureStarted()).Times(1);
  audio_capturer_source_->NotifyCaptureStarted();
}

TEST_F(OHOSAudioCapturerSourceTest, Initialize_NullCapturer) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  audio_capturer_source_->capturer_ = nullptr;

  auto params =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);

  audio_capturer_source_->Initialize(params, mock_callback_.get());
  EXPECT_EQ(audio_capturer_source_->callback_, mock_callback_.get());
}

TEST_F(OHOSAudioCapturerSourceTest, Initialize_WithEchoCanceller) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);

  EXPECT_CALL(*mock_capturer_ptr, Create(testing::_, testing::_)).Times(1);

  auto params =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);
  params.set_effects(media::AudioParameters::ECHO_CANCELLER);

  audio_capturer_source_->Initialize(params, mock_callback_.get());
}

TEST_F(OHOSAudioCapturerSourceTest, Start_NullCapturer) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  audio_capturer_source_->capturer_ = nullptr;
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_.get();

  audio_capturer_source_->Start();
}

TEST_F(OHOSAudioCapturerSourceTest, ReadData_WithCallbackAndValidFrameCount) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockCaptureCallback* mock_callback_ptr = mock_callback_.get();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_ptr;
  audio_capturer_source_->is_stopped_ = false;
  audio_capturer_source_->params_ =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);
  audio_capturer_source_->frameCount_ = 2 * kDefaultSampleRate / 100;

  // Set up a valid buffer for GetBufferDesc so FromInterleaved has valid data
  static std::vector<int16_t> dummy_buffer(
      kDefaultChannelCount * kDefaultSampleRate / 100 * 2, 0);
  EXPECT_CALL(*mock_capturer_ptr, GetBufferDesc(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([](std::shared_ptr<BufferDescAdapter> desc) {
        auto* impl = static_cast<OHOS::NWeb::BufferDescAdapterImpl*>(desc.get());
        impl->SetBuffer(reinterpret_cast<uint8_t*>(dummy_buffer.data()));
        impl->SetBufLength(dummy_buffer.size() * sizeof(int16_t));
        return 0;
      }));
  EXPECT_CALL(*mock_capturer_ptr, GetAudioTime()).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*mock_capturer_ptr, Enqueue(testing::_)).Times(1);
  EXPECT_CALL(*mock_callback_ptr,
              Capture(testing::_, testing::_, testing::_, testing::_))
      .Times(1);

  audio_capturer_source_->ReadData();
}

TEST_F(OHOSAudioCapturerSourceTest, ReadData_NullCallback) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = nullptr;
  audio_capturer_source_->is_stopped_ = false;
  audio_capturer_source_->params_ =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);
  audio_capturer_source_->frameCount_ = 2 * kDefaultSampleRate / 100;

  // Set up a valid buffer for GetBufferDesc so FromInterleaved has valid data
  static std::vector<int16_t> dummy_buffer(
      kDefaultChannelCount * kDefaultSampleRate / 100 * 2, 0);
  EXPECT_CALL(*mock_capturer_ptr, GetBufferDesc(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([](std::shared_ptr<BufferDescAdapter> desc) {
        auto* impl = static_cast<OHOS::NWeb::BufferDescAdapterImpl*>(desc.get());
        impl->SetBuffer(reinterpret_cast<uint8_t*>(dummy_buffer.data()));
        impl->SetBufLength(dummy_buffer.size() * sizeof(int16_t));
        return 0;
      }));
  EXPECT_CALL(*mock_capturer_ptr, GetAudioTime()).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*mock_capturer_ptr, Enqueue(testing::_)).Times(1);

  audio_capturer_source_->ReadData();
}

TEST_F(OHOSAudioCapturerSourceTest, SetOutputDeviceForAec) {
  EXPECT_NO_FATAL_FAILURE(
      audio_capturer_source_->SetOutputDeviceForAec("test_device_id"));
}

TEST_F(OHOSAudioCapturerSourceTest, Pause_NullCapturer) {
  audio_capturer_source_->capturer_ = nullptr;
  EXPECT_NO_FATAL_FAILURE(audio_capturer_source_->Pause());
  EXPECT_FALSE(audio_capturer_source_->is_stopped_.load());
}

TEST_F(OHOSAudioCapturerSourceTest, Pause_Failed) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_.get();

  EXPECT_CALL(*mock_capturer_ptr, Pause()).Times(1).WillOnce(Return(false));

  audio_capturer_source_->Pause();
}

TEST_F(OHOSAudioCapturerSourceTest, Pause_Success) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockCaptureCallback* mock_callback_ptr = mock_callback_.get();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_ptr;

  EXPECT_CALL(*mock_capturer_ptr, Pause()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*mock_callback_ptr, OnCapturePaused(true)).Times(1);

  audio_capturer_source_->Pause();
}

TEST_F(OHOSAudioCapturerSourceTest, Resume_NullCapturer) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  audio_capturer_source_->capturer_ = nullptr;
  audio_capturer_source_->audioCapturerReadCallback_ = nullptr;
  audio_capturer_source_->callback_ = mock_callback_.get();
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  EXPECT_NO_FATAL_FAILURE(audio_capturer_source_->Resume());
}

TEST_F(OHOSAudioCapturerSourceTest, Resume_Failed) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->audioCapturerReadCallback_ =
      std::make_shared<AudioCapturerReadCallback>(base::BindRepeating([]() {}));
  audio_capturer_source_->callback_ = mock_callback_.get();
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  EXPECT_CALL(*mock_capturer_ptr, Start()).Times(1).WillOnce(Return(false));

  audio_capturer_source_->Resume();
}

TEST_F(OHOSAudioCapturerSourceTest, Resume_Success) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockCaptureCallback* mock_callback_ptr = mock_callback_.get();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->audioCapturerReadCallback_ =
      std::make_shared<AudioCapturerReadCallback>(base::BindRepeating([]() {}));
  audio_capturer_source_->callback_ = mock_callback_ptr;
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  EXPECT_CALL(*mock_capturer_ptr, Start()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*mock_callback_ptr, OnCapturePaused(false)).Times(1);

  audio_capturer_source_->Resume();
}

// ========== New tests for improved branch coverage ==========

// SetVolume - under ARKWEB_TEST or ARKWEB_WEBRTC just returns
TEST_F(OHOSAudioCapturerSourceTest, SetVolume) {
  EXPECT_NO_FATAL_FAILURE(audio_capturer_source_->SetVolume(0.5));
}

// SetAutomaticGainControl - under ARKWEB_TEST just returns
TEST_F(OHOSAudioCapturerSourceTest, SetAutomaticGainControl) {
  EXPECT_NO_FATAL_FAILURE(
      audio_capturer_source_->SetAutomaticGainControl(true));
  EXPECT_NO_FATAL_FAILURE(
      audio_capturer_source_->SetAutomaticGainControl(false));
}

// Stop with null capturer - should return early after clearing callback
TEST_F(OHOSAudioCapturerSourceTest, Stop_NullCapturer) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  audio_capturer_source_->capturer_ = nullptr;
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_.get();

  audio_capturer_source_->Stop();
  EXPECT_EQ(audio_capturer_source_->callback_, nullptr);
  EXPECT_TRUE(audio_capturer_source_->is_stopped_.load());
}

// Start with null audioCapturerReadCallback_ but non-null capturer
TEST_F(OHOSAudioCapturerSourceTest, Start_NullReadCallback) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  audio_capturer_source_->capturer_ = nullptr;
  audio_capturer_source_->audioCapturerReadCallback_ = nullptr;
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_.get();

  // Both capturer_ and audioCapturerReadCallback_ are null, should return early
  audio_capturer_source_->Start();
}

// Pause with null callback in the lock section
TEST_F(OHOSAudioCapturerSourceTest, Pause_NullCallback) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = nullptr;

  EXPECT_CALL(*mock_capturer_ptr, Pause()).Times(1).WillOnce(Return(true));

  audio_capturer_source_->Pause();
  // callback_ is null, so OnCapturePaused should NOT be called
}

// Resume with null callback in the lock section
TEST_F(OHOSAudioCapturerSourceTest, Resume_NullCallback) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->audioCapturerReadCallback_ =
      std::make_shared<AudioCapturerReadCallback>(base::BindRepeating([]() {}));
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = nullptr;

  EXPECT_CALL(*mock_capturer_ptr, Start()).Times(1).WillOnce(Return(true));

  audio_capturer_source_->Resume();
  // callback_ is null, so OnCapturePaused should NOT be called
}

// ReportError - posts task to main_task_runner_
TEST_F(OHOSAudioCapturerSourceTest, ReportError) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  audio_capturer_source_->capturer_ =
      std::make_unique<MockAudioCapturerAdapter>();
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_.get();

  EXPECT_NO_FATAL_FAILURE(audio_capturer_source_->ReportError("test error"));
}

// ReadData with small frameCount_ (edge case: frameCount <= 2 * sample_rate /
// 100, just under the 20ms threshold)
TEST_F(OHOSAudioCapturerSourceTest, ReadData_SmallFrameCount) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);

  auto mock_capturer = std::make_unique<MockAudioCapturerAdapter>();
  MockCaptureCallback* mock_callback_ptr = mock_callback_.get();
  MockAudioCapturerAdapter* mock_capturer_ptr = mock_capturer.get();
  audio_capturer_source_->capturer_ = std::move(mock_capturer);
  audio_capturer_source_->main_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  audio_capturer_source_->callback_ = mock_callback_ptr;
  audio_capturer_source_->is_stopped_ = false;
  audio_capturer_source_->params_ =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Guess(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);
  // Use 1 frame - minimum valid frame count for AudioBus
  audio_capturer_source_->frameCount_ = 1;

  static std::vector<int16_t> small_buffer(kDefaultChannelCount * 2, 0);
  EXPECT_CALL(*mock_capturer_ptr, GetBufferDesc(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([](std::shared_ptr<BufferDescAdapter> desc) {
        auto* impl = static_cast<OHOS::NWeb::BufferDescAdapterImpl*>(desc.get());
        impl->SetBuffer(reinterpret_cast<uint8_t*>(small_buffer.data()));
        impl->SetBufLength(small_buffer.size() * sizeof(int16_t));
        return 0;
      }));
  EXPECT_CALL(*mock_capturer_ptr, GetAudioTime()).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*mock_capturer_ptr, Enqueue(testing::_)).Times(1);
  EXPECT_CALL(*mock_callback_ptr,
              Capture(testing::_, testing::_, testing::_, testing::_))
      .Times(1);

  audio_capturer_source_->ReadData();
}

}  // namespace base
