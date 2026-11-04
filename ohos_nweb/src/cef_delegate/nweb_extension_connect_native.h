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

#ifndef NWEB_EXTENSION_CONNECT_NATIVE_H_
#define NWEB_EXTENSION_CONNECT_NATIVE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/launch.h"
#include "base/process/kill.h"
#include "base/synchronization/lock.h"

#include "arkweb/ohos_nweb/include/nweb_handler.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "ohos_nweb_ex/core/arkweb/nweb_extension_runtime_delegate_impl.h"
#endif

namespace OHOS::NWeb {

struct ConnectNativeInfo {
  int connectionId;
  int extensionPid;
  std::string bundleName;
  std::string extensionOrigin;
  int messageReadPipeId;
  int messageWritePipeId;
};

class OHOS_NWEB_EXPORT ConnectNativeOptions {
 public:
  using ConnectCallback = std::function<void(const int connection)>;
  using DisconnectCallback = std::function<void(const int connection)>;
  using FailCallback = std::function<void(int error_code)>;

  ConnectNativeOptions() = default;
  ~ConnectNativeOptions() = default;

  void SetOnConnect(ConnectCallback callback) {
    on_connect_callback_ = callback;
  }

  void SetOnDisconnect(DisconnectCallback callback) {
    on_disconnect_callback_ = callback;
  }

  void SetOnFail(FailCallback callback) {
    on_fail_callback_ = callback;
  }

  void TriggerConnect(const int connection) {
    if (on_connect_callback_) {
      on_connect_callback_(connection);
    }
  }

  void TriggerDisconnect(const int connection) {
    if (on_disconnect_callback_) {
      on_disconnect_callback_(connection);
    }
  }

  void TriggerFail(int error_code) {
    if (on_fail_callback_) {
      on_fail_callback_(error_code);
    }
  }

 private:
  ConnectCallback on_connect_callback_;
  DisconnectCallback on_disconnect_callback_;
  FailCallback on_fail_callback_;
};


class NWebConnectNativeManager {
 public:

  static NWebConnectNativeManager* GetInstance();

  void RegisterNWebHandler(int nweb_id, std::shared_ptr<NWebHandler> handler);

  void UnRegisterNWebHandler(int nweb_id);

  base::Process LaunchConnectNative(const std::string& native_host_name,
                          const std::string& origin,
                          int read_file,
                          int write_file);


  void DisconnectNative(int connectionId);

#if BUILDFLAG(IS_ARKWEB_EXT)
  void SendBrowserMessage(const std::string& extension_id,
                          const std::string& message_json,
                          base::OnceCallback<void(std::string response)> callback);

  // Returns the task runner used for response/cleanup tasks.
  // Used by the C API bridge to post responses to the correct sequence.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

  // Called on the manager's task runner when EX/NAPI sends a response.
  // Looks up request_id in the pending map, removes it, and invokes the
  // callback outside the lock.  Duplicate/late responses are logged and
  // ignored.
  void OnBrowserMessageResponse(uint64_t request_id, const std::string& response);
#endif

  NWebConnectNativeManager(const NWebConnectNativeManager&) = delete;

  NWebConnectNativeManager& operator=(const NWebConnectNativeManager&) = delete;

 private:
  friend class base::NoDestructor<NWebConnectNativeManager>;

#if BUILDFLAG(IS_ARKWEB_EXT)
  NWebConnectNativeManager();
  ~NWebConnectNativeManager();
#else
  NWebConnectNativeManager() = default;
#endif

#if BUILDFLAG(IS_ARKWEB_EXT)
  // Initializes the C API callback bridge on first use.
  void EnsureRuntimeCallbackInitialized();

  // Schedules a cleanup pass that removes pending callbacks older than
  // |kPendingTimeoutSeconds|.  The timer only removes callbacks; it does not
  // invoke them (to preserve Chromium's 10-second reject semantics).
  void SchedulePendingCleanup();

  // Performs one cleanup pass: removes expired pending callbacks.
  void DoPendingCleanup();

  static constexpr int kPendingTimeoutSeconds = 15;
#endif

  base::Lock lock_;

  std::unordered_map<int, std::weak_ptr<NWebHandler>> nweb_handler_map_;

  std::unordered_map<int, base::Process> connectid_process_map_;

#if BUILDFLAG(IS_ARKWEB_EXT)
  // Tracks whether the runtime callback has been registered.
  bool initialized_ = false;

  // Monotonically increasing request ID.  Guarded by |lock_|.
  uint64_t next_request_id_ = 1;

  // Pending callbacks waiting for a response from the EX/NAPI/App layer.
  // Key: request_id, Value: (callback, timestamp).
  // Guarded by |lock_|.
  struct PendingEntry {
    std::unique_ptr<base::OnceCallback<void(std::string)>> callback;
    base::TimeTicks timestamp;
  };
  std::unordered_map<uint64_t, PendingEntry> pending_map_;

  // One-shot timer for cleaning up expired pending callbacks.
  std::unique_ptr<base::RetainingOneShotTimer> cleanup_timer_;

  // Task runner for scheduling the cleanup timer.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
#endif
};

class NWebNativeMessageCallbackImpl : public NWebNativeMessageCallback {
 public:
  explicit NWebNativeMessageCallbackImpl(OHOS::NWeb::ConnectNativeOptions callback)
      : callback_(callback) {}

  void OnConnect(int pid) override {
    callback_.TriggerConnect(pid);
  }

  void OnDisconnect(int nativeInfo) override {
    callback_.TriggerDisconnect(nativeInfo);
  }
  void OnFailed(int code) override {
    callback_.TriggerFail(code);
  }

 private:
  OHOS::NWeb::ConnectNativeOptions callback_;
};

}  // namespace OHOS::NWeb

#endif  // NWEB_EXTENSION_CONNECT_NATIVE_H_
