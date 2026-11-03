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

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_platform_data_fetcher_ohos.h"
#include "gamepad_platform_data_fetcher_bridge.h"

namespace device {

GamepadPlatformDataFetcherOhos::Factory::Factory() = default;

GamepadPlatformDataFetcherOhos::Factory::~Factory() = default;

std::unique_ptr<GamepadDataFetcher>
GamepadPlatformDataFetcherOhos::Factory::CreateDataFetcher() {
  return std::make_unique<GamepadPlatformDataFetcherOhos>();
}

GamepadSource GamepadPlatformDataFetcherOhos::Factory::source() {
  return Factory::static_source();
}

GamepadSource GamepadPlatformDataFetcherOhos::Factory::static_source() {
  return GamepadSource::kOhos;
}

GamepadPlatformDataFetcherOhos::GamepadPlatformDataFetcherOhos() = default;

GamepadPlatformDataFetcherOhos::~GamepadPlatformDataFetcherOhos() {
  // Unregister callbacks before destroying members — blocks until in-flight
  // FFRT callbacks complete, preventing Use-After-Free.
  device::UnregisterGamepadFetcherCallbacks();

  for (auto& [device_id, device] : devices_) {
    device->Shutdown();
  }
}

GamepadSource GamepadPlatformDataFetcherOhos::source() {
  return Factory::static_source();
}

namespace {

void OnDeviceStateFromBridge(void* user_data,
                             const GamepadDeviceStateEvent& event) {
  auto* fetcher = static_cast<GamepadPlatformDataFetcherOhos*>(user_data);
  fetcher->PostDeviceStateEvent(event);
}

void OnButtonFromBridge(void* user_data,
                        const GamepadButtonEvent& event) {
  auto* fetcher = static_cast<GamepadPlatformDataFetcherOhos*>(user_data);
  fetcher->PostButtonEvent(event);
}

void OnAxisFromBridge(void* user_data,
                      const GamepadAxisEvent& event) {
  auto* fetcher = static_cast<GamepadPlatformDataFetcherOhos*>(user_data);
  fetcher->PostAxisEvent(event);
}

}  // anonymous namespace

void GamepadPlatformDataFetcherOhos::OnAddedToProvider() {
  polling_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  device::RegisterGamepadFetcherCallbacks(this,
                                          OnDeviceStateFromBridge,
                                          OnButtonFromBridge,
                                          OnAxisFromBridge);
  // Enumerate devices already connected at browser startup.
  device::EnumerateExistingGamepadDevices();
}

void GamepadPlatformDataFetcherOhos::PostDeviceStateEvent(
    const GamepadDeviceStateEvent& event) {
  // Copy device_name before crossing thread boundary.
  std::string device_name(event.device_name ? event.device_name : "");
  // Called on FFRT thread — bridge mutex ensures safety.
  polling_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadPlatformDataFetcherOhos::HandleDeviceStateEvent,
                     weak_factory_.GetWeakPtr(), event.device_id,
                     event.connected, std::move(device_name),
                     event.vendor_id, event.product_id));
}

void GamepadPlatformDataFetcherOhos::PostButtonEvent(
    const GamepadButtonEvent& event) {
  // Called on FFRT thread — bridge mutex ensures safety.
  polling_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadPlatformDataFetcherOhos::HandleButtonEvent,
                     weak_factory_.GetWeakPtr(), event.device_id,
                     event.button_code, event.pressed));
}

void GamepadPlatformDataFetcherOhos::PostAxisEvent(
    const GamepadAxisEvent& event) {
  // Called on FFRT thread — bridge mutex ensures safety.
  polling_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GamepadPlatformDataFetcherOhos::HandleAxisEvent,
                     weak_factory_.GetWeakPtr(), event.device_id,
                     event.axis_type, event.x, event.y));
}

void GamepadPlatformDataFetcherOhos::HandleDeviceStateEvent(
    int32_t device_id,
    bool connected,
    const std::string& device_name,
    uint16_t vendor_id,
    uint16_t product_id) {
  DCHECK_CALLED_ON_VALID_THREAD(polling_task_runner_);
  DCHECK(polling_task_runner_->RunsTasksInCurrentSequence());
  if (connected) {
    OnDeviceConnected(device_id, device_name, vendor_id, product_id);
  } else {
    OnDeviceDisconnected(device_id);
  }
}

void GamepadPlatformDataFetcherOhos::HandleButtonEvent(int32_t device_id,
                                                       int button_code,
                                                       bool pressed) {
  DCHECK_CALLED_ON_VALID_THREAD(polling_task_runner_);
  DCHECK(polling_task_runner_->RunsTasksInCurrentSequence());
  OnButtonEvent(device_id, button_code, pressed);
}

void GamepadPlatformDataFetcherOhos::HandleAxisEvent(int32_t device_id,
                                                     int axis_type,
                                                     float x,
                                                     float y) {
  DCHECK_CALLED_ON_VALID_THREAD(polling_task_runner_);
  DCHECK(polling_task_runner_->RunsTasksInCurrentSequence());
  OnAxisEvent(device_id, axis_type, x, y);
}

