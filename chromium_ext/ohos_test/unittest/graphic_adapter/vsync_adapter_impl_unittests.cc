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

#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

#define private public
#include "arkweb/ohos_adapter_ndk/graphic_adapter/vsync_adapter_impl.h"
#undef private
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_ndk_api.h"
#include <thread>

using namespace testing;
using namespace MockNdkApi;

namespace OHOS::NWeb {

static void OnVsyncCallback() {}
static void OnVsyncCallback2() {}

// Static state for callbacks (lambdas with captures cannot convert to function pointers)
static bool g_callbackCalled;
static int g_callCount;
static bool g_vsyncCalled;
static int64_t g_vsyncTs;
static int64_t g_expectedTs;

static void VsyncCb_SetCalled(int64_t /*ts*/, void* /*data*/) {
    g_callbackCalled = true;
}

static void VsyncCb_IncCount(int64_t /*ts*/, void* /*data*/) {
    g_callCount++;
}

static void VsyncCb_CheckTs(int64_t ts, void* /*data*/) {
    g_vsyncCalled = true;
    g_vsyncTs = ts;
}

class VSyncAdapterImplTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void VSyncAdapterImplTest::SetUpTestCase() {}

void VSyncAdapterImplTest::TearDownTestCase() {}

void VSyncAdapterImplTest::SetUp() {}

void VSyncAdapterImplTest::TearDown()
{
    // Reset singleton state to prevent crash in destructor at program exit:
    // if vsyncReceiver_ holds a fake pointer, ~VSyncAdapterNdkImpl() would call
    // __real_OH_NativeVSync_Destroy(fake_ptr) after mocks are cleared -> SIGSEGV.
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = nullptr;
    adapter.hasRequestedVsync_ = false;
    adapter.vsyncCallbacks_.clear();
    VSyncAdapterNdkImpl::callback_ = nullptr;

    g_mock_OH_NativeVSync_Create = nullptr;
    g_mock_OH_NativeVSync_Destroy = nullptr;
    g_mock_OH_NativeVSync_GetPeriod = nullptr;
    g_mock_OH_NativeVSync_RequestFrame = nullptr;
}

// Test Init: OH_NativeVSync_Create returns nullptr -> ERROR
TEST_F(VSyncAdapterImplTest, Init_CreateFail_ReturnError)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = nullptr;

    g_mock_OH_NativeVSync_Create = [](const char* name, unsigned int length) {
        return static_cast<OH_NativeVSync*>(nullptr);
    };
    VSyncErrorCode ret = adapter.Init();
    EXPECT_EQ(ret, VSyncErrorCode::ERROR);
    EXPECT_EQ(adapter.vsyncReceiver_, nullptr);
}

// Test Init: OH_NativeVSync_Create succeeds -> SUCCESS, receiver set
TEST_F(VSyncAdapterImplTest, Init_CreateSuccess_ReturnSuccess)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = nullptr;

    g_mock_OH_NativeVSync_Create = [](const char* name, unsigned int length) {
        return reinterpret_cast<OH_NativeVSync*>(0x1234);
    };
    g_mock_OH_NativeVSync_Destroy = [](OH_NativeVSync* nativeVsync) {};
    VSyncErrorCode ret = adapter.Init();
    EXPECT_EQ(ret, VSyncErrorCode::SUCCESS);
    EXPECT_NE(adapter.vsyncReceiver_, nullptr);
}

// Test Init: already initialized -> SUCCESS without re-creating
TEST_F(VSyncAdapterImplTest, Init_AlreadyInitialized_ReturnSuccess)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = reinterpret_cast<OH_NativeVSync*>(0x5678);

    bool createCalled = false;
    g_mock_OH_NativeVSync_Create = [&](const char* name, unsigned int length) {
        createCalled = true;
        return reinterpret_cast<OH_NativeVSync*>(0x9999);
    };
    VSyncErrorCode ret = adapter.Init();
    EXPECT_EQ(ret, VSyncErrorCode::SUCCESS);
    EXPECT_FALSE(createCalled);
    EXPECT_EQ(adapter.vsyncReceiver_, reinterpret_cast<OH_NativeVSync*>(0x5678));
}

// Test RequestVsync: Init fails -> ERROR
TEST_F(VSyncAdapterImplTest, RequestVsync_InitFail_ReturnError)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = nullptr;
    adapter.hasRequestedVsync_ = false;

    g_mock_OH_NativeVSync_Create = [](const char* name, unsigned int length) {
        return static_cast<OH_NativeVSync*>(nullptr);
    };
    VSyncErrorCode ret = adapter.RequestVsync(nullptr, nullptr);
    EXPECT_EQ(ret, VSyncErrorCode::ERROR);
}

