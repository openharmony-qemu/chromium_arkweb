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

#include <memory>

#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#include "ohos_sdk/openharmony/native/llvm/bin/../include/libcxx-ohos/include/c++/v1/__ranges/lazy_split_view.h"
#define private public
#include "arkweb/chromium_ext/base/ohos/sys_info_utils_ext.cc"
#undef private
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_ohos_adapter_helper.h"
#include "arkweb/ohos_adapter_ndk/interfaces/mock/mock_system_properties_adapter.h"
#include "base/test/scoped_command_line.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace base {
namespace ohos {

class SystemPropertiesTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {
    if (adapter_override_installed_) {
      OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
    }
    *properties_ = original_properties_;
    SetPageScale(original_page_scale_);
  }

  void InstallMockAdapter() {
    OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
    auto& original_hitrace = OHOS::NWeb::OhosAdapterHelper::GetInstance()
                                 .GetHiTraceAdapterInstance();
    ON_CALL(adapter_helper_, GetSystemPropertiesInstance())
        .WillByDefault(testing::ReturnRef(system_properties_adapter_));
    ON_CALL(adapter_helper_, GetHiTraceAdapterInstance())
        .WillByDefault(testing::ReturnRef(original_hitrace));
    OHOS::NWeb::OhosAdapterHelper::SetInstance(&adapter_helper_);
    adapter_override_installed_ = true;
  }

  testing::NiceMock<OHOS::NWeb::MockOhosAdapterHelper> adapter_helper_;
  testing::NiceMock<OHOS::NWeb::MockSystemPropertiesAdapter>
      system_properties_adapter_;
  SystemProperties* properties_ = SystemProperties::Instance();
  SystemProperties original_properties_ = *properties_;
  bool original_page_scale_ = IsPageScale();
  bool adapter_override_installed_ = false;
};

TEST_F(SystemPropertiesTest, ApplicationApiVersion001) {
  EXPECT_EQ(ApplicationApiVersion(), -1);
}

TEST_F(SystemPropertiesTest, ApplicationApiVersion002) {
  base::test::ScopedCommandLine scoped_cl;
  scoped_cl.GetProcessCommandLine()->RemoveSwitch(switches::kOhosAppApiVersion);
  EXPECT_EQ(ApplicationApiVersion(), -1);
}

TEST_F(SystemPropertiesTest, ApplicationApiVersion003) {
  base::test::ScopedCommandLine scoped_cl;
  scoped_cl.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kOhosAppApiVersion, "");

  EXPECT_EQ(ApplicationApiVersion(), -1);
}

TEST_F(SystemPropertiesTest, ApplicationApiVersion004) {
  base::test::ScopedCommandLine scoped_cl;
  scoped_cl.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kOhosAppApiVersion, "99");

  EXPECT_EQ(ApplicationApiVersion(), 99);
}

TEST_F(SystemPropertiesTest, ApplicationApiVersion005) {
  base::test::ScopedCommandLine scoped_cl;
  scoped_cl.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kOhosAppApiVersion, "105");

  EXPECT_EQ(ApplicationApiVersion(), 5);
}

TEST_F(SystemPropertiesTest, ApplicationApiVersion006) {
  base::test::ScopedCommandLine scoped_cl;
  scoped_cl.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kOhosAppApiVersion, "0");

  EXPECT_EQ(ApplicationApiVersion(), 0);
}

TEST_F(SystemPropertiesTest, is_compatible_mode001) {
  EXPECT_FALSE(properties_->is_compatible_mode());
}

TEST_F(SystemPropertiesTest, is_compatible_mode002) {
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_2IN1;
  properties_->compatible_device_type_ = "abc";
  EXPECT_FALSE(properties_->is_compatible_mode());
}

TEST_F(SystemPropertiesTest, is_compatible_mode003) {
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_2IN1;
  properties_->compatible_device_type_ = "Tablet";
  EXPECT_TRUE(properties_->is_compatible_mode());
}

TEST_F(SystemPropertiesTest, is_compatible_mode004) {
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_2IN1;
  properties_->compatible_device_type_ = "Phone";
  EXPECT_TRUE(properties_->is_compatible_mode());
}

