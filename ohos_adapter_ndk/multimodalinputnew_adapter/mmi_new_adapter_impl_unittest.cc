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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api.h"
#define private public
#include "arkweb/ohos_adapter_ndk/multimodalinputnew_adapter/mmi_new_adapter_impl.h"
#undef private

using namespace testing;
using namespace OHOS::NWeb;
using namespace MockNdkApi;

class MMINewAdapterImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    g_mock_OH_Input_RegisterDeviceListener = [](Input_DeviceListener*) {
      return INPUT_SUCCESS;
    };
    g_mock_OH_Input_UnregisterDeviceListener = [](Input_DeviceListener*) {
      return INPUT_SUCCESS;
    };
    ResetSingleton();
  }

  void TearDown() override {
    g_mock_OH_Input_UnregisterDeviceListener = [](Input_DeviceListener*) {
      return INPUT_SUCCESS;
    };
    ResetSingleton();
    g_mock_OH_Input_CreateDeviceInfo = nullptr;
    g_mock_OH_Input_GetDeviceIds = nullptr;
    g_mock_OH_Input_GetDevice = nullptr;
    g_mock_OH_Input_DestroyDeviceInfo = nullptr;
    g_mock_OH_Input_GetKeyboardType = nullptr;
    g_mock_OH_Input_GetCapabilities = nullptr;
    g_mock_OH_Input_RegisterDeviceListener = nullptr;
    g_mock_OH_Input_UnregisterDeviceListener = nullptr;
    g_mock_OH_Input_GetDeviceId = nullptr;
    g_mock_OH_Input_GetDeviceVersion = nullptr;
    g_mock_OH_Input_GetDeviceProduct = nullptr;
    g_mock_OH_Input_GetDeviceVendor = nullptr;
    g_mock_OH_Input_GetDeviceName = nullptr;
    g_mock_OH_Input_GetDeviceAddress = nullptr;
  }

  void ResetSingleton() {
    auto& adapter = MMINewAdapterImpl::GetInstance();
    adapter.inputDeviceAdapterListener_.reset();
    adapter.inputDeviceListener_ = {};
    adapter.type_.clear();
  }
};

namespace OHOS::NWeb {
void OnDeviceAdded(int32_t deviceId);
void OnDeviceRemoved(int32_t deviceId);
}

class MockMMIListenerAdapter : public MMIListenerAdapter {
 public:
  MOCK_METHOD(void, OnDeviceAdded, (int32_t deviceId, const std::string& type), (override));
  MOCK_METHOD(void, OnDeviceRemoved, (int32_t deviceId, const std::string& type), (override));
};

class MockMMIDeviceInfoAdapter : public MMIDeviceInfoAdapter {
 public:
  MOCK_METHOD(int32_t, GetId, (), (override));
  MOCK_METHOD(int32_t, GetType, (), (override));
  MOCK_METHOD(int32_t, GetBus, (), (override));
  MOCK_METHOD(int32_t, GetVersion, (), (override));
  MOCK_METHOD(int32_t, GetProduct, (), (override));
  MOCK_METHOD(int32_t, GetVendor, (), (override));
  MOCK_METHOD(std::string, GetName, (), (override));
  MOCK_METHOD(std::string, GetPhys, (), (override));
  MOCK_METHOD(std::string, GetUniq, (), (override));
  MOCK_METHOD(void, SetId, (int32_t id), (override));
  MOCK_METHOD(void, SetType, (int32_t type), (override));
  MOCK_METHOD(void, SetBus, (int32_t bus), (override));
  MOCK_METHOD(void, SetVersion, (int32_t version), (override));
  MOCK_METHOD(void, SetProduct, (int32_t product), (override));
  MOCK_METHOD(void, SetVendor, (int32_t vendor), (override));
  MOCK_METHOD(void, SetName, (std::string name), (override));
  MOCK_METHOD(void, SetPhys, (std::string phys), (override));
  MOCK_METHOD(void, SetUniq, (std::string uniq), (override));
};

/**
 * @tc.name: MMINewAdapterImplTest_OnDeviceAdded_001.
 * @tc.desc: test of OnDeviceAdded in MMINewAdapterImplTest
 * @tc.type: FUNC.
 */
