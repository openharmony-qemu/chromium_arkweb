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

#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api_new.h"

#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_api_define.h"

namespace MockNdkApi {

bool MockOHOSFunction::pasteboard_GetDataParams_Create = false;
bool MockOHOSFunction::abilityRuntime_ApplicationContextGetCacheDir = false;
bool MockOHOSFunction::pasteboard_GetDataParams_SetProgressIndicator = false;
bool MockOHOSFunction::pasteboard_GetDataParams_SetDestUri = false;
bool MockOHOSFunction::pasteboard_GetDataParams_SetFileConflictOptions = false;
bool MockOHOSFunction::pasteboard_GetDataWithProgress = false;
bool MockOHOSFunction::pasteboard_HasData = false;
bool MockOHOSFunction::pasteboard_GetDataParams_Destroy = false;
bool MockOHOSFunction::udmfData_GetRecords = false;
bool MockOHOSFunction::udmfData_IsLocal = false;

IMPLEMENT_MOCK_KEY(MockOHOSFunction, OH_PixelmapImageInfo_GetWidth);
IMPLEMENT_MOCK_KEY(MockOHOSFunction, OH_PixelmapImageInfo_GetHeight);
IMPLEMENT_MOCK_KEY(MockOHOSFunction, OH_PixelmapImageInfo_GetRowStride);
IMPLEMENT_MOCK_KEY(MockOHOSFunction, OH_PixelmapImageInfo_GetAlphaType);
IMPLEMENT_MOCK_KEY(MockOHOSFunction, OH_PixelmapImageInfo_GetPixelFormat);
IMPLEMENT_MOCK_KEY(MockOHOSFunction, OH_PixelmapNative_ReadPixels);

#ifdef __cplusplus
extern "C" {
#endif

IMPLEMENT_WRAP_FUNCTION(Image_ErrorCode,
                        MockOHOSFunction,
                        OH_PixelmapImageInfo_GetWidth,
                        (OH_Pixelmap_ImageInfo * info, uint32_t* width),
                        info,
                        width)

IMPLEMENT_WRAP_FUNCTION(Image_ErrorCode,
                        MockOHOSFunction,
                        OH_PixelmapImageInfo_GetHeight,
                        (OH_Pixelmap_ImageInfo * info, uint32_t* height),
                        info,
                        height)

IMPLEMENT_WRAP_FUNCTION(Image_ErrorCode,
                        MockOHOSFunction,
                        OH_PixelmapImageInfo_GetRowStride,
                        (OH_Pixelmap_ImageInfo * info, uint32_t* rowStride),
                        info,
                        rowStride)

IMPLEMENT_WRAP_FUNCTION(Image_ErrorCode,
                        MockOHOSFunction,
                        OH_PixelmapImageInfo_GetAlphaType,
                        (OH_Pixelmap_ImageInfo * info, int32_t* alphaType),
                        info,
                        alphaType)

IMPLEMENT_WRAP_FUNCTION(Image_ErrorCode,
                        MockOHOSFunction,
                        OH_PixelmapImageInfo_GetPixelFormat,
                        (OH_Pixelmap_ImageInfo * info, int32_t* pixelFormat),
                        info,
                        pixelFormat)

IMPLEMENT_WRAP_FUNCTION(Image_ErrorCode,
                        MockOHOSFunction,
                        OH_PixelmapNative_ReadPixels,
                        (OH_PixelmapNative * pixelmap,
                         uint8_t* destination,
                         size_t* bufferSize),
                        pixelmap,
                        destination,
                        bufferSize)

Pasteboard_GetDataParams* __wrap_OH_Pasteboard_GetDataParams_Create() {
  if (MockOHOSFunction::pasteboard_GetDataParams_Create) {
    return MockOHOSFunction::GetInstance().OH_Pasteboard_GetDataParams_Create();
  } else {
    return __real_OH_Pasteboard_GetDataParams_Create();
  }
}

AbilityRuntime_ErrorCode __wrap_OH_AbilityRuntime_ApplicationContextGetCacheDir(
    char* buffer,
    int32_t bufferSize,
    int32_t* writeLength) {
  if (MockOHOSFunction::abilityRuntime_ApplicationContextGetCacheDir) {
    return MockOHOSFunction::GetInstance()
        .OH_AbilityRuntime_ApplicationContextGetCacheDir(buffer, bufferSize,
                                                         writeLength);
  } else {
    return __real_OH_AbilityRuntime_ApplicationContextGetCacheDir(
        buffer, bufferSize, writeLength);
  }
}

void __wrap_OH_Pasteboard_GetDataParams_SetProgressIndicator(
    Pasteboard_GetDataParams* params,
    Pasteboard_ProgressIndicator progressIndicator) {
  if (MockOHOSFunction::pasteboard_GetDataParams_SetProgressIndicator) {
    return MockOHOSFunction::GetInstance()
        .OH_Pasteboard_GetDataParams_SetProgressIndicator(params,
                                                          progressIndicator);
  } else {
    return __real_OH_Pasteboard_GetDataParams_SetProgressIndicator(
        params, progressIndicator);
  }
}

void __wrap_OH_Pasteboard_GetDataParams_SetDestUri(
    Pasteboard_GetDataParams* params,
    const char* destUri,
    uint32_t destUriLen) {
  if (MockOHOSFunction::pasteboard_GetDataParams_SetDestUri) {
    return MockOHOSFunction::GetInstance()
        .OH_Pasteboard_GetDataParams_SetDestUri(params, destUri, destUriLen);
  } else {
    return __real_OH_Pasteboard_GetDataParams_SetDestUri(params, destUri,
                                                         destUriLen);
  }
}

void __wrap_OH_Pasteboard_GetDataParams_SetFileConflictOptions(
    Pasteboard_GetDataParams* params,
    Pasteboard_FileConflictOptions option) {
  if (MockOHOSFunction::pasteboard_GetDataParams_SetFileConflictOptions) {
    return MockOHOSFunction::GetInstance()
        .OH_Pasteboard_GetDataParams_SetFileConflictOptions(params, option);
  } else {
    return __real_OH_Pasteboard_GetDataParams_SetFileConflictOptions(params,
                                                                     option);
  }
}

OH_UdmfData* __wrap_OH_Pasteboard_GetDataWithProgress(
    OH_Pasteboard* pasteboard,
    Pasteboard_GetDataParams* params,
    int* status) {
  if (MockOHOSFunction::pasteboard_GetDataWithProgress) {
    return MockOHOSFunction::GetInstance().OH_Pasteboard_GetDataWithProgress(
        pasteboard, params, status);
  } else {
    return __real_OH_Pasteboard_GetDataWithProgress(pasteboard, params, status);
  }
}

bool __wrap_OH_Pasteboard_HasData(OH_Pasteboard* pasteboard) {
  if (MockOHOSFunction::pasteboard_HasData) {
    return MockOHOSFunction::GetInstance().OH_Pasteboard_HasData(pasteboard);
  } else {
    return __real_OH_Pasteboard_HasData(pasteboard);
  }
}

void __wrap_OH_Pasteboard_GetDataParams_Destroy(
    Pasteboard_GetDataParams* params) {
  if (MockOHOSFunction::pasteboard_GetDataParams_Destroy) {
    return MockOHOSFunction::GetInstance().OH_Pasteboard_GetDataParams_Destroy(
        params);
  } else {
    return __real_OH_Pasteboard_GetDataParams_Destroy(params);
  }
}

OH_UdmfRecord** __wrap_OH_UdmfData_GetRecords(OH_UdmfData* pThis,
                                              unsigned int* count) {
  if (MockOHOSFunction::udmfData_GetRecords) {
    return MockOHOSFunction::GetInstance().OH_UdmfData_GetRecords(pThis, count);
  } else {
    return __real_OH_UdmfData_GetRecords(pThis, count);
  }
}

bool __wrap_OH_UdmfData_IsLocal(OH_UdmfData* data) {
  if (MockOHOSFunction::udmfData_IsLocal) {
    return MockOHOSFunction::GetInstance().OH_UdmfData_IsLocal(data);
  } else {
    return __real_OH_UdmfData_IsLocal(data);
  }
}

#ifdef __cplusplus
}
#endif
}  // namespace MockNdkApi
