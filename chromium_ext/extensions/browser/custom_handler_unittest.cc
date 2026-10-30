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

#include <gtest/gtest.h>

#include <string>

#include "arkweb/build/features/features.h"
#include "base/values.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"

// clang-format off
#define private public
#include "extensions/browser/custom_handler.h"
#undef private
// clang-format on

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

namespace extensions {

// ---------------------------------------------------------------------------
// Unit tests for custom_handler.cc - the ArkWeb CustomData / CustomHandler
// implementation that persists a webstore_type in the extension manifest's
// "custom_data" key.
//
// Branch coverage:
//   CustomData::GetStoreType:
//     [A] null extension            -> kWebStoreTypeDefault
//     [B] no manifest data          -> kWebStoreTypeDefault
//     [C] manifest data present     -> stored store_type_
//   CustomData::SetStoreType:
//     [D] null manifest             -> early return
//     [E] valid manifest            -> writes "custom_data"/"webstore_type"
//   ShouldBypassStoreCheckForWebstoreType:
//     [F] external store type       -> true
//     [G] other store type          -> false
//   CanBeUpdated / IsFromHuaweiWebStore:
//     [H] HW store type         -> true
//     [I] other store type          -> false
//   CustomHandler::Parse:
//     [J] manifest without custom_data -> stores default
//     [K] manifest with custom_data    -> stores parsed int
//   CustomHandler::Keys:
//     [L] returns {kCustomData}
// ---------------------------------------------------------------------------

class CustomHandlerTest : public ::testing::Test {
 protected:
  // Builds an extension whose manifest contains a "custom_data" dict with the
  // given webstore_type (or no "custom_data" key at all when |include_data|
  // is false).
  scoped_refptr<const Extension> BuildExtensionWithCustomData(
      std::optional<int> webstore_type) {
    auto builder = ExtensionBuilder("CustomDataTestExtension");
    if (webstore_type.has_value()) {
      base::Value::Dict custom_data;
      custom_data.Set("webstore_type", webstore_type.value());
      builder.SetManifestKey(manifest_keys::kCustomData,
                             base::Value(std::move(custom_data)));
    }
    return builder.Build();
  }

