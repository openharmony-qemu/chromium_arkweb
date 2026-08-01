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

#include "arkweb/build/features/features.h"
#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

#include <gtest/gtest.h>

#include "extensions/browser/ui_util.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"

namespace extensions {
namespace ui_util {

// ---------------------------------------------------------------------------
// Unit tests for ui_util_ext.cc - the ArkWeb override of
// ShouldDisplayInExtensionSettings(const Extension&).
//
// The ArkWeb override adds two additional checks after the base Type/Location
// filter:
//   1. OHOS::NWeb::GetBrowserContext() - if null, returns true (default
//      display).
//   2. ExtensionPrefs::IsNotDisplayInSettings() - if true, returns false
//      (hidden from settings).
//
// Branch coverage:
//   [A] ShouldDisplayInExtensionSettings(Type, Location) returns false
//       -> overall returns false
//   [B] GetBrowserContext() returns nullptr -> returns true
//   [C] ExtensionPrefs::Get() returns nullptr -> returns true
//   [D] IsNotDisplayInSettings returns true -> returns false
//   [E] IsNotDisplayInSettings returns false -> returns true
//
// In the UT environment, OHOS::NWeb::GetBrowserContext() always returns
// nullptr because it depends on the CEF global context which is not
// initialized. This means branch [B] is always taken for the Extension
// overload, giving us a true result for any extension that passes the
// Type/Location check. Branches [C], [D], [E] require a valid
// BrowserContext with ExtensionPrefs and are covered by integration tests.
//
// The (Type, Location) overload is the base Chromium logic; its branches
// are fully tested here.
// ---------------------------------------------------------------------------

// --- Tests for the (Type, Location) overload ---
// This is the first gate in the ArkWeb Extension overload and determines
// whether an extension is even considered for display.

// Theme type should not display in extension settings.
TEST(UiUtilExtTest,
     ShouldDisplayInExtensionSettings_TypeLocation_Theme_ReturnsFalse) {
  EXPECT_FALSE(ShouldDisplayInExtensionSettings(
      Manifest::Type::kTheme, mojom::ManifestLocation::kInternal));
}

// Component location should not display (unless the command-line switch is
// set, which it is not in UT).
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_TypeLocation_ComponentLocation_ReturnsFalse) {
  EXPECT_FALSE(ShouldDisplayInExtensionSettings(
      Manifest::Type::kExtension, mojom::ManifestLocation::kComponent));
}

// Hosted app at internal (non-unpacked) location should not display.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_TypeLocation_HostedAppInternal_ReturnsFalse) {
  EXPECT_FALSE(ShouldDisplayInExtensionSettings(
      Manifest::TYPE_HOSTED_APP, mojom::ManifestLocation::kInternal));
}

// Hosted app at external install location should not display.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_TypeLocation_HostedAppExternal_ReturnsFalse) {
  EXPECT_FALSE(ShouldDisplayInExtensionSettings(
      Manifest::TYPE_HOSTED_APP, mojom::ManifestLocation::kExternalPref));
}

// Regular extension at internal location should display.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_TypeLocation_ExtensionInternal_ReturnsTrue) {
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(
      Manifest::Type::kExtension, mojom::ManifestLocation::kInternal));
}

// Platform app at internal location should display.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_TypeLocation_PlatformAppInternal_ReturnsTrue) {
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(
      Manifest::Type::kPlatformApp, mojom::ManifestLocation::kInternal));
}

// Unpacked hosted app should display (exception for debugging).
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_TypeLocation_UnpackedHostedApp_ReturnsTrue) {
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(
      Manifest::TYPE_HOSTED_APP, mojom::ManifestLocation::kUnpacked));
}

// Unpacked extension should display.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_TypeLocation_UnpackedExtension_ReturnsTrue) {
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(
      Manifest::Type::kExtension, mojom::ManifestLocation::kUnpacked));
}

// Legacy packaged app at internal location should display.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_TypeLocation_LegacyPackagedApp_ReturnsTrue) {
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(
      Manifest::Type::kLegacyPackagedApp, mojom::ManifestLocation::kInternal));
}

