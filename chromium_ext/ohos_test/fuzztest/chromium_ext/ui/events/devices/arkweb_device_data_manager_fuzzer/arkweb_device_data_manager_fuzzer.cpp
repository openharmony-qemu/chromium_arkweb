/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <fuzzer/FuzzedDataProvider.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/events/devices/input_device.h"
#define private public
#include "arkweb/build/features/features.h"
#include "arkweb/chromium_ext/ui/events/devices/arkweb_device_data_manager_utils.cc"
#include "arkweb/chromium_ext/ui/events/devices/arkweb_device_data_manager_utils.h"
#include "arkweb/ohos_adapter_ndk/ohos_adapter_helper_ext.h"
#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "content/browser/scheduler/browser_io_thread_delegate.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/scheduler/browser_task_priority.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/mmi_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/keyboard_device.h"

using namespace content;
using namespace OHOS::NWeb;
using testing::SetArgReferee;
using testing::Return;
using testing::_;
using testing::DoAll;

namespace ui {

class MockMMIAdapter : public MMIAdapter {
 public:
  MOCK_METHOD(char*, KeyCodeToString, (int32_t keyCode), (override));
  MOCK_METHOD(int32_t, RegisterMMIInputListener,
              (std::shared_ptr<MMIInputListenerAdapter> eventCallback), (override));
  MOCK_METHOD(void, UnregisterMMIInputListener, (int32_t monitorId), (override));
  MOCK_METHOD(int32_t, RegisterDevListener,
              (std::string type, std::shared_ptr<MMIListenerAdapter> listener), (override));
  MOCK_METHOD(int32_t, UnregisterDevListener, (std::string type), (override));
  MOCK_METHOD(int32_t, GetKeyboardType, (int32_t deviceId, int32_t& type), (override));
  MOCK_METHOD(int32_t, GetDeviceIds, (std::vector<int32_t>& ids), (override));
  MOCK_METHOD(int32_t, GetDeviceInfo,
              (int32_t deviceId, std::shared_ptr<MMIDeviceInfoAdapter> info), (override));
  MOCK_METHOD(int32_t, GetMaxTouchPoints, (), (override));
};

class MockArkWebDeviceDataManagerUtils : public ArkWebDeviceDataManagerUtils {
 public:
  explicit MockArkWebDeviceDataManagerUtils(DeviceDataManager* deviceDataManager)
      : ArkWebDeviceDataManagerUtils(deviceDataManager) {}
  MOCK_METHOD(void, AddMouseDevice, (const InputDevice&));
  MOCK_METHOD(void, AddTouchpadDevice, (const TouchpadDevice&));
  MOCK_METHOD(void, AddKeyboardDevice, (const KeyboardDevice&));
  MOCK_METHOD(void, DeleteDevice, (const InputDevice&));
};

class MockMMIListenerAdapterImpl : public MMIListenerAdapterImpl {
 public:
  MockMMIListenerAdapterImpl(
      ArkWebDeviceDataManagerUtils* utils,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      std::unique_ptr<OHOS::NWeb::MMIAdapter> mock_adapter)
      : MMIListenerAdapterImpl(utils, task_runner) {
    mmi_adapter_ = std::move(mock_adapter);
  }
};

class DeviceDataManagerFuzzer {
 public:
  DeviceDataManagerFuzzer() {
    auto ui_sequence_manager =
        base::sequence_manager::CreateUnboundSequenceManager(
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetPrioritySettings(
                    content::internal::CreateBrowserTaskPrioritySettings())
                .Build());
    auto browser_ui_thread_scheduler =
        content::BrowserUIThreadScheduler::CreateForTesting(
            ui_sequence_manager.get());
    content::BrowserTaskExecutor::CreateForTesting(
        std::move(browser_ui_thread_scheduler),
        std::make_unique<content::BrowserIOThreadDelegate>());

    DeviceDataManager::CreateInstance();
    device_data_manager_ = DeviceDataManager::GetInstance();
    utils_ =
        std::make_unique<ArkWebDeviceDataManagerUtils>(device_data_manager_);
    task_runner_ = content::GetUIThreadTaskRunner({});
  }

  ~DeviceDataManagerFuzzer() {
    utils_.reset();
    DeviceDataManager::DeleteInstance();
  }

  DeviceDataManager* GetDeviceDataManager() { return device_data_manager_; }
  ArkWebDeviceDataManagerUtils* GetUtils() { return utils_.get(); }
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

