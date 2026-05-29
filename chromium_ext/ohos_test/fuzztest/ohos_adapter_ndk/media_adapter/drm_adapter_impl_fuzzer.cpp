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

#define private public
#include "arkweb/ohos_adapter_ndk/media_adapter/drm_adapter_impl.h"
#undef private

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fuzzer/FuzzedDataProvider.h>

#include "base/logging.h"

using namespace OHOS::NWeb;

namespace OHOS::NWeb {
    int32_t GetSecurityLevelFromContentProtectionLevel(int32_t levelData);
    DRM_ContentProtectionLevel GetContentProtectionLevelFromSecurityLevel(int32_t levelData);
}

namespace OHOS {
#define MAX_KEY_SET_ID_LEN 64
#define MAX_STR_LEN 32
#define MAX_ARRAY_LEN 64
#define MAX_INT_RANGE_MAX 64
#define MAX_INT_RANGE_MIN 1
#define SECURITY_LEVEL_MIN 0
#define SECURITY_LEVEL_MAX 3
#define CERT_STATUS_MIN 0
#define CERT_STATUS_MAX 4
#ifndef MAX_KEYS_INFO_COUNT
#define MAX_KEYS_INFO_COUNT 16
#endif

class DrmCallbackAdapterMock : public DrmCallbackAdapter {
public:
    DrmCallbackAdapterMock() = default;
    ~DrmCallbackAdapterMock() override = default;
    void OnSessionMessage(const std::string& sessionId, int32_t& type, const std::vector<uint8_t>& message) override {}

    void OnProvisionRequest(const std::string& defaultUrl, const std::string& requestData) override {}

    void OnProvisioningComplete(bool success) override {}

    void OnMediaKeySessionReady(void* session) override {}

    void OnPromiseRejected(uint32_t promiseId, const std::string& errorMessage) override {}

    void OnPromiseResolved(uint32_t promiseId) override {}

    void OnPromiseResolvedWithSession(uint32_t promiseId, const std::string& sessionId) override {}

    void OnSessionClosed(const std::string& sessionId) override {}

    void OnSessionKeysChange(const std::string& sessionId, const std::vector<std::string>& keyIdArray,
        const std::vector<uint32_t>& statusArray, bool hasAdditionalUsableKey, bool isKeyRelease) override {}

    void OnSessionExpirationUpdate(const std::string& sessionId, uint64_t expirationTime) override {}

    void OnStorageProvisioned() override {}

    void OnStorageSaveInfo(const std::vector<uint8_t>& ketSetId, const std::string& mimeType,
        const std::string& sessionId, int32_t keyType) override {}

    void OnStorageLoadInfo(const std::string& sessionId) override {}

    void OnStorageClearInfoForKeyRelease(const std::string& sessionId) override {}

    void OnStorageClearInfoForLoadFail(const std::string& sessionId) override {}

    void OnMediaLicenseReady(bool success) override {}
};

bool g_isSupportDrm = true;
auto mockCallback_ = std::make_shared<DrmCallbackAdapterMock>();

static const std::string GetKeySystemName()
{
    // TODO: realize get media session
    g_isSupportDrm = true;
    return "com.wiseplay.drm";
}

void DrmAdapterImpl__CreateSessionId(FuzzedDataProvider* fdp)
{
    auto session1 = SessionId::CreateSessionId("");

    std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    auto session2 = SessionId::CreateSessionId(sessionId);
}

void DrmAdapterImpl__SessionIdMethods(FuzzedDataProvider* fdp)
{
    std::string emeId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    size_t random_length = fdp->ConsumeIntegralInRange<size_t>(MAX_INT_RANGE_MIN, MAX_INT_RANGE_MAX);
    std::vector<uint8_t> keySetId(random_length);
    fdp->ConsumeData(keySetId.data(), random_length);
    auto session = NWeb::SessionId::CreateSessionId(emeId);

    session->EmeId();
    session->SetKeySetId(keySetId.data(), random_length);
    session->KeySetId();
    session->KeySetIdLen();

    // Cover keySetIdLen > MAX_KEY_SET_ID_LEN branch
    std::vector<uint8_t> longKeySetId(MAX_KEY_SET_ID_LEN + 1, 0x01);
    session->SetKeySetId(longKeySetId.data(), longKeySetId.size());

    // Cover keySetId == nullptr && keySetIdLen <= 0 branch
    session->SetKeySetId(nullptr, 0);
}

void DrmAdapterImpl__SessionInfoMethods(FuzzedDataProvider* fdp)
{
    std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    std::string mimeType = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    int32_t keyType = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
    auto session = SessionId::CreateSessionId(sessionId);
    auto sessionInfo = SessionInfo(session, mimeType, keyType);
    sessionInfo.MimeType();
    sessionInfo.KeyType();
    sessionInfo.IsRelease();
    sessionInfo.GetSessionId();

    sessionInfo.SetKeyType(keyType);
}

void GetMediaKeySession(MediaKeySession** keySession)
{
    // TODO: realize get media session
}

void DrmCallbackImpl__TestFunctionsOne(FuzzedDataProvider* fdp)
{
    std::shared_ptr<DrmCallbackAdapterMock>  cbTest = std::make_shared<DrmCallbackAdapterMock>();
    DrmCallbackImpl cb(cbTest);

    {
        std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
        int32_t type = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
        size_t random_length = fdp->ConsumeIntegralInRange<size_t>(MAX_INT_RANGE_MIN, MAX_INT_RANGE_MAX);
        std::vector<uint8_t> message(random_length);
        fdp->ConsumeData(message.data(), random_length);

        cb.OnSessionMessage(sessionId, type, message);
    }

    {
       std::string url = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
       std::string requestData = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
       cb.OnProvisionRequest(url, requestData);
    }

    {
        bool succeed = fdp->ConsumeBool();
        cb.OnProvisioningComplete(succeed);
    }

    {
        MediaKeySession* keySession = nullptr;
        GetMediaKeySession(&keySession);
        if (keySession == nullptr) {
            cb.OnMediaKeySessionReady(keySession);
        } else {
            cb.OnMediaKeySessionReady(reinterpret_cast<OHOSMediaKeySession>(keySession));
        }
    }

    {
        uint32_t promiseId = fdp->ConsumeIntegral<uint32_t>();
        std::string errorMessage = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
        cb.OnPromiseRejected(promiseId, errorMessage);
    }
}

void DrmCallbackImpl__TestFunctionsTwo(FuzzedDataProvider* fdp)
{
    std::shared_ptr<DrmCallbackAdapter>  cbTest = std::make_shared<DrmCallbackAdapterMock>();
    DrmCallbackImpl cb(cbTest);
    std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    uint32_t promiseId = fdp->ConsumeIntegral<uint32_t>();

    cb.OnStorageProvisioned();
    cb.OnStorageLoadInfo(sessionId);
    cb.OnSessionClosed(sessionId);
    cb.OnPromiseResolved(promiseId);
    cb.OnPromiseResolvedWithSession(promiseId, sessionId);

    {
        uint64_t expirationTime = fdp->ConsumeIntegral<uint64_t>();
        cb.OnSessionExpirationUpdate(sessionId, expirationTime);
    }
}

void DrmCallbackImpl__OnSessionKeysChange(FuzzedDataProvider* fdp)
{
    std::shared_ptr<DrmCallbackAdapter> cbTest = std::make_shared<DrmCallbackAdapterMock>();
    DrmCallbackImpl cb(cbTest);
    std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    std::vector<std::string> keyIdArray;
    std::vector<uint32_t> statusArray;
    uint32_t count = fdp->ConsumeIntegralInRange<uint32_t>(0, MAX_ARRAY_LEN);
    for (uint32_t i = 0; i < count; ++i) {
        keyIdArray.push_back(fdp->ConsumeRandomLengthString(MAX_STR_LEN));
        statusArray.push_back(fdp->ConsumeIntegral<uint32_t>());
    }
    bool hasAdditionalUsableKey = fdp->ConsumeBool();
    bool isKeyRelease = fdp->ConsumeBool();
    cb.OnSessionKeysChange(sessionId, keyIdArray, statusArray, hasAdditionalUsableKey, isKeyRelease);

    // Test with null callbackAdapter
    DrmCallbackImpl cb2(nullptr);
    cb2.OnSessionKeysChange(sessionId, keyIdArray, statusArray, hasAdditionalUsableKey, isKeyRelease);
}

void DrmCallbackImpl__OnStorageSaveInfo(FuzzedDataProvider* fdp)
{
    std::shared_ptr<DrmCallbackAdapter> cbTest = std::make_shared<DrmCallbackAdapterMock>();
    DrmCallbackImpl cb(cbTest);
    size_t randomLength = fdp->ConsumeIntegralInRange<size_t>(MAX_INT_RANGE_MIN, MAX_KEY_SET_ID_LEN);
    std::vector<uint8_t> ketSetId(randomLength);
    fdp->ConsumeData(ketSetId.data(), randomLength);
    std::string mimeType = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    int32_t keyType = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
    cb.OnStorageSaveInfo(ketSetId, mimeType, sessionId, keyType);

    // Test with null callbackAdapter
    DrmCallbackImpl cb2(nullptr);
    cb2.OnStorageSaveInfo(ketSetId, mimeType, sessionId, keyType);
}

void DrmCallbackImpl__TestFunctionsThree(FuzzedDataProvider* fdp)
{
    std::shared_ptr<DrmCallbackAdapter> cbTest = std::make_shared<DrmCallbackAdapterMock>();
    DrmCallbackImpl cb(cbTest);
    std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    bool success = fdp->ConsumeBool();
    cb.OnStorageClearInfoForKeyRelease(sessionId);
    cb.OnStorageClearInfoForLoadFail(sessionId);
    cb.OnMediaLicenseReady(success);

    // Test with null callbackAdapter
    DrmCallbackImpl cb2(nullptr);
    cb2.OnStorageClearInfoForKeyRelease(sessionId);
    cb2.OnStorageClearInfoForLoadFail(sessionId);
    cb2.OnMediaLicenseReady(success);
}

void DrmCallbackImpl__UpdateMediaKeySessionInfoMap(FuzzedDataProvider* fdp)
{
    std::shared_ptr<DrmCallbackAdapter> cbTest = std::make_shared<DrmCallbackAdapterMock>();
    DrmCallbackImpl cb(cbTest);

    MediaKeySession* keySession = nullptr;
    GetMediaKeySession(&keySession);
    std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    std::string mimeType = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    int32_t keyType = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
    auto session = SessionId::CreateSessionId(sessionId);

    std::shared_ptr<SessionInfo>  sessionInfo = make_shared<SessionInfo>(session, mimeType, keyType);

    cb.UpdateMediaKeySessionInfoMap(keySession, sessionInfo);
}

void DrmCallbackImpl__TestFunctionsFour(FuzzedDataProvider* fdp)
{
    std::shared_ptr<DrmCallbackAdapter> cbTest = std::make_shared<DrmCallbackAdapterMock>();
    DrmCallbackImpl cb(cbTest);

    MediaKeySession* keySession = nullptr;
    GetMediaKeySession(&keySession);

    cb.GetMediaKeySessionInfo(keySession);
    cb.RemoveMediaKeySessionInfo(keySession);
    cb.ClearMediaKeySessionInfo();

    // Test with non-null keySession in map
    MediaKeySession* fakeSession = reinterpret_cast<MediaKeySession*>(0x3333);
    auto sessionInfo = std::make_shared<SessionInfo>(SessionId::CreateSessionId("test"), "video/mp4", 0);
    cb.UpdateMediaKeySessionInfoMap(fakeSession, sessionInfo);
    cb.GetMediaKeySessionInfo(fakeSession);
    cb.RemoveMediaKeySessionInfo(fakeSession);
}

void GetMediaKeySystem(MediaKeySystem** keySession)
{
    // TODO: realize get media key system
}

void DrmAdapterImpl__SystemCallBackWithObj(FuzzedDataProvider* fdp)
{
    char extra1[] = "extra";
    // Test with nullptr system
    {
        MediaKeySystem* mediaKeySystem = nullptr;
        int32_t tmpInt = fdp->ConsumeIntegralInRange<int32_t>(200, 206);
        DRM_EventType eventType = static_cast<DRM_EventType>(tmpInt);
        size_t infoLen = fdp->ConsumeIntegralInRange<size_t>(MAX_INT_RANGE_MIN, MAX_INT_RANGE_MAX);
        std::vector<uint8_t> info(infoLen);
        fdp->ConsumeData(info.data(), infoLen);
        std::string extra = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
        DrmAdapterImpl::SystemCallBackWithObj(mediaKeySystem, eventType, info.data(), infoLen, const_cast<char*>(extra.data()));
    }

    // Test with system not in map
    {
        MediaKeySystem* fakeSystem = reinterpret_cast<MediaKeySystem*>(0xBBBB);
        std::vector<uint8_t> info(16);
        fdp->ConsumeData(info.data(), info.size());
        DrmAdapterImpl::SystemCallBackWithObj(fakeSystem, EVENT_PROVISION_REQUIRED, info.data(), info.size(), extra1);
    }

    // Test with system in map but callback is nullptr
    {
        MediaKeySystem* fakeSystem = reinterpret_cast<MediaKeySystem*>(0xCCCC);
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySystemCallbackMapMutex_);
            DrmAdapterImpl::mediaKeySystemCallbackMap_[fakeSystem] = nullptr;
        }
        std::vector<uint8_t> info(16);
        fdp->ConsumeData(info.data(), info.size());
        DrmAdapterImpl::SystemCallBackWithObj(fakeSystem, EVENT_PROVISION_REQUIRED, info.data(), info.size(), extra1);

