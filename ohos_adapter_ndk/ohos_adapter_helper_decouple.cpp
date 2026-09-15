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

#include "arkweb/ohos_adapter_ndk/aafwk_adapter/aafwk_app_mgr_client_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/access_token_adapter/access_token_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/audio_capturer_adapter/audio_capturer_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/audio_capturer_adapter/audio_renderer_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/audio_capturer_adapter/audio_system_manager_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/battery_mgr_adapter/battery_mgr_client_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/camera_adapter/include/camera_manager_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/cert_mgr_adapter/cert_mgr_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/date_time_format_adapter/date_time_format_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/display_manager_adapter/native_display_manager_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/drawing_text_adapter/ohos_drawing_text_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/enterprise_device_management_adapter/enterprise_device_management_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/event_handler_adapter/event_handler_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/flowbuffer_adapter/flowbuffer_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/graphic_adapter/native_image_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/graphic_adapter/native_window_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/graphic_adapter/vsync_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/graphic_adapter/window_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/hiviewdfx_adapter/hisysevent_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/inputmethodframework_adapter/imf_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/interfaces/ndk_ohos_adapter_helper.h"
#include "arkweb/ohos_adapter_ndk/keystore_adapter/keystore_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/media_adapter/audio_codec_decoder_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/media_adapter/drm_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/media_adapter/include/media_codec_list_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/media_adapter/include/video_encoder_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/media_adapter/media_codec_decoder_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/media_adapter/player_framework_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/media_avsession_adapter/media_avsession_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/migration_manager_adapter/migration_manager_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/multimodalinputnew_adapter/mmi_new_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/net_config_adapter/net_config_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/net_connect_adapter/net_connect_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/net_proxy_adapter_impl/net_proxy_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/ohos_image_adapter/ohos_image_decoder_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/ohos_init_web_adapter/ohos_init_web_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/ohos_native_buffer_adapter/ohos_native_buffer_adapter_impl.h"
#define DEVICE_TYPE_TV ARKWEB_RESMGR_DEVICE_TYPE_TV
#include "arkweb/ohos_adapter_ndk/ohos_resource_adapter/ohos_resource_adapter_impl.h"
#undef DEVICE_TYPE_TV
#include "arkweb/ohos_adapter_ndk/power_mgr_adapter/power_mgr_client_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/print_manager_adapter/print_manager_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/screen_capture_adapter/screen_capture_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/sensor_adapter/sensor_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/soc_perf_adapter/soc_perf_client_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/system_properties_adapter/system_properties_adapter_impl.h"
#include "color_picker_adapter/color_picker_adapter_impl.h"
#include "datashare_adapter/datashare_adapter_impl.h"
#include "distributeddatamgr_adapter/ohos_web_data_base_adapter_impl.h"
#include "distributeddatamgr_adapter/ohos_web_permission_data_base_adapter_impl.h"
#include "hiviewdfx_adapter/hiappevent_adapter_impl.h"
#include "hiviewdfx_adapter/hitrace_adapter_impl.h"
#include "pasteboard_adapter/include/pasteboard_client_adapter_impl.h"

namespace OHOS::NWeb {
#if BUILDFLAG(ARKWEB_TEST)
static OhosAdapterHelper* instance_ = nullptr;
#endif
OhosAdapterHelper& OhosAdapterHelper::GetInstance() {
#if BUILDFLAG(ARKWEB_TEST)
  if (instance_) {
    return *instance_;
  }
#endif
  static ArkWeb::NDKOhosAdapterHelper instance;
  return instance;
}

#if BUILDFLAG(ARKWEB_TEST)
void OhosAdapterHelper::SetInstance(OhosAdapterHelper* instance) {
  instance_ = instance;
}
#endif
}  // namespace OHOS::NWeb

namespace OHOS::ArkWeb {

namespace {
class DisabledSurfaceControlAdapter final : public NWeb::SurfaceControlAdapter {
 public:
  void* CreateSurfaceControlFromNativeWindow(void*, const char*) override {
    return nullptr;
  }

