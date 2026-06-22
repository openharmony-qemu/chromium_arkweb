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

#include "nweb_command_action_manager_impl.h"

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)

#include <algorithm>
#include <sstream>
#include "base/cef_logging.h"
#include "ohos_nweb/include/nweb_web_message.h"

#include "nweb_delegate.h"

namespace OHOS::NWeb {

NWebCommandActionManagerImpl::NWebCommandActionManagerImpl(
    base::WeakPtr<NWebDelegate> nweb_delegate)
    : nweb_delegate_(nweb_delegate) {}

NWebCommandActionManagerImpl::~NWebCommandActionManagerImpl() = default;
CefRefPtr<CefBrowser> NWebCommandActionManagerImpl::GetBrowser() const {
    auto* delegate = nweb_delegate_.get();
    if (delegate) {
        return delegate->GetBrowser();
    }
    return nullptr;
}

int32_t NWebCommandActionManagerImpl::HandleInputCommand(
    std::shared_ptr<NWebCommandActionInfo> info) {
    if (!info) {
        LOG(ERROR) << "[WebCommandAction] HandleInputCommand: info is null";
        return static_cast<int32_t>(WebCommandErrorCode::FAILED);
    }
    if (!nweb_delegate_) {
        LOG(ERROR) << "[WebCommandAction] HandleInputCommand: delegate is null";
        return static_cast<int32_t>(WebCommandErrorCode::DELEGATE_NULL);
    }

    auto input_xpath = info->GetXPath();
    auto input_value = info->GetInputValue();

    if (input_xpath.empty()) {
        LOG(ERROR) << "[WebCommandAction] HandleInputCommand: xpath is empty";
        return static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_XPATH);
    }
    if (input_value.empty()) {
        LOG(ERROR) << "[WebCommandAction] HandleInputCommand: value is empty";
        return static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_INPUT_VALUE);
    }

    auto result = nweb_delegate_->ExecuteInputCommand(
        input_xpath, input_value, info->GetEventType());
    if (result != static_cast<int32_t>(WebCommandErrorCode::SUCCESS)) {
        LOG(ERROR) << "[WebCommandAction] HandleInputCommand failed, code=" << result;
    }
    return result;
}

int32_t NWebCommandActionManagerImpl::HandleSelectCommand(
    std::shared_ptr<NWebCommandActionInfo> info) {
    if (!info) {
        LOG(ERROR) << "[WebCommandAction] HandleSelectCommand: info is null";
        return static_cast<int32_t>(WebCommandErrorCode::FAILED);
    }
    if (!nweb_delegate_) {
        LOG(ERROR) << "[WebCommandAction] HandleSelectCommand: delegate is null";
        return static_cast<int32_t>(WebCommandErrorCode::DELEGATE_NULL);
    }

    auto select_xpath = info->GetXPath();
    auto select_values = info->GetOptionValues();
    auto select_indexes = info->GetOptionIndexes();

    if (select_xpath.empty()) {
        LOG(ERROR) << "[WebCommandAction] HandleSelectCommand: xpath is empty";
        return static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_XPATH);
    }
    if (select_values.empty() && select_indexes.empty()) {
        LOG(ERROR) << "[WebCommandAction] HandleSelectCommand: both values and "
                      "indexes are empty";
        return static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_SELECT_OPTIONS);
    }

    auto result = nweb_delegate_->ExecuteSelectCommand(select_xpath, select_indexes, select_values);
    if (result != static_cast<int32_t>(WebCommandErrorCode::SUCCESS)) {
        LOG(ERROR) << "[WebCommandAction] HandleSelectCommand failed, code=" << result;
    }
    return result;
}

