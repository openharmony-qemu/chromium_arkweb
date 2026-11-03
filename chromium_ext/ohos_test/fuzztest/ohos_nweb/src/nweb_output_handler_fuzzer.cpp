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

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "nweb_output_handler.h"

using namespace OHOS::NWeb;

namespace {

auto MakeCallback() {
  return [](const char* data, uint32_t width, uint32_t height) -> bool {
    return data != nullptr && width > 0 && height > 0;
  };
}
std::shared_ptr<NWebOutputHandler> g_handler = nullptr;
void EnsureGlobalHandler() {
  if(g_handler) {
      return;
  }
  g_handler = NWebOutputHandler::Create(10, 10, MakeCallback());
}

void FuzzCreateAndAccessors(FuzzedDataProvider* fdp) {
  // Cover the nullptr-callback early-return branch in Create().
  auto null_handler = NWebOutputHandler::Create(
    fdp->ConsumeIntegralInRange<uint32_t>(0, 32),
    fdp->ConsumeIntegralInRange<uint32_t>(0, 32),
    nullptr);
  (void)null_handler;

  EnsureGlobalHandler();
  if (!g_handler) {
    return;
  }

  uint32_t out_w = 0;
  uint32_t out_h = 0;
  g_handler->GetWindowInfo(out_w, out_h);

  g_handler->SetNWebId(fdp->ConsumeIntegral<uint32_t>());
  (void)g_handler->IsSizeValid();
}

void FuzzResize(FuzzedDataProvider* fdp) {
  auto handler = std::make_shared<NWebOutputHandler>(MakeCallback());
  // Multiple resizes to exercise the size-change branch and dump_buf realloc.
  const int kIters = fdp->ConsumeIntegralInRange<int>(1, 4);
  for (int i = 0; i < kIters; ++i) {
    uint32_t w = fdp->ConsumeIntegralInRange<uint32_t>(0, 32);
    uint32_t h = fdp->ConsumeIntegralInRange<uint32_t>(0, 32);
    handler->Resize(w, h);
  }
  handler->Init(fdp->ConsumeIntegralInRange<uint32_t>(0, 32),
                    fdp->ConsumeIntegralInRange<uint32_t>(0, 32));
  (void)handler->IsSizeValid();
}

void FuzzDumpPathFlow(FuzzedDataProvider* fdp) {
  // Drives StartDumpToFile() (private) via SetDumpPath() + StartRenderOutput().
  EnsureGlobalHandler();
  if (!g_handler) {
    return;
  }
  std::string path = fdp->ConsumeRandomLengthString(32);
  g_handler->SetDumpPath(path);
  g_handler->StartRenderOutput();
  // Clear the path so the detached dump thread exits its loop promptly.
  g_handler->SetDumpPath("");
}

void FuzzFrameStatFlow(FuzzedDataProvider* fdp) {
  // Drives StartFrameStat() (private path via SetFrameInfoDump + StartRenderOutput).
  EnsureGlobalHandler();
  if (!g_handler) {
    return;
  }
  g_handler->SetDumpPath("");
  g_handler->SetFrameInfoDump(fdp->ConsumeBool());
  g_handler->StartRenderOutput();
  // Disable so detached frame-stat thread exits promptly.
  g_handler->SetFrameInfoDump(false);
}

void FuzzNativeWindow(FuzzedDataProvider* fdp) {
  auto handler = NWebOutputHandler::Create(
    fdp->ConsumeIntegralInRange<uint32_t>(1, 8),
    fdp->ConsumeIntegralInRange<uint32_t>(1, 8),
    MakeCallback());
  if (!handler) {
    return;
  }
  // Pass nullptr surface (the only safe sentinel for a fuzzer).
  (void)handler->GetNativeWindowFromSurface(nullptr);
  // Trigger another resize after window assignment for is_initialized_resize_.
  handler->Resize(fdp->ConsumeIntegralInRange<uint32_t>(1, 16),
                fdp->ConsumeIntegralInRange<uint32_t>(1, 16));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(uint32_t)) {
    return 0;
  }
  FuzzedDataProvider fdp(data, size);
  FuzzCreateAndAccessors(&fdp);
  FuzzResize(&fdp);
  FuzzFrameStatFlow(&fdp);
  FuzzDumpPathFlow(&fdp);
  FuzzNativeWindow(&fdp);
  // Give detached worker threads a brief chance to observe state-clearing
  // signals before this iteration's handler is destructed.
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  return 0;
}