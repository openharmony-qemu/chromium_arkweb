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

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "gamepad_adapter_impl.h"
#include "device/gamepad/gamepad_ohos_layout.h"
#include "gamepad_platform_data_fetcher_bridge.h"
#include "multimodalinput/oh_key_code.h"

#include "arkweb/ohos_nweb/src/nweb_hilog.h"

namespace OHOS::NWeb {

namespace {

constexpr double kAxisEpsilon = 0.001;
constexpr int kEnumerateRetryDelayMs = 500;

std::mutex g_adapter_mutex;
std::unordered_map<std::string, int32_t> g_device_id_map;
std::atomic<int32_t> g_next_device_id{0};

// On watch devices libohgame_controller.z.so does not exist; dlopen at runtime
// so the web engine can still load (gamepad becomes a no-op).

using RegisterDeviceMonitorFunc =
    GameController_ErrorCode (*)(GameDevice_DeviceMonitorCallback);
using UnregisterFunc = GameController_ErrorCode (*)(void);
using RegisterButtonFunc =
    GameController_ErrorCode (*)(GamePad_ButtonInputMonitorCallback);
using RegisterAxisFunc =
    GameController_ErrorCode (*)(GamePad_AxisInputMonitorCallback);
using DeviceEventGetChangedTypeFunc =
    GameController_ErrorCode (*)(const GameDevice_DeviceEvent*, GameDevice_StatusChangedType*);
using DeviceEventGetDeviceInfoFunc =
    GameController_ErrorCode (*)(const GameDevice_DeviceEvent*, GameDevice_DeviceInfo**);
using DeviceInfoGetDeviceTypeFunc =
    GameController_ErrorCode (*)(const GameDevice_DeviceInfo*, GameDevice_DeviceType*);
using DeviceInfoGetStringFunc =
    GameController_ErrorCode (*)(const GameDevice_DeviceInfo*, char**);
using DeviceInfoGetProductFunc =
    GameController_ErrorCode (*)(const GameDevice_DeviceInfo*, int32_t*);
using DestroyDeviceInfoFunc = GameController_ErrorCode (*)(GameDevice_DeviceInfo**);
using GetAllDeviceInfosFunc = GameController_ErrorCode (*)(GameDevice_AllDeviceInfos**);
using AllDeviceInfosGetCountFunc =
    GameController_ErrorCode (*)(const GameDevice_AllDeviceInfos*, int32_t*);
using AllDeviceInfosGetDeviceInfoFunc =
    GameController_ErrorCode (*)(const GameDevice_AllDeviceInfos*, const int32_t, GameDevice_DeviceInfo**);
using DestroyAllDeviceInfosFunc = GameController_ErrorCode (*)(GameDevice_AllDeviceInfos**);
using ButtonEventGetDeviceIdFunc =
    GameController_ErrorCode (*)(const GamePad_ButtonEvent*, char**);
using ButtonEventGetButtonActionFunc =
    GameController_ErrorCode (*)(const GamePad_ButtonEvent*, GamePad_Button_ActionType*);
using ButtonEventGetButtonCodeFunc =
    GameController_ErrorCode (*)(const GamePad_ButtonEvent*, int32_t*);
using AxisEventGetDeviceIdFunc =
    GameController_ErrorCode (*)(const GamePad_AxisEvent*, char**);
using AxisEventGetAxisSourceTypeFunc =
    GameController_ErrorCode (*)(const GamePad_AxisEvent*, GamePad_AxisSourceType*);
using AxisEventGetDoubleFunc =
    GameController_ErrorCode (*)(const GamePad_AxisEvent*, double*);

struct GameControllerFuncs {
  RegisterDeviceMonitorFunc registerDeviceMonitor = nullptr;
  UnregisterFunc unregisterDeviceMonitor = nullptr;
  DeviceEventGetChangedTypeFunc deviceEventGetChangedType = nullptr;
  DeviceEventGetDeviceInfoFunc deviceEventGetDeviceInfo = nullptr;
  DeviceInfoGetDeviceTypeFunc deviceInfoGetDeviceType = nullptr;
  DeviceInfoGetStringFunc deviceInfoGetDeviceId = nullptr;
  DeviceInfoGetStringFunc deviceInfoGetName = nullptr;
  DeviceInfoGetProductFunc deviceInfoGetProduct = nullptr;
  DestroyDeviceInfoFunc destroyDeviceInfo = nullptr;
  GetAllDeviceInfosFunc getAllDeviceInfos = nullptr;
  AllDeviceInfosGetCountFunc allDeviceInfosGetCount = nullptr;
  AllDeviceInfosGetDeviceInfoFunc allDeviceInfosGetDeviceInfo = nullptr;
  DestroyAllDeviceInfosFunc destroyAllDeviceInfos = nullptr;
  RegisterButtonFunc registerButtonA = nullptr;
  UnregisterFunc unregisterButtonA = nullptr;
  RegisterButtonFunc registerButtonB = nullptr;
  UnregisterFunc unregisterButtonB = nullptr;
  RegisterButtonFunc registerButtonX = nullptr;
  UnregisterFunc unregisterButtonX = nullptr;
  RegisterButtonFunc registerButtonY = nullptr;
  UnregisterFunc unregisterButtonY = nullptr;
  RegisterButtonFunc registerButtonMenu = nullptr;
  UnregisterFunc unregisterButtonMenu = nullptr;
  RegisterButtonFunc registerButtonHome = nullptr;
  UnregisterFunc unregisterButtonHome = nullptr;
  RegisterButtonFunc registerLeftShoulder = nullptr;
  UnregisterFunc unregisterLeftShoulder = nullptr;
  RegisterButtonFunc registerRightShoulder = nullptr;
  UnregisterFunc unregisterRightShoulder = nullptr;
  RegisterButtonFunc registerLeftTrigger = nullptr;
  UnregisterFunc unregisterLeftTrigger = nullptr;
  RegisterButtonFunc registerRightTrigger = nullptr;
  UnregisterFunc unregisterRightTrigger = nullptr;
  RegisterButtonFunc registerLeftThumbstick = nullptr;
  UnregisterFunc unregisterLeftThumbstick = nullptr;
  RegisterButtonFunc registerRightThumbstick = nullptr;
  UnregisterFunc unregisterRightThumbstick = nullptr;
  RegisterButtonFunc registerDpadUp = nullptr;
  UnregisterFunc unregisterDpadUp = nullptr;
  RegisterButtonFunc registerDpadDown = nullptr;
  UnregisterFunc unregisterDpadDown = nullptr;
  RegisterButtonFunc registerDpadLeft = nullptr;
  UnregisterFunc unregisterDpadLeft = nullptr;
  RegisterButtonFunc registerDpadRight = nullptr;
  UnregisterFunc unregisterDpadRight = nullptr;
  RegisterAxisFunc registerAxisLeftThumbstick = nullptr;
  UnregisterFunc unregisterAxisLeftThumbstick = nullptr;
  RegisterAxisFunc registerAxisRightThumbstick = nullptr;
  UnregisterFunc unregisterAxisRightThumbstick = nullptr;
  RegisterAxisFunc registerAxisLeftTrigger = nullptr;
  UnregisterFunc unregisterAxisLeftTrigger = nullptr;
  RegisterAxisFunc registerAxisRightTrigger = nullptr;
  UnregisterFunc unregisterAxisRightTrigger = nullptr;
  RegisterAxisFunc registerAxisDpad = nullptr;
  UnregisterFunc unregisterAxisDpad = nullptr;
  ButtonEventGetDeviceIdFunc buttonEventGetDeviceId = nullptr;
  ButtonEventGetButtonActionFunc buttonEventGetButtonAction = nullptr;
  ButtonEventGetButtonCodeFunc buttonEventGetButtonCode = nullptr;
  AxisEventGetDeviceIdFunc axisEventGetDeviceId = nullptr;
  AxisEventGetAxisSourceTypeFunc axisEventGetAxisSourceType = nullptr;
  AxisEventGetDoubleFunc axisEventGetXAxisValue = nullptr;
  AxisEventGetDoubleFunc axisEventGetYAxisValue = nullptr;
  AxisEventGetDoubleFunc axisEventGetZAxisValue = nullptr;
  AxisEventGetDoubleFunc axisEventGetRZAxisValue = nullptr;
  AxisEventGetDoubleFunc axisEventGetBrakeAxisValue = nullptr;
  AxisEventGetDoubleFunc axisEventGetGasAxisValue = nullptr;
};

GameControllerFuncs g_gc_funcs;
// Atomic: read concurrently from FFRT callback threads.
std::atomic<bool> g_so_loaded{false};

#define LOAD_FUNC(member, type, dl, sym) \
  g_gc_funcs.member = reinterpret_cast<type>(dlsym(dl, sym))

#define LOAD_BUTTON_PAIR(dl, member, sym_prefix)                           \
  LOAD_FUNC(register##member, RegisterButtonFunc, dl,                     \
            sym_prefix "_RegisterButtonInputMonitor");                     \
  LOAD_FUNC(unregister##member, UnregisterFunc, dl,                       \
            sym_prefix "_UnregisterButtonInputMonitor")

#define LOAD_AXIS_PAIR(dl, member, sym_prefix)                            \
  LOAD_FUNC(registerAxis##member, RegisterAxisFunc, dl,                   \
            sym_prefix "_RegisterAxisInputMonitor");                       \
  LOAD_FUNC(unregisterAxis##member, UnregisterFunc, dl,                   \
            sym_prefix "_UnregisterAxisInputMonitor")

void LoadAllFuncs(void* dl) {
  LOAD_FUNC(registerDeviceMonitor, RegisterDeviceMonitorFunc, dl, "OH_GameDevice_RegisterDeviceMonitor");
  LOAD_FUNC(unregisterDeviceMonitor, UnregisterFunc, dl, "OH_GameDevice_UnregisterDeviceMonitor");
  LOAD_FUNC(deviceEventGetChangedType, DeviceEventGetChangedTypeFunc, dl, "OH_GameDevice_DeviceEvent_GetChangedType");
  LOAD_FUNC(deviceEventGetDeviceInfo, DeviceEventGetDeviceInfoFunc, dl, "OH_GameDevice_DeviceEvent_GetDeviceInfo");
  LOAD_FUNC(deviceInfoGetDeviceType, DeviceInfoGetDeviceTypeFunc, dl, "OH_GameDevice_DeviceInfo_GetDeviceType");
  LOAD_FUNC(deviceInfoGetDeviceId, DeviceInfoGetStringFunc, dl, "OH_GameDevice_DeviceInfo_GetDeviceId");
  LOAD_FUNC(deviceInfoGetName, DeviceInfoGetStringFunc, dl, "OH_GameDevice_DeviceInfo_GetName");
  LOAD_FUNC(deviceInfoGetProduct, DeviceInfoGetProductFunc, dl, "OH_GameDevice_DeviceInfo_GetProduct");
  LOAD_FUNC(destroyDeviceInfo, DestroyDeviceInfoFunc, dl, "OH_GameDevice_DestroyDeviceInfo");
  LOAD_FUNC(getAllDeviceInfos, GetAllDeviceInfosFunc, dl, "OH_GameDevice_GetAllDeviceInfos");
  LOAD_FUNC(allDeviceInfosGetCount, AllDeviceInfosGetCountFunc, dl, "OH_GameDevice_AllDeviceInfos_GetCount");
  LOAD_FUNC(allDeviceInfosGetDeviceInfo, AllDeviceInfosGetDeviceInfoFunc, dl,
            "OH_GameDevice_AllDeviceInfos_GetDeviceInfo");
  LOAD_FUNC(destroyAllDeviceInfos, DestroyAllDeviceInfosFunc, dl, "OH_GameDevice_DestroyAllDeviceInfos");
  LOAD_BUTTON_PAIR(dl, ButtonA, "OH_GamePad_ButtonA");
  LOAD_BUTTON_PAIR(dl, ButtonB, "OH_GamePad_ButtonB");
  LOAD_BUTTON_PAIR(dl, ButtonX, "OH_GamePad_ButtonX");
  LOAD_BUTTON_PAIR(dl, ButtonY, "OH_GamePad_ButtonY");
  LOAD_BUTTON_PAIR(dl, ButtonMenu, "OH_GamePad_ButtonMenu");
  LOAD_BUTTON_PAIR(dl, ButtonHome, "OH_GamePad_ButtonHome");
  LOAD_BUTTON_PAIR(dl, LeftShoulder, "OH_GamePad_LeftShoulder");
  LOAD_BUTTON_PAIR(dl, RightShoulder, "OH_GamePad_RightShoulder");
  LOAD_BUTTON_PAIR(dl, LeftTrigger, "OH_GamePad_LeftTrigger");
  LOAD_BUTTON_PAIR(dl, RightTrigger, "OH_GamePad_RightTrigger");
  LOAD_BUTTON_PAIR(dl, LeftThumbstick, "OH_GamePad_LeftThumbstick");
  LOAD_BUTTON_PAIR(dl, RightThumbstick, "OH_GamePad_RightThumbstick");
  LOAD_BUTTON_PAIR(dl, DpadUp, "OH_GamePad_Dpad_UpButton");
  LOAD_BUTTON_PAIR(dl, DpadDown, "OH_GamePad_Dpad_DownButton");
  LOAD_BUTTON_PAIR(dl, DpadLeft, "OH_GamePad_Dpad_LeftButton");
  LOAD_BUTTON_PAIR(dl, DpadRight, "OH_GamePad_Dpad_RightButton");
  LOAD_AXIS_PAIR(dl, LeftThumbstick, "OH_GamePad_LeftThumbstick");
  LOAD_AXIS_PAIR(dl, RightThumbstick, "OH_GamePad_RightThumbstick");
  LOAD_AXIS_PAIR(dl, LeftTrigger, "OH_GamePad_LeftTrigger");
  LOAD_AXIS_PAIR(dl, RightTrigger, "OH_GamePad_RightTrigger");
  LOAD_AXIS_PAIR(dl, Dpad, "OH_GamePad_Dpad");
  LOAD_FUNC(buttonEventGetDeviceId, ButtonEventGetDeviceIdFunc, dl, "OH_GamePad_ButtonEvent_GetDeviceId");
  LOAD_FUNC(buttonEventGetButtonAction, ButtonEventGetButtonActionFunc, dl, "OH_GamePad_ButtonEvent_GetButtonAction");
  LOAD_FUNC(buttonEventGetButtonCode, ButtonEventGetButtonCodeFunc, dl, "OH_GamePad_ButtonEvent_GetButtonCode");
  LOAD_FUNC(axisEventGetDeviceId, AxisEventGetDeviceIdFunc, dl, "OH_GamePad_AxisEvent_GetDeviceId");
  LOAD_FUNC(axisEventGetAxisSourceType, AxisEventGetAxisSourceTypeFunc, dl, "OH_GamePad_AxisEvent_GetAxisSourceType");
  LOAD_FUNC(axisEventGetXAxisValue, AxisEventGetDoubleFunc, dl, "OH_GamePad_AxisEvent_GetXAxisValue");
  LOAD_FUNC(axisEventGetYAxisValue, AxisEventGetDoubleFunc, dl, "OH_GamePad_AxisEvent_GetYAxisValue");
  LOAD_FUNC(axisEventGetZAxisValue, AxisEventGetDoubleFunc, dl, "OH_GamePad_AxisEvent_GetZAxisValue");
  LOAD_FUNC(axisEventGetRZAxisValue, AxisEventGetDoubleFunc, dl, "OH_GamePad_AxisEvent_GetRZAxisValue");
  LOAD_FUNC(axisEventGetBrakeAxisValue, AxisEventGetDoubleFunc, dl, "OH_GamePad_AxisEvent_GetBrakeAxisValue");
  LOAD_FUNC(axisEventGetGasAxisValue, AxisEventGetDoubleFunc, dl, "OH_GamePad_AxisEvent_GetGasAxisValue");
}

#undef LOAD_FUNC
#undef LOAD_BUTTON_PAIR
#undef LOAD_AXIS_PAIR

bool ValidateFuncs() {
  return g_gc_funcs.registerDeviceMonitor && g_gc_funcs.unregisterDeviceMonitor &&
         g_gc_funcs.deviceEventGetChangedType && g_gc_funcs.deviceEventGetDeviceInfo &&
         g_gc_funcs.deviceInfoGetDeviceType && g_gc_funcs.deviceInfoGetDeviceId &&
         g_gc_funcs.deviceInfoGetName && g_gc_funcs.deviceInfoGetProduct &&
         g_gc_funcs.destroyDeviceInfo && g_gc_funcs.getAllDeviceInfos &&
         g_gc_funcs.allDeviceInfosGetCount && g_gc_funcs.allDeviceInfosGetDeviceInfo &&
         g_gc_funcs.destroyAllDeviceInfos &&
         g_gc_funcs.registerButtonA && g_gc_funcs.unregisterButtonA &&
         g_gc_funcs.registerButtonB && g_gc_funcs.unregisterButtonB &&
         g_gc_funcs.registerButtonX && g_gc_funcs.unregisterButtonX &&
         g_gc_funcs.registerButtonY && g_gc_funcs.unregisterButtonY &&
         g_gc_funcs.registerButtonMenu && g_gc_funcs.unregisterButtonMenu &&
         g_gc_funcs.registerButtonHome && g_gc_funcs.unregisterButtonHome &&
         g_gc_funcs.registerLeftShoulder && g_gc_funcs.unregisterLeftShoulder &&
         g_gc_funcs.registerRightShoulder && g_gc_funcs.unregisterRightShoulder &&
         g_gc_funcs.registerLeftTrigger && g_gc_funcs.unregisterLeftTrigger &&
         g_gc_funcs.registerRightTrigger && g_gc_funcs.unregisterRightTrigger &&
         g_gc_funcs.registerLeftThumbstick && g_gc_funcs.unregisterLeftThumbstick &&
         g_gc_funcs.registerRightThumbstick && g_gc_funcs.unregisterRightThumbstick &&
         g_gc_funcs.registerDpadUp && g_gc_funcs.unregisterDpadUp &&
         g_gc_funcs.registerDpadDown && g_gc_funcs.unregisterDpadDown &&
         g_gc_funcs.registerDpadLeft && g_gc_funcs.unregisterDpadLeft &&
         g_gc_funcs.registerDpadRight && g_gc_funcs.unregisterDpadRight &&
         g_gc_funcs.registerAxisLeftThumbstick && g_gc_funcs.unregisterAxisLeftThumbstick &&
         g_gc_funcs.registerAxisRightThumbstick && g_gc_funcs.unregisterAxisRightThumbstick &&
         g_gc_funcs.registerAxisLeftTrigger && g_gc_funcs.unregisterAxisLeftTrigger &&
         g_gc_funcs.registerAxisRightTrigger && g_gc_funcs.unregisterAxisRightTrigger &&
         g_gc_funcs.registerAxisDpad && g_gc_funcs.unregisterAxisDpad &&
         g_gc_funcs.buttonEventGetDeviceId && g_gc_funcs.buttonEventGetButtonAction &&
         g_gc_funcs.buttonEventGetButtonCode && g_gc_funcs.axisEventGetDeviceId &&
         g_gc_funcs.axisEventGetAxisSourceType && g_gc_funcs.axisEventGetXAxisValue &&
         g_gc_funcs.axisEventGetYAxisValue && g_gc_funcs.axisEventGetZAxisValue &&
         g_gc_funcs.axisEventGetRZAxisValue && g_gc_funcs.axisEventGetBrakeAxisValue &&
         g_gc_funcs.axisEventGetGasAxisValue;
}

void InitDlopenFun() {
  static std::once_flag initFlag;
  std::call_once(initFlag, []() {
#if defined(ARCH_CPU_ARM64) || defined(ARCH_CPU_X86_64)
    void* dl = dlopen("/system/lib64/ndk/libohgame_controller.z.so", RTLD_NOW);
#else
    void* dl = dlopen("/system/lib/ndk/libohgame_controller.z.so", RTLD_NOW);
#endif
    if (!dl) {
      WVLOG_W("libohgame_controller.z.so not available, gamepad disabled");
      return;
    }
    LoadAllFuncs(dl);
    if (!ValidateFuncs()) {
      WVLOG_W("libohgame_controller.z.so missing required symbols, gamepad disabled");
      return;
    }
    g_so_loaded.store(true, std::memory_order_release);
    WVLOG_I("libohgame_controller.z.so loaded successfully");
  });
}

int32_t RegisterDeviceId(const char* deviceIdStr) {
  std::string key(deviceIdStr ? deviceIdStr : "");
  std::lock_guard<std::mutex> lock(g_adapter_mutex);
  auto it = g_device_id_map.find(key);
  if (it != g_device_id_map.end()) {
    return it->second;
  }
  int32_t id = g_next_device_id.fetch_add(1);
  g_device_id_map[key] = id;
  return id;
}

int32_t LookupDeviceId(const char* deviceIdStr) {
  std::string key(deviceIdStr ? deviceIdStr : "");
  std::lock_guard<std::mutex> lock(g_adapter_mutex);
  auto it = g_device_id_map.find(key);
  if (it != g_device_id_map.end()) {
    return it->second;
  }
  return -1;
}

void UnregisterDeviceId(const char* deviceIdStr) {
  std::string key(deviceIdStr ? deviceIdStr : "");
  std::lock_guard<std::mutex> lock(g_adapter_mutex);
  auto it = g_device_id_map.find(key);
  if (it == g_device_id_map.end()) {
    return;
  }
  g_device_id_map.erase(it);
}

// deviceId format: "vendor_product_index". product_id prefers the API value,
// falling back to the parsed value.
void GetVendorAndProduct(const char* deviceIdStr,
                                GameDevice_DeviceInfo* deviceInfo,
                                uint16_t& vendor_id,
                                uint16_t& product_id) {
  vendor_id = 0;
  product_id = 0;
  int32_t gcProduct = 0;
  if (deviceInfo && g_gc_funcs.deviceInfoGetProduct) {
    g_gc_funcs.deviceInfoGetProduct(deviceInfo, &gcProduct);
  }
  if (!deviceIdStr || deviceIdStr[0] == '\0') {
    return;
  }
  std::string idStr(deviceIdStr);
  size_t firstUnder = idStr.find('_');
  size_t secondUnder = idStr.find('_', firstUnder + 1);
  if (firstUnder == std::string::npos || secondUnder == std::string::npos) {
    return;
  }
  vendor_id = static_cast<uint16_t>(std::atoi(idStr.substr(0, firstUnder).c_str()));
  int32_t parsedProduct = std::atoi(idStr.substr(firstUnder + 1, secondUnder - firstUnder - 1).c_str());
  product_id = static_cast<uint16_t>(gcProduct != 0 ? gcProduct : parsedProduct);
}

// Maps OHOS keycodes to raw layout indices; gamepad_standard_mappings_ohos.cpp
// converts these to W3C canonical order.
int MapButtonCode(int32_t buttonCode) {
  switch (buttonCode) {
    case KEYCODE_BUTTON_A: return device::kOhosButtonA;
    case KEYCODE_BUTTON_B: return device::kOhosButtonB;
    case KEYCODE_BUTTON_X: return device::kOhosButtonX;
    case KEYCODE_BUTTON_Y: return device::kOhosButtonY;
    case KEYCODE_BUTTON_L1: return device::kOhosButtonLeftShoulder;
    case KEYCODE_BUTTON_R1: return device::kOhosButtonRightShoulder;
    case KEYCODE_BUTTON_L2: return device::kOhosButtonLeftTrigger;
    case KEYCODE_BUTTON_R2: return device::kOhosButtonRightTrigger;
    case KEYCODE_BUTTON_SELECT: return device::kOhosButtonBackSelect;
    case KEYCODE_BUTTON_START: return device::kOhosButtonStart;
    case KEYCODE_BUTTON_MODE: return device::kOhosButtonMeta;
    case KEYCODE_BUTTON_THUMBL: return device::kOhosButtonLeftThumbstick;
    case KEYCODE_BUTTON_THUMBR: return device::kOhosButtonRightThumbstick;
    case KEYCODE_DPAD_UP: return device::kOhosButtonDpadUp;
    case KEYCODE_DPAD_DOWN: return device::kOhosButtonDpadDown;
    case KEYCODE_DPAD_LEFT: return device::kOhosButtonDpadLeft;
    case KEYCODE_DPAD_RIGHT: return device::kOhosButtonDpadRight;
    default: return buttonCode;
  }
}

int MapAxisType(GamePad_AxisSourceType axisType) {
  switch (axisType) {
    case GamePad_AxisSourceType::LEFT_THUMBSTICK: return device::kOhosAxisLeftStickX;
    case GamePad_AxisSourceType::RIGHT_THUMBSTICK: return device::kOhosAxisRightStickX;
    case GamePad_AxisSourceType::LEFT_TRIGGER: return device::kOhosAxisLeftTrigger;
    case GamePad_AxisSourceType::RIGHT_TRIGGER: return device::kOhosAxisRightTrigger;
    default: return static_cast<int>(axisType);
  }
}

// D-pad is handled via button events; skip axis events only when x/y are both
// zero (some gamepads report stick data via DPAD axis type).
bool IsDpadAxisEvent(GamePad_AxisSourceType axisType, double x, double y) {
  return axisType == GamePad_AxisSourceType::DPAD &&
         std::abs(x) < kAxisEpsilon && std::abs(y) < kAxisEpsilon;
}

// Falls back to Z/RZ axes for right stick when X/Y are zero.
void ResolveRightStickAxes(GamePad_AxisSourceType axisType,
                                  double& x, double& y, double z, double rz) {
  if (axisType != GamePad_AxisSourceType::RIGHT_THUMBSTICK) {
    return;
  }
  if (std::abs(x) < kAxisEpsilon && std::abs(z) > kAxisEpsilon) {
    x = z;
  }
  if (std::abs(y) < kAxisEpsilon && std::abs(rz) > kAxisEpsilon) {
    y = rz;
  }
}

// Falls back to brake/gas axis for triggers when X is zero.
float ResolveTriggerValue(GamePad_AxisSourceType axisType,
                                 double x, double brake, double gas) {
  if (axisType == GamePad_AxisSourceType::LEFT_TRIGGER &&
      std::abs(x) < kAxisEpsilon && brake > kAxisEpsilon) {
    return static_cast<float>(brake);
  }
  if (axisType == GamePad_AxisSourceType::RIGHT_TRIGGER &&
      std::abs(x) < kAxisEpsilon && gas > kAxisEpsilon) {
    return static_cast<float>(gas);
  }
  return static_cast<float>(x);
}

}  // namespace

GamepadAdapterImpl::GamepadAdapterImpl() {
  InitDlopenFun();
  if (!g_so_loaded.load(std::memory_order_acquire)) {
    return;
  }
  g_gc_funcs.registerDeviceMonitor(OnGameDeviceEvent);
  g_gc_funcs.registerButtonA(OnGamePadButtonEvent);
  g_gc_funcs.registerButtonB(OnGamePadButtonEvent);
  g_gc_funcs.registerButtonX(OnGamePadButtonEvent);
  g_gc_funcs.registerButtonY(OnGamePadButtonEvent);
  g_gc_funcs.registerButtonMenu(OnGamePadButtonEvent);
  g_gc_funcs.registerButtonHome(OnGamePadButtonEvent);
  g_gc_funcs.registerLeftShoulder(OnGamePadButtonEvent);
  g_gc_funcs.registerRightShoulder(OnGamePadButtonEvent);
  g_gc_funcs.registerLeftTrigger(OnGamePadButtonEvent);
  g_gc_funcs.registerRightTrigger(OnGamePadButtonEvent);
  g_gc_funcs.registerLeftThumbstick(OnGamePadButtonEvent);
  g_gc_funcs.registerRightThumbstick(OnGamePadButtonEvent);
  g_gc_funcs.registerDpadUp(OnGamePadButtonEvent);
  g_gc_funcs.registerDpadDown(OnGamePadButtonEvent);
  g_gc_funcs.registerDpadLeft(OnGamePadButtonEvent);
  g_gc_funcs.registerDpadRight(OnGamePadButtonEvent);
  g_gc_funcs.registerAxisLeftThumbstick(OnGamePadAxisEvent);
  g_gc_funcs.registerAxisRightThumbstick(OnGamePadAxisEvent);
  g_gc_funcs.registerAxisLeftTrigger(OnGamePadAxisEvent);
  g_gc_funcs.registerAxisRightTrigger(OnGamePadAxisEvent);
  g_gc_funcs.registerAxisDpad(OnGamePadAxisEvent);
}

GamepadAdapterImpl::~GamepadAdapterImpl() {
  if (!g_so_loaded.load(std::memory_order_acquire)) {
    return;
  }
  g_gc_funcs.unregisterDeviceMonitor();
  g_gc_funcs.unregisterButtonA();
  g_gc_funcs.unregisterButtonB();
  g_gc_funcs.unregisterButtonX();
  g_gc_funcs.unregisterButtonY();
  g_gc_funcs.unregisterButtonMenu();
  g_gc_funcs.unregisterButtonHome();
  g_gc_funcs.unregisterLeftShoulder();
  g_gc_funcs.unregisterRightShoulder();
  g_gc_funcs.unregisterLeftTrigger();
  g_gc_funcs.unregisterRightTrigger();
  g_gc_funcs.unregisterLeftThumbstick();
  g_gc_funcs.unregisterRightThumbstick();
  g_gc_funcs.unregisterDpadUp();
  g_gc_funcs.unregisterDpadDown();
  g_gc_funcs.unregisterDpadLeft();
  g_gc_funcs.unregisterDpadRight();
  g_gc_funcs.unregisterAxisLeftThumbstick();
  g_gc_funcs.unregisterAxisRightThumbstick();
  g_gc_funcs.unregisterAxisLeftTrigger();
  g_gc_funcs.unregisterAxisRightTrigger();
  g_gc_funcs.unregisterAxisDpad();
}

void GamepadAdapterImpl::OnGameDeviceEvent(const GameDevice_DeviceEvent* deviceEvent) {
  if (deviceEvent == nullptr || !g_so_loaded.load(std::memory_order_acquire)) {
    return;
  }
  GameDevice_StatusChangedType status;
  GameController_ErrorCode ret = g_gc_funcs.deviceEventGetChangedType(deviceEvent, &status);
  if (ret != GAME_CONTROLLER_SUCCESS) {
    return;
  }
  GameDevice_DeviceInfo* deviceInfo = nullptr;
  ret = g_gc_funcs.deviceEventGetDeviceInfo(deviceEvent, &deviceInfo);
  if (ret != GAME_CONTROLLER_SUCCESS || deviceInfo == nullptr) {
    return;
  }
  GameDevice_DeviceType deviceType = GameDevice_DeviceType::UNKNOWN;
  g_gc_funcs.deviceInfoGetDeviceType(deviceInfo, &deviceType);
  if (deviceType != GameDevice_DeviceType::GAME_PAD) {
    g_gc_funcs.destroyDeviceInfo(&deviceInfo);
    return;
  }
  char* deviceId = nullptr;
  g_gc_funcs.deviceInfoGetDeviceId(deviceInfo, &deviceId);
  char* deviceName = nullptr;
  g_gc_funcs.deviceInfoGetName(deviceInfo, &deviceName);
  int32_t numericDeviceId = RegisterDeviceId(deviceId);
  const char* nameToSend = deviceName ? deviceName : (deviceId ? deviceId : "");
  if (status == ONLINE) {
    uint16_t vendor_id = 0;
    uint16_t product_id = 0;
    GetVendorAndProduct(deviceId, deviceInfo, vendor_id, product_id);
    device::OnGamepadDeviceConnected(numericDeviceId, nameToSend, vendor_id, product_id);
  } else {
    device::OnGamepadDeviceDisconnected(numericDeviceId);
    UnregisterDeviceId(deviceId);
  }
  free(deviceId);
  free(deviceName);
  g_gc_funcs.destroyDeviceInfo(&deviceInfo);
}

void GamepadAdapterImpl::OnGamePadButtonEvent(const GamePad_ButtonEvent* buttonEvent) {
  if (buttonEvent == nullptr || !g_so_loaded.load(std::memory_order_acquire)) {
    return;
  }
  char* deviceId = nullptr;
  g_gc_funcs.buttonEventGetDeviceId(buttonEvent, &deviceId);
  GamePad_Button_ActionType action{};
  if (g_gc_funcs.buttonEventGetButtonAction(buttonEvent, &action) != GAME_CONTROLLER_SUCCESS) {
    free(deviceId);
    return;
  }
  int32_t buttonCode = 0;
  g_gc_funcs.buttonEventGetButtonCode(buttonEvent, &buttonCode);
  int32_t numericDeviceId = LookupDeviceId(deviceId);
  free(deviceId);
  if (numericDeviceId < 0) {
    return;
  }
  device::OnGamepadButtonEvent(numericDeviceId, MapButtonCode(buttonCode),
                               action == GamePad_Button_ActionType::DOWN);
}

void GamepadAdapterImpl::OnGamePadAxisEvent(const GamePad_AxisEvent* axisEvent) {
  if (axisEvent == nullptr || !g_so_loaded.load(std::memory_order_acquire)) {
    return;
  }
  char* deviceId = nullptr;
  g_gc_funcs.axisEventGetDeviceId(axisEvent, &deviceId);
  GamePad_AxisSourceType axisType{};
  if (g_gc_funcs.axisEventGetAxisSourceType(axisEvent, &axisType) != GAME_CONTROLLER_SUCCESS) {
    free(deviceId);
    return;
  }
  double x = 0;
  double y = 0;
  double z = 0;
  double rz = 0;
  double brake = 0;
  double gas = 0;
  g_gc_funcs.axisEventGetXAxisValue(axisEvent, &x);
  g_gc_funcs.axisEventGetYAxisValue(axisEvent, &y);
  g_gc_funcs.axisEventGetZAxisValue(axisEvent, &z);
  g_gc_funcs.axisEventGetRZAxisValue(axisEvent, &rz);
  g_gc_funcs.axisEventGetBrakeAxisValue(axisEvent, &brake);
  g_gc_funcs.axisEventGetGasAxisValue(axisEvent, &gas);
  if (IsDpadAxisEvent(axisType, x, y)) {
    free(deviceId);
    return;
  }
  ResolveRightStickAxes(axisType, x, y, z, rz);
  float triggerValue = ResolveTriggerValue(axisType, x, brake, gas);
  int32_t numericDeviceId = LookupDeviceId(deviceId);
  free(deviceId);
  if (numericDeviceId < 0) {
    return;
  }
  bool isTrigger = axisType == GamePad_AxisSourceType::LEFT_TRIGGER ||
                   axisType == GamePad_AxisSourceType::RIGHT_TRIGGER;
  device::OnGamepadAxisEvent(numericDeviceId, MapAxisType(axisType),
                             isTrigger ? triggerValue : static_cast<float>(x),
                             static_cast<float>(y));
}

void GamepadAdapterImpl::EnumerateExistingDevices() {
  if (!g_so_loaded.load(std::memory_order_acquire)) {
    return;
  }
  GameDevice_AllDeviceInfos* allDeviceInfos = nullptr;
  GameController_ErrorCode ret = g_gc_funcs.getAllDeviceInfos(&allDeviceInfos);
  if (ret != GAME_CONTROLLER_SUCCESS || allDeviceInfos == nullptr) {
    // Retry without capturing `this` — only uses static/global state.
    std::thread([]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(kEnumerateRetryDelayMs));
      if (!g_so_loaded.load(std::memory_order_acquire)) {
        return;
      }
      GameDevice_AllDeviceInfos* retry_infos = nullptr;
      if (g_gc_funcs.getAllDeviceInfos(&retry_infos) != GAME_CONTROLLER_SUCCESS ||
          retry_infos == nullptr) {
        return;
      }
      EnumerateFromDeviceInfos(retry_infos);
    }).detach();
    return;
  }
  EnumerateFromDeviceInfos(allDeviceInfos);
}

