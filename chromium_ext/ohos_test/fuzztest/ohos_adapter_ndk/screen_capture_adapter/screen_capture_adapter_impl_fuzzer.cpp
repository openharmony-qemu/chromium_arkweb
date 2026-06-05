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

#define private public
#include "arkweb/ohos_adapter_ndk/screen_capture_adapter/screen_capture_adapter_impl.h"
#undef private

#include <cstring>
#include <fuzzer/FuzzedDataProvider.h>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ohos_adapter_helper.h"
#include "arkweb/ohos_adapter_ndk/ohos_adapter_helper_ext.h"

using namespace OHOS::NWeb;
 
namespace OHOS {
constexpr int MAX_SET_RATE = 48000;
constexpr int MAX_SET_CHANNELS = 12;
constexpr int MAX_SET_BITRATE = 2000000;
constexpr int MAX_SET_FRAME = 120;
constexpr int MAX_SET_WIDTH = 1920;
constexpr int MIN_SET_WIDTH = 192;
constexpr int MAX_SET_HEIGHT = 1080;
constexpr int MIN_SET_HEIGHT = 144;
constexpr int MAX_SET_TIMESTAMP = 10000000;
constexpr int32_t kFuzzAudioSampleRate = 48000;
constexpr int32_t kFuzzAudioChannels = 2;
constexpr int32_t kFuzzInnerAudioSampleRate = 48000;
constexpr int32_t kFuzzInnerAudioBitrate = 48000;
constexpr int32_t kFuzzVideoBitrate = 2000000;
constexpr int32_t kFuzzVideoFrameRate = 30;
constexpr int32_t kFuzzMaxOriginalStreamNums = 64;
static int64_t kFuzzMaxOriginalStream = 0;

class AudioCaptureInfoAdapterMock : public AudioCaptureInfoAdapter {
public:
    AudioCaptureInfoAdapterMock() = default;

    ~AudioCaptureInfoAdapterMock() override = default;

    int32_t GetAudioSampleRate() override
    {
        return sample_rate_;
    }

    int32_t GetAudioChannels() override
    {
        return channels_;
    }

    AudioCaptureSourceTypeAdapter GetAudioSource() override
    {
        return source_;
    }

    void SetAudioSampleRate(int32_t rate)
    {
        sample_rate_ = rate;
    }

    void SetAudioChannels(int32_t channel)
    {
        channels_ = channel;
    }

    void SetAudioSource(AudioCaptureSourceTypeAdapter type)
    {
        source_ = type;
    }

    int32_t sample_rate_ = 0;
    int32_t channels_ = 0;
    AudioCaptureSourceTypeAdapter source_ =
      AudioCaptureSourceTypeAdapter::SOURCE_DEFAULT;
};

class AudioEncInfoAdapterMock : public AudioEncInfoAdapter {
public:
    AudioEncInfoAdapterMock() = default;

    ~AudioEncInfoAdapterMock() override = default;

    int32_t GetAudioBitrate() override
    {
        return bitrate_;
    }

    AudioCodecFormatAdapter GetAudioCodecformat() override
    {
        return codec_format_;
    }

    void SetAudioBitrate(int32_t bitrate)
    {
        bitrate_ = bitrate;
    }

    void SetAudioCodecformat(AudioCodecFormatAdapter format)
    {
        codec_format_ = format;
    }

    int32_t bitrate_ = 0;
    AudioCodecFormatAdapter codec_format_ =
        AudioCodecFormatAdapter::AUDIO_DEFAULT;
};

class AudioInfoAdapterMock : public AudioInfoAdapter {
public:
    AudioInfoAdapterMock() = default;

    ~AudioInfoAdapterMock() override = default;

    std::shared_ptr<AudioCaptureInfoAdapter> GetMicCapInfo() override
    {
        return mic_cap_info_;
    }

    std::shared_ptr<AudioCaptureInfoAdapter> GetInnerCapInfo() override
    {
        return inner_cap_info_;
    }

    std::shared_ptr<AudioEncInfoAdapter> GetAudioEncInfo() override
    {
        return enc_info_;
    }

    void SetMicCapInfo(std::shared_ptr<AudioCaptureInfoAdapter> info)
    {
        mic_cap_info_ = info;
    }

    void SetInnerCapInfo(std::shared_ptr<AudioCaptureInfoAdapter> info)
    {
        inner_cap_info_ = info;
    }

    void SetAudioEncInfo(std::shared_ptr<AudioEncInfoAdapter> info)
    {
        enc_info_ = info;
    }

    std::shared_ptr<AudioCaptureInfoAdapter> mic_cap_info_ = nullptr;
    std::shared_ptr<AudioCaptureInfoAdapter> inner_cap_info_ = nullptr;
    std::shared_ptr<AudioEncInfoAdapter> enc_info_ = nullptr;
};

class VideoCaptureInfoAdapterMock : public VideoCaptureInfoAdapter {
public:
    VideoCaptureInfoAdapterMock() = default;

    ~VideoCaptureInfoAdapterMock() override = default;

    uint64_t GetDisplayId() override
    {
        return display_id_;
    }

