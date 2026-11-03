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

#include "arkweb/ohos_nweb/src/nweb_hilog.h"
#include "mmi_new_adapter_impl.h"

namespace OHOS::NWeb {

MMINewAdapterImpl::MMINewAdapterImpl() = default;
MMINewAdapterImpl::~MMINewAdapterImpl() = default;

void OnDeviceAdded(int32_t deviceId)
{
    std::shared_ptr<MMIListenerAdapter> deviceListener = MMINewAdapterImpl::GetInstance().GetDeviceAdapterListener();
    std::string type = MMINewAdapterImpl::GetInstance().GetType();
    if (deviceListener) {
        deviceListener->OnDeviceAdded(deviceId, type);
    }
}

void OnDeviceRemoved(int32_t deviceId)
{
    std::shared_ptr<MMIListenerAdapter> deviceListener = MMINewAdapterImpl::GetInstance().GetDeviceAdapterListener();
    std::string type = MMINewAdapterImpl::GetInstance().GetType();
    if (deviceListener) {
        deviceListener->OnDeviceRemoved(deviceId, type);
    }
}

MMINewAdapterImpl& MMINewAdapterImpl::GetInstance()
{
    static MMINewAdapterImpl instance;
    return instance;
}

std::shared_ptr<MMIListenerAdapter> MMINewAdapterImpl::GetDeviceAdapterListener()
{
    return inputDeviceAdapterListener_;
}

std::string MMINewAdapterImpl::GetType()
{
    return type_;
}

char* MMINewAdapterImpl::KeyCodeToString(int32_t keyCode)
{
    WVLOG_W("deprecated key code to string");
    return NULL;
}

int32_t MMINewAdapterImpl::RegisterMMIInputListener(std::shared_ptr<MMIInputListenerAdapter> eventCallback)
{
    WVLOG_W("deprecated register MMI input listener");
    return 0;
}

void MMINewAdapterImpl::UnregisterMMIInputListener(int32_t monitorId)
{
    WVLOG_W("deprecated unregister MMI input listener");
}

int32_t MMINewAdapterImpl::RegisterDevListener(std::string type, std::shared_ptr<MMIListenerAdapter> listener)
{
    if (!listener) {
        WVLOG_E("register device listener is nullptr");
        return -1;
    }
    inputDeviceAdapterListener_ = listener;
    type_ = type;
    inputDeviceListener_ = {
        .deviceAddedCallback = OnDeviceAdded,
        .deviceRemovedCallback = OnDeviceRemoved,
    };

    return OH_Input_RegisterDeviceListener(&inputDeviceListener_);
}

int32_t MMINewAdapterImpl::UnregisterDevListener(std::string type)
{
    if (type == type_) {
        return OH_Input_UnregisterDeviceListener(&inputDeviceListener_);
    }
    return -1;
}

int32_t MMINewAdapterImpl::GetKeyboardType(int32_t deviceId, int32_t& type)
{
    return OH_Input_GetKeyboardType(deviceId, &type);
}

int32_t MMINewAdapterImpl::GetDeviceIds(std::vector<int32_t>& ids)
{
    const size_t maxSize = 1024;
    int32_t idsArr[maxSize];

    int32_t outSize = 0;
    Input_Result result = OH_Input_GetDeviceIds(idsArr, maxSize, &outSize);
    if (result != INPUT_SUCCESS) {
        return -1;
    }
    if (outSize <= 0) {
        return outSize;
    }
    ids.clear();
    for (int32_t i = 0; i < outSize; ++i) {
        ids.push_back(idsArr[i]);
    }
    return 0;
}

int32_t MMINewAdapterImpl::GetDeviceInfo(int32_t deviceId, std::shared_ptr<MMIDeviceInfoAdapter> info)
{
    if (!info) {
        WVLOG_E("GetDeviceInfo info is nullptr");
        return -1;
    }
    Input_DeviceInfo *deviceInfo = OH_Input_CreateDeviceInfo();
    if (deviceInfo == nullptr) {
        WVLOG_E("create device info error");
        return -1;
    }
    OH_Input_GetDevice(deviceId, &deviceInfo);
    int32_t id = -1;
    int32_t version = -1;
    int32_t product = -1;
    int32_t vendor = -1;
    char *name = nullptr;
    char *address = nullptr;
    int32_t capabilities = -1;

    OH_Input_GetDeviceId(deviceInfo, &id);
    OH_Input_GetDeviceVersion(deviceInfo, &version);
    OH_Input_GetDeviceProduct(deviceInfo, &product);
    OH_Input_GetDeviceVendor(deviceInfo, &vendor);
    OH_Input_GetDeviceName(deviceInfo, &name);
    OH_Input_GetDeviceAddress(deviceInfo, &address);
    OH_Input_GetCapabilities(deviceInfo, &capabilities);

    info->SetId(id);
    info->SetType(capabilities);
    info->SetVersion(version);
    info->SetProduct(product);
    info->SetVendor(vendor);
    info->SetName(name);
    info->SetPhys(address);

    OH_Input_DestroyDeviceInfo(&deviceInfo);
    return 0;
}

int32_t MMINewAdapterImpl::GetMaxTouchPoints()
{
    return 0;
}

namespace {
constexpr int32_t INPUT_DEVICE_CAP_EV_KEY = 1 << 0;   // Keyboard/key events
constexpr int32_t INPUT_DEVICE_CAP_EV_REL = 1 << 1;   // Relative coordinates (mouse)
constexpr int32_t INPUT_DEVICE_CAP_EV_ABS = 1 << 2;   // Absolute coordinates (touchscreen)

bool ClassifyInputDevice(int32_t deviceId, bool& isPointer, bool& isKeyboard)
{
    isPointer = false;
    isKeyboard = false;

    Input_DeviceInfo* deviceInfo = OH_Input_CreateDeviceInfo();
    if (deviceInfo == nullptr) {
        WVLOG_E("failed to create device info for deviceId=%{public}d", deviceId);
        return false;
    }
    Input_Result result = OH_Input_GetDevice(deviceId, &deviceInfo);
    if (result != INPUT_SUCCESS) {
        WVLOG_W("failed to get device for deviceId=%{public}d, result=%{public}d",
            deviceId, result);
        OH_Input_DestroyDeviceInfo(&deviceInfo);
        return false;
    }

    int32_t capabilities = -1;
    result = OH_Input_GetCapabilities(deviceInfo, &capabilities);
    if (result != INPUT_SUCCESS || capabilities < 0) {
        WVLOG_W("OH_Input_GetCapabilities failed, deviceId=%{public}d, "
            "result=%{public}d, capabilities=%{public}d", deviceId, result, capabilities);
        OH_Input_DestroyDeviceInfo(&deviceInfo);
        return false;
    }

    // A pointer device has either EV_REL (relative coordinates, e.g. mouse)
    // or EV_ABS (absolute coordinates, e.g. touchscreen or BT mouse misreported
    // by MMI). This may incorrectly include touchscreens and internal sensors
    // as pointer devices; to be refined when input API exposes bus type information.
    isPointer = (capabilities & INPUT_DEVICE_CAP_EV_REL) || (capabilities & INPUT_DEVICE_CAP_EV_ABS);

    // Only alphabetic/digital/remote_control keyboards are valid for Pointer
    // Lock exit (ESC key). Power keys and stylus are excluded.
    int32_t keyboardType = 0;
    if (capabilities & INPUT_DEVICE_CAP_EV_KEY) {
        result = OH_Input_GetKeyboardType(deviceId, &keyboardType);
        if (result != INPUT_SUCCESS) {
            WVLOG_W("OH_Input_GetKeyboardType failed for deviceId=%{public}d, result=%{public}d, "
                "treat as non-keyboard device", deviceId, result);
        }
        if (keyboardType == KEYBOARD_TYPE_ALPHABETIC ||
            keyboardType == KEYBOARD_TYPE_DIGITAL ||
            keyboardType == KEYBOARD_TYPE_REMOTE_CONTROL) {
            isKeyboard = true;
        }
    }

    OH_Input_DestroyDeviceInfo(&deviceInfo);
    return true;
}
} // namespace

bool MMINewAdapterImpl::HasKeyboardAndMouse() const
{
    const int32_t maxDeviceCount = 1024;
    int32_t ids[maxDeviceCount] = { 0 };
    int32_t outSize = 0;
    Input_Result result = OH_Input_GetDeviceIds(ids, maxDeviceCount, &outSize);
    if (result != INPUT_SUCCESS) {
        WVLOG_W("OH_Input_GetDeviceIds failed, result=%{public}d", result);
        return false;
    }
    if (outSize < 0 || outSize > maxDeviceCount) {
        WVLOG_W("OH_Input_GetDeviceIds returned invalid outSize=%{public}d", outSize);
        return false;
    }

    int32_t keyboardDeviceId = -1;
    int32_t pointerDeviceId = -1;

    for (int32_t i = 0; i < outSize; ++i) {
        bool isPointer = false;
        bool isKeyboard = false;
        if (!ClassifyInputDevice(ids[i], isPointer, isKeyboard)) {
            continue;
        }

        // Pointer Lock requires keyboard and pointer from DIFFERENT devices.
        // A combined device (USB dongle) may set both IDs to the same deviceId;
        // allow later devices to reassign when both slots point to the same device.
        if (isPointer && (pointerDeviceId < 0 || pointerDeviceId == keyboardDeviceId)) {
            pointerDeviceId = ids[i];
        }
        if (isKeyboard && (keyboardDeviceId < 0 || keyboardDeviceId == pointerDeviceId)) {
            keyboardDeviceId = ids[i];
        }
        if (keyboardDeviceId >= 0 && pointerDeviceId >= 0 && keyboardDeviceId != pointerDeviceId) {
            WVLOG_I("keyboard=%{public}d, pointer=%{public}d, allow",
                keyboardDeviceId, pointerDeviceId);
            return true;
        }
    }

    return false;
}
} // namespace OHOS::NWeb
