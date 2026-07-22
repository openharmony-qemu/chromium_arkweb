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
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif
#include "base/compiler_specific.h"
#include "base/test/task_environment.h"
#include "build/buildflag.h"

#include <netdb.h>

#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#include "arkweb/ohos_adapter_ndk/interfaces/net_connect_adapter.h"
#include "net/dns/arkweb_address_info_ext.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace OHOS::NWeb {

// ---------- Mock for NetConnectAdapter ----------
class MockNetConnectAdapter : public NetConnectAdapter {
 public:
  MOCK_METHOD(int32_t, RegisterNetConnCallback, (std::shared_ptr<NetConnCallback> cb), (override));
  MOCK_METHOD(int32_t, UnregisterNetConnCallback, (int32_t id), (override));
  MOCK_METHOD(int32_t, GetDefaultNetConnect, (NetConnectType& type, NetConnectSubtype& netConnectSubtype), (override));
  MOCK_METHOD(std::vector<std::string>, GetDnsServers, (), (override));
  MOCK_METHOD(std::vector<std::string>, GetDnsServersByNetId, (int32_t netId), (override));
  MOCK_METHOD(std::vector<std::string>, GetDnsServersForVpn, (), (override));
  MOCK_METHOD(void, RegisterVpnListener, (std::shared_ptr<VpnListener>), (override));
  MOCK_METHOD(void, UnRegisterVpnListener, (), (override));
  MOCK_METHOD(std::vector<std::string>, GetNetAddrListByNetId, (int32_t netId), (override));
  MOCK_METHOD(std::vector<std::string>, GetNetAddrListForVpn, (), (override));
  MOCK_METHOD(bool,
              NotifyNetworkServiceAuthRequired,
              (std::shared_ptr<NetGlobalHttpProxyRefreshCallback> callback),
              (override));
  MOCK_METHOD(int,
              GetAddrInfoForNetwork,
              (char* host, char* serv, struct addrinfo* hints, struct addrinfo** res, int32_t network),
              (override));
  MOCK_METHOD(int, FreeDnsResult, (struct addrinfo* res), (override));
};

