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
#include "date_time_format_adapter_impl_fuzz_mock.h"
#include <cstring>
#include <BasicServicesKit/time_service.h>

const char* OH_CommonEvent_GetEventFromRcvData(const CommonEvent_RcvData* rcvData) {
    if (rcvData == nullptr) {
        return "";
    }
    return rcvData->event.c_str();
}

TimeService_ErrCode OH_TimeService_GetTimeZone(char* timeZone, uint32_t len) {
    if (timeZone == nullptr || len == 0) {
        return TIMESERVICE_ERR_INTERNAL_ERROR;
    }
    std::strncpy(timeZone, "UTC", len - 1);
    timeZone[len - 1] = '\0';
    return TIMESERVICE_ERR_OK;
}