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

#include <cstdint>
#include <cstring>
#include <memory>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "arkweb/ohos_nweb/src/nweb_hilog.h"
#include <database/udmf/udmf_err_code.h>
#include <database/udmf/udmf_meta.h>
#include <database/pasteboard/oh_pasteboard_err_code.h>
#include "ohos_sdk/openharmony/native/sysroot/usr/include/AbilityKit/ability_runtime/ability_runtime_common.h"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api_new.h"
#include "third_party/bounds_checking_function/include/securec.h"
#define private public
#include "arkweb/ohos_adapter_ndk/pasteboard_adapter/include/pasteboard_client_adapter_impl.h"
#undef private

using namespace testing;
using testing::_;
using testing::Return;
using namespace MockNdkApi;

namespace OHOS::NWeb {
const int RESULT_OK = 0;
const std::string g_test_str = "testhtml";
const std::string g_empty_uri = "";
const std::string g_test_uri = "/data/test/path";
std::shared_ptr<PasteDataRecordAdapterImpl> g_paster;
std::shared_ptr<PasteDataRecordAdapterImpl> g_datarecord;
std::shared_ptr<PasteDataRecordAdapterImpl> g_datarecord_null;
std::shared_ptr<PasteDataAdapterImpl> g_dataadapter;
std::shared_ptr<PasteDataAdapterImpl> g_dataadapter_null;
std::shared_ptr<std::string> g_empty_text;
std::shared_ptr<std::string> g_test_text;
PasteCustomData g_empty_custom_data;
PasteCustomData g_test_custom_data;
PasteCustomData g_fake_custom_data;

class PasteboardClientAdapterImplTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void PasteboardClientAdapterImplTest::SetUpTestCase() {
    g_empty_text = std::make_shared<std::string>("");
    g_test_text = std::make_shared<std::string>("test");
    std::string format = "ohos/custom-data";
    std::vector<uint8_t> data = { 0, 1, 2 };
    g_test_custom_data.insert(std::make_pair(format, data));

    std::string fake_format = "format";
    std::vector<uint8_t> fake_data = { 0, 1, 2 };
    g_fake_custom_data.insert(std::make_pair(fake_format, fake_data));

    g_datarecord_null = std::make_shared<PasteDataRecordAdapterImpl>(nullptr, std::shared_ptr<OH_UdmfData>());
    ASSERT_NE(g_datarecord_null, nullptr);
    EXPECT_EQ(g_datarecord_null->record_, nullptr);

    OH_UdmfRecord* record = OH_UdmfRecord_Create();
    EXPECT_NE(record, nullptr);
    g_datarecord = std::make_shared<PasteDataRecordAdapterImpl>(record, std::shared_ptr<OH_UdmfData>());
    ASSERT_NE(g_datarecord, nullptr);
    

    OH_UdmfData* udmfData = OH_UdmfData_Create();
    g_dataadapter = std::make_shared<PasteDataAdapterImpl>(udmfData);
    ASSERT_NE(g_dataadapter, nullptr);

    g_dataadapter_null = std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(g_dataadapter_null, nullptr);
}

void PasteboardClientAdapterImplTest::TearDownTestCase() {}

void PasteboardClientAdapterImplTest::SetUp() {}

void PasteboardClientAdapterImplTest::TearDown() {}

class MockClipBoardImageDataAdapter : public ClipBoardImageDataAdapter {
public:
    MockClipBoardImageDataAdapter() = default;

    ClipBoardImageColorType GetColorType() override
    {
        return colorType;
    }

    ClipBoardImageAlphaType GetAlphaType() override
    {
        return alphaType;
    }

    uint32_t* GetData() override
    {
        return data;
    }

    size_t GetDataSize() override
    {
        return dataSize;
    }

    size_t GetRowBytes() override
    {
        return rowBytes;
    }

    int32_t GetWidth() override
    {
        return width;
    }

    int32_t GetHeight() override
    {
        return height;
    }

    void SetColorType(ClipBoardImageColorType color) override
    {
        colorType = color;
    }

    void SetAlphaType(ClipBoardImageAlphaType alpha) override
    {
        alphaType = alpha;
    }

    void SetData(uint32_t* d) override
    {
        data = d;
    }

    void SetDataSize(size_t size) override
    {
        dataSize = size;
    }

    void SetRowBytes(size_t r) override
    {
        rowBytes = r;
    }

    void SetWidth(int32_t w) override
    {
        width = w;
    }

    void SetHeight(int32_t h) override
    {
        height = h;
    }

    ClipBoardImageColorType colorType =
        ClipBoardImageColorType::COLOR_TYPE_UNKNOWN;
    ClipBoardImageAlphaType alphaType =
        ClipBoardImageAlphaType::ALPHA_TYPE_UNKNOWN;
    uint32_t* data = nullptr;
    size_t dataSize = 0;
    size_t rowBytes = 0;
    int32_t width = 0;
    int32_t height = 0;
};

// The producer must publish packed RGBA/BGRA rows whatever the source row
// stride is, and must not publish a synthetic image when the pixel read
// fails.
class PasteboardImageReadTest : public testing::Test {
   public:
    void SetUp() override {
        record_ = std::make_shared<PasteDataRecordAdapterImpl>(
            OH_UdmfRecord_Create(), std::shared_ptr<OH_UdmfData>());
        ASSERT_NE(record_->GetRecord(), nullptr);
        auto input = std::make_shared<MockClipBoardImageDataAdapter>();
        input->SetWidth(3);
        input->SetHeight(3);
        input->SetRowBytes(12);
        input->SetData(pixels_);
        input->SetDataSize(sizeof(pixels_));
        input->SetColorType(ClipBoardImageColorType::COLOR_TYPE_BGRA_8888);
        input->SetAlphaType(ClipBoardImageAlphaType::ALPHA_TYPE_OPAQUE);
        ASSERT_TRUE(record_->SetImgData(input));
        output_ = std::make_shared<MockClipBoardImageDataAdapter>();

        SET_MOCK_KEY_TRUE(MockOHOSFunction, OH_PixelmapImageInfo_GetWidth);
        SET_MOCK_KEY_TRUE(MockOHOSFunction, OH_PixelmapImageInfo_GetHeight);
        SET_MOCK_KEY_TRUE(MockOHOSFunction, OH_PixelmapImageInfo_GetRowStride);
        SET_MOCK_KEY_TRUE(MockOHOSFunction, OH_PixelmapImageInfo_GetAlphaType);
        SET_MOCK_KEY_TRUE(MockOHOSFunction,
                          OH_PixelmapImageInfo_GetPixelFormat);
        SET_MOCK_KEY_TRUE(MockOHOSFunction, OH_PixelmapNative_ReadPixels);
        auto& mock = MockOHOSFunction::GetInstance();
        ON_CALL(mock, OH_PixelmapImageInfo_GetWidth(_, _))
            .WillByDefault(DoAll(SetArgPointee<1>(3u), Return(IMAGE_SUCCESS)));
        ON_CALL(mock, OH_PixelmapImageInfo_GetHeight(_, _))
            .WillByDefault(DoAll(SetArgPointee<1>(3u), Return(IMAGE_SUCCESS)));
        ON_CALL(mock, OH_PixelmapImageInfo_GetRowStride(_, _))
            .WillByDefault(DoAll(SetArgPointee<1>(12u), Return(IMAGE_SUCCESS)));
        ON_CALL(mock, OH_PixelmapImageInfo_GetAlphaType(_, _))
            .WillByDefault(DoAll(SetArgPointee<1>(PIXELMAP_ALPHA_TYPE_OPAQUE),
                                 Return(IMAGE_SUCCESS)));
        ON_CALL(mock, OH_PixelmapImageInfo_GetPixelFormat(_, _))
            .WillByDefault(DoAll(SetArgPointee<1>(PIXEL_FORMAT_BGRA_8888),
                                 Return(IMAGE_SUCCESS)));
        ON_CALL(mock, OH_PixelmapNative_ReadPixels(_, _, _))
            .WillByDefault(
                [this](OH_PixelmapNative*, uint8_t* destination, size_t* size) {
                  EXPECT_GE(*size, sizeof(pixels_));
                  if (*size < sizeof(pixels_)) {
                    return IMAGE_BAD_PARAMETER;
                  }
                  // ReadPixels packs rows, including for padded sources.
                  EXPECT_EQ(memcpy_s(destination, *size, pixels_, sizeof(pixels_)), EOK);
                  return IMAGE_SUCCESS;
                });
    }