        // Clean up
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySystemCallbackMapMutex_);
            DrmAdapterImpl::mediaKeySystemCallbackMap_.erase(fakeSystem);
        }
    }

    // Test with valid callback but non-PROVISION_REQUIRED event
    {
        auto callback = std::make_shared<DrmCallbackImpl>(mockCallback_);
        MediaKeySystem* fakeSystem = reinterpret_cast<MediaKeySystem*>(0xDDDD);
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySystemCallbackMapMutex_);
            DrmAdapterImpl::mediaKeySystemCallbackMap_[fakeSystem] = callback;
        }
        std::vector<uint8_t> info(16);
        fdp->ConsumeData(info.data(), info.size());
        DrmAdapterImpl::SystemCallBackWithObj(fakeSystem, EVENT_DRM_BASE, info.data(), info.size(), extra1);

        // Clean up
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySystemCallbackMapMutex_);
            DrmAdapterImpl::mediaKeySystemCallbackMap_.erase(fakeSystem);
        }
    }
}

void DrmAdapterImpl__SessionEventCallBackWithObj(FuzzedDataProvider* fdp)
{
    char extra1[] = "extra";
    // Test with nullptr session and random eventType (covers default branch)
    {
        MediaKeySession* mediaKeySession = nullptr;
        int32_t tmpInt = fdp->ConsumeIntegralInRange<int32_t>(200, 206);
        DRM_EventType eventType = static_cast<DRM_EventType>(tmpInt);
        size_t infoLen = fdp->ConsumeIntegralInRange<size_t>(MAX_INT_RANGE_MIN, MAX_INT_RANGE_MAX);
        std::vector<uint8_t> info(infoLen);
        fdp->ConsumeData(info.data(), infoLen);
        std::string extra = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
        DrmAdapterImpl::SessionEventCallBackWithObj(mediaKeySession, eventType, info.data(), infoLen, const_cast<char*>(extra.data()));
    }

    // Test EVENT_EXPIRATION_UPDATE with fake session
    {
        auto callback = std::make_shared<DrmCallbackImpl>(mockCallback_);
        MediaKeySession* fakeSession = reinterpret_cast<MediaKeySession*>(0x2222);
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_[fakeSession] = callback;
        }
        auto sessionId = SessionId::CreateSessionId("event_session");
        auto sessionInfo = std::make_shared<SessionInfo>(sessionId, "video/mp4", 0);
        callback->mediaKeySessionInfoMap_[fakeSession] = sessionInfo;

        std::string infoStr = "1234ms";
        DrmAdapterImpl::SessionEventCallBackWithObj(fakeSession, EVENT_EXPIRATION_UPDATE,
            reinterpret_cast<uint8_t*>(const_cast<char*>(infoStr.data())), infoStr.size(), extra1);

        // Test EVENT_KEY_REQUIRED with long mimeType to avoid OH_ call
        auto longMimeSessionId = SessionId::CreateSessionId("keyreq_event");
        std::string longMimeType(256, 'x');
        auto longMimeInfo = std::make_shared<SessionInfo>(longMimeSessionId, longMimeType, 0);
        callback->mediaKeySessionInfoMap_[fakeSession] = longMimeInfo;
        std::vector<uint8_t> info(16);
        DrmAdapterImpl::SessionEventCallBackWithObj(fakeSession, EVENT_KEY_REQUIRED, info.data(), info.size(), extra1);

        // Clean up
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_.erase(fakeSession);
        }
    }
}

