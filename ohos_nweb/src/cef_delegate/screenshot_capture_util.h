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

#ifndef OHOS_NWEB_SRC_CEF_DELEGATE_SCREENSHOT_CAPTURE_UTIL_H_
#define OHOS_NWEB_SRC_CEF_DELEGATE_SCREENSHOT_CAPTURE_UTIL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "cef/include/cef_base.h"
#include "nweb_errors.h"

class ArkWebBrowserExt;

namespace OHOS::NWeb {

struct CaptureResult {
    bool success;
    std::string base64_data;
    WebCommandErrorCode error_code;
    std::string error_message;

    CaptureResult() : success(false), error_code(WebCommandErrorCode::SUCCESS) {}
    CaptureResult(bool s, std::string data)
        : success(s), base64_data(std::move(data)), error_code(WebCommandErrorCode::SUCCESS) {}
    CaptureResult(WebCommandErrorCode code, std::string msg)
        : success(false), error_code(code), error_message(std::move(msg)) {}
};

class ScreenshotCaptureUtil {
public:
    using CaptureCallback = base::OnceCallback<void(CaptureResult result)>;

    static void CaptureWithClip(CefRefPtr<ArkWebBrowserExt> browser,
                                double x, double y,
                                double width, double height,
                                CaptureCallback callback);
};

}  // namespace OHOS::NWeb

#endif  // OHOS_NWEB_SRC_CEF_DELEGATE_SCREENSHOT_CAPTURE_UTIL_H_
