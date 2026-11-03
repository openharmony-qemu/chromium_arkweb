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

#ifndef GAMEPAD_ADAPTER_GAMEPAD_PLATFORM_DATA_FETCHER_BRIDGE_H_
#define GAMEPAD_ADAPTER_GAMEPAD_PLATFORM_DATA_FETCHER_BRIDGE_H_

#include <cstdint>

namespace device {

struct GamepadDeviceStateEvent {
  int32_t device_id;
  bool connected;
  const char* device_name;
  uint16_t vendor_id;
  uint16_t product_id;
};

struct GamepadButtonEvent {
  int32_t device_id;
  int32_t button_code;
  bool pressed;
};

struct GamepadAxisEvent {
  int32_t device_id;
  int32_t axis_type;
  float x;
  float y;
};

using DeviceStateCallback = void(*)(void* user_data,
                                    const GamepadDeviceStateEvent&);
using ButtonEventCallback = void(*)(void* user_data,
                                    const GamepadButtonEvent&);
using AxisEventCallback = void(*)(void* user_data,
                                  const GamepadAxisEvent&);

void RegisterGamepadFetcherCallbacks(void* user_data,
                                     DeviceStateCallback on_device_state,
                                     ButtonEventCallback on_button,
                                     AxisEventCallback on_axis);

void UnregisterGamepadFetcherCallbacks();

void OnGamepadDeviceConnected(int32_t device_id, const char* device_name,
                              uint16_t vendor_id, uint16_t product_id);
void OnGamepadDeviceDisconnected(int32_t device_id);
void OnGamepadButtonEvent(int32_t device_id, int button_code, bool pressed);
void OnGamepadAxisEvent(int32_t device_id, int axis_type, float x, float y);

void EnumerateExistingGamepadDevices();

}  // namespace device

#endif  // GAMEPAD_ADAPTER_GAMEPAD_PLATFORM_DATA_FETCHER_BRIDGE_H_
