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

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "extensions/common/extension_urls.h"

namespace extension_urls {

// Direct-call tests for extension_urls_ext.cc functions.
// These functions operate on file-scope global state (g_web_store_enable,
// g_web_store_config), so test ordering matters.

class ExtensionUrlsExtTest : public testing::Test {
 protected:
  void SetUp() override {}
};

// IsWebStoreEnable - initial state is false (default)
TEST_F(ExtensionUrlsExtTest, IsWebStoreEnable_DefaultIsFalse) {
  // Before any SetWebStoreConfig call, enable should be false.
  // However, other tests may have called SetWebStoreConfig, so we check
  // that the function is callable without crashing.
  // Just verify the return type is bool.
  bool result = IsWebStoreEnable();
  EXPECT_FALSE(result);
}

// GetWebStoreConfig - returns a copy of current config
TEST_F(ExtensionUrlsExtTest, GetWebStoreConfig_ReturnsCopy) {
  WebStoreConfig config = GetWebStoreConfig();
  // Default config has empty strings
  EXPECT_TRUE(config.api_url.empty());
  EXPECT_TRUE(config.update_url.empty());
  EXPECT_TRUE(config.launch_url.empty());
  EXPECT_TRUE(config.verify_url.empty());
  EXPECT_TRUE(config.home_page_url.empty());
}

// SetWebStoreConfig - same config returns false
TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_SameConfigReturnsFalse) {
  // Set a known config first
  WebStoreConfig config1;
  config1.api_url = "https://api.example.com";
  config1.update_url = "https://update.example.com";
  config1.launch_url = "https://launch.example.com";
  config1.verify_url = "https://verify.example.com";
  config1.home_page_url = "https://home.example.com";
  SetWebStoreConfig(config1);

  // Setting the same config again should return false
  bool result = SetWebStoreConfig(config1);
  EXPECT_FALSE(result);
}

// SetWebStoreConfig - changed api_url returns true
TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_ChangedApiUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = "https://api2.example.com";
  config.update_url = "https://update.example.com";
  config.launch_url = "https://launch.example.com";
  config.verify_url = "https://verify.example.com";
  config.home_page_url = "https://home.example.com";
  bool result = SetWebStoreConfig(config);
  EXPECT_TRUE(result);
}

// SetWebStoreConfig - changed launch_url returns true
TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_ChangedLaunchUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = "https://api2.example.com";
  config.update_url = "https://update.example.com";
  config.launch_url = "https://launch2.example.com";
  config.verify_url = "https://verify.example.com";
  config.home_page_url = "https://home.example.com";
  bool result = SetWebStoreConfig(config);
  EXPECT_TRUE(result);
}

// SetWebStoreConfig - changed update_url returns true
TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_ChangedUpdateUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = "https://api2.example.com";
  config.update_url = "https://update2.example.com";
  config.launch_url = "https://launch2.example.com";
  config.verify_url = "https://verify.example.com";
  config.home_page_url = "https://home.example.com";
  bool result = SetWebStoreConfig(config);
  EXPECT_TRUE(result);
}

// SetWebStoreConfig - changed verify_url returns true
TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_ChangedVerifyUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = "https://api2.example.com";
  config.update_url = "https://update2.example.com";
  config.launch_url = "https://launch2.example.com";
  config.verify_url = "https://verify2.example.com";
  config.home_page_url = "https://home.example.com";
  bool result = SetWebStoreConfig(config);
  EXPECT_TRUE(result);
}

// SetWebStoreConfig - changed home_page_url returns true
TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_ChangedHomePageUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = "https://api2.example.com";
  config.update_url = "https://update2.example.com";
  config.launch_url = "https://launch2.example.com";
  config.verify_url = "https://verify2.example.com";
  config.home_page_url = "https://home2.example.com";
  bool result = SetWebStoreConfig(config);
  EXPECT_TRUE(result);
}

// SetWebStoreConfig - after setting, IsWebStoreEnable returns true
TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_ChangeEnablesWebStore) {
  WebStoreConfig config;
  config.api_url = "https://api3.example.com";
  config.update_url = "https://update3.example.com";
  SetWebStoreConfig(config);
  EXPECT_TRUE(IsWebStoreEnable());
}

// SetWebStoreConfig - all five fields changed returns true
TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_AllFieldsChangedReturnsTrue) {
  WebStoreConfig config;
  config.api_url = "https://api4.example.com";
  config.update_url = "https://update4.example.com";
  config.launch_url = "https://launch4.example.com";
  config.verify_url = "https://verify4.example.com";
  config.home_page_url = "https://home4.example.com";
  bool result = SetWebStoreConfig(config);
  EXPECT_TRUE(result);
  // Verify config was stored
  WebStoreConfig stored = GetWebStoreConfig();
  EXPECT_EQ(stored.api_url, "https://api4.example.com");
  EXPECT_EQ(stored.update_url, "https://update4.example.com");
  EXPECT_EQ(stored.launch_url, "https://launch4.example.com");
  EXPECT_EQ(stored.verify_url, "https://verify4.example.com");
  EXPECT_EQ(stored.home_page_url, "https://home4.example.com");
}

