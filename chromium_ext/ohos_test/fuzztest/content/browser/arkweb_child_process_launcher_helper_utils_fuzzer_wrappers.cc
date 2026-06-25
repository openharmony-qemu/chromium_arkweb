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

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/res_sched_client_adapter.h"

namespace OHOS::NWeb {
class SurfaceControlAdapter;
class SurfaceTransactionAdapter;
}  // namespace OHOS::NWeb

namespace {

constexpr pid_t kWrappedRenderPid = 12345;

template <typename T>
T& UnusedAdapterRef() {
  alignas(T) static unsigned char storage[sizeof(T)];
  return *reinterpret_cast<T*>(storage);
}

template <typename T>
T& UnusedIncompleteAdapterRef() {
  alignas(void*) static uintptr_t storage;
  return *reinterpret_cast<T*>(&storage);
}

class WrappedAafwkAppMgrClientAdapter
    : public OHOS::NWeb::AafwkAppMgrClientAdapter {
 public:
  int StartRenderProcess(const std::string& render_param,
                         int32_t ipc_fd,
                         int32_t shared_fd,
                         int32_t crash_fd,
                         pid_t& render_pid) override {
    (void)render_param;
    (void)ipc_fd;
    (void)shared_fd;
    (void)crash_fd;
    render_pid = kWrappedRenderPid;
    return 0;
  }

  void AttachRenderProcess(
      std::shared_ptr<OHOS::NWeb::AafwkRenderSchedulerHostAdapter> adapter)
      override {
    (void)adapter;
  }

  int GetRenderProcessTerminationStatus(pid_t render_pid,
                                        int& status) override {
    (void)render_pid;
    status = 0;
    return 0;
  }

  int StartChildProcess(const std::string& render_param,
                        int32_t ipc_fd,
                        int32_t shared_fd,
                        int32_t crash_fd,
                        pid_t& render_pid,
                        const std::string& process_type) override {
    (void)render_param;
    (void)ipc_fd;
    (void)shared_fd;
    (void)crash_fd;
    (void)process_type;
    render_pid = kWrappedRenderPid;
    return 0;
  }

  void SaveBrowserConnect(
      std::shared_ptr<OHOS::NWeb::AafwkBrowserHostAdapter> adapter) override {
    (void)adapter;
  }
};

class WrappedSystemPropertiesAdapter
    : public OHOS::NWeb::SystemPropertiesAdapter {
 public:
  bool GetResourceUseHapPathEnable() override { return false; }
  std::string GetDeviceInfoProductModel() override { return "fuzzer"; }
  std::string GetDeviceInfoBrand() override { return "fuzzer"; }
  int32_t GetDeviceInfoMajorVersion() override { return 0; }
  OHOS::NWeb::ProductDeviceType GetProductDeviceType() override {
    return OHOS::NWeb::ProductDeviceType::DEVICE_TYPE_UNKNOWN;
  }
  bool GetWebOptimizationValue() override { return false; }
  bool IsAdvancedSecurityMode() override { return false; }
  std::string GetUserAgentOSName() override { return "fuzzer"; }
  int32_t GetSoftwareMajorVersion() override { return 0; }
  int32_t GetSoftwareSeniorVersion() override { return 0; }
  std::string GetNetlogMode() override { return ""; }
  bool GetTraceDebugEnable() override { return false; }
  std::string GetSiteIsolationMode() override { return ""; }
  int32_t GetFlowBufMaxFd() override { return 0; }
  bool GetOOPGPUEnable() override { return false; }
  void SetOOPGPUDisable() override {}
  void AttachSysPropObserver(
      OHOS::NWeb::PropertiesKey key,
      OHOS::NWeb::SystemPropertiesObserver* observer) override {
    (void)key;
    (void)observer;
  }
  void DetachSysPropObserver(
      OHOS::NWeb::PropertiesKey key,
      OHOS::NWeb::SystemPropertiesObserver* observer) override {
    (void)key;
    (void)observer;
  }
  bool GetBoolParameter(const std::string& key, bool default_value) override {
    (void)key;
    return default_value;
  }
  std::vector<OHOS::NWeb::FrameRateSetting> GetLTPOConfig(
      const std::string& setting_name) override {
    (void)setting_name;
    return {};
  }
  std::string GetOOPGPUStatus() override { return ""; }
  bool IsLTPODynamicApp(const std::string& bundle_name) override {
    (void)bundle_name;
    return false;
  }
  int32_t GetLTPOStrategy() override { return 0; }
  std::string GetUserAgentBaseOSName() override { return "fuzzer"; }
  std::string GetUserAgentOSVersion() override { return "0"; }
  std::string GetVulkanStatus() override { return ""; }
  std::string GetCompatibleDeviceType() override { return ""; }
  std::string GetDeviceInfoApiVersion() override { return "0"; }
  std::string GetPRPPreloadMode() override { return ""; }
  std::string GetScrollVelocityScale() override { return ""; }
  std::string GetScrollFriction() override { return ""; }
  std::string GetBundleName() override { return "arkweb_fuzzer"; }
  std::string GetStringParameter(const std::string& key,
                                 const std::string& default_value) override {
    (void)key;
    return default_value;
  }
  int32_t GetInitialCongestionWindowSize() override { return 0; }
  int32_t GetIntParameter(const std::string& key,
                          int32_t default_value) override {
    (void)key;
    return default_value;
  }
  int32_t GetLoadUrlStrategy() override { return 0; }
  int32_t GetLTPOIntConfig(const std::string& config_name,
                           int32_t default_value) override {
    (void)config_name;
    return default_value;
  }
};

class WrappedHiTraceAdapter : public OHOS::NWeb::HiTraceAdapter {
 public:
  void StartTrace(const std::string& value, float limit = -1) override {
    (void)value;
    (void)limit;
  }
  void FinishTrace() override {}
  void StartAsyncTrace(const std::string& value,
                       int32_t task_id,
                       float limit = -1) override {
    (void)value;
    (void)task_id;
    (void)limit;
  }
  void FinishAsyncTrace(const std::string& value, int32_t task_id) override {
    (void)value;
    (void)task_id;
  }
  void CountTrace(const std::string& name, int64_t count) override {
    (void)name;
    (void)count;
  }
  bool IsHiTraceEnable() override { return false; }
  void StartOHOSTrace(const std::string& value, float limit = -1) override {
    (void)value;
    (void)limit;
  }
  void FinishOHOSTrace() override {}
  void CountOHOSTrace(const std::string& name, int64_t count) override {
    (void)name;
    (void)count;
  }
  bool IsACETraceEnable() override { return false; }
};

class WrappedHiSysEventAdapter : public OHOS::NWeb::HiSysEventAdapter {
 public:
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string, const std::string>& data)
      override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const std::string,
                             const std::string,
                             const std::string>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const int64_t,
                             const std::string,
                             const int,
                             const std::string,
                             const std::vector<uint16_t>,
                             const std::string,
                             const int>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int,
                             const std::string,
                             const int,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const uint32_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t,
                             const std::string,
                             const int64_t>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string, const uint32_t, const std::string, const uint64_t>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const int32_t,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const int32_t>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string, const int32_t>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string,
                             const std::string>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
  int Write(const std::string& event_name,
            EventType type,
            const std::tuple<const int32_t, const int32_t>& data) override {
    (void)event_name;
    (void)type;
    (void)data;
    return 0;
  }
};

