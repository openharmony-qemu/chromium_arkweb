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

#define private public
#include "arkweb/ohos_adapter_ndk/date_time_format_adapter/date_time_format_adapter_impl.h"
#undef private

#include <cstring>
#include <fuzzer/FuzzedDataProvider.h>
#include "arkweb/ohos_adapter_ndk/date_time_format_adapter/date_time_format_adapter_impl.cpp"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api.h"

using namespace OHOS::NWeb;
using namespace MockNdkApi;

namespace OHOS {

class TimezoneEventCallbackAdapterFuzz : public TimezoneEventCallbackAdapter {
public:
    TimezoneEventCallbackAdapterFuzz() = default;
    void TimezoneChanged(std::shared_ptr<WebTimezoneInfo> info) override {}
};

void DateTimeFormatReceiveCallbackFuzzTest(FuzzedDataProvider* fdp)
{
    int testCase = fdp->ConsumeIntegralInRange<int>(0, 3);
    switch (testCase) {
        case 0: {
            DateTimeFormatReceiveCallback(nullptr);
            break;
        }
        case 1: {
            CommonEvent_RcvData data;
            g_mock_OH_CommonEvent_GetEventFromRcvData = [](const CommonEvent_RcvData* rcvData) {
                return COMMON_EVENT_PACKAGE_ADDED;
            };
            DateTimeFormatReceiveCallback(&data);
            g_mock_OH_CommonEvent_GetEventFromRcvData = nullptr;
            break;
        }
        case 2: {
            CommonEvent_RcvData data;
            g_mock_OH_CommonEvent_GetEventFromRcvData = [](const CommonEvent_RcvData* rcvData) {
                return COMMON_EVENT_TIMEZONE_CHANGED;
            };
            g_mock_OH_TimeService_GetTimeZone = [](char *timeZone, uint32_t len) {
                return TIMESERVICE_ERR_INTERNAL_ERROR;
            };
            DateTimeFormatReceiveCallback(&data);
            g_mock_OH_CommonEvent_GetEventFromRcvData = nullptr;
            g_mock_OH_TimeService_GetTimeZone = nullptr;
            break;
        }
        case 3: {
            CommonEvent_RcvData data;
            g_mock_OH_CommonEvent_GetEventFromRcvData = [](const CommonEvent_RcvData* rcvData) {
                return COMMON_EVENT_TIMEZONE_CHANGED;
            };
            g_mock_OH_TimeService_GetTimeZone = [](char *timeZone, uint32_t len) {
                if (timeZone && len > 0) {
                    std::strncpy(timeZone, "Asia/Shanghai", len - 1);
                    timeZone[len - 1] = '\0';
                }
                return TIMESERVICE_ERR_OK;
            };
            auto adapter = std::make_shared<DateTimeFormatAdapterImpl>();
            std::shared_ptr<TimezoneEventCallbackAdapter> cb = std::make_shared<TimezoneEventCallbackAdapterFuzz>();
            adapter->RegTimezoneEvent(std::move(cb));
            adapter->StartListen();
            DateTimeFormatReceiveCallback(&data);
            adapter->StopListen();
            g_mock_OH_CommonEvent_GetEventFromRcvData = nullptr;
            g_mock_OH_TimeService_GetTimeZone = nullptr;
            break;
        }
        default:
            break;
    }
}

void DateTimeFormatAdapterImplFuzzTest(FuzzedDataProvider* fdp)
{
    bool hasCallback = fdp->ConsumeBool();
    auto adapter = std::make_shared<DateTimeFormatAdapterImpl>();
    adapter->RegTimezoneEvent(nullptr);
    std::shared_ptr<TimezoneEventCallbackAdapter> cb = std::make_shared<TimezoneEventCallbackAdapterFuzz>();
    adapter->RegTimezoneEvent(std::move(cb));
    adapter->StartListen();
    if (hasCallback) {
        std::shared_ptr<TimezoneEventCallbackAdapter> cb2 = std::make_shared<TimezoneEventCallbackAdapterFuzz>();
        adapter->RegTimezoneEvent(std::move(cb2));
    }
    adapter->StopListen();
    adapter->GetTimezone();
}

} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);
    OHOS::DateTimeFormatReceiveCallbackFuzzTest(&fdp);
    OHOS::DateTimeFormatAdapterImplFuzzTest(&fdp);

    return 0;
}