TEST_F(MMINewAdapterImplTest, MMINewAdapterImplTest_OnDeviceAdded_001) {
  int32_t deviceId = 0;
  std::string type;
  auto listener = std::make_shared<MockMMIListenerAdapter>();
  MMINewAdapterImpl::GetInstance().RegisterDevListener(type, listener);
  bool callback_called = false;
  EXPECT_CALL(*listener, OnDeviceAdded(::testing::_, ::testing::_))
      .WillRepeatedly(::testing::Assign(&callback_called, true));
  OnDeviceAdded(deviceId);
  EXPECT_EQ(callback_called, true);
}

/**
 * @tc.name: MMINewAdapterImplTest_OnDeviceRemoved_001.
 * @tc.desc: test of OnDeviceRemoved in MMINewAdapterImplTest
 * @tc.type: FUNC.
 */
TEST_F(MMINewAdapterImplTest, MMINewAdapterImplTest_OnDeviceRemoved_001) {
  std::string type;
  auto listener = std::make_shared<MockMMIListenerAdapter>();
  MMINewAdapterImpl::GetInstance().RegisterDevListener(type, listener);
  int32_t deviceId = 0;
  bool callback_called = false;
  EXPECT_CALL(*listener, OnDeviceRemoved(::testing::_, ::testing::_))
      .WillRepeatedly(::testing::Assign(&callback_called, true));
  OnDeviceRemoved(deviceId);
  EXPECT_EQ(callback_called, true);
}

/**
 * @tc.name: MMINewAdapterImplTest_RegisterDevListener_001.
 * @tc.desc: test of RegisterDevListener in MMINewAdapterImplTest
 * @tc.type: FUNC.
 */
TEST_F(MMINewAdapterImplTest, MMINewAdapterImplTest_RegisterDevListener_001) {
  std::string type;
  std::shared_ptr<MockMMIListenerAdapter> listener = nullptr;
  auto result = MMINewAdapterImpl::GetInstance().RegisterDevListener(type, listener);
  EXPECT_EQ(result, -1);

  listener = std::make_shared<MockMMIListenerAdapter>();
  result = MMINewAdapterImpl::GetInstance().RegisterDevListener(type, listener);
  EXPECT_EQ(result, 0);

  MMINewAdapterImpl::GetInstance().UnregisterDevListener(type);
  g_mock_OH_Input_RegisterDeviceListener = [](Input_DeviceListener*) {
    return INPUT_SERVICE_EXCEPTION;
  };
  result = MMINewAdapterImpl::GetInstance().RegisterDevListener(type, listener);
  EXPECT_EQ(result, INPUT_SERVICE_EXCEPTION);
  ResetSingleton();
}

/**
 * @tc.name: MMINewAdapterImplTest_UnregisterDevListener_001.
 * @tc.desc: test of UnregisterDevListener in MMINewAdapterImplTest
 * @tc.type: FUNC.
 */
TEST_F(MMINewAdapterImplTest, MMINewAdapterImplTest_UnregisterDevListener_001) {
  std::string type = "test";
  auto listener = std::make_shared<MockMMIListenerAdapter>();
  ASSERT_EQ(
      MMINewAdapterImpl::GetInstance().RegisterDevListener(type, listener),
      INPUT_SUCCESS);
  auto result = MMINewAdapterImpl::GetInstance().UnregisterDevListener(type);
  EXPECT_EQ(result, INPUT_SUCCESS);
  ResetSingleton();

  ASSERT_EQ(
      MMINewAdapterImpl::GetInstance().RegisterDevListener(type, listener),
      INPUT_SUCCESS);
  g_mock_OH_Input_UnregisterDeviceListener = [](Input_DeviceListener*) {
    return INPUT_SERVICE_EXCEPTION;
  };
  result = MMINewAdapterImpl::GetInstance().UnregisterDevListener(type);
  EXPECT_EQ(result, INPUT_SERVICE_EXCEPTION);
  EXPECT_EQ(MMINewAdapterImpl::GetInstance().GetDeviceAdapterListener(),
            listener);
  EXPECT_EQ(MMINewAdapterImpl::GetInstance().GetType(), type);

  g_mock_OH_Input_UnregisterDeviceListener = [](Input_DeviceListener*) {
    return INPUT_SUCCESS;
  };
  EXPECT_EQ(MMINewAdapterImpl::GetInstance().UnregisterDevListener(type),
            INPUT_SUCCESS);
  ResetSingleton();
  result = MMINewAdapterImpl::GetInstance().UnregisterDevListener(type);
  EXPECT_EQ(result, -1);
}