// ---------- Mock for OhosAdapterHelper ----------
class MockOhosAdapterHelper : public OhosAdapterHelper {
 public:
  MOCK_METHOD(std::unique_ptr<AafwkAppMgrClientAdapter>, CreateAafwkAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<PowerMgrClientAdapter>, CreatePowerMgrClientAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<DisplayManagerAdapter>, CreateDisplayMgrAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<BatteryMgrClientAdapter>, CreateBatteryClientAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<NetConnectAdapter>, CreateNetConnectAdapter, (), (override));
  MOCK_METHOD(OhosWebDataBaseAdapter&, GetOhosWebDataBaseAdapterInstance, (), (override));
  MOCK_METHOD(PasteBoardClientAdapter&, GetPasteBoard, (), (override));
  MOCK_METHOD(std::unique_ptr<AudioRendererAdapter>, CreateAudioRendererAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<AudioCapturerAdapter>, CreateAudioCapturerAdapter, (), (override));
  MOCK_METHOD(AudioSystemManagerAdapter&, GetAudioSystemManager, (), (override));
  MOCK_METHOD(OhosWebPermissionDataBaseAdapter&, GetWebPermissionDataBaseInstance, (), (override));
  MOCK_METHOD(std::unique_ptr<MMIAdapter>, CreateMMIAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<SocPerfClientAdapter>, CreateSocPerfClientAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<OhosResourceAdapter>, GetResourceAdapter, (const std::string& hapPath), (override));
  MOCK_METHOD(SystemPropertiesAdapter&, GetSystemPropertiesInstance, (), (override));
  MOCK_METHOD(VSyncAdapter&, GetVSyncAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<OhosInitWebAdapter>, GetInitWebAdapter, (), (override));
  MOCK_METHOD(KeystoreAdapter&, GetKeystoreAdapterInstance, (), (override));
  MOCK_METHOD(EnterpriseDeviceManagementAdapter&, GetEnterpriseDeviceManagementInstance, (), (override));
  MOCK_METHOD(DatashareAdapter&, GetDatashareInstance, (), (override));
  MOCK_METHOD(std::unique_ptr<IMFAdapter>, CreateIMFAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<CertManagerAdapter>, GetRootCertDataAdapter, (), (override));
  MOCK_METHOD(AccessTokenAdapter&, GetAccessTokenAdapterInstance, (), (override));
  MOCK_METHOD(std::unique_ptr<EventHandlerAdapter>, GetEventHandlerAdapter, (), (override));
  MOCK_METHOD(PrintManagerAdapter&, GetPrintManagerInstance, (), (override));
  MOCK_METHOD(std::unique_ptr<IConsumerSurfaceAdapter>, CreateConsumerSurfaceAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<PlayerAdapter>, CreatePlayerAdapter, (), (override));
  MOCK_METHOD(WindowAdapter&, GetWindowAdapterInstance, (), (override));
  MOCK_METHOD(HiSysEventAdapter&, GetHiSysEventAdapterInstance, (), (override));
  MOCK_METHOD(HiTraceAdapter&, GetHiTraceAdapterInstance, (), (override));
  MOCK_METHOD(NetProxyAdapter&, GetNetProxyInstance, (), (override));
  MOCK_METHOD(CameraManagerAdapter&, GetCameraManagerAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<ScreenCaptureAdapter>, CreateScreenCaptureAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<DateTimeFormatAdapter>, CreateDateTimeFormatAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<MediaCodecDecoderAdapter>, CreateMediaCodecDecoderAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<NativeImageAdapter>, CreateNativeImageAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<MediaCodecAdapter>, CreateMediaCodecEncoderAdapter, (), (override));
  MOCK_METHOD(MediaCodecListAdapter&, GetMediaCodecListAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<FlowbufferAdapter>, CreateFlowbufferAdapter, (), (override));
  MOCK_METHOD(std::shared_ptr<MediaAVSessionAdapter>, CreateMediaAVSessionAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<OhosImageDecoderAdapter>, CreateOhosImageDecoderAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<SensorAdapter>, CreateSensorAdapter, (), (override));
  MOCK_METHOD(void, SetArkWebCoreHapPathOverride, (const std::string& hapPath), (override));
  MOCK_METHOD(OhosNativeBufferAdapter&, GetOhosNativeBufferAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<AudioCodecDecoderAdapter>, CreateAudioCodecDecoderAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<DrmAdapter>, CreateDrmAdapter, (), (override));
  MOCK_METHOD(OhosDrawingTextFontAdapter&, GetOhosDrawingTextFontAdapter, (), (override));
  MOCK_METHOD(OhosDrawingTextTypographyAdapter&, GetOhosDrawingTextTypographyAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<MigrationManagerAdapter>, CreateMigrationMgrAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<ScreenlockManagerAdapter>, CreateScreenlockManagerAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<CertManagerAdapter>, GetCertManagerAdapter, (), (override));
  MOCK_METHOD(std::unique_ptr<NetConfigAdapter>, GetNetConfigAdapter, (), (override));
  MOCK_METHOD(ColorPickerAdapter&, GetColorPickerAdapter, (), (override));
  MOCK_METHOD(HiAppeventAdapter&, GetHiAppeventAdapterInstance, (), (override));
  MOCK_METHOD(SurfaceControlAdapter&, GetSurfaceControlAdapter, (), (override));
  MOCK_METHOD(SurfaceTransactionAdapter&, GetSurfaceTransactionAdapter, (), (override));
};

} // namespace OHOS::NWeb

namespace net {

namespace ohos {

class ArkWebAddressInfoExtTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_helper_ = new OHOS::NWeb::MockOhosAdapterHelper();
    OHOS::NWeb::OhosAdapterHelper::SetInstance(mock_helper_);
  }
  void TearDown() override {
    OHOS::NWeb::OhosAdapterHelper::SetInstance(nullptr);
    delete mock_helper_;
    mock_helper_ = nullptr;
  }
  OHOS::NWeb::MockOhosAdapterHelper* mock_helper_ = nullptr;
};

// Test case 1: Invalid network handle
TEST_F(ArkWebAddressInfoExtTest, TestInvalidNetworkHandle) {
  struct addrinfo* result = nullptr;
  char host[] = "example.com";
  char serv[] = "http";
  struct addrinfo hints = {};

  int ret = ohos::GetAddrInfoForNetwork(host, serv, &hints, &result,
                                        handles::kInvalidNetworkHandle);
  EXPECT_EQ(ret, EAI_SYSTEM);
  EXPECT_EQ(errno, EINVAL);
}

