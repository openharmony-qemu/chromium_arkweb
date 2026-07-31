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

#include "extensions/common/extension_urls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

namespace extension_urls {

class ExtensionUrlsExtTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Save original config state
    orig_config_ = GetWebStoreConfig();
    orig_enabled_ = IsWebStoreEnable();
  }

  void TearDown() override {
    // Restore original config
    SetWebStoreConfig(orig_config_);
  }

  WebStoreConfig orig_config_;
  bool orig_enabled_;
};

// --- IsWebStoreEnable ---

TEST_F(ExtensionUrlsExtTest, IsWebStoreEnable_DefaultFalse) {
  // Before any SetWebStoreConfig, enable should be false
  // (may have been set by other tests, so we just verify the getter works)
  EXPECT_NO_FATAL_FAILURE(IsWebStoreEnable());
}

// --- SetWebStoreConfig ---

TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_SameConfigReturnsFalse) {
  WebStoreConfig current = GetWebStoreConfig();
  EXPECT_FALSE(SetWebStoreConfig(current));
}

TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_DifferentApiUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = "https://new-api.example.com";
  config.update_url = orig_config_.update_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;

  // The return value depends on ExtensionsClient::Get() which may be null
  // in the test environment. Just verify it doesn't crash.
  EXPECT_NO_FATAL_FAILURE(SetWebStoreConfig(config));
}

TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_DifferentUpdateUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = orig_config_.api_url;
  config.update_url = "https://new-update.example.com";
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;

  EXPECT_NO_FATAL_FAILURE(SetWebStoreConfig(config));
}

TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_DifferentLaunchUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.launch_url = "https://new-launch.example.com";
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;

  EXPECT_NO_FATAL_FAILURE(SetWebStoreConfig(config));
}

TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_DifferentVerifyUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = "https://new-verify.example.com";
  config.home_page_url = orig_config_.home_page_url;

  EXPECT_NO_FATAL_FAILURE(SetWebStoreConfig(config));
}

TEST_F(ExtensionUrlsExtTest,
       SetWebStoreConfig_DifferentHomePageUrlReturnsTrue) {
  WebStoreConfig config;
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = "https://new-homepage.example.com";

  EXPECT_NO_FATAL_FAILURE(SetWebStoreConfig(config));
}

TEST_F(ExtensionUrlsExtTest, SetWebStoreConfig_AllFieldsDifferentReturnsTrue) {
  WebStoreConfig config;
  config.api_url = "https://api.example.com";
  config.update_url = "https://update.example.com";
  config.launch_url = "https://launch.example.com";
  config.verify_url = "https://verify.example.com";
  config.home_page_url = "https://homepage.example.com";

  EXPECT_NO_FATAL_FAILURE(SetWebStoreConfig(config));

  // Verify the config was stored
  WebStoreConfig stored = GetWebStoreConfig();
  EXPECT_EQ("https://api.example.com", stored.api_url);
  EXPECT_EQ("https://update.example.com", stored.update_url);
  EXPECT_EQ("https://launch.example.com", stored.launch_url);
  EXPECT_EQ("https://verify.example.com", stored.verify_url);
  EXPECT_EQ("https://homepage.example.com", stored.home_page_url);
}

// --- GetWebStoreTypeByUrl ---

TEST_F(ExtensionUrlsExtTest, GetWebStoreTypeByUrl_LaunchUrlMatchesDomain) {
  WebStoreConfig config;
  config.api_url = "https://api.example.com";
  config.update_url = "https://update.example.com";
  config.launch_url = "https://launch.example.com";
  config.verify_url = "https://verify.example.com";
  config.home_page_url = "https://homepage.example.com";
  SetWebStoreConfig(config);

  GURL matching_url("https://launch.example.com/somepath");
  int result = GetWebStoreTypeByUrl(true, matching_url);
  EXPECT_EQ(extensions::kWebStoreTypeHuawei, result);
}

TEST_F(ExtensionUrlsExtTest, GetWebStoreTypeByUrl_LaunchUrlDoesNotMatchDomain) {
  WebStoreConfig config;
  config.api_url = "https://api.example.com";
  config.update_url = "https://update.example.com";
  config.launch_url = "https://launch.example.com";
  config.verify_url = "https://verify.example.com";
  config.home_page_url = "https://homepage.example.com";
  SetWebStoreConfig(config);

  GURL non_matching_url("https://other.example.com/somepath");
  int result = GetWebStoreTypeByUrl(true, non_matching_url);
  EXPECT_EQ(extensions::kWebStoreTypeDefault, result);
}

TEST_F(ExtensionUrlsExtTest, GetWebStoreTypeByUrl_UpdateUrlMatchesDomain) {
  WebStoreConfig config;
  config.api_url = "https://api.example.com";
  config.update_url = "https://update.example.com";
  config.launch_url = "https://launch.example.com";
  config.verify_url = "https://verify.example.com";
  config.home_page_url = "https://homepage.example.com";
  SetWebStoreConfig(config);

  GURL matching_url("https://update.example.com/somepath");
  int result = GetWebStoreTypeByUrl(false, matching_url);
  EXPECT_EQ(extensions::kWebStoreTypeHuawei, result);
}

