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
#include <fuzzer/FuzzedDataProvider.h>

#define private public
#include "arkweb/ohos_adapter_ndk/distributeddatamgr_adapter/ohos_web_snapshot_data_base.h"
#undef private

using namespace OHOS::NWeb;

namespace {
constexpr size_t MAX_STRING_LENGTH = 255;
constexpr float MAX_SIMILARITY = 1.0f;
constexpr int64_t MAX_KEY = 100;
constexpr size_t MAX_KEY_LENGTH = 100;
constexpr int32_t MAX_LCP_TIME = 5000;
constexpr int32_t MIN_CAPACITY = 0;
constexpr int32_t MAX_CAPACITY = 200;
constexpr int32_t MAX_FILE_SIZE = 30 * 1024 * 1024;
constexpr int64_t MICRO_SECONDS_PER_DAY = 1000LL * 1000 * 3600 * 24;
constexpr size_t kMaxIterations = 10;

class OhosWebSnapshotDataBaseCallbackFuzz : public OhosWebSnapshotDataBaseCallback {
public:
    OhosWebSnapshotDataBaseCallbackFuzz() = default;

    ~OhosWebSnapshotDataBaseCallbackFuzz() override = default;

    void OnDataDelete(const std::string& path) override {}

    void OnDataExist(const std::unordered_set<std::string>& existPaths) override {}

