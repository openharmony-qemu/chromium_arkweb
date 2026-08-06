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
#include "audio_codec_decoder_adapter_impl.h"
#include "audio_cenc_info_adapter_impl.h"
#undef private
#include <multimedia/drm_framework/native_drm_common.h>
#include <multimedia/player_framework/native_avcodec_base.h>
#include <multimedia/player_framework/native_avbuffer_info.h>

#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include <thread>
#include <chrono>

using namespace testing;

namespace OHOS {
namespace NWeb {
const char *OH_AVCODEC_MIMETYPE_AUDIO_MPEG = "audio/mpeg";
const char *OH_AVCODEC_NAME_AUDIO_MPEG = "OH.Media.Codec.Decoder.Audio.Mpeg";
const int64_t WAIT_FOR_BUFFER_TIMEOUT = 500;

class AudioDecoderCallbackImplTest : public testing::Test {};

class AudioDecoderCallbackAdapterMock : public AudioDecoderCallbackAdapter {
public:
    AudioDecoderCallbackAdapterMock() = default;

    ~AudioDecoderCallbackAdapterMock() override = default;

    void OnError(int32_t errorCode) override {}

    void OnOutputFormatChanged() override {}

    void OnInputBufferAvailable(uint32_t index) override {}

    void OnOutputBufferAvailable(
        uint32_t index, uint8_t *bufferData, int32_t size, int64_t pts, int32_t offset, uint32_t flags) override {}
};

class AudioDecoderFormatAdapterMock : public AudioDecoderFormatAdapter {
public:
    AudioDecoderFormatAdapterMock() = default;

    ~AudioDecoderFormatAdapterMock() override = default;

    int32_t GetSampleRate() override
    {
        return sampleRate_;
    }

    int32_t GetChannelCount() override
    {
        return channelCount_;
    }

    int64_t GetBitRate() override
    {
        return bitRate_;
    }

    int32_t GetMaxInputSize() override
    {
        return maxInputSize_;
    }

    bool GetAACIsAdts() override
    {
        return aacIsAdts_;
    }

    int32_t GetAudioSampleFormat() override
    {
        return audioSampleFormat_;
    }

    int32_t GetIdentificationHeader() override
    {
        return idHeader_;
    }

    int32_t GetSetupHeader() override
    {
        return setupHeader_;
    }

    uint8_t* GetCodecConfig() override
    {
        return codecConfig_;
    }

    uint32_t GetCodecConfigSize() override
    {
        return codecConfigSize_;
    }

    void SetSampleRate(int32_t sampleRate) override
    {
        sampleRate_ = sampleRate;
    }

    void SetChannelCount(int32_t channelCount) override
    {
        channelCount_ = channelCount;
    }

    void SetBitRate(int64_t bitRate) override
    {
        bitRate_ = bitRate;
    }

    void SetMaxInputSize(int32_t maxInputSize) override
    {
        maxInputSize_ = maxInputSize;
    }

    void SetAudioSampleFormat(int32_t audioSampleFormat) override
    {
        audioSampleFormat_ = audioSampleFormat;
    }

    void SetAACIsAdts(bool isAdts) override
    {
        aacIsAdts_ = isAdts;
    }

    void SetIdentificationHeader(int32_t idHeader) override
    {
        idHeader_ = idHeader;
    }

    void SetSetupHeader(int32_t setupHeader) override
    {
        setupHeader_ = setupHeader;
    }

    void SetCodecConfig(uint8_t* codecConfig) override
    {
        codecConfig_ = codecConfig;
    }

