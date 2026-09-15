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

#include <string>
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#endif

#include "nweb_extension_connect_native.h"
#include "nweb_runtime_connect_info_impl.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "base/functional/callback.h"
#endif


namespace OHOS::NWeb {

#if BUILDFLAG(IS_ARKWEB_EXT)
NWebConnectNativeManager::NWebConnectNativeManager()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT}))
{}

NWebConnectNativeManager::~NWebConnectNativeManager() {
  if (cleanup_timer_) {
    cleanup_timer_->Stop();
    cleanup_timer_.reset();
  }
  // Release the C API callback handle
  if (runtime_callback_) {
    ArkWebExtensionRuntimeController_ReleaseRuntimeCallback();
    runtime_callback_ = nullptr;
  }
  // All pending callbacks are destroyed when the map is cleared.
  // We do not invoke them during teardown to avoid calling back into
  // Chromium objects that may already be destroyed.
  pending_map_.clear();
}
#endif

NWebConnectNativeManager* NWebConnectNativeManager::GetInstance() {
  static base::NoDestructor<NWebConnectNativeManager> instance;
  return instance.get();
}

void NWebConnectNativeManager::RegisterNWebHandler(int nweb_id, std::shared_ptr<NWebHandler> handler) {
  base::AutoLock lock(lock_);
  nweb_handler_map_[nweb_id] = handler;
}

void NWebConnectNativeManager::UnRegisterNWebHandler(int nweb_id) {
  base::AutoLock lock(lock_);
  auto it = nweb_handler_map_.find(nweb_id);
  if (it != nweb_handler_map_.end()) {
    nweb_handler_map_.erase(it);
  }
}

base::Process NWebConnectNativeManager::LaunchConnectNative(const std::string& native_host_name,
                                                  const std::string& origin,
                                                  int read_file,
                                                  int write_file) {
  ConnectNativeInfo info;
  info.connectionId = -1;
  info.bundleName = native_host_name;
  info.extensionOrigin = origin;
  info.extensionPid = -1;
  info.messageReadPipeId = read_file;
  info.messageWritePipeId = write_file;
  LOG(INFO) << "NWebConnectNativeManager::LaunchConnectNative bundleName: " << info.bundleName
            << " extensionOrigin: " << info.extensionOrigin
            << " read_file: " << info.messageReadPipeId
            << " write_file: " << info.messageWritePipeId;

  OHOS::NWeb::ConnectNativeOptions options;

  options.SetOnConnect([](const int connectionId) {
    LOG(INFO) << "Connection Success, PID is " << connectionId;
  });

  options.SetOnDisconnect([](const int connectionId) {
    LOG(INFO) << "DisConnection Sucess, PID is" << connectionId;
  });

  options.SetOnFail([](int error_code) {
    LOG(ERROR) << "Connection Fail, ERROR CODE is " << error_code;
  });

  std::string connection_result = "";
  std::shared_ptr<NWebNativeMessageCallback> nweb_callback =
      std::make_shared<NWebNativeMessageCallbackImpl>(options);
  std::shared_ptr<NWebRuntimeConnectInfo> nwebRuntimeConnectInfo =
      std::make_shared<NWebRuntimeConnectInfoImpl>(info.bundleName, info.extensionOrigin,
      info.messageReadPipeId, info.messageWritePipeId);

  std::shared_ptr<NWebHandler> handler;
  {
    base::AutoLock lock(lock_);
    if (!nweb_handler_map_.empty()) {
      auto weak_handler = nweb_handler_map_.begin()->second;
      handler = weak_handler.lock();
    }
  }

  if (handler) {
    connection_result = handler->OnWebNativeMessage(nwebRuntimeConnectInfo, nweb_callback);
    LOG(INFO) << "Connect appservice create process connection_result: " << connection_result;
  } else {
    LOG(ERROR) << "LaunchConnectNative cannot find web handler";
    return base::Process();
  }

  int connectionId = -1;
  size_t pos = connection_result.find(':');
  if (pos == std::string::npos) {
    LOG(ERROR) << "Invalid connection result format, missing ':': " << connection_result;
    return base::Process();
  }
  std::string status = connection_result.substr(0, pos);
  if (status == "success"){
    int parsed_id = 0;
    if (!base::StringToInt(connection_result.substr(pos + 1), &parsed_id)) {
      LOG(ERROR) << "Invalid connection ID in result: " << connection_result;
      return base::Process();
    }
    connectionId = parsed_id;
  } else {
    LOG(ERROR) << "Connect appservice process failed";
    return base::Process();
  }

  LOG(INFO) << "Connect appservice create process PID: " << connectionId;
  base::Process local_process = base::Process(connectionId);

  {
    base::AutoLock lock(lock_);
    auto it = connectid_process_map_.find(connectionId);
    if (it == connectid_process_map_.end()) {
      base::Process duplicated_process  = local_process.Duplicate();
      connectid_process_map_[connectionId] = std::move(duplicated_process);
    }
  }
  return local_process;
}