    std::list<int32_t> GetTaskIDs() override
    {
        return task_ids_;
    }

    int32_t GetVideoFrameWidth() override
    {
        return video_frame_width_;
    }

    int32_t GetVideoFrameHeight() override
    {
        return video_frame_height_;
    }

    VideoSourceTypeAdapter GetVideoSourceType() override
    {
        return video_source_;
    }

    void SetDisplayId(uint64_t id)
    {
        display_id_ = id;
    }

    void SetTaskIDs(std::list<int32_t> ids)
    {
        task_ids_ = ids;
    }

    void SetVideoFrameWidth(int32_t width)
    {
        video_frame_width_ = width;
    }

    void SetVideoFrameHeight(int32_t height)
    {
        video_frame_height_ = height;
    }

    void SetVideoSourceType(VideoSourceTypeAdapter type)
    {
        video_source_ = type;
    }

    uint64_t display_id_ = 0;
    std::list<int32_t> task_ids_;
    int32_t video_frame_width_ = 0;
    int32_t video_frame_height_ = 0;
    VideoSourceTypeAdapter video_source_ =
        VideoSourceTypeAdapter::VIDEO_SOURCE_SURFACE_RGBA;
};

class VideoEncInfoAdapterMock : public VideoEncInfoAdapter {
public:
    VideoEncInfoAdapterMock() = default;

    ~VideoEncInfoAdapterMock() override = default;

    VideoCodecFormatAdapter GetVideoCodecFormat() override
    {
        return codec_;
    }

    int32_t GetVideoBitrate() override
    {
        return bitrate_;
    }

    int32_t GetVideoFrameRate() override
    {
        return frame_rate_;
    }

    void SetVideoCodecFormat(VideoCodecFormatAdapter format)
    {
        codec_ = format;
    }

    void SetVideoBitrate(int32_t bitrate)
    {
        bitrate_ = bitrate;
    }

    void SetVideoFrameRate(int32_t frameRate)
    {
        frame_rate_ = frameRate;
    }

    VideoCodecFormatAdapter codec_ = VideoCodecFormatAdapter::VIDEO_DEFAULT;
    int32_t bitrate_ = 0;
    int32_t frame_rate_ = 0;
};

class VideoInfoAdapterMock : public VideoInfoAdapter {
public:
    VideoInfoAdapterMock() = default;

    ~VideoInfoAdapterMock() override = default;

    std::shared_ptr<VideoCaptureInfoAdapter> GetVideoCapInfo() override
    {
        return cap_info_;
    }

    std::shared_ptr<VideoEncInfoAdapter> GetVideoEncInfo() override
    {
        return enc_info_;
    }

    void SetVideoCapInfo(std::shared_ptr<VideoCaptureInfoAdapter> info)
    {
        cap_info_ = info;
    }

    void SetVideoEncInfo(std::shared_ptr<VideoEncInfoAdapter> info)
    {
        enc_info_ = info;
    }

    std::shared_ptr<VideoCaptureInfoAdapter> cap_info_ = nullptr;
    std::shared_ptr<VideoEncInfoAdapter> enc_info_ = nullptr;
};

class RecorderInfoAdapterMock : public RecorderInfoAdapter {
public:
    RecorderInfoAdapterMock() = default;

    ~RecorderInfoAdapterMock() override = default;

    std::string GetUrl() override
    {
        return url_;
    }

    ContainerFormatTypeAdapter GetFileFormat() override
    {
        return format_;
    }

    void SetUrl(const std::string& url)
    {
        url_ = url;
    }

    void SetFileFormat(ContainerFormatTypeAdapter format)
    {
        format_ = format;
    }

    std::string url_;
    ContainerFormatTypeAdapter format_ = ContainerFormatTypeAdapter::CFT_MPEG_4A_TYPE;
};

class ScreenCaptureConfigAdapterMock : public ScreenCaptureConfigAdapter {
public:
    ScreenCaptureConfigAdapterMock() = default;
    
    ~ScreenCaptureConfigAdapterMock() override = default;

    CaptureModeAdapter GetCaptureMode() override
    {
        return capture_mode_;
    }

    DataTypeAdapter GetDataType() override
    {
        return data_type_;
    }

    std::shared_ptr<AudioInfoAdapter> GetAudioInfo() override
    {
        return audio_info_;
    }

    std::shared_ptr<VideoInfoAdapter> GetVideoInfo() override
    {
        return video_info_;
    }

    std::shared_ptr<RecorderInfoAdapter> GetRecorderInfo() override
    {
        return recorder_info_;
    }

    void SetCaptureMode(CaptureModeAdapter mode)
    {
        capture_mode_ = mode;
    }

    void SetDataType(DataTypeAdapter type)
    {
        data_type_ = type;
    }

    void SetAudioInfo(std::shared_ptr<AudioInfoAdapter> info)
    {
        audio_info_ = info;
    }

    void SetVideoInfo(std::shared_ptr<VideoInfoAdapter> info)
    {
        video_info_ = info;
    }

