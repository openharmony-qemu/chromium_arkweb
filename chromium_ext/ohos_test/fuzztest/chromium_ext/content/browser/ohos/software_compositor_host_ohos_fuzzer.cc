/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <cmath>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "mojo/core/embedder/embedder.h"

#ifdef BUILDFLAG_INTERNAL_ARKWEB_TEST
#undef BUILDFLAG_INTERNAL_ARKWEB_TEST
#endif
#define BUILDFLAG_INTERNAL_ARKWEB_TEST() (1)

#include "content/browser/ohos/software_compositor_host_ohos.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

namespace content {

class SoftwareCompositorHostOhosTest {
 public:
  static gfx::SizeF& GetCurrentSize(SoftwareCompositorHostOhos* host) {
    return host->current_;
  }
  static bool HasValidShm(SoftwareCompositorHostOhos* host) {
    return host->software_draw_shm_ != nullptr;
  }
};

namespace {

constexpr char kFuzzId[] = "fuzz_id";
constexpr size_t kMaxStringLen = 256;

void FuzzCallback(const char*, bool, void*, int, int) {}

class FuzzerEnv {
 public:
  FuzzerEnv()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::IO),
        content_client_initializer_() {
    test_enabler_ = std::make_unique<RenderViewHostTestEnabler>();
    browser_context_ = std::make_unique<TestBrowserContext>();
    ui::DeviceDataManager::CreateInstance();
    aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>(
        ImageTransportFactory::GetInstance()->GetContextFactory());
    aura_test_helper_->SetUp();
    auto site_instance = SiteInstance::Create(browser_context_.get());
    if (!site_instance) {
      return;
    }
    contents_ = TestWebContents::Create(browser_context_.get(),
                                        site_instance);
    if (!contents_) {
      return;
    }
    contents_->NavigateAndCommit(GURL("about:blank"));
    auto* rvh = contents_->GetRenderViewHost();
    if (!rvh) {
      return;
    }
    widget_ = rvh->GetWidget();
  }

  RenderWidgetHostImpl* widget() { return widget_; }
  bool valid() const { return widget_ != nullptr; }

