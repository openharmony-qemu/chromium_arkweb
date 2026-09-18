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

#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <AbilityKit/native_child_process.h>
#include <IPCKit/ipc_kit.h>
#include <native_window/external_window.h>

#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#include "base/command_line.h"
#include "include/cef_app.h"
#include "third_party/blink/renderer/core/render_mojom/render_mojom_client.h"
#include "nweb_export.h"
#include "nweb_hilog.h"

namespace {
constexpr uint32_t kInitDataCode = 10001;
constexpr uint32_t kQueryWindowCode = 10002;
constexpr uint32_t kDestroyWindowCode = 10003;
constexpr uint32_t kPassWindowCode = 10004;
constexpr uint32_t kQueryBoolCode = 10005;
constexpr uint32_t kDestroyPassedWindowCode = 10006;
constexpr uint32_t kQueryIntCode = 10007;
constexpr char kGpuIpcDescriptor[] = "chromium.native.gpu";
constexpr auto kStartTimeout = std::chrono::seconds(3);

int g_ipc_fd = -1;
int g_shared_fd = -1;
int g_crash_fd = -1;
std::string g_command_line;
std::mutex g_start_mutex;
std::condition_variable g_start_condition;
OHIPCRemoteProxy *g_browser_proxy = nullptr;

void DestroyParcels(OHIPCParcel *data, OHIPCParcel *reply) {
  if (data != nullptr) {
    OH_IPCParcel_Destroy(data);
  }
  if (reply != nullptr) {
    OH_IPCParcel_Destroy(reply);
  }
}
} // namespace

extern bool SetGlobalDescriptors(int ipc_fd, int shared_fd, int crash_fd);

void *QueryRenderWindowFromBrowserProcess(int32_t window_id) {
  if (g_browser_proxy == nullptr) {
    WVLOG_E("browser IPC proxy is null");
    return nullptr;
  }

  OHIPCParcel *data = OH_IPCParcel_Create();
  OHIPCParcel *reply = OH_IPCParcel_Create();
  if (data == nullptr || reply == nullptr ||
      OH_IPCParcel_WriteInt32(data, window_id) != 0 ||
      OH_IPCRemoteProxy_SendRequest(g_browser_proxy, kQueryWindowCode, data,
                                    reply, nullptr) != 0) {
    WVLOG_E("query render window failed, id=%{public}d", window_id);
    DestroyParcels(data, reply);
    return nullptr;
  }

  OHNativeWindow *window = nullptr;
  if (OH_NativeWindow_ReadFromParcel(reply, &window) != 0) {
    WVLOG_E("read render window failed, id=%{public}d", window_id);
    DestroyParcels(data, reply);
    return nullptr;
  }
  DestroyParcels(data, reply);
  return window;
}

void DestroyRenderWindowFromBrowserProcess(int32_t window_id) {
  if (g_browser_proxy == nullptr) {
    WVLOG_E("browser IPC proxy is null");
    return;
  }

  OHIPCParcel *data = OH_IPCParcel_Create();
  OHIPCParcel *reply = OH_IPCParcel_Create();
  if (data == nullptr || reply == nullptr ||
      OH_IPCParcel_WriteInt32(data, window_id) != 0 ||
      OH_IPCRemoteProxy_SendRequest(g_browser_proxy, kDestroyWindowCode, data,
                                    reply, nullptr) != 0) {
    WVLOG_E("destroy render window failed, id=%{public}d", window_id);
  }
  DestroyParcels(data, reply);
}

void PassWindowToBrowserProcess(int64_t window_id) {
  if (g_browser_proxy == nullptr) {
    WVLOG_E("browser IPC proxy is null");
    return;
  }

  OHNativeWindow *window = nullptr;
  if (OH_NativeWindow_CreateNativeWindowFromSurfaceId(window_id, &window) !=
          0 ||
      window == nullptr) {
    WVLOG_E("create native window failed, id=%{public}lld",
            static_cast<long long>(window_id));
    return;
  }

  OHIPCParcel *data = OH_IPCParcel_Create();
  OHIPCParcel *reply = OH_IPCParcel_Create();
  if (data == nullptr || reply == nullptr ||
      OH_NativeWindow_WriteToParcel(window, data) != 0 ||
      OH_IPCRemoteProxy_SendRequest(g_browser_proxy, kPassWindowCode, data,
                                    reply, nullptr) != 0) {
    WVLOG_E("pass native window failed, id=%{public}lld",
            static_cast<long long>(window_id));
  }
  DestroyParcels(data, reply);
  OH_NativeWindow_NativeObjectUnreference(window);
}

void DestroyPassedSurfaceFromGpuProcess(int64_t surface_id) {
  if (g_browser_proxy == nullptr) {
    return;
  }
  OHIPCParcel *data = OH_IPCParcel_Create();
  OHIPCParcel *reply = OH_IPCParcel_Create();
  if (data == nullptr || reply == nullptr ||
      OH_IPCParcel_WriteInt64(data, surface_id) != 0 ||
      OH_IPCRemoteProxy_SendRequest(g_browser_proxy, kDestroyPassedWindowCode,
                                    data, reply, nullptr) != 0) {
    WVLOG_E("destroy passed surface failed, id=%{public}lld",
            static_cast<long long>(surface_id));
  }
  DestroyParcels(data, reply);
}