void GamepadAdapterImpl::EnumerateFromDeviceInfos(GameDevice_AllDeviceInfos* allDeviceInfos) {
  int32_t count = 0;
  if (g_gc_funcs.allDeviceInfosGetCount(allDeviceInfos, &count) != GAME_CONTROLLER_SUCCESS) {
    g_gc_funcs.destroyAllDeviceInfos(&allDeviceInfos);
    return;
  }
  for (int32_t i = 0; i < count; ++i) {
    GameDevice_DeviceInfo* deviceInfo = nullptr;
    if (g_gc_funcs.allDeviceInfosGetDeviceInfo(allDeviceInfos, i, &deviceInfo) != GAME_CONTROLLER_SUCCESS ||
        deviceInfo == nullptr) {
      continue;
    }
    GameDevice_DeviceType deviceType = GameDevice_DeviceType::UNKNOWN;
    g_gc_funcs.deviceInfoGetDeviceType(deviceInfo, &deviceType);
    if (deviceType != GameDevice_DeviceType::GAME_PAD) {
      continue;
    }
    char* deviceId = nullptr;
    g_gc_funcs.deviceInfoGetDeviceId(deviceInfo, &deviceId);
    char* deviceName = nullptr;
    g_gc_funcs.deviceInfoGetName(deviceInfo, &deviceName);
    int32_t numericDeviceId = RegisterDeviceId(deviceId);
    const char* nameToSend = deviceName ? deviceName : (deviceId ? deviceId : "");
    uint16_t vendor_id = 0;
    uint16_t product_id = 0;
    GetVendorAndProduct(deviceId, deviceInfo, vendor_id, product_id);
    device::OnGamepadDeviceConnected(numericDeviceId, nameToSend, vendor_id, product_id);
    free(deviceId);
    free(deviceName);
  }
  g_gc_funcs.destroyAllDeviceInfos(&allDeviceInfos);
}

}  // namespace OHOS::NWeb
