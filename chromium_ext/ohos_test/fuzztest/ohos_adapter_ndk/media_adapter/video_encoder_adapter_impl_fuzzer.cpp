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

#include "arkweb/ohos_adapter_ndk/media_adapter/include/video_encoder_adapter_impl.h"

#include <cstring>
#include <unordered_map>
#include <vector>
#include <string>
#include <fuzzer/FuzzedDataProvider.h>
#include <multimedia/player_framework/native_avbuffer.h>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ohos_adapter_helper.h"

using namespace OHOS::NWeb;

namespace OHOS {
constexpr uint8_t MAX_STRING_LENGTH = 255;
const std::vector<OH_AVCodecBufferFlags> CODEC_BUFFER_FLAG_VECTOR = {
    OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_NONE,
    OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_EOS,
    OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_SYNC_FRAME,
    OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_INCOMPLETE_FRAME,
    OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_CODEC_DATA
};

class CodecCallbackAdapterMock : public CodecCallbackAdapter {
public:
    CodecCallbackAdapterMock() = default;

    ~CodecCallbackAdapterMock() override = default;

    void OnError(ErrorType errorType, int32_t errorCode) override {}

    void OnStreamChanged(const std::shared_ptr<CodecFormatAdapter> format) override {}

    void OnNeedInputData(uint32_t index, std::shared_ptr<OhosBufferAdapter> buffer) override {}

    void OnNeedOutputData(uint32_t index, std::shared_ptr<BufferInfoAdapter> info, BufferFlag flag,
        std::shared_ptr<OhosBufferAdapter> buffer) override {}
};

class CodecConfigParaAdapterMock : public CodecConfigParaAdapter {
public:
    CodecConfigParaAdapterMock() = default;

    ~CodecConfigParaAdapterMock() override = default;

    int32_t GetWidth() override
    {
        return width_;
    }

    int32_t GetHeight() override
    {
        return height_;
    }

    int64_t GetBitRate() override
    {
        return bitRate_;
    }

    double GetFrameRate() override
    {
        return frameRate_;
    }

    void SetWidth(int32_t width)
    {
        width_ = width;
    }

    void SetHeight(int32_t height)
    {
        height_ = height;
    }

    void SetBitRate(int64_t bitRate)
    {
        bitRate_ = bitRate;
    }

    void SetFrameRate(double frameRate)
    {
        frameRate_ = frameRate;
    }

