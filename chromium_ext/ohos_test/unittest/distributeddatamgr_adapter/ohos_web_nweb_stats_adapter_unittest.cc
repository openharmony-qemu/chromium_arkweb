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

#include <AbilityKit/ability_runtime/ability_runtime_common.h>
#include <cinttypes>
#include <cstring>
#include <ctime>
#include <securec.h>
#include <string>
#include <gtest/gtest.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"

#define private public
#include "arkweb/ohos_adapter_ndk/distributeddatamgr_adapter/ohos_web_nweb_stats_adapter_impl.h"
#undef private

#include <database/rdb/relational_store.h>
#include <database/rdb/relational_store_error_code.h>

namespace {
enum class AbilityRuntimeStage {
    kOk,
    kCacheDirError,
    kMissingWebDir,
    kBundleNameError,
    kAreaModeError,
};

AbilityRuntimeStage g_stage = AbilityRuntimeStage::kOk;
std::string g_cache_root;

bool CopyString(const std::string& value, char* buffer, int32_t buffer_size, int32_t* write_length)
{
    if (buffer == nullptr || write_length == nullptr || buffer_size <= static_cast<int32_t>(value.size())) {
        return false;
    }
    errno_t ret = memcpy_s(buffer, static_cast<size_t>(buffer_size), value.c_str(), value.size() + 1);
    if (ret != EOK) {
        return false;
    }
    *write_length = static_cast<int32_t>(value.size());
    return true;
}
} // namespace

// The production code calls these AbilityRuntime NDK entry points directly. The
// executable defines them here so the unit test can drive every InitDatabase
// branch without touching production code or build flags.
extern "C" AbilityRuntime_ErrorCode OH_AbilityRuntime_ApplicationContextGetCacheDir(
    char* buffer, int32_t bufferSize, int32_t* writeLength)
{
    if (g_stage == AbilityRuntimeStage::kCacheDirError) {
        return ABILITY_RUNTIME_ERROR_CODE_CONTEXT_NOT_EXIST;
    }
    if (!CopyString(g_cache_root, buffer, bufferSize, writeLength)) {
        return ABILITY_RUNTIME_ERROR_CODE_PARAM_INVALID;
    }
    return ABILITY_RUNTIME_ERROR_CODE_NO_ERROR;
}

extern "C" AbilityRuntime_ErrorCode OH_AbilityRuntime_ApplicationContextGetBundleName(
    char* buffer, int32_t bufferSize, int32_t* writeLength)
{
    if (g_stage == AbilityRuntimeStage::kBundleNameError) {
        return ABILITY_RUNTIME_ERROR_CODE_CONTEXT_NOT_EXIST;
    }
    if (!CopyString("com.example.arkweb.ut", buffer, bufferSize, writeLength)) {
        return ABILITY_RUNTIME_ERROR_CODE_PARAM_INVALID;
    }
    return ABILITY_RUNTIME_ERROR_CODE_NO_ERROR;
}

extern "C" AbilityRuntime_ErrorCode OH_AbilityRuntime_ApplicationContextGetAreaMode(
    AbilityRuntime_AreaMode* areaMode)
{
    if (g_stage == AbilityRuntimeStage::kAreaModeError) {
        return ABILITY_RUNTIME_ERROR_CODE_CONTEXT_NOT_EXIST;
    }
    if (areaMode == nullptr) {
        return ABILITY_RUNTIME_ERROR_CODE_PARAM_INVALID;
    }
    *areaMode = AbilityRuntime_AreaMode::ABILITY_RUNTIME_AREA_MODE_EL2;
    return ABILITY_RUNTIME_ERROR_CODE_NO_ERROR;
}

namespace OHOS::NWeb {
namespace {
static const int32_t RDB_VERSION = 1;
static const std::string STATS_TABLE_NAME = "nweb_stats";
static const std::string KEY_COL = "key";
static const std::string VALUE_COL = "value";
static const std::string MAX_INSTANCE_COUNT_KEY = "max_instance_count";
static const std::string MAX_INSTANCE_COUNT_TIMESTAMP_KEY = "max_instance_count_timestamp";

static const std::string CREATE_TABLE = "CREATE TABLE IF NOT EXISTS " + STATS_TABLE_NAME
    + " (" + KEY_COL + " TEXT PRIMARY KEY, "
    + VALUE_COL + " INTEGER);";

static const std::string DELETE_TABLE = "DELETE FROM " + STATS_TABLE_NAME + ";";

OH_Rdb_Store* g_TmpRdbStore = nullptr;

void CreateTableIfNeed(OH_Rdb_Store* store)
{
    if (store == nullptr) {
        return;
    }
    int version = 0;
    if (OH_Rdb_GetVersion(store, &version) != RDB_OK) {
        return;
    }
    if (version == 0) {
        OH_Rdb_Execute(store, CREATE_TABLE.c_str());
        OH_Rdb_SetVersion(store, RDB_VERSION);
    }
}

void CleanTable(OH_Rdb_Store* store)
{
    if (store == nullptr) {
        return;
    }
    OH_Rdb_Execute(store, DELETE_TABLE.c_str());
}
} // namespace

class NWebStatsAdapterTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase() {}
    void SetUp() override;
    void TearDown() override {}
};

