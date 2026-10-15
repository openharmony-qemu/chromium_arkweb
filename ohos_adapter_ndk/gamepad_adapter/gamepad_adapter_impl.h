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

#ifndef GAMEPAD_ADAPTER_GAMEPAD_ADAPTER_IMPL_H
#define GAMEPAD_ADAPTER_GAMEPAD_ADAPTER_IMPL_H

#include <cstdint>

#include <GameControllerKit/game_device.h>
#include <GameControllerKit/game_pad.h>

namespace OHOS::NWeb {

class GamepadAdapterImpl {
public:
  GamepadAdapterImpl();
  ~GamepadAdapterImpl();

  void EnumerateExistingDevices();

private:
  static void EnumerateFromDeviceInfos(GameDevice_AllDeviceInfos* allDeviceInfos);

  static void OnGameDeviceEvent(const GameDevice_DeviceEvent* deviceEvent);
  static void OnGamePadButtonEvent(const GamePad_ButtonEvent* buttonEvent);
  static void OnGamePadAxisEvent(const GamePad_AxisEvent* axisEvent);
};

}  // namespace OHOS::NWeb

#endif  // GAMEPAD_ADAPTER_GAMEPAD_ADAPTER_IMPL_H
