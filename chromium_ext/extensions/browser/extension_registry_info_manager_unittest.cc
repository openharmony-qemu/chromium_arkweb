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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#define private public
#include "extensions/browser/extension_registry_info_manager.h"
#include "extensions/common/extension.h"
#undef private
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "extension_registry_info_manager_test.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/api/chrome_url_overrides.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/chrome_url_overrides_handler.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/test/test_extensions_client.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif

#if BUILDFLAG(ARKWEB_NWEB_EX)
#include "extensions/browser/extension_util.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "ohos_nweb_ex/core/arkweb/nweb_extension_manager_delegate_impl.h"
#endif

namespace extensions {
class Extension;
class ExtensionAction;
class ExtensionRegistry;

namespace {
static int g_expect_reason = 0;

struct ManifestTestHelper {
  std::function<void(base::Value::Dict& manifest)> initManifest;
  std::function<void(const WebExtensionManifestInfo& manifest)> checkManifest;
};
using extensions::api::chrome_url_overrides::UrlOverrideInfo;
using extensions::api::manifest_types::ChromeSettingsOverrides;
using ChromeUrlOverridesKeys = api::chrome_url_overrides::ManifestKeys;
}  // namespace

class ExtensionRegistryInfoManagerTest : public ExtensionsTest {
 public:
  ExtensionRegistryInfoManagerTest() {}
  ~ExtensionRegistryInfoManagerTest() {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ExtensionsTest::SetUp();
    client_ = std::make_unique<extensions::TestExtensionsClient>();
    extensions::ExtensionsClient::Set(client_.get());
    manager_ =
        std::make_unique<ExtensionRegistryInfoManager>(browser_context());
    callback_ = nweb_ex::NWebExtensionManagerDelegateImpl::GetInstance()
                    .CreateCallback();
  }

  void TearDown() override {
    nweb_ex::NWebExtensionManagerDelegateImpl::GetInstance().ReleaseCallback();
    callback_ = nullptr;
    manager_.reset();
    client_.reset();
    ExtensionsTest::TearDown();
    base::DeletePathRecursively(temp_dir_.GetPath());
  }

  scoped_refptr<Extension> CreateTestExtension(
      const std::string& name,
      api::incognito::IncognitoMode mode =
          api::incognito::IncognitoMode::kNone) {
    base::FilePath path = temp_dir_.GetPath();
    path = path.AppendASCII(name);

    auto manifest = base::Value::Dict()
                        .Set("name", name)
                        .Set("version", "1")
                        .Set("manifest_version", 2)
                        .Set("chrome_url_overrides", base::Value::Dict())
                        .Set("chrome_settings_overrides", base::Value::Dict());
    if (mode != api::incognito::IncognitoMode::kNone) {
      manifest.Set("incognito", api::incognito::ToString(mode));
    }

    std::u16string error;
    scoped_refptr<Extension> extension(
        Extension::Create(path, mojom::ManifestLocation::kInternal, manifest,
                          Extension::NO_FLAGS, &error));
    EXPECT_TRUE(extension) << error;
    return extension;
  }

  std::unique_ptr<SettingsOverrides> init_settings_info(
      const base::Value::Dict& manifest) {
    std::unique_ptr<SettingsOverrides> info = nullptr;
    const base::Value::Dict* dict =
        manifest.FindDict(manifest_keys::kSettingsOverride);
    if (dict != nullptr) {
      auto settings = ChromeSettingsOverrides::FromValue(*dict);
      if (!settings.has_value()) {
        ADD_FAILURE() << base::UTF16ToUTF8(settings.error());
        return info;
      }
      info = std::make_unique<SettingsOverrides>();
      if (settings->homepage) {
        info->homepage = CreateManifestURL(*settings->homepage);
      }
      info->search_engine = ParseSearchEngine(&settings.value());
      info->startup_pages = ParseStartupPage(*settings);
    }
    return info;
  }

  scoped_refptr<Extension> CreateTestExtensionWithManifest(
      const std::string& name,
      base::Value::Dict manifest = {}) {
    base::FilePath path = temp_dir_.GetPath();
    path = path.AppendASCII(name);

    std::u16string error;
    scoped_refptr<Extension> extension(
        Extension::Create(path, mojom::ManifestLocation::kInternal, manifest,
                          Extension::NO_FLAGS, &error));
    EXPECT_TRUE(extension) << error;
    if (!extension) {
      return extension;
    }

    std::unique_ptr<SettingsOverrides> info = init_settings_info(manifest);
    if (info) {
      extension->SetManifestData(manifest_keys::kSettingsOverride,
                                 std::move(info));
    }

    std::u16string str;
    ChromeUrlOverridesKeys manifest_keys;
    if (ChromeUrlOverridesKeys::ParseFromDictionary(
            extension->manifest()->available_values(), manifest_keys, str)) {
      auto url_overrides = std::make_unique<URLOverrides>();
      auto property_map =
          std::map<const char*,
                   std::reference_wrapper<const std::optional<std::string>>>{
              {UrlOverrideInfo::kNewtab,
               std::ref(manifest_keys.chrome_url_overrides.newtab)},
              {UrlOverrideInfo::kBookmarks,
               std::ref(manifest_keys.chrome_url_overrides.bookmarks)},
              {UrlOverrideInfo::kHistory,
               std::ref(manifest_keys.chrome_url_overrides.history)},
              {UrlOverrideInfo::kActivationmessage,
               std::ref(manifest_keys.chrome_url_overrides.activationmessage)},
              {UrlOverrideInfo::kKeyboard,
               std::ref(manifest_keys.chrome_url_overrides.keyboard)}};

      for (const auto& property : property_map) {
        if (!property.second.get()) {
          continue;
        }
        url_overrides->chrome_url_overrides_[property.first] =
            extension->GetResourceURL(*property.second.get());
      }

      extension->SetManifestData(ChromeUrlOverridesKeys::kChromeUrlOverrides,
                                 std::move(url_overrides));
    }
    return extension;
  }

