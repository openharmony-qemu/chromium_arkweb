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

#include <gtest/gtest.h>

#include "arkweb/ohos_nweb_ex/build/features/features.h"

#if BUILDFLAG(ARKWEB_NWEB_EX)

#include "nweb_permission_status_query_manager.h"
#include "ohos_nweb_ex/public/capi/arkweb_access_query.h"
#include "ohos_nweb_ex/public/capi/arkweb_permission_status_query.h"
#include "third_party/bounds_checking_function/include/securec.h"

namespace OHOS::NWeb {

class NWebPermissionStatusQueryManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

static int32_t g_report_status = -1;
static int g_report_key = -1;

static void TestReportQueryResult(int32_t status, int key) {
  g_report_status = status;
  g_report_key = key;
}

TEST_F(NWebPermissionStatusQueryManagerTest, QueryPermissionStatus_NullQuery) {
  // Should not crash with null query
  EXPECT_NO_FATAL_FAILURE(
      NWebPermissionStatusQueryManager::QueryPermissionStatus(nullptr));
}

TEST_F(NWebPermissionStatusQueryManagerTest,
       QueryPermissionStatus_NullAccessQuery) {
  ArkWebPermissionStatusQuery query;
  (void)memset_s(&query, sizeof(query), 0, sizeof(query));
  query.cef_access_query_key = 123;
  query.resource_id = 1;
  query.arkweb_access_query = nullptr;
  // Should not crash with null access query
  EXPECT_NO_FATAL_FAILURE(
      NWebPermissionStatusQueryManager::QueryPermissionStatus(&query));
}

TEST_F(NWebPermissionStatusQueryManagerTest,
       QueryPermissionStatus_WithAccessQuery) {
  ArkWebAccessQuery access_query;
  (void)memset_s(&access_query, sizeof(access_query), 0, sizeof(access_query));
  access_query.cef_access_query_key = 456;
  access_query.report_query_result = &TestReportQueryResult;

  ArkWebPermissionStatusQuery query;
  (void)memset_s(&query, sizeof(query), 0, sizeof(query));
  query.cef_access_query_key = 456;
  query.resource_id = 1;
  query.arkweb_access_query = &access_query;

  g_report_status = -1;
  g_report_key = -1;

  NWebPermissionStatusQueryManager::QueryPermissionStatus(&query);

  // If OnPermissionStatusQuery returns false, the fallback path calls
  // report_query_result with status=1 and the access query key
  EXPECT_EQ(g_report_status, 1);
  EXPECT_EQ(g_report_key, 456);
}

}  // namespace OHOS::NWeb

#endif  // BUILDFLAG(ARKWEB_NWEB_EX)