void DrmAdapterImpl__SessionKeyChangeCallBackWithObj(FuzzedDataProvider* fdp)
{
    // Test with nullptr keysInfo
    DrmAdapterImpl::SessionKeyChangeCallBackWithObj(nullptr, nullptr, false);

    // Test with session not in map
    {
        DRM_KeysInfo keysInfo;
        keysInfo.keysInfoCount = fdp->ConsumeIntegralInRange<uint32_t>(MAX_INT_RANGE_MIN, MAX_ARRAY_LEN);
        for (uint32_t i = 0; i < keysInfo.keysInfoCount && i < MAX_KEYS_INFO_COUNT; ++i) {
            fdp->ConsumeData(keysInfo.keyId[i], MAX_KEY_ID_LEN);
            std::string status = fdp->ConsumeRandomLengthString(16);
            strncpy(keysInfo.statusValue[i], status.c_str(), sizeof(keysInfo.statusValue[i]) - 1);
            keysInfo.statusValue[i][sizeof(keysInfo.statusValue[i]) - 1] = '\0';
        }
        bool newKeysAvailable = fdp->ConsumeBool();
        MediaKeySession* fakeSession = reinterpret_cast<MediaKeySession*>(0xEEEE);
        DrmAdapterImpl::SessionKeyChangeCallBackWithObj(fakeSession, &keysInfo, newKeysAvailable);
    }

    // Test with session in map and callback is nullptr
    {
        MediaKeySession* fakeSession = reinterpret_cast<MediaKeySession*>(0xFFFF);
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_[fakeSession] = nullptr;
        }
        DRM_KeysInfo keysInfo;
        keysInfo.keysInfoCount = 1;
        memset(keysInfo.keyId[0], 0x01, MAX_KEY_ID_LEN);
        strncpy(keysInfo.statusValue[0], "USABLE", sizeof(keysInfo.statusValue[0]) - 1);
        keysInfo.statusValue[0][sizeof(keysInfo.statusValue[0]) - 1] = '\0';
        DrmAdapterImpl::SessionKeyChangeCallBackWithObj(fakeSession, &keysInfo, true);

        // Clean up
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_.erase(fakeSession);
        }
    }

    // Test with valid callback and sessionInfo in map
    {
        auto callback = std::make_shared<DrmCallbackImpl>(mockCallback_);
        MediaKeySession* fakeSession = reinterpret_cast<MediaKeySession*>(0x1111);
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_[fakeSession] = callback;
        }
        auto sessionId = SessionId::CreateSessionId("keychange_session");
        auto sessionInfo = std::make_shared<SessionInfo>(sessionId, "video/mp4", static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_RELEASE));
        callback->mediaKeySessionInfoMap_[fakeSession] = sessionInfo;

        DRM_KeysInfo keysInfo;
        keysInfo.keysInfoCount = 3;
        for (uint32_t i = 0; i < 3; ++i) {
            memset(keysInfo.keyId[i], 0xAB + i, MAX_KEY_ID_LEN);
        }
        strncpy(keysInfo.statusValue[0], "USABLE", sizeof(keysInfo.statusValue[0]) - 1);
        strncpy(keysInfo.statusValue[1], "EXPIRED", sizeof(keysInfo.statusValue[1]) - 1);
        strncpy(keysInfo.statusValue[2], "UNKNOWN", sizeof(keysInfo.statusValue[2]) - 1);
        keysInfo.statusValue[0][sizeof(keysInfo.statusValue[0]) - 1] = '\0';
        keysInfo.statusValue[1][sizeof(keysInfo.statusValue[1]) - 1] = '\0';
        keysInfo.statusValue[2][sizeof(keysInfo.statusValue[2]) - 1] = '\0';
        DrmAdapterImpl::SessionKeyChangeCallBackWithObj(fakeSession, &keysInfo, true);

        // Test with null sessionId inside sessionInfo (use SessionInfo with nullptr sessionId)
        auto nullSessionInfo = std::make_shared<SessionInfo>(nullptr, "video/mp4", 0);
        callback->mediaKeySessionInfoMap_[fakeSession] = nullSessionInfo;
        DrmAdapterImpl::SessionKeyChangeCallBackWithObj(fakeSession, &keysInfo, false);

        // Clean up
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_.erase(fakeSession);
        }
    }
}

void DrmAdapterImpl__IsSupported(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    std::string name = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    std::string mimeType = fdp->ConsumeRandomLengthString(16);
    int32_t level = fdp->ConsumeIntegral<int32_t>();
    drmAdapter.IsSupported(name);
    drmAdapter.IsSupported2(name, mimeType);
    drmAdapter.IsSupported3(name, mimeType, level);
    drmAdapter.IsSupported("name");
    drmAdapter.IsSupported2("name", "mimeType");
    drmAdapter.IsSupported3("name", "mimeType", level);
    drmAdapter.GetUUID("name");
    drmAdapter.GetUUID(GetKeySystemName());

    // Cover empty string branches
    drmAdapter.IsSupported("");
    drmAdapter.IsSupported2("", mimeType);
    drmAdapter.IsSupported2("name", "");
    drmAdapter.IsSupported3("", mimeType, level);
    drmAdapter.IsSupported3("name", "", level);
}

void DrmAdapterImpl__StorageProvisionedResult(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    bool result = fdp->ConsumeBool();
    drmAdapter.StorageProvisionedResult(result);
    drmAdapter.StorageProvisionedResult(false);
    if (g_isSupportDrm) {
        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
        drmAdapter.RegistDrmCallback(mockCallback_);
        drmAdapter.contentProtectionLevel_ = CONTENT_PROTECTION_LEVEL_SW_CRYPTO;
        int32_t certStatus = -1;
        drmAdapter.GetCertificateStatus(certStatus);
        if (certStatus == 0) {
            drmAdapter.StorageProvisionedResult(true);
            drmAdapter.ReleaseMediaKeySession();
            drmAdapter.StorageProvisionedResult(true);
        }
        drmAdapter.ReleaseMediaKeySession();
        drmAdapter.ReleaseMediaKeySystem();
    }
}

void DrmAdapterImpl__StorageSaveInfoResult(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    bool result = fdp->ConsumeBool();
    int32_t type = fdp->ConsumeIntegral<int32_t>();
    drmAdapter.StorageSaveInfoResult(result, type);
    drmAdapter.StorageSaveInfoResult(true, 1);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.StorageSaveInfoResult(true, 1);

    // Cover !result branch with callback registered
    drmAdapter.StorageSaveInfoResult(false, 1);

    // Cover RELEASE type branch with various scenarios
    if (g_isSupportDrm) {
        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
        drmAdapter.RegistDrmCallback(mockCallback_);
        auto validSessionId = std::make_shared<SessionId>("release_eme", nullptr, 0);
        std::string mimeType = "video/mp4";
        drmAdapter.PutSessionInfo(validSessionId, mimeType, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
        drmAdapter.RemoveSession(1, "release_eme");
        drmAdapter.StorageSaveInfoResult(true, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_RELEASE));

        // Test with no drmKeySystem
        drmAdapter.ReleaseMediaKeySystem();
        drmAdapter.StorageSaveInfoResult(true, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_RELEASE));

        // Test sessionId == nullptr branch by setting releaseEmeId_ to non-existent session
        drmAdapter.releaseEmeId_ = "nonexistent_session";
        drmAdapter.StorageSaveInfoResult(true, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_RELEASE));

        // Test drmKeySession == nullptr branch (WISEPLAY with released session)
        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
        auto sessionWithKey = std::make_shared<SessionId>("session_with_key", nullptr, 0);
        drmAdapter.PutSessionInfo(sessionWithKey, "video/mp4", static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
        drmAdapter.RemoveSession(2, "session_with_key");
        drmAdapter.ReleaseMediaKeySession();
        drmAdapter.StorageSaveInfoResult(true, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_RELEASE));

        drmAdapter.ReleaseMediaKeySession();
    }
}