  std::optional<GURL> CreateManifestURL(const std::string& url) {
    std::optional<GURL> manifest_url(std::in_place, url);
    if (!manifest_url->is_valid() || !manifest_url->SchemeIsHTTPOrHTTPS()) {
      return std::nullopt;
    }
    return manifest_url;
  }

  std::vector<GURL> ParseStartupPage(const ChromeSettingsOverrides& overrides) {
    std::vector<GURL> urls;
    if (!overrides.startup_pages) {
      return urls;
    }

    for (std::vector<std::string>::const_iterator i =
             overrides.startup_pages->begin();
         i != overrides.startup_pages->end(); ++i) {
      std::optional<GURL> manifest_url = CreateManifestURL(*i);
      if (manifest_url) {
        urls.push_back(std::move(*manifest_url));
      }
    }
    return urls;
  }

  std::optional<ChromeSettingsOverrides::SearchProvider> ParseSearchEngine(
      ChromeSettingsOverrides* overrides) {
    if (!overrides->search_provider) {
      return std::nullopt;
    }

    if (!CreateManifestURL(overrides->search_provider->search_url)) {
      return std::nullopt;
    }
    if (overrides->search_provider->favicon_url) {
      if (!CreateManifestURL(*overrides->search_provider->favicon_url)) {
        overrides->search_provider->favicon_url.reset();
      }
    }
    return std::move(*overrides->search_provider);
  }

  scoped_refptr<const Extension> CreateTestExtension() {
    return ExtensionBuilder("Test Extension")
        .SetManifestVersion(3)
        .SetID("test_extension_id")
        .AddAPIPermission("contextMenus")
        .AddAPIPermission("sidePanel")
        .SetAction(ActionInfo::Type::kBrowser)
        .Build();
  }

  ExtensionRegistryInfoManager* GetInfoManager() { return manager_.get(); }

  void SetListener(
      void (*onWebExtensionLoaded)(const WebExtensionInfo& loadedinfo),
      void (*onWebExtensionUnLoaded)(std::string extensionId),
      void (*onWebExtensionOpenUrlFun)(std::string url)) {
    callback_called_count_ = 0;
    ExtensionRegistryInfoManager::UnRegisterWebExtensionManagerListener();
    auto mock_listener = std::make_shared<NWebExtensionManagerCallBack>();
    EXPECT_NE(mock_listener, nullptr);
    mock_listener->OnWebExtensionLoaded = onWebExtensionLoaded;
    mock_listener->OnWebExtensionUnLoaded = onWebExtensionUnLoaded;
    mock_listener->OnWebExtensionOpenUrlFun = onWebExtensionOpenUrlFun;
    ExtensionRegistryInfoManager::RegisterWebExtensionManagerListener(
        mock_listener);
  }

  static void OnWebExtensionLoadedCallback(const WebExtensionInfo& info) {
    callback_called_count_++;
  }
  static void OnWebExtensionUnLoadedCallback(std::string id) {
    callback_called_count_++;
  }
  static void OnWebExtensionOpenUrlCallback(std::string url) {
    callback_called_count_++;
  }

  int GetCalledCount() { return callback_called_count_; }

 protected:
  void* callback_ = nullptr;

