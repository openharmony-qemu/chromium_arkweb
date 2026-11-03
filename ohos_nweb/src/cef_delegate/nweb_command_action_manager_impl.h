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

#ifndef NWEB_COMMAND_ACTION_MANAGER_IMPL_H
#define NWEB_COMMAND_ACTION_MANAGER_IMPL_H

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "nweb_command_action_info.h"
#include "nweb_command_action_manager.h"
#include "nweb_errors.h"
#include "cef/include/cef_browser.h"

namespace OHOS::NWeb {

class NWebDelegate;

class NWebCommandActionManagerImpl : public NWebCommandActionManager {
public:
    explicit NWebCommandActionManagerImpl(
        base::WeakPtr<NWebDelegate> nweb_delegate);

    ~NWebCommandActionManagerImpl() override;

    int32_t HandleInputCommand(std::shared_ptr<NWebCommandActionInfo> info) override;

    int32_t HandleSelectCommand(std::shared_ptr<NWebCommandActionInfo> info) override;
    int32_t HandleGestureCommand(std::shared_ptr<NWebCommandActionInfo> info) override;

    int32_t HandleAutoFillCommand(std::shared_ptr<NWebCommandActionInfo> info) override;

    CefRefPtr<CefBrowser> GetBrowser() const;
    std::string BuildTapGestureCommand(double x, double y, int32_t duration, int32_t tapCount) const;
    std::string BuildScrollGestureCommand(double x, double y, double distanceX, double distanceY, int32_t speed) const;
    std::string BuildPinchGestureCommand(double x, double y, float scaleFactor, int32_t speed) const;
private:
    base::WeakPtr<NWebDelegate> nweb_delegate_;
    int32_t pending_message_id_ = 1;
    bool is_autofill_running_ = false;  // Prevents reentrant HandleAutoFillCommand calls
};

}  // namespace OHOS::NWeb

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)

#endif  // NWEB_COMMAND_ACTION_MANAGER_IMPL_H
