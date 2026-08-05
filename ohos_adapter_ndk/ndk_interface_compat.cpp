/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <array>

#include "arkweb/ohos_adapter_ndk/camera_adapter/include/camera_manager_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/hiviewdfx_adapter/hisysevent_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/system_properties_adapter/system_properties_adapter_impl.h"

namespace OHOS::NWeb {

int HiSysEventAdapterImpl::Write(const std::string&,
                                 EventType,
                                 const std::tuple<const std::string,
                                                  const int32_t,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const int32_t>&) {
  return 0;
}

int HiSysEventAdapterImpl::Write(
    const std::string&,
    EventType,
    const std::tuple<const std::string, const int32_t>&) {
  return 0;
}

int HiSysEventAdapterImpl::Write(const std::string&,
                                 EventType,
                                 const std::tuple<const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string,
                                                  const std::string>&) {
  return 0;
}

int HiSysEventAdapterImpl::Write(
    const std::string&,
    EventType,
    const std::tuple<const int32_t, const int32_t>&) {
  return 0;
}

int32_t SystemPropertiesAdapterImpl::GetIntParameter(const std::string& key,
                                                     int32_t defaultValue) {
  return GetIntParameter(key.c_str(), defaultValue);
}

int32_t SystemPropertiesAdapterImpl::GetLoadUrlStrategy() {
  return 0;
}

int32_t SystemPropertiesAdapterImpl::GetLTPOIntConfig(const std::string&,
                                                      int32_t defaultValue) {
  return defaultValue;
}

int32_t SystemPropertiesAdapterImpl::GetRenderProcessMode() {
  return 0;
}

int32_t CameraManagerAdapterImpl::GetSupportedFlashModes(
    std::vector<FlashModeAdapter>& flashModesAdapter) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (captureSession_ == nullptr) {
    return CAMERA_ERROR;
  }

  constexpr std::array<FlashModeAdapter, 4> modes = {
      FlashModeAdapter::FLASH_MODE_CLOSE,
      FlashModeAdapter::FLASH_MODE_OPEN,
      FlashModeAdapter::FLASH_MODE_AUTO,
      FlashModeAdapter::FLASH_MODE_ALWAYS_OPEN,
  };
  for (auto mode : modes) {
    bool supported = false;
    auto result = OH_CaptureSession_IsFlashModeSupported(
        captureSession_, static_cast<Camera_FlashMode>(mode), &supported);
    if (result != Camera_ErrorCode::CAMERA_OK) {
      return CAMERA_ERROR;
    }
    if (supported) {
      flashModesAdapter.push_back(mode);
    }
  }
  return CAMERA_OK;
}

bool CameraManagerAdapterImpl::HasFlash() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (captureSession_ == nullptr) {
    return false;
  }
  bool hasFlash = false;
  return OH_CaptureSession_HasFlash(captureSession_, &hasFlash) ==
             Camera_ErrorCode::CAMERA_OK &&
         hasFlash;
}

FlashModeAdapter CameraManagerAdapterImpl::GetFlashMode() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (captureSession_ == nullptr) {
    return FlashModeAdapter::FLASH_MODE_CLOSE;
  }
  Camera_FlashMode mode = Camera_FlashMode::FLASH_MODE_CLOSE;
  if (OH_CaptureSession_GetFlashMode(captureSession_, &mode) !=
      Camera_ErrorCode::CAMERA_OK) {
    return FlashModeAdapter::FLASH_MODE_CLOSE;
  }
  return static_cast<FlashModeAdapter>(mode);
}

int32_t CameraManagerAdapterImpl::SetFlashMode(FlashModeAdapter flashMode) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (captureSession_ == nullptr) {
    return CAMERA_ERROR;
  }
  return OH_CaptureSession_SetFlashMode(
             captureSession_, static_cast<Camera_FlashMode>(flashMode)) ==
                 Camera_ErrorCode::CAMERA_OK
             ? CAMERA_OK
             : CAMERA_ERROR;
}

}  // namespace OHOS::NWeb