 private:
  raw_ptr<DeviceDataManager> device_data_manager_ = nullptr;
  std::unique_ptr<ArkWebDeviceDataManagerUtils> utils_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

void FuzzAddMouseDevice(FuzzedDataProvider* fdp,
                        DeviceDataManagerFuzzer* fuzzer) {
  int32_t device_id = fdp->ConsumeIntegral<int32_t>();
  std::string name = fdp->ConsumeRandomLengthString(64);
  InputDevice device(device_id, InputDeviceType::INPUT_DEVICE_USB, name);
  device.enabled = fdp->ConsumeBool();
  device.suspected_keyboard_imposter = fdp->ConsumeBool();
  device.suspected_mouse_imposter = fdp->ConsumeBool();

  fuzzer->GetUtils()->AddMouseDevice(device);

  InputDevice device2(device_id, InputDeviceType::INPUT_DEVICE_USB, name);
  device2.enabled = device.enabled;
  device2.suspected_keyboard_imposter = device.suspected_keyboard_imposter;
  device2.suspected_mouse_imposter = device.suspected_mouse_imposter;
  fuzzer->GetUtils()->AddMouseDevice(device2);
}

void FuzzAddKeyboardDevice(FuzzedDataProvider* fdp,
                           DeviceDataManagerFuzzer* fuzzer) {
  int32_t device_id = fdp->ConsumeIntegral<int32_t>();
  std::string name = fdp->ConsumeRandomLengthString(64);
  KeyboardDevice device(device_id, InputDeviceType::INPUT_DEVICE_USB, name);
  device.enabled = fdp->ConsumeBool();
  device.suspected_keyboard_imposter = fdp->ConsumeBool();
  device.suspected_mouse_imposter = fdp->ConsumeBool();

  fuzzer->GetUtils()->AddKeyboardDevice(device);

  KeyboardDevice device2(device_id, InputDeviceType::INPUT_DEVICE_USB, name);
  device2.enabled = device.enabled;
  device2.suspected_keyboard_imposter = device.suspected_keyboard_imposter;
  device2.suspected_mouse_imposter = device.suspected_mouse_imposter;
  fuzzer->GetUtils()->AddKeyboardDevice(device2);
}

void FuzzAddTouchpadDevice(FuzzedDataProvider* fdp,
                           DeviceDataManagerFuzzer* fuzzer) {
  int32_t device_id = fdp->ConsumeIntegral<int32_t>();
  std::string name = fdp->ConsumeRandomLengthString(64);
  TouchpadDevice device(device_id, InputDeviceType::INPUT_DEVICE_USB, name);
  device.enabled = fdp->ConsumeBool();
  device.suspected_keyboard_imposter = fdp->ConsumeBool();
  device.suspected_mouse_imposter = fdp->ConsumeBool();

  fuzzer->GetUtils()->AddTouchpadDevice(device);

  TouchpadDevice device2(device_id, InputDeviceType::INPUT_DEVICE_USB, name);
  device2.enabled = device.enabled;
  device2.suspected_keyboard_imposter = device.suspected_keyboard_imposter;
  device2.suspected_mouse_imposter = device.suspected_mouse_imposter;
  fuzzer->GetUtils()->AddTouchpadDevice(device2);
}

void FuzzDeleteDevice(FuzzedDataProvider* fdp,
                      DeviceDataManagerFuzzer* fuzzer) {
  int32_t device_id = fdp->ConsumeIntegral<int32_t>();
  std::string name = fdp->ConsumeRandomLengthString(32);
  InputDevice device(device_id, InputDeviceType::INPUT_DEVICE_USB, name);

  fuzzer->GetUtils()->DeleteDevice(device);
}

void FuzzDeleteExactDevice(FuzzedDataProvider* fdp,
                           DeviceDataManagerFuzzer* fuzzer) {
  int32_t device_id = fdp->ConsumeIntegralInRange<int32_t>(1, 10);
  InputDevice device(device_id, InputDeviceType::INPUT_DEVICE_USB,
                     fdp->ConsumeRandomLengthString(32));
  device.enabled = fdp->ConsumeBool();
  device.suspected_keyboard_imposter = fdp->ConsumeBool();
  device.suspected_mouse_imposter = fdp->ConsumeBool();

  fuzzer->GetUtils()->DeleteDevice(device);
}

void FuzzMixedAddDelete(FuzzedDataProvider* fdp,
                        DeviceDataManagerFuzzer* fuzzer) {
  int32_t base_id = fdp->ConsumeIntegralInRange<int32_t>(1, 100);

  InputDevice mouse(base_id, InputDeviceType::INPUT_DEVICE_USB, "fuzz_mouse");
  mouse.enabled = true;
  fuzzer->GetUtils()->AddMouseDevice(mouse);

  TouchpadDevice touchpad(base_id + 1, InputDeviceType::INPUT_DEVICE_USB,
                          "fuzz_tp");
  touchpad.enabled = true;
  fuzzer->GetUtils()->AddTouchpadDevice(touchpad);

  KeyboardDevice keyboard(base_id + 2, InputDeviceType::INPUT_DEVICE_USB,
                          "fuzz_kbd");
  keyboard.enabled = true;
  fuzzer->GetUtils()->AddKeyboardDevice(keyboard);

  InputDevice del_mouse(base_id, InputDeviceType::INPUT_DEVICE_USB, "");
  del_mouse.enabled = true;
  fuzzer->GetUtils()->DeleteDevice(del_mouse);

  InputDevice del_touchpad(base_id + 1, InputDeviceType::INPUT_DEVICE_USB, "");
  del_touchpad.enabled = true;
  fuzzer->GetUtils()->DeleteDevice(del_touchpad);

  InputDevice del_keyboard(base_id + 2, InputDeviceType::INPUT_DEVICE_USB,
                           "");
  del_keyboard.enabled = true;
  fuzzer->GetUtils()->DeleteDevice(del_keyboard);
}

void FuzzOnDeviceAdded(FuzzedDataProvider* fdp,
                       DeviceDataManagerFuzzer* fuzzer) {
  auto mock_adapter = std::make_unique<MockMMIAdapter>();
  auto* raw_adapter = mock_adapter.get();
  auto mock_utils = std::make_unique<MockArkWebDeviceDataManagerUtils>(
      fuzzer->GetDeviceDataManager());

  auto listener = std::make_unique<MockMMIListenerAdapterImpl>(
      mock_utils.get(), fuzzer->GetTaskRunner(), std::move(mock_adapter));

  uint8_t type_choice = fdp->ConsumeIntegralInRange<uint8_t>(0, 5);
  int32_t device_type = 0;
  switch (type_choice) {
    case 0:
      device_type = (1 << 2);
      break;
    case 1:
      device_type = (1 << 3);
      break;
    case 2:
      device_type = (1 << 1);
      break;
    case 3:
      device_type = (1 << 4);
      break;
    case 4:
      device_type = (1 << 1) | (1 << 2) | (1 << 3);
      break;
    default:
      device_type = fdp->ConsumeIntegral<int32_t>();
      break;
  }

  int32_t device_id = fdp->ConsumeIntegral<int32_t>();
  std::string device_name = fdp->ConsumeRandomLengthString(64);

  EXPECT_CALL(*raw_adapter, GetDeviceInfo(_, _))
      .WillOnce([device_id, device_type, device_name](
                    int32_t, std::shared_ptr<MMIDeviceInfoAdapter> info) {
        info->SetId(device_id);
        info->SetType(device_type);
        info->SetName(device_name);
        return 0;
      });

  EXPECT_CALL(*mock_utils, AddMouseDevice(_)).Times(testing::AnyNumber());
  EXPECT_CALL(*mock_utils, AddTouchpadDevice(_)).Times(testing::AnyNumber());
  EXPECT_CALL(*mock_utils, AddKeyboardDevice(_)).Times(testing::AnyNumber());

  listener->OnDeviceAdded(device_id, "change");
}

void FuzzOnDeviceRemoved(FuzzedDataProvider* fdp,
                         DeviceDataManagerFuzzer* fuzzer) {
  auto mock_adapter = std::make_unique<MockMMIAdapter>();
  auto mock_utils = std::make_unique<MockArkWebDeviceDataManagerUtils>(
      fuzzer->GetDeviceDataManager());

  auto listener = std::make_unique<MockMMIListenerAdapterImpl>(
      mock_utils.get(), fuzzer->GetTaskRunner(), std::move(mock_adapter));

  int32_t device_id = fdp->ConsumeIntegral<int32_t>();

  EXPECT_CALL(*mock_utils, DeleteDevice(_)).Times(testing::AnyNumber());

  listener->OnDeviceRemoved(device_id, "change");
}

void FuzzCleanupDeviceListeners(FuzzedDataProvider* fdp,
                                DeviceDataManagerFuzzer* fuzzer) {
  if (fdp->ConsumeBool()) {
    auto mock = std::make_unique<MockMMIAdapter>();
    testing::Mock::AllowLeak(mock.get());
    EXPECT_CALL(*mock, UnregisterDevListener(_)).Times(1);
    fuzzer->GetUtils()->mmi_adapter_ = std::move(mock);
  } else {
    fuzzer->GetUtils()->mmi_adapter_ = nullptr;
  }
  fuzzer->GetUtils()->CleanupDeviceListeners();
}

void FuzzArkWebDeviceDataManagerUtils(FuzzedDataProvider* fdp) {
  DeviceDataManagerFuzzer fuzzer;

  uint8_t num_operations = fdp->ConsumeIntegralInRange<uint8_t>(1, 20);

  for (uint8_t i = 0; i < num_operations; i++) {
    if (fdp->remaining_bytes() == 0) {
      break;
    }

    uint8_t op = fdp->ConsumeIntegralInRange<uint8_t>(0, 8);
    switch (op) {
      case 0:
        FuzzAddMouseDevice(fdp, &fuzzer);
        break;
      case 1:
        FuzzAddKeyboardDevice(fdp, &fuzzer);
        break;
      case 2:
        FuzzAddTouchpadDevice(fdp, &fuzzer);
        break;
      case 3:
        FuzzDeleteDevice(fdp, &fuzzer);
        break;
      case 4:
        FuzzDeleteExactDevice(fdp, &fuzzer);
        break;
      case 5:
        FuzzMixedAddDelete(fdp, &fuzzer);
        break;
      case 6:
        FuzzOnDeviceAdded(fdp, &fuzzer);
        break;
      case 7:
        FuzzOnDeviceRemoved(fdp, &fuzzer);
        break;
      case 8:
        FuzzCleanupDeviceListeners(fdp, &fuzzer);
        break;
    }
  }
}

}  // namespace ui

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }
  FuzzedDataProvider fdp(data, size);
  ui::FuzzArkWebDeviceDataManagerUtils(&fdp);
  return 0;
}