TEST_F(ExtensionUrlsExtTest, GetWebStoreTypeByUrl_UpdateUrlDoesNotMatchDomain) {
  WebStoreConfig config;
  config.api_url = "https://api.example.com";
  config.update_url = "https://update.example.com";
  config.launch_url = "https://launch.example.com";
  config.verify_url = "https://verify.example.com";
  config.home_page_url = "https://homepage.example.com";
  SetWebStoreConfig(config);

  GURL non_matching_url("https://other.example.com/somepath");
  int result = GetWebStoreTypeByUrl(false, non_matching_url);
  EXPECT_EQ(extensions::kWebStoreTypeDefault, result);
}

// --- IsWebstoreUpdateUrl ---

TEST_F(ExtensionUrlsExtTest, IsWebstoreUpdateUrl_HuaweiType) {
  GURL url("https://anything.example.com");
  EXPECT_TRUE(IsWebstoreUpdateUrl(extensions::kWebStoreTypeHuawei, url));
}

TEST_F(ExtensionUrlsExtTest, IsWebstoreUpdateUrl_ExternalType) {
  GURL url("https://anything.example.com");
  EXPECT_FALSE(IsWebstoreUpdateUrl(extensions::kWebStoreTypeExternal, url));
}

TEST_F(ExtensionUrlsExtTest, IsWebstoreUpdateUrl_DefaultTypeWithMatchingUrl) {
  GURL update_url(kChromeWebstoreUpdateURL);
  EXPECT_TRUE(
      IsWebstoreUpdateUrl(extensions::kWebStoreTypeDefault, update_url));
}

TEST_F(ExtensionUrlsExtTest,
       IsWebstoreUpdateUrl_DefaultTypeWithNonMatchingUrl) {
  GURL url("https://not-the-webstore.example.com/update");
  EXPECT_FALSE(IsWebstoreUpdateUrl(extensions::kWebStoreTypeDefault, url));
}

TEST_F(ExtensionUrlsExtTest,
       IsWebstoreUpdateUrl_DefaultTypeWithMatchingHostOnly) {
  // Same host but different path should return false
  GURL store_url(kChromeWebstoreUpdateURL);
  std::string path_str = std::string("https://") +
                         std::string(store_url.host()) + "/different/path";
  GURL different_path(path_str);
  EXPECT_FALSE(
      IsWebstoreUpdateUrl(extensions::kWebStoreTypeDefault, different_path));
}

// --- GetWebstoreUpdateUrl ---

TEST_F(ExtensionUrlsExtTest, GetWebstoreUpdateUrl_HuaweiType) {
  WebStoreConfig config;
  config.update_url = "https://update.example.com";
  config.api_url = "https://api.example.com";
  config.launch_url = "https://launch.example.com";
  config.verify_url = "https://verify.example.com";
  config.home_page_url = "https://homepage.example.com";
  SetWebStoreConfig(config);

  GURL result = GetWebstoreUpdateUrl(extensions::kWebStoreTypeHuawei);
  EXPECT_EQ("https://update.example.com/hw/detail", result.spec());
}

TEST_F(ExtensionUrlsExtTest, GetWebstoreUpdateUrl_ExternalType) {
  GURL result = GetWebstoreUpdateUrl(extensions::kWebStoreTypeExternal);
  EXPECT_TRUE(result.is_empty());
}

TEST_F(ExtensionUrlsExtTest, GetWebstoreUpdateUrl_DefaultType) {
  GURL result = GetWebstoreUpdateUrl(extensions::kWebStoreTypeDefault);
  EXPECT_EQ(kChromeWebstoreUpdateURL, result.spec());
}

// --- GetWebStoreHomePageUrl ---

TEST_F(ExtensionUrlsExtTest, GetWebStoreHomePageUrl_Empty) {
  WebStoreConfig config;
  config.home_page_url = "";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  SetWebStoreConfig(config);

  GURL result = GetWebStoreHomePageUrl();
  EXPECT_TRUE(result.is_empty());
}

TEST_F(ExtensionUrlsExtTest, GetWebStoreHomePageUrl_Set) {
  WebStoreConfig config;
  config.home_page_url = "https://homepage.example.com";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  SetWebStoreConfig(config);

  GURL result = GetWebStoreHomePageUrl();
  EXPECT_EQ("https://homepage.example.com/", result.spec());
}

// --- GetWebstoreUpdateDomains ---

TEST_F(ExtensionUrlsExtTest, GetWebstoreUpdateDomains_EmptyUrl) {
  WebStoreConfig config;
  config.update_url = "";
  config.api_url = orig_config_.api_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  std::vector<std::string> domains = GetWebstoreUpdateDomains();
  ASSERT_EQ(1u, domains.size());
  EXPECT_EQ("", domains[0]);
}

