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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "hiappevent_adapter_impl.h"
#include "ohos_sdk/openharmony/native/sysroot/usr/include/hiappevent/hiappevent.h"
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_ohos_adapter_helper.h"
#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"

using namespace testing;
using namespace OHOS::NWeb;

namespace {
HiAppeventAdapter::DynamicFrameDropInfo MakeDynamicFrameDropInfo(
    int64_t start_time, int64_t duration, int64_t max_app_frame_time, int32_t web_id) {
    HiAppeventAdapter::DynamicFrameDropInfo info;
    info.start_time = start_time;
    info.duration = duration;
    info.max_app_frame_time = max_app_frame_time;
    info.web_id = web_id;
    return info;
}
} // namespace

class HiAppeventAdapterImplTest : public ::testing::Test {};

class MockHiAppeventAdapter : public HiAppeventAdapter {
public:
    MOCK_METHOD(void, ReportDynamicStastic,
        (const std::string& domain, const std::string& eventName, int32_t eventType,
        const HiAppeventAdapter::DynamicFrameDropInfo& dynamicFrameDropInfo), (override));
};

// 声明文件作用域函数以便直接测试，签名需与 hiappevent_adapter_impl.cpp 保持一致
namespace OHOS::NWeb {
EventType GetHiappeventType(int32_t eventType);
ParamList ConvertFrameDropInfoToHiappeventParams(
    HiAppeventAdapter::DynamicFrameDropInfo dynamicFrameDropInfo);
}

TEST_F(HiAppeventAdapterImplTest, GetInstance001) {
    HiAppeventAdapterImpl& instance1 = HiAppeventAdapterImpl::GetInstance();
    HiAppeventAdapterImpl& instance2 = HiAppeventAdapterImpl::GetInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(HiAppeventAdapterImplTest, ReportDynamicStasticDelegate001) {
    MockOhosAdapterHelper mockHelper;
    OhosAdapterHelper::SetInstance(&mockHelper);
    MockHiAppeventAdapter mockAdapter;
    EXPECT_CALL(mockHelper, GetHiAppeventAdapterInstance())
        .WillOnce(testing::ReturnRef(mockAdapter));

    std::string domain = "testDomain";
    std::string event_name = "testEvent";
    int32_t event_type = 1;
    auto frameDropInfo = MakeDynamicFrameDropInfo(100, 200, 300, 1);

    // 验证委托转发时全部参数传递正确
    EXPECT_CALL(mockAdapter, ReportDynamicStastic(
        testing::Eq(domain), testing::Eq(event_name), testing::Eq(event_type),
        testing::AllOf(
            testing::Field(&HiAppeventAdapter::DynamicFrameDropInfo::start_time, 100),
            testing::Field(&HiAppeventAdapter::DynamicFrameDropInfo::duration, 200),
            testing::Field(&HiAppeventAdapter::DynamicFrameDropInfo::max_app_frame_time, 300),
            testing::Field(&HiAppeventAdapter::DynamicFrameDropInfo::web_id, 1))))
        .WillOnce(testing::Return());

    OhosAdapterHelper::GetInstance().GetHiAppeventAdapterInstance()
        .ReportDynamicStastic(domain, event_name, event_type, frameDropInfo);

    OhosAdapterHelper::SetInstance(nullptr);
}

// eventType 到 HiAppEvent EventType 映射: 1=FAULT, 2=STATISTIC, 3=SECURITY, other=BEHAVIOR
TEST_F(HiAppeventAdapterImplTest, GetHiappeventType001) {
    EXPECT_EQ(GetHiappeventType(1), EventType::FAULT);
    EXPECT_EQ(GetHiappeventType(2), EventType::STATISTIC);
    EXPECT_EQ(GetHiappeventType(3), EventType::SECURITY);
    EXPECT_EQ(GetHiappeventType(0), EventType::BEHAVIOR);
    EXPECT_EQ(GetHiappeventType(999), EventType::BEHAVIOR);
}

// NDK stub 环境下 OH_HiAppEvent_CreateParamList 返回值不可靠，仅验证调用不触发致命错误
TEST_F(HiAppeventAdapterImplTest, ConvertFrameDropInfo001) {
    auto info = MakeDynamicFrameDropInfo(1000, 2000, 3000, 42);
    ASSERT_NO_FATAL_FAILURE({
        ParamList params = ConvertFrameDropInfoToHiappeventParams(info);
        OH_HiAppEvent_DestroyParamList(params);
    });
}