    void TearDown() override {
        SET_MOCK_KEY_FALSE(MockOHOSFunction, OH_PixelmapImageInfo_GetWidth);
        SET_MOCK_KEY_FALSE(MockOHOSFunction, OH_PixelmapImageInfo_GetHeight);
        SET_MOCK_KEY_FALSE(MockOHOSFunction, OH_PixelmapImageInfo_GetRowStride);
        SET_MOCK_KEY_FALSE(MockOHOSFunction, OH_PixelmapImageInfo_GetAlphaType);
        SET_MOCK_KEY_FALSE(MockOHOSFunction,
                           OH_PixelmapImageInfo_GetPixelFormat);
        SET_MOCK_KEY_FALSE(MockOHOSFunction, OH_PixelmapNative_ReadPixels);
        testing::Mock::VerifyAndClear(&MockOHOSFunction::GetInstance());
        if (output_) {
          free(output_->GetData());
          output_->SetData(nullptr);
        }
    }

    void ExpectReadFailure() {
        EXPECT_FALSE(record_->GetImgData(output_));
        EXPECT_EQ(output_->GetData(), nullptr);
        EXPECT_EQ(output_->GetDataSize(), 0u);
        EXPECT_EQ(output_->GetRowBytes(), 0u);
    }

    void ExpectPackedImage() {
        ASSERT_TRUE(record_->GetImgData(output_));
        ASSERT_NE(output_->GetData(), nullptr);
        EXPECT_EQ(output_->GetWidth(), 3);
        EXPECT_EQ(output_->GetHeight(), 3);
        EXPECT_EQ(output_->GetRowBytes(), 12u);
        ASSERT_EQ(output_->GetDataSize(), sizeof(pixels_));
        EXPECT_EQ(memcmp(output_->GetData(), pixels_, sizeof(pixels_)), 0);
    }

