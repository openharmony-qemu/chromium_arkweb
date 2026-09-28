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

#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_device_ohos.h"
#include "device/gamepad/gamepad_ohos_layout.h"

namespace device {

GamepadDeviceOhos::GamepadDeviceOhos(int32_t device_id,
                                     const std::string& device_name,
                                     uint16_t vendor_id,
                                     uint16_t product_id)
    : device_id_(device_id), name_(device_name),
      vendor_id_(vendor_id), product_id_(product_id) {
}

GamepadDeviceOhos::~GamepadDeviceOhos() = default;

GamepadStandardMappingFunction GamepadDeviceOhos::GetMappingFunction() const {
  return GetGamepadStandardMappingFunction(
      name_, vendor_id_, product_id_,
      /*hid_specification_version=*/0, /*version_number=*/0,
      GAMEPAD_BUS_UNKNOWN);
}

void GamepadDeviceOhos::UpdateButtonState(int button_code,
                                          bool pressed,
                                          float value) {
  if (button_code < 0 ||
      button_code >= static_cast<int>(Gamepad::kButtonsLengthCap)) {
    return;
  }
  buttons_[button_code].pressed = pressed;
  buttons_[button_code].value = value;
  if (button_code >= static_cast<int>(buttons_length_)) {
    buttons_length_ = button_code + 1;
  }
}

void GamepadDeviceOhos::UpdateAxisState(int axis_type,
                                        float x_value,
                                        float y_value) {
  // Sticks set axes_[X] and axes_[Y]; triggers set axes_[trigger] only.
  if (axis_type == kOhosAxisLeftStickX) {
    axes_[kOhosAxisLeftStickX] = x_value;
    axes_[kOhosAxisLeftStickY] = y_value;
  } else if (axis_type == kOhosAxisRightStickX) {
    axes_[kOhosAxisRightStickX] = x_value;
    axes_[kOhosAxisRightStickY] = y_value;
  } else if (axis_type == kOhosAxisLeftTrigger) {
    axes_[kOhosAxisLeftTrigger] = x_value;
  } else if (axis_type == kOhosAxisRightTrigger) {
    axes_[kOhosAxisRightTrigger] = x_value;
  } else {
    return;
  }
  if (axes_length_ < kOhosRawAxesLength) {
    axes_length_ = kOhosRawAxesLength;
  }
}

void GamepadDeviceOhos::ReadPadState(Gamepad* pad) {
  if (!pad) {
    return;
  }

  pad->connected = connected_;
  pad->timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
  pad->buttons_length = buttons_length_;
  pad->axes_length = axes_length_;

  for (size_t i = 0; i < buttons_length_; ++i) {
    pad->buttons[i] = buttons_[i];
  }
  for (size_t i = 0; i < axes_length_; ++i) {
    pad->axes[i] = axes_[i];
  }
}

void GamepadDeviceOhos::SetVibration(
    mojom::GamepadEffectParametersPtr params) {
}

void GamepadDeviceOhos::SetZeroVibration() {
}

base::WeakPtr<AbstractHapticGamepad> GamepadDeviceOhos::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void GamepadDeviceOhos::DoShutdown() {
  connected_ = false;
}

}  // namespace device