    void SetCodecConfigSize(uint32_t size) override
    {
        codecConfigSize_ = size;
    }
private:
    int32_t sampleRate_ = 0;
    int32_t channelCount_ = 0;
    int64_t bitRate_ = 0;
    int32_t maxInputSize_ = 0;
    bool aacIsAdts_ = false;
    int32_t audioSampleFormat_ = 0;
    int32_t idHeader_ = 0;
    int32_t setupHeader_ = 0;
    uint8_t* codecConfig_ = nullptr;
    uint32_t codecConfigSize_ = 0;
};

/**
 * @tc.name: AudioDecoderCallbackImpl_NormalTest_001.
 * @tc.desc: test of AudioDecoderCallbackManager::OnError() OnOutputFormatChanged() OnInputBufferAvailable()
             OnOutputBufferAvailable()
 * @tc.type: FUNC.
 */
TEST_F(AudioDecoderCallbackImplTest, AudioDecoderCallbackImpl_NormalTest_001)
{
    std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder = std::make_shared<AudioCodecDecoderAdapterImpl>();
    EXPECT_NE(decoder, nullptr);
    EXPECT_EQ(decoder->CreateAudioDecoderByName(std::string(OH_AVCODEC_NAME_AUDIO_MPEG)),
        AudioDecoderAdapterCode::DECODER_OK);
    AudioDecoderCallbackManager::OnError(nullptr, 0, nullptr);
    AudioDecoderCallbackManager::OnError(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnOutputFormatChanged(nullptr, 0, nullptr);
    AudioDecoderCallbackManager::OnOutputFormatChanged(decoder->GetAVCodec(), 0, nullptr);
    constexpr int32_t MEMSIZE = 1024 * 1024;
    OH_AVBuffer* buffer = OH_AVBuffer_Create(MEMSIZE);
    AudioDecoderCallbackManager::OnInputBufferAvailable(nullptr, 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnInputBufferAvailable(decoder->GetAVCodec(), 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnInputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);
    AudioDecoderCallbackManager::OnInputBufferAvailable(
        decoder->GetAVCodec(), 0, buffer, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(nullptr, 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(
        decoder->GetAVCodec(), 0, buffer, nullptr);
    EXPECT_EQ(decoder->SetCallbackDec(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);
    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    EXPECT_EQ(decoder->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);
    AudioDecoderCallbackManager::OnError(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnOutputFormatChanged(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnInputBufferAvailable(
        decoder->GetAVCodec(), 0, buffer, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(
        decoder->GetAVCodec(), 0, buffer, nullptr);
    OH_AVBuffer_Destroy(buffer);
    buffer = nullptr;
    std::shared_ptr<AudioDecoderCallbackAdapterImpl> errCallbackImpl =
        std::make_shared<AudioDecoderCallbackAdapterImpl>(nullptr);
    errCallbackImpl->OnError(0);
    errCallbackImpl->OnOutputFormatChanged();
    errCallbackImpl->OnInputBufferAvailable(0);
    errCallbackImpl->OnOutputBufferAvailable(0, nullptr, 0, 0, 0, 0);
    std::shared_ptr<AudioDecoderCallbackAdapterImpl> callbackImpl =
        std::make_shared<AudioDecoderCallbackAdapterImpl>(callback);
    callbackImpl->OnError(0);
    callbackImpl->OnOutputFormatChanged();
    callbackImpl->OnInputBufferAvailable(0);
    callbackImpl->OnOutputBufferAvailable(0, nullptr, 0, 0, 0, 0);
}

/**
 * @tc.name: AudioDecoderCallbackImpl_NormalTest_002.
 * @tc.desc: test of AudioDecoderCallbackManager
 * @tc.type: FUNC.
 */
TEST_F(AudioDecoderCallbackImplTest, AudioDecoderCallbackImpl_NormalTest_002)
{
    std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder = std::make_shared<AudioCodecDecoderAdapterImpl>();
    EXPECT_NE(decoder, nullptr);
    EXPECT_EQ(decoder->CreateAudioDecoderByName(std::string(OH_AVCODEC_NAME_AUDIO_MPEG)),
        AudioDecoderAdapterCode::DECODER_OK);
    AudioDecoderCallbackManager::OnError(nullptr, 0, nullptr);
    AudioDecoderCallbackManager::OnError(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnOutputFormatChanged(nullptr, 0, nullptr);
    AudioDecoderCallbackManager::OnOutputFormatChanged(decoder->GetAVCodec(), 0, nullptr);

    constexpr int32_t MEMSIZE = 1024 * 1024;
    OH_AVBuffer* buffer = OH_AVBuffer_Create(MEMSIZE);
    AudioDecoderCallbackManager::OnInputBufferAvailable(nullptr, 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnInputBufferAvailable(decoder->GetAVCodec(), 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnInputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);
    AudioDecoderCallbackManager::OnInputBufferAvailable(
        decoder->GetAVCodec(), 0, buffer, nullptr);

    AudioDecoderCallbackManager::OnOutputBufferAvailable(nullptr, 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(
        decoder->GetAVCodec(), 0, buffer, nullptr);
    std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder_1 = std::make_shared<AudioCodecDecoderAdapterImpl>();

    decoder_1->CreateAudioDecoderByName(std::string(OH_AVCODEC_NAME_AUDIO_MPEG));
    AudioDecoderCallbackManager::AddAudioDecoder(decoder_1.get());
    AudioDecoderCallbackManager::DeleteAudioDecoder(decoder->GetAVCodec());
    AudioDecoderCallbackManager::OnInputBufferAvailable(decoder->GetAVCodec(), 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, nullptr, nullptr);
    AudioDecoderCallbackManager::OnError(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnOutputFormatChanged(decoder->GetAVCodec(), 0, nullptr);

    AudioDecoderCallbackManager::AddAudioDecoder(nullptr);
    AudioDecoderCallbackManager::DeleteAudioDecoder(nullptr);
    EXPECT_EQ(decoder->SetCallbackDec(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);

    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    EXPECT_EQ(decoder->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);
    AudioDecoderCallbackManager::OnError(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnOutputFormatChanged(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnInputBufferAvailable(
        decoder->GetAVCodec(), 0, buffer, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(
        decoder->GetAVCodec(), 0, buffer, nullptr);
    OH_AVBuffer_Destroy(buffer);
    buffer = nullptr;
}

/**
 * @tc.name: AudioDecoderCallbackImpl_NormalTest_003.
 * @tc.desc: test of AudioDecoderCallbackManager
 * @tc.type: FUNC.
 */
TEST_F(AudioDecoderCallbackImplTest, AudioDecoderCallbackImpl_NormalTest_003)
{
    std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder = std::make_shared<AudioCodecDecoderAdapterImpl>();
    EXPECT_NE(decoder, nullptr);
    EXPECT_EQ(decoder->CreateAudioDecoderByName(std::string(OH_AVCODEC_NAME_AUDIO_MPEG)),
        AudioDecoderAdapterCode::DECODER_OK);

    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    EXPECT_EQ(decoder->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);

    constexpr int32_t MEMSIZE = 1024 * 1024;
    OH_AVBuffer* buffer = OH_AVBuffer_Create(MEMSIZE);
    AudioDecoderCallbackManager::FindAudioDecoder(nullptr);
    AudioDecoderCallbackManager::AddAudioDecoder(nullptr);
    AudioDecoderCallbackManager::decoders_.clear();
    AudioDecoderCallbackManager::AddAudioDecoder(decoder.get());
    AudioDecoderCallbackManager::OnError(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnOutputFormatChanged(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnInputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);
    AudioDecoderCallbackManager::DeleteAudioDecoder(nullptr);
    OH_AVBuffer_Destroy(buffer);
    buffer = nullptr;
}

class AudioCodecDecoderAdapterImplTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
    void SetCencInfoAboutKeyIdIvAlgo(std::shared_ptr<AudioCencInfoAdapter> cencInfo);
    void SetCencInfoAboutClearHeaderAndPayLoadLens(std::shared_ptr<AudioCencInfoAdapter> cencInfo);

protected:
    std::shared_ptr<AudioDecoderFormatAdapterMock> format_ = nullptr;
    std::shared_ptr<AudioCodecDecoderAdapterImpl> AudioCodecDecoderAdapterImpl_ = nullptr;
};

void AudioCodecDecoderAdapterImplTest::SetUpTestCase(void) {}

void AudioCodecDecoderAdapterImplTest::TearDownTestCase(void) {}

void AudioCodecDecoderAdapterImplTest::SetUp()
{
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_, nullptr);
    AudioCodecDecoderAdapterImpl_ = std::make_shared<AudioCodecDecoderAdapterImpl>();
    EXPECT_NE(AudioCodecDecoderAdapterImpl_, nullptr);
    format_ = std::make_unique<AudioDecoderFormatAdapterMock>();
    EXPECT_NE(format_, nullptr);
}

void AudioCodecDecoderAdapterImplTest::TearDown(void)
{
    format_ = nullptr;
    AudioCodecDecoderAdapterImpl_ = nullptr;
}

void AudioCodecDecoderAdapterImplTest::SetCencInfoAboutKeyIdIvAlgo(
    std::shared_ptr<AudioCencInfoAdapter> cencInfo)
{
    uint8_t keyId[] = {
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
    uint8_t iv[] = {
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
    cencInfo->SetAlgo(0);
    cencInfo->SetKeyId(keyId);
    cencInfo->SetKeyIdLen(DRM_KEY_ID_SIZE);
    cencInfo->SetIv(iv);
    cencInfo->SetIvLen(DRM_KEY_ID_SIZE);
}

void AudioCodecDecoderAdapterImplTest::SetCencInfoAboutClearHeaderAndPayLoadLens(
    std::shared_ptr<AudioCencInfoAdapter> cencInfo)
{
    const std::vector<uint32_t> clearHeaderLens = {1, 2, 3};
    const std::vector<uint32_t> payLoadLens = {4, 5, 6};
    const uint32_t ERR_BLOCK_COUNT = 10000;
    cencInfo->SetClearHeaderLens(clearHeaderLens);
    cencInfo->SetPayLoadLens(payLoadLens);
    cencInfo->SetEncryptedBlockCount(ERR_BLOCK_COUNT);
    cencInfo->SetSkippedBlockCount(0);
    cencInfo->SetFirstEncryptedOffset(0);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_CreateAudioDecoderByName_001.
 * @tc.desc: test of AudioCodecDecoderAdapterImpl::CreateAudioDecoderByName() CreateAudioDecoderByMime()
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_CreateAudioDecoderByName_001)
{
    EXPECT_NE(AudioCodecDecoderAdapterImpl_, nullptr);

    // create decoder by invalid name or mimetype.
    std::string name = "testname";
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByName(name), AudioDecoderAdapterCode::DECODER_ERROR);

    std::string mimetype = "testmimeType";
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype),
        AudioDecoderAdapterCode::DECODER_ERROR);

    // create decoder by normal name.
    name = std::string(OH_AVCODEC_NAME_AUDIO_MPEG);
    AudioDecoderAdapterCode ret = AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByName(name);
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);

    // repeat create
    ret = AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByName(name);
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);

    // release decoder.
    ret = AudioCodecDecoderAdapterImpl_->ReleaseDecoder();
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);

    // create decoder by normal mimetype.
    mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    ret = AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype);
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);

    // repeat create
    ret = AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype);
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);

    // release decoder.
    ret = AudioCodecDecoderAdapterImpl_->ReleaseDecoder();
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);

    // repeat release decoder.
    ret = AudioCodecDecoderAdapterImpl_->ReleaseDecoder();
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_InvalidValueTest_002.
 * @tc.desc: test of InvalidValueScene in AudioCodecDecoderAdapterImpl
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_InvalidValueTest_002)
{
    // not create decoder
    format_->SetSampleRate(0);
    format_->SetChannelCount(0);
    format_->SetBitRate(0);
    format_->SetMaxInputSize(0);
    format_->SetAudioSampleFormat(0);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(format_), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetParameterDecoder(format_), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetParameterDecoder(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);

    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetCallbackDec(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetAudioDecoderCallBack(), nullptr);

    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, nullptr, false,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputFormatDec(format_), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ReleaseOutputBufferDec(0), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->PrepareDecoder(), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StartDecoder(), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StopDecoder(), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->FlushDecoder(), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ResetDecoder(), AudioDecoderAdapterCode::DECODER_ERROR);
    AudioCodecDecoderAdapterImpl_->ResetDecoder();
    AudioCodecDecoderAdapterImpl_->FlushDecoder();
    AudioCodecDecoderAdapterImpl_->StopDecoder();
    AudioCodecDecoderAdapterImpl_->StartDecoder();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetDecryptionConfig(nullptr, false),
        AudioDecoderAdapterCode::DECODER_ERROR);

    int32_t tmp = 100;
    void *session = reinterpret_cast<void*>(&tmp);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetDecryptionConfig(session, false),
        AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetBufferFlag(static_cast<OH_AVCodecBufferFlags>(tmp)),
        BufferFlag::CODEC_BUFFER_FLAG_NONE);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_QueueInputBufferDec_003.
 * @tc.desc: test of InvalidValueScene in AudioCodecDecoderAdapterImpl
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_QueueInputBufferDec_003)
{
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    AudioDecoderAdapterCode ret = AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype);
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, nullptr, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    // test QueueInputBufferDec with decrypt data
    std::shared_ptr<AudioCencInfoAdapterImpl> cencInfo = std::make_shared<AudioCencInfoAdapterImpl>();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    const uint32_t ERR_ALGO = 10000;
    cencInfo->SetAlgo(ERR_ALGO);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    SetCencInfoAboutKeyIdIvAlgo(cencInfo);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    SetCencInfoAboutClearHeaderAndPayLoadLens(cencInfo);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    cencInfo->SetEncryptedBlockCount(0);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    const uint32_t ERR_MODE = 10000;
    cencInfo->SetMode(ERR_MODE);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    cencInfo->SetMode(0);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    // test QueueInputBufferDec with input buffer and output buffer
    constexpr int32_t MEMSIZE = 1024 * 1024;
    OH_AVBuffer* buffer = OH_AVBuffer_Create(MEMSIZE);
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(0, buffer);
    AudioCodecDecoderAdapterImpl_->SetOutputBuffer(0, buffer);
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(0, nullptr);
    AudioCodecDecoderAdapterImpl_->SetOutputBuffer(0, nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(1, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, cencInfo, true,
        BufferFlag::CODEC_BUFFER_FLAG_EOS), AudioDecoderAdapterCode::DECODER_ERROR);
    AudioCodecDecoderAdapterImpl_->ReleaseOutputBufferDec(0);
    AudioCodecDecoderAdapterImpl_->ReleaseOutputBufferDec(100);
    OH_AVBuffer_Destroy(buffer);
    buffer = nullptr;
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_NormalValueTest_004.
 * @tc.desc: test of NormalScene in AudioCodecDecoderAdapterImpl
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_NormalValueTest_004)
{
    // create decoder by normal mimetype.
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    AudioDecoderAdapterCode ret = AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype);
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);

    // config decoder.
    constexpr uint32_t DEFAULT_SAMPLERATE = 44100;
    constexpr uint32_t DEFAULT_BITRATE = 32000;
    constexpr uint32_t DEFAULT_CHANNEL_COUNT = 2;
    constexpr uint32_t DEFAULT_MAX_INPUT_SIZE = 1152;
    format_->SetSampleRate(DEFAULT_SAMPLERATE);
    format_->SetChannelCount(DEFAULT_CHANNEL_COUNT);
    format_->SetBitRate(DEFAULT_BITRATE);
    format_->SetMaxInputSize(DEFAULT_MAX_INPUT_SIZE);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(format_), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputFormatDec(nullptr), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputFormatDec(format_), AudioDecoderAdapterCode::DECODER_OK);