   protected:
    uint32_t pixels_[9] = {
        0xff010203, 0xff040506, 0xff070809, 0xff101112, 0xff131415,
        0xff161718, 0xff202122, 0xff232425, 0xff262728,
    };
    std::shared_ptr<PasteDataRecordAdapterImpl> record_;
    std::shared_ptr<MockClipBoardImageDataAdapter> output_;
};

TEST_F(PasteboardImageReadTest, PackedPixels) {
    ExpectPackedImage();
    EXPECT_EQ(output_->GetColorType(),
              ClipBoardImageColorType::COLOR_TYPE_BGRA_8888);
    EXPECT_EQ(output_->GetAlphaType(),
              ClipBoardImageAlphaType::ALPHA_TYPE_OPAQUE);
}

TEST_F(PasteboardImageReadTest, PaddedSourceStillPublishesPackedPixels) {
    auto& mock = MockOHOSFunction::GetInstance();
    EXPECT_CALL(mock, OH_PixelmapImageInfo_GetRowStride(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(16u), Return(IMAGE_SUCCESS)));
    EXPECT_CALL(mock, OH_PixelmapNative_ReadPixels(_, _, _))
        .WillOnce(
            [this](OH_PixelmapNative*, uint8_t* destination, size_t* size) {
              EXPECT_EQ(*size, 48u);
              if (*size < 48u) {
                return IMAGE_BAD_PARAMETER;
              }
              EXPECT_EQ(memcpy_s(destination, *size, pixels_, sizeof(pixels_)), EOK);
              return IMAGE_SUCCESS;
            });
    ExpectPackedImage();
}

TEST_F(PasteboardImageReadTest, SmallerStrideStillPublishesPackedPixels) {
    auto& mock = MockOHOSFunction::GetInstance();
    EXPECT_CALL(mock, OH_PixelmapImageInfo_GetRowStride(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(8u), Return(IMAGE_SUCCESS)));
    ExpectPackedImage();
}

TEST_F(PasteboardImageReadTest, ReadPixelsFailureDoesNotPublishData) {
    EXPECT_CALL(MockOHOSFunction::GetInstance(),
                OH_PixelmapNative_ReadPixels(_, _, _))
        .WillOnce(Return(IMAGE_BAD_PARAMETER));
    ExpectReadFailure();
}

TEST_F(PasteboardImageReadTest, InvalidLayoutRejectedBeforeReading) {
    const struct {
        uint32_t width;
        uint32_t height;
        uint32_t stride;
    } layouts[] = {
        {0, 3, 12},             {3, 0, 12},          {0x40000001, 4, 4},
        {65536, 16384, 262144}, {3, UINT32_MAX, 12}, {3, 3, UINT32_MAX},
    };
    auto& mock = MockOHOSFunction::GetInstance();
    EXPECT_CALL(mock, OH_PixelmapNative_ReadPixels(_, _, _)).Times(0);
    for (const auto& layout : layouts) {
        SCOPED_TRACE(testing::Message() << layout.width << "x" << layout.height
                                        << ", stride=" << layout.stride);
        ON_CALL(mock, OH_PixelmapImageInfo_GetWidth(_, _))
            .WillByDefault(
                DoAll(SetArgPointee<1>(layout.width), Return(IMAGE_SUCCESS)));
        ON_CALL(mock, OH_PixelmapImageInfo_GetHeight(_, _))
            .WillByDefault(
                DoAll(SetArgPointee<1>(layout.height), Return(IMAGE_SUCCESS)));
        ON_CALL(mock, OH_PixelmapImageInfo_GetRowStride(_, _))
            .WillByDefault(
                DoAll(SetArgPointee<1>(layout.stride), Return(IMAGE_SUCCESS)));
        ExpectReadFailure();
    }
}

TEST_F(PasteboardImageReadTest, RgbaFormatAndUnknownAlphaRemainSupported) {
    auto& mock = MockOHOSFunction::GetInstance();
    EXPECT_CALL(mock, OH_PixelmapImageInfo_GetPixelFormat(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(PIXEL_FORMAT_RGBA_8888),
                        Return(IMAGE_SUCCESS)));
    EXPECT_CALL(mock, OH_PixelmapImageInfo_GetAlphaType(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(PIXELMAP_ALPHA_TYPE_UNKNOWN),
                        Return(IMAGE_SUCCESS)));
    ExpectPackedImage();
    EXPECT_EQ(output_->GetColorType(),
              ClipBoardImageColorType::COLOR_TYPE_RGBA_8888);
    EXPECT_EQ(output_->GetAlphaType(),
              ClipBoardImageAlphaType::ALPHA_TYPE_PREMULTIPLIED);
}

TEST_F(PasteboardImageReadTest, GetWidthFailure) {
    auto& mock = MockOHOSFunction::GetInstance();
    EXPECT_CALL(mock, OH_PixelmapImageInfo_GetWidth(_, _))
        .WillOnce(Return(IMAGE_BAD_PARAMETER));
    EXPECT_CALL(mock, OH_PixelmapNative_ReadPixels(_, _, _)).Times(0);
    ExpectReadFailure();
}

TEST_F(PasteboardImageReadTest, GetHeightFailure) {
    auto& mock = MockOHOSFunction::GetInstance();
    EXPECT_CALL(mock, OH_PixelmapImageInfo_GetHeight(_, _))
        .WillOnce(Return(IMAGE_BAD_PARAMETER));
    EXPECT_CALL(mock, OH_PixelmapNative_ReadPixels(_, _, _)).Times(0);
    ExpectReadFailure();
}

class MockPasteboardObserver : public PasteboardObserverAdapter {
public:
    MockPasteboardObserver() = default;
    void OnPasteboardChanged() override {}
};

void SetMockState(bool status) {
    MockOHOSFunction::pasteboard_GetDataParams_Create = status;
    MockOHOSFunction::abilityRuntime_ApplicationContextGetCacheDir = status;
    MockOHOSFunction::pasteboard_GetDataParams_SetProgressIndicator = status;
    MockOHOSFunction::pasteboard_GetDataParams_SetDestUri = status;
    MockOHOSFunction::pasteboard_GetDataParams_SetFileConflictOptions = status;
    MockOHOSFunction::pasteboard_GetDataWithProgress = status;
    MockOHOSFunction::pasteboard_HasData = status;
    MockOHOSFunction::pasteboard_GetDataParams_Destroy = status;
    MockOHOSFunction::udmfData_GetRecords = status;
    MockOHOSFunction::udmfData_IsLocal = status;
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetHtmlText)
{
    std::shared_ptr<std::string> html_text = g_datarecord->GetHtmlText();
    EXPECT_EQ(html_text, nullptr);

    std::shared_ptr<std::string> plain_text = g_datarecord->GetPlainText();
    EXPECT_EQ(plain_text, nullptr);

    EXPECT_EQ(g_datarecord->GetMimeType(), "");

    EXPECT_EQ(g_datarecord->SetHtmlText(g_test_text), true);
    EXPECT_NE(g_datarecord->GetMimeType(), "");

    auto mimeTypes = g_datarecord->GetMimeTypes();
    bool isEmpty = mimeTypes.empty();
    EXPECT_EQ(isEmpty, false);

    html_text = g_datarecord->GetHtmlText();
    EXPECT_NE(html_text, nullptr);

    plain_text = g_datarecord->GetPlainText();
    EXPECT_NE(plain_text, nullptr);

    // record is nullptr
    EXPECT_EQ(g_datarecord_null->GetPlainText(), nullptr);
    EXPECT_EQ(g_datarecord_null->SetHtmlText(g_test_text), false);
    EXPECT_EQ(g_datarecord_null->GetMimeType(), "");
    mimeTypes = g_datarecord_null->GetMimeTypes();
    isEmpty = mimeTypes.empty();
    EXPECT_EQ(isEmpty, true);
    html_text = g_datarecord_null->GetHtmlText();
    EXPECT_EQ(html_text, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetPlainText)
{
    EXPECT_EQ(g_datarecord->SetPlainText(g_test_text), true);
    std::shared_ptr<std::string> plain_text = g_datarecord->GetPlainText();
    EXPECT_NE(plain_text, nullptr);


    EXPECT_EQ(g_datarecord_null->SetPlainText(g_test_text), false);
    plain_text = g_datarecord->GetPlainText();
    EXPECT_NE(plain_text, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetUri)
{
    std::shared_ptr<std::string> uri = g_datarecord->GetUri();
    EXPECT_EQ(uri, nullptr);
    EXPECT_EQ(g_datarecord->SetUri(g_empty_uri), false);
    EXPECT_EQ(g_datarecord->SetUri(g_test_uri), true);
    uri = g_datarecord->GetUri();
    EXPECT_NE(uri, nullptr);

    EXPECT_EQ(g_datarecord_null->SetUri(g_test_uri), false);
    uri = g_datarecord_null->GetUri();
    EXPECT_EQ(uri, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetCustomData)
{
    EXPECT_EQ(g_datarecord->SetCustomData(g_empty_custom_data), false);

    std::shared_ptr<PasteCustomData> custom_data = g_datarecord->GetCustomData();
    EXPECT_EQ(custom_data, nullptr);

    EXPECT_EQ(g_datarecord->SetCustomData(g_fake_custom_data), true);
    custom_data = g_datarecord->GetCustomData();
    EXPECT_EQ(custom_data, nullptr);

    EXPECT_EQ(g_datarecord->SetCustomData(g_test_custom_data), true);
    custom_data = g_datarecord->GetCustomData();
    EXPECT_NE(custom_data, nullptr);

    EXPECT_EQ(g_datarecord_null->SetCustomData(g_test_custom_data), false);
    custom_data = g_datarecord_null->GetCustomData();
    EXPECT_EQ(custom_data, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetImgData)
{
    uint32_t storage[][5] = {
        { 0xCA, 0xDA, 0xCA, 0xC9, 0xA3 },
        { 0xAC, 0xA8, 0x89, 0x47, 0x87 },
        { 0x4B, 0x25, 0x25, 0x25, 0x46 },
        { 0x90, 0x1, 0x25, 0x41, 0x33 },
        { 0x75, 0x55, 0x44, 0x20, 0x00 },
    };

    std::shared_ptr<MockClipBoardImageDataAdapter> image = std::make_shared<MockClipBoardImageDataAdapter>();
    image->SetColorType(ClipBoardImageColorType::COLOR_TYPE_BGRA_8888);
    image->SetAlphaType(ClipBoardImageAlphaType::ALPHA_TYPE_UNKNOWN);
    image->SetData(storage[0]);
    image->SetDataSize(sizeof(storage));
    image->SetRowBytes(5);
    image->SetWidth(5);
    image->SetHeight(5);

    EXPECT_EQ(g_datarecord->SetImgData(image), true);

    EXPECT_EQ(g_datarecord->GetImgData(nullptr), false);
    std::shared_ptr<MockClipBoardImageDataAdapter> getted_image_data =
        std::make_shared<MockClipBoardImageDataAdapter>();
    EXPECT_EQ(g_datarecord->GetImgData(getted_image_data), true);

    EXPECT_EQ(g_datarecord->SetImgData(nullptr), false);

    image->SetData(nullptr);
    EXPECT_EQ(g_datarecord->SetImgData(image), false);
    image->SetData(storage[0]);
    image->SetDataSize(sizeof(storage));
    EXPECT_EQ(g_datarecord_null->SetImgData(image), false);
    EXPECT_EQ(g_datarecord_null->GetImgData(nullptr), false);
}

TEST_F(PasteboardClientAdapterImplTest, CheckTypeTranslate)
{
    EXPECT_EQ(g_datarecord->ClipBoardToPixelColorType(ClipBoardImageColorType::COLOR_TYPE_RGBA_8888),
        PIXEL_FORMAT::PIXEL_FORMAT_RGBA_8888);
    EXPECT_EQ(g_datarecord->ClipBoardToPixelColorType(ClipBoardImageColorType::COLOR_TYPE_BGRA_8888),
        PIXEL_FORMAT::PIXEL_FORMAT_BGRA_8888);
    EXPECT_EQ(g_datarecord->ClipBoardToPixelColorType(ClipBoardImageColorType::COLOR_TYPE_UNKNOWN),
        PIXEL_FORMAT::PIXEL_FORMAT_UNKNOWN);
    
    EXPECT_EQ(g_datarecord->PixelToClipBoardColorType(PIXEL_FORMAT::PIXEL_FORMAT_RGBA_8888),
        ClipBoardImageColorType::COLOR_TYPE_RGBA_8888);
    EXPECT_EQ(g_datarecord->PixelToClipBoardColorType(PIXEL_FORMAT::PIXEL_FORMAT_BGRA_8888),
        ClipBoardImageColorType::COLOR_TYPE_BGRA_8888);
    EXPECT_EQ(g_datarecord->PixelToClipBoardColorType(PIXEL_FORMAT::PIXEL_FORMAT_UNKNOWN),
        ClipBoardImageColorType::COLOR_TYPE_UNKNOWN);

    EXPECT_EQ(g_datarecord->ClipboardToPixelAlphaType(ClipBoardImageAlphaType::ALPHA_TYPE_UNKNOWN),
        PIXELMAP_ALPHA_TYPE::PIXELMAP_ALPHA_TYPE_UNKNOWN);
    EXPECT_EQ(g_datarecord->ClipboardToPixelAlphaType(ClipBoardImageAlphaType::ALPHA_TYPE_OPAQUE),
        PIXELMAP_ALPHA_TYPE::PIXELMAP_ALPHA_TYPE_OPAQUE);
    EXPECT_EQ(g_datarecord->ClipboardToPixelAlphaType(ClipBoardImageAlphaType::ALPHA_TYPE_PREMULTIPLIED),
        PIXELMAP_ALPHA_TYPE::PIXELMAP_ALPHA_TYPE_PREMULTIPLIED);
    EXPECT_EQ(g_datarecord->ClipboardToPixelAlphaType(ClipBoardImageAlphaType::ALPHA_TYPE_POSTMULTIPLIED),
        PIXELMAP_ALPHA_TYPE::PIXELMAP_ALPHA_TYPE_UNPREMULTIPLIED);
    
    EXPECT_EQ(g_datarecord->PixelToClipboardAlphaType(PIXELMAP_ALPHA_TYPE::PIXELMAP_ALPHA_TYPE_UNKNOWN),
        ClipBoardImageAlphaType::ALPHA_TYPE_UNKNOWN);
    EXPECT_EQ(g_datarecord->PixelToClipboardAlphaType(PIXELMAP_ALPHA_TYPE::PIXELMAP_ALPHA_TYPE_OPAQUE),
        ClipBoardImageAlphaType::ALPHA_TYPE_OPAQUE);
    EXPECT_EQ(g_datarecord->PixelToClipboardAlphaType(PIXELMAP_ALPHA_TYPE::PIXELMAP_ALPHA_TYPE_PREMULTIPLIED),
        ClipBoardImageAlphaType::ALPHA_TYPE_PREMULTIPLIED);
    EXPECT_EQ(g_datarecord->PixelToClipboardAlphaType(PIXELMAP_ALPHA_TYPE::PIXELMAP_ALPHA_TYPE_UNPREMULTIPLIED),
        ClipBoardImageAlphaType::ALPHA_TYPE_POSTMULTIPLIED);
}

TEST_F(PasteboardClientAdapterImplTest, ClearImgBuffer)
{
    g_datarecord->Clear();
    EXPECT_EQ(g_datarecord->imgBuffer_, nullptr);

    g_datarecord->bufferSize_ = 100;
    g_datarecord->imgBuffer_ = static_cast<uint8_t *>(calloc(g_datarecord->bufferSize_, sizeof(uint8_t)));
    EXPECT_NE(g_datarecord->imgBuffer_, nullptr);

    g_datarecord->Clear();
    EXPECT_EQ(g_datarecord->imgBuffer_, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, AddAndGetRecord)
{
    // get paste data while data is empty
    std::vector<std::string> mimeTypes = g_dataadapter->GetMimeTypes();
    bool isEmpty = mimeTypes.empty();
    EXPECT_EQ(isEmpty, true);
    std::shared_ptr<std::string> primaryHtml = g_dataadapter->GetPrimaryHtml();
    EXPECT_EQ(primaryHtml, nullptr);
    std::shared_ptr<std::string> primaryText = g_dataadapter->GetPrimaryText();
    EXPECT_EQ(primaryText, nullptr);
    std::shared_ptr<std::string> primaryMimeType = g_dataadapter->GetPrimaryMimeType();
    EXPECT_EQ(primaryMimeType, nullptr);
    std::shared_ptr<PasteDataRecordAdapter> record = g_dataadapter->GetRecordAt(0);
    EXPECT_EQ(record, nullptr);
    std::size_t count = g_dataadapter->GetRecordCount();
    EXPECT_EQ(count, 0);
    PasteRecordVector recordVector = g_dataadapter_null->AllRecords();
    isEmpty = recordVector.empty();
    EXPECT_EQ(isEmpty, true);

    // set and get paste data
    g_dataadapter->AddHtmlRecord(g_test_str);
    mimeTypes = g_dataadapter->GetMimeTypes();
    isEmpty = mimeTypes.empty();
    EXPECT_EQ(isEmpty, false);

    g_dataadapter_null->AddHtmlRecord(g_test_str);
    mimeTypes = g_dataadapter_null->GetMimeTypes();
    isEmpty = mimeTypes.empty();
    EXPECT_EQ(isEmpty, true);

    g_dataadapter->AddTextRecord(g_test_str);
    mimeTypes = g_dataadapter->GetMimeTypes();
    isEmpty = mimeTypes.empty();
    EXPECT_EQ(isEmpty, false);

    g_dataadapter_null->AddTextRecord(g_test_str);

    primaryHtml = g_dataadapter->GetPrimaryHtml();
    EXPECT_NE(primaryHtml, nullptr);
    primaryHtml = g_dataadapter_null->GetPrimaryHtml();
    EXPECT_EQ(primaryHtml, nullptr);

    primaryText = g_dataadapter->GetPrimaryText();
    EXPECT_NE(primaryText, nullptr);
    primaryText = g_dataadapter_null->GetPrimaryText();
    EXPECT_EQ(primaryText, nullptr);

    std::shared_ptr<std::string> primary = g_dataadapter_null->GetPrimaryMimeType();
    EXPECT_EQ(primary, nullptr);

    std::shared_ptr<PasteDataRecordAdapter> record_null = g_dataadapter_null->GetRecordAt(0);
    EXPECT_EQ(record_null, nullptr);
    count = g_dataadapter_null->GetRecordCount();
    EXPECT_EQ(count, 0);

    PasteRecordVector recordVector_null = g_dataadapter_null->AllRecords();
    isEmpty = recordVector_null.empty();
    EXPECT_EQ(isEmpty, true);
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetPasteData)
{
    PasteRecordVector data;
    PasteBoardClientAdapterImpl::GetInstance().Clear();
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);
    std::shared_ptr<PasteDataRecordAdapter> record = PasteDataRecordAdapter::NewRecord("text/html");
    EXPECT_NE(record, nullptr);
    std::shared_ptr<std::string> pasteData = std::make_shared<std::string>("test");
    EXPECT_NE(pasteData, nullptr);
    record->SetHtmlText(pasteData);
    data.push_back(record);
    PasteBoardClientAdapterImpl::GetInstance().SetPasteData(data);

    PasteBoardClientAdapterImpl::GetInstance().Clear();
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);

    PasteBoardClientAdapterImpl::GetInstance().SetPasteData(data, CopyOptionMode::NONE);
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);

    std::shared_ptr<PasteboardObserverAdapter> observer = std::make_shared<MockPasteboardObserver>();
    int32_t id = PasteBoardClientAdapterImpl::GetInstance().AddPasteboardChangedObserver(observer);
    EXPECT_EQ(id, 0);
    PasteBoardClientAdapterImpl::GetInstance().RemovePasteboardChangedObserver(id);
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().reg_.size(), 0);

    id = PasteBoardClientAdapterImpl::GetInstance().AddPasteboardChangedObserver(nullptr);
    EXPECT_EQ(id, -1);
    PasteBoardClientAdapterImpl::GetInstance().RemovePasteboardChangedObserver(-1);
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().reg_.size(), 0);
}

TEST_F(PasteboardClientAdapterImplTest, CheckTransitionCopyOption)
{
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().TransitionCopyOption(CopyOptionMode::IN_APP),
        Udmf_ShareOption::SHARE_OPTIONS_IN_APP);
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().TransitionCopyOption(CopyOptionMode::LOCAL_DEVICE),
        Udmf_ShareOption::SHARE_OPTIONS_CROSS_APP);
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().TransitionCopyOption(CopyOptionMode::CROSS_DEVICE),
        Udmf_ShareOption::SHARE_OPTIONS_CROSS_APP);
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().TransitionCopyOption(CopyOptionMode::NONE),
        Udmf_ShareOption::SHARE_OPTIONS_INVALID);
}

