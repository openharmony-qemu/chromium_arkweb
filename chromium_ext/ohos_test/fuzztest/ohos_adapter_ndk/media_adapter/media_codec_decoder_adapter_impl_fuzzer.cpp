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

#include "arkweb/ohos_adapter_ndk/media_adapter/media_codec_decoder_adapter_impl.h"

#include <cstring>
#include <fuzzer/FuzzedDataProvider.h>
#include <multimedia/player_framework/native_avbuffer.h>

using namespace OHOS::NWeb;

namespace OHOS {
constexpr uint8_t MAX_STRING_LENGTH = 255;

class DecoderCallbackAdapterMock : public DecoderCallbackAdapter {
public:
    DecoderCallbackAdapterMock() = default;

    ~DecoderCallbackAdapterMock() override = default;

    void OnError(ErrorType errorType, int32_t errorCode) override {}

    void OnStreamChanged(int32_t width, int32_t height, double frameRate) override {}

    void OnNeedInputData(uint32_t index, std::shared_ptr<OhosBufferAdapter> buffer) override {}

    void OnNeedOutputData(uint32_t index, std::shared_ptr<BufferInfoAdapter> info, BufferFlag flag) override {}
};

class DecoderFormatAdapterMock : public DecoderFormatAdapter {
public:
    DecoderFormatAdapterMock()
    {
        width = 1920;
        height = 1080;
        frameRate = 30.0;
    }

    int32_t GetWidth() override
    {
        return width;
    }

    int32_t GetHeight() override
    {
        return height;
    }

    double GetFrameRate() override
    {
        return frameRate;
    }

    void SetWidth(int32_t w) override
    {
        width = w;
    }

    void SetHeight(int32_t h) override
    {
        height = h;
    }

    void SetFrameRate(double fr) override
    {
        frameRate = fr;
    }

    int32_t width;
    int32_t height;
    double frameRate;
};

class AudioCencInfoAdapterMock : public AudioCencInfoAdapter {
public:
    AudioCencInfoAdapterMock() = default;
    ~AudioCencInfoAdapterMock() override = default;

    uint8_t* GetKeyId() override { return keyId_; }

    uint32_t GetKeyIdLen() override { return keyIdLen_; }

    uint8_t* GetIv() override { return iv_; }

    uint32_t GetIvLen() override { return ivLen_; }

    uint32_t GetEncryptedBlockCount() override { return encryptedBlockCount_; }

    uint32_t GetAlgo() override { return algo_; }

    uint32_t GetSkippedBlockCount() override { return skippedBlockCount_; }

    uint32_t GetFirstEncryptedOffset() override { return firstEncryptedOffset_; }

    std::vector<uint32_t> GetClearHeaderLens() override { return clearHeaderLens_; }

    std::vector<uint32_t> GetPayLoadLens() override { return payLoadLens_; }

    uint32_t GetMode() override { return mode_; }

    void SetKeyId(uint8_t* keyId) override { keyId_ = keyId; }

    void SetKeyIdLen(uint32_t keyIdLen) override { keyIdLen_ = keyIdLen; }

    void SetIv(uint8_t* iv) override { iv_ = iv; }

    void SetIvLen(uint32_t ivLen) override { ivLen_ = ivLen; }

    void SetAlgo(uint32_t algo) override { algo_ = algo; }

    void SetEncryptedBlockCount(uint32_t encryptedBlockCount) override { encryptedBlockCount_ = encryptedBlockCount; }

    void SetSkippedBlockCount(uint32_t skippedBlockCount) override { skippedBlockCount_ = skippedBlockCount; }

    void SetFirstEncryptedOffset(uint32_t firstEncryptedOffset) override { firstEncryptedOffset_ = firstEncryptedOffset; }

    void SetClearHeaderLens(const std::vector<uint32_t>& clearHeaderLens) override { clearHeaderLens_ = clearHeaderLens; }

    void SetPayLoadLens(const std::vector<uint32_t>& payLoadLens) override { payLoadLens_ = payLoadLens; }

