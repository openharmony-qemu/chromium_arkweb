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

#ifndef DEVICE_GAMEPAD_GAMEPAD_OHOS_LAYOUT_H_
#define DEVICE_GAMEPAD_GAMEPAD_OHOS_LAYOUT_H_

namespace device {

// OHOS raw button layout indices for GamepadDeviceOhos::buttons_[].
// Produced by MapButtonCode, transformed to W3C canonical order by mappers.
// Matches common HID Button usage page ordering (index 2 reserved).
inline constexpr int kOhosButtonA = 0;
inline constexpr int kOhosButtonB = 1;
inline constexpr int kOhosButtonX = 3;
inline constexpr int kOhosButtonY = 4;
inline constexpr int kOhosButtonMeta = 5;
inline constexpr int kOhosButtonLeftShoulder = 6;
inline constexpr int kOhosButtonRightShoulder = 7;
inline constexpr int kOhosButtonLeftTrigger = 8;
inline constexpr int kOhosButtonRightTrigger = 9;
inline constexpr int kOhosButtonBackSelect = 10;
inline constexpr int kOhosButtonStart = 11;
inline constexpr int kOhosButtonLeftThumbstick = 12;
inline constexpr int kOhosButtonRightThumbstick = 13;
inline constexpr int kOhosButtonDpadUp = 14;
inline constexpr int kOhosButtonDpadDown = 15;
inline constexpr int kOhosButtonDpadLeft = 16;
inline constexpr int kOhosButtonDpadRight = 17;

// OHOS raw axis layout. Sticks: 0-3, triggers: 4-5.
inline constexpr int kOhosAxisLeftStickX = 0;
inline constexpr int kOhosAxisLeftStickY = 1;
inline constexpr int kOhosAxisRightStickX = 2;
inline constexpr int kOhosAxisRightStickY = 3;
inline constexpr int kOhosAxisLeftTrigger = 4;
inline constexpr int kOhosAxisRightTrigger = 5;

inline constexpr size_t kOhosRawAxesLength = 6;

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_OHOS_LAYOUT_H_
