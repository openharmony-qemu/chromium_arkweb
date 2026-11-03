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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#include <libloaderapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cerrno>
#endif

#if defined(OH_ENABLE_HEAP_TRANSLATE)
#include "arkweb/chromium_ext/v8/dfx/heap_dump/dump-format.h"
#endif

namespace {

constexpr const char* kModeT1 = "t1";
constexpr const char* kModeT2 = "t2";
#if defined(_WIN32)
constexpr size_t kMaxPathSize = MAX_PATH;
#else
constexpr size_t kMaxPathSize = 4096;
#endif

bool FileExists(const std::string& path) {
#if defined(_WIN32)
  DWORD attrs = GetFileAttributesA(path.c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
  struct stat buffer;
  return (stat(path.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
#endif
}

std::string GetExecutableDir() {
#if defined(_WIN32)
  char buffer[kMaxPathSize] = {0};
  HMODULE hModule = GetModuleHandleA(NULL);
  if (hModule == NULL) {
    return "";
  }
  DWORD result = GetModuleFileNameA(hModule, buffer, kMaxPathSize);
  if (result == 0 || result >= kMaxPathSize) {
    return "";
  }
  std::string path(buffer);
  size_t pos = path.find_last_of("\\/");
  if (pos != std::string::npos) {
    return path.substr(0, pos);
  }
  return "";
#elif defined(__APPLE__)
  char buffer[kMaxPathSize] = {0};
  uint32_t bufsize = kMaxPathSize;
  if (_NSGetExecutablePath(buffer, &bufsize) != 0) {
    return "";
  }
  std::string path(buffer);
  size_t pos = path.find_last_of('/');
  if (pos != std::string::npos) {
    return path.substr(0, pos);
  }
  return "";
#else
  char buffer[kMaxPathSize] = {0};
  ssize_t len = readlink("/proc/self/exe", buffer, kMaxPathSize - 1);
  if (len > 0) {
    buffer[len] = '\0';
    std::string path(buffer);
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) {
      return path.substr(0, pos);
    }
    return "";
  }
  Dl_info info;
  if (dladdr(reinterpret_cast<void*>(GetExecutableDir), &info) != 0 &&
      info.dli_fname != nullptr) {
    std::string path(info.dli_fname);
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) {
      return path.substr(0, pos);
    }
    return "";
  }
  return "";
#endif
}

void PrintUsage(const char* program) {
  std::cerr << "Usage: " << program << " [OPTIONS] --translate <input.rawheap> "
            << "[--translate-out <output.heapsnapshot>] [v8 flags]\n\n"
            << "Translate a rawheap file to heapsnapshot.\n"
            << "Options:\n"
            << "  -h, --help             Show this help message.\n\n"
            << "Examples:\n"
            << "  " << program << " --translate input.rawheap\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string input_file;

  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      PrintUsage(argv[0]);
      return 0;
    }
    if (strcmp(arg, "--translate") == 0) {
      if (i + 1 < argc) {
        input_file = argv[i + 1];
      }
    }
  }

  if (input_file.empty()) {
    std::cerr << "Error: --translate is required\n";
    PrintUsage(argv[0]);
    return 1;
  }

  if (!FileExists(input_file)) {
    std::cerr << "Error: input file '" << input_file << "' does not exist\n";
    return 1;
  }

#if defined(OH_ENABLE_HEAP_TRANSLATE)
  bool is_sandbox = v8::dfx::IsRawHeapSandboxEnabled(input_file);
#else
  std::cerr << "Error: OH_ENABLE_HEAP_TRANSLATE is not defined\n";
  return 1;
#endif
  const char* mode = is_sandbox ? kModeT1 : kModeT2;

  std::string exe_dir = GetExecutableDir();
  if (exe_dir.empty()) {
    std::cerr << "Error: failed to determine executable directory\n";
    return 1;
  }

#if defined(_WIN32)
  std::string binary_path_with_exe = exe_dir + "\\" + mode + ".exe";
  std::string binary_path_without_exe = exe_dir + "\\" + mode;
  std::string binary_path;

  if (FileExists(binary_path_with_exe)) {
    binary_path = binary_path_with_exe;
  } else if (FileExists(binary_path_without_exe)) {
    binary_path = binary_path_without_exe;
  } else {
    std::cerr << "Error: neither '" << binary_path_with_exe << "' nor '"
              << binary_path_without_exe << "' exists\n";
    return 1;
  }

  std::ostringstream cmd_line;
  cmd_line << binary_path;
  for (int i = 1; i < argc; ++i) {
    cmd_line << " " << argv[i];
  }
  std::string cmd_line_str = cmd_line.str();

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);

  if (CreateProcessA(nullptr, const_cast<char*>(cmd_line_str.c_str()), nullptr, nullptr,
                     FALSE, 0, nullptr, nullptr, &si, &pi)) {
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exit_code);
  }

  std::cerr << "Error: failed to execute " << binary_path << " (error code: "
            << GetLastError() << ")\n";
  return 1;
#else
  std::string binary_path = exe_dir + "/" + mode;
  if (FileExists(binary_path)) {
    execv(binary_path.c_str(), argv);
  }
  std::cerr << "Error: failed to execute " << binary_path << ": " << strerror(errno) << "\n";
  return 1;
#endif
}