 private:
  std::unique_ptr<ExtensionRegistryInfoManager> manager_;
  std::unique_ptr<extensions::TestExtensionsClient> client_;
  base::ScopedTempDir temp_dir_;
  static int callback_called_count_;
};
int ExtensionRegistryInfoManagerTest::callback_called_count_ = 0;

#if BUILDFLAG(ARKWEB_NWEB_EX)
void SetIncognitoMode(Extension* extension,
                      api::incognito::IncognitoMode mode) {
  extension->SetManifestData(api::incognito::ManifestKeys::kIncognito,
                             std::make_unique<IncognitoInfo>(mode));
}

TEST_F(ExtensionRegistryInfoManagerTest, GetExtensionIncognitoMode) {
  // IsIncognitoAllowed true
  scoped_refptr<Extension> extension(CreateTestExtension(
      "test_extension_id", api::incognito::IncognitoMode::kNotAllowed));
  ASSERT_TRUE(extension);
  auto mode = GetInfoManager()->GetExtensionIncognitoMode(extension.get());
  EXPECT_EQ(mode, EXT_INCOGNITO_NOT_ALLOWED);

  // IsSpanningMode true
  scoped_refptr<Extension> extension2(CreateTestExtension(
      "test_extension_id", api::incognito::IncognitoMode::kSpanning));
  ASSERT_TRUE(extension2);
  mode = GetInfoManager()->GetExtensionIncognitoMode(extension2.get());
  EXPECT_EQ(mode, EXT_INCOGNITO_SPANNING);

  // IsSplitMode true
  scoped_refptr<Extension> extension3(CreateTestExtension(
      "test_extension_id", api::incognito::IncognitoMode::kSplit));
  ASSERT_TRUE(extension3);
  mode = GetInfoManager()->GetExtensionIncognitoMode(extension3.get());
  EXPECT_EQ(mode, EXT_INCOGNITO_SPLIT);

  // SetSplitMode false: Extensions and Chrome apps default to spanning mode.
  // See: extensions/common/manifest_handlers/incognito_info.cc
  // IncognitoHandler::Parse
  scoped_refptr<Extension> extension4(CreateTestExtension(
      "test_extension_id", api::incognito::IncognitoMode::kNone));
  ASSERT_TRUE(extension4);
  mode = GetInfoManager()->GetExtensionIncognitoMode(extension4.get());
  EXPECT_EQ(mode, EXT_INCOGNITO_SPANNING);
}
#endif

TEST_F(ExtensionRegistryInfoManagerTest, OnExtensionInstalled) {
  scoped_refptr<Extension> extension(CreateTestExtension("test_extension_id"));
  ASSERT_TRUE(extension);

#if BUILDFLAG(ARKWEB_NWEB_EX)
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionInstalledFun = [](const char* extensionId,
                                           int createFlag,
                                           int location) { g_expect_reason++; };
#endif

  // ShouldDisplayInExtensionSettings, no process
  ASSERT_NO_FATAL_FAILURE(GetInfoManager()->OnExtensionInstalled(
      browser_context(), extension.get(), false));

  g_expect_reason = 0;
  auto extension2 = CreateTestExtension();
  ASSERT_TRUE(extension2);
  ASSERT_NO_FATAL_FAILURE(GetInfoManager()->OnExtensionInstalled(
      browser_context(), extension2.get(), false));
#if BUILDFLAG(ARKWEB_NWEB_EX)
  EXPECT_EQ(g_expect_reason, 1);
#endif
}

TEST_F(ExtensionRegistryInfoManagerTest, OnExtensionUnloaded_DisableReason) {
  scoped_refptr<Extension> extension(CreateTestExtension("test_extension_id"));
  ASSERT_TRUE(extension);
#if BUILDFLAG(ARKWEB_NWEB_EX)
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionUnLoadedFunV2 =
      [](const char* extensionId, int reason) { g_expect_reason = reason; };
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionUnLoadedFun = [](const char* extensionId) {};
#endif

  // by reason
  std::map<UnloadedExtensionReason, int> test_data = {
      {UnloadedExtensionReason::UNDEFINED, 0},
      {UnloadedExtensionReason::DISABLE, 1},
      {UnloadedExtensionReason::UPDATE, 2},
      {UnloadedExtensionReason::UNINSTALL, 3},
      {UnloadedExtensionReason::TERMINATE, 0},
      {UnloadedExtensionReason::BLOCKLIST, 0},
      {UnloadedExtensionReason::LOCK_ALL, 0}};
  for (auto [reason, value] : test_data) {
    // check in callback
    GetInfoManager()->OnExtensionUnloaded(browser_context(), extension.get(),
                                          reason);
#if BUILDFLAG(ARKWEB_NWEB_EX)
    EXPECT_EQ(g_expect_reason, value);
#endif
  }
}

TEST_F(ExtensionRegistryInfoManagerTest, GetExtensionActionInfo_NoData) {
  scoped_refptr<const Extension> extension = CreateTestExtension();
  ASSERT_TRUE(extension);
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  EXPECT_NE(registry, nullptr);
  bool result = registry->AddEnabled(extension);
  ASSERT_TRUE(result);

  const int32_t tab_id = 100;
  WebExtensionActionInfo action_info =
      GetInfoManager()->GetExtensionActionInfo(*extension.get(), tab_id);
  EXPECT_TRUE(action_info.extensionId == extension->id());

  EXPECT_FALSE(action_info.isEnabled);
  EXPECT_FALSE(action_info.badgeBackgroundColor.has_value());
  EXPECT_FALSE(action_info.badgeTextColor.has_value());
  EXPECT_FALSE(action_info.badgeText.has_value());
  EXPECT_FALSE(action_info.popup.has_value());
  EXPECT_FALSE(action_info.title.has_value());

  WebExtensionActionInfoV2 action_info_v2 =
      GetInfoManager()->GetExtensionActionInfoV2(*extension.get(), tab_id);
  EXPECT_TRUE(action_info_v2.extensionId == extension->id());
  EXPECT_FALSE(action_info_v2.isEnabled);
  EXPECT_FALSE(action_info_v2.badgeBackgroundColor.has_value());
  EXPECT_FALSE(action_info_v2.badgeTextColor.has_value());
  EXPECT_FALSE(action_info_v2.badgeText.has_value());
  EXPECT_FALSE(action_info_v2.popup.has_value());
  EXPECT_FALSE(action_info_v2.title.has_value());
}

TEST_F(ExtensionRegistryInfoManagerTest, GetExtensionActionInfo_HasData) {
  scoped_refptr<const Extension> extension = CreateTestExtension();
  ASSERT_TRUE(extension);
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  EXPECT_NE(registry, nullptr);
  bool result = registry->AddEnabled(extension);
  ASSERT_TRUE(result);

  // create extension_action and set some values
  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context())
          ->GetExtensionAction(*extension.get());
  EXPECT_NE(extension_action, nullptr);
  const int32_t tab_id = 10;
  const std::string text = "test-001";
  const std::string title = "title-001";
  const GURL url("https://www.google.com");
  extension_action->SetBadgeBackgroundColor(tab_id, 0xFFFF0000u);
  extension_action->SetBadgeTextColor(tab_id, 0xFFFF0000u);
  extension_action->SetBadgeText(tab_id, text);
  extension_action->SetPopupUrl(tab_id, url);
  extension_action->SetTitle(tab_id, title);
  extension_action->SetIsVisible(tab_id, false);