void NWebConnectNativeManager::DisconnectNative(int connectionId) {
  // Extract handler and erase map entry under lock, then call callback
  // outside the lock to avoid holding lock during external callback.
  //
  // Safety: The lock is released before calling OnExtensionDisconnect, so
  // re-entry into this class's locked methods will not deadlock. The map
  // entry is already erased, so a re-entrant DisconnectNative on the same
  // connectionId will hit the "not found" path and return safely.
  //
  // Assumption: OnExtensionDisconnect implementations (in web_webview) do
  // not synchronously call back into NWebConnectNativeManager in a way that
  // creates infinite recursion. If this assumption is violated in the future,
  // revert to PostTask or add a re-entrancy guard.
  std::shared_ptr<NWebHandler> handler;
  {
    base::AutoLock lock(lock_);
    auto it = connectid_process_map_.find(connectionId);
    if (it == connectid_process_map_.end()) {
      LOG(ERROR) << "DisconnectNative Fail, Cannot found PID:" << connectionId;
      return;
    }

    if (nweb_handler_map_.empty()) {
      LOG(ERROR) << "DisconnectNative cannot find web handler";
      connectid_process_map_.erase(it);
      return;
    }

    auto weak_handler = nweb_handler_map_.begin()->second;
    handler = weak_handler.lock();
    if (!handler) {
      connectid_process_map_.erase(it);
      return;
    }

    connectid_process_map_.erase(it);
  }

  LOG(INFO) << "DisconnectNative send to appservice PID: " << connectionId;
  handler->OnExtensionDisconnect(connectionId);
}

#if BUILDFLAG(IS_ARKWEB_EXT)
// --- sendBrowserMessage with pending map ---

// Static bridge: invoked by the EX C API response path when the NAPI layer
// calls SendBrowserMessageResponse.  |request_id| identifies the pending
// request; |response| is the C string from the app.
void OnBrowserMessageResponseBridge(uint64_t request_id,
                                     const char* response) {
  auto* manager = NWebConnectNativeManager::GetInstance();
  // Route response to the manager's task runner to avoid executing on the
  // arbitrary thread that the C API (EX/NAPI layer) calls back on.
  manager->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&NWebConnectNativeManager::OnBrowserMessageResponse,
                     // Safe: NWebConnectNativeManager is a NoDestructor singleton.
                     base::Unretained(manager), request_id,
                     response ? std::string(response) : std::string()));
}

void NWebConnectNativeManager::EnsureRuntimeCallbackInitialized() {
  if (runtime_callback_) {
    return;
  }
  ArkWebExtensionRuntimeController_CreateRuntimeCallback(&runtime_callback_);
  if (!runtime_callback_) {
    LOG(ERROR) << "SendBrowserMessage: failed to create runtime callback";
    return;
  }
  ArkWebExtensionRuntimeController_SetOnSendBrowserMessageResponse(
      runtime_callback_, OnBrowserMessageResponseBridge);
}

void NWebConnectNativeManager::SendBrowserMessage(
    const std::string& extension_id,
    const std::string& message_json,
    base::OnceCallback<void(std::string response)> callback) {
  uint64_t request_id;
  std::unique_ptr<base::OnceCallback<void(std::string)>> error_callback;
  {
    base::AutoLock lock(lock_);
    EnsureRuntimeCallbackInitialized();
    if (!runtime_callback_) {
      LOG(ERROR) << "SendBrowserMessage: runtime callback not available";
      error_callback = std::make_unique<base::OnceCallback<void(std::string)>>(
          std::move(callback));
    } else {
      request_id = next_request_id_++;
      pending_map_[request_id] = PendingEntry{
          std::make_unique<base::OnceCallback<void(std::string)>>(std::move(callback)),
          base::TimeTicks::Now()
      };
    }
  }

  if (error_callback) {
    std::move(*error_callback).Run(R"({"code":500,"msg":"runtime callback creation failed"})");
    return;
  }

  SchedulePendingCleanup();

  ArkWebExtensionRuntimeController_OnSendBrowserMessage(
      /*nwebId=*/0,
      extension_id.c_str(),
      message_json.c_str(),
      request_id);
}

void NWebConnectNativeManager::OnBrowserMessageResponse(
    uint64_t request_id, const std::string& response) {
  // Extract callback under lock, invoke outside lock.
  std::unique_ptr<base::OnceCallback<void(std::string)>> callback;
  {
    base::AutoLock lock(lock_);
    auto it = pending_map_.find(request_id);
    if (it == pending_map_.end()) {
      LOG(WARNING) << "OnBrowserMessageResponse: unknown/late request_id=" << request_id
                   << ", ignoring";
      return;
    }
    callback = std::move(it->second.callback);
    pending_map_.erase(it);
  }

  std::move(*callback).Run(response);
}

void NWebConnectNativeManager::SchedulePendingCleanup() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&NWebConnectNativeManager::SchedulePendingCleanup,
                       // Safe: NWebConnectNativeManager is a NoDestructor singleton.
                       base::Unretained(this)));
    return;
  }

  if (!cleanup_timer_) {
    cleanup_timer_ = std::make_unique<base::RetainingOneShotTimer>();
    cleanup_timer_->SetTaskRunner(task_runner_);
  }

  if (cleanup_timer_->IsRunning()) {
    return;  // Already scheduled
  }

  // Check again after the timeout period
  cleanup_timer_->Start(
      FROM_HERE,
      base::Seconds(kPendingTimeoutSeconds),
      this,
      &NWebConnectNativeManager::DoPendingCleanup);
}

void NWebConnectNativeManager::DoPendingCleanup() {
  base::AutoLock lock(lock_);
  auto now = base::TimeTicks::Now();
  auto timeout = base::Seconds(kPendingTimeoutSeconds);

  for (auto it = pending_map_.begin(); it != pending_map_.end();) {
    if (now - it->second.timestamp >= timeout) {
      LOG(WARNING) << "DoPendingCleanup: expiring request_id=" << it->first;
      it = pending_map_.erase(it);
    } else {
      ++it;
    }
  }

  // Reschedule if there are still pending entries
  if (!pending_map_.empty()) {
    cleanup_timer_->Reset();
  }
}
#endif

}  // namespace OHOS::NWeb
