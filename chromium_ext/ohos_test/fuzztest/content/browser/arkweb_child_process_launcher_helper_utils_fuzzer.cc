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

#include <fuzzer/FuzzedDataProvider.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <variant>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/test/task_environment.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/posix_file_descriptor_info.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

#define private public
#define protected public
#include "content/browser/child_process_launcher_helper.h"
#undef protected
#undef private

#include "arkweb/chromium_ext/content/browser/arkweb_child_process_launcher_helper_utils.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/aafwk_app_mgr_client_adapter.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/res_sched_client_adapter.h"

namespace content {
namespace internal {

class FuzzArkwebChildProcessLauncherHelperUtilsFriend {
 public:
  static base::TerminationStatus GetProcessStatusByExitCode(int status,
                                                            bool known_dead) {
    return content::internal::ArkwebChildProcessLauncherHelperUtils::
        GetProcessStatusByExitCode(status, known_dead);
  }

  static std::string GetExitReasonByTerminationStatus(
      base::TerminationStatus status) {
    return content::internal::ArkwebChildProcessLauncherHelperUtils::
        GetExitReasonByTerminationStatus(status);
  }

  static void RenderProcessExitedInfo(pid_t pid,
                                      int exit_status,
                                      bool known_dead) {
    content::internal::ArkwebChildProcessLauncherHelperUtils::
        RenderProcessExitedInfo(pid, exit_status, known_dead);
  }

  static bool TerminateProcessByAppMgr(const base::Process& process) {
    return content::internal::ArkwebChildProcessLauncherHelperUtils::
        TerminateProcessByAppMgr(process);
  }

  static void GetTerminationInfoArkweb(
      content::internal::ArkwebChildProcessLauncherHelperUtils& helper,
      const base::Process& process,
      bool known_dead,
      ChildProcessTerminationInfo& info) {
    helper.GetTerminationInfoArkweb(process, known_dead, info);
  }

  static void LaunchChildProcess(
      content::internal::ArkwebChildProcessLauncherHelperUtils& helper,
      base::CommandLine* command_line,
      const base::LaunchOptions* options,
      content::internal::ChildProcessLauncherHelper::Process& process) {
    helper.LaunchChildProcess(command_line, options, process);
  }
};
}  // namespace internal
}  // namespace content