void NWebStatsAdapterTest::SetUpTestCase()
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    if (stats.rdbStore_ != nullptr) {
        return;
    }
    OH_Rdb_Config config = {0};
    config.selfSize = sizeof(OH_Rdb_Config);
    config.dataBaseDir = "/data/storage/el2/base/cache/web";
    config.bundleName = "com.example.myapplication";
    config.storeName = "nweb_stats.db";
    config.area = Rdb_SecurityArea::RDB_SECURITY_AREA_EL2;
    config.securityLevel = OH_Rdb_SecurityLevel::S1;
    int errCode = RDB_OK;
    stats.rdbStore_ = OH_Rdb_GetOrOpen(&config, &errCode);
    if (stats.rdbStore_ != nullptr) {
        CreateTableIfNeed(stats.rdbStore_);
        stats.dbInitialized_ = true;
    }
}

void NWebStatsAdapterTest::SetUp()
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    ASSERT_NE(stats.rdbStore_, nullptr);
    CleanTable(stats.rdbStore_);
}

TEST_F(NWebStatsAdapterTest, DbNotInitTest)
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    std::swap(stats.rdbStore_, g_TmpRdbStore);
    bool savedInit = stats.dbInitialized_;
    stats.dbInitialized_ = false;

    EXPECT_EQ(stats.GetMaxInstanceCount(), 0);
    stats.UpdateMaxInstanceCount(100);

    stats.dbInitialized_ = savedInit;
    std::swap(stats.rdbStore_, g_TmpRdbStore);
    EXPECT_EQ(stats.QueryInt64Value(MAX_INSTANCE_COUNT_KEY), 0);
}

TEST_F(NWebStatsAdapterTest, StoreAndQueryTest)
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    EXPECT_EQ(stats.QueryInt64Value("nonexistent_key"), 0);
    stats.StoreInt64Value("test_key", 42);
    EXPECT_EQ(stats.QueryInt64Value("test_key"), 42);
    // Overwrite existing key
    stats.StoreInt64Value("test_key", 99);
    EXPECT_EQ(stats.QueryInt64Value("test_key"), 99);
}

TEST_F(NWebStatsAdapterTest, UpgradeCompatTest)
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    // Has count but no timestamp (old version upgrade)
    stats.StoreInt64Value(MAX_INSTANCE_COUNT_KEY, 50);
    EXPECT_EQ(stats.QueryInt64Value(MAX_INSTANCE_COUNT_TIMESTAMP_KEY), 0);
    EXPECT_EQ(stats.GetMaxInstanceCount(), 50);
    EXPECT_NE(stats.QueryInt64Value(MAX_INSTANCE_COUNT_TIMESTAMP_KEY), 0);
}

TEST_F(NWebStatsAdapterTest, UpgradeCompatZeroCountTest)
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    // No count, no timestamp (fresh DB)
    EXPECT_EQ(stats.QueryInt64Value(MAX_INSTANCE_COUNT_KEY), 0);
    EXPECT_EQ(stats.QueryInt64Value(MAX_INSTANCE_COUNT_TIMESTAMP_KEY), 0);
    EXPECT_EQ(stats.GetMaxInstanceCount(), 0);
    EXPECT_NE(stats.QueryInt64Value(MAX_INSTANCE_COUNT_TIMESTAMP_KEY), 0);
}

