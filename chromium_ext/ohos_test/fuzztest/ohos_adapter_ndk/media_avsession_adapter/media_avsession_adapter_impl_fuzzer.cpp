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

#include <fuzzer/FuzzedDataProvider.h>

#include <climits>
#include <cstdlib>
#include <ctime>
#include "arkweb/ohos_nweb/src/nweb_hilog.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ohos_adapter_helper.h"
#define private public
#include "arkweb/ohos_adapter_ndk/media_avsession_adapter/media_avsession_adapter_impl.h"
#undef private
using namespace OHOS::NWeb;

namespace OHOS {

constexpr int32_t kFuzzMaxOriginalStreamNums = 2048;
static int64_t kFuzzMaxOriginalStream = 0;
static int64_t kFuzzMaxOriginalStreamNull = 0;

class MediaAVSessionCallbackAdapterMock : public MediaAVSessionCallbackAdapter {
public:
    MediaAVSessionCallbackAdapterMock() = default;
    void Play() override {}
    void Pause() override {}
    void Stop() override {}
    void SeekTo(int64_t millisTime) override {}
    int32_t GetMediaCastCurrentTime() override { return 100; }
    void PullUpCastBackGround(const std::string& device_name) override {}
    void SetAvCast(bool is_avcast) override {}
    void UpdateUiPlayState(bool is_playing) override {}
    void UpdateUiPlayPosition(int64_t position) override {}
    void MediaCastStopped() override {}
    void NotifyCastControlShow(bool is_show) override {}
};

class MediaAVSessionKeyMock : public MediaAVSessionKey {
public:
    std::string bundleName = "test1";
    std::string abilityName = "testAbility";

    std::string &GetBundleName() {
        return bundleName;
    }

    std::string &GetAbilityName() {
        return abilityName;
    }
};

class MediaAVSessionMetadataAdapterMock : public MediaAVSessionMetadataAdapter {
public:
    MediaAVSessionMetadataAdapterMock() = default;
    void SetTitle(const std::string& title) override {}

    std::string GetTitle() override { return "test1"; }

    void SetArtist(const std::string& artist) override {}

    std::string GetArtist() override { return "test2"; }

    void SetAlbum(const std::string& album) override {}

    std::string GetAlbum() override { return "test3"; }

    void SetImageUrl(const std::string& imageUrl) override {}

    std::string GetImageUrl() override { return "test4"; }
};

class MediaAVSessionPositionAdapterMock : public MediaAVSessionPositionAdapter {
public:
    MediaAVSessionPositionAdapterMock() = default;

    void SetDuration(int64_t duration) override {}

    int64_t GetDuration() override { return 20; }

    void SetElapsedTime(int64_t elapsedTime) override {}

    int64_t GetElapsedTime() override { return 10; }

    void SetUpdateTime(int64_t updateTime) override {}

    int64_t GetUpdateTime() override { return 10; }
};

class MediaAVSessionPositionAdapterHugeMock : public MediaAVSessionPositionAdapter {
public:
    MediaAVSessionPositionAdapterHugeMock() = default;

    void SetDuration(int64_t duration) override {}

    int64_t GetDuration() override { return INT64_MAX; }

    void SetElapsedTime(int64_t elapsedTime) override {}

    int64_t GetElapsedTime() override { return INT64_MAX; }

    void SetUpdateTime(int64_t updateTime) override {}