  auto action_info =
      GetInfoManager()->GetExtensionActionInfo(*extension.get(), tab_id);
  EXPECT_TRUE(action_info.extensionId == extension->id());

  EXPECT_TRUE(action_info.isEnabled);
  ASSERT_TRUE(action_info.badgeBackgroundColor.has_value());
  EXPECT_EQ(action_info.badgeBackgroundColor.value()[0], 255);
  ASSERT_TRUE(action_info.badgeTextColor.has_value());
  EXPECT_EQ(action_info.badgeTextColor.value()[0], 255);
  EXPECT_EQ(action_info.popup, url.spec());
  EXPECT_EQ(action_info.title, title);
  EXPECT_EQ(action_info.badgeText, text);

  WebExtensionActionInfoV2 action_info_v2 =
      GetInfoManager()->GetExtensionActionInfoV2(*extension.get(), tab_id);
  EXPECT_TRUE(action_info_v2.extensionId == extension->id());
  EXPECT_TRUE(action_info_v2.isEnabled);
  EXPECT_TRUE(action_info_v2.badgeBackgroundColor.has_value());
  EXPECT_TRUE(action_info_v2.badgeTextColor.has_value());
  EXPECT_EQ(action_info_v2.popup, url.spec());
  EXPECT_EQ(action_info_v2.title, title);
  EXPECT_EQ(action_info_v2.badgeText, text);
}

TEST_F(ExtensionRegistryInfoManagerTest, GetExtensionManifestInfo) {
  const std::string extension_id = "test-extension-id";
  const std::string launch_url = "https://chrome.google.com/launch";
  scoped_refptr<Extension> extension(CreateTestExtension(extension_id));
  ASSERT_TRUE(extension);
  WebExtensionManifestInfo manifest;
  // invalid launch
  GetInfoManager()->GetExtensionManifestInfo(*extension.get(), manifest);
  EXPECT_FALSE(manifest.homepage_url.has_value());

  std::unique_ptr<ManifestURL> manifest_url = std::make_unique<ManifestURL>();
  ASSERT_TRUE(manifest_url);
  manifest_url->url_ = GURL(launch_url);
  extension->SetManifestData(manifest_keys::kHomepageURL,
                             std::move(manifest_url));
  GetInfoManager()->GetExtensionManifestInfo(*extension.get(), manifest);
  EXPECT_EQ(manifest.homepage_url, launch_url);
}

TEST_F(ExtensionRegistryInfoManagerTest, GetExtensionManifestInfo_UrlOverride) {
  std::vector<ManifestTestHelper> helpers{
      {[=](base::Value::Dict& manifest) {
         manifest.Set("chrome_url_overrides",
                      base::Value::Dict().Set("history", "history.html"));
       },
       [=](const WebExtensionManifestInfo& manifest) {
         ASSERT_TRUE(manifest.url_override.has_value());
         ASSERT_TRUE(manifest.url_override->history.has_value());
         EXPECT_TRUE(manifest.url_override->history.value().find(
                         "history.html") != std::string::npos);
       }},
      {[=](base::Value::Dict& manifest) {
         manifest.Set("chrome_url_overrides",
                      base::Value::Dict().Set("bookmarks", "bookmarks.html"));
       },
       [=](const WebExtensionManifestInfo& manifest) {
         ASSERT_TRUE(manifest.url_override.has_value());
         ASSERT_TRUE(manifest.url_override->bookmarks.has_value());
         EXPECT_TRUE(manifest.url_override->bookmarks.value().find(
                         "bookmarks.html") != std::string::npos);
       }},
      {[=](base::Value::Dict& manifest) {
         manifest.Set("chrome_url_overrides",
                      base::Value::Dict().Set("newtab", "newtab.html"));
       },
       [=](const WebExtensionManifestInfo& manifest) {
         ASSERT_TRUE(manifest.url_override.has_value());
         ASSERT_TRUE(manifest.url_override->newtab.has_value());
         EXPECT_TRUE(manifest.url_override->newtab.value().find(
                         "newtab.html") != std::string::npos);
       }}};

  const std::string extension_id = "test-extension-id";
  for (auto helper : helpers) {
    auto manifest_tmp =
        base::Value::Dict()
            .Set("name", extension_id)
            .Set("version", "1")
            .Set("manifest_version", 2)
            .Set("incognito", "split")
            .Set("chrome_settings_overrides", base::Value::Dict());
    helper.initManifest(manifest_tmp);
    scoped_refptr<Extension> extension(
        CreateTestExtensionWithManifest(extension_id, std::move(manifest_tmp)));
    ASSERT_TRUE(extension);
    WebExtensionManifestInfo manifest;
    GetInfoManager()->GetExtensionManifestInfo(*extension.get(), manifest);
    helper.checkManifest(manifest);
  }
}