TEST_F(PasteboardClientAdapterImplTest, PasteDataNull)
{
    PasteRecordVector data;
    PasteBoardClientAdapterImpl::GetInstance().pasteboard_ = nullptr;
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data),
        false);
    std::shared_ptr<PasteDataRecordAdapter> record = PasteDataRecordAdapter::NewRecord("text/html");
    EXPECT_NE(record, nullptr);
    std::shared_ptr<std::string> pasteData = std::make_shared<std::string>("test");
    EXPECT_NE(pasteData, nullptr);
    record->SetHtmlText(pasteData);
    data.push_back(record);
    PasteBoardClientAdapterImpl::GetInstance().SetPasteData(data);
    PasteBoardClientAdapterImpl::GetInstance().Clear();
    std::shared_ptr<PasteboardObserverAdapter> observer = std::make_shared<MockPasteboardObserver>();
    int32_t id = PasteBoardClientAdapterImpl::GetInstance().AddPasteboardChangedObserver(observer);
    EXPECT_NE(id, -1);

    PasteBoardClientAdapterImpl::GetInstance().pasteboard_ = OH_Pasteboard_Create();
    int32_t newId = PasteBoardClientAdapterImpl::GetInstance().AddPasteboardChangedObserver(observer);
    EXPECT_EQ(id, newId - 1);
    PasteBoardClientAdapterImpl::GetInstance().pasteboard_ = nullptr;
    PasteBoardClientAdapterImpl::GetInstance().RemovePasteboardChangedObserver(id);
    PasteBoardClientAdapterImpl::GetInstance().RemovePasteboardChangedObserver(newId);
}

