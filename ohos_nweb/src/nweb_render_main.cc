/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <sstream>
#include <string>
#include <vector>

#include <AbilityKit/native_child_process.h>

#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#include "include/cef_app.h"
#include "nweb_export.h"
#include "nweb_hilog.h"

namespace {
constexpr char kIpcFdName[] = "IPC_FD";
constexpr char kSharedFdName[] = "SHARED_FD";
constexpr char kCrashFdName[] = "CRASH_FD";

bool SetGlobalDescriptor(int key, int fd, bool required) {
  if (fd <= 0) {
    return !required;
  }

  base::GlobalDescriptors *descriptors = base::GlobalDescriptors::GetInstance();
  if (descriptors == nullptr) {
    WVLOG_E("GlobalDescriptors is null");
    return false;
  }

  int duplicated_fd = dup(fd);
  if (duplicated_fd < 0) {
    WVLOG_E("duplicate fd failed, key=%{public}d", key);
    descriptors->Set(key, fd);
  } else {
    descriptors->Set(key, duplicated_fd);
    close(fd);
  }
  return true;
}
} // namespace

bool SetGlobalDescriptors(int ipc_fd, int shared_fd, int crash_fd) {
  return SetGlobalDescriptor(kMojoIPCChannel, ipc_fd, true) &&
         SetGlobalDescriptor(kFieldTrialDescriptor, shared_fd, true) &&
         SetGlobalDescriptor(kCrashDumpSignal, crash_fd, false);
}

extern "C" OHOS_NWEB_EXPORT void NWebRenderMain(NativeChildProcess_Args args) {
  WVLOG_I("NWebRenderMain start, sandbox pid=%{public}d", getpid());

  if (args.entryParams == nullptr) {
    WVLOG_E("render entry params are null");
    return;
  }

  int ipc_fd = -1;
  int shared_fd = -1;
  int crash_fd = -1;
  for (NativeChildProcess_Fd *fd_node = args.fdList.head; fd_node != nullptr;
       fd_node = fd_node->next) {
    if (fd_node->fdName == nullptr) {
      continue;
    }
    std::string fd_name(fd_node->fdName);
    if (fd_name == kIpcFdName) {
      ipc_fd = fd_node->fd;
    } else if (fd_name == kSharedFdName) {
      shared_fd = fd_node->fd;
    } else if (fd_name == kCrashFdName) {
      crash_fd = fd_node->fd;
    }
  }

  if (!SetGlobalDescriptors(ipc_fd, shared_fd, crash_fd)) {
    WVLOG_E("failed to set global descriptors");
    return;
  }

  std::stringstream args_stream{std::string(args.entryParams)};
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

  CefMainArgs main_args(static_cast<int>(argv_strings.size()), argv.data());
  (void)CefExecuteProcess(main_args, nullptr, nullptr);

  WVLOG_I("NWebRenderMain end, sandbox pid=%{public}d", getpid());
}