void DrmAdapterImpl__StorageClearInfoResult(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    bool result = fdp->ConsumeBool();
    int32_t type = fdp->ConsumeIntegral<int32_t>();
    drmAdapter.StorageClearInfoResult(result, type);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.StorageClearInfoResult(true, static_cast<int32_t>(ClearInfoType::KEY_RELEASE));
    drmAdapter.StorageClearInfoResult(false, static_cast<int32_t>(ClearInfoType::KEY_RELEASE));
    drmAdapter.StorageClearInfoResult(false, static_cast<int32_t>(ClearInfoType::LOAD_FAIL));
    drmAdapter.StorageClearInfoResult(true, static_cast<int32_t>(ClearInfoType::LOAD_FAIL));
    drmAdapter.callback_ = nullptr;
    drmAdapter.StorageClearInfoResult(true, static_cast<int32_t>(ClearInfoType::LOAD_FAIL));
}

void DrmAdapterImpl__ProcessKeySystemResponse(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    std::string response = fdp->ConsumeRandomLengthString(32);
    bool isResponseReceived = fdp->ConsumeBool();
    drmAdapter.ProcessKeySystemResponse(response, isResponseReceived);

    std::string KeySystemResponse = "response";
    drmAdapter.ProcessKeySystemResponse("", false);
    drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
    drmAdapter.ProcessKeySystemResponse(KeySystemResponse, false);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.ProcessKeySystemResponse(KeySystemResponse, false);

    // Cover responseLen > MAX_PROVISION_RESPONSE_LEN branch
    std::string longResponse(16 * 1024 + 1, 'x');
    drmAdapter.ProcessKeySystemResponse(longResponse, true);

    // Cover isResponseReceived = true with valid length response
    drmAdapter.ProcessKeySystemResponse("valid_response", true);

    // Cover isResponseReceived = true but response is empty (responseLen == 0, goes to else branch)
    drmAdapter.ProcessKeySystemResponse("", true);

    drmAdapter.ReleaseMediaKeySystem();
}

void DrmAdapterImpl__StorageLoadInfoResult(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    std::string sessionId = fdp->ConsumeRandomLengthString(16);
    std::vector<uint8_t> keySetId = fdp->ConsumeBytes<uint8_t>(16);
    std::string mimeType = fdp->ConsumeRandomLengthString(16);
    uint32_t keyType = fdp->ConsumeIntegralInRange<uint32_t>(0, 2);
    drmAdapter.StorageLoadInfoResult(sessionId, keySetId, mimeType, keyType);
    keySetId.clear();
    drmAdapter.callback_ = nullptr;
    drmAdapter.StorageLoadInfoResult("", keySetId, "", 0);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.StorageLoadInfoResult("", keySetId, "", 0);
    drmAdapter.callback_ = nullptr;
    std::vector<uint8_t> keySetId2 = { 0x01, 0x02 };
    drmAdapter.StorageLoadInfoResult("testEmeId063", keySetId2, "", 0);
    drmAdapter.StorageLoadInfoResult("testEmeId063", keySetId2, "", 0);
}

void DrmAdapterImpl__CreateKeySystem(FuzzedDataProvider* fdp)
{
    std::string name = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    std::string origin = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    int32_t securityLevel = fdp->ConsumeIntegralInRange<int32_t>(SECURITY_LEVEL_MIN, SECURITY_LEVEL_MAX);
    DrmAdapterImpl drmAdapter;
    drmAdapter.CreateKeySystem(name, origin, securityLevel);
}

void DrmAdapterImpl__GenerateMediaKeyRequest(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    auto mockCallback = std::make_shared<DrmCallbackAdapterMock>();
    std::string sessionId = fdp->ConsumeRandomLengthString(16);
    int32_t type = fdp->ConsumeIntegral<int32_t>();
    int32_t initDataLen = fdp->ConsumeIntegralInRange<int32_t>(1, 64);
    std::vector<uint8_t> initData(initDataLen);
    fdp->ConsumeData(initData.data(), initDataLen);
    std::string mimeType = fdp->ConsumeRandomLengthString(16);
    uint32_t promiseId = fdp->ConsumeIntegral<uint32_t>();
    // Covers !callback_ branch and drmKeySystem_ == nullptr branch
    drmAdapter.GenerateMediaKeyRequest(sessionId, type, initDataLen, initData, mimeType, promiseId);

    std::string emeId = "invalidEmeId";
    type = 1;
    initDataLen = 128;
    initData.resize(128);
    uint32_t optionsCount = 1;
    drmAdapter.RegistDrmCallback(mockCallback);
    // Covers drmKeySystem_ == nullptr branch with callback
    drmAdapter.GenerateMediaKeyRequest(emeId, type, initDataLen, initData, "video/avc", optionsCount);
    drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
    // Covers long mimeType to trigger memcpy_s failure
    drmAdapter.GenerateMediaKeyRequest(emeId, type, initDataLen, initData,
                                       "test_mime_type_long_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
                                       optionsCount);
    // Covers WIDEVINE keySystemType branch
    drmAdapter.keySystemType_ = KeySystemType::WIDEVINE;
    drmAdapter.GenerateMediaKeyRequest(emeId, type, initDataLen, initData, "video/avc", optionsCount);
    drmAdapter.ReleaseMediaKeySystem();
}

void DrmAdapterImpl__UpdateSession(FuzzedDataProvider* fdp)
{
    std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    int32_t len = fdp->ConsumeIntegralInRange<int32_t>(1, 64);
    std::vector<uint8_t> response(len);
    fdp->ConsumeData(response.data(), len);
    uint32_t promiseId = fdp->ConsumeIntegral<uint32_t>();
    DrmAdapterImpl drmAdapter;
    drmAdapter.UpdateSession(promiseId, sessionId, response);
    promiseId = 1;
    std::string emeId = "test_eme_id";
    response = { 0x01, 0x02, 0x03 };
    auto validSessionId = std::make_shared<SessionId>("test_eme_id", nullptr, 0);
    std::string mimeType = "video/mp4";
    drmAdapter.UpdateSession(promiseId, emeId, response);
    drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
    drmAdapter.UpdateSession(promiseId, emeId, response);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.PutSessionInfo(validSessionId, mimeType, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_ONLINE));
    drmAdapter.UpdateSession(promiseId, emeId, response);
    drmAdapter.PutSessionInfo(validSessionId, mimeType, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_RELEASE));
    drmAdapter.UpdateSession(promiseId, emeId, response);
    drmAdapter.UpdateSession(promiseId, "invalid_eme_id", response);

    // Cover sessionId == nullptr case by removing info before update
    drmAdapter.PutSessionInfo(validSessionId, mimeType, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_ONLINE));
    drmAdapter.RemoveSessionInfo(validSessionId);
    drmAdapter.UpdateSession(promiseId, emeId, response);

    // Cover info == nullptr case (via GetSessionInfo returning nullptr in UpdateSession)
    // Actually removing sessionInfo makes GetSessionIdByEmeId return nullptr too
    // Let's test with WISEPLAY keySystemType to cover OnMediaLicenseReady branch
    drmAdapter.PutSessionInfo(validSessionId, mimeType, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_ONLINE));
    drmAdapter.keySystemType_ = KeySystemType::WISEPLAY;
    drmAdapter.UpdateSession(promiseId, emeId, response);

    drmAdapter.RemoveSessionInfo(validSessionId);
    drmAdapter.ReleaseMediaKeySession();
    drmAdapter.ReleaseMediaKeySystem();
}

void DrmAdapterImpl__CloseSession(FuzzedDataProvider* fdp)
{
    std::string emeId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    uint32_t promiseId = fdp->ConsumeIntegral<uint32_t>();
    DrmAdapterImpl drmAdapter;
    drmAdapter.CloseSession(promiseId, emeId);
    auto validSessionId = std::make_shared<SessionId>("test_eme_id", nullptr, 0);
    promiseId = 1;
    emeId = "test_eme_id";
    drmAdapter.CloseSession(promiseId, emeId);
    if (g_isSupportDrm) {
        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
        drmAdapter.CloseSession(promiseId, emeId);
        std::string mimeType = "video/mp4";
        int32_t sessionType = 1;
        drmAdapter.PutSessionInfo(validSessionId, mimeType, sessionType);
        drmAdapter.CloseSession(promiseId, emeId);

        // Cover sessionId == nullptr case (no session info in map)
        drmAdapter.CloseSession(promiseId, "nonexistent_eme_id");

        drmAdapter.RemoveSessionInfo(validSessionId);
        drmAdapter.PutSessionInfo(validSessionId, mimeType, sessionType);
        drmAdapter.keySystemType_ = KeySystemType::WIDEVINE;
        drmAdapter.CloseSession(promiseId, emeId);
        drmAdapter.ReleaseMediaKeySession();
        drmAdapter.ReleaseMediaKeySystem();
    }
}

