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

#include <dlfcn.h>
#include <gtest/gtest.h>

#include "arkweb/build/features/features.h"
#include "ui/base/pointer/pointer_device.h"
#if BUILDFLAG(ARKWEB_TEST)
#if BUILDFLAG(ARKWEB_FLING)
namespace base::ohos {
extern "C" bool PointerDeviceTestIsPcDevice();
extern "C" bool PointerDeviceTestIsTabletDevice();
extern "C" bool PointerDeviceTestIsMobileDevice();
}  // namespace base::ohos
#define IsPcDevice PointerDeviceTestIsPcDevice
#define IsTabletDevice PointerDeviceTestIsTabletDevice
#define IsMobileDevice PointerDeviceTestIsMobileDevice
#endif
extern "C" void* PointerDeviceTestDlopen(const char*, int);
extern "C" void* PointerDeviceTestDlsym(void*, const char*);
extern "C" int PointerDeviceTestDlclose(void*);
#define dlopen PointerDeviceTestDlopen
#define dlsym PointerDeviceTestDlsym
#define dlclose PointerDeviceTestDlclose
#define protected public
#include "arkweb/chromium_ext/ui/base/pointer/pointer_device_ohos.cc"
#undef dlclose
#undef dlsym
#undef dlopen
#endif

#if BUILDFLAG(ARKWEB_TEST)
namespace {
enum class DynamicLibraryBehavior { kOpenFails, kSymbolMissing, kQuery };

DynamicLibraryBehavior g_dynamic_library_behavior =
    DynamicLibraryBehavior::kOpenFails;
Input_Result g_query_result = INPUT_SUCCESS;
int32_t g_query_point_count = 0;
int g_dlclose_count = 0;

Input_Result PointerDeviceTestQueryMaxTouchPoints(int32_t* count) {
  *count = g_query_point_count;
  return g_query_result;
}
}  // namespace

extern "C" void* PointerDeviceTestDlopen(const char*, int) {
  return g_dynamic_library_behavior == DynamicLibraryBehavior::kOpenFails
             ? nullptr
             : reinterpret_cast<void*>(1);
}

extern "C" void* PointerDeviceTestDlsym(void*, const char*) {
  if (g_dynamic_library_behavior == DynamicLibraryBehavior::kSymbolMissing) {
    return nullptr;
  }
  return reinterpret_cast<void*>(&PointerDeviceTestQueryMaxTouchPoints);
}

extern "C" int PointerDeviceTestDlclose(void*) {
  ++g_dlclose_count;
  return 0;
}

#if BUILDFLAG(ARKWEB_FLING)
namespace base::ohos {
namespace {
bool g_is_pc_device = false;
bool g_is_tablet_device = false;
bool g_is_mobile_device = false;
}  // namespace

extern "C" bool PointerDeviceTestIsPcDevice() {
  return g_is_pc_device;
}

extern "C" bool PointerDeviceTestIsTabletDevice() {
  return g_is_tablet_device;
}

extern "C" bool PointerDeviceTestIsMobileDevice() {
  return g_is_mobile_device;
}
}  // namespace base::ohos
#undef IsMobileDevice
#undef IsTabletDevice
#undef IsPcDevice
#endif  // BUILDFLAG(ARKWEB_FLING)
#endif  // BUILDFLAG(ARKWEB_TEST)

namespace ui {
constexpr int TYPE_VAL = 0;
class PointerDeviceOhosTest : public testing::Test {
 public:
  void SetUp() {
    DeviceDataManager::CreateInstance();
    manager_ = DeviceDataManager::GetInstance();
    ASSERT_TRUE(manager_);
#if BUILDFLAG(ARKWEB_TEST)
    g_dynamic_library_behavior = DynamicLibraryBehavior::kOpenFails;
    g_query_result = INPUT_SUCCESS;
    g_query_point_count = 0;
    g_dlclose_count = 0;
#if BUILDFLAG(ARKWEB_FLING)
    base::ohos::g_is_pc_device = false;
    base::ohos::g_is_tablet_device = false;
    base::ohos::g_is_mobile_device = false;
#endif
#endif
  }

  void TearDown() { DeviceDataManager::DeleteInstance(); }

  DeviceDataManager* GetDeviceDataManager() { return manager_; }