/**
 * @tc.name: MMINewAdapterImplTest_GetDeviceIds_001.
 * @tc.desc: test of GetDeviceIds in MMINewAdapterImplTest
 * @tc.type: FUNC.
 */
TEST_F(MMINewAdapterImplTest, MMINewAdapterImplTest_GetDeviceIds_001) {
  std::vector<int32_t> ids;

  g_mock_OH_Input_GetDeviceIds = [](int32_t *deviceIds, int32_t inSize, int32_t *outSize) {
    return INPUT_PERMISSION_DENIED;
  };
  auto result = MMINewAdapterImpl::GetInstance().GetDeviceIds(ids);
  EXPECT_EQ(result, -1);

  g_mock_OH_Input_GetDeviceIds = [](int32_t *deviceIds, int32_t inSize, int32_t *outSize) {
    *outSize = 0;
    return INPUT_SUCCESS;
  };
  result = MMINewAdapterImpl::GetInstance().GetDeviceIds(ids);
  EXPECT_EQ(result, 0);

  g_mock_OH_Input_GetDeviceIds = [](int32_t*, int32_t, int32_t* outSize) {
    *outSize = 0;
    return INPUT_SUCCESS;
  };
  result = MMINewAdapterImpl::GetInstance().GetDeviceIds(ids);
  EXPECT_EQ(result, 0);
}

/**
 * @tc.name: MMINewAdapterImplTest_GetDeviceInfo_001.
 * @tc.desc: test of GetDeviceInfo in MMINewAdapterImplTest
 * @tc.type: FUNC.
 */
