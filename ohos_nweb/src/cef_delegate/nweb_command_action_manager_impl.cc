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

// Concrete implementation of AutoFillItem for internal use in arkweb.
class AutoFillItemImpl : public AutoFillItem {
public:
    AutoFillItemImpl(const std::string& xpath, const std::string& content,
                     int32_t index, AutoFillMode mode)
        : xpath_(xpath), content_(content), index_(index), mode_(mode) {}

    std::string GetXPath() const override { return xpath_; }
    std::string GetContent() const override { return content_; }
    int32_t GetIndex() const override { return index_; }
    AutoFillMode GetMode() const override { return mode_; }

private:
    std::string xpath_;
    std::string content_;
    int32_t index_ = 0;
    AutoFillMode mode_ = AutoFillMode::Overwrite;
};

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

int32_t NWebCommandActionManagerImpl::HandleAutoFillCommand(
    std::shared_ptr<NWebCommandActionInfo> info) {
  LOG(INFO) << "[WebCommandAction] HandleAutoFillCommand";
  if (!info) {
    LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: info is null";
    return static_cast<int32_t>(WebCommandErrorCode::FAILED);
  }
  if (!nweb_delegate_) {
    LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: delegate is null";
    return static_cast<int32_t>(WebCommandErrorCode::DELEGATE_NULL);
  }

  // Prevent reentrancy: if a batch autofill is already running,
  // reject the new call immediately.
  if (is_autofill_running_) {
    LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: reentrant call rejected";
    return static_cast<int32_t>(WebCommandErrorCode::FAILED);
  }
  is_autofill_running_ = true;

  auto items = info->GetAutoFillItems();

  if (items.empty()) {
    LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: items is empty";
    is_autofill_running_ = false;
    return static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_ITEMS);
  }

  if (items.size() > 10) {
    LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: items count="
               << items.size() << " exceeds limit 10, truncating";
    items.resize(10);
  }

  AutoFillMode default_mode = info->GetDefaultMode();

  int32_t first_error = static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
  bool has_error = false;

  // Batch autofill timeout: 5s total, 500ms per item (in ExecuteAutoFillCommand)
  constexpr int32_t kTotalTimeoutMs = 5000;

  const base::TimeTicks deadline = base::TimeTicks::Now() +
                                    base::Milliseconds(kTotalTimeoutMs);

  for (size_t i = 0; i < items.size(); ++i) {
    base::TimeDelta remaining = deadline - base::TimeTicks::Now();
    if (remaining <= base::TimeDelta()) {
      LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: total 5s timeout";
      break;
    }

    const auto& item = items[i];

    // Validate item parameters — skip invalid items but continue processing
    int32_t node_id = item->GetNodeId();
    if (node_id <= 0 && item->GetXPath().empty()) {
      LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: item[" << i
                 << "] node id and xpath are invalid, skipping";
      if (!has_error) {
        // Reuse the legacy XPath error code when neither locator is usable.
        first_error = static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_ITEM_XPATH);
        has_error = true;
      }
      continue;
    }
    if (item->GetContent().empty()) {
      LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: item[" << i
                 << "] content is empty, skipping";
      if (!has_error) {
        first_error = static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_ITEM_CONTENT);
        has_error = true;
      }
      continue;
    }
    if (item->GetIndex() < 0) {
      LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: item[" << i
                 << "] index is invalid: " << item->GetIndex() << ", skipping";
      if (!has_error) {
        first_error = static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_ITEM_INDEX);
        has_error = true;
      }
      continue;
    }

    AutoFillMode mode = item->GetMode();
    if (mode == AutoFillMode::Unspecified) {
      mode = default_mode;
    }

    // Single ExecuteAutoFillCommand call per item
    // remaining is passed down so the per-item timer respects the total deadline
    int32_t result = nweb_delegate_->ExecuteAutoFillCommand(
        item->GetXPath(), node_id, item->GetContent(),
        item->GetIndex(), static_cast<int32_t>(mode),
        static_cast<int32_t>(remaining.InMilliseconds()));

    if (result != static_cast<int32_t>(WebCommandErrorCode::SUCCESS)) {
      LOG(ERROR) << "[WebCommandAction] HandleAutoFillCommand: item[" << i
                 << "] ExecuteAutoFillCommand failed, mode="
                 << static_cast<int>(mode) << " code=" << result;
      if (!has_error) {
        first_error = result;
        has_error = true;
      }
      continue;
    }
  }

  is_autofill_running_ = false;
  return has_error ? first_error
                   : static_cast<int32_t>(WebCommandErrorCode::SUCCESS);
}

}  // namespace OHOS::NWeb

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