#if BUILDFLAG(ARKWEB_RENDER_PROCESS_STARTUP)
namespace {

constexpr size_t kMaxActions = 64;
constexpr int kRenderNormalKilled = 0x10000000;
constexpr int kSharedFd = 10;
constexpr int kIpcFd = 11;
constexpr int kCrashSignalFd = 12;
constexpr pid_t kMockRenderPid = 12345;
constexpr std::array<int, 12> kInterestingSignals = {
    SIGABRT, SIGBUS,  SIGFPE, SIGILL,  SIGSEGV, SIGTRAP,
    SIGSYS,  SIGKILL, SIGINT, SIGTERM, SIGHUP,  SIGQUIT};
constexpr std::array<int, 7> kCrashSignals = {
    SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGTRAP, SIGSYS};
constexpr std::array<int, 3> kKilledSignals = {SIGKILL, SIGINT, SIGTERM};

constexpr std::array<int, 16> kInterestingRawStatuses = {
    0,
    1,
    2,
    0x7f,
    0x80,
    0xff,
    0xffff,
    -1,
    kRenderNormalKilled,
    std::numeric_limits<int>::min(),
    std::numeric_limits<int>::max(),
    (SIGSEGV | 0x80),
    (SIGABRT | 0x80),
    (SIGKILL | 0x80),
    (1 << 8),
    (255 << 8),
};

constexpr base::TerminationStatus kTerminationStatusEdges[] = {
    base::TERMINATION_STATUS_NORMAL_TERMINATION,
    base::TERMINATION_STATUS_ABNORMAL_TERMINATION,
    base::TERMINATION_STATUS_PROCESS_WAS_KILLED,
    base::TERMINATION_STATUS_PROCESS_CRASHED,
    base::TERMINATION_STATUS_STILL_RUNNING,
    base::TERMINATION_STATUS_LAUNCH_FAILED,
    base::TERMINATION_STATUS_OOM,
    base::TERMINATION_STATUS_EVICTED_FOR_MEMORY,
    base::TERMINATION_STATUS_MAX_ENUM,
    static_cast<base::TerminationStatus>(-1),
    static_cast<base::TerminationStatus>(
        static_cast<int>(base::TERMINATION_STATUS_MAX_ENUM) + 1),
    static_cast<base::TerminationStatus>(std::numeric_limits<int>::max()),
};

class MockAafwkAppMgrClientAdapter
    : public OHOS::NWeb::AafwkAppMgrClientAdapter {
 public:
  MockAafwkAppMgrClientAdapter(int start_child_process_result,
                               int termination_status_result,
                               int termination_status)
      : start_child_process_result_(start_child_process_result),
        termination_status_result_(termination_status_result),
        termination_status_(termination_status) {}

  int StartRenderProcess(const std::string& render_param,
                         int32_t ipc_fd,
                         int32_t shared_fd,
                         int32_t crash_fd,
                         pid_t& render_pid) override {
    render_pid = kMockRenderPid;
    return start_child_process_result_;
  }

  void AttachRenderProcess(
      std::shared_ptr<OHOS::NWeb::AafwkRenderSchedulerHostAdapter> adapter)
      override {}

  int GetRenderProcessTerminationStatus(pid_t render_pid,
                                        int& status) override {
    status = termination_status_;
    return termination_status_result_;
  }

  int StartChildProcess(const std::string& render_param,
                        int32_t ipc_fd,
                        int32_t shared_fd,
                        int32_t crash_fd,
                        pid_t& render_pid,
                        const std::string& process_type) override {
    last_render_param_ = render_param;
    last_ipc_fd_ = ipc_fd;
    last_shared_fd_ = shared_fd;
    last_crash_fd_ = crash_fd;
    last_process_type_ = process_type;
    render_pid = kMockRenderPid;
    return start_child_process_result_;
  }

  void SaveBrowserConnect(
      std::shared_ptr<OHOS::NWeb::AafwkBrowserHostAdapter> adapter) override {}

 private:
  int start_child_process_result_;
  int termination_status_result_;
  int termination_status_;
  std::string last_render_param_;
  std::string last_process_type_;
  int32_t last_ipc_fd_ = -1;
  int32_t last_shared_fd_ = -1;
  int32_t last_crash_fd_ = -1;
};

int MakeExitedStatus(uint8_t exit_code) {
  return static_cast<int>(exit_code) << 8;
}

int MakeSignaledStatus(int signal_number, bool dumped_core) {
  return (signal_number & 0x7f) | (dumped_core ? 0x80 : 0);
}

int ConsumeWaitStatus(FuzzedDataProvider& provider) {
  enum class StatusKind : uint8_t {
    kExited,
    kInterestingSignal,
    kRawSignal,
    kStoppedLike,
    kInterestingRaw,
    kRaw,
    kMaxValue = kRaw,
  };

  switch (provider.ConsumeEnum<StatusKind>()) {
    case StatusKind::kExited:
      return MakeExitedStatus(provider.ConsumeIntegral<uint8_t>());
    case StatusKind::kInterestingSignal:
      return MakeSignaledStatus(provider.PickValueInArray(kInterestingSignals),
                                provider.ConsumeBool());
    case StatusKind::kRawSignal:
      return MakeSignaledStatus(provider.ConsumeIntegralInRange<int>(0, 255),
                                provider.ConsumeBool());
    case StatusKind::kStoppedLike:
      return (provider.ConsumeIntegralInRange<int>(0, 255) << 8) | 0x7f;
    case StatusKind::kInterestingRaw:
      return provider.PickValueInArray(kInterestingRawStatuses);
    case StatusKind::kRaw:
      return provider.ConsumeIntegral<int>();
  }
  return 0;
}

base::TerminationStatus ConsumeTerminationStatus(FuzzedDataProvider& provider) {
  if (provider.ConsumeBool()) {
    return provider.PickValueInArray(kTerminationStatusEdges);
  }

  return static_cast<base::TerminationStatus>(
      provider.ConsumeIntegralInRange<int>(
          -16, static_cast<int>(base::TERMINATION_STATUS_MAX_ENUM) + 16));
}

void EnsureCommandLineInitialized() {
  if (!base::CommandLine::InitializedForCurrentProcess()) {
    const char* argv[] = {"arkweb_child_process_launcher_helper_utils_fuzzer"};
    base::CommandLine::Init(1, argv);
  }
}

content::internal::ChildProcessLauncherHelper& GetReusableHelper(
    const std::string& process_type,
    std::unique_ptr<OHOS::NWeb::AafwkAppMgrClientAdapter> app_mgr_adapter) {
  using Helper = content::internal::ChildProcessLauncherHelper;
  using CommandLinePtr = std::unique_ptr<base::CommandLine>;
  using AppMgrAdapterPtr =
      std::unique_ptr<OHOS::NWeb::AafwkAppMgrClientAdapter>;

  struct FakeHelperStorage {
    alignas(Helper) unsigned char storage[sizeof(Helper)];
    bool initialized = false;

    Helper* helper() { return reinterpret_cast<Helper*>(storage); }
  };

  static FakeHelperStorage fake_helper_storage;
  Helper* helper = fake_helper_storage.helper();
  auto* command_line_slot = reinterpret_cast<CommandLinePtr*>(
      reinterpret_cast<char*>(helper) + offsetof(Helper, command_line_));
  auto* app_mgr_adapter_slot = reinterpret_cast<AppMgrAdapterPtr*>(
      reinterpret_cast<char*>(helper) +
      offsetof(Helper, app_mgr_client_adapter_));

  if (!fake_helper_storage.initialized) {
    new (command_line_slot) CommandLinePtr();
    new (app_mgr_adapter_slot) AppMgrAdapterPtr();
    fake_helper_storage.initialized = true;
  }

  *command_line_slot = std::make_unique<base::CommandLine>(
      base::FilePath(FILE_PATH_LITERAL("/nonexistent_arkweb_fuzzer_child")));
  (*command_line_slot)->AppendSwitchASCII(switches::kProcessType, process_type);
  (*command_line_slot)->DetachFromCurrentSequence();
  *app_mgr_adapter_slot = std::move(app_mgr_adapter);
  return *helper;
}

void ExerciseExitCodeStatus(int status, bool known_dead) {
  base::TerminationStatus termination_status =
      content::internal::FuzzArkwebChildProcessLauncherHelperUtilsFriend::
          GetProcessStatusByExitCode(status, known_dead);
  std::string reason =
      content::internal::FuzzArkwebChildProcessLauncherHelperUtilsFriend::
          GetExitReasonByTerminationStatus(termination_status);
  (void)reason;
}

void ExerciseTerminationReason(base::TerminationStatus status) {
  std::string reason =
      content::internal::FuzzArkwebChildProcessLauncherHelperUtilsFriend::
          GetExitReasonByTerminationStatus(status);
  (void)reason;
}

void ExerciseRenderProcessExitedInfo(int status, bool known_dead) {
  content::internal::FuzzArkwebChildProcessLauncherHelperUtilsFriend::
      RenderProcessExitedInfo(base::kNullProcessHandle, status, known_dead);
}

void ExerciseTerminateProcessByAppMgr(pid_t pid) {
  (void)content::internal::FuzzArkwebChildProcessLauncherHelperUtilsFriend::
      TerminateProcessByAppMgr(base::Process(pid));
}

void ExerciseGetTerminationInfoStillRunning() {
  content::internal::ArkwebChildProcessLauncherHelperUtils helper(nullptr);
  content::ChildProcessTerminationInfo info;
  content::internal::FuzzArkwebChildProcessLauncherHelperUtilsFriend::
      GetTerminationInfoArkweb(helper, base::Process(), false, info);
}

void ExerciseGetTerminationInfoKnownDead(int adapter_result, int exit_status) {
  auto& child_process_launcher_helper =
      GetReusableHelper(switches::kRendererProcess,
                        std::make_unique<MockAafwkAppMgrClientAdapter>(
                            1, adapter_result, exit_status));
  content::internal::ArkwebChildProcessLauncherHelperUtils helper(
      &child_process_launcher_helper);
  content::ChildProcessTerminationInfo info;
  content::internal::FuzzArkwebChildProcessLauncherHelperUtilsFriend::
      GetTerminationInfoArkweb(helper, base::Process(kMockRenderPid), true,
                               info);
}

void ExerciseLaunchChildProcessForTest() {
  EnsureCommandLineInitialized();
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kForTest);