TEST_F(PasteboardClientAdapterImplTest, ReleaseMemory)
{
    g_datarecord->DestoryOptions(0, nullptr, "options is nullptr");
    g_datarecord->DestoryPixelmapNative(nullptr);
    g_datarecord->ReleaseMemory(nullptr, nullptr, nullptr, nullptr, "test all input is nullptr", 0);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText001)
{
    std::string html = "";
    std::string expected = "";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
    html = "aa";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText002)
{
    std::string html = "a<div";
    std::string expected = "";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText003)
{
    std::string html = "<p class=\"content\">aa</p>";
    std::string expected = "aa";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText004)
{
    std::string html = "<h1>aa</h1>";
    std::string expected = "aa";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText005)
{
    std::string html = "<div>aa</div><ul>bb</ul>";
    std::string expected = "aa\r\nbb";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText006)
{
    std::string html = "<p>aa";
    std::string expected = "aa";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText007)
{
    std::string html = "<span>aa</span>bb<span>cc</span>";
    std::string expected = "aabbcc";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText008)
{
    std::string html = "aa<h2>bb</h2><p>cc</p>dd";
    std::string expected = "bb\r\nccdd";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, AllRecords001)
{
    PasteRecordVector recordVector = g_dataadapter->AllRecords();
    bool isEmpty = recordVector.empty();
    EXPECT_EQ(isEmpty, true);
}

TEST_F(PasteboardClientAdapterImplTest, GetPasteDataTest001)
{
    PasteBoardClientAdapterImpl::GetInstance();
    SetMockState(true);
    PasteRecordVector data;
    auto& mock = MockOHOSFunction::GetInstance();
    Pasteboard_GetDataParams* params = reinterpret_cast<Pasteboard_GetDataParams*>(0x1);
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Create()).WillRepeatedly(Return(params));
    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_PARAM_INVALID));
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);

    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_NO_ERROR));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetProgressIndicator(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetDestUri(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetFileConflictOptions(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataWithProgress(_, _, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock, OH_Pasteboard_HasData(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Destroy(_)).WillRepeatedly(Return());
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);

    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Create()).WillRepeatedly(Return(params));
    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_NO_ERROR));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetProgressIndicator(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetDestUri(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetFileConflictOptions(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataWithProgress(_, _, _)).WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<2>(-1),testing::Return(nullptr)));
    EXPECT_CALL(mock, OH_Pasteboard_HasData(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Destroy(_)).WillRepeatedly(Return());
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);
    SetMockState(false);
}

TEST_F(PasteboardClientAdapterImplTest, GetPasteDataTest002)
{
    PasteBoardClientAdapterImpl::GetInstance();
    SetMockState(true);
    PasteRecordVector data;
    auto& mock = MockOHOSFunction::GetInstance();
    Pasteboard_GetDataParams* params = reinterpret_cast<Pasteboard_GetDataParams*>(0x1);
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Create()).WillRepeatedly(Return(params));
    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_NO_ERROR));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetProgressIndicator(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetDestUri(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetFileConflictOptions(_, _)).WillRepeatedly(Return());

    EXPECT_CALL(mock, OH_Pasteboard_GetDataWithProgress(_, _, _)).WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<2>(ERR_OK),testing::Return(nullptr)));
    EXPECT_CALL(mock, OH_Pasteboard_HasData(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, OH_UdmfData_GetRecords(_, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Destroy(_)).WillRepeatedly(Return());
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);

    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Create()).WillRepeatedly(Return(params));
    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_NO_ERROR));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetProgressIndicator(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetDestUri(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetFileConflictOptions(_, _)).WillRepeatedly(Return());

    EXPECT_CALL(mock, OH_Pasteboard_GetDataWithProgress(_, _, _)).WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<2>(ERR_OK),testing::Return(nullptr)));
    EXPECT_CALL(mock, OH_Pasteboard_HasData(_)).WillRepeatedly(Return(true));

    OH_UdmfRecord* records = reinterpret_cast<OH_UdmfRecord *>(0x1);
    EXPECT_CALL(mock, OH_UdmfData_GetRecords(_, _)).WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<1>(0),testing::Return(&records)));
    EXPECT_CALL(mock, OH_UdmfData_IsLocal(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Destroy(_)).WillRepeatedly(Return());
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), true);
    SetMockState(false);
}