TEST_F(NWebStatsAdapterTest, ExpiryTest)
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    // Not expired: timestamp 1 day ago
    stats.StoreInt64Value(MAX_INSTANCE_COUNT_KEY, 100);
    stats.StoreInt64Value(MAX_INSTANCE_COUNT_TIMESTAMP_KEY,
                          static_cast<int64_t>(time(nullptr)) - 86400);
    EXPECT_EQ(stats.GetMaxInstanceCount(), 100);

    // Expired: timestamp 8 days ago
    CleanTable(stats.rdbStore_);
    stats.StoreInt64Value(MAX_INSTANCE_COUNT_KEY, 100);
    stats.StoreInt64Value(MAX_INSTANCE_COUNT_TIMESTAMP_KEY,
                          static_cast<int64_t>(time(nullptr)) - 691200);
    EXPECT_EQ(stats.GetMaxInstanceCount(), 0);

    // Invalid negative count
    CleanTable(stats.rdbStore_);
    stats.StoreInt64Value(MAX_INSTANCE_COUNT_KEY, -1);
    stats.StoreInt64Value(MAX_INSTANCE_COUNT_TIMESTAMP_KEY,
                          static_cast<int64_t>(time(nullptr)));
    EXPECT_EQ(stats.GetMaxInstanceCount(), 0);
}

TEST_F(NWebStatsAdapterTest, UpdateNormalTest)
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    int64_t beforeUpdate = static_cast<int64_t>(time(nullptr));
    stats.UpdateMaxInstanceCount(200);

    EXPECT_EQ(stats.QueryInt64Value(MAX_INSTANCE_COUNT_KEY), 200);
    int64_t timestamp = stats.QueryInt64Value(MAX_INSTANCE_COUNT_TIMESTAMP_KEY);
    EXPECT_GE(timestamp, beforeUpdate);
    EXPECT_LE(timestamp, static_cast<int64_t>(time(nullptr)));
}

TEST_F(NWebStatsAdapterTest, AreaModeMapTest)
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    EXPECT_EQ(stats.GetAreaMode(AbilityRuntime_AreaMode::ABILITY_RUNTIME_AREA_MODE_EL1),
              Rdb_SecurityArea::RDB_SECURITY_AREA_EL1);
    EXPECT_EQ(stats.GetAreaMode(AbilityRuntime_AreaMode::ABILITY_RUNTIME_AREA_MODE_EL2),
              Rdb_SecurityArea::RDB_SECURITY_AREA_EL2);
    EXPECT_EQ(stats.GetAreaMode(AbilityRuntime_AreaMode::ABILITY_RUNTIME_AREA_MODE_EL3),
              Rdb_SecurityArea::RDB_SECURITY_AREA_EL3);
    EXPECT_EQ(stats.GetAreaMode(AbilityRuntime_AreaMode::ABILITY_RUNTIME_AREA_MODE_EL4),
              Rdb_SecurityArea::RDB_SECURITY_AREA_EL4);
    EXPECT_EQ(stats.GetAreaMode(AbilityRuntime_AreaMode::ABILITY_RUNTIME_AREA_MODE_EL5),
              Rdb_SecurityArea::RDB_SECURITY_AREA_EL5);
    auto unknownMode = static_cast<AbilityRuntime_AreaMode>(99);
    EXPECT_EQ(stats.GetAreaMode(unknownMode), Rdb_SecurityArea::RDB_SECURITY_AREA_EL2);
}

TEST_F(NWebStatsAdapterTest, RdbNullptrStoreTest)
{
    auto& stats = OhosWebNWebStatsAdapterImpl::GetInstance();
    std::swap(stats.rdbStore_, g_TmpRdbStore);

    EXPECT_EQ(stats.rdbStore_, nullptr);
    EXPECT_EQ(stats.QueryInt64Value("any_key"), 0);
    stats.StoreInt64Value("any_key", 123);

    std::swap(stats.rdbStore_, g_TmpRdbStore);
    EXPECT_EQ(stats.QueryInt64Value("any_key"), 0);
}

// InitDatabase is normally bypassed because the tests open their own store.
// These cases construct a real adapter instance so the whole initialization
// path, including table creation and the reopen branch, is executed.
class NWebStatsInitDatabaseTest : public testing::Test {
public:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
        g_cache_root = temp_dir_.GetPath().value();
        g_stage = AbilityRuntimeStage::kOk;
        ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().AppendASCII("web")));
    }

    void TearDown() override
    {
        g_stage = AbilityRuntimeStage::kOk;
        g_cache_root.clear();
    }

protected:
    base::ScopedTempDir temp_dir_;
};

TEST_F(NWebStatsInitDatabaseTest, CreatesDatabaseAndTable)
{
    OhosWebNWebStatsAdapterImpl stats;
    EXPECT_TRUE(stats.dbInitialized_);
    ASSERT_NE(stats.rdbStore_, nullptr);
    EXPECT_EQ(stats.QueryInt64Value(MAX_INSTANCE_COUNT_KEY), 0);
}