  content::internal::ArkwebChildProcessLauncherHelperUtils helper(nullptr);
  base::CommandLine command_line(
      base::FilePath(FILE_PATH_LITERAL("/nonexistent_arkweb_fuzzer_child")));
  base::LaunchOptions options;
  content::internal::ChildProcessLauncherHelper::Process process;
  content::internal::FuzzArkwebChildProcessLauncherHelperUtilsFriend::
      LaunchChildProcess(helper, &command_line, &options, process);
}

void ExerciseLaunchChildProcessViaAppMgr(const std::string& process_type,
                                         int start_child_process_result) {
  EnsureCommandLineInitialized();
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kForTest);

  auto& child_process_launcher_helper = GetReusableHelper(
      process_type, std::make_unique<MockAafwkAppMgrClientAdapter>(
                        start_child_process_result, 0, MakeExitedStatus(0)));
  content::internal::ArkwebChildProcessLauncherHelperUtils helper(
      &child_process_launcher_helper);
  base::CommandLine command_line(
      base::FilePath(FILE_PATH_LITERAL("/nonexistent_arkweb_fuzzer_child")));
  command_line.AppendSwitchASCII(switches::kProcessType, process_type);
  command_line.AppendArg("--fuzz-child-arg");
  command_line.AppendArg(process_type);

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(kSharedFd, kSharedFd);
  options.fds_to_remap.emplace_back(kIpcFd, kIpcFd);
  options.fds_to_remap.emplace_back(kCrashSignalFd, kCrashSignalFd);

  content::internal::ChildProcessLauncherHelper::Process process;
  content::internal::FuzzArkwebChildProcessLauncherHelperUtilsFriend::
      LaunchChildProcess(helper, &command_line, &options, process);
}