    void SetMode(uint32_t mode) override { mode_ = mode; }

private:
    uint8_t* keyId_ = nullptr;
    uint32_t keyIdLen_ = 0;
    uint8_t* iv_ = nullptr;
    uint32_t ivLen_ = 0;
    uint32_t algo_ = 0;
    uint32_t encryptedBlockCount_ = 0;
    uint32_t skippedBlockCount_ = 0;
    uint32_t firstEncryptedOffset_ = 0;
    std::vector<uint32_t> clearHeaderLens_;
    std::vector<uint32_t> payLoadLens_;
    uint32_t mode_ = 0;
};

bool MediaCodecDecoderAdapterImplFuzzTest(const uint8_t* data, size_t size)
{
    NWeb::MediaCodecDecoderAdapterImpl mediaCodecDecoderAdapterImpl;
    std::shared_ptr<NWeb::DecoderFormatAdapter> format = std::make_unique<DecoderFormatAdapterMock>();
    FuzzedDataProvider dataProvider(data, size);
    std::string stringParam = dataProvider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    mediaCodecDecoderAdapterImpl.CreateVideoDecoderByName(stringParam);
    NWeb::DecoderAdapterCode code = mediaCodecDecoderAdapterImpl.CreateVideoDecoderByMime("video/avc");

    int32_t intParam = dataProvider.ConsumeIntegralInRange<int32_t>(0, 10000);
    uint32_t uintParam = dataProvider.ConsumeIntegralInRange<uint32_t>(0, 10000);
    auto callback = std::make_shared<DecoderCallbackAdapterMock>();
    mediaCodecDecoderAdapterImpl.SetCallbackDec(nullptr);
    mediaCodecDecoderAdapterImpl.SetCallbackDec(callback);
    code = mediaCodecDecoderAdapterImpl.ConfigureDecoder(format);
    code = mediaCodecDecoderAdapterImpl.SetParameterDecoder(format);

    // Test ConfigureDecoder with potentially unsupported size
    format->SetWidth(99999);
    format->SetHeight(99999);
    code = mediaCodecDecoderAdapterImpl.ConfigureDecoder(format);
    format->SetWidth(1920);
    format->SetHeight(1080);

    void* window = nullptr;
    code = mediaCodecDecoderAdapterImpl.SetOutputSurface(window);
    code = mediaCodecDecoderAdapterImpl.PrepareDecoder();
    code = mediaCodecDecoderAdapterImpl.StartDecoder();

    constexpr int32_t MEMSIZE = 1024 * 1024;
    OH_AVFormat* codecFormat = OH_AVFormat_Create();
    OH_AVBuffer* buffer = OH_AVBuffer_Create(MEMSIZE);

    // Set default buffer attr for output buffer tests
    OH_AVCodecBufferAttr defaultAttr;
    defaultAttr.pts = 0;
    defaultAttr.size = 1024;
    defaultAttr.offset = 0;
    defaultAttr.flags = OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_NONE;
    OH_AVBuffer_SetBufferAttr(buffer, &defaultAttr);

    // Test OnInputBufferAvailable to add buffer to bufferMap_
    uint32_t inputIndex = uintParam;
    mediaCodecDecoderAdapterImpl.OnInputBufferAvailable(inputIndex, buffer);

    // Now QueueInputBufferDec should find the buffer and proceed
    code = mediaCodecDecoderAdapterImpl.QueueInputBufferDec(
        inputIndex, 0, intParam, intParam, BufferFlag::CODEC_BUFFER_FLAG_NONE);
    code = mediaCodecDecoderAdapterImpl.QueueInputBufferDec(
        inputIndex, 0, intParam, intParam, BufferFlag::CODEC_BUFFER_FLAG_EOS);
    code = mediaCodecDecoderAdapterImpl.QueueInputBufferDec(
        inputIndex, 0, intParam, intParam, BufferFlag::CODEC_BUFFER_FLAG_SYNC_FRAME);
    code = mediaCodecDecoderAdapterImpl.QueueInputBufferDec(
        inputIndex, 0, intParam, intParam, BufferFlag::CODEC_BUFFER_FLAG_PARTIAL_FRAME);
    code = mediaCodecDecoderAdapterImpl.QueueInputBufferDec(
        inputIndex, 0, intParam, intParam, BufferFlag::CODEC_BUFFER_FLAG_CODEC_DATA);

    // Test ReleaseOutputBufferDec with both true and false
    code = mediaCodecDecoderAdapterImpl.ReleaseOutputBufferDec(uintParam, true);
    code = mediaCodecDecoderAdapterImpl.ReleaseOutputBufferDec(uintParam, false);

    // Test OnOutputBufferAvailable with different buffer flags
    OH_AVCodecBufferAttr attr;
    attr.pts = 0;
    attr.size = 1024;
    attr.offset = 0;

    attr.flags = OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_NONE;
    OH_AVBuffer_SetBufferAttr(buffer, &attr);
    mediaCodecDecoderAdapterImpl.OnOutputBufferAvailable(uintParam, buffer);

    attr.flags = OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_EOS;
    OH_AVBuffer_SetBufferAttr(buffer, &attr);
    mediaCodecDecoderAdapterImpl.OnOutputBufferAvailable(uintParam, buffer);

    attr.flags = OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_SYNC_FRAME;
    OH_AVBuffer_SetBufferAttr(buffer, &attr);
    mediaCodecDecoderAdapterImpl.OnOutputBufferAvailable(uintParam, buffer);

    attr.flags = OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_INCOMPLETE_FRAME;
    OH_AVBuffer_SetBufferAttr(buffer, &attr);
    mediaCodecDecoderAdapterImpl.OnOutputBufferAvailable(uintParam, buffer);

    attr.flags = OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_CODEC_DATA;
    OH_AVBuffer_SetBufferAttr(buffer, &attr);
    mediaCodecDecoderAdapterImpl.OnOutputBufferAvailable(uintParam, buffer);

    // Test callback methods directly
    mediaCodecDecoderAdapterImpl.OnError(uintParam);
    mediaCodecDecoderAdapterImpl.OnOutputFormatChanged(nullptr);
    mediaCodecDecoderAdapterImpl.OnOutputFormatChanged(codecFormat);
    mediaCodecDecoderAdapterImpl.OnInputBufferAvailable(uintParam, nullptr);
    mediaCodecDecoderAdapterImpl.OnOutputBufferAvailable(uintParam, nullptr);

    // Test GetOutputFormatDec
    code = mediaCodecDecoderAdapterImpl.GetOutputFormatDec(format);
    code = mediaCodecDecoderAdapterImpl.GetOutputFormatDec(nullptr);

    // Test SetAVCencInfo with valid cencInfo
    auto cencInfo = std::make_shared<AudioCencInfoAdapterMock>();
    uint8_t keyId[16] = {0};
    uint8_t iv[16] = {0};
    cencInfo->SetKeyId(keyId);
    cencInfo->SetKeyIdLen(16);
    cencInfo->SetIv(iv);
    cencInfo->SetIvLen(16);
    cencInfo->SetAlgo(1); // DRM_ALG_CENC_AES_CTR
    cencInfo->SetEncryptedBlockCount(1);
    cencInfo->SetSkippedBlockCount(0);
    cencInfo->SetFirstEncryptedOffset(0);
    cencInfo->SetClearHeaderLens({16});
    cencInfo->SetPayLoadLens({1024});
    cencInfo->SetMode(0); // DRM_CENC_INFO_KEY_IV_SUBSAMPLES_SET

    // Add another buffer entry for SetAVCencInfo
    uint32_t cencIndex = uintParam + 1;
    mediaCodecDecoderAdapterImpl.OnInputBufferAvailable(cencIndex, buffer);
    code = mediaCodecDecoderAdapterImpl.SetAVCencInfo(cencIndex, cencInfo);

    // Test GetBufferFlag and GetAVBufferFlag with all valid and invalid values
    MediaCodecDecoderAdapterImpl::GetBufferFlag(OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_NONE);
    MediaCodecDecoderAdapterImpl::GetBufferFlag(OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_EOS);
    MediaCodecDecoderAdapterImpl::GetBufferFlag(OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_SYNC_FRAME);
    MediaCodecDecoderAdapterImpl::GetBufferFlag(OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_INCOMPLETE_FRAME);
    MediaCodecDecoderAdapterImpl::GetBufferFlag(OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_CODEC_DATA);
    MediaCodecDecoderAdapterImpl::GetBufferFlag(static_cast<OH_AVCodecBufferFlags>(9999));

    MediaCodecDecoderAdapterImpl::GetAVBufferFlag(BufferFlag::CODEC_BUFFER_FLAG_NONE);
    MediaCodecDecoderAdapterImpl::GetAVBufferFlag(BufferFlag::CODEC_BUFFER_FLAG_EOS);
    MediaCodecDecoderAdapterImpl::GetAVBufferFlag(BufferFlag::CODEC_BUFFER_FLAG_SYNC_FRAME);
    MediaCodecDecoderAdapterImpl::GetAVBufferFlag(BufferFlag::CODEC_BUFFER_FLAG_PARTIAL_FRAME);
    MediaCodecDecoderAdapterImpl::GetAVBufferFlag(BufferFlag::CODEC_BUFFER_FLAG_CODEC_DATA);
    MediaCodecDecoderAdapterImpl::GetAVBufferFlag(static_cast<BufferFlag>(9999));

    // Test VideoDecoderCallbackManager directly
    OH_AVCodec* decoder = mediaCodecDecoderAdapterImpl.GetAVDecoder();
    VideoDecoderCallbackManager::OnError(nullptr, 0, nullptr);
    VideoDecoderCallbackManager::OnStreamChanged(nullptr, codecFormat, nullptr);
    VideoDecoderCallbackManager::OnNeedInputBuffer(nullptr, uintParam, buffer, nullptr);
    VideoDecoderCallbackManager::OnNewOutputBuffer(nullptr, uintParam, buffer, nullptr);

    if (decoder != nullptr) {
        VideoDecoderCallbackManager::OnError(decoder, 0, nullptr);
        VideoDecoderCallbackManager::OnStreamChanged(decoder, codecFormat, nullptr);
        VideoDecoderCallbackManager::OnNeedInputBuffer(decoder, uintParam, buffer, nullptr);
        VideoDecoderCallbackManager::OnNewOutputBuffer(decoder, uintParam, buffer, nullptr);

        VideoDecoderCallbackManager::FindVideoDecoder(nullptr);
        VideoDecoderCallbackManager::FindVideoDecoder(decoder);
    }

    // Test VideoDecoderCallbackManager::AddVideoDecoder edge cases
    VideoDecoderCallbackManager::AddVideoDecoder(nullptr);
    {
        NWeb::MediaCodecDecoderAdapterImpl tempImpl;
        VideoDecoderCallbackManager::AddVideoDecoder(&tempImpl); // decoder_ is nullptr, returns early
    }

    // Test SetDecryptionConfig - sets isSecure_ when decoder_ is valid
    code = mediaCodecDecoderAdapterImpl.SetDecryptionConfig(nullptr, true);
    code = mediaCodecDecoderAdapterImpl.SetDecryptionConfig(nullptr, false);

    // Stop, flush, reset
    code = mediaCodecDecoderAdapterImpl.StopDecoder();
    code = mediaCodecDecoderAdapterImpl.FlushDecoder();
    code = mediaCodecDecoderAdapterImpl.ResetDecoder();

    // Release decoder
    code = mediaCodecDecoderAdapterImpl.ReleaseDecoder();

    OH_AVFormat_Destroy(codecFormat);
    OH_AVBuffer_Destroy(buffer);
    codecFormat = nullptr;
    buffer = nullptr;
    return true;
}

bool MediaCodecDecoderAdapterImplNullFuzzTest(const uint8_t* data, size_t size)
{
    NWeb::MediaCodecDecoderAdapterImpl mediaCodecDecoderAdapterImpl;
    NWeb::DecoderAdapterCode code = mediaCodecDecoderAdapterImpl.CreateVideoDecoderByMime("test");
    std::shared_ptr<NWeb::DecoderFormatAdapter> format = std::make_unique<DecoderFormatAdapterMock>();
    FuzzedDataProvider dataProvider(data, size);
    std::string stringParam = dataProvider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    int32_t intParam = dataProvider.ConsumeIntegralInRange<int32_t>(0, 10000);
    uint32_t uintParam = dataProvider.ConsumeIntegralInRange<uint32_t>(0, 10000);

    mediaCodecDecoderAdapterImpl.SetCallbackDec(nullptr);
    code = mediaCodecDecoderAdapterImpl.CreateVideoDecoderByName(stringParam);
    code = mediaCodecDecoderAdapterImpl.ConfigureDecoder(format);
    code = mediaCodecDecoderAdapterImpl.ConfigureDecoder(nullptr);
    code = mediaCodecDecoderAdapterImpl.SetParameterDecoder(format);
    code = mediaCodecDecoderAdapterImpl.SetParameterDecoder(nullptr);

    void* window = nullptr;
    code = mediaCodecDecoderAdapterImpl.SetOutputSurface(window);
    code = mediaCodecDecoderAdapterImpl.PrepareDecoder();
    code = mediaCodecDecoderAdapterImpl.StartDecoder();
    code = mediaCodecDecoderAdapterImpl.StopDecoder();
    code = mediaCodecDecoderAdapterImpl.FlushDecoder();
    code = mediaCodecDecoderAdapterImpl.ResetDecoder();
    code = mediaCodecDecoderAdapterImpl.ReleaseDecoder();
    code = mediaCodecDecoderAdapterImpl.SetAVCencInfo(uintParam, nullptr);
    code = mediaCodecDecoderAdapterImpl.SetDecryptionConfig(nullptr, true);
    code = mediaCodecDecoderAdapterImpl.SetDecryptionConfig(nullptr, false);

    code = mediaCodecDecoderAdapterImpl.QueueInputBufferDec(
        uintParam, 0, intParam, intParam, BufferFlag::CODEC_BUFFER_FLAG_NONE);
    code = mediaCodecDecoderAdapterImpl.GetOutputFormatDec(format);
    code = mediaCodecDecoderAdapterImpl.GetOutputFormatDec(nullptr);
    code = mediaCodecDecoderAdapterImpl.ReleaseOutputBufferDec(uintParam, true);
    code = mediaCodecDecoderAdapterImpl.ReleaseOutputBufferDec(uintParam, false);

    constexpr int32_t MEMSIZE = 1024 * 1024;
    OH_AVFormat* codecFormat = OH_AVFormat_Create();
    OH_AVBuffer* buffer = OH_AVBuffer_Create(MEMSIZE);
    mediaCodecDecoderAdapterImpl.OnError(uintParam);
    mediaCodecDecoderAdapterImpl.OnOutputFormatChanged(nullptr);
    mediaCodecDecoderAdapterImpl.OnInputBufferAvailable(uintParam, nullptr);
    mediaCodecDecoderAdapterImpl.OnOutputBufferAvailable(uintParam, nullptr);
    mediaCodecDecoderAdapterImpl.OnInputBufferAvailable(uintParam, buffer);
    mediaCodecDecoderAdapterImpl.OnOutputBufferAvailable(uintParam, buffer);

    // Test static methods with invalid inputs when no decoder is registered
    MediaCodecDecoderAdapterImpl::GetBufferFlag(OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_NONE);
    MediaCodecDecoderAdapterImpl::GetAVBufferFlag(BufferFlag::CODEC_BUFFER_FLAG_NONE);
    VideoDecoderCallbackManager::FindVideoDecoder(nullptr);
    VideoDecoderCallbackManager::DeleteVideoDecoder(nullptr);
    VideoDecoderCallbackManager::AddVideoDecoder(nullptr);

    OH_AVFormat_Destroy(codecFormat);
    OH_AVBuffer_Destroy(buffer);
    codecFormat = nullptr;
    buffer = nullptr;
    return true;
}

bool MediaCodecDecoderAdapterImplDestructorFuzzTest(const uint8_t* data, size_t size)
{
    (void)data;
    (void)size;
    {
        NWeb::MediaCodecDecoderAdapterImpl mediaCodecDecoderAdapterImpl;
        mediaCodecDecoderAdapterImpl.CreateVideoDecoderByMime("video/avc");
        // destructor will call ReleaseDecoder if decoder_ != nullptr
    }
    return true;
}

bool MediaCodecDecoderAdapterImplSecureFuzzTest(const uint8_t* data, size_t size)
{
    (void)data;
    (void)size;
    NWeb::MediaCodecDecoderAdapterImpl mediaCodecDecoderAdapterImpl;
    NWeb::DecoderAdapterCode code = mediaCodecDecoderAdapterImpl.CreateVideoDecoderByMime("video/avc");
    if (code == DecoderAdapterCode::DECODER_OK) {
        // Set isSecure_ = true via SetDecryptionConfig with nullptr session
        code = mediaCodecDecoderAdapterImpl.SetDecryptionConfig(nullptr, true);
        code = mediaCodecDecoderAdapterImpl.ReleaseDecoder();
        // Recreate decoder with isSecure_ = true to test secure paths
        code = mediaCodecDecoderAdapterImpl.CreateVideoDecoderByMime("video/avc");
        code = mediaCodecDecoderAdapterImpl.ReleaseDecoder();
    }
    return true;
}

} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::MediaCodecDecoderAdapterImplFuzzTest(data, size);
    OHOS::MediaCodecDecoderAdapterImplNullFuzzTest(data, size);
    OHOS::MediaCodecDecoderAdapterImplDestructorFuzzTest(data, size);
    OHOS::MediaCodecDecoderAdapterImplSecureFuzzTest(data, size);
    return 0;
}