// Shared module at internal location should display.
TEST(UiUtilExtTest,
     ShouldDisplayInExtensionSettings_TypeLocation_SharedModule_ReturnsTrue) {
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(
      Manifest::Type::kSharedModule, mojom::ManifestLocation::kInternal));
}

// --- Tests for the (const Extension&) overload ---
// In UT, GetBrowserContext() returns nullptr, so the function returns true
// for any extension that passes the Type/Location gate. This covers
// branch [B].

// Regular extension with null BrowserContext returns true.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_Extension_RegularExtension_NullBrowserContext) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension").Build();
  ASSERT_TRUE(extension);
  // GetBrowserContext() returns nullptr in UT -> branch [B] -> returns true
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(*extension));
}

// Platform app with null BrowserContext returns true.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_Extension_PlatformApp_NullBrowserContext) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Platform App",
                       ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  ASSERT_TRUE(extension);
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(*extension));
}

// Extension at unpacked location with null BrowserContext returns true.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_Extension_UnpackedExtension_NullBrowserContext) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Unpacked Extension")
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .Build();
  ASSERT_TRUE(extension);
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(*extension));
}

// Theme extension: Type/Location gate returns false, so overall returns
// false even with null BrowserContext. This covers branch [A].
TEST(UiUtilExtTest,
     ShouldDisplayInExtensionSettings_Extension_Theme_ReturnsFalse) {
  // Build a theme extension using custom manifest.
  auto manifest = base::Value::Dict()
                      .Set("name", "Test Theme")
                      .Set("version", "1")
                      .Set("manifest_version", 2)
                      .Set("theme", base::Value::Dict());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  ASSERT_TRUE(extension);
  // Theme is filtered out by the Type/Location gate (branch [A])
  EXPECT_FALSE(ShouldDisplayInExtensionSettings(*extension));
}

// Hosted app at internal location: Type/Location gate returns false.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_Extension_InternalHostedApp_ReturnsFalse) {
  auto manifest =
      base::Value::Dict()
          .Set("name", "Test Hosted App")
          .Set("version", "1")
          .Set("manifest_version", 2)
          .Set("app", base::Value::Dict().Set(
                          "launch", base::Value::Dict().Set(
                                        "web_url", "https://example.com")));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  ASSERT_TRUE(extension);
  // Hosted app at non-unpacked location filtered by Type/Location gate
  EXPECT_FALSE(ShouldDisplayInExtensionSettings(*extension));
}

// Unpacked hosted app: Type/Location gate passes, null BrowserContext
// returns true.
TEST(
    UiUtilExtTest,
    ShouldDisplayInExtensionSettings_Extension_UnpackedHostedApp_NullBrowserContext) {
  auto manifest =
      base::Value::Dict()
          .Set("name", "Unpacked Hosted App")
          .Set("version", "1")
          .Set("manifest_version", 2)
          .Set("app", base::Value::Dict().Set(
                          "launch", base::Value::Dict().Set(
                                        "web_url", "https://example.com")));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .Build();
  ASSERT_TRUE(extension);
  // Unpacked hosted app passes Type/Location, null BrowserContext -> true
  EXPECT_TRUE(ShouldDisplayInExtensionSettings(*extension));
}

// ---------------------------------------------------------------------------
// Coverage summary:
//   [A] Type/Location false -> false: COVERED (theme, component, hosted app)
//   [B] GetBrowserContext() nullptr -> true: COVERED (regular extension,
//       platform app, unpacked extension in UT environment)
//   [C] ExtensionPrefs::Get() nullptr -> true: NOT COVERED (requires
//       valid BrowserContext without ExtensionPrefs; integration test needed)
//   [D] IsNotDisplayInSettings true -> false: NOT COVERED (requires
//       BrowserContext + ExtensionPrefs with flag set; integration test needed)
//   [E] IsNotDisplayInSettings false -> true: NOT COVERED (requires
//       BrowserContext + ExtensionPrefs with flag unset; integration test
//       needed)
// ---------------------------------------------------------------------------

}  // namespace ui_util
}  // namespace extensions

#endif  // BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