void DrmAdapterImpl__RemoveSession(FuzzedDataProvider* fdp)
{
    std::string emeId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    uint32_t promiseId = fdp->ConsumeIntegral<uint32_t>();
    DrmAdapterImpl drmAdapter;

    // Test with null callback
    drmAdapter.RemoveSession(promiseId, emeId);

    // Test with callback but session doesn't exist
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.RemoveSession(promiseId, emeId);

    if (g_isSupportDrm) {
        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
        auto validSessionId = std::make_shared<SessionId>("test_eme_id", nullptr, 0);
        std::string mimeType = "video/mp4";

        // ONLINE type - should reject
        drmAdapter.PutSessionInfo(validSessionId, mimeType, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_ONLINE));
        drmAdapter.RemoveSession(promiseId, "test_eme_id");

        // OFFLINE type - should proceed (set key type to RELEASE)
        drmAdapter.RemoveSessionInfo(validSessionId);
        drmAdapter.PutSessionInfo(validSessionId, mimeType, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
        drmAdapter.RemoveSession(promiseId, "test_eme_id");

        // Test sessionInfo nullptr case via emeSessionInfoMap_ manipulation
        drmAdapter.emeSessionInfoMap_["null_info"] = nullptr;
        drmAdapter.RemoveSession(promiseId, "null_info");

        drmAdapter.ReleaseMediaKeySession();
        drmAdapter.ReleaseMediaKeySystem();
    }
}

void DrmAdapterImpl__LoadSession(FuzzedDataProvider* fdp)
{
    std::string sessionId = fdp->ConsumeRandomLengthString(MAX_STR_LEN);
    uint32_t promiseId = fdp->ConsumeIntegral<uint32_t>();
    DrmAdapterImpl drmAdapter;
    drmAdapter.LoadSession(promiseId, sessionId);
    promiseId = 1;
    sessionId = "sessionId";
    drmAdapter.LoadSession(promiseId, sessionId);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
    drmAdapter.LoadSession(promiseId, sessionId);
    drmAdapter.ReleaseMediaKeySession();
    drmAdapter.ReleaseMediaKeySystem();
}

void DrmAdapterImpl__SetConfigurationString(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    std::string configName = fdp->ConsumeRandomLengthString(16);
    std::string value = fdp->ConsumeRandomLengthString(32);
    drmAdapter.SetConfigurationString(configName, value);
    drmAdapter.SetConfigurationString("", "");
    drmAdapter.SetConfigurationString("version", "");
    drmAdapter.SetConfigurationString("version", "2.0");
    uint8_t description[4] = { 0x00, 0x00, 0x00, 0x00 };
    int32_t valueLen = sizeof(description);
    drmAdapter.SetConfigurationByteArray("", description, valueLen);
    drmAdapter.SetConfigurationByteArray("description", nullptr, valueLen);
    drmAdapter.SetConfigurationByteArray("description", description, valueLen);
}

void DrmAdapterImpl__GetConfigurationByteArray(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    char value[256];
    int32_t valueLen = sizeof(value);
    uint8_t valueGetArray[128];

    // Properly test GetConfigurationByteArray
    drmAdapter.GetConfigurationByteArray("", valueGetArray, &valueLen);
    drmAdapter.GetConfigurationByteArray("test", nullptr, &valueLen);
    drmAdapter.GetConfigurationByteArray("test", valueGetArray, nullptr);
    drmAdapter.GetConfigurationByteArray("test", valueGetArray, &valueLen);

    // Test GetConfigurationString
    drmAdapter.GetConfigurationString("", value, valueLen);
    drmAdapter.GetConfigurationString("version", nullptr, valueLen);
    drmAdapter.GetConfigurationString("version", value, valueLen);
    if (g_isSupportDrm) {
        drmAdapter.RegistDrmCallback(mockCallback_);
        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
        drmAdapter.SetConfigurationString("version", "2.0");
        drmAdapter.GetConfigurationString("testVersion", value, valueLen);
        drmAdapter.GetConfigurationString("version", value, valueLen);
        // Test GetConfigurationByteArray with valid keySystem
        drmAdapter.GetConfigurationByteArray("version", valueGetArray, &valueLen);
        drmAdapter.ReleaseMediaKeySession();
        drmAdapter.ReleaseMediaKeySystem();
    }
}

void DrmAdapterImpl__RequireSecureDecoderModule(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    std::string mimeType = fdp->ConsumeRandomLengthString(16);
    bool status = fdp->ConsumeBool();
    drmAdapter.RequireSecureDecoderModule(mimeType, status);
    mimeType = "video/avc";
    status = false;
    drmAdapter.RequireSecureDecoderModule(mimeType, status);
    drmAdapter.RequireSecureDecoderModule("", status);
    if (g_isSupportDrm) {
        drmAdapter.RegistDrmCallback(mockCallback_);
        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
        drmAdapter.RequireSecureDecoderModule(mimeType, status);
        int32_t certStatus = -1;
        drmAdapter.GetCertificateStatus(certStatus);
        drmAdapter.ReleaseMediaKeySession();
        drmAdapter.ReleaseMediaKeySystem();
    }
}

void DrmAdapterImpl__TestFunctionsSeven(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    {
        int32_t level = 0;
        int32_t certStatus = 0;
        drmAdapter.GetMaxContentProtectionLevel(level);
        drmAdapter.GetCertificateStatus(certStatus);
        if (g_isSupportDrm) {
            drmAdapter.RegistDrmCallback(mockCallback_);
            drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
            drmAdapter.GetMaxContentProtectionLevel(level);
            drmAdapter.ReleaseMediaKeySession();
            drmAdapter.ReleaseMediaKeySystem();
        }
    }

    {
        auto callbackAdapter = std::make_shared<DrmCallbackAdapterMock>();
        drmAdapter.RegistDrmCallback(callbackAdapter);
    }

    {
        drmAdapter.ClearMediaKeys();
        drmAdapter.GetSecurityLevel();
    }

    {
        std::string emeId = fdp->ConsumeRandomLengthString(16);
        drmAdapter.CreateMediaKeySession(emeId);
        drmAdapter.GetMediaKeySession(emeId);
    }

    {
        MediaKeySession* drmKeySession = nullptr;
        GetMediaKeySession(&drmKeySession);
        drmAdapter.ReleaseMediaKeySession(drmKeySession);
    }
}

void DrmAdapterImpl__PutSessionInfo(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    auto id = fdp->ConsumeRandomLengthString(32);
    std::shared_ptr<SessionId> sessionId = SessionId::CreateSessionId(id);
    std::string mimeType = fdp->ConsumeRandomLengthString(16);
    int32_t type = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
    drmAdapter.PutSessionInfo(sessionId, mimeType, type);
    drmAdapter.PutSessionInfo(nullptr, mimeType, type);
}

void DrmAdapterImpl__GetSessionInfo(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    auto id = fdp->ConsumeRandomLengthString(32);
    std::shared_ptr<SessionId> sessionId = SessionId::CreateSessionId(id);
    drmAdapter.GetSessionInfo(sessionId);
    drmAdapter.GetSessionInfo(nullptr);

    // Put info and then get it
    std::string mimeType = "video/mp4";
    int32_t type = 1;
    auto validSessionId = std::make_shared<SessionId>("test_eme_id", nullptr, 0);
    drmAdapter.PutSessionInfo(validSessionId, mimeType, type);
    drmAdapter.GetSessionInfo(validSessionId);
}

void DrmAdapterImpl__RemoveSessionInfo(FuzzedDataProvider* fdp)
{
    auto id = fdp->ConsumeRandomLengthString(32);
    std::shared_ptr<SessionId> sessionId = SessionId::CreateSessionId(id);
    DrmAdapterImpl drmAdapter;
    drmAdapter.RemoveSessionInfo(sessionId);
    auto validSessionId = std::make_shared<SessionId>("test_eme_id", nullptr, 0);
    std::string mimeType = "video/mp4";
    int32_t sessionType = 1;
    drmAdapter.PutSessionInfo(nullptr, mimeType, sessionType);
    drmAdapter.PutSessionInfo(validSessionId, mimeType, sessionType);
    drmAdapter.GetSessionInfo(validSessionId);
    drmAdapter.RemoveSessionInfo(nullptr);
    drmAdapter.RemoveSessionInfo(validSessionId);
    drmAdapter.GetSessionInfo(validSessionId);
}

