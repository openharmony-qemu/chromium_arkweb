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

#include <fuzzer/FuzzedDataProvider.h>

#include "arkweb/chromium_ext/content/browser/gpu/ark_web_compositor_util.h"
#include "arkweb/chromium_ext/content/public/common/content_switches_ext.h"
#include "base/command_line.h"
#include "cc/base/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>
#include <string>

namespace content {

class CommandLineEnvironment {
 public:
  CommandLineEnvironment() {
    base::CommandLine::Init(0, nullptr);
  }

  ~CommandLineEnvironment() = default;

  void ResetCommandLine() {
    base::CommandLine::Reset();
    base::CommandLine::Init(0, nullptr);
  }

  void SetSwitch(const std::string& switch_name) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switch_name);
  }

  void SetSwitchWithValue(const std::string& switch_name,
                          const std::string& value) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(switch_name, value);
  }

  bool HasSwitch(const std::string& switch_name) {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name);
  }

  void RemoveSwitch(const std::string& switch_name) {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(switch_name);
  }
};

void FuzzWithNoSwitches(FuzzedDataProvider* fdp, CommandLineEnvironment* env) {
  env->ResetCommandLine();
  
  auto result = ArkWebNumberOfRendererRasterThreads();
  (void)result;
}

void FuzzWithEnableNwebExOnly(FuzzedDataProvider* fdp,
                              CommandLineEnvironment* env) {
  env->ResetCommandLine();
  env->SetSwitch(switches::kEnableNwebEx);
  
  auto result = ArkWebNumberOfRendererRasterThreads();
  (void)result;
}

void FuzzWithNumRasterThreadsOnly(FuzzedDataProvider* fdp,
                                  CommandLineEnvironment* env) {
  env->ResetCommandLine();
  
  std::string thread_count = std::to_string(fdp->ConsumeIntegralInRange<int>(1, 16));
  env->SetSwitchWithValue(switches::kNumRasterThreads, thread_count);
  
  auto result = ArkWebNumberOfRendererRasterThreads();
  (void)result;
}

void FuzzWithBothSwitches(FuzzedDataProvider* fdp, CommandLineEnvironment* env) {
  env->ResetCommandLine();
  
  env->SetSwitch(switches::kEnableNwebEx);
  
  std::string thread_count = std::to_string(fdp->ConsumeIntegralInRange<int>(1, 16));
  env->SetSwitchWithValue(switches::kNumRasterThreads, thread_count);
  
  auto result = ArkWebNumberOfRendererRasterThreads();
  (void)result;
}

void FuzzWithBoundaryValues(FuzzedDataProvider* fdp, CommandLineEnvironment* env) {
  env->ResetCommandLine();
  
  int scenario = fdp->ConsumeIntegralInRange<int>(0, 5);
  
  switch (scenario) {
    case 0:
      env->SetSwitch(switches::kEnableNwebEx);
      env->SetSwitchWithValue(switches::kNumRasterThreads, "1");
      break;
    case 1:
      env->SetSwitch(switches::kEnableNwebEx);
      env->SetSwitchWithValue(switches::kNumRasterThreads, "16");
      break;
    case 2:
      env->SetSwitch(switches::kEnableNwebEx);
      env->SetSwitchWithValue(switches::kNumRasterThreads, "0");
      break;
    case 3:
      env->SetSwitch(switches::kEnableNwebEx);
      env->SetSwitchWithValue(switches::kNumRasterThreads, "-1");
      break;
    case 4:
      env->SetSwitch(switches::kEnableNwebEx);
      env->SetSwitchWithValue(switches::kNumRasterThreads, "999999");
      break;
    case 5:
      env->SetSwitch(switches::kEnableNwebEx);
      env->SetSwitchWithValue(switches::kNumRasterThreads, 
                              fdp->ConsumeRandomLengthString(64));
      break;
    default:
      break;
  }
  
  auto result = ArkWebNumberOfRendererRasterThreads();
  (void)result;
}

void FuzzWithRandomSwitchCombinations(FuzzedDataProvider* fdp,
                                      CommandLineEnvironment* env) {
  env->ResetCommandLine();
  
  bool enable_nweb_ex = fdp->ConsumeBool();
  bool enable_num_raster = fdp->ConsumeBool();
  
  if (enable_nweb_ex) {
    env->SetSwitch(switches::kEnableNwebEx);
  }
  
  if (enable_num_raster) {
    std::string thread_count = std::to_string(fdp->ConsumeIntegral<int>());
    env->SetSwitchWithValue(switches::kNumRasterThreads, thread_count);
  }
  
  auto result = ArkWebNumberOfRendererRasterThreads();
  (void)result;
}

void FuzzWithInvalidInputs(FuzzedDataProvider* fdp, CommandLineEnvironment* env) {
  env->ResetCommandLine();
  
  env->SetSwitch(switches::kEnableNwebEx);
  
  int invalid_scenario = fdp->ConsumeIntegralInRange<int>(0, 3);
  
  switch (invalid_scenario) {
    case 0:
      env->SetSwitchWithValue(switches::kNumRasterThreads, "");
      break;
    case 1:
      env->SetSwitchWithValue(switches::kNumRasterThreads, "abc");
      break;
    case 2:
      env->SetSwitchWithValue(switches::kNumRasterThreads, "1.5");
      break;
    case 3:
      env->SetSwitchWithValue(switches::kNumRasterThreads, 
                              fdp->ConsumeRandomLengthString(256));
      break;
    default:
      break;
  }
  
  auto result = ArkWebNumberOfRendererRasterThreads();
  (void)result;
}

void FuzzStressTest(FuzzedDataProvider* fdp, CommandLineEnvironment* env) {
  int iterations = fdp->ConsumeIntegralInRange<int>(1, 10);
  
  for (int i = 0; i < iterations; ++i) {
    env->ResetCommandLine();
    
    if (fdp->ConsumeBool()) {
      env->SetSwitch(switches::kEnableNwebEx);
    }
    
    if (fdp->ConsumeBool()) {
      std::string thread_count = std::to_string(fdp->ConsumeIntegralInRange<int>(-100, 100));
      env->SetSwitchWithValue(switches::kNumRasterThreads, thread_count);
    }
    
    auto result = ArkWebNumberOfRendererRasterThreads();
    (void)result;
  }
}

}  // namespace content

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(int32_t)) {
    return 0;
  }

  static content::CommandLineEnvironment env;
  
  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 6);

  switch (selector) {
    case 0:
      content::FuzzWithNoSwitches(&fdp, &env);
      break;
    case 1:
      content::FuzzWithEnableNwebExOnly(&fdp, &env);
      break;
    case 2:
      content::FuzzWithNumRasterThreadsOnly(&fdp, &env);
      break;
    case 3:
      content::FuzzWithBothSwitches(&fdp, &env);
      break;
    case 4:
      content::FuzzWithBoundaryValues(&fdp, &env);
      break;
    case 5:
      content::FuzzWithRandomSwitchCombinations(&fdp, &env);
      break;
    case 6:
      content::FuzzWithInvalidInputs(&fdp, &env);
      break;
    case 7:
      content::FuzzStressTest(&fdp, &env);
      break;
    default:
      content::FuzzWithNoSwitches(&fdp, &env);
      break;
  }

  return 0;
}