    int32_t width_ = 0;
    int32_t height_ = 0;
    int64_t bitRate_ = 0;
    double frameRate_ = 0.0;
};

std::unique_ptr<OHOS::NWeb::MediaCodecAdapter> CreateMediaCodecEncoderAdapterMock()
{
    return OHOS::NWeb::OhosAdapterHelper::GetInstance().CreateMediaCodecEncoderAdapter();
}

OH_AVBuffer* CreateTestBuffer(int32_t capacity, OH_AVCodecBufferFlags flag)
{
    OH_AVBuffer* buffer = OH_AVBuffer_Create(capacity);
    if (buffer != nullptr) {
        OH_AVCodecBufferAttr attr;
        attr.pts = 12345;
        attr.size = capacity / 2;
        attr.offset = 0;
        attr.flags = flag;
        OH_AVBuffer_SetBufferAttr(buffer, &attr);
    }
    return buffer;
}

bool VideoEncoderAdapterImplFuzzTest(FuzzedDataProvider* fdp)
{
    // Test path with potentially valid encoder creation
    {
        VideoEncoderAdapterImpl videoEncoderAdapterImpl;
        std::shared_ptr<CodecCallbackAdapterMock> callbackImpl =
            std::make_shared<CodecCallbackAdapterMock>();
        std::shared_ptr<CodecConfigParaAdapterMock> config =
            std::make_shared<CodecConfigParaAdapterMock>();

        // First try with random string (likely fails)
        std::string mimeParam = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
        videoEncoderAdapterImpl.CreateVideoCodecByMime(mimeParam);
        std::string name = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
        videoEncoderAdapterImpl.CreateVideoCodecByName(name);

        // Test all null/invalid paths before valid encoder is created
        videoEncoderAdapterImpl.SetCodecCallback(nullptr);
        videoEncoderAdapterImpl.SetCodecCallback(callbackImpl);
        videoEncoderAdapterImpl.Configure(nullptr);
        videoEncoderAdapterImpl.Configure(config);
        videoEncoderAdapterImpl.Prepare();
        videoEncoderAdapterImpl.Start();
        videoEncoderAdapterImpl.Stop();
        videoEncoderAdapterImpl.Reset();
        videoEncoderAdapterImpl.Release();
        videoEncoderAdapterImpl.CreateInputSurface();
        videoEncoderAdapterImpl.ReleaseOutputBuffer(0, true);
        videoEncoderAdapterImpl.RequestKeyFrameSoon();

        // Try to create a valid encoder
        auto ret = videoEncoderAdapterImpl.CreateVideoCodecByMime("video/avc");
        if (ret == CodecCodeAdapter::OK) {
            // Test SetCodecCallback with null callback path
            videoEncoderAdapterImpl.SetCodecCallback(nullptr);
            // Test valid callback
            videoEncoderAdapterImpl.SetCodecCallback(callbackImpl);
            // Test SetCodecCallback again to trigger callback_index_ > 0 branch
            videoEncoderAdapterImpl.SetCodecCallback(callbackImpl);

            // Configure with fuzzed values
            config->SetWidth(fdp->ConsumeIntegralInRange<int32_t>(1, 3840));
            config->SetHeight(fdp->ConsumeIntegralInRange<int32_t>(1, 2160));
            config->SetBitRate(fdp->ConsumeIntegralInRange<int64_t>(1, 10000000));
            config->SetFrameRate(static_cast<double>(fdp->ConsumeIntegralInRange<int32_t>(1, 60)));

            videoEncoderAdapterImpl.Configure(nullptr);
            videoEncoderAdapterImpl.Configure(config);

            videoEncoderAdapterImpl.Prepare();
            videoEncoderAdapterImpl.Start();
            videoEncoderAdapterImpl.CreateInputSurface();
            videoEncoderAdapterImpl.RequestKeyFrameSoon();
            videoEncoderAdapterImpl.ReleaseOutputBuffer(
                fdp->ConsumeIntegralInRange<uint32_t>(0, 100), true);
            videoEncoderAdapterImpl.Stop();
            videoEncoderAdapterImpl.Reset();
            // Intentionally do NOT call Release() here to cover destructor path
        }
    } // destructor may call Release() and callback_wrapper_.Clear()

    // Test GetBufferFlag with all valid flags
    for (const auto& flag : CODEC_BUFFER_FLAG_VECTOR) {
        VideoEncoderAdapterImpl::GetBufferFlag(flag);
    }
    // Test GetBufferFlag with invalid flag
    VideoEncoderAdapterImpl::GetBufferFlag(static_cast<OH_AVCodecBufferFlags>(9999));

    // Test EncoderCallbackImpl with valid callback
    std::shared_ptr<CodecCallbackAdapterMock> cb = std::make_shared<CodecCallbackAdapterMock>();
    NWeb::EncoderCallbackImpl encoderCallback(cb);
    int32_t errorCode = fdp->ConsumeIntegralInRange<int32_t>(0, 1);
    encoderCallback.OnError(errorCode);

    OH_AVFormat* format = OH_AVFormat_Create();
    encoderCallback.OnOutputFormatChanged(format);
    OH_AVFormat_Destroy(format);

    // Test OnInputBufferAvailable with valid buffer
    int32_t capacity = fdp->ConsumeIntegralInRange<int32_t>(1024, 8192);
    int32_t bufferIndex = fdp->ConsumeIntegralInRange<int32_t>(1, 100);
    OH_AVBuffer* buffer = CreateTestBuffer(capacity, OH_AVCodecBufferFlags::AVCODEC_BUFFER_FLAGS_NONE);
    if (buffer != nullptr) {
        encoderCallback.OnInputBufferAvailable(bufferIndex, buffer);
        OH_AVBuffer_Destroy(buffer);
    }

    // Test OnOutputBufferAvailable with different flags
    for (const auto& flag : CODEC_BUFFER_FLAG_VECTOR) {
        OH_AVBuffer* outBuffer = CreateTestBuffer(capacity, flag);
        if (outBuffer != nullptr) {
            encoderCallback.OnOutputBufferAvailable(bufferIndex, outBuffer);
            OH_AVBuffer_Destroy(outBuffer);
        }
    }

    return true;
}

bool VideoEncoderAdapterImplNullFuzzTest(FuzzedDataProvider* fdp)
{
    (void)fdp;
    VideoEncoderAdapterImpl videoEncoderAdapterImpl;
    std::shared_ptr<CodecCallbackAdapterMock> callbackImpl =
        std::make_shared<CodecCallbackAdapterMock>();
    std::shared_ptr<CodecConfigParaAdapterMock> config =
        std::make_shared<CodecConfigParaAdapterMock>();

    // Create with invalid mime to ensure encoder_ remains nullptr
    videoEncoderAdapterImpl.CreateVideoCodecByMime("invalid/mime");
    videoEncoderAdapterImpl.CreateVideoCodecByName("invalid_name");

    // All these should hit encoder_ == nullptr or param == nullptr paths
    videoEncoderAdapterImpl.SetCodecCallback(nullptr);
    videoEncoderAdapterImpl.SetCodecCallback(callbackImpl);
    videoEncoderAdapterImpl.Configure(nullptr);
    videoEncoderAdapterImpl.Configure(config);
    videoEncoderAdapterImpl.Prepare();
    videoEncoderAdapterImpl.Start();
    videoEncoderAdapterImpl.Stop();
    videoEncoderAdapterImpl.Reset();
    videoEncoderAdapterImpl.Release();
    videoEncoderAdapterImpl.CreateInputSurface();
    videoEncoderAdapterImpl.ReleaseOutputBuffer(0, true);
    videoEncoderAdapterImpl.ReleaseOutputBuffer(0, false);
    videoEncoderAdapterImpl.RequestKeyFrameSoon();

    // Test static callbacks with nullptr userData
    VideoEncoderAdapterImpl::OnError(nullptr, 0, nullptr);
    VideoEncoderAdapterImpl::OnStreamChanged(nullptr, nullptr, nullptr);
    VideoEncoderAdapterImpl::OnNeedInputBuffer(nullptr, 0, nullptr, nullptr);
    VideoEncoderAdapterImpl::OnNewOutputBuffer(nullptr, 0, nullptr, nullptr);

    // Test static callbacks with invalid userData (no matching callback in wrapper)
    void* invalidUserData = reinterpret_cast<void*>(999999);
    VideoEncoderAdapterImpl::OnError(nullptr, 0, invalidUserData);
    VideoEncoderAdapterImpl::OnStreamChanged(nullptr, nullptr, invalidUserData);
    VideoEncoderAdapterImpl::OnNeedInputBuffer(nullptr, 0, nullptr, invalidUserData);
    VideoEncoderAdapterImpl::OnNewOutputBuffer(nullptr, 0, nullptr, invalidUserData);

    // Test EncoderCallbackImpl with nullptr callback adapter
    NWeb::EncoderCallbackImpl nullCallback(nullptr);
    nullCallback.OnError(0);
    nullCallback.OnOutputFormatChanged(nullptr);
    nullCallback.OnInputBufferAvailable(0, nullptr);
    nullCallback.OnOutputBufferAvailable(0, nullptr);

    // Test with buffer but cb_ is null
    OH_AVBuffer* buffer = OH_AVBuffer_Create(1024);
    if (buffer != nullptr) {
        nullCallback.OnInputBufferAvailable(0, buffer);
        nullCallback.OnOutputBufferAvailable(0, buffer);
        OH_AVBuffer_Destroy(buffer);
    }

    return true;
}

bool VideoEncoderAdapterImplDestructorFuzzTest(const uint8_t* data, size_t size)
{
    (void)data;
    (void)size;
    // Test destructor path where encoder_ != nullptr
    {
        VideoEncoderAdapterImpl videoEncoderAdapterImpl;
        videoEncoderAdapterImpl.CreateVideoCodecByMime("video/avc");
        // destructor should call Release() if encoder_ != nullptr
    }
    return true;
}

bool VideoAvcEncoderAdapterImplFuzzTest(FuzzedDataProvider* fdp)
{
    auto videoEncoderAdapter = CreateMediaCodecEncoderAdapterMock();
    if (!videoEncoderAdapter) {
        return false;
    }

    std::shared_ptr<CodecCallbackAdapterMock> callbackImpl =
        std::make_shared<CodecCallbackAdapterMock>();
    std::shared_ptr<CodecConfigParaAdapterMock> config =
        std::make_shared<CodecConfigParaAdapterMock>();

    // Configure with fuzzed values before creation
    config->SetWidth(fdp->ConsumeIntegralInRange<int32_t>(1, 3840));
    config->SetHeight(fdp->ConsumeIntegralInRange<int32_t>(1, 2160));
    config->SetBitRate(fdp->ConsumeIntegralInRange<int64_t>(1, 10000000));
    config->SetFrameRate(static_cast<double>(fdp->ConsumeIntegralInRange<int32_t>(1, 60)));

    videoEncoderAdapter->CreateVideoCodecByMime("video/avc");
    videoEncoderAdapter->SetCodecCallback(callbackImpl);
    videoEncoderAdapter->Configure(config);
    videoEncoderAdapter->Prepare();
    videoEncoderAdapter->Start();
    videoEncoderAdapter->Stop();
    videoEncoderAdapter->Reset();
    videoEncoderAdapter->Release();
    videoEncoderAdapter->CreateInputSurface();
    videoEncoderAdapter->ReleaseOutputBuffer(0, true);
    videoEncoderAdapter->RequestKeyFrameSoon();

    std::shared_ptr<CodecCallbackAdapterMock> cb = std::make_shared<CodecCallbackAdapterMock>();
    NWeb::EncoderCallbackImpl encoderCallback(cb);
    int32_t errorCode = fdp->ConsumeIntegralInRange<int32_t>(0, 1);
    encoderCallback.OnError(errorCode);

    OH_AVFormat* format = OH_AVFormat_Create();
    encoderCallback.OnOutputFormatChanged(format);
    OH_AVFormat_Destroy(format);

    int32_t capacity = fdp->ConsumeIntegralInRange<int32_t>(1, 8);
    int32_t bufferIndex = fdp->ConsumeIntegralInRange<int32_t>(1, 100);
    OH_AVBuffer* buffer = OH_AVBuffer_Create(capacity);
    encoderCallback.OnInputBufferAvailable(bufferIndex, buffer);
    encoderCallback.OnOutputBufferAvailable(bufferIndex, buffer);
    OH_AVBuffer_Destroy(buffer);

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
    OHOS::VideoEncoderAdapterImplFuzzTest(&fdp);
    OHOS::VideoEncoderAdapterImplNullFuzzTest(&fdp);
    OHOS::VideoEncoderAdapterImplDestructorFuzzTest(data, size);
    OHOS::VideoAvcEncoderAdapterImplFuzzTest(&fdp);

    return 0;
}