    // set callback for decoding.
    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);

    // prepare decoder.
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->PrepareDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StartDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetParameterDecoder(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetParameterDecoder(format_), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->FlushDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StopDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ResetDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);

    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetBufferFlag(
        OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_EOS), BufferFlag::CODEC_BUFFER_FLAG_EOS);
    AudioCodecDecoderAdapterImpl_->ResetDecoder();
    AudioCodecDecoderAdapterImpl_->FlushDecoder();
    AudioCodecDecoderAdapterImpl_->StopDecoder();
    AudioCodecDecoderAdapterImpl_->StartDecoder();
    (void)AudioCodecDecoderAdapterImpl_->GetDecoderMutex();
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_NormalValueTest_005.
 * @tc.desc: test of NormalScene in AudioCodecDecoderAdapterImpl
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_NormalValueTest_005)
{
    constexpr int32_t DEFAULT_SAMPLERATE = 44100;
    constexpr int64_t DEFAULT_BITRATE = 32000;
    constexpr int32_t DEFAULT_CHANNEL_COUNT = 2;
    constexpr int32_t DEFAULT_MAX_INPUT_SIZE = 1152;
    constexpr bool DEFAUL_AAC_IS_ADTS = false;
    constexpr int32_t DEFAULT_AUDIO_SAMPLE_FORMAT = 100;
    constexpr int32_t DEFAUL_ID_HEARDER = 100;
    constexpr int32_t DEFAULT_SETUP_HEADER = 100;
    uint8_t codecConfig[10] = {0};
    constexpr uint32_t DEFAULT_CODEC_Config_SIZE = 10;
    std::shared_ptr<AudioDecoderFormatAdapterImpl> format = std::make_shared<AudioDecoderFormatAdapterImpl>();
    format->SetSampleRate(DEFAULT_SAMPLERATE);
    format->SetChannelCount(DEFAULT_CHANNEL_COUNT);
    format->SetBitRate(DEFAULT_BITRATE);
    format->SetMaxInputSize(DEFAULT_MAX_INPUT_SIZE);
    format->SetAACIsAdts(DEFAUL_AAC_IS_ADTS);
    format->SetAudioSampleFormat(DEFAULT_AUDIO_SAMPLE_FORMAT);
    format->SetIdentificationHeader(DEFAUL_ID_HEARDER);
    format->SetSetupHeader(DEFAULT_SETUP_HEADER);
    format->SetCodecConfig(codecConfig);
    format->SetCodecConfigSize(DEFAULT_CODEC_Config_SIZE);
    format->PrintFormatData(format);
    EXPECT_EQ(format->GetSampleRate(), DEFAULT_SAMPLERATE);
    EXPECT_EQ(format->GetChannelCount(), DEFAULT_CHANNEL_COUNT);
    EXPECT_EQ(format->GetBitRate(), DEFAULT_BITRATE);
    EXPECT_EQ(format->GetMaxInputSize(), DEFAULT_MAX_INPUT_SIZE);
    EXPECT_EQ(format->GetAACIsAdts(), DEFAUL_AAC_IS_ADTS);
    EXPECT_EQ(format->GetAudioSampleFormat(), DEFAULT_AUDIO_SAMPLE_FORMAT);
    EXPECT_EQ(format->GetIdentificationHeader(), DEFAUL_ID_HEARDER);
    EXPECT_EQ(format->GetSetupHeader(), DEFAULT_SETUP_HEADER);
    EXPECT_EQ(format->GetCodecConfig(), codecConfig);
    EXPECT_EQ(format->GetCodecConfigSize(), DEFAULT_CODEC_Config_SIZE);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_NormalValueTest_006.
 * @tc.desc: test of NormalScene in AudioCodecDecoderAdapterImpl
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_NormalValueTest_006)
{
    // create decoder by normal mimetype.
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    AudioDecoderAdapterCode ret = AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype);
    EXPECT_EQ(ret, AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_NE(AudioCodecDecoderAdapterImpl_->GetAVCodec(), nullptr);

    AudioCodecDecoderAdapterImpl_->inputBuffers_.clear();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(1000), nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ReleaseOutputBufferDec(0), AudioDecoderAdapterCode::DECODER_ERROR);

    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, nullptr, false,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
    std::shared_ptr<AudioCencInfoAdapterImpl> cencInfo = std::make_shared<AudioCencInfoAdapterImpl>();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StartDecoder(), AudioDecoderAdapterCode::DECODER_ERROR);
    std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_FOR_BUFFER_TIMEOUT));
    if (!AudioCodecDecoderAdapterImpl_->inputBuffers_.empty()) {
        int32_t index = AudioCodecDecoderAdapterImpl_->inputBuffers_.begin()->first;
        EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(index, 0, nullptr, 0, cencInfo, true,
            BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
        EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(index, 0, nullptr, 0, cencInfo, true,
            BufferFlag::CODEC_BUFFER_FLAG_EOS), AudioDecoderAdapterCode::DECODER_ERROR);
        AudioCodecDecoderAdapterImpl_->ReleaseOutputBufferDec(index);
    }
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_GetBufferFlag_007.
 * @tc.desc: test all branches of GetBufferFlag mapping
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_GetBufferFlag_007)
{
    // Test all valid buffer flag mappings
    EXPECT_EQ(AudioCodecDecoderAdapterImpl::GetBufferFlag(
        OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_NONE), BufferFlag::CODEC_BUFFER_FLAG_NONE);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl::GetBufferFlag(
        OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_EOS), BufferFlag::CODEC_BUFFER_FLAG_EOS);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl::GetBufferFlag(
        OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_SYNC_FRAME), BufferFlag::CODEC_BUFFER_FLAG_SYNC_FRAME);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl::GetBufferFlag(
        OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_INCOMPLETE_FRAME),
        BufferFlag::CODEC_BUFFER_FLAG_PARTIAL_FRAME);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl::GetBufferFlag(
        OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_CODEC_DATA), BufferFlag::CODEC_BUFFER_FLAG_CODEC_DATA);

    // Test unknown/invalid flag returns CODEC_BUFFER_FLAG_NONE
    EXPECT_EQ(AudioCodecDecoderAdapterImpl::GetBufferFlag(
        static_cast<OH_AVCodecBufferFlags>(9999)), BufferFlag::CODEC_BUFFER_FLAG_NONE);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_SetInputOutputBuffer_008.
 * @tc.desc: test SetInputBuffer/SetOutputBuffer and GetInputBuffer/GetOutputBuffer edge cases
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_SetInputOutputBuffer_008)
{
    constexpr int32_t MEMSIZE = 1024;
    OH_AVBuffer* buffer1 = OH_AVBuffer_Create(MEMSIZE);
    OH_AVBuffer* buffer2 = OH_AVBuffer_Create(MEMSIZE);
    ASSERT_NE(buffer1, nullptr);
    ASSERT_NE(buffer2, nullptr);

    // Test empty map lookup
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetInputBuffer(0), nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(0), nullptr);

    // Test set and get input buffer
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(0, buffer1);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetInputBuffer(0), buffer1);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetInputBuffer(1), nullptr);

    // Test set and get output buffer
    AudioCodecDecoderAdapterImpl_->SetOutputBuffer(0, buffer2);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(0), buffer2);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(1), nullptr);

    // Test duplicate set (should be ignored, buffer remains the same)
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(0, buffer2);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetInputBuffer(0), buffer1);
    AudioCodecDecoderAdapterImpl_->SetOutputBuffer(0, buffer1);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(0), buffer2);

    // Test multiple indices
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(1, buffer2);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetInputBuffer(1), buffer2);
    AudioCodecDecoderAdapterImpl_->SetOutputBuffer(1, buffer1);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(1), buffer1);

    OH_AVBuffer_Destroy(buffer1);
    OH_AVBuffer_Destroy(buffer2);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_QueueInputBufferDec_EOS_009.
 * @tc.desc: test QueueInputBufferDec with EOS flag and valid input buffer
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_QueueInputBufferDec_EOS_009)
{
    // Without decoder, should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, nullptr, false,
        BufferFlag::CODEC_BUFFER_FLAG_EOS), AudioDecoderAdapterCode::DECODER_ERROR);

    // Create decoder
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);

    // With decoder but no input buffer in map, should fail
    AudioCodecDecoderAdapterImpl_->inputBuffers_.clear();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, nullptr, false,
        BufferFlag::CODEC_BUFFER_FLAG_EOS), AudioDecoderAdapterCode::DECODER_ERROR);

    // Set up input buffer
    constexpr int32_t MEMSIZE = 1024 * 1024;
    OH_AVBuffer* buffer = OH_AVBuffer_Create(MEMSIZE);
    ASSERT_NE(buffer, nullptr);
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(0, buffer);

    // With valid input buffer, non-EOS flag with null bufferData should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 1000, nullptr, 4, nullptr, false,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);

    // EOS flag with null bufferData - bufferData null is acceptable for EOS
    // But OH_AudioCodec_PushInputBuffer will likely fail since decoder not configured/started
    (void)AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(
        0, 1000, nullptr, 0, nullptr, false, BufferFlag::CODEC_BUFFER_FLAG_EOS);

    OH_AVBuffer_Destroy(buffer);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_QueueInputBufferDec_BufferCapacity_010.
 * @tc.desc: test QueueInputBufferDec when buffer capacity is less than buffer size
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_QueueInputBufferDec_BufferCapacity_010)
{
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);

    // Create a small buffer to test capacity check
    constexpr int32_t SMALL_MEMSIZE = 16;
    OH_AVBuffer* smallBuffer = OH_AVBuffer_Create(SMALL_MEMSIZE);
    ASSERT_NE(smallBuffer, nullptr);
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(0, smallBuffer);

    // bufferSize larger than buffer capacity should fail
    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, testData, SMALL_MEMSIZE + 100,
        nullptr, false, BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);

    OH_AVBuffer_Destroy(smallBuffer);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_SetDecryptionConfig_011.
 * @tc.desc: test SetDecryptionConfig with various session and secureAudio values
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_SetDecryptionConfig_011)
{
    // null session should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetDecryptionConfig(nullptr, false),
        AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetDecryptionConfig(nullptr, true),
        AudioDecoderAdapterCode::DECODER_ERROR);

    // null decoder with valid session should fail
    int32_t tmp = 100;
    void *session = reinterpret_cast<void*>(&tmp);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetDecryptionConfig(session, false),
        AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetDecryptionConfig(session, true),
        AudioDecoderAdapterCode::DECODER_ERROR);

    // Create decoder, verify SetDecryptionConfig with null session still fails
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);

    // null session with valid decoder should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetDecryptionConfig(nullptr, false),
        AudioDecoderAdapterCode::DECODER_ERROR);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetDecryptionConfig(nullptr, true),
        AudioDecoderAdapterCode::DECODER_ERROR);

    // Note: SetDecryptionConfig with a real session calls OH_AudioCodec_SetDecryptionConfig
    // which requires a valid MediaKeySession object. Using a fake pointer causes crash.
    // Only test parameter validation paths here.
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_GetOutputFormatDec_012.
 * @tc.desc: test GetOutputFormatDec when GetOutputDescription returns nullptr
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_GetOutputFormatDec_012)
{
    // Without decoder should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputFormatDec(format_),
        AudioDecoderAdapterCode::DECODER_ERROR);

    // Create decoder, configure, set callback, prepare, start for valid output format
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);
    constexpr uint32_t DEFAULT_SAMPLERATE = 44100;
    constexpr uint32_t DEFAULT_CHANNEL_COUNT = 2;
    format_->SetSampleRate(DEFAULT_SAMPLERATE);
    format_->SetChannelCount(DEFAULT_CHANNEL_COUNT);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(format_), AudioDecoderAdapterCode::DECODER_OK);
    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->PrepareDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StartDecoder(), AudioDecoderAdapterCode::DECODER_OK);

    // With valid decoder, GetOutputFormatDec should succeed
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputFormatDec(format_),
        AudioDecoderAdapterCode::DECODER_OK);

    // Stop and release
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StopDecoder(), AudioDecoderAdapterCode::DECODER_OK);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_FlushDecoder_ClearBuffers_013.
 * @tc.desc: test FlushDecoder clears input and output buffers
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_FlushDecoder_ClearBuffers_013)
{
    // Without decoder should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->FlushDecoder(), AudioDecoderAdapterCode::DECODER_ERROR);

    // Create decoder
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);

    // Add buffers to the maps
    constexpr int32_t MEMSIZE = 1024;
    OH_AVBuffer* buffer1 = OH_AVBuffer_Create(MEMSIZE);
    OH_AVBuffer* buffer2 = OH_AVBuffer_Create(MEMSIZE);
    ASSERT_NE(buffer1, nullptr);
    ASSERT_NE(buffer2, nullptr);
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(0, buffer1);
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(1, buffer1);
    AudioCodecDecoderAdapterImpl_->SetOutputBuffer(0, buffer2);
    AudioCodecDecoderAdapterImpl_->SetOutputBuffer(1, buffer2);

    // Verify buffers are in the maps
    EXPECT_NE(AudioCodecDecoderAdapterImpl_->GetInputBuffer(0), nullptr);
    EXPECT_NE(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(0), nullptr);

    // Configure, set callback, prepare, start - needed before flush can succeed
    constexpr uint32_t DEFAULT_SAMPLERATE = 44100;
    constexpr uint32_t DEFAULT_CHANNEL_COUNT = 2;
    format_->SetSampleRate(DEFAULT_SAMPLERATE);
    format_->SetChannelCount(DEFAULT_CHANNEL_COUNT);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(format_), AudioDecoderAdapterCode::DECODER_OK);
    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->PrepareDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StartDecoder(), AudioDecoderAdapterCode::DECODER_OK);

    // Flush should clear the buffer maps
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->FlushDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetInputBuffer(0), nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetInputBuffer(1), nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(0), nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(1), nullptr);

    // Stop decoder before destroying buffers
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StopDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    OH_AVBuffer_Destroy(buffer1);
    OH_AVBuffer_Destroy(buffer2);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_ResetDecoder_ClearBuffers_014.
 * @tc.desc: test ResetDecoder clears input and output buffers
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_ResetDecoder_ClearBuffers_014)
{
    // Without decoder should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ResetDecoder(), AudioDecoderAdapterCode::DECODER_ERROR);

    // Create decoder
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);

    // Add buffers to the maps
    constexpr int32_t MEMSIZE = 1024;
    OH_AVBuffer* buffer1 = OH_AVBuffer_Create(MEMSIZE);
    OH_AVBuffer* buffer2 = OH_AVBuffer_Create(MEMSIZE);
    ASSERT_NE(buffer1, nullptr);
    ASSERT_NE(buffer2, nullptr);
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(0, buffer1);
    AudioCodecDecoderAdapterImpl_->SetOutputBuffer(0, buffer2);

    // Configure and start
    constexpr uint32_t DEFAULT_SAMPLERATE = 44100;
    constexpr uint32_t DEFAULT_CHANNEL_COUNT = 2;
    format_->SetSampleRate(DEFAULT_SAMPLERATE);
    format_->SetChannelCount(DEFAULT_CHANNEL_COUNT);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(format_), AudioDecoderAdapterCode::DECODER_OK);
    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->PrepareDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StartDecoder(), AudioDecoderAdapterCode::DECODER_OK);

    // Reset should clear the buffer maps
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ResetDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetInputBuffer(0), nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(0), nullptr);

    OH_AVBuffer_Destroy(buffer1);
    OH_AVBuffer_Destroy(buffer2);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_ReleaseDecoder_ClearBuffers_015.
 * @tc.desc: test ReleaseDecoder clears input and output buffers and sets decoder_ to nullptr
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_ReleaseDecoder_ClearBuffers_015)
{
    // Release without decoder should return OK
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ReleaseDecoder(), AudioDecoderAdapterCode::DECODER_OK);

    // Create decoder
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_NE(AudioCodecDecoderAdapterImpl_->GetAVCodec(), nullptr);

    // Add buffers
    constexpr int32_t MEMSIZE = 1024;
    OH_AVBuffer* buffer1 = OH_AVBuffer_Create(MEMSIZE);
    OH_AVBuffer* buffer2 = OH_AVBuffer_Create(MEMSIZE);
    ASSERT_NE(buffer1, nullptr);
    ASSERT_NE(buffer2, nullptr);
    AudioCodecDecoderAdapterImpl_->SetInputBuffer(0, buffer1);
    AudioCodecDecoderAdapterImpl_->SetOutputBuffer(0, buffer2);

    // Release should succeed and clear buffers
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ReleaseDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetAVCodec(), nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetInputBuffer(0), nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputBuffer(0), nullptr);

    // Double release should return OK
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ReleaseDecoder(), AudioDecoderAdapterCode::DECODER_OK);

    OH_AVBuffer_Destroy(buffer1);
    OH_AVBuffer_Destroy(buffer2);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_CallbackManager_FindDelete_016.
 * @tc.desc: test AudioDecoderCallbackManager FindAudioDecoder and DeleteAudioDecoder with edge cases
 * @tc.type: FUNC.
 */