void GamepadPlatformDataFetcherOhos::GetGamepadData(bool devices_changed_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(polling_task_runner_);
  DCHECK(polling_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("GAMEPAD", "GetGamepadData (OHOS)");

  for (const auto& [device_id, device] : devices_) {
    PadState* state = GetPadState(device_id);
    if (!state) {
      continue;
    }

    Gamepad& pad = state->data;
    device->ReadPadState(&pad);

    state->is_active = !device->IsEmpty();
  }
}

bool GamepadPlatformDataFetcherOhos::DisconnectUnrecognizedGamepad(
    int source_id) {
  DCHECK_CALLED_ON_VALID_THREAD(polling_task_runner_);
  DCHECK(polling_task_runner_->RunsTasksInCurrentSequence());
  auto it = devices_.find(source_id);
  if (it == devices_.end()) {
    return false;
  }
  it->second->Shutdown();
  devices_.erase(it);
  return true;
}

void GamepadPlatformDataFetcherOhos::PlayEffect(
    int pad_index,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  GamepadDataFetcher::RunVibrationCallback(
      std::move(callback), std::move(callback_runner),
      mojom::GamepadHapticsResult::GamepadHapticsResultError);
}

void GamepadPlatformDataFetcherOhos::ResetVibration(
    int pad_index,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  GamepadDataFetcher::RunVibrationCallback(
      std::move(callback), std::move(callback_runner),
      mojom::GamepadHapticsResult::GamepadHapticsResultError);
}

GamepadDeviceOhos*
GamepadPlatformDataFetcherOhos::GetDeviceWithDeviceId(int32_t device_id) {
  DCHECK_CALLED_ON_VALID_THREAD(polling_task_runner_);
  DCHECK(polling_task_runner_->RunsTasksInCurrentSequence());
  auto it = devices_.find(device_id);
  if (it != devices_.end()) {
    return it->second.get();
  }
  return nullptr;
}

GamepadDeviceOhos*
GamepadPlatformDataFetcherOhos::GetOrCreateMatchingDevice(
    int32_t device_id,
    const std::string& name,
    uint16_t vendor_id,
    uint16_t product_id) {
  DCHECK_CALLED_ON_VALID_THREAD(polling_task_runner_);
  DCHECK(polling_task_runner_->RunsTasksInCurrentSequence());
  auto it = devices_.find(device_id);
  if (it != devices_.end()) {
    return it->second.get();
  }
  auto device = std::make_unique<GamepadDeviceOhos>(device_id, name,
                                                     vendor_id, product_id);
  GamepadDeviceOhos* result = device.get();
  devices_[device_id] = std::move(device);
  return result;
}

void GamepadPlatformDataFetcherOhos::RemoveDevice(int32_t device_id) {
  DCHECK_CALLED_ON_VALID_THREAD(polling_task_runner_);
  DCHECK(polling_task_runner_->RunsTasksInCurrentSequence());
  auto it = devices_.find(device_id);
  if (it != devices_.end()) {
    it->second->Shutdown();
    devices_.erase(it);
  }
}

void GamepadPlatformDataFetcherOhos::OnDeviceConnected(
    int32_t device_id,
    const std::string& device_name,
    uint16_t vendor_id,
    uint16_t product_id) {
  GamepadDeviceOhos* device = GetOrCreateMatchingDevice(device_id, device_name,
                                                        vendor_id, product_id);
  device->SetConnected(true);

  PadState* state = GetPadState(device_id);
  if (!state) {
    return;
  }

  state->is_newly_active = true;
  state->is_initialized = true;

  state->mapper = device->GetMappingFunction();

  Gamepad& pad = state->data;
  GamepadDataFetcher::UpdateGamepadStrings(
      device->GetName(), device->GetVendorId(), device->GetProductId(),
      state->mapper != nullptr, pad);

  pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  pad.vibration_actuator.not_null = false;
  pad.connected = true;

  state->button_mask = ~0u;
  state->axis_mask = ~0u;
}

void GamepadPlatformDataFetcherOhos::OnDeviceDisconnected(int32_t device_id) {
  RemoveDevice(device_id);
}

void GamepadPlatformDataFetcherOhos::OnButtonEvent(int32_t device_id,
                                                   int button_code,
                                                   bool pressed) {
  GamepadDeviceOhos* device = GetDeviceWithDeviceId(device_id);
  if (!device) {
    return;
  }
  device->UpdateButtonState(button_code, pressed, pressed ? 1.0f : 0.0f);
}

void GamepadPlatformDataFetcherOhos::OnAxisEvent(int32_t device_id,
                                                 int axis_type,
                                                 float x,
                                                 float y) {
  GamepadDeviceOhos* device = GetDeviceWithDeviceId(device_id);
  if (!device) {
    return;
  }
  device->UpdateAxisState(axis_type, x, y);
}

}  // namespace device