TEST_F(ExtensionRegistryInfoManagerTest,
       GetExtensionManifestInfo_SettingsOverrides) {
  std::string homepage = "http://www.google.com/home";
  std::string startup = "http://startup.com/startup.html";
  std::string search_url = "http://google.com/search.html";
  std::string alternate_url = "http://wikipedia.org/wiki/Favicon";
  std::vector<ManifestTestHelper> helpers = {
      {[=](base::Value::Dict& settings_override) {
         settings_override.Set("homepage", homepage);
       },
       [=](const WebExtensionManifestInfo& manifest) {
         ASSERT_TRUE(manifest.settings_overrides.has_value());
         ASSERT_TRUE(manifest.settings_overrides->homepage.has_value());
         EXPECT_EQ(manifest.settings_overrides->homepage, homepage);
       }},
      {[=](base::Value::Dict& settings_override) {
         base::Value::List startup_pages;
         startup_pages.Append(startup);
         settings_override.Set("startup_pages", std::move(startup_pages));
       },
       [=](const WebExtensionManifestInfo& manifest) {
         ASSERT_TRUE(manifest.settings_overrides.has_value());
         ASSERT_EQ(manifest.settings_overrides->startup_pages.size(), 1u);
         ASSERT_EQ(manifest.settings_overrides->startup_pages[0], startup);
       }},
      {[=](base::Value::Dict& settings_override) {
         base::Value::Dict search_provider;
         search_provider.Set("search_url", search_url);
         search_provider.Set("name", "test");
         search_provider.Set("keyword", "lock");
         search_provider.Set("encoding", "UTF-8");
         search_provider.Set("is_default", true);
         search_provider.Set("favicon_url", alternate_url);
         settings_override.Set("search_provider", std::move(search_provider));
       },
       [=](const WebExtensionManifestInfo& manifest) {
         ASSERT_TRUE(manifest.settings_overrides.has_value());
         ASSERT_TRUE(manifest.settings_overrides->search_provider.has_value());
         ASSERT_EQ(manifest.settings_overrides->search_provider->search_url,
                   search_url);
       }},
      {[=](base::Value::Dict& settings_override) {
         auto alternate_urls = base::Value::List();
         alternate_urls.Append(alternate_url);
         base::Value::Dict search_provider;
         search_provider.Set("search_url", search_url);
         search_provider.Set("name", "test");
         search_provider.Set("keyword", "lock");
         search_provider.Set("encoding", "UTF-8");
         search_provider.Set("is_default", true);
         search_provider.Set("alternate_urls", std::move(alternate_urls));
         settings_override.Set("search_provider", std::move(search_provider));
       },
       [=](const WebExtensionManifestInfo& manifest) {
         ASSERT_TRUE(manifest.settings_overrides.has_value());
         ASSERT_TRUE(manifest.settings_overrides->search_provider.has_value());
         ASSERT_TRUE(manifest.settings_overrides->search_provider
                         ->alternate_urls.size() == 1u);
         ASSERT_EQ(
             manifest.settings_overrides->search_provider->alternate_urls[0],
             alternate_url);
       }}};
  const std::string extension_id = "test-extension-id";
  for (auto helper : helpers) {
    base::Value::Dict settings_override;
    helper.initManifest(settings_override);
    auto manifest_tmp =
        base::Value::Dict()
            .Set("name", extension_id)
            .Set("version", "1")
            .Set("manifest_version", 2)
            .Set("incognito", "split")
            .Set("chrome_settings_overrides", std::move(settings_override));
    scoped_refptr<Extension> extension(
        CreateTestExtensionWithManifest(extension_id, std::move(manifest_tmp)));
    ASSERT_TRUE(extension);
    WebExtensionManifestInfo manifest;
    GetInfoManager()->GetExtensionManifestInfo(*extension.get(), manifest);
    helper.checkManifest(manifest);
  }
}

TEST_F(ExtensionRegistryInfoManagerTest, GetExtensionManifestInfo_options_ui) {
  auto options_ui = base::Value::Dict()
                        .Set("page", "options.html")
                        .Set("chrome_style", true)
                        .Set("open_in_tab", true);
  const std::string extension_id = "test-extension-id";
  auto manifest_tmp = base::Value::Dict()
                          .Set("name", extension_id)
                          .Set("version", "1")
                          .Set("manifest_version", 2)
                          .Set("options_ui", std::move(options_ui));
  scoped_refptr<Extension> extension(
      CreateTestExtensionWithManifest(extension_id, std::move(manifest_tmp)));
  ASSERT_TRUE(extension);
  WebExtensionManifestInfo manifest;

  GetInfoManager()->GetExtensionManifestInfo(*extension.get(), manifest);
  ASSERT_TRUE(manifest.options_page.has_value());
  ASSERT_TRUE(manifest.options_page->open_in_tab);
  EXPECT_TRUE(manifest.options_page->options_page.find("options.html") !=
              std::string::npos);
}