    void OnDataClear() override {}
};

void HookDataBase()
{
    OhosWebSnapshotDataBase& dataBase = OhosWebSnapshotDataBase::GetInstance();
    OH_Rdb_Config config = {0};
    config.selfSize = sizeof(OH_Rdb_Config);
    config.dataBaseDir = "/data/storage/el2/base/cache/web";
    config.bundleName = "com.example.myapplication";
    config.storeName = "web_snapshot.db";
    config.area = Rdb_SecurityArea::RDB_SECURITY_AREA_EL2;
    config.securityLevel = OH_Rdb_SecurityLevel::S3;
    dataBase.GetOrOpen(config);
    dataBase.GetAllInfo();
}

SnapshotDataItem ConsumeSnapshotDataItem(FuzzedDataProvider& dataProvider)
{
    SnapshotDataItem dataItem = {
        dataProvider.ConsumeRandomLengthString(MAX_STRING_LENGTH),
        dataProvider.ConsumeRandomLengthString(MAX_STRING_LENGTH),
        dataProvider.ConsumeFloatingPointInRange<float>(0, MAX_SIMILARITY),
        dataProvider.ConsumeIntegralInRange<int32_t>(0, MAX_LCP_TIME),
        dataProvider.ConsumeIntegralInRange<int64_t>(0, MAX_FILE_SIZE),
        dataProvider.ConsumeIntegral<int64_t>(),
        dataProvider.ConsumeIntegral<int64_t>()
    };
    return dataItem;
}

void OhosWebSnapshotDataBaseFuzzTest(const uint8_t* data, size_t size)
{
    FuzzedDataProvider dataProvider(data, size);
    HookDataBase();
    OhosWebSnapshotDataBase& dataBase = OhosWebSnapshotDataBase::GetInstance();
    std::shared_ptr<OhosWebSnapshotDataBaseCallback> callback = std::make_shared<OhosWebSnapshotDataBaseCallbackFuzz>();
    if (dataProvider.ConsumeBool()) {
        dataBase.rdbStore_ = nullptr;
    }
    dataBase.RegisterDataBaseCallback(callback);
    SnapshotDataItem dataItem = ConsumeSnapshotDataItem(dataProvider);
    if (!dataProvider.ConsumeBool()) {
        dataItem.snapShotFileSize = 1;
    }
    dataBase.InsertSnapshotDataItem(dataProvider.ConsumeIntegralInRange<int64_t>(0, MAX_KEY), dataItem);
    dataBase.InsertSnapshotDataItem(dataProvider.ConsumeIntegralInRange<int64_t>(0, MAX_KEY), dataItem,
        dataProvider.ConsumeIntegralInRange<int64_t>(0, dataBase.GetCurrentTime() + 1 * MICRO_SECONDS_PER_DAY));
    if (!dataProvider.ConsumeBool()) {
        dataBase.InsertSnapshotDataItem(1, dataItem);
        dataBase.InsertSnapshotDataItem(2, dataItem,
            dataProvider.ConsumeIntegralInRange<int64_t>(0, dataBase.GetCurrentTime() + 1 * MICRO_SECONDS_PER_DAY));
    }
    dataBase.GetSnapshotDataItem(dataProvider.ConsumeIntegralInRange<int64_t>(0, MAX_KEY));
    int32_t capacity = MAX_CAPACITY;
    if (dataProvider.ConsumeBool()) {
        capacity = dataProvider.ConsumeIntegralInRange<int32_t>(MIN_CAPACITY, MAX_CAPACITY);
    }
    dataBase.SetBlanklessLoadingCacheCapacity(capacity);
    dataBase.UpdateCapacityData(capacity);
    dataBase.GetCapacityInByte();
    std::vector<int64_t> keys = {};
    for (size_t idx = 0; idx < dataProvider.ConsumeIntegralInRange<size_t>(0, MAX_KEY_LENGTH); idx++) {
        keys.push_back(dataProvider.ConsumeIntegralInRange<int64_t>(0, MAX_KEY));
    }
    if (!dataProvider.ConsumeBool()) {
        keys.push_back(1);
    }
    dataBase.ClearSnapshotDataItem(keys);
    dataBase.ClearSnapshotDataItem({});
    dataBase.GetDatabaseInfo();
    dataBase.InsertInner(dataProvider.ConsumeIntegralInRange<int64_t>(0, MAX_KEY),
        {dataItem, dataBase.GetCurrentTime() + 7 * MICRO_SECONDS_PER_DAY});
    dataBase.DeleteInner(dataBase.GetOldestKey());
    dataBase.ClearInner();
}

void OhosWebSnapshotDataBaseFuzzTestExtra(const uint8_t* data, size_t size)
{
    FuzzedDataProvider dataProvider(data, size);
    HookDataBase();
    OhosWebSnapshotDataBase& dataBase = OhosWebSnapshotDataBase::GetInstance();
    std::shared_ptr<OhosWebSnapshotDataBaseCallback> callback = std::make_shared<OhosWebSnapshotDataBaseCallbackFuzz>();
    dataBase.RegisterDataBaseCallback(callback);
    dataBase.RegisterDataBaseCallback(nullptr);
    dataBase.SetBlanklessLoadingCacheCapacity(-1);
    dataBase.SetBlanklessLoadingCacheCapacity(300);
    SnapshotDataItem zeroSizeItem = {"/data/a.png", "/data/b.png", 0.5, 100, 0, 0, 0};
    dataBase.InsertSnapshotDataItem(1, zeroSizeItem);
    SnapshotDataItem overSizeItem = {"/data/c.png", "/data/d.png", 0.8, 200, MAX_FILE_SIZE * 2, 0, 0};
    dataBase.InsertSnapshotDataItem(2, overSizeItem);
    dataBase.SetBlanklessLoadingCacheCapacity(50);
    SnapshotDataItem validItem = {"/data/e.png", "/data/f.png", 0.6, 150, 100, 0, 0};
    int64_t key1 = 10;
    dataBase.InsertSnapshotDataItem(key1, validItem);
    dataBase.InsertSnapshotDataItem(key1, validItem);
    dataBase.GetSnapshotDataItem(key1);
    dataBase.GetSnapshotDataItem(9999);
    dataBase.InsertSnapshotDataItem(20, validItem, dataBase.GetCurrentTime() - 1000);
    dataBase.GetSnapshotDataItem(20);
    dataBase.SetBlanklessLoadingCacheCapacity(0);
    dataBase.RegisterDataBaseCallback(callback);
    dataBase.SetBlanklessLoadingCacheCapacity(50);
    dataBase.InsertSnapshotDataItem(key1, validItem);
    SnapshotDataItem pathItem = {"/path/whole.png", "/path/static.png", 0.7, 300, 50, 0, 0};
    int64_t keyPath = 30;
    dataBase.InsertSnapshotDataItem(keyPath, pathItem);
    dataBase.SetBlanklessLoadingCacheCapacity(1);
    std::vector<int64_t> delKeys = {keyPath};
    dataBase.ClearSnapshotDataItem(delKeys);
    dataBase.ClearSnapshotDataItem({});
    dataBase.SetBlanklessLoadingCacheCapacity(50);
    dataBase.InsertSnapshotDataItem(key1, validItem);
    dataBase.InsertSnapshotDataItem(keyPath, pathItem);
    dataBase.GetDatabaseInfo();
    dataBase.InsertInner(40, {validItem, dataBase.GetCurrentTime() + MICRO_SECONDS_PER_DAY});
    dataBase.DeleteInner(dataBase.GetOldestKey());
    dataBase.ClearInner();
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if ((data == nullptr) || (size == 0)) {
        return 0;
    }
    FuzzedDataProvider fdp(data, size);
    size_t iterations = std::min(fdp.ConsumeIntegralInRange<size_t>(1, ::kMaxIterations), size);
    
    for (size_t i = 0; i < iterations; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 2);
        switch (scenario) {
            case 0:
                OhosWebSnapshotDataBaseFuzzTest(data, size);
                break;
            case 1:
                OhosWebSnapshotDataBaseFuzzTestExtra(data, size);
                break;
            default:
                break;
        }
    }
    return 0;
}