TEST_F(SystemPropertiesTest, is_compatible_mode005) {
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_TV;
  properties_->compatible_device_type_ = "Phone";
  EXPECT_FALSE(properties_->is_compatible_mode());
}

TEST_F(SystemPropertiesTest, is_pc_mode001) {
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_2IN1;
  properties_->compatible_device_type_ = "Phone";
  EXPECT_TRUE(properties_->is_compatible_mode());
}

TEST_F(SystemPropertiesTest, is_pc_mode002) {
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_MOBILE;
  properties_->compatible_device_type_ = "Phone";
  EXPECT_FALSE(properties_->is_compatible_mode());
}

TEST_F(SystemPropertiesTest, is_pc_mode003) {
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_TABLET;
  properties_->compatible_device_type_ = "Phone";
  EXPECT_FALSE(properties_->is_compatible_mode());
}

TEST_F(SystemPropertiesTest, is_pc_mode004) {
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_TV;
  properties_->compatible_device_type_ = "Phone";
  EXPECT_FALSE(properties_->is_compatible_mode());
}

TEST_F(SystemPropertiesTest, IsDelegatedCompositingEnabledOhos_EnabledViaTestProperty) {
  InstallMockAdapter();
  EXPECT_CALL(system_properties_adapter_,
              GetBoolParameter("web.delegatedCompositing.enable", false))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(system_properties_adapter_,
              GetBoolParameter("web.delegatedCompositingTest.enable", false))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(IsDelegatedCompositingEnabledOhos());
}

TEST_F(SystemPropertiesTest, IsDelegatedCompositingEnabledOhos_Default) {
  // Cached from the first call (true), so returns true without re-querying.
  EXPECT_TRUE(IsDelegatedCompositingEnabledOhos());
}

TEST_F(SystemPropertiesTest, DevicePredicatesCoverTrueAndFalse) {
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_MOBILE;
  EXPECT_TRUE(IsMobileDevice());
  EXPECT_FALSE(IsTabletDevice());
  EXPECT_FALSE(IsPcDevice());
  EXPECT_FALSE(IsWearableDevice());

  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_TABLET;
  EXPECT_FALSE(IsMobileDevice());
  EXPECT_TRUE(IsTabletDevice());

  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_2IN1;
  EXPECT_TRUE(IsPcDevice());

  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_WEARABLE;
  EXPECT_TRUE(IsWearableDevice());

  properties_->product_model_ = "emulator";
  EXPECT_TRUE(IsEmulator());
  properties_->product_model_ = "physical-device";
  EXPECT_FALSE(IsEmulator());
}

TEST_F(SystemPropertiesTest, NotifyIsPcModeReturnsFalseWhenUnsupported) {
  InstallMockAdapter();
  EXPECT_CALL(
      system_properties_adapter_,
      GetStringParameter("const.window.support_window_pcmode_switch", "false"))
      .WillOnce(testing::Return("false"));
  EXPECT_CALL(system_properties_adapter_,
              GetStringParameter("persist.sceneboard.ispcmode", "false"))
      .Times(0);

  EXPECT_FALSE(properties_->NotifyIsPcMode());
}

TEST_F(SystemPropertiesTest, NotifyIsPcModeReturnsPersistedFalse) {
  InstallMockAdapter();
  testing::InSequence sequence;
  EXPECT_CALL(
      system_properties_adapter_,
      GetStringParameter("const.window.support_window_pcmode_switch", "false"))
      .WillOnce(testing::Return("true"));
  EXPECT_CALL(system_properties_adapter_,
              GetStringParameter("persist.sceneboard.ispcmode", "false"))
      .WillOnce(testing::Return("false"));

  EXPECT_FALSE(properties_->NotifyIsPcMode());
}

TEST_F(SystemPropertiesTest, NotifyIsPcModeReturnsPersistedTrue) {
  InstallMockAdapter();
  testing::InSequence sequence;
  EXPECT_CALL(
      system_properties_adapter_,
      GetStringParameter("const.window.support_window_pcmode_switch", "false"))
      .WillOnce(testing::Return("true"));
  EXPECT_CALL(system_properties_adapter_,
              GetStringParameter("persist.sceneboard.ispcmode", "false"))
      .WillOnce(testing::Return("true"));

  EXPECT_TRUE(properties_->NotifyIsPcMode());
}