void DrmAdapterImpl__LoadSessionInfo(FuzzedDataProvider* fdp)
{
    std::string emeId = fdp->ConsumeRandomLengthString(16);
    DrmAdapterImpl drmAdapter;
    drmAdapter.LoadSessionInfo(emeId);

    // Test with callback registered
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.LoadSessionInfo(emeId);
}

void DrmAdapterImpl__LoadSessionWithLoadedStorage(FuzzedDataProvider* fdp)
{
    auto id = fdp->ConsumeRandomLengthString(32);
    std::shared_ptr<SessionId> sessionId = SessionId::CreateSessionId(id);
    uint32_t promiseId = fdp->ConsumeIntegral<uint32_t>();
    DrmAdapterImpl drmAdapter;
    drmAdapter.LoadSessionWithLoadedStorage(sessionId, promiseId);
    drmAdapter.LoadSessionWithLoadedStorage(nullptr, 1);
    sessionId = std::make_shared<SessionId>("sessionId_028", nullptr, 0);
    drmAdapter.LoadSessionWithLoadedStorage(sessionId, 1);

    // Cover RELEASE keyType branch
    drmAdapter.RegistDrmCallback(mockCallback_);
    auto releaseSession = std::make_shared<SessionId>("release_session", nullptr, 0);
    drmAdapter.PutSessionInfo(releaseSession, "video/mp4", static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_RELEASE));
    drmAdapter.LoadSessionWithLoadedStorage(releaseSession, 1);

    // Cover non-OFFLINE keyType branch (ONLINE)
    auto onlineSession = std::make_shared<SessionId>("online_session", nullptr, 0);
    drmAdapter.PutSessionInfo(onlineSession, "video/mp4", static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_ONLINE));
    drmAdapter.LoadSessionWithLoadedStorage(onlineSession, 1);

    // Cover WIDEVINE keySystemType branch
    if (g_isSupportDrm) {
        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
        drmAdapter.RegistDrmCallback(mockCallback_);
        auto offlineSession = std::make_shared<SessionId>("offline_session", nullptr, 0);
        drmAdapter.PutSessionInfo(offlineSession, "video/mp4", static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
        drmAdapter.keySystemType_ = KeySystemType::WIDEVINE;
        drmAdapter.LoadSessionWithLoadedStorage(offlineSession, 1);
        drmAdapter.ReleaseMediaKeySession();
        drmAdapter.ReleaseMediaKeySystem();
    }
}

