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
#ifndef DATE_TIME_FORMAT_ADAPTER_IMPL_FUZZ_MOCK_H
#define DATE_TIME_FORMAT_ADAPTER_IMPL_FUZZ_MOCK_H

#include <cstdint>
#include <string>
#include <BasicServicesKit/oh_commonevent_support.h>
#include <BasicServicesKit/time_service.h>

#define FUZZ_MOCK_EXPORT __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

struct CArrParameters {
  int32_t netType;
};

typedef struct CommonEvent_RcvData {
  std::string event;
  std::string bundleName;
  int32_t code;
  std::string data;
  CArrParameters* parameters = nullptr;
} CommonEvent_RcvData;

FUZZ_MOCK_EXPORT const char* OH_CommonEvent_GetEventFromRcvData(
    const CommonEvent_RcvData* rcvData);

FUZZ_MOCK_EXPORT TimeService_ErrCode OH_TimeService_GetTimeZone(char* timeZone, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif  // DATE_TIME_FORMAT_ADAPTER_IMPL_FUZZ_MOCK_H