    int64_t GetUpdateTime() override { return 0; }
};

bool MediaAVSessionAdapterImplFuzzTest(FuzzedDataProvider* fdp)
{
    std::shared_ptr<MediaAVSessionMetadataAdapterMock> metadataadapter =
        std::make_shared<MediaAVSessionMetadataAdapterMock>();
    std::shared_ptr<MediaAVSessionPositionAdapterMock> pointeradapter =
        std::make_shared<MediaAVSessionPositionAdapterMock>();
    std::shared_ptr<MediaAVSessionAdapterImpl> avSessionAdapter = std::make_shared<MediaAVSessionAdapterImpl>();
    std::shared_ptr<MediaAVSessionKey> key = std::make_shared<MediaAVSessionKey>();

    auto type = MediaAVSessionType::MEDIA_TYPE_AUDIO;
    if (kFuzzMaxOriginalStream <= kFuzzMaxOriginalStreamNums) {
        kFuzzMaxOriginalStream++;
        OH_AVSession_Create(SESSION_TYPE_AUDIO, "OH_AVSession_Create_001",
            "com.xxx.hmxx", "ndkxx", &(avSessionAdapter->avSession_));
    } else if (kFuzzMaxOriginalStreamNull <= kFuzzMaxOriginalStreamNums) {
        kFuzzMaxOriginalStreamNull++;
        avSessionAdapter->CreateAVSession(type);
        type = MediaAVSessionType::MEDIA_TYPE_VIDEO;
        avSessionAdapter->CreateAVSession(type);
    } else {
        return false;
    }

    key->Init();
    key->GetPID();
    key->GetType();
    key->ToString();
    key->SetType(MediaAVSessionType::MEDIA_TYPE_INVALID);

    auto mediaAVSessionCallbackAdapterMock = std::make_shared<MediaAVSessionCallbackAdapterMock>();
    avSessionAdapter->RegistCallback(mediaAVSessionCallbackAdapterMock);

    auto avSessionKey = std::make_shared<MediaAVSessionKeyMock>();
    avSessionAdapter->avSessionKey_ = avSessionKey;
    avSessionAdapter->avSessionKey_->Init();

    // Test CreateAVSession with invalid type
    avSessionAdapter->CreateAVSession(MediaAVSessionType::MEDIA_TYPE_INVALID);

    int32_t rawValue = fdp->ConsumeIntegralInRange<int32_t>(-1, 1);
    type = static_cast<MediaAVSessionType>(rawValue);
    avSessionAdapter->CreateAVSession(type);
    avSessionAdapter->IsActivated();
    avSessionAdapter->Activate();
    avSessionAdapter->SetMetadata(nullptr);
    avSessionAdapter->SetMetadata(metadataadapter);
    rawValue = fdp->ConsumeIntegralInRange<int64_t>(0, 3);
    auto playState = static_cast<MediaAVSessionPlayState>(rawValue);
    avSessionAdapter->SetPlaybackState(playState);

    avSessionAdapter->SetPlaybackPosition(pointeradapter);
    // Test SetPlaybackPosition with huge values (exceeds INT32_MAX)
    auto hugePosition = std::make_shared<MediaAVSessionPositionAdapterHugeMock>();
    avSessionAdapter->SetPlaybackPosition(hugePosition);

    avSessionAdapter->UpdateMetaData(metadataadapter);
    avSessionAdapter->UpdateDuration(pointeradapter);
    avSessionAdapter->UpdatePlaybackState(playState);
    avSessionAdapter->UpdatePlaybackPosition(pointeradapter);
    avSessionAdapter->UpdateAVMetadata();
    avSessionAdapter->DeActivate();
    avSessionAdapter->DestroyAVSession();
    avSessionAdapter->DestroyAndEraseSession();

    // Test Cast-related methods
    std::string testUri = "http://test.com/media.mp4";
    avSessionAdapter->SetMediaCastUri(testUri);

    bool enabled = fdp->ConsumeBool();
    avSessionAdapter->SetRemoteCastEnabled(enabled);

    // Test PrepareMediaCastDescription
    avSessionAdapter->PrepareMediaCastDescription();

    // Test GetMediaCastCurrentTime
    avSessionAdapter->GetMediaCastCurrentTime();

    // Test HandleStopMediaCast
    avSessionAdapter->HandleStopMediaCast();

    // Test UpdateRemotePlayState and UpdateRemotePlayPosition
    bool isPlaying = fdp->ConsumeBool();
    avSessionAdapter->UpdateRemotePlayState(isPlaying);

    int64_t position = fdp->ConsumeIntegral<int64_t>();
    avSessionAdapter->UpdateRemotePlayPosition(position);

    // Test GetAVCastController
    avSessionAdapter->GetAVCastController();

    // Test Prepare method
    MediaCastDescription mediaCastDescription;
    mediaCastDescription.assetId = "test_asset_id";
    mediaCastDescription.title = "Test Title";
    mediaCastDescription.mediaUri = "http://test.com/test.mp4";
    mediaCastDescription.mediaType = "VIDEO";
    mediaCastDescription.duration = 1000;
    mediaCastDescription.startPosition = 0;
    mediaCastDescription.albumUrl = "http://test.com/poster.jpg";
    avSessionAdapter->Prepare(mediaCastDescription);

    // Test PlayRemote, PauseRemote, SetPlaybackPositionRemote
    avSessionAdapter->PlayRemote();
    avSessionAdapter->PauseRemote();
    avSessionAdapter->SetPlaybackPositionRemote(static_cast<int32_t>(position));

    // Test UpdateRemotePlayStateCast and UpdateRemotePlayPositionCast
    avSessionAdapter->UpdateRemotePlayStateCast(isPlaying);
    avSessionAdapter->UpdateRemotePlayPositionCast(position);

    // Test GetAVCastPlaybackState
    avSessionAdapter->GetAVCastPlaybackState();

    // Test IsAvCastPlaying
    avSessionAdapter->IsAvCastPlaying();

    // Test GetPlaybackPosition
    avSessionAdapter->GetPlaybackPosition();

    // Test SetAVCastUiPlayState
    AVSession_PlaybackState avSessionPlaybackState = PLAYBACK_STATE_PLAYING;
    avSessionAdapter->SetAVCastUiPlayState(avSessionPlaybackState);

    // Test SetAVCastUiPlayPosition
    AVSession_PlaybackPosition playbackPosition;
    playbackPosition.elapsedTime = position;
    playbackPosition.updateTime = 1L;
    avSessionAdapter->SetAVCastUiPlayPosition(playbackPosition);

    // Test SetAVCastUilastUiTime
    avSessionAdapter->SetAVCastUilastUiTime(position);

    // Test SetAVCastUiSeeking
    bool isSeeking = fdp->ConsumeBool();
    avSessionAdapter->SetAVCastUiSeeking(isSeeking);

    // Test GetAVCastUiPlayState
    avSessionAdapter->GetAVCastUiPlayState();

    // Test GetAVCastUiPlayPosition
    avSessionAdapter->GetAVCastUiPlayPosition();

    // Test GetAVCastUilastUiTime
    avSessionAdapter->GetAVCastUilastUiTime();

    // Test GetAVCastUiSeeking
    avSessionAdapter->GetAVCastUiSeeking();

    // Test UpdateUiPlayPosition static method
    avSessionAdapter->UpdateUiPlayPosition(avSessionAdapter, position, isSeeking);

    // Test SetAVCastDevice and GetAVCastDevice
    avSessionAdapter->SetAVCastDevice("TestDevice");
    avSessionAdapter->GetAVCastDevice();

    // Test SetAvCast
    avSessionAdapter->SetAvCast(true);
    avSessionAdapter->SetAvCast(false);

    // Test UpdateUiPlayState and UpdateUiPlayPosition
    avSessionAdapter->UpdateUiPlayState(isPlaying);
    avSessionAdapter->UpdateUiPlayPosition(position);

    // Test UpdateUiPlayStateByClient, SetUiPlayStateByClient, SetUiPlayPositionByClient
    avSessionAdapter->UpdateUiPlayStateByClient(avSessionPlaybackState);
    avSessionAdapter->SetUiPlayStateByClient(avSessionPlaybackState);
    avSessionAdapter->SetUiPlayPositionByClient(playbackPosition);

    // Test SetUilastUiTimeByClient, SetUiSeekingByClient
    avSessionAdapter->SetUilastUiTimeByClient(position);
    avSessionAdapter->SetUiSeekingByClient(isSeeking);

    // Test GetUiPlayStateByClient, GetUilastUiTimeByClient, GetUiSeekingByClient
    avSessionAdapter->GetUiPlayStateByClient();
    avSessionAdapter->GetUilastUiTimeByClient();
    avSessionAdapter->GetUiSeekingByClient();

    // Test RegisterCallback and UnregisterCallback
    avSessionAdapter->RegisterCallback();
    avSessionAdapter->UnregisterCallback();

    // Test callback functions (static methods)
    OH_AVSession_AVPlaybackState* playbackStatePtr = nullptr;
    MediaAVSessionAdapterImpl::PlaybackStateChangedCallback(nullptr, playbackStatePtr, nullptr);

    OH_AVSession_AVQueueItem* avQueueItemPtr = nullptr;
    MediaAVSessionAdapterImpl::MediaItemChangeCallback(nullptr, avQueueItemPtr, nullptr);

    int32_t seekPosition = static_cast<int32_t>(position);
    MediaAVSessionAdapterImpl::SeekDoneCallback(nullptr, seekPosition, nullptr);

    MediaAVSessionAdapterImpl::EndOfStreamCallback(nullptr, nullptr);

    AVSession_ErrCode errorCode = static_cast<AVSession_ErrCode>(fdp->ConsumeIntegral<int32_t>());
    MediaAVSessionAdapterImpl::ErrorCallback(nullptr, nullptr, errorCode);

    // Test OutputDeviceChangeCallback with different states
    AVSession_OutputDeviceInfo outputDeviceInfo;
    outputDeviceInfo.deviceInfos = nullptr;
    if (outputDeviceInfo.deviceInfos != nullptr) {
        MediaAVSessionAdapterImpl::OutputDeviceChangeCallback(avSessionAdapter->avSession_,
            STATE_CONNECTED, &outputDeviceInfo);
        MediaAVSessionAdapterImpl::OutputDeviceChangeCallback(avSessionAdapter->avSession_,
            STATE_DISCONNECTED, &outputDeviceInfo);
        MediaAVSessionAdapterImpl::OutputDeviceChangeCallback(avSessionAdapter->avSession_,
            static_cast<AVSession_ConnectionState>(99), &outputDeviceInfo);
        free(outputDeviceInfo.deviceInfos);
    }

    // Test UnregisterMediaCastOutputDeviceCallback
    avSessionAdapter->UnregisterMediaCastOutputDeviceCallback();

    // Test SeekNative, PlayNative, PauseNative
    avSessionAdapter->SeekNative(position);
    avSessionAdapter->PlayNative();
    avSessionAdapter->PauseNative();

    // Test MediaCastStopped
    avSessionAdapter->MediaCastStopped();

    // Test PullUpCastBackGround
    avSessionAdapter->PullUpCastBackGround();

    // Test PrepareAndStartCast
    avSessionAdapter->PrepareAndStartCast();

    // Test StartCast
    avSessionAdapter->StartCast();

    return true;
}

bool MediaAVSessionAdapterImplEdgeCaseFuzzTest(FuzzedDataProvider* fdp)
{
    (void)fdp;
    // Test functions without avSession_ (default constructed state)
    auto avSessionAdapter = std::make_shared<MediaAVSessionAdapterImpl>();

    avSessionAdapter->IsActivated();
    avSessionAdapter->Activate();
    avSessionAdapter->DeActivate();
    avSessionAdapter->SetMetadata(nullptr);
    avSessionAdapter->RegistCallback(nullptr);
    avSessionAdapter->GetMediaCastCurrentTime();

    // Test methods that check callback via callback_wrapper_ with index 0
    avSessionAdapter->SetAvCast(true);
    avSessionAdapter->UpdateUiPlayState(true);
    avSessionAdapter->UpdateUiPlayPosition(100);
    avSessionAdapter->PullUpCastBackGround();
    avSessionAdapter->MediaCastStopped();
    avSessionAdapter->SeekNative(100);
    avSessionAdapter->PlayNative();
    avSessionAdapter->PauseNative();

    // Test private methods with nullptr parameters
    avSessionAdapter->UpdateMetaData(nullptr);
    avSessionAdapter->UpdateDuration(nullptr);
    avSessionAdapter->UpdatePlaybackPosition(nullptr);

    // Test DestroyAndEraseSession when no map entry exists
    avSessionAdapter->DestroyAndEraseSession();

    // Test PrepareAndStartCast without avSession_
    avSessionAdapter->PrepareAndStartCast();

    // Test GetAVCastController without avSession_
    avSessionAdapter->GetAVCastController();

    // Test UpdateAVCastDevice with nullptr
    avSessionAdapter->UpdateAVCastDevice(nullptr);

    // Test OutputDeviceChangeCallback with null device info
    MediaAVSessionAdapterImpl::OutputDeviceChangeCallback(nullptr,
        STATE_CONNECTED, nullptr);

    return true;
}

bool MediaAVSessionCommandCallbackFuzzTest(FuzzedDataProvider* fdp)
{
    (void)fdp;
    auto avSessionAdapter = std::make_shared<MediaAVSessionAdapterImpl>();
    auto callback = std::make_shared<MediaAVSessionCallbackAdapterMock>();

    // Set up a valid callback in the wrapper
    size_t callbackIndex = MediaAVSessionAdapterImpl::callback_wrapper_.AddCallback(callback);
    avSessionAdapter->callback_index_ = callbackIndex;

    // Test AVSessionOnCommandCallback with all valid commands
    MediaAVSessionAdapterImpl::AVSessionOnCommandCallback(
        nullptr, CONTROL_CMD_PLAY, reinterpret_cast<void*>(callbackIndex));
    MediaAVSessionAdapterImpl::AVSessionOnCommandCallback(
        nullptr, CONTROL_CMD_PAUSE, reinterpret_cast<void*>(callbackIndex));
    MediaAVSessionAdapterImpl::AVSessionOnCommandCallback(
        nullptr, CONTROL_CMD_STOP, reinterpret_cast<void*>(callbackIndex));

    // Test with invalid command (default branch)
    MediaAVSessionAdapterImpl::AVSessionOnCommandCallback(
        nullptr, static_cast<AVSession_ControlCommand>(999), reinterpret_cast<void*>(callbackIndex));

    // Test with null userData
    MediaAVSessionAdapterImpl::AVSessionOnCommandCallback(
        nullptr, CONTROL_CMD_PLAY, nullptr);

    // Test with invalid callback index
    MediaAVSessionAdapterImpl::AVSessionOnCommandCallback(
        nullptr, CONTROL_CMD_PLAY, reinterpret_cast<void*>(99999));

    // Test AVSessionOnSeekCallback
    MediaAVSessionAdapterImpl::AVSessionOnSeekCallback(
        nullptr, 12345, reinterpret_cast<void*>(callbackIndex));

    // Test with null userData
    MediaAVSessionAdapterImpl::AVSessionOnSeekCallback(
        nullptr, 12345, nullptr);

    // Test with invalid callback index
    MediaAVSessionAdapterImpl::AVSessionOnSeekCallback(
        nullptr, 12345, reinterpret_cast<void*>(99999));

    // Clean up
    MediaAVSessionAdapterImpl::callback_wrapper_.Clear(callbackIndex);
    avSessionAdapter->callback_index_ = 0;

    return true;
}

bool MediaAVSessionCastCallbackFuzzTest(FuzzedDataProvider* fdp)
{
    (void)fdp;
    auto avSessionAdapter = std::make_shared<MediaAVSessionAdapterImpl>();

    // Set up a valid adapter in the avsession callback wrapper
    size_t avsessionIndex = MediaAVSessionAdapterImpl::avsession_callback_wrapper_.AddCallback(avSessionAdapter);
    avSessionAdapter->avsession_callback_index_ = avsessionIndex;

    // Test PlaybackStateChangedCallback with valid adapter
    MediaAVSessionAdapterImpl::PlaybackStateChangedCallback(
        nullptr, nullptr, reinterpret_cast<void*>(avsessionIndex));

    // Test with null userData
    MediaAVSessionAdapterImpl::PlaybackStateChangedCallback(
        nullptr, nullptr, nullptr);

    // Test with invalid index
    MediaAVSessionAdapterImpl::PlaybackStateChangedCallback(
        nullptr, nullptr, reinterpret_cast<void*>(99999));

    // Test SeekDoneCallback with valid adapter
    MediaAVSessionAdapterImpl::SeekDoneCallback(
        nullptr, 100, reinterpret_cast<void*>(avsessionIndex));

    // Test with null userData
    MediaAVSessionAdapterImpl::SeekDoneCallback(
        nullptr, 100, nullptr);

    // Test with invalid index
    MediaAVSessionAdapterImpl::SeekDoneCallback(
        nullptr, 100, reinterpret_cast<void*>(99999));

    // Test ErrorCallback with valid adapter
    MediaAVSessionAdapterImpl::ErrorCallback(
        nullptr, reinterpret_cast<void*>(avsessionIndex), AV_SESSION_ERR_SUCCESS);
    MediaAVSessionAdapterImpl::ErrorCallback(
        nullptr, reinterpret_cast<void*>(avsessionIndex), static_cast<AVSession_ErrCode>(-1));

    // Test with null userData
    MediaAVSessionAdapterImpl::ErrorCallback(
        nullptr, nullptr, AV_SESSION_ERR_SUCCESS);

    // Test with invalid index
    MediaAVSessionAdapterImpl::ErrorCallback(
        nullptr, reinterpret_cast<void*>(99999), AV_SESSION_ERR_SUCCESS);

    // Test EndOfStreamCallback
    MediaAVSessionAdapterImpl::EndOfStreamCallback(nullptr, nullptr);

    // Test MediaItemChangeCallback
    MediaAVSessionAdapterImpl::MediaItemChangeCallback(nullptr, nullptr, nullptr);

    // Clean up
    MediaAVSessionAdapterImpl::avsession_callback_wrapper_.Clear(avsessionIndex);
    avSessionAdapter->avsession_callback_index_ = 0;

    return true;
}

bool MediaAVSessionUpdateUiFuzzTest(FuzzedDataProvider* fdp)
{
    (void)fdp;
    // Test UpdateUiPlayPosition static method branches

    // 1. adapter == nullptr
    MediaAVSessionAdapterImpl::UpdateUiPlayPosition(nullptr, 100, false);

    auto avSessionAdapter = std::make_shared<MediaAVSessionAdapterImpl>();
    auto callback = std::make_shared<MediaAVSessionCallbackAdapterMock>();
    size_t callbackIndex = MediaAVSessionAdapterImpl::callback_wrapper_.AddCallback(callback);
    avSessionAdapter->callback_index_ = callbackIndex;

    // 2. position == 0
    MediaAVSessionAdapterImpl::UpdateUiPlayPosition(avSessionAdapter, 0, false);

    // 3. is_seek == true
    MediaAVSessionAdapterImpl::UpdateUiPlayPosition(avSessionAdapter, 1000, true);

    // 4. is_seeking_ == true (GetUiSeekingByClient() returns true)
    avSessionAdapter->SetAVCastUiSeeking(true);
    MediaAVSessionAdapterImpl::UpdateUiPlayPosition(avSessionAdapter, 500, false);
    avSessionAdapter->SetAVCastUiSeeking(false);

    // 5. Time difference < UiTIME_UPDATE_INTERVAL (200ms), should skip update
    avSessionAdapter->SetAVCastUilastUiTime(1000);
    MediaAVSessionAdapterImpl::UpdateUiPlayPosition(avSessionAdapter, 1050, false);

    // 6. Time difference >= UiTIME_UPDATE_INTERVAL, should update
    MediaAVSessionAdapterImpl::UpdateUiPlayPosition(avSessionAdapter, 1300, false);

    // Clean up
    MediaAVSessionAdapterImpl::callback_wrapper_.Clear(callbackIndex);
    avSessionAdapter->callback_index_ = 0;

    return true;
}

} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);
    OHOS::MediaAVSessionAdapterImplFuzzTest(&fdp);
    OHOS::MediaAVSessionAdapterImplEdgeCaseFuzzTest(&fdp);
    OHOS::MediaAVSessionCommandCallbackFuzzTest(&fdp);
    OHOS::MediaAVSessionCastCallbackFuzzTest(&fdp);
    OHOS::MediaAVSessionUpdateUiFuzzTest(&fdp);
    return 0;
}