class WrappedOhosAdapterHelper : public OHOS::NWeb::OhosAdapterHelper {
 public:
  std::unique_ptr<OHOS::NWeb::AafwkAppMgrClientAdapter> CreateAafwkAdapter()
      override {
    return std::make_unique<WrappedAafwkAppMgrClientAdapter>();
  }

  std::unique_ptr<OHOS::NWeb::PowerMgrClientAdapter>
  CreatePowerMgrClientAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::DisplayManagerAdapter> CreateDisplayMgrAdapter()
      override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::BatteryMgrClientAdapter>
  CreateBatteryClientAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::NetConnectAdapter> CreateNetConnectAdapter()
      override {
    return nullptr;
  }
  OHOS::NWeb::OhosWebDataBaseAdapter& GetOhosWebDataBaseAdapterInstance()
      override {
    return UnusedAdapterRef<OHOS::NWeb::OhosWebDataBaseAdapter>();
  }
  OHOS::NWeb::PasteBoardClientAdapter& GetPasteBoard() override {
    return UnusedAdapterRef<OHOS::NWeb::PasteBoardClientAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::AudioRendererAdapter>
  CreateAudioRendererAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::AudioCapturerAdapter>
  CreateAudioCapturerAdapter() override {
    return nullptr;
  }
  OHOS::NWeb::AudioSystemManagerAdapter& GetAudioSystemManager() override {
    return UnusedAdapterRef<OHOS::NWeb::AudioSystemManagerAdapter>();
  }
  OHOS::NWeb::OhosWebPermissionDataBaseAdapter&
  GetWebPermissionDataBaseInstance() override {
    return UnusedAdapterRef<OHOS::NWeb::OhosWebPermissionDataBaseAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::MMIAdapter> CreateMMIAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::SocPerfClientAdapter>
  CreateSocPerfClientAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::OhosResourceAdapter> GetResourceAdapter(
      const std::string& hap_path = "") override {
    (void)hap_path;
    return nullptr;
  }
  OHOS::NWeb::SystemPropertiesAdapter& GetSystemPropertiesInstance()
      override {
    static WrappedSystemPropertiesAdapter system_properties_adapter;
    return system_properties_adapter;
  }
  OHOS::NWeb::VSyncAdapter& GetVSyncAdapter() override {
    return UnusedAdapterRef<OHOS::NWeb::VSyncAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::OhosInitWebAdapter> GetInitWebAdapter()
      override {
    return nullptr;
  }
  OHOS::NWeb::KeystoreAdapter& GetKeystoreAdapterInstance() override {
    return UnusedAdapterRef<OHOS::NWeb::KeystoreAdapter>();
  }
  OHOS::NWeb::EnterpriseDeviceManagementAdapter&
  GetEnterpriseDeviceManagementInstance() override {
    return UnusedAdapterRef<OHOS::NWeb::EnterpriseDeviceManagementAdapter>();
  }
  OHOS::NWeb::DatashareAdapter& GetDatashareInstance() override {
    return UnusedAdapterRef<OHOS::NWeb::DatashareAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::IMFAdapter> CreateIMFAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::CertManagerAdapter> GetRootCertDataAdapter()
      override {
    return nullptr;
  }
  OHOS::NWeb::AccessTokenAdapter& GetAccessTokenAdapterInstance() override {
    return UnusedAdapterRef<OHOS::NWeb::AccessTokenAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::EventHandlerAdapter> GetEventHandlerAdapter()
      override {
    return nullptr;
  }
  OHOS::NWeb::PrintManagerAdapter& GetPrintManagerInstance() override {
    return UnusedAdapterRef<OHOS::NWeb::PrintManagerAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::IConsumerSurfaceAdapter>
  CreateConsumerSurfaceAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::PlayerAdapter> CreatePlayerAdapter() override {
    return nullptr;
  }
  OHOS::NWeb::WindowAdapter& GetWindowAdapterInstance() override {
    return UnusedAdapterRef<OHOS::NWeb::WindowAdapter>();
  }
  OHOS::NWeb::HiSysEventAdapter& GetHiSysEventAdapterInstance() override {
    static WrappedHiSysEventAdapter hisysevent_adapter;
    return hisysevent_adapter;
  }
  OHOS::NWeb::HiTraceAdapter& GetHiTraceAdapterInstance() override {
    static WrappedHiTraceAdapter hitrace_adapter;
    return hitrace_adapter;
  }
  OHOS::NWeb::NetProxyAdapter& GetNetProxyInstance() override {
    return UnusedAdapterRef<OHOS::NWeb::NetProxyAdapter>();
  }
  OHOS::NWeb::CameraManagerAdapter& GetCameraManagerAdapter() override {
    return UnusedAdapterRef<OHOS::NWeb::CameraManagerAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::ScreenCaptureAdapter>
  CreateScreenCaptureAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::DateTimeFormatAdapter>
  CreateDateTimeFormatAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::MediaCodecDecoderAdapter>
  CreateMediaCodecDecoderAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::NativeImageAdapter> CreateNativeImageAdapter()
      override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::MediaCodecAdapter>
  CreateMediaCodecEncoderAdapter() override {
    return nullptr;
  }
  OHOS::NWeb::MediaCodecListAdapter& GetMediaCodecListAdapter() override {
    return UnusedAdapterRef<OHOS::NWeb::MediaCodecListAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::FlowbufferAdapter> CreateFlowbufferAdapter()
      override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::MediaAVSessionAdapter>
  CreateMediaAVSessionAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::OhosImageDecoderAdapter>
  CreateOhosImageDecoderAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::SensorAdapter> CreateSensorAdapter() override {
    return nullptr;
  }
  void SetArkWebCoreHapPathOverride(const std::string& hap_path) override {
    (void)hap_path;
  }
  OHOS::NWeb::OhosNativeBufferAdapter& GetOhosNativeBufferAdapter()
      override {
    return UnusedAdapterRef<OHOS::NWeb::OhosNativeBufferAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::AudioCodecDecoderAdapter>
  CreateAudioCodecDecoderAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::DrmAdapter> CreateDrmAdapter() override {
    return nullptr;
  }
  OHOS::NWeb::OhosDrawingTextFontAdapter& GetOhosDrawingTextFontAdapter()
      override {
    return UnusedAdapterRef<OHOS::NWeb::OhosDrawingTextFontAdapter>();
  }
  OHOS::NWeb::OhosDrawingTextTypographyAdapter&
  GetOhosDrawingTextTypographyAdapter() override {
    return UnusedAdapterRef<OHOS::NWeb::OhosDrawingTextTypographyAdapter>();
  }
  std::unique_ptr<OHOS::NWeb::MigrationManagerAdapter>
  CreateMigrationMgrAdapter() override {
    return nullptr;
  }
  std::unique_ptr<OHOS::NWeb::ScreenlockManagerAdapter>
  CreateScreenlockManagerAdapter() override {
    return nullptr;
  }
};

}  // namespace

extern "C" OHOS::NWeb::OhosAdapterHelper&
__wrap__ZN4OHOS4NWeb17OhosAdapterHelper11GetInstanceEv() {
  static WrappedOhosAdapterHelper helper;
  return helper;
}

extern "C" base::Process
__wrap__ZN4base13LaunchProcessERKNS_11CommandLineERKNS_13LaunchOptionsE(
    const base::CommandLine& command_line,
    const base::LaunchOptions& options) {
  (void)command_line;
  (void)options;
  return base::Process();
}

extern "C" void __wrap__Z26ReportChildProcessInitFailbi(bool is_gpu, int err) {
  (void)is_gpu;
  (void)err;
}

extern "C" void
__wrap__Z28ReportRenderProcessTerminatebiRKNSt4__Cr12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEi(
    bool is_gpu,
    int pid,
    const std::string& reason,
    int error) {
  (void)is_gpu;
  (void)pid;
  (void)reason;
  (void)error;
}

extern "C" bool
__wrap__ZN4OHOS4NWeb21ResSchedClientAdapter15ReportKeyThreadENS0_21ResSchedStatusAdapterEiiNS0_19ResSchedRoleAdapterE(
    OHOS::NWeb::ResSchedStatusAdapter status_adapter,
    pid_t pid,
    pid_t tid,
    OHOS::NWeb::ResSchedRoleAdapter role_adapter) {
  (void)status_adapter;
  (void)pid;
  (void)tid;
  (void)role_adapter;
  return true;
}