// GetWebStoreTypeByUrl - flag=true with matching launch_url host
TEST_F(ExtensionUrlsExtTest, GetWebStoreTypeByUrl_FlagTrueMatchingLaunchUrl) {
  // First set a config with known launch_url
  WebStoreConfig config;
  config.launch_url = "https://launch4.example.com";
  config.update_url = "https://update4.example.com";
  SetWebStoreConfig(config);

  GURL url("https://launch4.example.com/somepath");
  int result = GetWebStoreTypeByUrl(true, url);
  EXPECT_EQ(result, extensions::kWebStoreTypeHuawei);
}

// GetWebStoreTypeByUrl - flag=true with non-matching launch_url host
TEST_F(ExtensionUrlsExtTest,
       GetWebStoreTypeByUrl_FlagTrueNonMatchingLaunchUrl) {
  GURL url("https://other.example.com/somepath");
  int result = GetWebStoreTypeByUrl(true, url);
  EXPECT_EQ(result, extensions::kWebStoreTypeDefault);
}

// GetWebStoreTypeByUrl - flag=false with matching update_url host
TEST_F(ExtensionUrlsExtTest, GetWebStoreTypeByUrl_FlagFalseMatchingUpdateUrl) {
  GURL url("https://update4.example.com/somepath");
  int result = GetWebStoreTypeByUrl(false, url);
  EXPECT_EQ(result, extensions::kWebStoreTypeHuawei);
}

// GetWebStoreTypeByUrl - flag=false with non-matching update_url host
TEST_F(ExtensionUrlsExtTest,
       GetWebStoreTypeByUrl_FlagFalseNonMatchingUpdateUrl) {
  GURL url("https://other.example.com/somepath");
  int result = GetWebStoreTypeByUrl(false, url);
  EXPECT_EQ(result, extensions::kWebStoreTypeDefault);
}

TEST_F(ExtensionUrlsExtTest, IsWebstoreUpdateUrl_HuaweiTypeReturnsTrue) {
  GURL dummy_url("https://example.com");
  EXPECT_TRUE(IsWebstoreUpdateUrl(extensions::kWebStoreTypeHuawei, dummy_url));
}

// IsWebstoreUpdateUrl - External type always returns false
TEST_F(ExtensionUrlsExtTest, IsWebstoreUpdateUrl_ExternalTypeReturnsFalse) {
  GURL dummy_url("https://example.com");
  EXPECT_FALSE(
      IsWebstoreUpdateUrl(extensions::kWebStoreTypeExternal, dummy_url));
}

// IsWebstoreUpdateUrl - Default type with matching URL
TEST_F(ExtensionUrlsExtTest, IsWebstoreUpdateUrl_DefaultTypeWithMatchingUrl) {
  // The default type checks against kChromeWebstoreUpdateURL
  GURL update_url((GURL(kChromeWebstoreUpdateURL)));
  EXPECT_TRUE(
      IsWebstoreUpdateUrl(extensions::kWebStoreTypeDefault, update_url));
}

// IsWebstoreUpdateUrl - Default type with non-matching URL
TEST_F(ExtensionUrlsExtTest,
       IsWebstoreUpdateUrl_DefaultTypeWithNonMatchingUrl) {
  GURL other_url("https://example.com/update");
  EXPECT_FALSE(
      IsWebstoreUpdateUrl(extensions::kWebStoreTypeDefault, other_url));
}

// IsWebstoreUpdateUrl - Default type with matching host but different path
TEST_F(ExtensionUrlsExtTest,
       IsWebstoreUpdateUrl_DefaultTypeHostMatchPathMismatch) {
  GURL store_url((GURL(kChromeWebstoreUpdateURL)));
  // Construct a URL with the same host but different path
  GURL different_path_url(std::string("https://") +
                          std::string(store_url.host()) + "/different/path");
  EXPECT_FALSE(IsWebstoreUpdateUrl(extensions::kWebStoreTypeDefault,
                                   different_path_url));
}

TEST_F(ExtensionUrlsExtTest, GetWebstoreUpdateUrl_HuaweiTypeReturnsCustomUrl) {
  GURL result = GetWebstoreUpdateUrl(extensions::kWebStoreTypeHuawei);
  EXPECT_TRUE(result.is_valid());
  // Should be update_url + "/hw/detail" (kDownloadCrxPath)
  WebStoreConfig config = GetWebStoreConfig();
  EXPECT_EQ(result.spec(), GURL(config.update_url + "/hw/detail").spec());
}

