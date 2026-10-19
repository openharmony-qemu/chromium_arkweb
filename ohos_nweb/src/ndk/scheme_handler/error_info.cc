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

#include "ohos_nweb/src/ndk/scheme_handler/error_info.h"

ArkWeb_ErrorInfo_::ArkWeb_ErrorInfo_() = default;

void ArkWeb_ErrorInfo_::SetCompleteIfNoResponse(bool completeIfNoResponse) {
    complete_if_no_response_ = completeIfNoResponse;
}

bool ArkWeb_ErrorInfo_::GetCompleteIfNoResponse() const {
    return complete_if_no_response_;
}

void ArkWeb_ErrorInfo_::SetCustomErrorCode(int32_t customErrorCode) {
    custom_error_code_ = customErrorCode;
}

int32_t ArkWeb_ErrorInfo_::GetCustomErrorCode() const {
    return custom_error_code_;
}

void ArkWeb_ErrorInfo_::SetErrorCode(ArkWeb_NetError errorCode) {
    error_code_ = errorCode;
}

ArkWeb_NetError ArkWeb_ErrorInfo_::GetErrorCode() const {
    return error_code_;
}