TEST_F(ExtensionRegistryInfoManagerTest,
       GetExtensionManifestInfo_options_ui_NoTab) {
  auto options_ui = base::Value::Dict()
                        .Set("page", "options.html")
                        .Set("chrome_style", true)
                        .Set("open_in_tab", false);
  const std::string extension_id = "test-extension-id";
  auto manifest_tmp = base::Value::Dict()
                          .Set("name", extension_id)
                          .Set("version", "1")
                          .Set("manifest_version", 2)
                          .Set("options_page", "options.html")
                          .Set("options_ui", std::move(options_ui));
  scoped_refptr<Extension> extension(
      CreateTestExtensionWithManifest(extension_id, std::move(manifest_tmp)));
  ASSERT_TRUE(extension);
  WebExtensionManifestInfo manifest;

  GetInfoManager()->GetExtensionManifestInfo(*extension.get(), manifest);
  ASSERT_TRUE(manifest.options_page.has_value());
  ASSERT_FALSE(manifest.options_page->open_in_tab);
  EXPECT_TRUE(manifest.options_page->options_page.find(extension_id) ==
              std::string::npos);
}

TEST_F(ExtensionRegistryInfoManagerTest, GetExtensionManifestInfo_ShortName) {
  const std::string extension_id = "test-extension-id";
  const std::string name = "test-extension-name";
  const std::string short_name = "test-short-name";

  auto manifest_tmp = base::Value::Dict()
                          .Set("name", name)
                          .Set("version", "1")
                          .Set("manifest_version", 3)
                          .Set("short_name", short_name);
  scoped_refptr<Extension> extension(
      CreateTestExtensionWithManifest(extension_id, std::move(manifest_tmp)));
  ASSERT_TRUE(extension);
  WebExtensionManifestInfo manifest;

  GetInfoManager()->GetExtensionManifestInfo(*extension.get(), manifest);
  ASSERT_TRUE(manifest.short_name.has_value());
  EXPECT_EQ(manifest.short_name.value(), short_name);
  EXPECT_EQ(manifest.name, name);
}

TEST_F(ExtensionRegistryInfoManagerTest,
       ExtensionRegistryInfoManager_OnExtensionLoadedCallBack) {
  WebExtensionInfo extension_info;
  extension_info.extensionId = "test_extension_id";

  testing::internal::CaptureStderr();
  // clear env
  ExtensionRegistryInfoManager::UnRegisterWebExtensionManagerListener();
  // no register
  ExtensionRegistryInfoManager::OnExtensionLoadedCallBack(extension_info);
  std::string log_output = testing::internal::GetCapturedStderr();
  EXPECT_NE(log_output.find("No web extension manager listener"),
            std::string::npos);

  SetListener(nullptr, nullptr, nullptr);
  testing::internal::CaptureStderr();
  ExtensionRegistryInfoManager::OnExtensionLoadedCallBack(extension_info);
  log_output = testing::internal::GetCapturedStderr();
  EXPECT_NE(log_output.find("No OnWebExtensionLoaded listener"),
            std::string::npos);

  SetListener(OnWebExtensionLoadedCallback, OnWebExtensionUnLoadedCallback,
              OnWebExtensionOpenUrlCallback);
  ExtensionRegistryInfoManager::OnExtensionLoadedCallBack(extension_info);
  EXPECT_EQ(GetCalledCount(), 1);
}

TEST_F(ExtensionRegistryInfoManagerTest,
       ExtensionRegistryInfoManager_OnExtensionUnLoadedCallBack) {
  std::string eid = "test_extension_id";
  testing::internal::CaptureStderr();
  // clear env
  ExtensionRegistryInfoManager::UnRegisterWebExtensionManagerListener();
  // no register
  ExtensionRegistryInfoManager::OnExtensionUnLoadedCallBack(eid);
  std::string log_output = testing::internal::GetCapturedStderr();
  EXPECT_NE(log_output.find("No web extension manager listener"),
            std::string::npos);

  SetListener(nullptr, nullptr, nullptr);
  testing::internal::CaptureStderr();
  ExtensionRegistryInfoManager::OnExtensionUnLoadedCallBack(eid);
  log_output = testing::internal::GetCapturedStderr();
  EXPECT_NE(log_output.find("No OnWebExtensionUnLoaded listener"),
            std::string::npos);

  SetListener(OnWebExtensionLoadedCallback, OnWebExtensionUnLoadedCallback,
              OnWebExtensionOpenUrlCallback);
  ExtensionRegistryInfoManager::OnExtensionUnLoadedCallBack(eid);
  EXPECT_EQ(GetCalledCount(), 1);
}

TEST_F(ExtensionRegistryInfoManagerTest, GetTypeStr_Test) {
  EXPECT_STREQ(GetTypeStr(extensions::MenuItem::Type::NORMAL).c_str(),
               "normal");
  EXPECT_STREQ(GetTypeStr(extensions::MenuItem::Type::CHECKBOX).c_str(),
               "checkbox");
  EXPECT_STREQ(GetTypeStr(extensions::MenuItem::Type::RADIO).c_str(), "radio");
  EXPECT_STREQ(GetTypeStr(extensions::MenuItem::Type::SEPARATOR).c_str(),
               "separator");
}