TEST_F(NWebStatsInitDatabaseTest, ReopensExistingDatabase)
{
    {
        OhosWebNWebStatsAdapterImpl first;
        ASSERT_TRUE(first.dbInitialized_);
        first.StoreInt64Value(MAX_INSTANCE_COUNT_KEY, 7);
    }
    OhosWebNWebStatsAdapterImpl second;
    EXPECT_TRUE(second.dbInitialized_);
    EXPECT_EQ(second.QueryInt64Value(MAX_INSTANCE_COUNT_KEY), 7);
}

TEST_F(NWebStatsInitDatabaseTest, CacheDirFailure) {
    g_stage = AbilityRuntimeStage::kCacheDirError;
    OhosWebNWebStatsAdapterImpl stats;
    EXPECT_FALSE(stats.dbInitialized_);
    EXPECT_EQ(stats.rdbStore_, nullptr);
}

TEST_F(NWebStatsInitDatabaseTest, MissingWebDirectoryFailure) {
    g_stage = AbilityRuntimeStage::kMissingWebDir;
    ASSERT_TRUE(base::DeletePathRecursively(temp_dir_.GetPath().AppendASCII("web")));
    OhosWebNWebStatsAdapterImpl stats;
    EXPECT_FALSE(stats.dbInitialized_);
    EXPECT_EQ(stats.rdbStore_, nullptr);
}

TEST_F(NWebStatsInitDatabaseTest, BundleNameFailure) {
    g_stage = AbilityRuntimeStage::kBundleNameError;
    OhosWebNWebStatsAdapterImpl stats;
    EXPECT_FALSE(stats.dbInitialized_);
    EXPECT_EQ(stats.rdbStore_, nullptr);
}

TEST_F(NWebStatsInitDatabaseTest, AreaModeFailure) {
    g_stage = AbilityRuntimeStage::kAreaModeError;
    OhosWebNWebStatsAdapterImpl stats;
    EXPECT_FALSE(stats.dbInitialized_);
    EXPECT_EQ(stats.rdbStore_, nullptr);
}

TEST_F(NWebStatsInitDatabaseTest, RdbOpenFailure) {
    ASSERT_TRUE(base::DeletePathRecursively(temp_dir_.GetPath().AppendASCII("web")));
    const std::string not_a_directory = "not a directory";
    ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().AppendASCII("web"), not_a_directory));
    OhosWebNWebStatsAdapterImpl stats;
    EXPECT_FALSE(stats.dbInitialized_);
    EXPECT_EQ(stats.rdbStore_, nullptr);
}

TEST_F(NWebStatsInitDatabaseTest, StoreFailsWithoutTable) {
    OhosWebNWebStatsAdapterImpl stats;
    ASSERT_NE(stats.rdbStore_, nullptr);

    const std::string web_dir = temp_dir_.GetPath().AppendASCII("web").value();
    OH_Rdb_Config config = {0};
    config.selfSize = sizeof(OH_Rdb_Config);
    config.dataBaseDir = web_dir.c_str();
    config.bundleName = "com.example.arkweb.ut";
    config.storeName = "empty.db";
    config.area = Rdb_SecurityArea::RDB_SECURITY_AREA_EL2;
    config.securityLevel = OH_Rdb_SecurityLevel::S1;
    int errCode = RDB_OK;
    OH_Rdb_Store* empty_store = OH_Rdb_GetOrOpen(&config, &errCode);
    ASSERT_NE(empty_store, nullptr);

    OH_Rdb_Store* original = stats.rdbStore_;
    stats.rdbStore_ = empty_store;
    stats.StoreInt64Value("missing_table_key", 1);
    stats.rdbStore_ = original;
    OH_Rdb_CloseStore(empty_store);
}

TEST_F(NWebStatsInitDatabaseTest, QueryMissingKeyAndResetInterval)
{
    OhosWebNWebStatsAdapterImpl stats;
    ASSERT_NE(stats.rdbStore_, nullptr);

    EXPECT_EQ(stats.QueryInt64Value("missing"), 0);
    EXPECT_GT(stats.GetResetIntervalSeconds(), 0);

    stats.StoreInt64Value(MAX_INSTANCE_COUNT_KEY, -5);
    EXPECT_EQ(stats.GetMaxInstanceCount(), 0);
}
} // namespace OHOS::NWeb