TEST_F(PasteboardClientAdapterImplTest, GetPasteDataTest003)
{
    PasteBoardClientAdapterImpl::GetInstance();
    SetMockState(true);
    PasteRecordVector data;
    auto& mock = MockOHOSFunction::GetInstance();
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Create()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_PARAM_INVALID));
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);

    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_NO_ERROR));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetProgressIndicator(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetDestUri(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetFileConflictOptions(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataWithProgress(_, _, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock, OH_Pasteboard_HasData(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Destroy(_)).WillRepeatedly(Return());
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);

    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Create()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_NO_ERROR));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetProgressIndicator(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetDestUri(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetFileConflictOptions(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataWithProgress(_, _, _)).WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<2>(-1),testing::Return(nullptr)));
    EXPECT_CALL(mock, OH_Pasteboard_HasData(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Destroy(_)).WillRepeatedly(Return());
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);
    SetMockState(false);
}

TEST_F(PasteboardClientAdapterImplTest, GetPasteDataTest004)
{
    PasteBoardClientAdapterImpl::GetInstance();
    SetMockState(true);
    PasteRecordVector data;
    auto& mock = MockOHOSFunction::GetInstance();
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Create()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_NO_ERROR));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetProgressIndicator(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetDestUri(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetFileConflictOptions(_, _)).WillRepeatedly(Return());

    EXPECT_CALL(mock, OH_Pasteboard_GetDataWithProgress(_, _, _)).WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<2>(ERR_OK),testing::Return(nullptr)));
    EXPECT_CALL(mock, OH_Pasteboard_HasData(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, OH_UdmfData_GetRecords(_, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Destroy(_)).WillRepeatedly(Return());
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), false);

    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Create()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock, OH_AbilityRuntime_ApplicationContextGetCacheDir(_, _,
        _)).WillRepeatedly(Return(ABILITY_RUNTIME_ERROR_CODE_NO_ERROR));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetProgressIndicator(_, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetDestUri(_, _, _)).WillRepeatedly(Return());
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_SetFileConflictOptions(_, _)).WillRepeatedly(Return());

    EXPECT_CALL(mock, OH_Pasteboard_GetDataWithProgress(_, _, _)).WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<2>(ERR_OK),testing::Return(nullptr)));
    EXPECT_CALL(mock, OH_Pasteboard_HasData(_)).WillRepeatedly(Return(true));

    OH_UdmfRecord* records = reinterpret_cast<OH_UdmfRecord *>(0x1);
    EXPECT_CALL(mock, OH_UdmfData_GetRecords(_, _)).WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<1>(0),testing::Return(&records)));
    EXPECT_CALL(mock, OH_UdmfData_IsLocal(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Destroy(_)).WillRepeatedly(Return());
    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data), true);
    SetMockState(false);
}

TEST_F(PasteboardClientAdapterImplTest,
       CRITICAL_SetUri_NullptrFromOH_UdsFileUri_Create) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/html");
    ASSERT_NE(record, nullptr);

    EXPECT_EQ(record->SetUri(g_test_uri), true);

    std::shared_ptr<std::string> retrievedUri = record->GetUri();
    ASSERT_NE(retrievedUri, nullptr);
    EXPECT_EQ(*retrievedUri, g_test_uri);
}

TEST_F(PasteboardClientAdapterImplTest, CRITICAL_SetHtmlText_NullptrParameter) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/html");
    ASSERT_NE(record, nullptr);
    std::shared_ptr<std::string> validText =
        std::make_shared<std::string>("<html>test</html>");
    EXPECT_EQ(record->SetHtmlText(validText), true);
}

TEST_F(PasteboardClientAdapterImplTest,
       CRITICAL_SetPlainText_NullptrParameter) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/plain");
    ASSERT_NE(record, nullptr);
    std::shared_ptr<std::string> validText =
        std::make_shared<std::string>("plain text");
    EXPECT_EQ(record->SetPlainText(validText), true);
}

TEST_F(PasteboardClientAdapterImplTest,
       CRITICAL_GetPasteData_NullptrFromOH_Pasteboard_GetDataParams_Create) {
    SetMockState(true);
    PasteRecordVector data;
    auto& mock = MockOHOSFunction::GetInstance();

    EXPECT_CALL(mock, OH_Pasteboard_GetDataParams_Create())
        .WillOnce(Return(nullptr));

    EXPECT_EQ(PasteBoardClientAdapterImpl::GetInstance().GetPasteData(data),
              false);

    SetMockState(false);
}

TEST_F(PasteboardClientAdapterImplTest,
       CRITICAL_SetPasteData_NullptrFromOH_UdmfData_Create) {
    PasteRecordVector data;
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/html");
    ASSERT_NE(record, nullptr);

    std::shared_ptr<std::string> htmlText =
        std::make_shared<std::string>("<html>test</html>");
    record->SetHtmlText(htmlText);
    data.push_back(record);

    PasteBoardClientAdapterImpl::GetInstance().SetPasteData(data);
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_GetPrimaryHtml_CreateFailure) {
    std::shared_ptr<PasteDataAdapter> adapter =
        std::make_shared<PasteDataAdapterImpl>();
    ASSERT_NE(adapter, nullptr);

    std::shared_ptr<std::string> result = adapter->GetPrimaryHtml();
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_GetPrimaryText_CreateFailure) {
    std::shared_ptr<PasteDataAdapter> adapter =
        std::make_shared<PasteDataAdapterImpl>();
    ASSERT_NE(adapter, nullptr);

    std::shared_ptr<std::string> result = adapter->GetPrimaryText();
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest,
       HIGH_RemovePasteboardChangedObserver_UnsubscribeFailure) {
    std::shared_ptr<PasteboardObserverAdapter> observer =
        std::make_shared<MockPasteboardObserver>();

    int32_t id =
        PasteBoardClientAdapterImpl::GetInstance().AddPasteboardChangedObserver(
            observer);
    ASSERT_GE(id, 0);

    PasteBoardClientAdapterImpl::GetInstance().RemovePasteboardChangedObserver(
        id);
}