    void SetRecorderInfo(std::shared_ptr<RecorderInfoAdapter> info)
    {
        recorder_info_ = info;
    }

    CaptureModeAdapter capture_mode_ = CaptureModeAdapter::CAPTURE_INVAILD;
    DataTypeAdapter data_type_ = DataTypeAdapter::INVAILD_DATA_TYPE;
    std::shared_ptr<AudioInfoAdapter> audio_info_ = nullptr;
    std::shared_ptr<VideoInfoAdapter> video_info_ = nullptr;
    std::shared_ptr<RecorderInfoAdapter> recorder_info_ = nullptr;
};

class AudioBufferAdapterMock : public AudioBufferAdapter {
public:
    AudioBufferAdapterMock() = default;

    ~AudioBufferAdapterMock() override = default;
 
    uint8_t* GetBuffer() override
    {
        return buffer_;
    }
 
    int32_t GetLength() override
    {
        return length_;
    }
 
    int64_t GetTimestamp() override
    {
        return timestamp_;
    }
 
    AudioCaptureSourceTypeAdapter GetSourcetype() override
    {
        return sourcetype_;
    }
 
    void SetBuffer(uint8_t* buffer) override
    {
        buffer_ = buffer;
    }
 
    void SetLength(int32_t length) override
    {
        length_ = length;
    }
 
    void SetTimestamp(int64_t timestamp) override
    {
        timestamp_ = timestamp;
    }
 
    void SetSourcetype(AudioCaptureSourceTypeAdapter sourcetype) override
    {
        sourcetype_ = sourcetype;
    }
 
    uint8_t* buffer_ = nullptr;
    int32_t length_ = 0;
    int64_t timestamp_ = 0;
    AudioCaptureSourceTypeAdapter sourcetype_ = AudioCaptureSourceTypeAdapter::SOURCE_INVALID;
};

class OHOSScreenCaptureCallbackMock : public ScreenCaptureCallbackAdapter {
public:
    OHOSScreenCaptureCallbackMock() = default;

    ~OHOSScreenCaptureCallbackMock() override = default;

    void OnError(int32_t errorCode) override {}

    void OnAudioBufferAvailable(bool isReady, AudioCaptureSourceTypeAdapter type) override {}

    void OnVideoBufferAvailable(bool isReady) override {}

    void OnStateChange(ScreenCaptureStateCodeAdapter stateCode) override {}

    void OnAudioBufferAvailableV2(bool isReady, AudioCaptureSourceTypeAdapter type, int32_t nweb_id) override {}

    void OnVideoBufferAvailableV2(bool isReady, int32_t nweb_id) override {}

