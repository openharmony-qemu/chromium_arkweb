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

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_OHOS_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_OHOS_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_device_ohos.h"
#include "gamepad_platform_data_fetcher_bridge.h"

namespace device {
class GamepadPlatformDataFetcherOhos : public GamepadDataFetcher {
 public:
  class Factory : public GamepadDataFetcherFactory {
   public:
    Factory();
    ~Factory() override;
    std::unique_ptr<GamepadDataFetcher> CreateDataFetcher() override;
    GamepadSource source() override;
    static GamepadSource static_source();
  };

  GamepadPlatformDataFetcherOhos();
  ~GamepadPlatformDataFetcherOhos() override;

  GamepadSource source() override;

  void PlayEffect(int pad_index,
                  mojom::GamepadHapticEffectType,
                  mojom::GamepadEffectParametersPtr,
                  mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
                  scoped_refptr<base::SequencedTaskRunner>) override;

  void GetGamepadData(bool devices_changed_hint) override;
  bool DisconnectUnrecognizedGamepad(int source_id) override;

  void ResetVibration(
      int pad_index,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback,
      scoped_refptr<base::SequencedTaskRunner>) override;

  void OnDeviceConnected(int32_t device_id, const std::string& device_name,
                         uint16_t vendor_id, uint16_t product_id);
  void OnDeviceDisconnected(int32_t device_id);
  void OnButtonEvent(int32_t device_id, int button_code, bool pressed);
  void OnAxisEvent(int32_t device_id, int axis_type, float x, float y);

  // Post events from FFRT callback threads to the polling thread.
  void PostDeviceStateEvent(const GamepadDeviceStateEvent& event);
  void PostButtonEvent(const GamepadButtonEvent& event);
  void PostAxisEvent(const GamepadAxisEvent& event);

 private:
  void OnAddedToProvider() override;

  // Handlers executed on the polling thread via PostTask.
  void HandleDeviceStateEvent(int32_t device_id,
                              bool connected,
                              const std::string& device_name,
                              uint16_t vendor_id,
                              uint16_t product_id);
  void HandleButtonEvent(int32_t device_id, int button_code, bool pressed);
  void HandleAxisEvent(int32_t device_id, int axis_type, float x, float y);

  GamepadDeviceOhos* GetDeviceWithDeviceId(int32_t device_id);
  GamepadDeviceOhos* GetOrCreateMatchingDevice(int32_t device_id,
                                               const std::string& name,
                                               uint16_t vendor_id,
                                               uint16_t product_id);
  void RemoveDevice(int32_t device_id);

  std::unordered_map<int32_t, std::unique_ptr<GamepadDeviceOhos>> devices_;

  scoped_refptr<base::SequencedTaskRunner> polling_task_runner_;

  base::WeakPtrFactory<GamepadPlatformDataFetcherOhos> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_OHOS_H_
