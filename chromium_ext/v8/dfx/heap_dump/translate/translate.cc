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

#include "arkweb/chromium_ext/v8/dfx/heap_dump/translate/translate.h"

#include <stdio.h>

#if DEBUG && defined(__linux__)
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#if defined(OH_ENABLE_HEAP_TRANSLATE)
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/binary-reader.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/snapshot-generator.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/snapshot-serializer.h"
#include "arkweb/chromium_ext/v8/dfx/heap_dump/translator/translate-heap.h"
#endif
#include "src/flags/flags.h"

namespace v8 {
namespace internal {
namespace translate {
namespace {

#if DEBUG && defined(__linux__)
void CrashSignalHandler(int signum) {
  void* frames[128];
  int frame_count = backtrace(frames, 128);
  std::fprintf(stderr, "\n[translate-debug] fatal signal %d\n", signum);
  backtrace_symbols_fd(frames, frame_count, STDERR_FILENO);
  _exit(128 + signum);
}

void InstallCrashSignalHandlers() {
  signal(SIGSEGV, CrashSignalHandler);
  signal(SIGABRT, CrashSignalHandler);
  signal(SIGBUS, CrashSignalHandler);
  signal(SIGILL, CrashSignalHandler);
}
#endif

struct ToolOptions {
  bool show_help = false;
  std::string translate_path;
  std::string translate_out_path;
  std::vector<char*> v8_args;
};

enum class V8FlagResult {
  kOk,
  kShowHelp,
  kError,
};

bool HasSuffix(const std::string& value, const char* suffix) {
  size_t suffix_size = std::strlen(suffix);
  if (value.size() < suffix_size) {
    return false;
  }
  return value.compare(value.size() - suffix_size, suffix_size, suffix) == 0;
}

bool ParseInlineOption(const std::string& arg, const char* option,
                       std::string* value) {
  size_t option_size = std::strlen(option);
  if (arg.size() <= option_size || arg.compare(0, option_size, option) != 0 ||
      arg[option_size] != '=') {
    return false;
  }
  *value = arg.substr(option_size + 1);
  return true;
}

std::string BuildUsage(const char* program_name) {
  std::string usage = "Usage: ";
  usage += program_name;
  usage +=
      " --translate <input.rawheap> [--translate-out <output.heapsnapshot>] "
      "[v8 flags]\n";
  usage += "       ";
  usage += program_name;
  usage += " --help\n\n";
  usage += "Options:\n";
  usage +=
      "  --translate <file.rawheap>          Input rawheap file to translate.\n";
  usage +=
      "  --translate-out <file.heapsnapshot> Output heapsnapshot file path.\n";
  usage +=
      "                                      Default: rawheap.heapsnapshot\n";
  usage +=
      "  -h, --help                          Show translate and V8 flag help.\n";
  return usage;
}

void PrintToolError(const std::string& message) {
  std::fprintf(stderr, "Error: %s\n", message.c_str());
  std::fprintf(stderr, "Try --help for options\n");
}

bool ParseToolArgs(int argc, char* argv[], ToolOptions* options,
                   std::string* error) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);

    if (arg == "-h" || arg == "--help") {
      options->show_help = true;
      continue;
    }

    if (arg == "--translate") {
      if (i + 1 >= argc) {
        *error = "missing value for --translate";
        return false;
      }
      options->translate_path = argv[++i];
      continue;
    }

    std::string option_value;
    if (ParseInlineOption(arg, "--translate", &option_value)) {
      options->translate_path = std::move(option_value);
      continue;
    }

    if (arg == "--translate-out") {
      if (i + 1 >= argc) {
        *error = "missing value for --translate-out";
        return false;
      }
      options->translate_out_path = argv[++i];
      continue;
    }

    if (ParseInlineOption(arg, "--translate-out", &option_value)) {
      options->translate_out_path = std::move(option_value);
      continue;
    }

    if (arg == "--") {
      for (int j = i + 1; j < argc; ++j) {
        options->v8_args.push_back(argv[j]);
      }
      break;
    }