    void OnStateChangeV2(ScreenCaptureStateCodeAdapter stateCode, int32_t nweb_id) override {}
};

std::unique_ptr<OHOS::NWeb::ScreenCaptureAdapter> CreateScreenCaptureAdapterMock()
{
    return OHOS::NWeb::OhosAdapterHelper::GetInstance().CreateScreenCaptureAdapter();
}

bool ScreenCaptureAdapterImplFuzzTest(FuzzedDataProvider* fdp)
{
    auto screenCaptureAdapter = CreateScreenCaptureAdapterMock();
    if (!screenCaptureAdapter) {
        return false;
    }

    //setting the microphone information
    std::shared_ptr<AudioCaptureInfoAdapterMock> micCapInfo =
        std::make_shared<AudioCaptureInfoAdapterMock>();
    int32_t audioSampleRate = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_SET_RATE);
    micCapInfo->SetAudioSampleRate(audioSampleRate);
    int32_t audioChannels = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_SET_CHANNELS);
    micCapInfo->SetAudioChannels(audioChannels);
    int32_t rawValue = fdp->ConsumeIntegralInRange<int32_t>(-1, 3);
    AudioCaptureSourceTypeAdapter audioSource = static_cast<AudioCaptureSourceTypeAdapter>(rawValue);
    micCapInfo->SetAudioSource(audioSource);

    //setting the system audio information
    std::shared_ptr<AudioCaptureInfoAdapterMock> innerCapInfo =
        std::make_shared<AudioCaptureInfoAdapterMock>();
    int32_t innerAudioSampleRate = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_SET_RATE);
    innerCapInfo->SetAudioSampleRate(innerAudioSampleRate);
    int32_t innerAudioChannels = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_SET_CHANNELS);
    innerCapInfo->SetAudioChannels(innerAudioChannels);
    int32_t innerRawValue = fdp->ConsumeIntegralInRange<int32_t>(-1, 3);
    AudioCaptureSourceTypeAdapter innerAudioSource = static_cast<AudioCaptureSourceTypeAdapter>(innerRawValue);
    innerCapInfo->SetAudioSource(innerAudioSource);

    //setting Audio Encoding Information
    std::shared_ptr<AudioEncInfoAdapterMock> audioEncInfo =
        std::make_shared<AudioEncInfoAdapterMock>();
    int32_t innerAudioBitrate = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_SET_RATE);
    audioEncInfo->SetAudioBitrate(innerAudioBitrate);
    int32_t formatValue = fdp->ConsumeIntegralInRange<int32_t>(0, 4);
    AudioCodecFormatAdapter codecFormat = static_cast<AudioCodecFormatAdapter>(formatValue);
    audioEncInfo->SetAudioCodecformat(codecFormat);

    std::shared_ptr<AudioInfoAdapterMock> audioInfo = std::make_shared<AudioInfoAdapterMock>();
    audioInfo->SetMicCapInfo(micCapInfo);
    audioInfo->SetInnerCapInfo(innerCapInfo);
    audioInfo->SetAudioEncInfo(audioEncInfo);

    //setting video information
    std::shared_ptr<VideoCaptureInfoAdapterMock> videoCapInfo =
        std::make_shared<VideoCaptureInfoAdapterMock>();
    int32_t videoFrameWidth = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_SET_WIDTH);
    videoCapInfo->SetVideoFrameWidth(videoFrameWidth);
    int32_t videoFrameHeight = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_SET_HEIGHT);
    videoCapInfo->SetVideoFrameHeight(videoFrameHeight);
    int32_t typeValue = fdp->ConsumeIntegralInRange<int32_t>(0, 3);
    VideoSourceTypeAdapter videoSourceType = static_cast<VideoSourceTypeAdapter>(typeValue);
    videoCapInfo->SetVideoSourceType(videoSourceType);

    //setting Video Encoding Information
    std::shared_ptr<VideoEncInfoAdapterMock> videoEncInfo =
        std::make_shared<VideoEncInfoAdapterMock>();
    int32_t kVideoBitrate = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_SET_BITRATE);
    videoEncInfo->SetVideoBitrate(kVideoBitrate);
    int32_t kVideoFrameRate = fdp->ConsumeIntegralInRange<int32_t>(0, MAX_SET_FRAME);
    videoEncInfo->SetVideoFrameRate(kVideoFrameRate);
    int32_t videoCodecFormatValue = fdp->ConsumeIntegralInRange<int32_t>(0, 11);
    OHOS::NWeb::VideoCodecFormatAdapter videoCodecFormat =
        static_cast<OHOS::NWeb::VideoCodecFormatAdapter>(videoCodecFormatValue);
    videoEncInfo->SetVideoCodecFormat(videoCodecFormat);

    std::shared_ptr<VideoInfoAdapterMock> videoInfo =
        std::make_shared<VideoInfoAdapterMock>();
    videoInfo->SetVideoCapInfo(videoCapInfo);
    videoInfo->SetVideoEncInfo(videoEncInfo);

    std::shared_ptr<ScreenCaptureConfigAdapterMock> config =
        std::make_shared<ScreenCaptureConfigAdapterMock>();
    int32_t captureModeValue = fdp->ConsumeIntegralInRange<int32_t>(-1, 2);
    CaptureModeAdapter captureMode = static_cast<CaptureModeAdapter>(captureModeValue);
    config->SetCaptureMode(captureMode);
    int32_t dataTypeValue = fdp->ConsumeIntegralInRange<int32_t>(-1, 2);
    DataTypeAdapter dataType = static_cast<DataTypeAdapter>(dataTypeValue);
    config->SetDataType(dataType);
    config->SetAudioInfo(audioInfo);
    config->SetVideoInfo(videoInfo);

    bool useRecorderInfo = fdp->ConsumeBool();
    if (useRecorderInfo && dataType == DataTypeAdapter::CAPTURE_FILE_DATA_TYPE) {
        auto recorderInfo = std::make_shared<RecorderInfoAdapterMock>();
        recorderInfo->SetUrl("test_recorder.mp4");
        int32_t fileFormatValue = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
        ContainerFormatTypeAdapter fileFormat = static_cast<ContainerFormatTypeAdapter>(fileFormatValue);
        recorderInfo->SetFileFormat(fileFormat);
        config->SetRecorderInfo(recorderInfo);
    }

    int32_t nweb_id = fdp->ConsumeIntegralInRange<int32_t>(0, 100);
    screenCaptureAdapter->InitV2(config, nweb_id);
    // Test re-init (screenCapture_ already exists path)
    int32_t nweb_id2 = fdp->ConsumeIntegralInRange<int32_t>(0, 100);
    screenCaptureAdapter->InitV2(config, nweb_id2);

    // Test Init (base class method)
    screenCaptureAdapter->Init(config);

    bool enable = fdp->ConsumeBool();
    screenCaptureAdapter->SetMicrophoneEnable(enable);
    auto callback = std::make_shared<OHOSScreenCaptureCallbackMock>();
    screenCaptureAdapter->SetCaptureCallback(callback);
    screenCaptureAdapter->StartCapture();
    screenCaptureAdapter->AcquireVideoBuffer();
    screenCaptureAdapter->ReleaseVideoBuffer();

    int32_t audioSourceTypeValue = fdp->ConsumeIntegralInRange<int32_t>(-1, 3);
    AudioCaptureSourceTypeAdapter type =
        static_cast<AudioCaptureSourceTypeAdapter>(audioSourceTypeValue);
    std::shared_ptr<AudioBufferAdapterMock> audiobufferImpl =
        std::make_shared<AudioBufferAdapterMock>();
    screenCaptureAdapter->AcquireAudioBuffer(audiobufferImpl, type);
    screenCaptureAdapter->ReleaseAudioBuffer(type);
    screenCaptureAdapter->StopCapture();
    
    return true;
}

