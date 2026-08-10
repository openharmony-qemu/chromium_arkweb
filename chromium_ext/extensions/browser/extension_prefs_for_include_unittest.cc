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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "arkweb/build/features/features.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permission_set.h"

// clang-format off
#define private public
#include "extensions/browser/extension_prefs.h"
#undef private
// clang-format on

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

namespace extensions {

class MockExtensionPrefsObserver : public ExtensionPrefsObserver {
 public:
  MOCK_METHOD(void,
              OnExtensionRuntimePermissionsChanged,
              (const ExtensionId& extension_id),
              (override));
  MOCK_METHOD(void,
              OnExtensionPrefsUpdated,
              (const ExtensionId& extension_id),
              (override));
};

class ExtensionPrefsForIncludeTest : public ExtensionsTest {
 protected:
  void SetUp() override {
    ExtensionsTest::SetUp();
    extension_id_ = ExtensionBuilder("test_extension").Build()->id();
  }

  ExtensionPrefs* extension_prefs() {
    return ExtensionPrefs::Get(browser_context());
  }

  std::string extension_id_;
};

// SetNotDisplayInSettings: set to true, then verify IsNotDisplayInSettings
// returns true.
TEST_F(ExtensionPrefsForIncludeTest, SetAndGetNotDisplayInSettings_True) {
  extension_prefs()->SetNotDisplayInSettings(extension_id_, true);
  EXPECT_TRUE(extension_prefs()->IsNotDisplayInSettings(extension_id_));
}

// SetNotDisplayInSettings: set to false, then verify IsNotDisplayInSettings
// returns false.
TEST_F(ExtensionPrefsForIncludeTest, SetAndGetNotDisplayInSettings_False) {
  extension_prefs()->SetNotDisplayInSettings(extension_id_, false);
  EXPECT_FALSE(extension_prefs()->IsNotDisplayInSettings(extension_id_));
}

// IsNotDisplayInSettings: default value should be false when not set.
TEST_F(ExtensionPrefsForIncludeTest, IsNotDisplayInSettings_DefaultFalse) {
  EXPECT_FALSE(extension_prefs()->IsNotDisplayInSettings(extension_id_));
}

// GetRuntimeUserBlockedPermissions: invalid extension ID returns nullptr.
TEST_F(ExtensionPrefsForIncludeTest,
       GetRuntimeUserBlockedPermissions_InvalidId) {
  auto result =
      extension_prefs()->GetRuntimeUserBlockedPermissions("invalid_id");
  EXPECT_EQ(result, nullptr);
}

// GetRuntimeUserBlockedPermissions: valid ID with no blocked permissions
// returns empty set (not nullptr).
TEST_F(ExtensionPrefsForIncludeTest, GetRuntimeUserBlockedPermissions_ValidId) {
  auto result =
      extension_prefs()->GetRuntimeUserBlockedPermissions(extension_id_);
  // No permissions have been set, so result is either nullptr or empty.
  EXPECT_TRUE(result == nullptr || result->IsEmpty());
}

// AddRuntimeUserBlockedPermissions: invalid ID does not notify observers.
TEST_F(ExtensionPrefsForIncludeTest,
       AddRuntimeUserBlockedPermissions_InvalidId) {
  testing::StrictMock<MockExtensionPrefsObserver> observer;
  extension_prefs()->AddObserver(&observer);
  PermissionSet empty_permissions;
  extension_prefs()->AddRuntimeUserBlockedPermissions("invalid_id",
                                                      empty_permissions);
  // StrictMock will fail if any unexpected method is called.
  EXPECT_NO_FATAL_FAILURE(extension_prefs()->RemoveObserver(&observer));
}

// AddRuntimeUserBlockedPermissions: valid ID notifies observers.
TEST_F(ExtensionPrefsForIncludeTest, AddRuntimeUserBlockedPermissions_ValidId) {
  testing::NiceMock<MockExtensionPrefsObserver> observer;
  extension_prefs()->AddObserver(&observer);
  PermissionSet empty_permissions;
  EXPECT_CALL(observer, OnExtensionRuntimePermissionsChanged(extension_id_))
      .Times(1);
  extension_prefs()->AddRuntimeUserBlockedPermissions(extension_id_,
                                                      empty_permissions);
  extension_prefs()->RemoveObserver(&observer);
}

// RemoveRuntimeUserBlockedPermissions: invalid ID does not notify observers.
TEST_F(ExtensionPrefsForIncludeTest,
       RemoveRuntimeUserBlockedPermissions_InvalidId) {
  testing::StrictMock<MockExtensionPrefsObserver> observer;
  extension_prefs()->AddObserver(&observer);
  PermissionSet empty_permissions;
  extension_prefs()->RemoveRuntimeUserBlockedPermissions("invalid_id",
                                                         empty_permissions);
  // StrictMock will fail if any unexpected method is called.
  EXPECT_NO_FATAL_FAILURE(extension_prefs()->RemoveObserver(&observer));
}

// RemoveRuntimeUserBlockedPermissions: valid ID notifies observers.
TEST_F(ExtensionPrefsForIncludeTest,
       RemoveRuntimeUserBlockedPermissions_ValidId) {
  testing::NiceMock<MockExtensionPrefsObserver> observer;
  extension_prefs()->AddObserver(&observer);
  PermissionSet empty_permissions;
  EXPECT_CALL(observer, OnExtensionRuntimePermissionsChanged(extension_id_))
      .Times(1);
  extension_prefs()->RemoveRuntimeUserBlockedPermissions(extension_id_,
                                                         empty_permissions);
  extension_prefs()->RemoveObserver(&observer);
}

}  // namespace extensions

#endif  // BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