TEST_F(AudioDecoderCallbackImplTest, AudioCodecDecoderAdapterImpl_CallbackManager_FindDelete_016)
{
    // Find with nullptr codec should return nullptr
    EXPECT_EQ(AudioDecoderCallbackManager::FindAudioDecoder(nullptr), nullptr);

    // Create decoder
    std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder = std::make_shared<AudioCodecDecoderAdapterImpl>();
    EXPECT_EQ(decoder->CreateAudioDecoderByName(std::string(OH_AVCODEC_NAME_AUDIO_MPEG)),
        AudioDecoderAdapterCode::DECODER_OK);
    OH_AVCodec* codec = decoder->GetAVCodec();
    ASSERT_NE(codec, nullptr);

    // After CreateAudioDecoderByName, decoder is already in the map
    EXPECT_EQ(AudioDecoderCallbackManager::FindAudioDecoder(codec), decoder.get());

    // Delete and verify removed
    AudioDecoderCallbackManager::DeleteAudioDecoder(codec);
    EXPECT_EQ(AudioDecoderCallbackManager::FindAudioDecoder(codec), nullptr);

    // Delete again (already removed) should not crash
    AudioDecoderCallbackManager::DeleteAudioDecoder(codec);

    // Add back
    AudioDecoderCallbackManager::AddAudioDecoder(decoder.get());
    EXPECT_EQ(AudioDecoderCallbackManager::FindAudioDecoder(codec), decoder.get());

    // Add duplicate should be ignored
    AudioDecoderCallbackManager::AddAudioDecoder(decoder.get());

    // Add nullptr should be ignored
    AudioDecoderCallbackManager::AddAudioDecoder(nullptr);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_SetBufferCencInfo_NullCencInfo_017.
 * @tc.desc: test SetBufferCencInfo with nullptr cencInfo
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_SetBufferCencInfo_NullCencInfo_017)
{
    // Without decoder, QueueInputBufferDec with isEncrypted=true and null cencInfo should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, nullptr, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);

    // Create decoder
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);

    // With decoder but null cencInfo and isEncrypted=true should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->QueueInputBufferDec(0, 0, nullptr, 0, nullptr, true,
        BufferFlag::CODEC_BUFFER_FLAG_NONE), AudioDecoderAdapterCode::DECODER_ERROR);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_FormatAdapterImpl_AACIsAdts_018.
 * @tc.desc: test AudioDecoderFormatAdapterImpl with AAC IsAdts true and false, and all getters/setters
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_FormatAdapterImpl_AACIsAdts_018)
{
    std::shared_ptr<AudioDecoderFormatAdapterImpl> format = std::make_shared<AudioDecoderFormatAdapterImpl>();

    // Default is false
    EXPECT_EQ(format->GetAACIsAdts(), false);

    // Set to true
    format->SetAACIsAdts(true);
    EXPECT_EQ(format->GetAACIsAdts(), true);

    // Set back to false
    format->SetAACIsAdts(false);
    EXPECT_EQ(format->GetAACIsAdts(), false);

    // Test all getter/setter pairs with non-default values
    format->SetSampleRate(48000);
    EXPECT_EQ(format->GetSampleRate(), 48000);

    format->SetChannelCount(6);
    EXPECT_EQ(format->GetChannelCount(), 6);

    format->SetBitRate(128000);
    EXPECT_EQ(format->GetBitRate(), 128000);

    format->SetMaxInputSize(4096);
    EXPECT_EQ(format->GetMaxInputSize(), 4096);

    format->SetAudioSampleFormat(1);
    EXPECT_EQ(format->GetAudioSampleFormat(), 1);

    format->SetIdentificationHeader(42);
    EXPECT_EQ(format->GetIdentificationHeader(), 42);

    format->SetSetupHeader(84);
    EXPECT_EQ(format->GetSetupHeader(), 84);

    uint8_t configData[] = {0x12, 0x34};
    format->SetCodecConfig(configData);
    EXPECT_EQ(format->GetCodecConfig(), configData);

    format->SetCodecConfigSize(2);
    EXPECT_EQ(format->GetCodecConfigSize(), 2u);

    // PrintFormatData with valid format
    AudioDecoderFormatAdapterImpl::PrintFormatData(format);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_GetParamFromAVFormat_Null_019.
 * @tc.desc: test GetParamFromAVFormat with null parameters via GetOutputFormatDec
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_GetParamFromAVFormat_Null_019)
{
    // Create decoder
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);

    // Configure decoder
    constexpr uint32_t DEFAULT_SAMPLERATE = 44100;
    constexpr uint32_t DEFAULT_CHANNEL_COUNT = 2;
    format_->SetSampleRate(DEFAULT_SAMPLERATE);
    format_->SetChannelCount(DEFAULT_CHANNEL_COUNT);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(format_), AudioDecoderAdapterCode::DECODER_OK);

    // GetOutputFormatDec with null format: GetParamFromAVFormat checks null and returns early,
    // but GetOutputFormatDec itself returns DECODER_OK since avFormat was obtained successfully.
    // The null format path in GetParamFromAVFormat is safely handled.
    (void)AudioCodecDecoderAdapterImpl_->GetOutputFormatDec(nullptr);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_CallbackAdapterImpl_020.
 * @tc.desc: test AudioDecoderCallbackAdapterImpl with valid callback
 * @tc.type: FUNC.
 */
