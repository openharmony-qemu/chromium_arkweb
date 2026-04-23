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

#include <cstdint>
#include <memory>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#define private public
#include "ohos_nweb/src/nweb_web_storage_impl.h"
#undef private

using namespace OHOS::NWeb;

namespace {

class MockWebStorageDelegate : public NWebWebStorageDelegateInterface {
public:
    void DeleteAllData(bool incognito_mode) override {}
    int DeleteOrigin(const std::string& origin) override { return 0; }
    void GetOrigins(
        std::shared_ptr<NWebWebStorageOriginVectorValueCallback> callback) override {}
    void GetOrigins(
        std::vector<std::shared_ptr<NWebWebStorageOrigin>>& origins) override {}
    void GetOriginQuota(
        const std::string& origin,
        std::shared_ptr<NWebLongValueCallback> callback) override {}
    long GetOriginQuota(const std::string& origin) override { return 0; }
    void GetOriginUsage(
        const std::string& origin,
        std::shared_ptr<NWebLongValueCallback> callback) override {}
    long GetOriginUsage(const std::string& origin) override { return 0; }

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
    void RegisterWebStorageExtensionCallback(
        std::shared_ptr<NWebStorageExtensionCallback> callback) override {}
    std::string GetPassword(const std::string& url,
                            const std::string& username,
                            int callback_id) override { return ""; }
    void GetSavedPasswordsInfo(int callback_id) override {}
    void MigratePasswordsInfo() override {}
    void ClearPassword() override {}
    void RemovePassword(const std::string& url,
                        const std::string& username) override {}
    void RemovePasswordByUrl(const std::string& url) override {}
    void ModifyPassword(const std::string& url,
                        const std::string& old_username,
                        const std::string& new_username,
                        const std::string& new_password) override {}
#endif  // BUILDFLAG(ARKWEB_EXT_PASSWORD)
};

class MockOriginVectorCallback
    : public NWebWebStorageOriginVectorValueCallback {
public:
    void OnReceiveValue(
        const std::vector<std::shared_ptr<NWebWebStorageOrigin>>& value) override {}
};

class MockLongCallback : public NWebLongValueCallback {
public:
    void OnReceiveValue(long value) override {}
};

void TestNullDelegate(FuzzedDataProvider* fdp)
{
    NWebWebStorageImpl impl;

    impl.DeleteAllData(fdp->ConsumeBool());

    std::string origin = fdp->ConsumeRandomLengthString(256);
    impl.DeleteOrigin(origin);

    auto origin_cb = std::make_shared<MockOriginVectorCallback>();
    impl.GetOrigins(origin_cb);

    impl.GetOrigins();

    std::string quota_origin = fdp->ConsumeRandomLengthString(256);
    auto long_cb = std::make_shared<MockLongCallback>();
    impl.GetOriginQuota(quota_origin, long_cb);
    impl.GetOriginQuota(quota_origin);

    std::string usage_origin = fdp->ConsumeRandomLengthString(256);
    impl.GetOriginUsage(usage_origin, long_cb);
    impl.GetOriginUsage(usage_origin);

#if BUILDFLAG(IS_OHOS)
    impl.PutWebStorageCallback(nullptr);

    std::string url = fdp->ConsumeRandomLengthString(256);
    std::string username = fdp->ConsumeRandomLengthString(64);
    int callback_id = fdp->ConsumeIntegralInRange<int>(0, 64);
    impl.GetPassword(url, username, callback_id);

    impl.GetSavedPasswords(callback_id);
    impl.MigratePasswords();
    impl.ClearPassword();
    impl.RemovePassword(url, username);

    std::string new_username = fdp->ConsumeRandomLengthString(64);
    std::string new_password = fdp->ConsumeRandomLengthString(64);
    impl.ModifyPassword(url, username, new_username, new_password);
    impl.RemovePasswordByUrl(url);
#endif  // BUILDFLAG(IS_OHOS)
}

void TestWithDelegate(FuzzedDataProvider* fdp)
{
    NWebWebStorageImpl impl;
    impl.delegate_ = std::make_shared<MockWebStorageDelegate>();

    impl.DeleteAllData(fdp->ConsumeBool());

    std::string origin = fdp->ConsumeRandomLengthString(256);
    impl.DeleteOrigin(origin);

    auto origin_cb = std::make_shared<MockOriginVectorCallback>();
    impl.GetOrigins(origin_cb);
    impl.GetOrigins();

    std::string quota_origin = fdp->ConsumeRandomLengthString(256);
    auto long_cb = std::make_shared<MockLongCallback>();
    impl.GetOriginQuota(quota_origin, long_cb);
    impl.GetOriginQuota(quota_origin);

    std::string usage_origin = fdp->ConsumeRandomLengthString(256);
    impl.GetOriginUsage(usage_origin, long_cb);
    impl.GetOriginUsage(usage_origin);

#if BUILDFLAG(IS_OHOS)
    impl.PutWebStorageCallback(nullptr);

    std::string url = fdp->ConsumeRandomLengthString(256);
    std::string username = fdp->ConsumeRandomLengthString(64);
    int callback_id = fdp->ConsumeIntegralInRange<int>(0, 64);
    impl.GetPassword(url, username, callback_id);

    impl.GetSavedPasswords(callback_id);
    impl.MigratePasswords();
    impl.ClearPassword();
    impl.RemovePassword(url, username);

    std::string new_username = fdp->ConsumeRandomLengthString(64);
    std::string new_password = fdp->ConsumeRandomLengthString(64);
    impl.ModifyPassword(url, username, new_username, new_password);
    impl.RemovePasswordByUrl(url);
#endif  // BUILDFLAG(IS_OHOS)
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);

    TestNullDelegate(&fdp);
    TestWithDelegate(&fdp);

    return 0;
}
