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

#include <mutex>

#include "gamepad_platform_data_fetcher_bridge.h"
#include "gamepad_adapter_impl.h"

namespace device {

static std::mutex g_callback_mutex;
static void* g_fetcher_user_data = nullptr;
static DeviceStateCallback g_on_device_state = nullptr;
static ButtonEventCallback g_on_button = nullptr;
static AxisEventCallback g_on_axis = nullptr;

// Process-level singleton, intentionally never deleted — FFRT callbacks may
// fire at any time during process lifetime.
static OHOS::NWeb::GamepadAdapterImpl* g_adapter = nullptr;
static std::once_flag g_adapter_once_flag;

static void EnsureGamepadAdapter() {
  std::call_once(g_adapter_once_flag, []() {
    g_adapter = new OHOS::NWeb::GamepadAdapterImpl();
  });
}

void RegisterGamepadFetcherCallbacks(void* user_data,
                                     DeviceStateCallback on_device_state,
                                     ButtonEventCallback on_button,
                                     AxisEventCallback on_axis) {
  // Create adapter before acquiring mutex to avoid deadlock if a callback
  // fires synchronously during construction.
  EnsureGamepadAdapter();
  std::lock_guard<std::mutex> lock(g_callback_mutex);
  g_fetcher_user_data = user_data;
  g_on_device_state = on_device_state;
  g_on_button = on_button;
  g_on_axis = on_axis;
}

void UnregisterGamepadFetcherCallbacks() {
  std::lock_guard<std::mutex> lock(g_callback_mutex);
  g_fetcher_user_data = nullptr;
  g_on_device_state = nullptr;
  g_on_button = nullptr;
  g_on_axis = nullptr;
}

void OnGamepadDeviceConnected(int32_t device_id, const char* device_name,
                              uint16_t vendor_id, uint16_t product_id) {
  std::lock_guard<std::mutex> lock(g_callback_mutex);
  if (g_on_device_state) {
    g_on_device_state(g_fetcher_user_data,
                      {device_id, true, device_name, vendor_id, product_id});
  }
}

void OnGamepadDeviceDisconnected(int32_t device_id) {
  std::lock_guard<std::mutex> lock(g_callback_mutex);
  if (g_on_device_state) {
    g_on_device_state(g_fetcher_user_data, {device_id, false, nullptr, 0, 0});
  }
}

void OnGamepadButtonEvent(int32_t device_id, int button_code, bool pressed) {
  std::lock_guard<std::mutex> lock(g_callback_mutex);
  if (g_on_button) {
    g_on_button(g_fetcher_user_data, {device_id, button_code, pressed});
  }
}

void OnGamepadAxisEvent(int32_t device_id, int axis_type, float x, float y) {
  std::lock_guard<std::mutex> lock(g_callback_mutex);
  if (g_on_axis) {
    g_on_axis(g_fetcher_user_data, {device_id, axis_type, x, y});
  }
}

void EnumerateExistingGamepadDevices() {
  if (!g_adapter) {
    return;
  }
  g_adapter->EnumerateExistingDevices();
}

}  // namespace device