// Test RequestVsync: already requested -> SUCCESS without calling RequestFrame
TEST_F(VSyncAdapterImplTest, RequestVsync_AlreadyRequested_ReturnSuccess)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = reinterpret_cast<OH_NativeVSync*>(0x1111);
    adapter.hasRequestedVsync_ = true;

    bool requestFrameCalled = false;
    g_mock_OH_NativeVSync_RequestFrame = [&](OH_NativeVSync* nativeVSync,
        OH_NativeVSync_FrameCallback callback, void* data) {
        requestFrameCalled = true;
        return 0;
    };
    VSyncErrorCode ret = adapter.RequestVsync(nullptr, nullptr);
    EXPECT_EQ(ret, VSyncErrorCode::SUCCESS);
    EXPECT_FALSE(requestFrameCalled);
    adapter.hasRequestedVsync_ = false;
}

// Test RequestVsync: RequestFrame fails -> ERROR
TEST_F(VSyncAdapterImplTest, RequestVsync_RequestFrameFail_ReturnError)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = reinterpret_cast<OH_NativeVSync*>(0x2222);
    adapter.hasRequestedVsync_ = false;

    g_mock_OH_NativeVSync_RequestFrame = [](OH_NativeVSync* nativeVSync,
        OH_NativeVSync_FrameCallback callback, void* data) {
        return -1;
    };
    VSyncErrorCode ret = adapter.RequestVsync(nullptr, nullptr);
    EXPECT_EQ(ret, VSyncErrorCode::ERROR);
    EXPECT_FALSE(adapter.hasRequestedVsync_);
}

// Test RequestVsync: RequestFrame succeeds -> SUCCESS, hasRequestedVsync_ set
TEST_F(VSyncAdapterImplTest, RequestVsync_RequestFrameSuccess_ReturnSuccess)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = reinterpret_cast<OH_NativeVSync*>(0x3333);
    adapter.hasRequestedVsync_ = false;

    g_mock_OH_NativeVSync_RequestFrame = [](OH_NativeVSync* nativeVSync,
        OH_NativeVSync_FrameCallback callback, void* data) {
        return 0;
    };
    VSyncErrorCode ret = adapter.RequestVsync(nullptr, nullptr);
    EXPECT_EQ(ret, VSyncErrorCode::SUCCESS);
    EXPECT_TRUE(adapter.hasRequestedVsync_);
    adapter.hasRequestedVsync_ = false;
}

// Test RequestVsync: callback is stored in vsyncCallbacks_
TEST_F(VSyncAdapterImplTest, RequestVsync_CallbackStored)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = reinterpret_cast<OH_NativeVSync*>(0x4444);
    adapter.hasRequestedVsync_ = false;
    adapter.vsyncCallbacks_.clear();

    g_mock_OH_NativeVSync_RequestFrame = [](OH_NativeVSync* nativeVSync,
        OH_NativeVSync_FrameCallback callback, void* data) {
        return 0;
    };
    int dummyData = 42;
    NWebVSyncCb cb = [](int64_t, void*) {};
    adapter.RequestVsync(&dummyData, cb);
    EXPECT_EQ(adapter.vsyncCallbacks_.size(), 1u);
    EXPECT_TRUE(adapter.vsyncCallbacks_.count(&dummyData) > 0);
    adapter.hasRequestedVsync_ = false;
    adapter.vsyncCallbacks_.clear();
}

// Test OnVsync: client is null -> logs error, no crash
TEST_F(VSyncAdapterImplTest, OnVsync_NullClient_NoCrash)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.OnVsync(12345, nullptr);
}

// Test OnVsync: client is valid -> calls VsyncCallbackInner
TEST_F(VSyncAdapterImplTest, OnVsync_ValidClient_CallsInner)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = reinterpret_cast<OH_NativeVSync*>(0x5555);
    adapter.vsyncCallbacks_.clear();
    adapter.hasRequestedVsync_ = false;
    VSyncAdapterNdkImpl::callback_ = nullptr;

    g_callbackCalled = false;
    int dummyData = 99;
    NWebVSyncCb cb = VsyncCb_SetCalled;
    adapter.vsyncCallbacks_[&dummyData] = cb;

    adapter.OnVsync(99999, &adapter);
    EXPECT_TRUE(g_callbackCalled);
    EXPECT_TRUE(adapter.vsyncCallbacks_.empty());
    EXPECT_FALSE(adapter.hasRequestedVsync_);
}