 private:
  DeviceDataManager* manager_;
};

TEST_F(PointerDeviceOhosTest, MaxTouchPoints) {
  EXPECT_EQ(MaxTouchPoints(), 0);
  EXPECT_EQ(g_dlclose_count, 0);

  g_dynamic_library_behavior = DynamicLibraryBehavior::kSymbolMissing;
  EXPECT_EQ(MaxTouchPoints(), 0);
  EXPECT_EQ(g_dlclose_count, 1);

  g_dynamic_library_behavior = DynamicLibraryBehavior::kQuery;
  g_query_result = INPUT_SERVICE_EXCEPTION;
  g_query_point_count = 5;
  EXPECT_EQ(MaxTouchPoints(), 0);
  EXPECT_EQ(g_dlclose_count, 2);

  g_query_result = INPUT_SUCCESS;
  g_query_point_count = -1;
  EXPECT_EQ(MaxTouchPoints(), 0);
  EXPECT_EQ(g_dlclose_count, 3);

  g_query_point_count = 7;
  EXPECT_EQ(MaxTouchPoints(), 7);
  EXPECT_EQ(g_dlclose_count, 4);
}

TEST_F(PointerDeviceOhosTest, GetPrimaryPointerType) {
  EXPECT_EQ(GetPrimaryPointerType(POINTER_TYPE_FINE), POINTER_TYPE_FINE);
  EXPECT_EQ(GetPrimaryPointerType(POINTER_TYPE_COARSE), POINTER_TYPE_COARSE);
  EXPECT_EQ(GetPrimaryPointerType(POINTER_TYPE_NONE), POINTER_TYPE_NONE);
}

TEST_F(PointerDeviceOhosTest, GetPrimaryHoverType) {
  EXPECT_EQ(GetPrimaryHoverType(HOVER_TYPE_HOVER), HOVER_TYPE_HOVER);
  EXPECT_EQ(GetPrimaryHoverType(HOVER_TYPE_NONE), HOVER_TYPE_NONE);
}

TEST_F(PointerDeviceOhosTest, GetAvailableHoverTypes) {
  GetDeviceDataManager()->ResetDeviceListsForTest();
  TouchpadDevice device;
  device.enabled = true;
  std::vector<TouchpadDevice> touchpadDeviceList;
  touchpadDeviceList.push_back(device);
  GetDeviceDataManager()->OnTouchpadDevicesUpdated(touchpadDeviceList);
  EXPECT_EQ(GetAvailableHoverTypes(), HOVER_TYPE_HOVER);
  GetDeviceDataManager()->ResetDeviceListsForTest();

  InputDevice input;
  input.enabled = true;
  std::vector<InputDevice> inputDeviceList;
  inputDeviceList.push_back(input);
  GetDeviceDataManager()->OnMouseDevicesUpdated(inputDeviceList);
  EXPECT_EQ(GetAvailableHoverTypes(), HOVER_TYPE_HOVER);
  GetDeviceDataManager()->ResetDeviceListsForTest();

  GetDeviceDataManager()->OnPointingStickDevicesUpdated(inputDeviceList);
  EXPECT_EQ(GetAvailableHoverTypes(), HOVER_TYPE_HOVER);
  GetDeviceDataManager()->ResetDeviceListsForTest();

  EXPECT_EQ(GetAvailableHoverTypes(), HOVER_TYPE_NONE);

  device.enabled = false;
  touchpadDeviceList.clear();
  touchpadDeviceList.push_back(device);
  GetDeviceDataManager()->OnTouchpadDevicesUpdated(touchpadDeviceList);
  EXPECT_EQ(GetAvailableHoverTypes(), HOVER_TYPE_NONE);
  GetDeviceDataManager()->ResetDeviceListsForTest();

  inputDeviceList.clear();
  input.enabled = false;
  inputDeviceList.push_back(input);
  GetDeviceDataManager()->OnMouseDevicesUpdated(inputDeviceList);
  EXPECT_EQ(GetAvailableHoverTypes(), HOVER_TYPE_NONE);
  GetDeviceDataManager()->ResetDeviceListsForTest();

  GetDeviceDataManager()->OnPointingStickDevicesUpdated(inputDeviceList);
  EXPECT_EQ(GetAvailableHoverTypes(), HOVER_TYPE_NONE);
  GetDeviceDataManager()->ResetDeviceListsForTest();
}

TEST_F(PointerDeviceOhosTest, GetTouchScreensAvailability) {
  GetDeviceDataManager()->ResetDeviceListsForTest();
  EXPECT_EQ(GetTouchScreensAvailability(), TouchScreensAvailability::NONE);

  TouchscreenDevice device;
  std::vector<TouchscreenDevice> touchscreenDeviceList;
  touchscreenDeviceList.push_back(device);
  GetDeviceDataManager()->OnTouchscreenDevicesUpdated(touchscreenDeviceList);
  GetDeviceDataManager()->SetTouchscreensEnabled(true);
  EXPECT_EQ(GetTouchScreensAvailability(), TouchScreensAvailability::ENABLED);

  GetDeviceDataManager()->SetTouchscreensEnabled(false);
  EXPECT_EQ(GetTouchScreensAvailability(), TouchScreensAvailability::DISABLED);
}

TEST_F(PointerDeviceOhosTest, GetAvailablePointerTypes) {
#if BUILDFLAG(ARKWEB_FLING)
  EXPECT_EQ(GetAvailablePointerTypes(), POINTER_TYPE_NONE);

  base::ohos::g_is_pc_device = true;
  EXPECT_EQ(GetAvailablePointerTypes(), POINTER_TYPE_FINE);

  base::ohos::g_is_tablet_device = true;
  EXPECT_EQ(GetAvailablePointerTypes(),
            POINTER_TYPE_FINE | POINTER_TYPE_COARSE);

  base::ohos::g_is_pc_device = false;
  base::ohos::g_is_tablet_device = false;
  base::ohos::g_is_mobile_device = true;
  EXPECT_EQ(GetAvailablePointerTypes(), POINTER_TYPE_COARSE);
#else
  TouchpadDevice device;
  device.enabled = true;
  std::vector<TouchpadDevice> touchpadDeviceList;
  touchpadDeviceList.push_back(device);
  GetDeviceDataManager()->OnTouchpadDevicesUpdated(touchpadDeviceList);

  int atypes = TYPE_VAL;
  atypes |= POINTER_TYPE_COARSE;
  EXPECT_EQ(GetAvailablePointerTypes(), atypes);
  GetDeviceDataManager()->ResetDeviceListsForTest();

  atypes = TYPE_VAL;
  atypes |= POINTER_TYPE_COARSE;
  EXPECT_EQ(GetAvailablePointerTypes(), atypes);
#endif
}

TEST_F(PointerDeviceOhosTest, IsMouseOrTouchpadPresent001) {
  EXPECT_FALSE(IsMouseOrTouchpadPresent());
}

TEST_F(PointerDeviceOhosTest, IsMouseOrTouchpadPresent002) {
  GetDeviceDataManager()->ResetDeviceListsForTest();
  TouchpadDevice device;
  device.enabled = true;
  std::vector<TouchpadDevice> touchpadDeviceList;
  touchpadDeviceList.push_back(device);
  GetDeviceDataManager()->OnTouchpadDevicesUpdated(touchpadDeviceList);
  EXPECT_TRUE(IsMouseOrTouchpadPresent());
  GetDeviceDataManager()->ResetDeviceListsForTest();

  InputDevice input;
  input.enabled = true;
  std::vector<InputDevice> inputDeviceList;
  inputDeviceList.push_back(input);
  GetDeviceDataManager()->OnMouseDevicesUpdated(inputDeviceList);
  EXPECT_TRUE(IsMouseOrTouchpadPresent());
  GetDeviceDataManager()->ResetDeviceListsForTest();

  GetDeviceDataManager()->OnPointingStickDevicesUpdated(inputDeviceList);
  EXPECT_TRUE(IsMouseOrTouchpadPresent());
  GetDeviceDataManager()->ResetDeviceListsForTest();

  EXPECT_FALSE(IsMouseOrTouchpadPresent());

  device.enabled = false;
  touchpadDeviceList.clear();
  touchpadDeviceList.push_back(device);
  GetDeviceDataManager()->OnTouchpadDevicesUpdated(touchpadDeviceList);
  EXPECT_FALSE(IsMouseOrTouchpadPresent());
  GetDeviceDataManager()->ResetDeviceListsForTest();

  inputDeviceList.clear();
  input.enabled = false;
  inputDeviceList.push_back(input);
  GetDeviceDataManager()->OnMouseDevicesUpdated(inputDeviceList);
  EXPECT_FALSE(IsMouseOrTouchpadPresent());
  GetDeviceDataManager()->ResetDeviceListsForTest();

  GetDeviceDataManager()->OnPointingStickDevicesUpdated(inputDeviceList);
  EXPECT_FALSE(IsMouseOrTouchpadPresent());
  GetDeviceDataManager()->ResetDeviceListsForTest();
}

TEST_F(PointerDeviceOhosTest, GetAvailablePointerAndHoverTypesImpl) {
  GetDeviceDataManager()->ResetDeviceListsForTest();

  const auto types = GetAvailablePointerAndHoverTypesImpl();

  EXPECT_EQ(types.first, GetAvailablePointerTypes());
  EXPECT_EQ(types.second, GetAvailableHoverTypes());
}

TEST_F(PointerDeviceOhosTest, PointerDeviceEnumerationIsUnsupported) {
  EXPECT_FALSE(GetPointerDevice(PointerDevice::Key(1)).has_value());
  EXPECT_TRUE(GetPointerDevices().empty());
}

}  // namespace ui

#if BUILDFLAG(ARKWEB_TEST)
#undef protected
#endif