// GetWebstoreUpdateUrl - External type returns empty GURL
TEST_F(ExtensionUrlsExtTest, GetWebstoreUpdateUrl_ExternalTypeReturnsEmpty) {
  GURL result = GetWebstoreUpdateUrl(extensions::kWebStoreTypeExternal);
  EXPECT_FALSE(result.is_valid());
}

// GetWebstoreUpdateUrl - Default type returns Chrome webstore URL
TEST_F(ExtensionUrlsExtTest, GetWebstoreUpdateUrl_DefaultTypeReturnsChromeUrl) {
  GURL result = GetWebstoreUpdateUrl(extensions::kWebStoreTypeDefault);
  EXPECT_EQ(result, GURL(kChromeWebstoreUpdateURL));
}

// GetWebStoreHomePageUrl - returns configured home_page_url
TEST_F(ExtensionUrlsExtTest, GetWebStoreHomePageUrl_WithConfig) {
  GURL result = GetWebStoreHomePageUrl();
  WebStoreConfig config = GetWebStoreConfig();
  EXPECT_EQ(result, GURL(config.home_page_url));
}

// GetWebstoreUpdateDomains - returns domain from update_url
TEST_F(ExtensionUrlsExtTest, GetWebstoreUpdateDomains_WithUrl) {
  std::vector<std::string> domains = GetWebstoreUpdateDomains();
  EXPECT_EQ(domains.size(), 1u);
  WebStoreConfig config = GetWebStoreConfig();
  GURL url(config.update_url);
  EXPECT_EQ(domains[0], url.host());
}

// GetWebStoreCheckUpdateUrl - concatenates update_url with kCheckCrxPath
TEST_F(ExtensionUrlsExtTest, GetWebStoreCheckUpdateUrl_WithUrl) {
  GURL result = GetWebStoreCheckUpdateUrl();
  WebStoreConfig config = GetWebStoreConfig();
  // kCheckCrxPath = "/hw/update2json"
  GURL expected(config.update_url + "/hw/update2json");
  EXPECT_EQ(result.spec(), expected.spec());
}

// GetDefaultWebstoreApiUrl - returns configured api_url
TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreApiUrl_WithConfig) {
  GURL result = GetDefaultWebstoreApiUrl();
  WebStoreConfig config = GetWebStoreConfig();
  EXPECT_EQ(result, GURL(config.api_url));
}

// GetDefaultWebstoreLaunchURL - returns configured launch_url
TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreLaunchURL_WithConfig) {
  GURL result = GetDefaultWebstoreLaunchURL();
  WebStoreConfig config = GetWebStoreConfig();
  EXPECT_EQ(result, GURL(config.launch_url));
}

// GetDefaultWebstoreVerifyURL - returns configured verify_url
TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreVerifyURL_WithConfig) {
  GURL result = GetDefaultWebstoreVerifyURL();
  WebStoreConfig config = GetWebStoreConfig();
  EXPECT_EQ(result, GURL(config.verify_url));
}

// GetDefaultNewWebstoreLaunchURL - returns launch_url + "/"
TEST_F(ExtensionUrlsExtTest, GetDefaultNewWebstoreLaunchURL_WithConfig) {
  GURL result = GetDefaultNewWebstoreLaunchURL();
  WebStoreConfig config = GetWebStoreConfig();
  GURL expected(config.launch_url + "/");
  EXPECT_EQ(result.spec(), expected.spec());
}

// GetDefaultWebstoreLaunchURLPattern - returns launch_url + "/*"
TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreLaunchURLPattern_WithConfig) {
  GURL result = GetDefaultWebstoreLaunchURLPattern();
  WebStoreConfig config = GetWebStoreConfig();
  GURL expected(config.launch_url + "/*");
  EXPECT_EQ(result.spec(), expected.spec());
}

// Edge case: empty config produces invalid GURLs for URLs requiring base
TEST_F(ExtensionUrlsExtTest, EmptyConfigProducesInvalidUrls) {
  // Reset config to empty strings
  WebStoreConfig empty;
  empty.api_url = "";
  empty.update_url = "";
  empty.launch_url = "";
  empty.verify_url = "";
  empty.home_page_url = "";
  SetWebStoreConfig(empty);

  // With empty base URL, GURL construction with paths is invalid
  EXPECT_FALSE(GetWebStoreCheckUpdateUrl().is_valid());
  EXPECT_FALSE(GetDefaultNewWebstoreLaunchURL().is_valid());
  EXPECT_FALSE(GetDefaultWebstoreLaunchURLPattern().is_valid());
}

}  // namespace extension_urls

#endif  // BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