TEST_F(AudioDecoderCallbackImplTest, AudioCodecDecoderAdapterImpl_CallbackAdapterImpl_020)
{
    // Test with valid callback
    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    std::shared_ptr<AudioDecoderCallbackAdapterImpl> callbackImpl =
        std::make_shared<AudioDecoderCallbackAdapterImpl>(callback);

    // All callbacks should be forwarded without crash
    callbackImpl->OnError(0);
    callbackImpl->OnError(-1);
    callbackImpl->OnOutputFormatChanged();
    callbackImpl->OnInputBufferAvailable(0);
    callbackImpl->OnInputBufferAvailable(100);
    uint8_t testData[] = {0x01, 0x02, 0x03};
    callbackImpl->OnOutputBufferAvailable(0, testData, 3, 1000, 0, 0);
    callbackImpl->OnOutputBufferAvailable(1, nullptr, 0, 0, 0, 0);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_OnOutputBufferAvailable_Edge_021.
 * @tc.desc: test OnOutputBufferAvailable with edge cases (negative size, oversized attr)
 * @tc.type: FUNC.
 */
TEST_F(AudioDecoderCallbackImplTest, AudioCodecDecoderAdapterImpl_OnOutputBufferAvailable_Edge_021)
{
    std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder = std::make_shared<AudioCodecDecoderAdapterImpl>();
    EXPECT_EQ(decoder->CreateAudioDecoderByName(std::string(OH_AVCODEC_NAME_AUDIO_MPEG)),
        AudioDecoderAdapterCode::DECODER_OK);
    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    EXPECT_EQ(decoder->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);

    constexpr int32_t MEMSIZE = 1024 * 1024;
    OH_AVBuffer* buffer = OH_AVBuffer_Create(MEMSIZE);
    ASSERT_NE(buffer, nullptr);

    // Set a valid buffer attr (normal case)
    OH_AVCodecBufferAttr attr = {0};
    attr.pts = 1000;
    attr.size = 100;
    attr.offset = 0;
    attr.flags = OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_NONE;
    OH_AVBuffer_SetBufferAttr(buffer, &attr);

    // Normal OnOutputBufferAvailable call
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);

    // Test with zero-size output buffer
    attr.size = 0;
    OH_AVBuffer_SetBufferAttr(buffer, &attr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);

    // Test with negative size (should trigger errCode check)
    attr.size = -1;
    OH_AVBuffer_SetBufferAttr(buffer, &attr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);

    // Test with size larger than capacity (should trigger attr.size > capacity check)
    attr.size = MEMSIZE + 100;
    OH_AVBuffer_SetBufferAttr(buffer, &attr);
    AudioDecoderCallbackManager::OnOutputBufferAvailable(decoder->GetAVCodec(), 0, buffer, nullptr);

    OH_AVBuffer_Destroy(buffer);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_CreateByMime_ThenByName_022.
 * @tc.desc: test CreateAudioDecoderByMime then CreateAudioDecoderByName replaces decoder
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_CreateByMime_ThenByName_022)
{
    // Create by mime first
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_NE(AudioCodecDecoderAdapterImpl_->GetAVCodec(), nullptr);

    // Create by name should release the old one and create new decoder
    std::string name = std::string(OH_AVCODEC_NAME_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByName(name), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_NE(AudioCodecDecoderAdapterImpl_->GetAVCodec(), nullptr);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_DecoderFullLifecycle_023.
 * @tc.desc: test full lifecycle: create -> configure -> setCallback -> prepare -> start -> stop -> release
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_DecoderFullLifecycle_023)
{
    // Create
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_NE(AudioCodecDecoderAdapterImpl_->GetAVCodec(), nullptr);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetAudioDecoderCallBack(), nullptr);

    // Configure
    constexpr uint32_t DEFAULT_SAMPLERATE = 44100;
    constexpr uint32_t DEFAULT_CHANNEL_COUNT = 2;
    constexpr uint32_t DEFAULT_BITRATE = 32000;
    format_->SetSampleRate(DEFAULT_SAMPLERATE);
    format_->SetChannelCount(DEFAULT_CHANNEL_COUNT);
    format_->SetBitRate(DEFAULT_BITRATE);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(format_), AudioDecoderAdapterCode::DECODER_OK);

    // Set callback
    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_NE(AudioCodecDecoderAdapterImpl_->GetAudioDecoderCallBack(), nullptr);

    // Prepare
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->PrepareDecoder(), AudioDecoderAdapterCode::DECODER_OK);

    // Start
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StartDecoder(), AudioDecoderAdapterCode::DECODER_OK);

    // Get output format
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetOutputFormatDec(format_), AudioDecoderAdapterCode::DECODER_OK);

    // SetParameter while running
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetParameterDecoder(format_), AudioDecoderAdapterCode::DECODER_OK);

    // Stop
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->StopDecoder(), AudioDecoderAdapterCode::DECODER_OK);

    // Release
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ReleaseDecoder(), AudioDecoderAdapterCode::DECODER_OK);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->GetAVCodec(), nullptr);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_Destructor_024.
 * @tc.desc: test destructor releases decoder properly
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_Destructor_024)
{
    // Create a decoder in a scoped block to test destructor
    {
        std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder = std::make_shared<AudioCodecDecoderAdapterImpl>();
        std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
        EXPECT_EQ(decoder->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);
        EXPECT_NE(decoder->GetAVCodec(), nullptr);
    }
    // If we reach here without crash, destructor worked correctly

    // Test destructor with null decoder (no decoder created)
    {
        std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder = std::make_shared<AudioCodecDecoderAdapterImpl>();
    }
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_OnInputBufferAvailable_NullCodec_025.
 * @tc.desc: test OnInputBufferAvailable with various null parameters
 * @tc.type: FUNC.
 */
TEST_F(AudioDecoderCallbackImplTest, AudioCodecDecoderAdapterImpl_OnInputBufferAvailable_NullCodec_025)
{
    // null codec
    AudioDecoderCallbackManager::OnInputBufferAvailable(nullptr, 0, nullptr, nullptr);

    // valid codec but not in the map (after deletion)
    std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder = std::make_shared<AudioCodecDecoderAdapterImpl>();
    EXPECT_EQ(decoder->CreateAudioDecoderByName(std::string(OH_AVCODEC_NAME_AUDIO_MPEG)),
        AudioDecoderAdapterCode::DECODER_OK);
    OH_AVCodec* codec = decoder->GetAVCodec();

    // Remove from the map
    AudioDecoderCallbackManager::DeleteAudioDecoder(codec);
    constexpr int32_t MEMSIZE = 1024;
    OH_AVBuffer* buffer = OH_AVBuffer_Create(MEMSIZE);
    ASSERT_NE(buffer, nullptr);

    // Codec not in map, should not crash
    AudioDecoderCallbackManager::OnInputBufferAvailable(codec, 0, buffer, nullptr);

    OH_AVBuffer_Destroy(buffer);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_OnError_OnOutputFormatChanged_NoCallback_026.
 * @tc.desc: test OnError and OnOutputFormatChanged when callback is not set
 * @tc.type: FUNC.
 */
TEST_F(AudioDecoderCallbackImplTest, AudioCodecDecoderAdapterImpl_OnError_OnOutputFormatChanged_NoCallback_026)
{
    std::shared_ptr<AudioCodecDecoderAdapterImpl> decoder = std::make_shared<AudioCodecDecoderAdapterImpl>();
    EXPECT_EQ(decoder->CreateAudioDecoderByName(std::string(OH_AVCODEC_NAME_AUDIO_MPEG)),
        AudioDecoderAdapterCode::DECODER_OK);

    // Without setting callback, OnError and OnOutputFormatChanged should not crash
    AudioDecoderCallbackManager::OnError(decoder->GetAVCodec(), 0, nullptr);
    AudioDecoderCallbackManager::OnOutputFormatChanged(decoder->GetAVCodec(), 0, nullptr);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_ReleaseOutputBufferDec_NoDecoder_027.
 * @tc.desc: test ReleaseOutputBufferDec with various error paths
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_ReleaseOutputBufferDec_NoDecoder_027)
{
    // No decoder
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ReleaseOutputBufferDec(0),
        AudioDecoderAdapterCode::DECODER_ERROR);

    // Create decoder
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);

    // No output buffer at given index
    AudioCodecDecoderAdapterImpl_->outputBuffers_.clear();
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ReleaseOutputBufferDec(0),
        AudioDecoderAdapterCode::DECODER_ERROR);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_ConfigureDecoder_NullFormat_028.
 * @tc.desc: test ConfigureDecoder/SetParameterDecoder when decoder is not null but format is null
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_ConfigureDecoder_NullFormat_028)
{
    // Create decoder first
    std::string mimetype = std::string(OH_AVCODEC_MIMETYPE_AUDIO_MPEG);
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->CreateAudioDecoderByMime(mimetype), AudioDecoderAdapterCode::DECODER_OK);

    // null format should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->ConfigureDecoder(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);

    // null format for SetParameterDecoder should also fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetParameterDecoder(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_AudioMimeType_029.
 * @tc.desc: test AudioMimeType enum values
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_AudioMimeType_029)
{
    EXPECT_EQ(static_cast<int32_t>(AudioMimeType::MIMETYPE_UNKNOW), 0);
    EXPECT_EQ(static_cast<int32_t>(AudioMimeType::MIMETYPE_AUDIO_AAC), 1);
    EXPECT_EQ(static_cast<int32_t>(AudioMimeType::MIMETYPE_AUDIO_FLAC), 2);
    EXPECT_EQ(static_cast<int32_t>(AudioMimeType::MIMETYPE_AUDIO_VORBIS), 3);
    EXPECT_EQ(static_cast<int32_t>(AudioMimeType::MIMETYPE_AUDIO_MPEG), 4);
    EXPECT_EQ(static_cast<int32_t>(AudioMimeType::MIMETYPE_AUDIO_AMR_NB), 5);
    EXPECT_EQ(static_cast<int32_t>(AudioMimeType::MIMETYPE_AUDIO_AMR_WB), 6);
    EXPECT_EQ(static_cast<int32_t>(AudioMimeType::MIMETYPE_AUDIO_G711MU), 7);
    EXPECT_EQ(static_cast<int32_t>(AudioMimeType::MIMETYPE_AUDIO_APE), 8);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_SetCallbackDec_NullDecoder_030.
 * @tc.desc: test SetCallbackDec with null decoder
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_SetCallbackDec_NullDecoder_030)
{
    // null callback should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetCallbackDec(nullptr), AudioDecoderAdapterCode::DECODER_ERROR);

    std::shared_ptr<AudioDecoderCallbackAdapter> callback = std::make_shared<AudioDecoderCallbackAdapterMock>();
    // No decoder created yet, should fail
    EXPECT_EQ(AudioCodecDecoderAdapterImpl_->SetCallbackDec(callback), AudioDecoderAdapterCode::DECODER_ERROR);
}

/**
 * @tc.name: AudioCodecDecoderAdapterImpl_GetDecoderMutex_031.
 * @tc.desc: test GetDecoderMutex returns a valid mutex reference
 * @tc.type: FUNC.
 */
TEST_F(AudioCodecDecoderAdapterImplTest, AudioCodecDecoderAdapterImpl_GetDecoderMutex_031)
{
    std::mutex& mtx = AudioCodecDecoderAdapterImpl::GetDecoderMutex();
    mtx.lock();
    mtx.unlock();
    {
        std::unique_lock<std::mutex> lock(mtx);
    }
}
} // namespace NWeb
} // namespace OHOS