void ExerciseLaunchChildProcessSuccess(const std::string& process_type) {
  ExerciseLaunchChildProcessViaAppMgr(process_type, 0);
}

void RunDeterministicCoverageCases() {
  const int normal_exit = MakeExitedStatus(0);
  const int abnormal_exit = MakeExitedStatus(1);
  const int crash_signal = MakeSignaledStatus(SIGSEGV, false);
  const int killed_signal = MakeSignaledStatus(SIGTERM, false);
  const int unknown_signal = MakeSignaledStatus(SIGHUP, false);
  const int core_crash_signal = MakeSignaledStatus(SIGABRT, true);
  const int abnormal_exit_255 = MakeExitedStatus(255);

  ExerciseExitCodeStatus(normal_exit, false);
  ExerciseExitCodeStatus(abnormal_exit, false);
  ExerciseExitCodeStatus(abnormal_exit_255, true);
  ExerciseExitCodeStatus(crash_signal, true);
  ExerciseExitCodeStatus(core_crash_signal, false);
  ExerciseExitCodeStatus(MakeSignaledStatus(SIGKILL, false), false);
  ExerciseExitCodeStatus(MakeSignaledStatus(SIGKILL, false), true);
  ExerciseExitCodeStatus(killed_signal, true);
  ExerciseExitCodeStatus(unknown_signal, true);
  ExerciseExitCodeStatus(unknown_signal, false);
  for (int signal : kCrashSignals) {
    ExerciseExitCodeStatus(MakeSignaledStatus(signal, false), true);
    ExerciseRenderProcessExitedInfo(MakeSignaledStatus(signal, true), true);
  }
  for (int signal : kKilledSignals) {
    ExerciseExitCodeStatus(MakeSignaledStatus(signal, false), true);
    ExerciseRenderProcessExitedInfo(MakeSignaledStatus(signal, false), true);
  }

  for (base::TerminationStatus status : kTerminationStatusEdges) {
    ExerciseTerminationReason(status);
  }

  ExerciseRenderProcessExitedInfo(normal_exit, false);
  ExerciseRenderProcessExitedInfo(normal_exit, true);
  ExerciseRenderProcessExitedInfo(abnormal_exit, false);
  ExerciseRenderProcessExitedInfo(abnormal_exit_255, true);
  ExerciseRenderProcessExitedInfo(crash_signal, true);
  ExerciseRenderProcessExitedInfo(core_crash_signal, false);
  ExerciseRenderProcessExitedInfo(killed_signal, true);
  ExerciseRenderProcessExitedInfo(unknown_signal, true);
  ExerciseRenderProcessExitedInfo(unknown_signal, false);

  ExerciseGetTerminationInfoStillRunning();
  ExerciseGetTerminationInfoKnownDead(1, MakeExitedStatus(1));
  ExerciseGetTerminationInfoKnownDead(-1, MakeExitedStatus(1));
  ExerciseGetTerminationInfoKnownDead(0, -1);
  ExerciseGetTerminationInfoKnownDead(0, std::numeric_limits<int>::min());
  ExerciseGetTerminationInfoKnownDead(0, normal_exit);
  ExerciseGetTerminationInfoKnownDead(0, abnormal_exit);
  ExerciseGetTerminationInfoKnownDead(0, abnormal_exit_255);
  ExerciseGetTerminationInfoKnownDead(0, crash_signal);
  ExerciseGetTerminationInfoKnownDead(0, core_crash_signal);
  ExerciseGetTerminationInfoKnownDead(0, killed_signal);
  ExerciseGetTerminationInfoKnownDead(0, unknown_signal);

  ExerciseTerminateProcessByAppMgr(base::kNullProcessHandle);
  ExerciseTerminateProcessByAppMgr(kMockRenderPid);

  ExerciseLaunchChildProcessViaAppMgr(switches::kRendererProcess, 1);
  ExerciseLaunchChildProcessViaAppMgr(switches::kGpuProcess, 1);
  ExerciseLaunchChildProcessViaAppMgr("utility", 1);
  ExerciseLaunchChildProcessViaAppMgr("arkweb_fuzzer_child", -1);
  ExerciseLaunchChildProcessViaAppMgr("arkweb_fuzzer_child", 2);
  ExerciseLaunchChildProcessSuccess(switches::kRendererProcess);
  ExerciseLaunchChildProcessSuccess(switches::kGpuProcess);
  ExerciseLaunchChildProcessSuccess("utility");
  ExerciseLaunchChildProcessForTest();
}