TEST_F(PasteboardClientAdapterImplTest,
       HIGH_RemovePasteboardChangedObserver_DestroyFailure) {
    std::shared_ptr<PasteboardObserverAdapter> observer =
        std::make_shared<MockPasteboardObserver>();

    int32_t id =
        PasteBoardClientAdapterImpl::GetInstance().AddPasteboardChangedObserver(
            observer);
    ASSERT_GE(id, 0);

    PasteBoardClientAdapterImpl::GetInstance().RemovePasteboardChangedObserver(
        id);
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_HasType_NullptrPasteboard) {
    PasteBoardClientAdapterImpl::GetInstance().pasteboard_ = nullptr;

    bool result =
        PasteBoardClientAdapterImpl::GetInstance().HasType("text/html");
    EXPECT_EQ(result, false);

    PasteBoardClientAdapterImpl::GetInstance().pasteboard_ =
        OH_Pasteboard_Create();
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_HasType_NullptrType) {
    bool result = PasteBoardClientAdapterImpl::GetInstance().HasType(nullptr);
    EXPECT_EQ(result, false);
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_GetMimeType_NullptrRecord) {
    std::shared_ptr<PasteDataRecordAdapterImpl> record_null =
        std::make_shared<PasteDataRecordAdapterImpl>(
            nullptr, std::shared_ptr<OH_UdmfData>());
    ASSERT_NE(record_null, nullptr);

    std::string result = record_null->GetMimeType();
    EXPECT_EQ(result, "");
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_GetMimeTypes_NullptrRecord) {
    std::shared_ptr<PasteDataRecordAdapterImpl> record_null =
        std::make_shared<PasteDataRecordAdapterImpl>(
            nullptr, std::shared_ptr<OH_UdmfData>());
    ASSERT_NE(record_null, nullptr);

    std::vector<std::string> result = record_null->GetMimeTypes();
    EXPECT_EQ(result.empty(), true);
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_GetHtmlText_NullptrRecord) {
    std::shared_ptr<PasteDataRecordAdapterImpl> record_null =
        std::make_shared<PasteDataRecordAdapterImpl>(
            nullptr, std::shared_ptr<OH_UdmfData>());
    ASSERT_NE(record_null, nullptr);

    std::shared_ptr<std::string> result = record_null->GetHtmlText();
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_GetUri_NullptrRecord) {
    std::shared_ptr<PasteDataRecordAdapterImpl> record_null =
        std::make_shared<PasteDataRecordAdapterImpl>(
            nullptr, std::shared_ptr<OH_UdmfData>());
    ASSERT_NE(record_null, nullptr);

    std::shared_ptr<std::string> result = record_null->GetUri();
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_GetCustomData_NullptrRecord) {
    std::shared_ptr<PasteDataRecordAdapterImpl> record_null =
        std::make_shared<PasteDataRecordAdapterImpl>(
            nullptr, std::shared_ptr<OH_UdmfData>());
    ASSERT_NE(record_null, nullptr);

    std::shared_ptr<PasteCustomData> result = record_null->GetCustomData();
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_GetImgData_NullptrRecord) {
    std::shared_ptr<PasteDataRecordAdapterImpl> record_null =
        std::make_shared<PasteDataRecordAdapterImpl>(
            nullptr, std::shared_ptr<OH_UdmfData>());
    ASSERT_NE(record_null, nullptr);

    std::shared_ptr<MockClipBoardImageDataAdapter> image =
        std::make_shared<MockClipBoardImageDataAdapter>();

    bool result = record_null->GetImgData(image);
    EXPECT_EQ(result, false);
}

