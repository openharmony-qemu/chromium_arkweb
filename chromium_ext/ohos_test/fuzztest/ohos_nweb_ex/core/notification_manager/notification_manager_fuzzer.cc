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

#include <memory>
#include <string>
#include <vector>

#define private public
#include "ohos_nweb_ex/core/notification_manager.cc"
#include "ohos_nweb/src/nweb_notification_manager_delegate.h"
#include "ohos_nweb/src/capi/nweb_notification_options_item.h"

using namespace OHOS::NWeb;

constexpr size_t MAX_STRING_LEN = 256;
constexpr size_t MAX_BUTTONS = 8;
constexpr size_t MAX_ICON_BITMAPS = 4;

static std::shared_ptr<NWebNotificationOptionsItem> g_lastShowedNotification;
static std::string g_lastClosedId;

void MockShowNotification(std::shared_ptr<NWebNotificationOptionsItem> optionsItem) {
    g_lastShowedNotification = optionsItem;
}

void MockCloseNotification(std::string id) {
    g_lastClosedId = id;
}

std::shared_ptr<NWebNotificationOptionsItem> CreateFuzzedNotificationItem(FuzzedDataProvider* fdp) {
    auto item = std::make_shared<NWebNotificationOptionsItem>();

    item->id = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
    item->originUrl = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
    item->title = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
    item->body = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
    item->requireInteraction = fdp->ConsumeBool();

    size_t numButtons = fdp->ConsumeIntegralInRange<size_t>(0, MAX_BUTTONS);
    for (size_t i = 0; i < numButtons; ++i) {
        item->buttons.push_back(fdp->ConsumeRandomLengthString(MAX_STRING_LEN));
    }

    if (fdp->ConsumeBool()) {
        auto icon = std::make_unique<NWebNotificationOptionsItemIcon>();
        size_t numBitmaps = fdp->ConsumeIntegralInRange<size_t>(0, MAX_ICON_BITMAPS);
        for (size_t i = 0; i < numBitmaps; ++i) {
            auto bitmap = std::make_unique<NWebNotificationOptionsItemIconBitmap>();
            bitmap->colorType = static_cast<NWebNotificationOptionsItemIconColorType>(
                fdp->ConsumeIntegral<int>());
            bitmap->alphaType = static_cast<NWebNotificationOptionsItemIconAlphaType>(
                fdp->ConsumeIntegral<int>());
            bitmap->width = fdp->ConsumeIntegral<int32_t>();
            bitmap->height = fdp->ConsumeIntegral<int32_t>();

            size_t bitmapSize = bitmap->width * bitmap->height * 4;
            if (bitmapSize > 0 && bitmapSize < 65536) {
                bitmap->bitmap = new uint8_t[bitmapSize];
                auto bytes = fdp->ConsumeBytes<uint8_t>(bitmapSize);
                memcpy(bitmap->bitmap, bytes.data(), std::min(bytes.size(), bitmapSize));
            }
            double scale = fdp->ConsumeFloatingPoint<float>();
            icon->bitmaps[scale] = bitmap.release();
        }
        item->icon = icon.release();
    }

    return item;
}

void FuzzCreateDelegateCallback() {
    NWebNotificationDelegateCallback* callback = nullptr;
    NotificationManager_CreateDelegateCallback(&callback);
    if (callback) {
        delete callback;
    }
}

void FuzzSetShowNotification() {
    NotificationManager_SetShowNotification(nullptr, MockShowNotification);

    NWebNotificationDelegateCallback callback;
    callback.showNotification = MockShowNotification;
    NotificationManager_SetShowNotification(&callback, MockShowNotification);
}

void FuzzSetCloseNotification() {
    NotificationManager_SetCloseNotification(nullptr, MockCloseNotification);

    NWebNotificationDelegateCallback callback;
    callback.closeNotification = MockCloseNotification;
    NotificationManager_SetCloseNotification(&callback, MockCloseNotification);
}

void FuzzPutNotificationCallback() {
    NotificationManager_PutNotificationCallback(nullptr);

    NWebNotificationDelegateCallback callback;
    callback.showNotification = MockShowNotification;
    callback.closeNotification = MockCloseNotification;
    NotificationManager_PutNotificationCallback(&callback);
}

void FuzzOnShowed(const std::string& id) {
    NotificationEventHandler_OnShowed(id);
}

void FuzzOnClosed(const std::string& id) {
    NotificationEventHandler_OnClosed(id);
}

void FuzzOnClicked(const std::string& id, int buttonIndex) {
    NotificationEventHandler_OnClicked(id, buttonIndex);
}

