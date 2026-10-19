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

#ifndef OHOS_NWEB_SRC_NDK_SCHEME_HANDLER_ERROR_INFO_H_
#define OHOS_NWEB_SRC_NDK_SCHEME_HANDLER_ERROR_INFO_H_

#include "ohos_nweb/src/capi/arkweb_scheme_handler.h"

struct ArkWeb_ErrorInfo_ {
    ArkWeb_ErrorInfo_();
    ~ArkWeb_ErrorInfo_() = default;

    void SetCompleteIfNoResponse(bool completeIfNoResponse);
    bool GetCompleteIfNoResponse() const;

    void SetCustomErrorCode(int32_t customErrorCode);
    int32_t GetCustomErrorCode() const;

    void SetErrorCode(ArkWeb_NetError errorCode);
    ArkWeb_NetError GetErrorCode() const;

private:
    bool complete_if_no_response_ = false;
    int32_t custom_error_code_ = 0;
    ArkWeb_NetError error_code_ = ARKWEB_NET_OK;
};

#endif  // OHOS_NWEB_SRC_NDK_SCHEME_HANDLER_ERROR_INFO_H_