TEST_F(SystemPropertiesTest, ConstructorCachesAdapterValues) {
  InstallMockAdapter();
  ON_CALL(system_properties_adapter_, GetSoftwareMajorVersion())
      .WillByDefault(testing::Return(6));
  ON_CALL(system_properties_adapter_, GetSoftwareSeniorVersion())
      .WillByDefault(testing::Return(3));
  ON_CALL(system_properties_adapter_, GetProductDeviceType())
      .WillByDefault(testing::Return(ProductDeviceType::DEVICE_TYPE_TABLET));
  ON_CALL(system_properties_adapter_, GetUserAgentOSName())
      .WillByDefault(testing::Return("OpenHarmonyTest"));
  ON_CALL(system_properties_adapter_, GetUserAgentOSVersion())
      .WillByDefault(testing::Return("6.0"));
  ON_CALL(system_properties_adapter_, GetUserAgentBaseOSName())
      .WillByDefault(testing::Return("BaseOSTest"));
  ON_CALL(system_properties_adapter_, GetDeviceInfoProductModel())
      .WillByDefault(testing::Return("ProductModelTest"));
  ON_CALL(system_properties_adapter_, GetDeviceInfoApiVersion())
      .WillByDefault(testing::Return("18"));
  ON_CALL(system_properties_adapter_, GetCompatibleDeviceType())
      .WillByDefault(testing::Return("Tablet"));
  ON_CALL(system_properties_adapter_,
          GetStringParameter(testing::_, testing::_))
      .WillByDefault(testing::Invoke(
          [](const std::string& key, const std::string& default_value) {
            if (key == "const.product.model") {
              return std::string("UaModelTest");
            }
            if (key == "const.product.os.dist.version") {
              return std::string("DistVersionTest");
            }
            if (key == "const.window.support_window_pcmode_switch" ||
                key == "persist.sceneboard.ispcmode") {
              return std::string("true");
            }
            return default_value;
          }));

  SystemProperties local_properties;

  EXPECT_EQ(local_properties.major_version(), 6);
  EXPECT_EQ(local_properties.senior_version(), 3);
  EXPECT_EQ(local_properties.device_type(),
            ProductDeviceType::DEVICE_TYPE_TABLET);
  EXPECT_EQ(local_properties.os_name(), "OpenHarmonyTest");
  EXPECT_EQ(local_properties.os_version(), "6.0");
  EXPECT_EQ(local_properties.base_os_name(), "BaseOSTest");
  EXPECT_EQ(local_properties.product_model(), "ProductModelTest");
  EXPECT_EQ(local_properties.api_version(), "18");
  EXPECT_EQ(local_properties.compatible_device_type(), "Tablet");
  EXPECT_EQ(local_properties.ua_model(), "UaModelTest");
  EXPECT_EQ(local_properties.os_dist_version(), "DistVersionTest");
  EXPECT_TRUE(local_properties.is_pc_mode());
}

TEST_F(SystemPropertiesTest, MutableGlobalStateAccessors) {
  const float original_pixel_ratio = GetPixelRatio();
  const bool original_page_scale = IsPageScale();

  SetPixelRatio(3.5f);
  SetPageScale(true);

  EXPECT_FLOAT_EQ(GetPixelRatio(), 3.5f);
  EXPECT_TRUE(IsPageScale());

  SetPixelRatio(original_pixel_ratio);
  SetPageScale(original_page_scale);
}