void DrmAdapterImpl__UpdateSessionResult(FuzzedDataProvider* fdp)
{
    bool isKeyRelease = fdp->ConsumeBool();
    auto id = fdp->ConsumeRandomLengthString(32);
    std::shared_ptr<SessionId> sessionId = SessionId::CreateSessionId(id);
    std::vector<uint8_t> mediaKeyId = fdp->ConsumeBytes<uint8_t>(16);
    int32_t mediaKeyIdLen = mediaKeyId.size();
    DrmAdapterImpl drmAdapter;
    drmAdapter.UpdateSessionResult(isKeyRelease, sessionId, mediaKeyId.data(), mediaKeyIdLen);
    drmAdapter.UpdateSessionResult(false, nullptr, nullptr, 0);
    sessionId = SessionId::CreateSessionId("tempSessionId068");
    drmAdapter.UpdateSessionResult(false, sessionId, nullptr, 0);

    std::string mimeType = "video/mp4";
    int32_t sessionType = 1;
    drmAdapter.callback_ = nullptr;
    drmAdapter.PutSessionInfo(sessionId, mimeType, sessionType);
    drmAdapter.UpdateSessionResult(false, sessionId, nullptr, 0);
    drmAdapter.UpdateSessionResult(true, sessionId, nullptr, 0);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.UpdateSessionResult(false, sessionId, nullptr, 0);

    // Cover OFFLINE keyType + mediaKeyIdLen > 0 branch
    auto offlineSession = std::make_shared<SessionId>("offline_update", nullptr, 0);
    drmAdapter.PutSessionInfo(offlineSession, mimeType, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
    std::vector<uint8_t> keyIdData = { 0x01, 0x02, 0x03 };
    drmAdapter.UpdateSessionResult(false, offlineSession, keyIdData.data(), keyIdData.size());
}

void DrmAdapterImpl__SetKeyType(FuzzedDataProvider* fdp)
{
    auto id = fdp->ConsumeRandomLengthString(32);
    std::shared_ptr<SessionId> sessionId = SessionId::CreateSessionId(id);
    int32_t keyType = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
    DrmAdapterImpl drmAdapter;
    drmAdapter.SetKeyType(sessionId, keyType);

    std::vector<uint8_t> keySetId = { 0x01, 0x02, 0x03 };
    sessionId = std::make_shared<SessionId>("sessionId", keySetId.data(), keySetId.size());
    auto sessionInfo = std::make_shared<SessionInfo>(
        sessionId, "mimeType", static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
    drmAdapter.SetKeyType(nullptr, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.PutSessionInfo(sessionId, "mimeType", static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
    drmAdapter.SetKeyType(sessionId, static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
    drmAdapter.RemoveSessionInfo(sessionId);
    drmAdapter.ReleaseMediaKeySession();
    drmAdapter.ReleaseMediaKeySystem();
}

void DrmAdapterImpl__SetKeySetId(FuzzedDataProvider* fdp)
{
    auto id = fdp->ConsumeRandomLengthString(32);
    std::shared_ptr<SessionId> sessionId = SessionId::CreateSessionId(id);
    std::vector<uint8_t> mediaKeyId = fdp->ConsumeBytes<uint8_t>(16);
    int32_t mediaKeyIdLen = mediaKeyId.size();
    DrmAdapterImpl drmAdapter;
    drmAdapter.SetKeySetId(sessionId, mediaKeyId.data(), mediaKeyIdLen);

    // Test with null sessionId -> HandleKeyUpdatedCallback(false)
    drmAdapter.SetKeySetId(nullptr, mediaKeyId.data(), mediaKeyIdLen);

    // Test with callback_ set but no sessionInfo -> HandleKeyUpdatedCallback(false)
    drmAdapter.RegistDrmCallback(mockCallback_);
    auto unknownSession = std::make_shared<SessionId>("unknown_session", nullptr, 0);
    drmAdapter.SetKeySetId(unknownSession, mediaKeyId.data(), mediaKeyIdLen);

    // Test with callback_ set and valid sessionInfo -> OnStorageSaveInfo
    auto validSession = std::make_shared<SessionId>("valid_session", nullptr, 0);
    drmAdapter.PutSessionInfo(validSession, "video/mp4", static_cast<int32_t>(MediaKeyType::MEDIA_KEY_TYPE_OFFLINE));
    drmAdapter.SetKeySetId(validSession, mediaKeyId.data(), mediaKeyIdLen);
}

void DrmAdapterImpl__GetSessionIdByEmeId(FuzzedDataProvider* fdp)
{
    std::string emeId = fdp->ConsumeRandomLengthString(16);
    DrmAdapterImpl drmAdapter;
    drmAdapter.GetSessionIdByEmeId(emeId);
    auto validSessionId = std::make_shared<SessionId>("test_eme_id", nullptr, 0);
    std::string mimeType = "video/mp4";
    int32_t sessionType = 1;
    drmAdapter.PutSessionInfo(validSessionId, mimeType, sessionType);
    drmAdapter.GetSessionIdByEmeId(validSessionId->EmeId());
    drmAdapter.GetSessionIdByEmeId("invalid_eme_id");
    drmAdapter.emeSessionInfoMap_["nullptr_eme_id"] = nullptr;
    drmAdapter.GetSessionIdByEmeId("nullptr_eme_id");

}

void DrmAdapterImpl__ClearPersistentSessionInfoFroKeyRelease(FuzzedDataProvider* fdp)
{
    auto id = fdp->ConsumeRandomLengthString(32);
    std::shared_ptr<SessionId> sessionId = SessionId::CreateSessionId(id);
    DrmAdapterImpl drmAdapter;
    drmAdapter.ClearPersistentSessionInfoFroKeyRelease(sessionId);
    drmAdapter.ClearPersistentSessionInfoFroKeyRelease(nullptr);

    // Test with callback registered to cover OnStorageClearInfoForKeyRelease
    drmAdapter.RegistDrmCallback(mockCallback_);
    auto validSession = std::make_shared<SessionId>("clear_release", nullptr, 0);
    drmAdapter.ClearPersistentSessionInfoFroKeyRelease(validSession);
}

void DrmAdapterImpl__ClearPersistentSessionInfoForLoadFail(FuzzedDataProvider* fdp)
{
    auto id = fdp->ConsumeRandomLengthString(32);
    std::shared_ptr<SessionId> sessionId = SessionId::CreateSessionId(id);
    DrmAdapterImpl drmAdapter;
    drmAdapter.ClearPersistentSessionInfoForLoadFail(sessionId);
    drmAdapter.ClearPersistentSessionInfoForLoadFail(nullptr);

    // Test with callback registered to cover OnStorageClearInfoForLoadFail
    drmAdapter.RegistDrmCallback(mockCallback_);
    auto validSession = std::make_shared<SessionId>("clear_loadfail", nullptr, 0);
    drmAdapter.ClearPersistentSessionInfoForLoadFail(validSession);
}

void DrmAdapterImpl__HandleKeyUpdatedCallback(FuzzedDataProvider* fdp)
{
    uint32_t promiseId = fdp->ConsumeIntegral<uint32_t>();
    bool result = fdp->ConsumeBool();
    DrmAdapterImpl drmAdapter;
    drmAdapter.HandleKeyUpdatedCallback(promiseId, result);

    // Test with callback registered and result = false (OnPromiseRejected)
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.HandleKeyUpdatedCallback(promiseId, false);
    // Test with callback registered and result = true (OnPromiseResolved)
    drmAdapter.HandleKeyUpdatedCallback(promiseId, true);
}

void DrmAdapterImpl__OnSessionExpirationUpdate(FuzzedDataProvider* fdp)
{
    // Test with nullptr session
    {
        MediaKeySession* drmKeySession = nullptr;
        std::vector<uint8_t> info = fdp->ConsumeBytes<uint8_t>(32);
        int32_t infoLen = info.size();
        DrmAdapterImpl::OnSessionExpirationUpdate(drmKeySession, info.data(), infoLen);
    }

    // Test with session in callback map to cover EndsWithAndRemove / IsValidNumber branches
    {
        auto callback = std::make_shared<DrmCallbackImpl>(mockCallback_);
        MediaKeySession* fakeSession = reinterpret_cast<MediaKeySession*>(0x1234);
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_[fakeSession] = callback;
        }
        auto sessionId = SessionId::CreateSessionId("exp_session");
        auto sessionInfo = std::make_shared<SessionInfo>(sessionId, "video/mp4", 0);
        callback->mediaKeySessionInfoMap_[fakeSession] = sessionInfo;

        // EndsWithAndRemove returns true, IsValidNumber returns true, size > 3
        std::string infoStr1 = "1234567ms";
        DrmAdapterImpl::OnSessionExpirationUpdate(fakeSession,
            reinterpret_cast<uint8_t*>(const_cast<char*>(infoStr1.data())), infoStr1.size());

        // EndsWithAndRemove returns true, IsValidNumber returns true, size <= 3
        std::string infoStr2 = "12ms";
        DrmAdapterImpl::OnSessionExpirationUpdate(fakeSession,
            reinterpret_cast<uint8_t*>(const_cast<char*>(infoStr2.data())), infoStr2.size());

        // EndsWithAndRemove returns true, IsValidNumber returns false
        std::string infoStr3 = "abcms";
        DrmAdapterImpl::OnSessionExpirationUpdate(fakeSession,
            reinterpret_cast<uint8_t*>(const_cast<char*>(infoStr3.data())), infoStr3.size());

        // EndsWithAndRemove returns false
        std::string infoStr4 = "1234";
        DrmAdapterImpl::OnSessionExpirationUpdate(fakeSession,
            reinterpret_cast<uint8_t*>(const_cast<char*>(infoStr4.data())), infoStr4.size());

        // infoLen == 0
        DrmAdapterImpl::OnSessionExpirationUpdate(fakeSession, nullptr, 0);

        // infoLen > EXPIRATION_INFO_MAX_LEN (should skip parsing)
        std::string infoStr5(32, '1');
        DrmAdapterImpl::OnSessionExpirationUpdate(fakeSession,
            reinterpret_cast<uint8_t*>(const_cast<char*>(infoStr5.data())), infoStr5.size());

        // Test callback nullptr case
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_[fakeSession] = nullptr;
        }
        DrmAdapterImpl::OnSessionExpirationUpdate(fakeSession,
            reinterpret_cast<uint8_t*>(const_cast<char*>(infoStr1.data())), infoStr1.size());

        // Clean up
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_.erase(fakeSession);
        }
    }

    // Test session not found in map
    {
        MediaKeySession* fakeSession2 = reinterpret_cast<MediaKeySession*>(0x5678);
        std::string infoStr = "1234ms";
        DrmAdapterImpl::OnSessionExpirationUpdate(fakeSession2,
            reinterpret_cast<uint8_t*>(const_cast<char*>(infoStr.data())), infoStr.size());
    }
}

void DrmAdapterImpl__GetKeyRequest(FuzzedDataProvider* fdp)
{
    // Test with nullptr session
    {
        MediaKeySession* drmKeySession = nullptr;
        std::vector<uint8_t> info = fdp->ConsumeBytes<uint8_t>(32);
        int32_t infoLen = info.size();
        DrmAdapterImpl::GetKeyRequest(drmKeySession, info.data(), infoLen);
    }

    // Test session not found in map
    {
        MediaKeySession* fakeSession = reinterpret_cast<MediaKeySession*>(0x9999);
        std::vector<uint8_t> info = fdp->ConsumeBytes<uint8_t>(32);
        DrmAdapterImpl::GetKeyRequest(fakeSession, info.data(), info.size());
    }

    // Test with session in map but no sessionInfo
    {
        auto callback = std::make_shared<DrmCallbackImpl>(mockCallback_);
        MediaKeySession* fakeSession = reinterpret_cast<MediaKeySession*>(0xAAAA);
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_[fakeSession] = callback;
        }
        std::vector<uint8_t> info = fdp->ConsumeBytes<uint8_t>(32);
        DrmAdapterImpl::GetKeyRequest(fakeSession, info.data(), info.size());

        // Test with sessionInfo but long mimeType to trigger memcpy_s failure
        auto sessionId = SessionId::CreateSessionId("keyreq_session");
        std::string longMimeType(256, 'x');
        auto sessionInfo = std::make_shared<SessionInfo>(sessionId, longMimeType, 0);
        callback->mediaKeySessionInfoMap_[fakeSession] = sessionInfo;
        DrmAdapterImpl::GetKeyRequest(fakeSession, info.data(), info.size());

        // Test with valid mimeType but long initData to trigger memcpy_s failure
        std::string validMime = "video/avc";
        auto sessionInfo2 = std::make_shared<SessionInfo>(sessionId, validMime, 0);
        callback->mediaKeySessionInfoMap_[fakeSession] = sessionInfo2;
        std::vector<uint8_t> longInitData(1024 * 1024, 0x01);
        DrmAdapterImpl::GetKeyRequest(fakeSession, longInitData.data(), longInitData.size());

        // Clean up
        {
            std::lock_guard<std::mutex> lock(DrmAdapterImpl::mediaKeySessionCallbackMutex_);
            DrmAdapterImpl::mediaKeySessionCallbackMap_.erase(fakeSession);
        }
    }
}

void DrmAdapterImpl__TestFunctionsEight(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    auto g_callback = std::make_shared<DrmCallbackImpl>(mockCallback_);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.CreateKeySystem("", "", SECURITY_LEVEL_3);
    drmAdapter.CreateKeySystem("com.test.drm", "", SECURITY_LEVEL_3);
    drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
    if (g_isSupportDrm) {
        int32_t certStatus = -1;
        drmAdapter.GetCertificateStatus(certStatus);
        if (certStatus == 0) {
            drmAdapter.ReleaseMediaKeySession();
            drmAdapter.ReleaseMediaKeySystem();

            drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_1);
            drmAdapter.ReleaseMediaKeySession();
            drmAdapter.ReleaseMediaKeySystem();
        }

        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_UNKNOWN);
        drmAdapter.ReleaseMediaKeySession();
        drmAdapter.ReleaseMediaKeySystem();

        drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", -1);
        drmAdapter.ReleaseMediaKeySession();
        drmAdapter.ReleaseMediaKeySystem();
    }
}