bool ScreenCaptureAdapterImplNullFuzzTest(FuzzedDataProvider* fdp)
{
    auto screenCaptureAdapter = CreateScreenCaptureAdapterMock();
    if (!screenCaptureAdapter) {
        return false;
    }

    auto displayMgr =
          OHOS::NWeb::OhosAdapterHelperExt::CreateDisplayMgrAdapter();
    if (!displayMgr) {
        return false;
    }
    auto display = displayMgr->GetDefaultDisplay();
    if (!display) {
        return false;
    }
    int32_t videoFrameWidth = display->GetWidth();
    int32_t videoFrameHeight = display->GetHeight();
    if (videoFrameWidth > MIN_SET_WIDTH) {
        videoFrameWidth = MIN_SET_WIDTH;
    }
    if (videoFrameHeight > MIN_SET_HEIGHT) {
        videoFrameHeight = MIN_SET_HEIGHT;
    }
    
    //setting the microphone information
    std::shared_ptr<AudioCaptureInfoAdapterMock> micCapInfo =
        std::make_shared<AudioCaptureInfoAdapterMock>();
    micCapInfo->SetAudioSampleRate(kFuzzAudioSampleRate);
    micCapInfo->SetAudioChannels(kFuzzAudioChannels);
    micCapInfo->SetAudioSource(
          OHOS::NWeb::AudioCaptureSourceTypeAdapter::SOURCE_DEFAULT);

    //setting the system audio information
    std::shared_ptr<AudioCaptureInfoAdapterMock> innerCapInfo =
        std::make_shared<AudioCaptureInfoAdapterMock>();
    innerCapInfo->SetAudioSampleRate(kFuzzInnerAudioSampleRate);
    innerCapInfo->SetAudioChannels(kFuzzAudioChannels);
    innerCapInfo->SetAudioSource(
          OHOS::NWeb::AudioCaptureSourceTypeAdapter::ALL_PLAYBACK);

    //setting Audio Encoding Information
    std::shared_ptr<AudioEncInfoAdapterMock> audioEncInfo =
        std::make_shared<AudioEncInfoAdapterMock>();
    audioEncInfo->SetAudioBitrate(kFuzzInnerAudioBitrate);
    audioEncInfo->SetAudioCodecformat(
          OHOS::NWeb::AudioCodecFormatAdapter::AUDIO_DEFAULT);

    std::shared_ptr<AudioInfoAdapterMock> audioInfo = std::make_shared<AudioInfoAdapterMock>();
    audioInfo->SetMicCapInfo(micCapInfo);
    audioInfo->SetInnerCapInfo(innerCapInfo);
    audioInfo->SetAudioEncInfo(audioEncInfo);

    //setting video information
    std::shared_ptr<VideoCaptureInfoAdapterMock> videoCapInfo =
        std::make_shared<VideoCaptureInfoAdapterMock>();
    videoCapInfo->SetVideoFrameWidth(videoFrameWidth);
    videoCapInfo->SetVideoFrameHeight(videoFrameHeight);
    videoCapInfo->SetVideoSourceType(
          OHOS::NWeb::VideoSourceTypeAdapter::VIDEO_SOURCE_SURFACE_RGBA);

    //setting Video Encoding Information
    std::shared_ptr<VideoEncInfoAdapterMock> videoEncInfo =
        std::make_shared<VideoEncInfoAdapterMock>();
    videoEncInfo->SetVideoBitrate(kFuzzVideoBitrate);
    videoEncInfo->SetVideoFrameRate(kFuzzVideoFrameRate);
    videoEncInfo->SetVideoCodecFormat(
          OHOS::NWeb::VideoCodecFormatAdapter::H264);

    std::shared_ptr<VideoInfoAdapterMock> videoInfo =
        std::make_shared<VideoInfoAdapterMock>();
    videoInfo->SetVideoCapInfo(videoCapInfo);
    videoInfo->SetVideoEncInfo(videoEncInfo);

    std::shared_ptr<ScreenCaptureConfigAdapterMock> config =
        std::make_shared<ScreenCaptureConfigAdapterMock>();
    config->SetCaptureMode(OHOS::NWeb::CaptureModeAdapter::CAPTURE_HOME_SCREEN);
    if (kFuzzMaxOriginalStream <= kFuzzMaxOriginalStreamNums) {
        config->SetDataType(OHOS::NWeb::DataTypeAdapter::ORIGINAL_STREAM_DATA_TYPE);
        kFuzzMaxOriginalStream++;
    } else {
        config->SetDataType(OHOS::NWeb::DataTypeAdapter::CAPTURE_FILE_DATA_TYPE);
        auto recorderInfo = std::make_shared<RecorderInfoAdapterMock>();
        recorderInfo->SetUrl("test.mp4");
        recorderInfo->SetFileFormat(ContainerFormatTypeAdapter::CFT_MPEG_4_TYPE);
        config->SetRecorderInfo(recorderInfo);
    }
    config->SetAudioInfo(audioInfo);
    config->SetVideoInfo(videoInfo);

    int32_t nweb_id = fdp->ConsumeIntegralInRange<int32_t>(0, 100);
    screenCaptureAdapter->InitV2(config, nweb_id);
    screenCaptureAdapter->SetMicrophoneEnable(false);

    auto callback = std::make_shared<OHOSScreenCaptureCallbackMock>();
    screenCaptureAdapter->SetCaptureCallback(callback);
    screenCaptureAdapter->StartCapture();
    screenCaptureAdapter->AcquireVideoBuffer();
    screenCaptureAdapter->ReleaseVideoBuffer();

    std::shared_ptr<AudioBufferAdapterMock> audiobufferImpl =
        std::make_shared<AudioBufferAdapterMock>();
    screenCaptureAdapter->AcquireAudioBuffer(
        audiobufferImpl, OHOS::NWeb::AudioCaptureSourceTypeAdapter::ALL_PLAYBACK);
    screenCaptureAdapter->ReleaseAudioBuffer(OHOS::NWeb::AudioCaptureSourceTypeAdapter::ALL_PLAYBACK);
    screenCaptureAdapter->StopCapture();
    
    return true;
}

