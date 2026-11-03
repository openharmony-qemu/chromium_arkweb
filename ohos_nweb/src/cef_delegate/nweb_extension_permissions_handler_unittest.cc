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

#include "nweb_extension_permissions_handler.h"

#include <gtest/gtest.h>

#include "extensions/browser/permissions/site_permissions_helper.h"

namespace OHOS::NWeb {

class NWebExtensionPermissionsHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test: GetExtensionSiteInteraction with empty extension_id
TEST_F(NWebExtensionPermissionsHandlerTest,
       GetExtensionSiteInteraction_EmptyExtensionId) {
  std::string empty_extension_id = "";
  int tab_id = 1;
  int result = NWebExtensionPermissionsHandler::GetExtensionSiteInteraction(
      empty_extension_id, tab_id);
  EXPECT_EQ(result,
            static_cast<int>(
                extensions::SitePermissionsHelper::SiteInteraction::kNone));
}

// Test: GetExtensionSiteInteraction with invalid tab_id
TEST_F(NWebExtensionPermissionsHandlerTest,
       GetExtensionSiteInteraction_InvalidTabId) {
  std::string extension_id = "test_extension_id";
  int invalid_tab_id = -1;
  int result = NWebExtensionPermissionsHandler::GetExtensionSiteInteraction(
      extension_id, invalid_tab_id);
  EXPECT_EQ(result,
            static_cast<int>(
                extensions::SitePermissionsHelper::SiteInteraction::kNone));
}

// Test: GetExtensionSiteInteraction with valid parameters
TEST_F(NWebExtensionPermissionsHandlerTest,
       GetExtensionSiteInteraction_ValidParameters) {
  std::string extension_id = "test_extension_id";
  int tab_id = 1;
  int result = NWebExtensionPermissionsHandler::GetExtensionSiteInteraction(
      extension_id, tab_id);
  EXPECT_GE(result,
            static_cast<int>(
                extensions::SitePermissionsHelper::SiteInteraction::kNone));
  EXPECT_LE(result,
            static_cast<int>(
                extensions::SitePermissionsHelper::SiteInteraction::kGranted));
}

// Test: GetExtensionSiteInteraction with zero tab_id
TEST_F(NWebExtensionPermissionsHandlerTest,
       GetExtensionSiteInteraction_ZeroTabId) {
  std::string extension_id = "test_extension_id";
  int tab_id = 0;
  int result = NWebExtensionPermissionsHandler::GetExtensionSiteInteraction(
      extension_id, tab_id);
  EXPECT_EQ(result,
            static_cast<int>(
                extensions::SitePermissionsHelper::SiteInteraction::kNone));
}

// Test: HasApiPermission with empty extension_id
TEST_F(NWebExtensionPermissionsHandlerTest, HasApiPermission_EmptyExtensionId) {
  std::string empty_extension_id = "";
  std::string permission = "storage";
  bool result = NWebExtensionPermissionsHandler::HasApiPermission(
      empty_extension_id, permission);
  EXPECT_FALSE(result);
}

// Test: HasApiPermission with empty permission
TEST_F(NWebExtensionPermissionsHandlerTest, HasApiPermission_EmptyPermission) {
  std::string extension_id = "test_extension_id";
  std::string empty_permission = "";
  bool result = NWebExtensionPermissionsHandler::HasApiPermission(
      extension_id, empty_permission);
  EXPECT_FALSE(result);
}

// Test: HasApiPermission with both empty parameters
TEST_F(NWebExtensionPermissionsHandlerTest, HasApiPermission_BothEmpty) {
  std::string empty_extension_id = "";
  std::string empty_permission = "";
  bool result = NWebExtensionPermissionsHandler::HasApiPermission(
      empty_extension_id, empty_permission);
  EXPECT_FALSE(result);
}

// Test: HasApiPermission with valid parameters
TEST_F(NWebExtensionPermissionsHandlerTest, HasApiPermission_ValidParameters) {
  std::string extension_id = "test_extension_id";
  std::string permission = "storage";
  bool result = NWebExtensionPermissionsHandler::HasApiPermission(extension_id,
                                                                  permission);
  EXPECT_FALSE(result);
}

// Test: HasApiPermission with common permission names
TEST_F(NWebExtensionPermissionsHandlerTest,
       HasApiPermission_CommonPermissions) {
  std::string extension_id = "test_extension_id";

  std::vector<std::string> common_permissions = {
      "storage",       "tabs",   "activeTab", "webNavigation", "cookies",
      "notifications", "alarms", "idle",      "contextMenus"};

  for (const auto& permission : common_permissions) {
    bool result = NWebExtensionPermissionsHandler::HasApiPermission(
        extension_id, permission);
    EXPECT_FALSE(result);
  }
}

// Test: GetExtensionSiteInteraction with different tab_ids
TEST_F(NWebExtensionPermissionsHandlerTest,
       GetExtensionSiteInteraction_DifferentTabIds) {
  std::string extension_id = "test_extension_id";

  std::vector<int> tab_ids = {0, 1, 100, 999, -1, -100};

  for (int tab_id : tab_ids) {
    int result = NWebExtensionPermissionsHandler::GetExtensionSiteInteraction(
        extension_id, tab_id);
    EXPECT_GE(result,
              static_cast<int>(
                  extensions::SitePermissionsHelper::SiteInteraction::kNone));
    EXPECT_LE(
        result,
        static_cast<int>(
            extensions::SitePermissionsHelper::SiteInteraction::kGranted));
  }
}

// Test: GetExtensionSiteInteraction with different extension_ids
TEST_F(NWebExtensionPermissionsHandlerTest,
       GetExtensionSiteInteraction_DifferentExtensionIds) {
  int tab_id = 1;

  std::vector<std::string> extension_ids = {"", "test", "extension_id_123",
                                            "abcdefghijklmnopqrstuvwxzy"};

  for (const auto& extension_id : extension_ids) {
    int result = NWebExtensionPermissionsHandler::GetExtensionSiteInteraction(
        extension_id, tab_id);
    EXPECT_GE(result,
              static_cast<int>(
                  extensions::SitePermissionsHelper::SiteInteraction::kNone));
    EXPECT_LE(
        result,
        static_cast<int>(
            extensions::SitePermissionsHelper::SiteInteraction::kGranted));
  }
}

}  // namespace OHOS::NWeb
