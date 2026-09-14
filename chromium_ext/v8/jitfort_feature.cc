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

// This file is a separate compilation unit that ONLY includes Chromium base
// headers (no V8 headers). V8 base and Chromium base define conflicting macros
// (PRINTF_FORMAT, CHECK, DCHECK, DISABLE_CFI_PERF, DISABLE_CFI_ICALL, ...) so
// they cannot coexist in the same translation unit. Keeping IsJitFortEnabled()
// here allows jitfort.cc to call it via the header without pulling base headers
// into the V8 compilation unit.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "jitfort_feature.h"

// JitFort is enabled by default. Disable via --disable-features=JitFort.
bool IsJitFortEnabled() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  std::string disabled = cmd->GetSwitchValueASCII("disable-features");
  LOG(INFO) << "jitfort disable-features:" << disabled;
  auto features = base::SplitString(disabled, ",", base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_NONEMPTY);
  for (const auto& f : features) {
    if (f == "JitFort") {
      LOG(INFO) << "jitfort enable: false (disabled by --disable-features=JitFort)";
      return false;
    }
  }
  LOG(INFO) << "jitfort enable: true";
  return true;
}