bool ScreenCaptureAdapterImplEdgeCaseFuzzTest(FuzzedDataProvider* fdp)
{
    auto screenCaptureAdapter = CreateScreenCaptureAdapterMock();
    if (!screenCaptureAdapter) {
        return false;
    }

    // Test functions without Init (screenCapture_ is null)
    screenCaptureAdapter->SetMicrophoneEnable(true);
    screenCaptureAdapter->StartCapture();
    screenCaptureAdapter->StopCapture();
    screenCaptureAdapter->SetCaptureCallback(nullptr);
    screenCaptureAdapter->AcquireVideoBuffer();
    auto audiobufferImpl = std::make_shared<AudioBufferAdapterMock>();
    screenCaptureAdapter->AcquireAudioBuffer(audiobufferImpl, AudioCaptureSourceTypeAdapter::SOURCE_DEFAULT);
    screenCaptureAdapter->ReleaseVideoBuffer();
    screenCaptureAdapter->ReleaseAudioBuffer(AudioCaptureSourceTypeAdapter::SOURCE_DEFAULT);

    // Test InitV2 with nullptr config
    screenCaptureAdapter->InitV2(nullptr, 0);

    // Test config with null audio_info and video_info
    auto config = std::make_shared<ScreenCaptureConfigAdapterMock>();
    config->SetCaptureMode(CaptureModeAdapter::CAPTURE_HOME_SCREEN);
    config->SetDataType(DataTypeAdapter::ORIGINAL_STREAM_DATA_TYPE);
    config->SetAudioInfo(nullptr);
    config->SetVideoInfo(nullptr);
    config->SetRecorderInfo(nullptr);
    screenCaptureAdapter->InitV2(config, 0);

    // Test config with null sub-info objects
    screenCaptureAdapter = CreateScreenCaptureAdapterMock();
    config = std::make_shared<ScreenCaptureConfigAdapterMock>();
    config->SetCaptureMode(CaptureModeAdapter::CAPTURE_HOME_SCREEN);
    config->SetDataType(DataTypeAdapter::ORIGINAL_STREAM_DATA_TYPE);
    auto audioInfo = std::make_shared<AudioInfoAdapterMock>();
    auto videoInfo = std::make_shared<VideoInfoAdapterMock>();
    config->SetAudioInfo(audioInfo);
    config->SetVideoInfo(videoInfo);
    screenCaptureAdapter->InitV2(config, 0);

    // Test SetCaptureCallback before InitV2 (callback_index_ is 0)
    auto adapter2 = CreateScreenCaptureAdapterMock();
    auto callback = std::make_shared<OHOSScreenCaptureCallbackMock>();
    adapter2->SetCaptureCallback(callback);

    return true;
}