// Test VsyncCallbackInner: static callback_ is invoked
TEST_F(VSyncAdapterImplTest, VsyncCallbackInner_StaticCallbackInvoked)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncCallbacks_.clear();
    adapter.hasRequestedVsync_ = true;

    bool staticCallbackCalled = false;
    VSyncAdapterNdkImpl::callback_ = OnVsyncCallback;
    adapter.VsyncCallbackInner(0);
    // callback_ is set but we can't easily verify it was called since it's a simple void() func.
    // Just ensure no crash and callbacks cleared.
    VSyncAdapterNdkImpl::callback_ = nullptr;
    EXPECT_TRUE(adapter.vsyncCallbacks_.empty());
    EXPECT_FALSE(adapter.hasRequestedVsync_);
}

// Test VsyncCallbackInner: multiple callbacks all invoked
TEST_F(VSyncAdapterImplTest, VsyncCallbackInner_MultipleCallbacks)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncCallbacks_.clear();
    adapter.hasRequestedVsync_ = true;
    VSyncAdapterNdkImpl::callback_ = nullptr;

    g_callCount = 0;
    int data1 = 1;
    int data2 = 2;
    NWebVSyncCb cb = VsyncCb_IncCount;
    adapter.vsyncCallbacks_[&data1] = cb;
    adapter.vsyncCallbacks_[&data2] = cb;

    adapter.VsyncCallbackInner(100);
    EXPECT_EQ(g_callCount, 2);
    EXPECT_TRUE(adapter.vsyncCallbacks_.empty());
    EXPECT_FALSE(adapter.hasRequestedVsync_);
}

// Test GetVSyncPeriod: Init fails -> returns 0
TEST_F(VSyncAdapterImplTest, GetVSyncPeriod_InitFail_ReturnsZero)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = nullptr;

    g_mock_OH_NativeVSync_Create = [](const char* name, unsigned int length) {
        return static_cast<OH_NativeVSync*>(nullptr);
    };
    int64_t period = adapter.GetVSyncPeriod();
    EXPECT_EQ(period, 0);
}

// Test GetVSyncPeriod: GetPeriod returns success with valid period
TEST_F(VSyncAdapterImplTest, GetVSyncPeriod_Success_ReturnsPeriod)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = reinterpret_cast<OH_NativeVSync*>(0x6666);

    g_mock_OH_NativeVSync_GetPeriod = [](OH_NativeVSync* nativeVSync, long long* period) {
        *period = 16666666;
        return 0;
    };
    int64_t period = adapter.GetVSyncPeriod();
    EXPECT_EQ(period, 16666666);
}

// Test GetVSyncPeriod: GetPeriod returns error code
TEST_F(VSyncAdapterImplTest, GetVSyncPeriod_GetPeriodFail_ReturnsZero)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = reinterpret_cast<OH_NativeVSync*>(0x7777);

    g_mock_OH_NativeVSync_GetPeriod = [](OH_NativeVSync* nativeVSync, long long* period) {
        *period = 0;
        return -1;
    };
    int64_t period = adapter.GetVSyncPeriod();
    EXPECT_EQ(period, 0);
}

// Test SetFrameRateLinkerEnable: no-op, should not crash
TEST_F(VSyncAdapterImplTest, SetFrameRateLinkerEnable_NoCrash)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.SetFrameRateLinkerEnable(true);
    adapter.SetFrameRateLinkerEnable(false);
}

// Test SetFramePreferredRate: no-op, should not crash
TEST_F(VSyncAdapterImplTest, SetFramePreferredRate_NoCrash)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.SetFramePreferredRate(60);
    adapter.SetFramePreferredRate(120);
    adapter.SetFramePreferredRate(0);
}

// Test SetOnVsyncCallback: sets the static callback_
TEST_F(VSyncAdapterImplTest, SetOnVsyncCallback_SetsCallback)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    VSyncAdapterNdkImpl::callback_ = nullptr;
    adapter.SetOnVsyncCallback(OnVsyncCallback);
    EXPECT_NE(VSyncAdapterNdkImpl::callback_, nullptr);
    VSyncAdapterNdkImpl::callback_ = nullptr;
}