  void* Create(const char*) override { return nullptr; }
  void Release(void*) override {}
};

class DisabledSurfaceTransactionAdapter final
    : public NWeb::SurfaceTransactionAdapter {
 public:
  void* CreateSurfaceTransaction(void*) override { return nullptr; }
  void DeleteTransaction(void*) override {}
  void Commit(void*) override {}
  void SetOnComplete(void*,
                     void*,
                     NWeb::SurfaceControlOnCompleteCallback) override {}
  void Reparent(void*, void*, void*) override {}
  void SetVisibility(void*, void*, int8_t) override {}
  void SetZOrder(void*, void*, int32_t) override {}
  void SetBuffer(void*,
                 void*,
                 void*,
                 int,
                 void*,
                 NWeb::SurfaceControlSetBufferCallback) override {}
  void SetBounds(void*, void*, float, float, float, float) override {}
  void SetPivot(void*, void*, float, float) override {}
  void SetBufferTransform(void*, void*, int32_t) override {}
  void SetTranslate(void*, void*, float, float, float) override {}
  void SetDamageRegion(void*, void*, const void*, uint32_t) override {}
  void SetBufferAlpha(void*, void*, float) override {}
  void SetForegroundColor(void*, void*, float, float, float, float) override {}
  void SetBackgroundColor(void*, void*, float, float, float, float) override {}
  void SetBorderColor(void*, void*, float, float, float, float) override {}
  void SetBorderWidth(void*, void*, float, float, float, float) override {}
  void SetBorderStyle(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t)
      override {}
  void SetHardwareEnableHint(void*, void*, bool) override {}
  void SetName(void*, void*, const char*) override {}
  void SetFrameGravity(void*, void*, int32_t) override {}
  void SetSrcRect(void*, void*, float, float, float, float) override {}
  void SetDisplayRect(void*, void*, float, float, float, float) override {}
  void ClearBufferQueueCache(void*, void*, bool) override {}
};
}  // namespace

std::unique_ptr<NWeb::AafwkAppMgrClientAdapter>
NDKOhosAdapterHelper::CreateAafwkAdapter() {
  return std::make_unique<NWeb::AafwkAppMgrClientAdapterImpl>();
}

std::unique_ptr<NWeb::PowerMgrClientAdapter>
NDKOhosAdapterHelper::CreatePowerMgrClientAdapter() {
  return std::make_unique<NWeb::PowerMgrClientAdapterImpl>();
}

std::unique_ptr<NWeb::DisplayManagerAdapter>
NDKOhosAdapterHelper::CreateDisplayMgrAdapter() {
  return std::make_unique<NWeb::NativeDisplayManagerAdapterImpl>();
}

std::unique_ptr<NWeb::BatteryMgrClientAdapter>
NDKOhosAdapterHelper::CreateBatteryClientAdapter() {
  return std::make_unique<NWeb::BatteryMgrClientAdapterImpl>();
}

std::unique_ptr<NWeb::NetConnectAdapter>
NDKOhosAdapterHelper::CreateNetConnectAdapter() {
  return std::make_unique<NWeb::NetConnectAdapterImpl>();
}

NWeb::OhosWebDataBaseAdapter&
NDKOhosAdapterHelper::GetOhosWebDataBaseAdapterInstance(
    const std::string& userPath) {
  return NWeb::OhosWebDataBaseAdapterImpl::GetInstance(userPath);
}

NWeb::PasteBoardClientAdapter& NDKOhosAdapterHelper::GetPasteBoard() {
  static NWeb::PasteBoardClientAdapterImpl instance;
  return instance;
}

NWeb::ColorPickerAdapter& NDKOhosAdapterHelper::GetColorPickerAdapter() {
  static NWeb::ColorPickerAdapterImpl instance;
  return instance;
}

std::unique_ptr<NWeb::AudioRendererAdapter>
NDKOhosAdapterHelper::CreateAudioRendererAdapter() {
  return std::make_unique<NWeb::AudioRendererAdapterImpl>();
}

std::unique_ptr<NWeb::AudioCapturerAdapter>
NDKOhosAdapterHelper::CreateAudioCapturerAdapter() {
  return std::make_unique<NWeb::AudioCapturerAdapterImpl>();
}

NWeb::AudioSystemManagerAdapter& NDKOhosAdapterHelper::GetAudioSystemManager() {
  return NWeb::AudioSystemManagerAdapterImpl::GetInstance();
}

NWeb::OhosWebPermissionDataBaseAdapter&
NDKOhosAdapterHelper::GetWebPermissionDataBaseInstance(
    const std::string& userPath) {
  return NWeb::OhosWebPermissionDataBaseAdapterImpl::GetInstance(userPath);
}

std::unique_ptr<NWeb::MMIAdapter> NDKOhosAdapterHelper::CreateMMIAdapter() {
  return std::make_unique<NWeb::MMINewAdapterImpl>();
}

std::unique_ptr<NWeb::SocPerfClientAdapter>
NDKOhosAdapterHelper::CreateSocPerfClientAdapter() {
  return std::make_unique<NWeb::SocPerfClientAdapterImpl>();
}

std::unique_ptr<NWeb::OhosResourceAdapter>
NDKOhosAdapterHelper::GetResourceAdapter(const std::string& hapPath) {
  return std::make_unique<NWeb::OhosResourceAdapterImpl>(hapPath);
}

NWeb::SystemPropertiesAdapter&
NDKOhosAdapterHelper::GetSystemPropertiesInstance() {
  return NWeb::SystemPropertiesAdapterImpl::GetInstance();
}

NWeb::VSyncAdapter& NDKOhosAdapterHelper::GetVSyncAdapter() {
  return NWeb::VSyncAdapterNdkImpl::GetInstance();
}

std::unique_ptr<NWeb::OhosInitWebAdapter>
NDKOhosAdapterHelper::GetInitWebAdapter() {
  return std::make_unique<NWeb::OhosInitWebAdapterImpl>();
}

NWeb::KeystoreAdapter& NDKOhosAdapterHelper::GetKeystoreAdapterInstance() {
  return NWeb::KeystoreAdapterImpl::GetInstance();
}

NWeb::EnterpriseDeviceManagementAdapter&
NDKOhosAdapterHelper::GetEnterpriseDeviceManagementInstance() {
  return NWeb::EnterpriseDeviceManagementAdapterImpl::GetInstance();
}

NWeb::DatashareAdapter& NDKOhosAdapterHelper::GetDatashareInstance() {
  return NWeb::DatashareAdapterImpl::GetInstance();
}

std::unique_ptr<NWeb::IMFAdapter> NDKOhosAdapterHelper::CreateIMFAdapter() {
  return std::make_unique<NWeb::IMFAdapterImpl>();
}

std::unique_ptr<NWeb::CertManagerAdapter>
NDKOhosAdapterHelper::GetRootCertDataAdapter() {
  return std::make_unique<NWeb::CertManagerAdapterImpl>();
}

std::unique_ptr<NWeb::CertManagerAdapter>
NDKOhosAdapterHelper::GetCertManagerAdapter() {
  return std::make_unique<NWeb::CertManagerAdapterImpl>();
}

NWeb::AccessTokenAdapter&
NDKOhosAdapterHelper::GetAccessTokenAdapterInstance() {
  return NWeb::AccessTokenAdapterImpl::GetInstance();
}

std::unique_ptr<NWeb::EventHandlerAdapter>
NDKOhosAdapterHelper::GetEventHandlerAdapter() {
  return std::make_unique<NWeb::EventHandlerAdapterImpl>();
}

NWeb::PrintManagerAdapter& NDKOhosAdapterHelper::GetPrintManagerInstance() {
  return NWeb::PrintManagerAdapterImpl::GetInstance();
}

std::unique_ptr<NWeb::IConsumerSurfaceAdapter>
NDKOhosAdapterHelper::CreateConsumerSurfaceAdapter() {
  return std::make_unique<NWeb::ConsumerNativeAdapterImpl>();
}

std::unique_ptr<NWeb::PlayerAdapter>
NDKOhosAdapterHelper::CreatePlayerAdapter() {
  return std::make_unique<NWeb::PlayerAdapterImpl>();
}

NWeb::WindowAdapter& NDKOhosAdapterHelper::GetWindowAdapterInstance() {
  return NWeb::WindowAdapterNdkImpl::GetInstance();
}

NWeb::HiSysEventAdapter& NDKOhosAdapterHelper::GetHiSysEventAdapterInstance() {
  return NWeb::HiSysEventAdapterImpl::GetInstance();
}

NWeb::HiTraceAdapter& NDKOhosAdapterHelper::GetHiTraceAdapterInstance() {
  return NWeb::HiTraceAdapterImpl::GetInstance();
}

NWeb::NetProxyAdapter& NDKOhosAdapterHelper::GetNetProxyInstance() {
  return NWeb::NetProxyAdapterImpl::GetInstance();
}

NWeb::CameraManagerAdapter& NDKOhosAdapterHelper::GetCameraManagerAdapter() {
  return NWeb::CameraManagerAdapterImpl::GetInstance();
}

std::unique_ptr<NWeb::ScreenCaptureAdapter>
NDKOhosAdapterHelper::CreateScreenCaptureAdapter() {
  return std::make_unique<NWeb::ScreenCaptureAdapterImpl>();
}

std::unique_ptr<NWeb::DateTimeFormatAdapter>
NDKOhosAdapterHelper::CreateDateTimeFormatAdapter() {
  return std::make_unique<NWeb::DateTimeFormatAdapterImpl>();
}

std::unique_ptr<NWeb::MediaCodecDecoderAdapter>
NDKOhosAdapterHelper::CreateMediaCodecDecoderAdapter() {
  return std::make_unique<NWeb::MediaCodecDecoderAdapterImpl>();
}

std::unique_ptr<NWeb::NativeImageAdapter>
NDKOhosAdapterHelper::CreateNativeImageAdapter() {
  return std::make_unique<NWeb::NativeImageAdapterImpl>();
}

std::unique_ptr<NWeb::MediaCodecAdapter>
NDKOhosAdapterHelper::CreateMediaCodecEncoderAdapter() {
  return std::make_unique<NWeb::VideoEncoderAdapterImpl>();
}

NWeb::MediaCodecListAdapter& NDKOhosAdapterHelper::GetMediaCodecListAdapter() {
  return NWeb::MediaCodecListAdapterImpl::GetInstance();
}

std::unique_ptr<NWeb::FlowbufferAdapter>
NDKOhosAdapterHelper::CreateFlowbufferAdapter() {
  return std::make_unique<NWeb::FlowbufferAdapterImpl>();
}

std::shared_ptr<NWeb::MediaAVSessionAdapter>
NDKOhosAdapterHelper::CreateMediaAVSessionAdapter() {
  return std::make_shared<NWeb::MediaAVSessionAdapterImpl>();
}

std::unique_ptr<NWeb::OhosImageDecoderAdapter>
NDKOhosAdapterHelper::CreateOhosImageDecoderAdapter() {
  return std::make_unique<NWeb::OhosImageDecoderAdapterImpl>();
}

std::unique_ptr<NWeb::SensorAdapter>
NDKOhosAdapterHelper::CreateSensorAdapter() {
  return std::make_unique<NWeb::SensorAdapterImpl>();
}

void NDKOhosAdapterHelper::SetApplicationResourceManager(void* mgr) {
  NWeb::OhosResourceAdapterImpl::SetApplicationResourceManager(mgr);
}

void NDKOhosAdapterHelper::SetArkWebCoreHapPathOverride(
    const std::string& hapPath) {}

NWeb::OhosNativeBufferAdapter&
NDKOhosAdapterHelper::GetOhosNativeBufferAdapter() {
  return NWeb::OhosNativeBufferAdapterImpl::GetInstance();
}

std::unique_ptr<NWeb::AudioCodecDecoderAdapter>
NDKOhosAdapterHelper::CreateAudioCodecDecoderAdapter() {
  return std::make_unique<NWeb::AudioCodecDecoderAdapterImpl>();
}

std::unique_ptr<NWeb::DrmAdapter> NDKOhosAdapterHelper::CreateDrmAdapter() {
  return std::make_unique<NWeb::DrmAdapterImpl>();
}

NWeb::OhosDrawingTextFontAdapter&
NDKOhosAdapterHelper::GetOhosDrawingTextFontAdapter() {
  return NWeb::OhosDrawingTextFontAdapterImpl::GetInstance();
}

NWeb::OhosDrawingTextTypographyAdapter&
NDKOhosAdapterHelper::GetOhosDrawingTextTypographyAdapter() {
  return NWeb::OhosDrawingTextTypographyAdapterImpl::GetInstance();
}

std::unique_ptr<NWeb::MigrationManagerAdapter>
NDKOhosAdapterHelper::CreateMigrationMgrAdapter() {
  return std::make_unique<NWeb::MigrationManagerAdapterImpl>();
}

std::unique_ptr<NWeb::ScreenlockManagerAdapter>
NDKOhosAdapterHelper::CreateScreenlockManagerAdapter() {
  return nullptr;
}

std::unique_ptr<NWeb::NetConfigAdapter>
NDKOhosAdapterHelper::GetNetConfigAdapter() {
  return std::make_unique<NWeb::NetConfigAdapterImpl>();
}

std::unique_ptr<NWeb::BackgroundTaskAdapter>
NDKOhosAdapterHelper::CreateBackgroundTaskAdapter() {
  return nullptr;
}

NWeb::HiAppeventAdapter& NDKOhosAdapterHelper::GetHiAppeventAdapterInstance() {
  return NWeb::HiAppeventAdapterImpl::GetInstance();
}

NWeb::SurfaceControlAdapter& NDKOhosAdapterHelper::GetSurfaceControlAdapter() {
  static DisabledSurfaceControlAdapter instance;
  return instance;
}

NWeb::SurfaceTransactionAdapter&
NDKOhosAdapterHelper::GetSurfaceTransactionAdapter() {
  static DisabledSurfaceTransactionAdapter instance;
  return instance;
}
}  // namespace OHOS::ArkWeb
