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

#ifndef DEVICE_GAMEPAD_GAMEPAD_DEVICE_OHOS_H_
#define DEVICE_GAMEPAD_GAMEPAD_DEVICE_OHOS_H_

#include <array>
#include <string>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

class GamepadDeviceOhos final : public AbstractHapticGamepad {
 public:
  GamepadDeviceOhos(int32_t device_id, const std::string& device_name,
                    uint16_t vendor_id, uint16_t product_id);
  ~GamepadDeviceOhos() override;

  uint16_t GetVendorId() const { return vendor_id_; }
  uint16_t GetProductId() const { return product_id_; }
  std::string GetName() const { return name_; }
  GamepadStandardMappingFunction GetMappingFunction() const;

  void UpdateButtonState(int button_code, bool pressed, float value);
  void UpdateAxisState(int axis_type, float x_value, float y_value);

  void ReadPadState(Gamepad* pad);

  bool IsEmpty() const { return !connected_; }
  void SetConnected(bool connected) { connected_ = connected; }
  size_t GetAxesLength() const { return axes_length_; }
  size_t GetButtonsLength() const { return buttons_length_; }

  // AbstractHapticGamepad public implementation.
  void SetVibration(mojom::GamepadEffectParametersPtr params) override;
  void SetZeroVibration() override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

 private:
  void DoShutdown() override;

  int32_t device_id_;
  std::string name_;
  uint16_t vendor_id_ = 0;
  uint16_t product_id_ = 0;
  bool connected_ = true;

  std::array<GamepadButton, Gamepad::kButtonsLengthCap> buttons_{};
  std::array<double, Gamepad::kAxesLengthCap> axes_{};
  size_t buttons_length_ = 0;
  size_t axes_length_ = 0;

  base::WeakPtrFactory<GamepadDeviceOhos> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_DEVICE_OHOS_H_