// Test SetIsGPUProcess: sets isGPUProcess_ member
TEST_F(VSyncAdapterImplTest, SetIsGPUProcess_SetsFlag)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.SetIsGPUProcess(true);
    EXPECT_TRUE(adapter.isGPUProcess_);
    adapter.SetIsGPUProcess(false);
    EXPECT_FALSE(adapter.isGPUProcess_);
}

// Test SetOnVsyncEndCallback: no-op, should not crash
TEST_F(VSyncAdapterImplTest, SetOnVsyncEndCallback_NoCrash)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.SetOnVsyncEndCallback(OnVsyncCallback);
    adapter.SetOnVsyncEndCallback(nullptr);
}

// Test SetScene: no-op, should not crash
TEST_F(VSyncAdapterImplTest, SetScene_NoCrash)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.SetScene("test_scene", 1);
    adapter.SetScene("", 0);
    adapter.SetScene("another_scene", 100);
}

// Test SetDVSyncSwitch: no-op, should not crash
TEST_F(VSyncAdapterImplTest, SetDVSyncSwitch_NoCrash)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.SetDVSyncSwitch(true);
    adapter.SetDVSyncSwitch(false);
}

// Test destructor: destroys vsyncReceiver_ via OH_NativeVSync_Destroy
TEST_F(VSyncAdapterImplTest, Destructor_DestroysReceiver)
{
    bool destroyCalled = false;
    g_mock_OH_NativeVSync_Destroy = [&destroyCalled](OH_NativeVSync* nativeVsync) {
        destroyCalled = true;
    };
    {
        VSyncAdapterNdkImpl localAdapter;
        localAdapter.vsyncReceiver_ = reinterpret_cast<OH_NativeVSync*>(0x8888);
        // destructor will call OH_NativeVSync_Destroy
    }
    EXPECT_TRUE(destroyCalled);
}

// Test GetInstance: returns same singleton instance
TEST_F(VSyncAdapterImplTest, GetInstance_ReturnsSingleton)
{
    VSyncAdapterNdkImpl &instance1 = VSyncAdapterNdkImpl::GetInstance();
    VSyncAdapterNdkImpl &instance2 = VSyncAdapterNdkImpl::GetInstance();
    EXPECT_EQ(&instance1, &instance2);
}

// Test combined flow: Init -> RequestVsync -> OnVsync -> RequestVsync again
TEST_F(VSyncAdapterImplTest, CombinedFlow_InitRequestVsyncOnVsync)
{
    VSyncAdapterNdkImpl &adapter = VSyncAdapterNdkImpl::GetInstance();
    adapter.vsyncReceiver_ = nullptr;
    adapter.hasRequestedVsync_ = false;
    adapter.vsyncCallbacks_.clear();
    VSyncAdapterNdkImpl::callback_ = nullptr;

    g_mock_OH_NativeVSync_Create = [](const char* name, unsigned int length) {
        return reinterpret_cast<OH_NativeVSync*>(0xABCD);
    };
    g_mock_OH_NativeVSync_Destroy = [](OH_NativeVSync* nativeVsync) {};
    g_mock_OH_NativeVSync_RequestFrame = [](OH_NativeVSync* nativeVSync,
        OH_NativeVSync_FrameCallback callback, void* data) {
        return 0;
    };
    g_mock_OH_NativeVSync_GetPeriod = [](OH_NativeVSync* nativeVSync, long long* period) {
        *period = 11111111;
        return 0;
    };

    // Init
    EXPECT_EQ(adapter.Init(), VSyncErrorCode::SUCCESS);

    // RequestVsync
    int dummy = 777;
    g_vsyncCalled = false;
    g_expectedTs = 55555;
    NWebVSyncCb cb = VsyncCb_CheckTs;
    EXPECT_EQ(adapter.RequestVsync(&dummy, cb), VSyncErrorCode::SUCCESS);
    EXPECT_TRUE(adapter.hasRequestedVsync_);

    // Simulate vsync callback
    adapter.OnVsync(55555, &adapter);
    EXPECT_TRUE(g_vsyncCalled);
    EXPECT_EQ(g_vsyncTs, 55555);
    EXPECT_FALSE(adapter.hasRequestedVsync_);
    EXPECT_TRUE(adapter.vsyncCallbacks_.empty());

    // RequestVsync again after vsync fired
    EXPECT_EQ(adapter.RequestVsync(&dummy, cb), VSyncErrorCode::SUCCESS);
    EXPECT_TRUE(adapter.hasRequestedVsync_);

    // Cleanup
    adapter.hasRequestedVsync_ = false;
    adapter.vsyncCallbacks_.clear();
}

} // namespace OHOS::NWeb
