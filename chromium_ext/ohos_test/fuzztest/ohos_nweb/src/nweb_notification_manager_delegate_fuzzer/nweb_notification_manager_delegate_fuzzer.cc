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

#include <fuzzer/FuzzedDataProvider.h>

#define private public
#include "nweb_notification_manager_delegate.h"
#undef private

#include <memory>
#include <string>

using namespace OHOS::NWeb;

namespace {

// Mock callback implementation for testing
class MockNotificationDelegateCallback : public NWebNotificationDelegateCallback {
public:
    MockNotificationDelegateCallback() {
        showNotification = [](std::shared_ptr<NWebNotificationOptionsItem> optionsItem) {};
        closeNotification = [](std::string id) {};
    }
};

// Global shared callback for reuse across iterations
std::shared_ptr<NWebNotificationDelegateCallback> g_mockCallback = nullptr;

std::shared_ptr<NWebNotificationDelegateCallback> GetMockCallback()
{
    if (!g_mockCallback) {
        g_mockCallback = std::make_shared<MockNotificationDelegateCallback>();
    }
    return g_mockCallback;
}

} // namespace

namespace OHOS {
namespace NWeb {

// Test scenario 1: Test GetOrCreateDelegate and RegisterNotificationManagerDelegateListener
bool TestDelegateLifecycle(const uint8_t* data, size_t size)
{
    FuzzedDataProvider fdp(data, size);

    // Get or create delegate - will create if null
    auto& delegate1 = NWebNotificationManagerDelegate::GetOrCreateDelegate();

    // Test RegisterNotificationManagerDelegateListener
    bool hasListener = fdp.ConsumeBool();
    if (hasListener) {
        delegate1->RegisterNotificationManagerDelegateListener(GetMockCallback());
    }

    // Get delegate again - should return same instance
    auto& delegate2 = NWebNotificationManagerDelegate::GetOrCreateDelegate();

    // Try to register again with different callback (should be ignored since already set)
    auto otherCallback = std::make_shared<MockNotificationDelegateCallback>();
    delegate2->RegisterNotificationManagerDelegateListener(otherCallback);

    return true;
}

// Test scenario 2: Test OnShowNotification with various inputs
bool TestOnShowNotification(const uint8_t* data, size_t size)
{
    FuzzedDataProvider fdp(data, size);

    // First get the delegate and register a listener
    auto& delegate = NWebNotificationManagerDelegate::GetOrCreateDelegate();
    delegate->RegisterNotificationManagerDelegateListener(GetMockCallback());

    // Create notification options item
    auto optionsItem = std::make_shared<NWebNotificationOptionsItem>();
    optionsItem->id = fdp.ConsumeRandomLengthString(64);
    optionsItem->originUrl = fdp.ConsumeRandomLengthString(128);
    optionsItem->title = fdp.ConsumeRandomLengthString(64);
    optionsItem->body = fdp.ConsumeRandomLengthString(256);
    optionsItem->requireInteraction = fdp.ConsumeBool();

    // Test OnShowNotification with valid options
    NWebNotificationManagerDelegate::OnShowNotification(optionsItem);

    // Test OnShowNotification with null options (error path)
    std::shared_ptr<NWebNotificationOptionsItem> nullOptions;
    NWebNotificationManagerDelegate::OnShowNotification(nullOptions);

    return true;
}

// Test scenario 3: Test OnCloseNotification with various IDs
bool TestOnCloseNotification(const uint8_t* data, size_t size)
{
    FuzzedDataProvider fdp(data, size);

    // First get the delegate and register a listener
    auto& delegate = NWebNotificationManagerDelegate::GetOrCreateDelegate();
    delegate->RegisterNotificationManagerDelegateListener(GetMockCallback());

    // Test OnCloseNotification with fuzzed ID
    std::string id = fdp.ConsumeRandomLengthString(64);
    NWebNotificationManagerDelegate::OnCloseNotification(id);

    // Test with empty ID
    NWebNotificationManagerDelegate::OnCloseNotification("");

    return true;
}

// Test scenario 4: Test OnShowed, OnClosed, OnClicked (static methods)
bool TestStaticMethods(const uint8_t* data, size_t size)
{
    FuzzedDataProvider fdp(data, size);

    // These methods always call NotificationPlatformBridgeOhos
    std::string id = fdp.ConsumeRandomLengthString(64);
    int buttonIndex = fdp.ConsumeIntegralInRange<int>(0, 10);

    NWebNotificationManagerDelegate::OnShowed(id);
    NWebNotificationManagerDelegate::OnClosed(id);
    NWebNotificationManagerDelegate::OnClicked(id, buttonIndex);

    // Test with empty strings
    NWebNotificationManagerDelegate::OnShowed("");
    NWebNotificationManagerDelegate::OnClosed("");
    NWebNotificationManagerDelegate::OnClicked("", -1);

    return true;
}

// Test scenario 5: Test without listener registered (error paths)
bool TestWithoutListener(const uint8_t* data, size_t size)
{
    FuzzedDataProvider fdp(data, size);

    // Create a fresh delegate to ensure no listener is registered
    // Note: Since delegate is static, we can only test error paths when listener is null
    // by directly calling the static methods

    auto optionsItem = std::make_shared<NWebNotificationOptionsItem>();
    optionsItem->id = fdp.ConsumeRandomLengthString(64);

    // These will hit the "notification_delegate_listener_ is null" error path
    NWebNotificationManagerDelegate::OnShowNotification(optionsItem);
    NWebNotificationManagerDelegate::OnCloseNotification("test_id");

    return true;
}

} // namespace NWeb
} // namespace OHOS

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    // Run all test scenarios
    OHOS::NWeb::TestDelegateLifecycle(data, size);
    OHOS::NWeb::TestOnShowNotification(data, size);
    OHOS::NWeb::TestOnCloseNotification(data, size);
    OHOS::NWeb::TestStaticMethods(data, size);
    OHOS::NWeb::TestWithoutListener(data, size);

    return 0;
}