  // Runs the manifest handler so the parsed CustomData lands in the
  // extension's manifest data. GetStoreType /
  // ShouldBypassStoreCheckForWebstoreType / CanBeUpdated / IsFromHuaweiWebStore
  // read from GetManifestData, which is only populated by
  // CustomHandler::Parse() (ExtensionBuilder does not run manifest handlers).
  void ParseCustomData(scoped_refptr<const Extension> extension) {
    auto* mutable_extension = const_cast<Extension*>(extension.get());
    std::u16string error;
    CustomHandler handler;
    EXPECT_TRUE(handler.Parse(mutable_extension, &error));
  }
};

// [A] A null extension returns the default store type.
TEST_F(CustomHandlerTest, GetStoreType_NullExtension_ReturnsDefault) {
  EXPECT_EQ(CustomData::GetStoreType(nullptr), kWebStoreTypeDefault);
}

// [B] An extension without manifest data returns the default store type.
TEST_F(CustomHandlerTest, GetStoreType_NoManifestData_ReturnsDefault) {
  scoped_refptr<const Extension> extension =
      BuildExtensionWithCustomData(std::nullopt);
  ASSERT_NE(extension, nullptr);
  EXPECT_EQ(CustomData::GetStoreType(extension.get()), kWebStoreTypeDefault);
}

// [C] An extension with manifest data returns the stored store type.
TEST_F(CustomHandlerTest, GetStoreType_WithManifestData_ReturnsStored) {
  scoped_refptr<const Extension> extension =
      BuildExtensionWithCustomData(kWebStoreTypeHuawei);
  ASSERT_NE(extension, nullptr);
  ParseCustomData(extension);
  EXPECT_EQ(CustomData::GetStoreType(extension.get()), kWebStoreTypeHuawei);
}

// [D] SetStoreType with a null manifest is a no-op.
TEST_F(CustomHandlerTest, SetStoreType_NullManifest_NoOp) {
  EXPECT_NO_FATAL_FAILURE(
      CustomData::SetStoreType(kWebStoreTypeHuawei, nullptr));
}

// [E] SetStoreType writes the webstore_type into the manifest dict.
TEST_F(CustomHandlerTest, SetStoreType_ValidManifest_WritesDict) {
  base::Value::Dict manifest;
  CustomData::SetStoreType(kWebStoreTypeExternal, &manifest);

  const base::Value::Dict* custom_data =
      manifest.FindDict(manifest_keys::kCustomData);
  ASSERT_NE(custom_data, nullptr);
  EXPECT_EQ(custom_data->FindInt("webstore_type"), kWebStoreTypeExternal);
}

// [F] External store type bypasses the store check.
TEST_F(CustomHandlerTest,
       ShouldBypassStoreCheckForWebstoreType_External_ReturnsTrue) {
  scoped_refptr<const Extension> extension =
      BuildExtensionWithCustomData(kWebStoreTypeExternal);
  ASSERT_NE(extension, nullptr);
  ParseCustomData(extension);
  EXPECT_TRUE(
      CustomData::ShouldBypassStoreCheckForWebstoreType(extension.get()));
}

// [G] A non-external store type does not bypass the store check.
TEST_F(CustomHandlerTest,
       ShouldBypassStoreCheckForWebstoreType_Default_ReturnsFalse) {
  scoped_refptr<const Extension> extension =
      BuildExtensionWithCustomData(kWebStoreTypeDefault);
  ASSERT_NE(extension, nullptr);
  EXPECT_FALSE(
      CustomData::ShouldBypassStoreCheckForWebstoreType(extension.get()));
}

// [H] HW store type can be updated and is from the HW web store.
TEST_F(CustomHandlerTest, CanBeUpdated_And_IsFromHuaweiWebStore) {
  scoped_refptr<const Extension> hw =
      BuildExtensionWithCustomData(kWebStoreTypeHuawei);
  ASSERT_NE(hw, nullptr);
  ParseCustomData(hw);
  EXPECT_TRUE(CustomData::CanBeUpdated(hw.get()));
  EXPECT_TRUE(CustomData::IsFromHuaweiWebStore(hw.get()));
}

// [I] A default store type is not updatable and not from the HW store.
TEST_F(CustomHandlerTest, CanBeUpdated_And_IsFromHuaweiWebStore_Default) {
  scoped_refptr<const Extension> extension =
      BuildExtensionWithCustomData(kWebStoreTypeDefault);
  ASSERT_NE(extension, nullptr);
  EXPECT_FALSE(CustomData::CanBeUpdated(extension.get()));
  EXPECT_FALSE(CustomData::IsFromHuaweiWebStore(extension.get()));
}

// [J] Parse stores the default store type when custom_data is absent.
TEST_F(CustomHandlerTest, Parse_NoCustomData_StoresDefault) {
  scoped_refptr<const Extension> extension =
      BuildExtensionWithCustomData(std::nullopt);
  ASSERT_NE(extension, nullptr);

  // Parse operates on a mutable Extension.
  auto* mutable_extension = const_cast<Extension*>(extension.get());
  std::u16string error;
  CustomHandler handler;
  EXPECT_TRUE(handler.Parse(mutable_extension, &error));
  EXPECT_EQ(CustomData::GetStoreType(mutable_extension), kWebStoreTypeDefault);
}

// [K] Parse stores the parsed webstore_type when custom_data is present.
TEST_F(CustomHandlerTest, Parse_WithCustomData_StoresParsedType) {
  scoped_refptr<const Extension> extension =
      BuildExtensionWithCustomData(kWebStoreTypeExternal);
  ASSERT_NE(extension, nullptr);

  auto* mutable_extension = const_cast<Extension*>(extension.get());
  std::u16string error;
  CustomHandler handler;
  EXPECT_TRUE(handler.Parse(mutable_extension, &error));
  EXPECT_EQ(CustomData::GetStoreType(mutable_extension), kWebStoreTypeExternal);
}

// [L] Keys returns the custom_data manifest key.
TEST_F(CustomHandlerTest, Keys_ReturnsCustomDataKey) {
  CustomHandler handler;
  base::span<const char* const> keys = handler.Keys();
  ASSERT_EQ(keys.size(), 1u);
  EXPECT_EQ(std::string(keys[0]), manifest_keys::kCustomData);
}

}  // namespace extensions

#endif  // BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