// Test case 2: CreateNetConnectAdapter returns nullptr
TEST_F(ArkWebAddressInfoExtTest, TestCreateNetConnectAdapterNull) {
  EXPECT_CALL(*mock_helper_, CreateNetConnectAdapter())
      .WillOnce(Return(nullptr));

  struct addrinfo* result = nullptr;
  char host[] = "example.com";
  char serv[] = "http";
  struct addrinfo hints = {};

  int ret = ohos::GetAddrInfoForNetwork(host, serv, &hints, &result, 1);
  EXPECT_EQ(ret, EAI_SYSTEM);
  EXPECT_EQ(errno, EINVAL);
}

// Test case 3: GetAddrInfoForNetwork success
TEST_F(ArkWebAddressInfoExtTest, TestGetAddrInfoForNetworkSuccess) {
  auto mock_adapter = std::make_unique<OHOS::NWeb::MockNetConnectAdapter>();
  EXPECT_CALL(*mock_adapter, GetAddrInfoForNetwork(_, _, _, _, _))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_helper_, CreateNetConnectAdapter())
      .WillOnce(Return(std::move(mock_adapter)));

  struct addrinfo* result = nullptr;
  char host[] = "example.com";
  char serv[] = "http";
  struct addrinfo hints = {};

  int ret = ohos::GetAddrInfoForNetwork(host, serv, &hints, &result, 1);
  EXPECT_EQ(ret, 0);
}

// Test case 4: GetAddrInfoForNetwork returns error from adapter
TEST_F(ArkWebAddressInfoExtTest, TestGetAddrInfoForNetworkFail) {
  auto mock_adapter = std::make_unique<OHOS::NWeb::MockNetConnectAdapter>();
  EXPECT_CALL(*mock_adapter, GetAddrInfoForNetwork(_, _, _, _, _))
      .WillOnce(Return(EAI_NONAME));
  EXPECT_CALL(*mock_helper_, CreateNetConnectAdapter())
      .WillOnce(Return(std::move(mock_adapter)));

  struct addrinfo* result = nullptr;
  char host[] = "nonexistent.example.com";
  char serv[] = "http";
  struct addrinfo hints = {};

  int ret = ohos::GetAddrInfoForNetwork(host, serv, &hints, &result, 1);
  EXPECT_EQ(ret, EAI_NONAME);
}

// Test case 5: FreeDnsResult with CreateNetConnectAdapter returns nullptr
TEST_F(ArkWebAddressInfoExtTest, TestFreeDnsResultAdapterNull) {
  EXPECT_CALL(*mock_helper_, CreateNetConnectAdapter())
      .WillOnce(Return(nullptr));

  struct addrinfo res = {};
  int ret = ohos::FreeDnsResult(&res);
  EXPECT_EQ(ret, EAI_SYSTEM);
  EXPECT_EQ(errno, ENOSYS);
}

// Test case 6: FreeDnsResult success
TEST_F(ArkWebAddressInfoExtTest, TestFreeDnsResultSuccess) {
  auto mock_adapter = std::make_unique<OHOS::NWeb::MockNetConnectAdapter>();
  EXPECT_CALL(*mock_adapter, FreeDnsResult(_))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_helper_, CreateNetConnectAdapter())
      .WillOnce(Return(std::move(mock_adapter)));

  struct addrinfo res = {};
  int ret = ohos::FreeDnsResult(&res);
  EXPECT_EQ(ret, 0);
}

// Test case 7: FreeDnsResult returns error from adapter
TEST_F(ArkWebAddressInfoExtTest, TestFreeDnsResultFail) {
  auto mock_adapter = std::make_unique<OHOS::NWeb::MockNetConnectAdapter>();
  EXPECT_CALL(*mock_adapter, FreeDnsResult(_))
      .WillOnce(Return(EAI_FAIL));
  EXPECT_CALL(*mock_helper_, CreateNetConnectAdapter())
      .WillOnce(Return(std::move(mock_adapter)));

  struct addrinfo res = {};
  int ret = ohos::FreeDnsResult(&res);
  EXPECT_EQ(ret, EAI_FAIL);
}

}  // namespace ohos

}  // namespace net