TEST_F(PasteboardClientAdapterImplTest, HIGH_SetImgData_NullptrImageData) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("image/png");
    ASSERT_NE(record, nullptr);

    bool result = record->SetImgData(nullptr);
    EXPECT_EQ(result, false);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_SetUri_EmptyUri) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/html");
    ASSERT_NE(record, nullptr);

    bool result = record->SetUri("");
    EXPECT_EQ(result, false);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_SetCustomData_EmptyData) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/html");
    ASSERT_NE(record, nullptr);

    PasteCustomData emptyData;
    bool result = record->SetCustomData(emptyData);
    EXPECT_EQ(result, false);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_AddHtmlRecord_NullData) {
    std::shared_ptr<PasteDataAdapterImpl> adapter_null =
        std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(adapter_null, nullptr);

    adapter_null->AddHtmlRecord(g_test_str);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_AddTextRecord_NullData) {
    std::shared_ptr<PasteDataAdapterImpl> adapter_null =
        std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(adapter_null, nullptr);

    adapter_null->AddTextRecord(g_test_str);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_GetMimeTypes_NullData) {
    std::shared_ptr<PasteDataAdapterImpl> adapter_null =
        std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(adapter_null, nullptr);

    std::vector<std::string> result = adapter_null->GetMimeTypes();
    EXPECT_EQ(result.empty(), true);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_GetPrimaryHtml_NullData) {
    std::shared_ptr<PasteDataAdapterImpl> adapter_null =
        std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(adapter_null, nullptr);

    std::shared_ptr<std::string> result = adapter_null->GetPrimaryHtml();
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_GetPrimaryText_NullData) {
    std::shared_ptr<PasteDataAdapterImpl> adapter_null =
        std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(adapter_null, nullptr);

    std::shared_ptr<std::string> result = adapter_null->GetPrimaryText();
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_GetPrimaryMimeType_NullData) {
    std::shared_ptr<PasteDataAdapterImpl> adapter_null =
        std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(adapter_null, nullptr);

    std::shared_ptr<std::string> result = adapter_null->GetPrimaryMimeType();
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_AllRecords_NullData) {
    std::shared_ptr<PasteDataAdapterImpl> adapter_null =
        std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(adapter_null, nullptr);

    PasteRecordVector result = adapter_null->AllRecords();
    EXPECT_EQ(result.empty(), true);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_GetRecordAt_NullData) {
    std::shared_ptr<PasteDataAdapterImpl> adapter_null =
        std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(adapter_null, nullptr);

    std::shared_ptr<PasteDataRecordAdapter> result =
        adapter_null->GetRecordAt(0);
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_GetRecordAt_OutOfBounds) {
    std::shared_ptr<PasteDataAdapter> adapter =
        std::make_shared<PasteDataAdapterImpl>();
    ASSERT_NE(adapter, nullptr);

    std::shared_ptr<PasteDataRecordAdapter> result = adapter->GetRecordAt(999);
    EXPECT_EQ(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_SetPasteData_NoneCopyOption) {
    PasteRecordVector data;
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/html");
    ASSERT_NE(record, nullptr);
    data.push_back(record);

    PasteBoardClientAdapterImpl::GetInstance().SetPasteData(
        data, CopyOptionMode::NONE);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_GetTokenId) {
    uint32_t result = PasteBoardClientAdapterImpl::GetInstance().GetTokenId();
    EXPECT_EQ(result, 0);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_OpenRemoteUri) {
    int32_t result = PasteBoardClientAdapterImpl::GetInstance().OpenRemoteUri(
        "/data/test/path");
    EXPECT_EQ(result, -1);
}

TEST_F(PasteboardClientAdapterImplTest,
       MEDIUM_TransitionCopyOption_CrossDevice) {
    Udmf_ShareOption result =
        PasteBoardClientAdapterImpl::GetInstance().TransitionCopyOption(
            CopyOptionMode::CROSS_DEVICE);
    EXPECT_EQ(result, Udmf_ShareOption::SHARE_OPTIONS_CROSS_APP);
}

TEST_F(PasteboardClientAdapterImplTest,
       MEDIUM_AddPasteboardChangedObserver_NullptrCallback) {
    int32_t id =
        PasteBoardClientAdapterImpl::GetInstance().AddPasteboardChangedObserver(
            nullptr);
    EXPECT_EQ(id, -1);
}

TEST_F(PasteboardClientAdapterImplTest,
       MEDIUM_RemovePasteboardChangedObserver_InvalidId) {
    PasteBoardClientAdapterImpl::GetInstance().RemovePasteboardChangedObserver(
        -1);
}

TEST_F(PasteboardClientAdapterImplTest,
       MEDIUM_RemovePasteboardChangedObserver_NonExistentId) {
    PasteBoardClientAdapterImpl::GetInstance().RemovePasteboardChangedObserver(
        9999);
}

TEST_F(PasteboardClientAdapterImplTest, MEDIUM_GetRecordCount_NullData) {
    std::shared_ptr<PasteDataAdapterImpl> adapter_null =
        std::make_shared<PasteDataAdapterImpl>(nullptr);
    ASSERT_NE(adapter_null, nullptr);

    std::size_t result = adapter_null->GetRecordCount();
    EXPECT_EQ(result, 0);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_MultipleBlockTags) {
    std::string html = "<p>first</p><div>second</div><ul>third</ul>";
    std::string expected = "first\r\nsecond\r\nthird";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_NestedBlockTags) {
    std::string html = "<div><p>nested</p><div>content</div></div>";
    std::string expected = "nested\r\ncontent";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_H1ToH6Tags) {
    std::string html = "<h1>a</h1><h2>b</h2><h3>c</h3><h4>d</h4><h5>e</h5><h6>f</h6>";
    std::string expected = "a\r\nb\r\nc\r\nd\r\ne\r\nf";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_LiTags) {
    std::string html = "<ul><li>item1</li><li>item2</li></ul>";
    std::string expected = "item1\r\nitem2";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_OlTags) {
    std::string html = "<ol><li>first</li><li>second</li></ol>";
    std::string expected = "first\r\nsecond";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_MixedTags) {
    std::string html = "<p>para</p><span>inline</span><div>block</div>";
    std::string expected = "para\r\ninlineblock";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_SpaceInTag) {
    std::string html = "<p class=\"test\">content</p>";
    std::string expected = "content";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_OnlyOpeningTag) {
    std::string html = "<p>only opening";
    std::string expected = "only opening";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_EmptyBetweenTags) {
    std::string html = "<p></p><div></div><p>text</p>";
    std::string expected = "text";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetHtmlText_NullRecord) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/html");
    ASSERT_NE(record, nullptr);
    std::shared_ptr<std::string> htmlText = std::make_shared<std::string>("<html>test</html>");
    EXPECT_EQ(record->SetHtmlText(htmlText), true);
    std::shared_ptr<std::string> result = record->GetHtmlText();
    EXPECT_NE(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetPlainText_NullRecord) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/plain");
    ASSERT_NE(record, nullptr);
    std::shared_ptr<std::string> plainText = std::make_shared<std::string>("plain text content");
    EXPECT_EQ(record->SetPlainText(plainText), true);
    std::shared_ptr<std::string> result = record->GetPlainText();
    EXPECT_NE(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetUri_NullRecord) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("text/uri");
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->SetUri(g_test_uri), true);
    std::shared_ptr<std::string> result = record->GetUri();
    EXPECT_NE(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, SetAndGetCustomData_NullRecord) {
    std::shared_ptr<PasteDataRecordAdapter> record =
        PasteDataRecordAdapter::NewRecord("custom/data");
    ASSERT_NE(record, nullptr);
    std::string format = "ohos/custom-data";
    std::vector<uint8_t> data = { 0, 1, 2, 3, 4 };
    PasteCustomData customData;
    customData.insert(std::make_pair(format, data));
    EXPECT_EQ(record->SetCustomData(customData), true);
    std::shared_ptr<PasteCustomData> result = record->GetCustomData();
    EXPECT_NE(result, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, HasPasteDataTest) {
    PasteBoardClientAdapterImpl::GetInstance().Clear();
    bool hasData = PasteBoardClientAdapterImpl::GetInstance().HasPasteData();
    EXPECT_EQ(hasData, false);
}

TEST_F(PasteboardClientAdapterImplTest, IsLocalPasteTest) {
    bool isLocal = PasteBoardClientAdapterImpl::GetInstance().IsLocalPaste();
    EXPECT_EQ(isLocal, false);
}

TEST_F(PasteboardClientAdapterImplTest, GetTokenIdTest) {
    uint32_t tokenId = PasteBoardClientAdapterImpl::GetInstance().GetTokenId();
    EXPECT_EQ(tokenId, 0);
}

TEST_F(PasteboardClientAdapterImplTest, OpenRemoteUriTest) {
    int32_t result = PasteBoardClientAdapterImpl::GetInstance().OpenRemoteUri("/data/test/path");
    EXPECT_EQ(result, -1);
}

TEST_F(PasteboardClientAdapterImplTest, Clear_EmptyData) {
    PasteBoardClientAdapterImpl::GetInstance().Clear();
}

TEST_F(PasteboardClientAdapterImplTest, GetMimeTypes_ValidData) {
    std::shared_ptr<PasteDataAdapter> adapter =
        std::make_shared<PasteDataAdapterImpl>();
    ASSERT_NE(adapter, nullptr);
    adapter->AddTextRecord("test text");
    std::vector<std::string> mimeTypes = adapter->GetMimeTypes();
    EXPECT_EQ(mimeTypes.empty(), false);
}

TEST_F(PasteboardClientAdapterImplTest, GetPrimaryMimeType_ValidRecord) {
    std::shared_ptr<PasteDataAdapter> adapter =
        std::make_shared<PasteDataAdapterImpl>();
    ASSERT_NE(adapter, nullptr);
    adapter->AddHtmlRecord("<html>test</html>");
    std::shared_ptr<std::string> mimeType = adapter->GetPrimaryMimeType();
    EXPECT_NE(mimeType, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, GetRecordAt_ValidIndex) {
    std::shared_ptr<PasteDataAdapter> adapter =
        std::make_shared<PasteDataAdapterImpl>();
    ASSERT_NE(adapter, nullptr);
    adapter->AddTextRecord("test text");
    std::shared_ptr<PasteDataRecordAdapter> record = adapter->GetRecordAt(0);
    EXPECT_NE(record, nullptr);
}

TEST_F(PasteboardClientAdapterImplTest, AllRecords_WithData) {
    std::shared_ptr<PasteDataAdapter> adapter =
        std::make_shared<PasteDataAdapterImpl>();
    ASSERT_NE(adapter, nullptr);
    adapter->AddTextRecord("test text");
    PasteRecordVector records = adapter->AllRecords();
    EXPECT_EQ(records.empty(), false);
}

TEST_F(PasteboardClientAdapterImplTest, GetRecordCount_WithData) {
    std::shared_ptr<PasteDataAdapter> adapter =
        std::make_shared<PasteDataAdapterImpl>();
    ASSERT_NE(adapter, nullptr);
    adapter->AddTextRecord("test text");
    std::size_t count = adapter->GetRecordCount();
    EXPECT_EQ(count, 1);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_ConsecutiveBlockTags) {
    std::string html = "<h1>header</h1><div>content</div><p>paragraph</p>";
    std::string expected = "header\r\ncontent\r\nparagraph";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

TEST_F(PasteboardClientAdapterImplTest, HtmlToPlainText_NonBlockTagsOnly) {
    std::string html = "<span>hello</span><b>world</b><i>test</i>";
    std::string expected = "helloworldtest";
    EXPECT_EQ(g_datarecord->HtmlToPlainText(html), expected);
}

}  // namespace OHOS::NWeb