void FuzzNotificationManagerDelegate(FuzzedDataProvider* fdp) {
    auto& delegate = NWebNotificationManagerDelegate::GetOrCreateDelegate();
    if (!delegate) {
        return;
    }

    auto item = CreateFuzzedNotificationItem(fdp);
    if (item) {
        NWebNotificationManagerDelegate::OnShowNotification(item);
    }

    std::string closeId = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
    NWebNotificationManagerDelegate::OnCloseNotification(closeId);

    std::string showedId = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
    NWebNotificationManagerDelegate::OnShowed(showedId);

    NWebNotificationManagerDelegate::OnClosed(showedId);

    std::string clickId = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
    int btnIdx = fdp->ConsumeIntegral<int>();
    NWebNotificationManagerDelegate::OnClicked(clickId, btnIdx);
}

void FuzzWithNotificationItem(FuzzedDataProvider* fdp) {
    auto item = CreateFuzzedNotificationItem(fdp);
    if (!item) {
        return;
    }

    NWebNotificationDelegateCallback callback;
    callback.showNotification = MockShowNotification;
    callback.closeNotification = MockCloseNotification;

    auto& delegate = NWebNotificationManagerDelegate::GetOrCreateDelegate();
    if (delegate) {
        delegate->RegisterNotificationManagerDelegateListener(
            std::make_shared<NWebNotificationDelegateCallback>(callback));
        NWebNotificationManagerDelegate::OnShowNotification(item);
        NWebNotificationManagerDelegate::OnCloseNotification(item->id);
    }
}

void FuzzEventHandlerSequence(FuzzedDataProvider* fdp) {
    std::string id1 = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
    std::string id2 = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);

    NotificationEventHandler_OnShowed(id1);
    NotificationEventHandler_OnClicked(id1, 0);
    NotificationEventHandler_OnClosed(id1);

    NotificationEventHandler_OnShowed(id2);
    NotificationEventHandler_OnClicked(id2, -1);
    NotificationEventHandler_OnClicked(id2, INT_MAX);
    NotificationEventHandler_OnClosed(id2);

    NotificationEventHandler_OnShowed("");
    NotificationEventHandler_OnClosed("");
}

void FuzzMixedOperations(FuzzedDataProvider* fdp) {
    size_t numOps = fdp->ConsumeIntegralInRange<size_t>(1, 10);
    for (size_t i = 0; i < numOps; ++i) {
        uint8_t op = fdp->ConsumeIntegral<uint8_t>() % 12;
        switch (op) {
            case 0:
                FuzzCreateDelegateCallback();
                break;
            case 1:
                FuzzSetShowNotification();
                break;
            case 2:
                FuzzSetCloseNotification();
                break;
            case 3:
                FuzzPutNotificationCallback();
                break;
            case 4:
                FuzzOnShowed(fdp->ConsumeRandomLengthString(MAX_STRING_LEN));
                break;
            case 5:
                FuzzOnClosed(fdp->ConsumeRandomLengthString(MAX_STRING_LEN));
                break;
            case 6:
                FuzzOnClicked(
                    fdp->ConsumeRandomLengthString(MAX_STRING_LEN),
                    fdp->ConsumeIntegral<int>());
                break;
            case 7:
                FuzzWithNotificationItem(fdp);
                break;
            case 8:
                FuzzNotificationManagerDelegate(fdp);
                break;
            case 9:
                FuzzEventHandlerSequence(fdp);
                break;
            case 10: {
                auto item = CreateFuzzedNotificationItem(fdp);
                if (item) {
                    NWebNotificationManagerDelegate::OnShowNotification(item);
                }
                break;
            }
            case 11: {
                std::string id = fdp->ConsumeRandomLengthString(MAX_STRING_LEN);
                NWebNotificationManagerDelegate::OnCloseNotification(id);
                break;
            }
            default:
                break;
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (data == nullptr || size < 1) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);

    int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 9);

    switch (selector) {
        case 0:
            FuzzCreateDelegateCallback();
            break;
        case 1:
            FuzzSetShowNotification();
            break;
        case 2:
            FuzzSetCloseNotification();
            break;
        case 3:
            FuzzPutNotificationCallback();
            break;
        case 4:
            FuzzOnShowed(fdp.ConsumeRandomLengthString(MAX_STRING_LEN));
            break;
        case 5:
            FuzzOnClosed(fdp.ConsumeRandomLengthString(MAX_STRING_LEN));
            break;
        case 6:
            FuzzOnClicked(
                fdp.ConsumeRandomLengthString(MAX_STRING_LEN),
                fdp.ConsumeIntegral<int>());
            break;
        case 7:
            FuzzWithNotificationItem(&fdp);
            break;
        case 8:
            FuzzNotificationManagerDelegate(&fdp);
            break;
        case 9:
            FuzzMixedOperations(&fdp);
            break;
        default:
            FuzzPutNotificationCallback();
            break;
    }

    return 0;
}