void DrmAdapterImpl__CreateMediaKeySession(FuzzedDataProvider* fdp)
{
    DrmAdapterImpl drmAdapter;
    auto g_callback = std::make_shared<DrmCallbackImpl>(mockCallback_);
    drmAdapter.RegistDrmCallback(mockCallback_);
    drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
    if (g_isSupportDrm) {
        int32_t certStatus = -1;
        drmAdapter.GetCertificateStatus(certStatus);
        if (certStatus == 0) {
            drmAdapter.CreateMediaKeySession();
            drmAdapter.ReleaseMediaKeySession();

            drmAdapter.CreateMediaKeySession("emeId_006");
            drmAdapter.ReleaseMediaKeySession(drmAdapter.GetMediaKeySession("emeId_006"));

            drmAdapter.keySystemType_ = KeySystemType::WIDEVINE;
            drmAdapter.GetMediaKeySession("");

            drmAdapter.CreateMediaKeySession("emeId_006");
            drmAdapter.ReleaseMediaKeySession(drmAdapter.GetMediaKeySession("emeId_006"));
        }
    }
}

void DrmAdapterImpl__Destructor(FuzzedDataProvider* fdp)
{
    // Test destructor with callback only
    {
        DrmAdapterImpl drmAdapter;
        drmAdapter.RegistDrmCallback(mockCallback_);
    }

    // Test destructor with keySystem and keySession (WISEPLAY path)
    if (g_isSupportDrm) {
        {
            DrmAdapterImpl drmAdapter;
            drmAdapter.RegistDrmCallback(mockCallback_);
            drmAdapter.CreateKeySystem(GetKeySystemName(), "origin_id", SECURITY_LEVEL_3);
        }
    }

    // Test destructor with WIDEVINE keySystemType and sessions in map
    if (g_isSupportDrm) {
        {
            DrmAdapterImpl drmAdapter;
            drmAdapter.RegistDrmCallback(mockCallback_);
            drmAdapter.CreateKeySystem(WIDEVINE_NAME, "origin_id", SECURITY_LEVEL_3);
            drmAdapter.CreateMediaKeySession("eme1");
            drmAdapter.CreateMediaKeySession("eme2");
        }
    }
}

void DrmAdapterImpl__GetContentProtectionLevel(FuzzedDataProvider* fdp)
{
    // Directly cover all branches of GetSecurityLevelFromContentProtectionLevel
    int32_t level1 = GetSecurityLevelFromContentProtectionLevel(CONTENT_PROTECTION_LEVEL_UNKNOWN);
    int32_t level2 = GetSecurityLevelFromContentProtectionLevel(CONTENT_PROTECTION_LEVEL_SW_CRYPTO);
    int32_t level3 = GetSecurityLevelFromContentProtectionLevel(CONTENT_PROTECTION_LEVEL_HW_CRYPTO);
    int32_t level4 = GetSecurityLevelFromContentProtectionLevel(CONTENT_PROTECTION_LEVEL_ENHANCED_HW_CRYPTO);
    int32_t level5 = GetSecurityLevelFromContentProtectionLevel(CONTENT_PROTECTION_LEVEL_MAX);
    int32_t level6 = GetSecurityLevelFromContentProtectionLevel(999);

    // Directly cover all branches of GetContentProtectionLevelFromSecurityLevel
    DRM_ContentProtectionLevel cp1 = GetContentProtectionLevelFromSecurityLevel(SECURITY_LEVEL_UNKNOWN);
    DRM_ContentProtectionLevel cp2 = GetContentProtectionLevelFromSecurityLevel(SECURITY_LEVEL_1);
    DRM_ContentProtectionLevel cp3 = GetContentProtectionLevelFromSecurityLevel(SECURITY_LEVEL_3);
    DRM_ContentProtectionLevel cp4 = GetContentProtectionLevelFromSecurityLevel(999);

    (void)level1; (void)level2; (void)level3; (void)level4; (void)level5; (void)level6;
    (void)cp1; (void)cp2; (void)cp3; (void)cp4;
}

} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);
    OHOS::DrmAdapterImpl__CreateSessionId(&fdp);
    OHOS::DrmAdapterImpl__SessionIdMethods(&fdp);
    OHOS::DrmAdapterImpl__SessionInfoMethods(&fdp);
    OHOS::DrmCallbackImpl__TestFunctionsOne(&fdp);
    OHOS::DrmCallbackImpl__TestFunctionsTwo(&fdp);
    OHOS::DrmCallbackImpl__OnSessionKeysChange(&fdp);
    OHOS::DrmCallbackImpl__OnStorageSaveInfo(&fdp);
    OHOS::DrmCallbackImpl__TestFunctionsThree(&fdp);
    OHOS::DrmCallbackImpl__UpdateMediaKeySessionInfoMap(&fdp);
    OHOS::DrmCallbackImpl__TestFunctionsFour(&fdp);
    OHOS::DrmAdapterImpl__SystemCallBackWithObj(&fdp);
    OHOS::DrmAdapterImpl__SessionEventCallBackWithObj(&fdp);
    OHOS::DrmAdapterImpl__SessionKeyChangeCallBackWithObj(&fdp);
    OHOS::DrmAdapterImpl__IsSupported(&fdp);
    OHOS::DrmAdapterImpl__StorageProvisionedResult(&fdp);
    OHOS::DrmAdapterImpl__StorageSaveInfoResult(&fdp);
    OHOS::DrmAdapterImpl__StorageClearInfoResult(&fdp);
    OHOS::DrmAdapterImpl__ProcessKeySystemResponse(&fdp);
    OHOS::DrmAdapterImpl__StorageLoadInfoResult(&fdp);
    OHOS::DrmAdapterImpl__CreateKeySystem(&fdp);
    OHOS::DrmAdapterImpl__GenerateMediaKeyRequest(&fdp);
    OHOS::DrmAdapterImpl__UpdateSession(&fdp);
    OHOS::DrmAdapterImpl__CloseSession(&fdp);
    OHOS::DrmAdapterImpl__RemoveSession(&fdp);
    OHOS::DrmAdapterImpl__LoadSession(&fdp);
    OHOS::DrmAdapterImpl__SetConfigurationString(&fdp);
    OHOS::DrmAdapterImpl__GetConfigurationByteArray(&fdp);
    OHOS::DrmAdapterImpl__RequireSecureDecoderModule(&fdp);
    OHOS::DrmAdapterImpl__TestFunctionsSeven(&fdp);
    OHOS::DrmAdapterImpl__PutSessionInfo(&fdp);
    OHOS::DrmAdapterImpl__GetSessionInfo(&fdp);
    OHOS::DrmAdapterImpl__RemoveSessionInfo(&fdp);
    OHOS::DrmAdapterImpl__LoadSessionInfo(&fdp);
    OHOS::DrmAdapterImpl__LoadSessionWithLoadedStorage(&fdp);
    OHOS::DrmAdapterImpl__UpdateSessionResult(&fdp);
    OHOS::DrmAdapterImpl__SetKeyType(&fdp);
    OHOS::DrmAdapterImpl__SetKeySetId(&fdp);
    OHOS::DrmAdapterImpl__GetSessionIdByEmeId(&fdp);
    OHOS::DrmAdapterImpl__ClearPersistentSessionInfoFroKeyRelease(&fdp);
    OHOS::DrmAdapterImpl__ClearPersistentSessionInfoForLoadFail(&fdp);
    OHOS::DrmAdapterImpl__HandleKeyUpdatedCallback(&fdp);
    OHOS::DrmAdapterImpl__OnSessionExpirationUpdate(&fdp);
    OHOS::DrmAdapterImpl__GetKeyRequest(&fdp);
    OHOS::DrmAdapterImpl__TestFunctionsEight(&fdp);
    OHOS::DrmAdapterImpl__CreateMediaKeySession(&fdp);
    OHOS::DrmAdapterImpl__Destructor(&fdp);
    OHOS::DrmAdapterImpl__GetContentProtectionLevel(&fdp);

    return 0;
}