TEST_F(ExtensionRegistryInfoManagerTest, GetContextStr_Test) {
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::ALL).c_str(),
               "all");
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::PAGE).c_str(),
               "page");
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::SELECTION).c_str(),
               "selection");
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::LINK).c_str(),
               "link");
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::EDITABLE).c_str(),
               "editable");
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::IMAGE).c_str(),
               "image");
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::VIDEO).c_str(),
               "video");
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::FRAME).c_str(),
               "frame");
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::LAUNCHER).c_str(),
               "launcher");
  EXPECT_STREQ(
      GetContextStr(extensions::MenuItem::Context::BROWSER_ACTION).c_str(),
      "browser_action");
  EXPECT_STREQ(
      GetContextStr(extensions::MenuItem::Context::PAGE_ACTION).c_str(),
      "page_action");
  EXPECT_STREQ(GetContextStr(extensions::MenuItem::Context::ACTION).c_str(),
               "action");
}

TEST_F(ExtensionRegistryInfoManagerTest, ContextListToStrVector_Test) {
  extensions::MenuItem::ContextList contextList0(
      extensions::MenuItem::Context::IMAGE);
  EXPECT_EQ(ContextListToStrVector(contextList0).size(), 1);
}

TEST_F(ExtensionRegistryInfoManagerTest, UnloadedReasonToState_Test) {
  EXPECT_EQ(UnloadedReasonToState(UnloadedExtensionReason::DISABLE),
            NWebExtensionState::DISABLED);
  EXPECT_EQ(UnloadedReasonToState(UnloadedExtensionReason::TERMINATE),
            NWebExtensionState::TERMINATED);
  EXPECT_EQ(UnloadedReasonToState(UnloadedExtensionReason::BLOCKLIST),
            NWebExtensionState::BLOCKLISTED);
  EXPECT_EQ(UnloadedReasonToState(UnloadedExtensionReason::UNINSTALL),
            NWebExtensionState::UNINSTALLED);

  EXPECT_EQ(UnloadedReasonToState(UnloadedExtensionReason::UNDEFINED),
            NWebExtensionState::DISABLED);
  EXPECT_EQ(UnloadedReasonToState(UnloadedExtensionReason::UPDATE),
            NWebExtensionState::DISABLED);
  EXPECT_EQ(UnloadedReasonToState(UnloadedExtensionReason::PROFILE_SHUTDOWN),
            NWebExtensionState::DISABLED);
  EXPECT_EQ(UnloadedReasonToState(UnloadedExtensionReason::LOCK_ALL),
            NWebExtensionState::DISABLED);
  EXPECT_EQ(
      UnloadedReasonToState(UnloadedExtensionReason::MIGRATED_TO_COMPONENT),
      NWebExtensionState::DISABLED);
}

static int g_state_changed = -1;
static int g_uninstalled_reason = -1;
static int g_state_changed_count = 0;

TEST_F(ExtensionRegistryInfoManagerTest,
       GetExtensionManifestInfo_OmniboxWithKeyword) {
  // Note: OmniboxHandler is not registered in TestExtensionsClient, so
  // OmniboxInfo::GetKeyword() returns empty and GetManifestOmnibox returns
  // nullopt. This test verifies that the omnibox parsing path is hit without
  // crashing, even when the handler is not registered.
  const std::string extension_id = "test-omnibox-extension";
  const std::string keyword = "testkw";
  auto manifest_tmp =
      base::Value::Dict()
          .Set("name", extension_id)
          .Set("version", "1")
          .Set("manifest_version", 2)
          .Set("omnibox", base::Value::Dict().Set("keyword", keyword));
  scoped_refptr<Extension> extension(
      CreateTestExtensionWithManifest(extension_id, std::move(manifest_tmp)));
  ASSERT_TRUE(extension);
  WebExtensionManifestInfo manifest;
  GetInfoManager()->GetExtensionManifestInfo(*extension.get(), manifest);
  // OmniboxHandler not registered in test context, so keyword is empty
  EXPECT_FALSE(manifest.omnibox.has_value());
}

TEST_F(ExtensionRegistryInfoManagerTest,
       GetExtensionManifestInfo_OmniboxNoKeyword) {
  const std::string extension_id = "test-no-omnibox-extension";
  auto manifest_tmp = base::Value::Dict()
                          .Set("name", extension_id)
                          .Set("version", "1")
                          .Set("manifest_version", 2);
  scoped_refptr<Extension> extension(
      CreateTestExtensionWithManifest(extension_id, std::move(manifest_tmp)));
  ASSERT_TRUE(extension);
  WebExtensionManifestInfo manifest;
  GetInfoManager()->GetExtensionManifestInfo(*extension.get(), manifest);
  EXPECT_FALSE(manifest.omnibox.has_value());
}

TEST_F(ExtensionRegistryInfoManagerTest, OnExtensionLoaded_Test) {
  scoped_refptr<Extension> extension(CreateTestExtension("test_extension_id"));
  ASSERT_TRUE(extension);
#if BUILDFLAG(ARKWEB_NWEB_EX)
  g_state_changed = -1;
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionStateChangedFun = [](const char* extensionId,
                                              int32_t state) {
    g_state_changed = static_cast<int>(state);
  };
#endif
  ASSERT_NO_FATAL_FAILURE(
      GetInfoManager()->OnExtensionLoaded(browser_context(), extension.get()));
#if BUILDFLAG(ARKWEB_NWEB_EX)
  EXPECT_EQ(g_state_changed, static_cast<int>(NWebExtensionState::ENABLED));
#endif
}