void FuzzArkwebChildProcessLauncherHelperUtils(FuzzedDataProvider& provider) {
  const size_t actions =
      provider.ConsumeIntegralInRange<size_t>(1, kMaxActions);
  for (size_t i = 0; i < actions && provider.remaining_bytes() > 0; ++i) {
    enum class Action : uint8_t {
      kExitCodeStatus,
      kTerminationReason,
      kRenderExitedInfo,
      kTerminateProcessByAppMgr,
      kGetTerminationInfoStillRunning,
      kGetTerminationInfoKnownDead,
      kLaunchChildProcessViaAppMgr,
      kLaunchChildProcessSuccess,
      kLaunchChildProcessForTest,
      kMaxValue = kLaunchChildProcessForTest,
    };

    switch (provider.ConsumeEnum<Action>()) {
      case Action::kExitCodeStatus:
        ExerciseExitCodeStatus(ConsumeWaitStatus(provider),
                               provider.ConsumeBool());
        break;
      case Action::kTerminationReason:
        ExerciseTerminationReason(ConsumeTerminationStatus(provider));
        break;
      case Action::kRenderExitedInfo:
        ExerciseRenderProcessExitedInfo(ConsumeWaitStatus(provider),
                                        provider.ConsumeBool());
        break;
      case Action::kTerminateProcessByAppMgr:
        ExerciseTerminateProcessByAppMgr(provider.ConsumeIntegral<pid_t>());
        break;
      case Action::kGetTerminationInfoStillRunning:
        ExerciseGetTerminationInfoStillRunning();
        break;
      case Action::kGetTerminationInfoKnownDead:
        ExerciseGetTerminationInfoKnownDead(
            provider.ConsumeIntegralInRange<int>(0, 1),
            ConsumeWaitStatus(provider));
        break;
      case Action::kLaunchChildProcessViaAppMgr:
        ExerciseLaunchChildProcessViaAppMgr(provider.ConsumeBool()
                                                ? switches::kRendererProcess
                                                : switches::kGpuProcess,
                                            1);
        break;
      case Action::kLaunchChildProcessSuccess:
        ExerciseLaunchChildProcessSuccess(provider.ConsumeBool()
                                              ? switches::kRendererProcess
                                              : switches::kGpuProcess);
        break;
      case Action::kLaunchChildProcessForTest:
        ExerciseLaunchChildProcessForTest();
        break;
    }
  }
}

}  // namespace
#endif  // BUILDFLAG(ARKWEB_RENDER_PROCESS_STARTUP)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_STARTUP)
  RunDeterministicCoverageCases();
  FuzzedDataProvider provider(data, size);
  FuzzArkwebChildProcessLauncherHelperUtils(provider);
#endif
  return 0;
}
