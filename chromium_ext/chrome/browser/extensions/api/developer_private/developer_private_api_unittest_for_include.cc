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

TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateUpdateOptionalPermissions) {
  auto manifest =
      base::Value::Dict()
          .Set("name", "Optional Permissions Extension")
          .Set("version", "1.0")
          .Set("manifest_version", 3)
          .Set("optional_permissions", base::Value::List().Append("tabs"));

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetID(crx_file::id_util::GenerateId("optional_extension"))
          .Build();

  service()->AddExtension(extension.get());
  const ExtensionId& id = extension->id();
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());

  {
    base::Value::Dict update_dict;
    update_dict.Set("name", "tabs");
    update_dict.Set("enabled", false);

    base::Value::List args;
    args.Append(base::Value::Dict()
                    .Set("extensionId", id)
                    .Set("optionalPermission", std::move(update_dict)));

    auto function = base::MakeRefCounted<
        api::DeveloperPrivateUpdateExtensionConfigurationFunction>();

    EXPECT_TRUE(RunFunction(function, args));

    std::unique_ptr<const PermissionSet> blocked_set =
        extension_prefs->GetRuntimeUserBlockedPermissions(id);
    EXPECT_TRUE(blocked_set->HasAPIPermission(
        extensions::mojom::APIPermissionID::kTabs));
  }

  {
    base::Value::Dict update_dict;
    update_dict.Set("name", "tabs");
    update_dict.Set("enabled", true);

    base::Value::List args;
    args.Append(base::Value::Dict()
                    .Set("extensionId", id)
                    .Set("optionalPermission", std::move(update_dict)));

    auto function = base::MakeRefCounted<
        api::DeveloperPrivateUpdateExtensionConfigurationFunction>();

    EXPECT_TRUE(RunFunction(function, args));

    std::unique_ptr<const PermissionSet> blocked_set =
        extension_prefs->GetRuntimeUserBlockedPermissions(id);
    EXPECT_FALSE(blocked_set->HasAPIPermission(
        extensions::mojom::APIPermissionID::kTabs));
  }
}
 
TEST_F(DeveloperPrivateApiUnitTest,
       ArkWebInstallDroppedFileCrxSetsExternalWebstoreType) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 2
         })");
  base::FilePath crx_path = test_dir.Pack();
  base::AutoReset<bool> disable_ui =
      ExtensionInstallUI::disable_ui_for_tests(true);
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
 
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  DeveloperPrivateAPI::Get(profile())->SetDraggedPath(web_contents.get(),
                                                      crx_path);
#if BUILDFLAG(IS_OHOS)
  SetDraggedExtensionFileName(crx_path.value());
#endif
 
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateInstallDroppedFileFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
 
  TestExtensionRegistryObserver observer(registry());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), "[]", profile()))
      << function->GetError();
  scoped_refptr<const Extension> extension =
      observer.WaitForExtensionInstalled();
  ASSERT_TRUE(extension);
  EXPECT_EQ("foo", extension->name());
  EXPECT_EQ(kWebStoreTypeExternal, CustomData::GetStoreType(extension.get()));
}