TEST_F(ExtensionRegistryInfoManagerTest, OnExtensionUninstalled_Test) {
  scoped_refptr<Extension> extension(CreateTestExtension("test_extension_id"));
  ASSERT_TRUE(extension);
#if BUILDFLAG(ARKWEB_NWEB_EX)
  g_state_changed = -1;
  g_uninstalled_reason = -1;
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionStateChangedFun = [](const char* extensionId,
                                              int32_t state) {
    g_state_changed = static_cast<int>(state);
  };
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionUninstalledFun = [](const char* extensionId,
                                             int reason) {
    g_uninstalled_reason = reason;
  };
#endif
  ASSERT_NO_FATAL_FAILURE(GetInfoManager()->OnExtensionUninstalled(
      browser_context(), extension.get(),
      UninstallReason::UNINSTALL_REASON_USER_INITIATED));
#if BUILDFLAG(ARKWEB_NWEB_EX)
  EXPECT_EQ(g_state_changed, static_cast<int>(NWebExtensionState::UNINSTALLED));
  EXPECT_EQ(g_uninstalled_reason,
            static_cast<int>(UninstallReason::UNINSTALL_REASON_USER_INITIATED));
#endif
}

TEST_F(ExtensionRegistryInfoManagerTest, Loaded_UnknownExtension) {
  ASSERT_NO_FATAL_FAILURE(GetInfoManager()->Loaded("nonexistent_extension_id"));
}

TEST_F(ExtensionRegistryInfoManagerTest, StartAndStopInitialLoad) {
  ExtensionRegistryInfoManager::StartInitialLoad();
  ExtensionRegistryInfoManager::StopInitialLoad();
  ASSERT_NO_FATAL_FAILURE(ExtensionRegistryInfoManager::StopInitialLoad());
}

TEST_F(ExtensionRegistryInfoManagerTest, DeleteExtensionActionInfoIcon_NoIcon) {
  WebExtensionActionInfo action_info;
  action_info.extensionId = "test_id";
  action_info.icon = std::nullopt;
  DeleteExtensionActionInfoIcon(action_info);
  EXPECT_FALSE(action_info.icon.has_value());
}

TEST_F(ExtensionRegistryInfoManagerTest,
       GetExtensionSidePanelInfo_NullService) {
  scoped_refptr<Extension> extension(CreateTestExtension("test_extension_id"));
  ASSERT_TRUE(extension);
  WebExtensionSidePanelInfo side_panel_info =
      GetInfoManager()->GetExtensionSidePanelInfo(*extension.get(),
                                                  std::nullopt);
  EXPECT_FALSE(side_panel_info.enable.has_value());
  EXPECT_FALSE(side_panel_info.path.has_value());
}

// GetAllExtensionContextMenus/V2 tests skipped: MenuManager::Get() crashes
// because Profile::IsRegularProfile() hits profile_metrics not initialized
// in ExtensionsTest context.

TEST_F(ExtensionRegistryInfoManagerTest, RegisterAndUnregisterListener) {
  auto listener = std::make_shared<NWebExtensionManagerCallBack>();
  EXPECT_NE(listener, nullptr);
  ExtensionRegistryInfoManager::RegisterWebExtensionManagerListener(listener);
  ExtensionRegistryInfoManager::UnRegisterWebExtensionManagerListener();
}

TEST_F(ExtensionRegistryInfoManagerTest,
       OnExtensionUnloaded_UninstallReason_StateNotNotified) {
  scoped_refptr<Extension> extension(CreateTestExtension("test_extension_id"));
  ASSERT_TRUE(extension);
#if BUILDFLAG(ARKWEB_NWEB_EX)
  g_state_changed_count = 0;
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionStateChangedFun =
      [](const char* extensionId, int32_t state) { g_state_changed_count++; };
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionUnLoadedFun = [](const char* extensionId) {};
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionUnLoadedFunV2 = [](const char* extensionId,
                                            int reason) {};
#endif
  GetInfoManager()->OnExtensionUnloaded(browser_context(), extension.get(),
                                        UnloadedExtensionReason::UNINSTALL);
#if BUILDFLAG(ARKWEB_NWEB_EX)
  EXPECT_EQ(g_state_changed_count, 0);
#endif
}

TEST_F(ExtensionRegistryInfoManagerTest,
       OnExtensionUnloaded_NonUninstallReason_StateNotified) {
  scoped_refptr<Extension> extension(CreateTestExtension("test_extension_id"));
  ASSERT_TRUE(extension);
#if BUILDFLAG(ARKWEB_NWEB_EX)
  g_state_changed = -1;
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionStateChangedFun = [](const char* extensionId,
                                              int32_t state) {
    g_state_changed = static_cast<int>(state);
  };
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionUnLoadedFun = [](const char* extensionId) {};
  static_cast<ArkWebExtensionManagerCallBack*>(callback_)
      ->OnArkWebExtensionUnLoadedFunV2 = [](const char* extensionId,
                                            int reason) {};
#endif
  GetInfoManager()->OnExtensionUnloaded(browser_context(), extension.get(),
                                        UnloadedExtensionReason::DISABLE);
#if BUILDFLAG(ARKWEB_NWEB_EX)
  EXPECT_EQ(g_state_changed, static_cast<int>(NWebExtensionState::DISABLED));
#endif
}
}  // namespace extensions