int32_t NWebCommandActionManagerImpl::HandleGestureCommand(
    std::shared_ptr<NWebCommandActionInfo> info) {
    LOG(INFO) << "NWebCommandActionManagerImpl::HandleGestureCommand";
    if (!info) {
        LOG(ERROR) << "HandleGestureCommand info is null";
        return static_cast<int32_t>(WebCommandErrorCode::FAILED);
    }

    auto* delegate = nweb_delegate_.get();
    if (!delegate) {
        LOG(ERROR) << "HandleGestureCommand nweb_delegate is null";
        return static_cast<int32_t>(WebCommandErrorCode::DELEGATE_NULL);
    }

    auto browser = GetBrowser();
    if (!browser.get() || !browser->GetHost()) {
        LOG(ERROR) << "HandleGestureCommand browser is null";
        return static_cast<int32_t>(WebCommandErrorCode::CONTEXT_NULL);
    }

    std::string eventType = info->GetEventType();
    double x = info->GetX();
    double y = info->GetY();
    double distanceX = info->GetDistanceX();
    double distanceY = info->GetDistanceY();
    float actionScale = info->GetScale();
    int32_t speed = info->GetSpeed();
    int32_t duration = info->GetDuration();
    int32_t tapCount = info->GetTapCount();

    LOG(INFO) << "HandleGestureCommand eventType=" << eventType
              << " x=" << x << " y=" << y;

    float scale = browser->GetHost()->GetPageScaleFactor();
    if (scale <= 0.0f) {
        LOG(ERROR) << "HandleGestureCommand scale is invalid: " << scale;
        return static_cast<int32_t>(WebCommandErrorCode::FAILED);
    }

    double virtual_pixel_ratio = browser->GetHost()->GetVirtualPixelRatio();
    if (virtual_pixel_ratio <= 0.0) {
        LOG(ERROR) << "HandleGestureCommand virtual_pixel_ratio is invalid: " << virtual_pixel_ratio;
        return static_cast<int32_t>(WebCommandErrorCode::FAILED);
    }
    LOG(INFO) << "HandleGestureCommand scale=" << scale << " virtual_pixel_ratio=" << virtual_pixel_ratio;

    // If scroll offset is needed, call NWebDelegate::GetScrollOffset
    double cdp_x = x / virtual_pixel_ratio / scale;
    double cdp_y = y / virtual_pixel_ratio / scale;
    double cdp_distance_x = distanceX / virtual_pixel_ratio / scale;
    double cdp_distance_y = distanceY / virtual_pixel_ratio / scale;


    LOG(INFO) << "HandleGestureCommand converted CDP coords: "
              << "x=" << cdp_x << " y=" << cdp_y;

    std::ostringstream message;
    std::string method;

    if (eventType == "tap") {
        method = BuildTapGestureCommand(cdp_x, cdp_y, duration, tapCount);
    } else if (eventType == "scrollGesture") {
        method = BuildScrollGestureCommand(cdp_x, cdp_y, cdp_distance_x, cdp_distance_y, speed);
    } else if (eventType == "pinch") {
        method = BuildPinchGestureCommand(cdp_x, cdp_y, actionScale, speed);
    } else {
        LOG(WARNING) << "HandleGestureCommand unknown eventType: " << eventType;
        return static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_EVENT_TYPE);
    }

    message << "{" << "\"id\":" << pending_message_id_++ << "," << method << "}";
    std::string pending_message = message.str();
    LOG(INFO) << "HandleGestureCommand CDP message: " << pending_message;

    browser->GetHost()->SendDevToolsMessage(pending_message.data(), pending_message.size());

    return static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

std::string NWebCommandActionManagerImpl::BuildTapGestureCommand(
    double x, double y, int32_t duration, int32_t tapCount) const
{
    std::string params = "\"params\": {"
                        "\"x\": " + std::to_string(x) + ","
                        "\"y\": " + std::to_string(y);
    if (duration > 0) {
        params += ",\"duration\": " + std::to_string(duration);
    }
    if (tapCount > 1) {
        params += ",\"tapCount\": " + std::to_string(tapCount);
    }
    params += ",\"gestureSourceType\": \"mouse\""
             "}";
    return "\"method\": \"Input.synthesizeTapGesture\"," + params;
}

std::string NWebCommandActionManagerImpl::BuildScrollGestureCommand(
    double x, double y, double distanceX, double distanceY, int32_t speed) const
{
    std::string params = "\"params\": {"
                        "\"x\": " + std::to_string(x) + ","
                        "\"y\": " + std::to_string(y);
    if (distanceX != 0) {
        params += ",\"xDistance\": " + std::to_string(distanceX);
    }
    if (distanceY != 0) {
        params += ",\"yDistance\": " + std::to_string(distanceY);
    }
    if (speed > 0) {
        params += ",\"speed\": " + std::to_string(speed);
    }
    params += ",\"gestureSourceType\": \"mouse\""
             "}";
    return "\"method\": \"Input.synthesizeScrollGesture\"," + params;
}

std::string NWebCommandActionManagerImpl::BuildPinchGestureCommand(
    double x, double y, float scaleFactor, int32_t speed) const
{
    std::string params = "\"params\": {"
                        "\"x\": " + std::to_string(x) + ","
                        "\"y\": " + std::to_string(y) + ","
                        "\"scaleFactor\": " + std::to_string(scaleFactor);
    if (speed > 0) {
        params += ",\"relativeSpeed\": " + std::to_string(speed);
    }
    params += ",\"gestureSourceType\": \"mouse\""
             "}";
    return "\"method\": \"Input.synthesizePinchGesture\"," + params;
}

}  // namespace OHOS::NWeb

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