    options->v8_args.push_back(argv[i]);
  }

  if (!options->show_help && options->translate_path.empty()) {
    *error = "missing required option --translate <input.rawheap>";
    return false;
  }

  if (!options->translate_path.empty() &&
      !HasSuffix(options->translate_path, ".rawheap")) {
    *error = "you should use --translate <xxx.rawheap>";
    return false;
  }

  if (!options->translate_out_path.empty() &&
      !HasSuffix(options->translate_out_path, ".heapsnapshot")) {
    *error = "you should use --translate-out <xxx.heapsnapshot>";
    return false;
  }

  return true;
}

V8FlagResult ParseV8Flags(const char* program_name,
                          const std::vector<char*>& user_args) {
  std::vector<char*> args;
  args.reserve(user_args.size() + 1);
  args.push_back(const_cast<char*>(program_name));
  for (char* arg : user_args) {
    args.push_back(arg);
  }

  int argc = static_cast<int>(args.size());
  std::string usage = BuildUsage(program_name);
  using HelpOptions = i::FlagList::HelpOptions;
  int result = i::FlagList::SetFlagsFromCommandLine(
      &argc, args.data(), true, HelpOptions(HelpOptions::kDontExit, usage.c_str()));
  if (result != 0) {
    return V8FlagResult::kError;
  }

  for (int i = 1; i < argc; ++i) {
    char* remaining = args[i];
    if (remaining == nullptr) {
      continue;
    }
    if (remaining[0] == '-') {
      std::fprintf(stderr, "Error: unrecognized flag %s\n", remaining);
    } else {
      std::fprintf(stderr, "Error: unexpected positional argument %s\n",
                   remaining);
    }
    std::fprintf(stderr, "Try --help for options\n");
    return V8FlagResult::kError;
  }

  if (i::v8_flags.help || i::v8_flags.print_feature_flags_json) {
    return V8FlagResult::kShowHelp;
  }
  return V8FlagResult::kOk;
}

int RunTranslate(const ToolOptions& options) {
#if defined(OH_ENABLE_HEAP_TRANSLATE)
  std::ifstream input_stream(options.translate_path, std::ios::binary);
  if (!input_stream.is_open()) {
    std::fprintf(stderr, "Error: failed to open input file %s\n",
                 options.translate_path.c_str());
    return 1;
  }
  input_stream.close();

  dfx::BinaryReader reader(options.translate_path);
  dfx::SnapshotGenerator generator;
  dfx::HeapTranslator translator(&reader, &generator);
  if (!translator.TranslateHeap()) {
    return 1;
  }

  dfx::SnapshotJSONSerializer serializer(&generator);
  if (!options.translate_out_path.empty()) {
    serializer.SetOutputFile(options.translate_out_path);
  }
  serializer.Serialize();
  return 0;
#else
  std::fprintf(stderr,
               "Error: translate is not enabled in this build "
               "(OH_ENABLE_HEAP_TRANSLATE is off)\n");
  return 1;
#endif
}

}  // namespace

int Main(int argc, char* argv[]) {
#if DEBUG && defined(__linux__)
  InstallCrashSignalHandlers();
#endif
  ToolOptions options;
  std::string parse_error;
  if (!ParseToolArgs(argc, argv, &options, &parse_error)) {
    PrintToolError(parse_error);
    return 1;
  }

  if (options.show_help) {
    options.v8_args.clear();
    options.v8_args.push_back(const_cast<char*>("--help"));
  }

  V8FlagResult flag_result = ParseV8Flags(argv[0], options.v8_args);
  if (flag_result == V8FlagResult::kError) {
    return 1;
  }
  if (flag_result == V8FlagResult::kShowHelp) {
    return 0;
  }

  return RunTranslate(options);
}

}  // namespace translate
}  // namespace internal
}  // namespace v8

int main(int argc, char* argv[]) {
  return v8::internal::translate::Main(argc, argv);
}