TEST_F(SystemPropertiesTest, CachedPropertyAccessors) {
  const int32_t original_major_version = properties_->major_version_;
  const int32_t original_senior_version = properties_->senior_version_;
  const std::string original_os_name = properties_->os_name_;
  const std::string original_os_version = properties_->os_version_;
  const std::string original_base_os_name = properties_->base_os_name_;
  const std::string original_product_model = properties_->product_model_;
  const std::string original_compatible_device_type =
      properties_->compatible_device_type_;
  const bool original_is_pc_mode = properties_->is_pc_mode_;
  properties_->major_version_ = 4;
  properties_->senior_version_ = 2;
  properties_->os_name_ = "OpenHarmonyTest";
  properties_->os_version_ = "1.2.3";
  properties_->base_os_name_ = "BaseTest";
  properties_->product_model_ = "ModelTest";
  properties_->compatible_device_type_ = "Tablet";
  properties_->is_pc_mode_ = true;

  EXPECT_EQ(MajorVersion(), 4);
  EXPECT_EQ(SeniorVersion(), 2);
  EXPECT_EQ(OsName(), "OpenHarmonyTest");
  EXPECT_EQ(OsVersion(), "1.2.3");
  EXPECT_EQ(BaseOsName(), "BaseTest");
  EXPECT_EQ(ProductModel(), "ModelTest");
  EXPECT_EQ(CompatibleDeviceType(), "Tablet");
  EXPECT_EQ(ComponentName(), "ArkWeb");
  EXPECT_TRUE(IsPcMode());

  properties_->major_version_ = original_major_version;
  properties_->senior_version_ = original_senior_version;
  properties_->os_name_ = original_os_name;
  properties_->os_version_ = original_os_version;
  properties_->base_os_name_ = original_base_os_name;
  properties_->product_model_ = original_product_model;
  properties_->compatible_device_type_ = original_compatible_device_type;
  properties_->is_pc_mode_ = original_is_pc_mode;
}

TEST_F(SystemPropertiesTest,
       DelegatedCompositingUsesEnabledPropertiesAndCommandLineFallback) {
  // IsDelegatedCompositingEnabledOhos() is cached as true from the first call,
  // so it returns true without re-querying the adapter.
  EXPECT_TRUE(IsDelegatedCompositingEnabledOhos());
}

#if BUILDFLAG(ARKWEB_USERAGENT)
TEST_F(SystemPropertiesTest, DeviceTypeString) {
  const ProductDeviceType original_device_type = properties_->device_type_;
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_MOBILE;
  EXPECT_EQ(GetDeviceTypeString(), "Mobile");
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_WEARABLE;
  EXPECT_EQ(GetDeviceTypeString(), "Watch");
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_2IN1;
  EXPECT_EQ(GetDeviceTypeString(), "Desktop");
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_TABLET;
  EXPECT_EQ(GetDeviceTypeString(), "Tablet");
  properties_->device_type_ = ProductDeviceType::DEVICE_TYPE_TV;
  EXPECT_TRUE(GetDeviceTypeString().empty());
  properties_->device_type_ = original_device_type;
}

TEST_F(SystemPropertiesTest, GetProductModelAndOSDistVersion) {
  const std::string original_ua_model = properties_->ua_model_;
  const std::string original_os_dist_version = properties_->os_dist_version_;
  properties_->ua_model_ = "TestUaModel";
  properties_->os_dist_version_ = "5.0.0";
  EXPECT_EQ(GetProductModel(), "TestUaModel");
  EXPECT_EQ(GetOSDistVersion(), "5.0.0");
  properties_->ua_model_ = original_ua_model;
  properties_->os_dist_version_ = original_os_dist_version;
}
#endif

TEST_F(SystemPropertiesTest, IsOhosLowEndDevice_LowEndByDeviceEndType) {
  InstallMockAdapter();
  EXPECT_CALL(system_properties_adapter_,
              GetIntParameter("web.device_end.type", 0))
      .WillOnce(testing::Return(1));
  EXPECT_TRUE(IsOhosLowEndDevice());
}

TEST_F(SystemPropertiesTest, IsOhosLowEndDevice_CachedAfterFirstCall) {
  // IsOhosLowEndDevice result is cached; second call returns the cached value
  // without re-reading the adapter.
  EXPECT_TRUE(IsOhosLowEndDevice());
}

TEST_F(SystemPropertiesTest, IsAiHdrEnabledOhos_Enabled) {
  // IsDelegatedCompositingEnabledOhos() is cached as true from earlier tests,
  // so IsAiHdrEnabledOhos() proceeds to query web.aihdr.enable.
  InstallMockAdapter();
  EXPECT_CALL(system_properties_adapter_,
              GetBoolParameter("web.aihdr.enable", false))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(IsAiHdrEnabledOhos());
}

TEST_F(SystemPropertiesTest, IsAiHdrEnabledOhos_CachedAfterFirstCall) {
  // IsAiHdrEnabledOhos result is cached; returns same value without re-querying.
  EXPECT_TRUE(IsAiHdrEnabledOhos());
}
}  // namespace ohos
}  // namespace base
