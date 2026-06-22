/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include <fuzzer/FuzzedDataProvider.h>
#include <cstdint>
#include <memory>

#include "arkweb/chromium_ext/content/renderer/render_remote_proxy_ohos.h"
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#include "gpu/ipc/common/nweb_native_window_tracker.h"

namespace {

class MockClientAdapter : public OHOS::NWeb::AafwkBrowserClientAdapter {
 public:
  void* QueryRenderSurface(int32_t surface_id) override { return this; }
  void ReportThread(OHOS::NWeb::ResSchedStatusAdapter status,
                    int32_t process_id,
                    int32_t thread_id,
                    OHOS::NWeb::ResSchedRoleAdapter role) override {}
  void PassSurface(int64_t surface_id) override {}
  void DestroyRenderSurface(int32_t surface_id) override {}
};

void FuzzNotifyBrowserFd(FuzzedDataProvider* fdp) {
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  base::GlobalDescriptors::Mapping mapping;
  g_fds->Reset(mapping);

  auto proxy = std::make_shared<content::RenderRemoteProxy>();
  int ipc_fd = fdp->ConsumeIntegral<int>();
  int shared_fd = fdp->ConsumeIntegral<int>();
  int crash_fd = fdp->ConsumeIntegral<int>();

  proxy->NotifyBrowserFd(ipc_fd, shared_fd, crash_fd);
}

void FuzzNotifyBrowser(FuzzedDataProvider* fdp) {
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  base::GlobalDescriptors::Mapping mapping;
  g_fds->Reset(mapping);

  auto proxy = std::make_shared<content::RenderRemoteProxy>();
  int ipc_fd = fdp->ConsumeIntegral<int>();
  int shared_fd = fdp->ConsumeIntegral<int>();
  int crash_fd = fdp->ConsumeIntegral<int>();

  std::shared_ptr<OHOS::NWeb::AafwkBrowserClientAdapter> adapter = nullptr;
  if (fdp->ConsumeBool()) {
    adapter = std::make_shared<MockClientAdapter>();
  }

  proxy->NotifyBrowser(ipc_fd, shared_fd, crash_fd, adapter);
}

void FuzzWaitForBrowserFd(FuzzedDataProvider* fdp) {
  auto proxy = std::make_shared<content::RenderRemoteProxy>();
  proxy->WaitForBrowserFd();
}

void FuzzNegativeFds(FuzzedDataProvider* fdp) {
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  base::GlobalDescriptors::Mapping mapping;
  g_fds->Reset(mapping);

  auto proxy = std::make_shared<content::RenderRemoteProxy>();
  int neg_fds[] = {-1, -100, INT32_MIN};

  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, 2);
  proxy->NotifyBrowserFd(neg_fds[idx], neg_fds[idx], neg_fds[idx]);
}

void FuzzZeroFds(FuzzedDataProvider* fdp) {
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  base::GlobalDescriptors::Mapping mapping;
  g_fds->Reset(mapping);

  auto proxy = std::make_shared<content::RenderRemoteProxy>();
  proxy->NotifyBrowserFd(0, 0, 0);
  proxy->NotifyBrowser(0, 0, 0, nullptr);
}

void FuzzNotifyBrowserWithAdapter(FuzzedDataProvider* fdp) {
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  base::GlobalDescriptors::Mapping mapping;
  g_fds->Reset(mapping);

  auto proxy = std::make_shared<content::RenderRemoteProxy>();
  auto adapter = std::make_shared<MockClientAdapter>();

  int ipc_fd = fdp->ConsumeIntegralInRange<int>(0, 100);
  int shared_fd = fdp->ConsumeIntegralInRange<int>(0, 100);
  int crash_fd = fdp->ConsumeIntegralInRange<int>(0, 100);

  proxy->NotifyBrowser(ipc_fd, shared_fd, crash_fd, adapter);
}

void FuzzAllMethods(FuzzedDataProvider* fdp) {
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  base::GlobalDescriptors::Mapping mapping;
  g_fds->Reset(mapping);

  auto proxy = std::make_shared<content::RenderRemoteProxy>();
  int ipc_fd = fdp->ConsumeIntegral<int>();
  int shared_fd = fdp->ConsumeIntegral<int>();
  int crash_fd = fdp->ConsumeIntegral<int>();

  proxy->NotifyBrowserFd(ipc_fd, shared_fd, crash_fd);

  g_fds->Reset(mapping);
  auto adapter = std::make_shared<MockClientAdapter>();
  proxy->NotifyBrowser(ipc_fd, shared_fd, crash_fd, adapter);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!data || size < sizeof(int32_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  auto proxy = std::make_shared<content::RenderRemoteProxy>();
  uint32_t selector = fdp.ConsumeIntegralInRange<uint32_t>(0, 7);

  switch (selector) {
    case 0:
      FuzzNotifyBrowserFd(&fdp);
      break;
    case 1:
      FuzzNotifyBrowser(&fdp);
      break;
    case 2:
      FuzzWaitForBrowserFd(&fdp);
      break;
    case 3:
      FuzzNegativeFds(&fdp);
      break;
    case 4:
      FuzzZeroFds(&fdp);
      break;
    case 5:
      FuzzNotifyBrowserWithAdapter(&fdp);
      break;
    case 6:
      FuzzAllMethods(&fdp);
      break;
    default:
      FuzzNotifyBrowserFd(&fdp);
      break;
  }

  return 0;
}