TEST_F(MMINewAdapterImplTest, MMINewAdapterImplTest_GetDeviceInfo_001) {
  int32_t deviceId = 0;
  std::shared_ptr<MockMMIDeviceInfoAdapter> info = nullptr;
  auto result = MMINewAdapterImpl::GetInstance().GetDeviceInfo(deviceId, info);
  EXPECT_EQ(result, -1);

  info = std::make_shared<testing::NiceMock<MockMMIDeviceInfoAdapter>>();
  g_mock_OH_Input_CreateDeviceInfo = []() {
    return nullptr;
  };
  result = MMINewAdapterImpl::GetInstance().GetDeviceInfo(deviceId, info);
  EXPECT_EQ(result, -1);

  auto* fake_device_info = reinterpret_cast<Input_DeviceInfo*>(1);
  g_mock_OH_Input_CreateDeviceInfo = [fake_device_info]() {
    return fake_device_info;
  };
  bool destroy_called = false;
  g_mock_OH_Input_DestroyDeviceInfo =
      [&destroy_called](Input_DeviceInfo** device_info) {
        destroy_called = true;
        *device_info = nullptr;
      };
  g_mock_OH_Input_GetDevice = [fake_device_info](
                                  int32_t, Input_DeviceInfo** device_info) {
    *device_info = fake_device_info;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetDeviceId = [](Input_DeviceInfo*, int32_t* id) {
    *id = 7;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetDeviceVersion = [](Input_DeviceInfo*, int32_t* version) {
    *version = 8;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetDeviceProduct = [](Input_DeviceInfo*, int32_t* product) {
    *product = 9;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetDeviceVendor = [](Input_DeviceInfo*, int32_t* vendor) {
    *vendor = 10;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetDeviceName = [](Input_DeviceInfo*, char** name) {
    static char value[] = "keyboard";
    *name = value;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetDeviceAddress = [](Input_DeviceInfo*, char** address) {
    static char value[] = "usb-1";
    *address = value;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetCapabilities = [](Input_DeviceInfo*,
                                       int32_t* capabilities) {
    *capabilities = 3;
    return INPUT_SUCCESS;
  };
  EXPECT_CALL(*info, SetId(7));
  EXPECT_CALL(*info, SetType(3));
  EXPECT_CALL(*info, SetVersion(8));
  EXPECT_CALL(*info, SetProduct(9));
  EXPECT_CALL(*info, SetVendor(10));
  EXPECT_CALL(*info, SetName("keyboard"));
  EXPECT_CALL(*info, SetPhys("usb-1"));
  result = MMINewAdapterImpl::GetInstance().GetDeviceInfo(deviceId, info);
  EXPECT_EQ(result, 0);
  EXPECT_TRUE(destroy_called);
}

TEST_F(MMINewAdapterImplTest, DeprecatedMethodsReturnDefaults) {
  MMINewAdapterImpl& adapter = MMINewAdapterImpl::GetInstance();
  EXPECT_EQ(adapter.KeyCodeToString(1), nullptr);
  EXPECT_EQ(adapter.RegisterMMIInputListener(nullptr), 0);
  EXPECT_NO_FATAL_FAILURE(adapter.UnregisterMMIInputListener(1));
  EXPECT_EQ(adapter.GetMaxTouchPoints(), 0);
  EXPECT_EQ(&adapter, &MMINewAdapterImpl::GetInstance());
}

TEST_F(MMINewAdapterImplTest, GetDeviceIdsCopiesSuccessfulResult) {
  std::vector<int32_t> ids = {99};
  g_mock_OH_Input_GetDeviceIds = [](int32_t* device_ids, int32_t in_size,
                                    int32_t* out_size) {
    EXPECT_GE(in_size, 2);
    device_ids[0] = 7;
    device_ids[1] = 8;
    *out_size = 2;
    return INPUT_SUCCESS;
  };

  EXPECT_EQ(MMINewAdapterImpl::GetInstance().GetDeviceIds(ids), 0);
  EXPECT_THAT(ids, ElementsAre(7, 8));
}

TEST_F(MMINewAdapterImplTest, HasKeyboardAndMouseRejectsInvalidDeviceCounts) {
  g_mock_OH_Input_GetDeviceIds = [](int32_t*, int32_t, int32_t*) {
    return INPUT_PERMISSION_DENIED;
  };
  EXPECT_FALSE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());

  g_mock_OH_Input_GetDeviceIds = [](int32_t*, int32_t, int32_t* out_size) {
    *out_size = -1;
    return INPUT_SUCCESS;
  };
  EXPECT_FALSE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());

  g_mock_OH_Input_GetDeviceIds = [](int32_t*, int32_t in_size,
                                    int32_t* out_size) {
    *out_size = in_size + 1;
    return INPUT_SUCCESS;
  };
  EXPECT_FALSE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());
}

TEST_F(MMINewAdapterImplTest, HasKeyboardAndMouseClassifiesSeparateDevices) {
  int fake_device_info = 0;
  int32_t current_device_id = -1;
  g_mock_OH_Input_GetDeviceIds = [](int32_t* device_ids, int32_t,
                                    int32_t* out_size) {
    device_ids[0] = 10;
    device_ids[1] = 20;
    *out_size = 2;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_CreateDeviceInfo = [&fake_device_info]() {
    return reinterpret_cast<Input_DeviceInfo*>(&fake_device_info);
  };
  g_mock_OH_Input_GetDevice = [&current_device_id](int32_t device_id,
                                                   Input_DeviceInfo**) {
    current_device_id = device_id;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetCapabilities =
      [&current_device_id](Input_DeviceInfo*, int32_t* capabilities) {
        *capabilities = current_device_id == 10 ? 1 : 2;
        return INPUT_SUCCESS;
      };
  g_mock_OH_Input_GetKeyboardType = [](int32_t, int32_t* keyboard_type) {
    *keyboard_type = KEYBOARD_TYPE_ALPHABETIC;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_DestroyDeviceInfo = [](Input_DeviceInfo** device_info) {
    *device_info = nullptr;
  };

  EXPECT_TRUE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());
}

TEST_F(MMINewAdapterImplTest,
       HasKeyboardAndMouseReassignsCombinedDevicePointer) {
  int fake_device_info = 0;
  int32_t current_device_id = -1;
  g_mock_OH_Input_GetDeviceIds = [](int32_t* device_ids, int32_t,
                                    int32_t* out_size) {
    device_ids[0] = 1;
    device_ids[1] = 2;
    *out_size = 2;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_CreateDeviceInfo = [&fake_device_info]() {
    return reinterpret_cast<Input_DeviceInfo*>(&fake_device_info);
  };
  g_mock_OH_Input_GetDevice = [&current_device_id](int32_t device_id,
                                                   Input_DeviceInfo**) {
    current_device_id = device_id;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetCapabilities =
      [&current_device_id](Input_DeviceInfo*, int32_t* capabilities) {
        *capabilities = current_device_id == 1 ? 3 : 2;
        return INPUT_SUCCESS;
      };
  g_mock_OH_Input_GetKeyboardType = [](int32_t, int32_t* keyboard_type) {
    *keyboard_type = KEYBOARD_TYPE_ALPHABETIC;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_DestroyDeviceInfo = [](Input_DeviceInfo** device_info) {
    *device_info = nullptr;
  };

  EXPECT_TRUE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());
}

TEST_F(MMINewAdapterImplTest, HasKeyboardAndMouseRejectsDeviceApiFailures) {
  int fake_device_info = 0;
  g_mock_OH_Input_GetDeviceIds = [](int32_t* device_ids, int32_t,
                                    int32_t* out_size) {
    device_ids[0] = 7;
    *out_size = 1;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_DestroyDeviceInfo = [](Input_DeviceInfo** device_info) {
    *device_info = nullptr;
  };

  g_mock_OH_Input_CreateDeviceInfo = []() { return nullptr; };
  EXPECT_FALSE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());

  g_mock_OH_Input_CreateDeviceInfo = [&fake_device_info]() {
    return reinterpret_cast<Input_DeviceInfo*>(&fake_device_info);
  };
  g_mock_OH_Input_GetDevice = [](int32_t, Input_DeviceInfo**) {
    return INPUT_PARAMETER_ERROR;
  };
  EXPECT_FALSE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());

  g_mock_OH_Input_GetDevice = [](int32_t, Input_DeviceInfo**) {
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetCapabilities = [](Input_DeviceInfo*, int32_t*) {
    return INPUT_PARAMETER_ERROR;
  };
  EXPECT_FALSE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());

  g_mock_OH_Input_GetCapabilities = [](Input_DeviceInfo*,
                                       int32_t* capabilities) {
    *capabilities = -1;
    return INPUT_SUCCESS;
  };
  EXPECT_FALSE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());

  g_mock_OH_Input_GetCapabilities = [](Input_DeviceInfo*,
                                       int32_t* capabilities) {
    *capabilities = 1;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetKeyboardType = [](int32_t, int32_t*) {
    return INPUT_PARAMETER_ERROR;
  };
  EXPECT_FALSE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());
}

TEST_F(MMINewAdapterImplTest, HasKeyboardAndMouseAcceptsKeyboardTypes) {
  int fake_device_info = 0;
  int32_t current_device_id = -1;
  int32_t selected_keyboard_type = KEYBOARD_TYPE_DIGITAL;
  g_mock_OH_Input_GetDeviceIds = [](int32_t* device_ids, int32_t,
                                    int32_t* out_size) {
    device_ids[0] = 1;
    device_ids[1] = 2;
    *out_size = 2;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_CreateDeviceInfo = [&fake_device_info]() {
    return reinterpret_cast<Input_DeviceInfo*>(&fake_device_info);
  };
  g_mock_OH_Input_GetDevice = [&current_device_id](int32_t device_id,
                                                   Input_DeviceInfo**) {
    current_device_id = device_id;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_GetCapabilities =
      [&current_device_id](Input_DeviceInfo*, int32_t* capabilities) {
        *capabilities = current_device_id == 1 ? 1 : 2;
        return INPUT_SUCCESS;
      };
  g_mock_OH_Input_GetKeyboardType = [&selected_keyboard_type](
                                        int32_t, int32_t* keyboard_type) {
    *keyboard_type = selected_keyboard_type;
    return INPUT_SUCCESS;
  };
  g_mock_OH_Input_DestroyDeviceInfo = [](Input_DeviceInfo** device_info) {
    *device_info = nullptr;
  };

  EXPECT_TRUE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());
  selected_keyboard_type = KEYBOARD_TYPE_REMOTE_CONTROL;
  EXPECT_TRUE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());
  selected_keyboard_type = KEYBOARD_TYPE_UNKNOWN;
  EXPECT_FALSE(MMINewAdapterImpl::GetInstance().HasKeyboardAndMouse());
}
