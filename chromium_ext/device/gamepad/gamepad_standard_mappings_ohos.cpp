// Copyright (c) 2026 Huawei Device Co., Ltd.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>

#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_ohos_layout.h"

namespace device {

namespace {

// OHOS triggers report [0, 1] (not [-1, 1] like AxisToButton expects).
GamepadButton TriggerValueToButton(float value) {
  bool pressed = value > GamepadButton::kDefaultButtonPressedThreshold;
  bool touched = value > 0.f;
  return GamepadButton(pressed, touched, value);
}

// Xbox-style mapper: A/B/X/Y map directly to W3C canonical order.
// Covers Xbox 360/One/Bluetooth, PlayStation DS4/DualSense, most third-party.
void MapperOhosXboxStyle(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;

  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[kOhosButtonA];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[kOhosButtonB];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[kOhosButtonX];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[kOhosButtonY];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] =
      input.buttons[kOhosButtonLeftShoulder];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] =
      input.buttons[kOhosButtonRightShoulder];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] =
      TriggerValueToButton(input.axes[kOhosAxisLeftTrigger]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] =
      TriggerValueToButton(input.axes[kOhosAxisRightTrigger]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] =
      input.buttons[kOhosButtonBackSelect];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[kOhosButtonStart];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] =
      input.buttons[kOhosButtonLeftThumbstick];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] =
      input.buttons[kOhosButtonRightThumbstick];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[kOhosButtonDpadUp];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] =
      input.buttons[kOhosButtonDpadDown];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] =
      input.buttons[kOhosButtonDpadLeft];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      input.buttons[kOhosButtonDpadRight];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[kOhosButtonMeta];

  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[kOhosAxisLeftStickX];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[kOhosAxisLeftStickY];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[kOhosAxisRightStickX];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[kOhosAxisRightStickY];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

// Nintendo mapper: A/B and X/Y are swapped relative to Xbox layout.
void MapperOhosNintendoStyle(const Gamepad& input, Gamepad* mapped) {
  *mapped = input;

  mapped->buttons[BUTTON_INDEX_PRIMARY] = input.buttons[kOhosButtonB];
  mapped->buttons[BUTTON_INDEX_SECONDARY] = input.buttons[kOhosButtonA];
  mapped->buttons[BUTTON_INDEX_TERTIARY] = input.buttons[kOhosButtonY];
  mapped->buttons[BUTTON_INDEX_QUATERNARY] = input.buttons[kOhosButtonX];
  mapped->buttons[BUTTON_INDEX_LEFT_SHOULDER] =
      input.buttons[kOhosButtonLeftShoulder];
  mapped->buttons[BUTTON_INDEX_RIGHT_SHOULDER] =
      input.buttons[kOhosButtonRightShoulder];
  mapped->buttons[BUTTON_INDEX_LEFT_TRIGGER] =
      TriggerValueToButton(input.axes[kOhosAxisLeftTrigger]);
  mapped->buttons[BUTTON_INDEX_RIGHT_TRIGGER] =
      TriggerValueToButton(input.axes[kOhosAxisRightTrigger]);
  mapped->buttons[BUTTON_INDEX_BACK_SELECT] =
      input.buttons[kOhosButtonBackSelect];
  mapped->buttons[BUTTON_INDEX_START] = input.buttons[kOhosButtonStart];
  mapped->buttons[BUTTON_INDEX_LEFT_THUMBSTICK] =
      input.buttons[kOhosButtonLeftThumbstick];
  mapped->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK] =
      input.buttons[kOhosButtonRightThumbstick];
  mapped->buttons[BUTTON_INDEX_DPAD_UP] = input.buttons[kOhosButtonDpadUp];
  mapped->buttons[BUTTON_INDEX_DPAD_DOWN] =
      input.buttons[kOhosButtonDpadDown];
  mapped->buttons[BUTTON_INDEX_DPAD_LEFT] =
      input.buttons[kOhosButtonDpadLeft];
  mapped->buttons[BUTTON_INDEX_DPAD_RIGHT] =
      input.buttons[kOhosButtonDpadRight];
  mapped->buttons[BUTTON_INDEX_META] = input.buttons[kOhosButtonMeta];

  mapped->axes[AXIS_INDEX_LEFT_STICK_X] = input.axes[kOhosAxisLeftStickX];
  mapped->axes[AXIS_INDEX_LEFT_STICK_Y] = input.axes[kOhosAxisLeftStickY];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_X] = input.axes[kOhosAxisRightStickX];
  mapped->axes[AXIS_INDEX_RIGHT_STICK_Y] = input.axes[kOhosAxisRightStickY];

  mapped->buttons_length = BUTTON_INDEX_COUNT;
  mapped->axes_length = AXIS_INDEX_COUNT;
}

constexpr struct MappingData {
  GamepadId gamepad_id;
  GamepadStandardMappingFunction function;
} kAvailableMappings[] = {
    // Xbox 360 controllers (OHOS has no XInput, so these need mappers)
    {GamepadId::kMicrosoftProduct028e, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct0291, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct02a1, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct0719, MapperOhosXboxStyle},
    // Xbox One controllers
    {GamepadId::kMicrosoftProduct02d1, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct02dd, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct02e3, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct02ea, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct0b00, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct0b06, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct0b0a, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct0b12, MapperOhosXboxStyle},
    // Xbox Bluetooth (Xbox One S, Elite Series 2, Adaptive)
    {GamepadId::kMicrosoftProduct0b20, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct0b21, MapperOhosXboxStyle},
    {GamepadId::kMicrosoftProduct0b22, MapperOhosXboxStyle},
    // PlayStation Dualshock 4
    {GamepadId::kSonyProduct05c4, MapperOhosXboxStyle},
    {GamepadId::kSonyProduct09cc, MapperOhosXboxStyle},
    {GamepadId::kSonyProduct0ba0, MapperOhosXboxStyle},
    // PlayStation DualSense
    {GamepadId::kSonyProduct0ce6, MapperOhosXboxStyle},
    {GamepadId::kSonyProduct0df2, MapperOhosXboxStyle},
    {GamepadId::kSonyProduct0e5f, MapperOhosXboxStyle},
    // Nintendo Switch — A/B and X/Y are swapped
    {GamepadId::kNintendoProduct2006, MapperOhosNintendoStyle},
    {GamepadId::kNintendoProduct2007, MapperOhosNintendoStyle},
    {GamepadId::kNintendoProduct2009, MapperOhosNintendoStyle},
    {GamepadId::kNintendoProduct200e, MapperOhosNintendoStyle},
};

}  // namespace

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    std::string_view product_name,
    const uint16_t vendor_id,
    const uint16_t product_id,
    const uint16_t hid_specification_version,
    const uint16_t version_number,
    GamepadBusType bus_type) {
  GamepadId gamepad_id =
      GamepadIdList::Get().GetGamepadId(product_name, vendor_id, product_id);
  const auto* find_it = std::ranges::find(kAvailableMappings, gamepad_id,
                                          &MappingData::gamepad_id);
  return (find_it == std::end(kAvailableMappings)) ? nullptr : find_it->function;
}

}  // namespace device