bool ScreenCaptureCallbackFuzzTest(FuzzedDataProvider* fdp)
{
    auto screenCaptureAdapter = CreateScreenCaptureAdapterMock();
    if (!screenCaptureAdapter) {
        return false;
    }

    auto config = std::make_shared<ScreenCaptureConfigAdapterMock>();
    config->SetCaptureMode(CaptureModeAdapter::CAPTURE_HOME_SCREEN);
    config->SetDataType(DataTypeAdapter::ORIGINAL_STREAM_DATA_TYPE);
    
    auto audioInfo = std::make_shared<AudioInfoAdapterMock>();
    auto micCapInfo = std::make_shared<AudioCaptureInfoAdapterMock>();
    micCapInfo->SetAudioSampleRate(kFuzzAudioSampleRate);
    micCapInfo->SetAudioChannels(kFuzzAudioChannels);
    micCapInfo->SetAudioSource(AudioCaptureSourceTypeAdapter::SOURCE_DEFAULT);
    audioInfo->SetMicCapInfo(micCapInfo);
    audioInfo->SetInnerCapInfo(nullptr);
    audioInfo->SetAudioEncInfo(nullptr);
    
    auto videoInfo = std::make_shared<VideoInfoAdapterMock>();
    auto videoCapInfo = std::make_shared<VideoCaptureInfoAdapterMock>();
    videoCapInfo->SetVideoFrameWidth(MIN_SET_WIDTH);
    videoCapInfo->SetVideoFrameHeight(MIN_SET_HEIGHT);
    videoCapInfo->SetVideoSourceType(VideoSourceTypeAdapter::VIDEO_SOURCE_SURFACE_RGBA);
    videoInfo->SetVideoCapInfo(videoCapInfo);
    videoInfo->SetVideoEncInfo(nullptr);
    
    config->SetAudioInfo(audioInfo);
    config->SetVideoInfo(videoInfo);

    int32_t nweb_id = fdp->ConsumeIntegralInRange<int32_t>(0, 100);
    screenCaptureAdapter->InitV2(config, nweb_id);
    
    auto callback = std::make_shared<OHOSScreenCaptureCallbackMock>();
    screenCaptureAdapter->SetCaptureCallback(callback);

    size_t callback_index = 1;

    // Test ScreenCaptureCallbackOnError - valid path
    ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnError(
        nullptr, -1, reinterpret_cast<void*>(callback_index));

    // Test ScreenCaptureCallbackOnError - null userData
    ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnError(
        nullptr, -1, nullptr);

    // Test ScreenCaptureCallbackOnError - invalid callback index
    ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnError(
        nullptr, -1, reinterpret_cast<void*>(99999));

    // Test ScreenCaptureCallbackOnBufferAvailable with video buffer
    int32_t capacity = fdp->ConsumeIntegralInRange<int32_t>(1, 8);
    OH_AVBuffer* buffer = OH_AVBuffer_Create(capacity);
    if (buffer) {
        ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnBufferAvailable(
            nullptr, buffer, OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO,
            12345, reinterpret_cast<void*>(callback_index));
        
        // Audio inner buffer
        ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnBufferAvailable(
            nullptr, buffer, OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_INNER,
            12345, reinterpret_cast<void*>(callback_index));
        
        // Audio mic buffer (should be ignored)
        ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnBufferAvailable(
            nullptr, buffer, OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_MIC,
            12345, reinterpret_cast<void*>(callback_index));
        
        // Unknown buffer type
        ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnBufferAvailable(
            nullptr, buffer, static_cast<OH_AVScreenCaptureBufferType>(999),
            12345, reinterpret_cast<void*>(callback_index));
        
        // Null buffer
        ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnBufferAvailable(
            nullptr, nullptr, OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO,
            0, reinterpret_cast<void*>(callback_index));
        
        // Null userData
        ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnBufferAvailable(
            nullptr, buffer, OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO,
            0, nullptr);

        OH_AVBuffer_Destroy(buffer);
    }

    // Test ScreenCaptureCallbackOnStateChange with all valid state codes
    for (int32_t i = 0; i <= 9; ++i) {
        OH_AVScreenCaptureStateCode stateCode = static_cast<OH_AVScreenCaptureStateCode>(i);
        ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnStateChange(
            nullptr, stateCode, reinterpret_cast<void*>(callback_index));
    }
    // Unknown state code
    ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnStateChange(
        nullptr, static_cast<OH_AVScreenCaptureStateCode>(100),
        reinterpret_cast<void*>(callback_index));
    // Null userData
    ScreenCaptureAdapterImpl::ScreenCaptureCallbackOnStateChange(
        nullptr, OH_SCREEN_CAPTURE_STATE_STARTED, nullptr);

    // AcquireVideoBuffer and AcquireAudioBuffer with queued data
    screenCaptureAdapter->AcquireVideoBuffer();
    auto audiobufferImpl = std::make_shared<AudioBufferAdapterMock>();
    screenCaptureAdapter->AcquireAudioBuffer(
        audiobufferImpl, AudioCaptureSourceTypeAdapter::ALL_PLAYBACK);

    // Test AcquireAudioBuffer with null audiobuffer parameter
    screenCaptureAdapter->AcquireAudioBuffer(
        nullptr, AudioCaptureSourceTypeAdapter::ALL_PLAYBACK);

    // Test queue not found with different nweb_id after re-init
    int32_t nweb_id2 = fdp->ConsumeIntegralInRange<int32_t>(101, 200);
    screenCaptureAdapter->InitV2(config, nweb_id2);
    screenCaptureAdapter->AcquireVideoBuffer();
    screenCaptureAdapter->AcquireAudioBuffer(
        audiobufferImpl, AudioCaptureSourceTypeAdapter::ALL_PLAYBACK);

    return true;
}