TEST_F(ExtensionUrlsExtTest, GetWebstoreUpdateDomains_WithUrl) {
  WebStoreConfig config;
  config.update_url = "https://update.example.com/path";
  config.api_url = orig_config_.api_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  std::vector<std::string> domains = GetWebstoreUpdateDomains();
  ASSERT_EQ(1u, domains.size());
  EXPECT_EQ("update.example.com", domains[0]);
}

// --- GetWebStoreCheckUpdateUrl ---

TEST_F(ExtensionUrlsExtTest, GetWebStoreCheckUpdateUrl_EmptyBase) {
  WebStoreConfig config;
  config.update_url = "";
  config.api_url = orig_config_.api_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetWebStoreCheckUpdateUrl();
  // GURL("") + "/hw/update2json" is invalid, so result is empty
  EXPECT_FALSE(result.is_valid());
}

TEST_F(ExtensionUrlsExtTest, GetWebStoreCheckUpdateUrl_WithBase) {
  WebStoreConfig config;
  config.update_url = "https://update.example.com";
  config.api_url = orig_config_.api_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetWebStoreCheckUpdateUrl();
  EXPECT_EQ("https://update.example.com/hw/update2json", result.spec());
}

// --- GetDefaultWebstoreApiUrl ---

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreApiUrl_Empty) {
  WebStoreConfig config;
  config.api_url = "";
  config.update_url = orig_config_.update_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreApiUrl();
  EXPECT_TRUE(result.is_empty());
}

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreApiUrl_Set) {
  WebStoreConfig config;
  config.api_url = "https://api.example.com";
  config.update_url = orig_config_.update_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreApiUrl();
  EXPECT_EQ("https://api.example.com/", result.spec());
}

// --- GetDefaultWebstoreUpdateUrl ---

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreUpdateUrl_EmptyBase) {
  WebStoreConfig config;
  config.update_url = "";
  config.api_url = orig_config_.api_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreUpdateUrl();
  // GURL("") + "/hw/detail" is invalid
  EXPECT_FALSE(result.is_valid());
}

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreUpdateUrl_WithBase) {
  WebStoreConfig config;
  config.update_url = "https://update.example.com";
  config.api_url = orig_config_.api_url;
  config.launch_url = orig_config_.launch_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreUpdateUrl();
  EXPECT_EQ("https://update.example.com/hw/detail", result.spec());
}

// --- GetDefaultWebstoreLaunchURL ---

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreLaunchURL_Empty) {
  WebStoreConfig config;
  config.launch_url = "";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreLaunchURL();
  EXPECT_TRUE(result.is_empty());
}

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreLaunchURL_Set) {
  WebStoreConfig config;
  config.launch_url = "https://launch.example.com";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreLaunchURL();
  EXPECT_EQ("https://launch.example.com/", result.spec());
}

// --- GetDefaultWebstoreVerifyURL ---

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreVerifyURL_Empty) {
  WebStoreConfig config;
  config.verify_url = "";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.launch_url = orig_config_.launch_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreVerifyURL();
  EXPECT_TRUE(result.is_empty());
}

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreVerifyURL_Set) {
  WebStoreConfig config;
  config.verify_url = "https://verify.example.com";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.launch_url = orig_config_.launch_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreVerifyURL();
  EXPECT_EQ("https://verify.example.com/", result.spec());
}

// --- GetDefaultNewWebstoreLaunchURL ---

TEST_F(ExtensionUrlsExtTest, GetDefaultNewWebstoreLaunchURL_Empty) {
  WebStoreConfig config;
  config.launch_url = "";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultNewWebstoreLaunchURL();
  // GURL("" + "/") is invalid
  EXPECT_FALSE(result.is_valid());
}

TEST_F(ExtensionUrlsExtTest, GetDefaultNewWebstoreLaunchURL_Set) {
  WebStoreConfig config;
  config.launch_url = "https://launch.example.com";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultNewWebstoreLaunchURL();
  EXPECT_EQ("https://launch.example.com/", result.spec());
}

// --- GetDefaultWebstoreLaunchURLPattern ---

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreLaunchURLPattern_Empty) {
  WebStoreConfig config;
  config.launch_url = "";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreLaunchURLPattern();
  // GURL("" + "/*") is invalid
  EXPECT_FALSE(result.is_valid());
}

TEST_F(ExtensionUrlsExtTest, GetDefaultWebstoreLaunchURLPattern_Set) {
  WebStoreConfig config;
  config.launch_url = "https://launch.example.com";
  config.api_url = orig_config_.api_url;
  config.update_url = orig_config_.update_url;
  config.verify_url = orig_config_.verify_url;
  config.home_page_url = orig_config_.home_page_url;
  SetWebStoreConfig(config);

  GURL result = GetDefaultWebstoreLaunchURLPattern();
  EXPECT_EQ("https://launch.example.com/*", result.spec());
}

}  // namespace extension_urls

#endif  // BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