bool QueryBoolFromBrowserProcess(const std::string &key, bool default_value) {
  if (g_browser_proxy == nullptr) {
    return OHOS::NWeb::OhosAdapterHelper::GetInstance()
        .GetSystemPropertiesInstance()
        .GetBoolParameter(key, default_value);
  }

  OHIPCParcel *data = OH_IPCParcel_Create();
  OHIPCParcel *reply = OH_IPCParcel_Create();
  if (data == nullptr || reply == nullptr ||
      OH_IPCParcel_WriteString(data, key.c_str()) != 0 ||
      OH_IPCParcel_WriteInt32(data, default_value ? 1 : 0) != 0 ||
      OH_IPCRemoteProxy_SendRequest(g_browser_proxy, kQueryBoolCode, data,
                                    reply, nullptr) != 0) {
    WVLOG_E("query bool failed, key=%{public}s", key.c_str());
    DestroyParcels(data, reply);
    return default_value;
  }

  int32_t result = default_value ? 1 : 0;
  if (OH_IPCParcel_ReadInt32(reply, &result) != 0) {
    WVLOG_E("read bool failed, key=%{public}s", key.c_str());
    result = default_value ? 1 : 0;
  }
  DestroyParcels(data, reply);
  return result != 0;
}

int32_t QueryIntFromBrowserProcess(const std::string &key, int32_t default_value) {
  auto* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd && cmd->GetSwitchValueASCII("type") == "renderer") {
    int32_t mojo_result = default_value;
    if (blink::SysPropRenderObserverClientRep::QueryInt(key, default_value, &mojo_result)) {
      return mojo_result;
    }
    return default_value;
  }
  if (g_browser_proxy == nullptr) {
    return OHOS::NWeb::OhosAdapterHelper::GetInstance()
        .GetSystemPropertiesInstance()
        .GetIntParameter(key, default_value);
  }

  OHIPCParcel *data = OH_IPCParcel_Create();
  OHIPCParcel *reply = OH_IPCParcel_Create();
  if (data == nullptr || reply == nullptr ||
      OH_IPCParcel_WriteString(data, key.c_str()) != 0 ||
      OH_IPCParcel_WriteInt32(data, default_value) != 0 ||
      OH_IPCRemoteProxy_SendRequest(g_browser_proxy, kQueryIntCode, data,
                                    reply, nullptr) != 0) {
    WVLOG_E("query int failed, key=%{public}s", key.c_str());
    DestroyParcels(data, reply);
    return default_value;
  }

  int32_t result = default_value;
  if (OH_IPCParcel_ReadInt32(reply, &result) != 0) {
    WVLOG_E("read int failed, key=%{public}s", key.c_str());
    result = default_value;
  }
  DestroyParcels(data, reply);
  return result;
}

namespace {
void RunGpuProcess(const std::string &command_line) {
  WVLOG_I("GPU child process start, pid=%{public}d", getpid());

  std::stringstream args_stream(command_line);
  constexpr char kSeparator = '#';
  std::vector<std::string> argv_strings;
  std::string argument;
  while (std::getline(args_stream, argument, kSeparator)) {
    argv_strings.push_back(argument);
  }

  std::vector<char *> argv;
  argv.reserve(argv_strings.size() + 1);
  for (const auto &value : argv_strings) {
    argv.push_back(const_cast<char *>(value.c_str()));
  }
  argv.push_back(nullptr);

  if (!SetGlobalDescriptors(g_ipc_fd, g_shared_fd, g_crash_fd)) {
    WVLOG_E("failed to set GPU global descriptors");
    return;
  }

  CefMainArgs main_args(static_cast<int>(argv_strings.size()), argv.data());
  (void)CefExecuteProcess(main_args, nullptr, nullptr);
  WVLOG_I("GPU child process end, pid=%{public}d", getpid());
}

int OnRemoteRequest(uint32_t code, const OHIPCParcel *data, OHIPCParcel *reply,
                    void *user_data) {
  if (code != kInitDataCode || data == nullptr || reply == nullptr) {
    WVLOG_E("unexpected GPU child request, code=%{public}u", code);
    return -1;
  }

  std::lock_guard<std::mutex> lock(g_start_mutex);
  g_browser_proxy = OH_IPCParcel_ReadRemoteProxy(data);
  const char *command_line = OH_IPCParcel_ReadString(data);
  if (g_browser_proxy == nullptr || command_line == nullptr ||
      OH_IPCParcel_ReadFileDescriptor(data, &g_ipc_fd) != 0 ||
      OH_IPCParcel_ReadFileDescriptor(data, &g_shared_fd) != 0) {
    WVLOG_E("read GPU child init data failed");
    return -1;
  }
  // Crashpad may be disabled, in which case the crash fd is intentionally
  // absent. Keep the default -1 when reading it fails.
  (void)OH_IPCParcel_ReadFileDescriptor(data, &g_crash_fd);
  g_command_line = command_line;
  if (OH_IPCParcel_WriteInt32(reply, getpid()) != 0) {
    WVLOG_E("write GPU child pid failed");
    return -1;
  }
  g_start_condition.notify_all();
  return 0;
}
} // namespace

extern "C" OHOS_NWEB_EXPORT OHIPCRemoteStub *NativeChildProcess_OnConnect() {
  WVLOG_I("NativeChildProcess_OnConnect");
  return OH_IPCRemoteStub_Create(kGpuIpcDescriptor, &OnRemoteRequest, nullptr,
                                 nullptr);
}

extern "C" OHOS_NWEB_EXPORT void NativeChildProcess_MainProc() {
  WVLOG_I("NativeChildProcess_MainProc");
  std::unique_lock<std::mutex> lock(g_start_mutex);
  if (!g_start_condition.wait_for(lock, kStartTimeout, [] {
        return !g_command_line.empty() && g_ipc_fd > 0 && g_shared_fd > 0 &&
               g_browser_proxy != nullptr;
      })) {
    WVLOG_E("timeout waiting for GPU child init data");
    return;
  }
  std::string command_line = g_command_line;
  lock.unlock();
  RunGpuProcess(command_line);
}