bool OH_SurfaceBufferAdapterImplFuzzTest(FuzzedDataProvider* fdp)
{
    int32_t capacity = fdp->ConsumeIntegralInRange<int32_t>(0, 8);
    OH_AVBuffer* buffer = OH_AVBuffer_Create(capacity);
    if (!buffer) {
        return true;
    }
    OH_NativeBuffer* nativeBuffer = OH_AVBuffer_GetNativeBuffer(buffer);
    if (!nativeBuffer) {
        OH_AVBuffer_Destroy(buffer);
        return true;
    }
    OH_NativeBuffer_Config config;
    OH_NativeBuffer_GetConfig(nativeBuffer, &config);
    auto surfaceBufferImpl = std::make_shared<OH_SurfaceBufferAdapterImpl>(buffer, config);
    surfaceBufferImpl->GetFileDescriptor();
    surfaceBufferImpl->GetWidth();
    surfaceBufferImpl->GetHeight();
    surfaceBufferImpl->GetStride();
    surfaceBufferImpl->GetFormat();
    surfaceBufferImpl->GetSize();
    surfaceBufferImpl->GetVirAddr();
    OH_AVBuffer_Destroy(buffer);
    OH_NativeBuffer_Unreference(nativeBuffer);

    return true;
}

bool OH_AudioBufferAdapterImplFuzzTest(FuzzedDataProvider* fdp)
{
    int32_t capacity = fdp->ConsumeIntegralInRange<int32_t>(1, 8);
    OH_AVBuffer* buffer = OH_AVBuffer_Create(capacity);
    int32_t timestamp = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_SET_TIMESTAMP);
    int32_t sourceTypeValue = fdp->ConsumeIntegralInRange<int32_t>(-1, 3);
    OH_AudioCaptureSourceType audioSource =
        static_cast<OH_AudioCaptureSourceType>(sourceTypeValue);
    auto audioBufferImpl = std::make_shared<OH_AudioBufferAdapterImpl>(
        buffer, timestamp, audioSource);
    audioBufferImpl->SetBuffer(audioBufferImpl->avBuffer_);
    int32_t audioLength = fdp->ConsumeIntegralInRange<int32_t>(1, 1000);
    audioBufferImpl->SetLength(audioLength);
    int32_t audioTimestamp = fdp->ConsumeIntegralInRange<int32_t>(1, MAX_SET_TIMESTAMP);
    audioBufferImpl->SetTimestamp(audioTimestamp);
    int32_t audioSourceTypeValue = fdp->ConsumeIntegralInRange<int32_t>(-1, 3);
    AudioCaptureSourceTypeAdapter audioCaptureSource =
        static_cast<AudioCaptureSourceTypeAdapter>(audioSourceTypeValue);
    audioBufferImpl->SetSourcetype(audioCaptureSource);
    audioBufferImpl->GetBuffer();
    audioBufferImpl->GetLength();
    audioBufferImpl->GetTimestamp();
    audioBufferImpl->GetSourcetype();
    if (buffer) {
        OH_AVBuffer_Destroy(buffer);
    }

    return true;
}

bool OH_AudioBufferAdapterImplErrorFuzzTest(FuzzedDataProvider* fdp)
{
    (void)fdp;
    // Test with nullptr avBuffer
    auto audioBufferImpl = std::make_shared<OH_AudioBufferAdapterImpl>(
        nullptr, 0, OH_AudioCaptureSourceType::OH_SOURCE_DEFAULT);
    audioBufferImpl->GetBuffer();
    audioBufferImpl->GetLength();
    audioBufferImpl->GetTimestamp();
    audioBufferImpl->GetSourcetype();

    // Test with invalid source type to cover ConvertAudioCaptureSourceType default
    OH_AVBuffer* buffer = OH_AVBuffer_Create(4);
    if (buffer) {
        auto audioBufferImpl2 = std::make_shared<OH_AudioBufferAdapterImpl>(
            buffer, 0, static_cast<OH_AudioCaptureSourceType>(999));
        audioBufferImpl2->GetSourcetype();
        OH_AVBuffer_Destroy(buffer);
    }

    // Test with capacity 0 (length_ <= 0 branch)
    OH_AVBuffer* zeroBuffer = OH_AVBuffer_Create(0);
    if (zeroBuffer) {
        auto audioBufferImpl3 = std::make_shared<OH_AudioBufferAdapterImpl>(
            zeroBuffer, 0, OH_AudioCaptureSourceType::OH_ALL_PLAYBACK);
        audioBufferImpl3->GetBuffer();
        OH_AVBuffer_Destroy(zeroBuffer);
    }

    return true;
}

}  // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    /* Run your code on data */
    FuzzedDataProvider fdp(data, size);
    OHOS::ScreenCaptureAdapterImplFuzzTest(&fdp);
    OHOS::ScreenCaptureAdapterImplNullFuzzTest(&fdp);
    OHOS::ScreenCaptureAdapterImplEdgeCaseFuzzTest(&fdp);
    OHOS::ScreenCaptureCallbackFuzzTest(&fdp);
    OHOS::OH_SurfaceBufferAdapterImplFuzzTest(&fdp);
    OHOS::OH_AudioBufferAdapterImplFuzzTest(&fdp);
    OHOS::OH_AudioBufferAdapterImplErrorFuzzTest(&fdp);
 
    return 0;
}