 private:
  struct CommandLineInitializer {
    CommandLineInitializer() { base::CommandLine::Init(0, nullptr); }
  };
  struct MojoInitializer {
    MojoInitializer() {
      ForceCreateNetworkServiceDirectlyForTesting();
      StoragePartitionImpl::ForceInProcessStorageServiceForTesting();
      mojo::core::Init(
          mojo::core::Configuration{.is_broker_process = true});
    }
  };
  CommandLineInitializer cmdline_init_;
  MojoInitializer mojo_init_;
  BrowserTaskEnvironment task_environment_;
  TestContentClientInitializer content_client_initializer_;
  std::unique_ptr<RenderViewHostTestEnabler> test_enabler_;
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> contents_;
  RenderWidgetHostImpl* widget_ = nullptr;
};

void FuzzDemandDrawSwAsync(FuzzedDataProvider& fdp,
                           SoftwareCompositorHostOhos* host) {
  uint8_t clip_mode = fdp.ConsumeIntegralInRange<uint8_t>(0, 3);
  float clip_w = fdp.ConsumeFloatingPoint<float>();
  float clip_h = fdp.ConsumeFloatingPoint<float>();
  float sw = fdp.ConsumeFloatingPoint<float>();
  float sh = fdp.ConsumeFloatingPoint<float>();
  float ox = fdp.ConsumeFloatingPoint<float>();
  float oy = fdp.ConsumeFloatingPoint<float>();

  float final_clip_w = clip_w;
  float final_clip_h = clip_h;
  switch (clip_mode) {
    case 0:
      final_clip_w = 0.0f;
      break;
    case 1:
      final_clip_h = 0.0f;
      break;
    case 2:
      final_clip_w = 0.0f;
      final_clip_h = 0.0f;
      break;
    default:
      break;
  }

  gfx::SizeF size(std::fabs(sw) + 1.0f, std::fabs(sh) + 1.0f);
  if (final_clip_w < 0.0f) final_clip_w = 0.0f;
  if (final_clip_h < 0.0f) final_clip_h = 0.0f;

  host->DemandDrawSwAsync(
      kFuzzId, final_clip_w, final_clip_h,
      size, gfx::PointF(ox, oy),
      base::BindOnce(&FuzzCallback));
  base::RunLoop().RunUntilIdle();
}

void FuzzDemandDrawSwAsyncOverflow(FuzzedDataProvider& fdp,
                                   SoftwareCompositorHostOhos* host) {
  uint8_t mode = fdp.ConsumeIntegral<uint8_t>();
  float dim = (mode % 2 == 0) ? 100000.0f : 99999.0f;
  float clip_w = dim;
  float clip_h = dim;
  gfx::SizeF size(dim, dim);

  host->DemandDrawSwAsync(
      kFuzzId, clip_w, clip_h, size, gfx::PointF(0.0f, 0.0f),
      base::BindOnce(&FuzzCallback));
  base::RunLoop().RunUntilIdle();
}

void FuzzDrawRect(FuzzedDataProvider& fdp,
                  SoftwareCompositorHostOhos* host) {
  int32_t x = fdp.ConsumeIntegral<int32_t>();
  int32_t y = fdp.ConsumeIntegral<int32_t>();
  int32_t w = fdp.ConsumeIntegral<int32_t>();
  int32_t h = fdp.ConsumeIntegral<int32_t>();
  host->DrawRect(gfx::Rect(x, y, w, h));
}

void FuzzOnDrawSwCallbackFalse(SoftwareCompositorHostOhos* host) {
  host->OnDrawSwCallback(
      base::BindOnce(&FuzzCallback),
      std::string(kFuzzId), false);
  base::RunLoop().RunUntilIdle();
}

void FuzzOnDrawSwCallbackTrue(SoftwareCompositorHostOhos* host) {
  if (!SoftwareCompositorHostOhosTest::HasValidShm(host)) {
    return;
  }
  host->OnDrawSwCallback(
      base::BindOnce(&FuzzCallback),
      std::string(kFuzzId), true);
  base::RunLoop().RunUntilIdle();
}

void FuzzSetSharedMemory(FuzzedDataProvider& fdp,
                         SoftwareCompositorHostOhos* host) {
  uint8_t mode = fdp.ConsumeIntegral<uint8_t>();
  size_t stride;
  size_t buf_size;
  switch (mode % 3) {
    case 0:
      stride = 0;
      buf_size = 0;
      break;
    case 1:
      stride = fdp.ConsumeIntegralInRange<size_t>(1, 65536);
      buf_size = fdp.ConsumeIntegralInRange<size_t>(1, 4 * 1024 * 1024);
      break;
    default:
      stride = fdp.ConsumeIntegralInRange<size_t>(1, 256);
      buf_size = fdp.ConsumeIntegralInRange<size_t>(1, 1024);
      break;
  }
  host->SetSharedMemory(stride, buf_size);
  base::RunLoop().RunUntilIdle();
}

constexpr int kNumCases = 6;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static base::AtExitManager* at_exit = new base::AtExitManager();
  static FuzzerEnv* env = new FuzzerEnv();

  if (!data || size == 0) {
    return 0;
  }

  if (!env->valid()) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  auto host = std::make_unique<SoftwareCompositorHostOhos>(env->widget());
  if (!host) {
    return 0;
  }
  base::RunLoop().RunUntilIdle();

  uint8_t selector = fdp.ConsumeIntegral<uint8_t>();
  switch (selector % kNumCases) {
    case 0:
      FuzzDemandDrawSwAsync(fdp, host.get());
      break;
    case 1:
      FuzzDemandDrawSwAsyncOverflow(fdp, host.get());
      break;
    case 2:
      FuzzDrawRect(fdp, host.get());
      break;
    case 3:
      FuzzOnDrawSwCallbackFalse(host.get());
      break;
    case 4:
      FuzzOnDrawSwCallbackTrue(host.get());
      break;
    case 5:
      FuzzSetSharedMemory(fdp, host.get());
      break;
  }

  return 0;
}

}  // namespace content
