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

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <string>

#include "base/at_exit.h"
#include "base/test/task_environment.h"
#include "base/memory/writable_shared_memory_region.h"
#include "cc/mojo_embedder/software_compositor_renderer_ohos.h"
#include "arkweb/chromium_ext/third_party/blink/public/mojom/input/software_compositor_ohos.mojom-blink.h"
#include "third_party/blink/renderer/platform/widget/input/software_compositor_proxy_ohos.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"

namespace {

constexpr int32_t kMaxDim = 4096;
constexpr int32_t kMinDim = -4096;
constexpr float kMaxFloat = 4096.0f;
constexpr float kMinFloat = -4096.0f;
constexpr size_t kShmSize = 1024 * 1024;

class FakeSoftwareCompositorRendererOhos
    : public cc::mojo_embedder::SoftwareCompositorRendererOhos {
 public:
  FakeSoftwareCompositorRendererOhos()
      : SoftwareCompositorRendererOhos(nullptr, nullptr) {}

  bool DemandDrawSw(SkCanvas* canvas, gfx::SizeF size, gfx::PointF offset) override {
    return true;
  }

  void DrawRect(const gfx::Rect& rect) override {}
};

struct Environment {
  Environment()
      : at_exit_manager(),
        task_environment(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO) {}
  base::AtExitManager at_exit_manager;
  base::test::SingleThreadTaskEnvironment task_environment;
};

void FuzzDemandDrawSwAsync(FuzzedDataProvider& fdp,
                           blink::SoftwareCompositorProxyOhos& proxy) {
  float sw = fdp.ConsumeFloatingPointInRange<float>(kMinFloat, kMaxFloat);
  float sh = fdp.ConsumeFloatingPointInRange<float>(kMinFloat, kMaxFloat);
  float ox = fdp.ConsumeFloatingPointInRange<float>(kMinFloat, kMaxFloat);
  float oy = fdp.ConsumeFloatingPointInRange<float>(kMinFloat, kMaxFloat);

  auto params =
      blink::mojom::blink::SoftwareCompositorDemandDrawSwParams::New();
  params->size = gfx::SizeF(sw, sh);
  params->offset = gfx::PointF(ox, oy);

  bool callback_invoked = false;
  proxy.DemandDrawSwAsync(
      std::move(params),
      base::BindOnce(
          [](bool* invoked, bool success) { *invoked = true; },
          base::Unretained(&callback_invoked)));
}

void FuzzDrawRect(FuzzedDataProvider& fdp,
                  blink::SoftwareCompositorProxyOhos& proxy) {
  int32_t x = fdp.ConsumeIntegralInRange<int32_t>(kMinDim, kMaxDim);
  int32_t y = fdp.ConsumeIntegralInRange<int32_t>(kMinDim, kMaxDim);
  int32_t w = fdp.ConsumeIntegralInRange<int32_t>(kMinDim, kMaxDim);
  int32_t h = fdp.ConsumeIntegralInRange<int32_t>(kMinDim, kMaxDim);
  proxy.DrawRect(gfx::Rect(x, y, w, h));
}

void FuzzSetSharedMemory(FuzzedDataProvider& fdp,
                         blink::SoftwareCompositorProxyOhos& proxy) {
  bool use_valid = fdp.ConsumeBool();
  bool callback_invoked = false;

  if (use_valid) {
    auto region = base::WritableSharedMemoryRegion::Create(kShmSize);
    proxy.SetSharedMemory(
        std::move(region),
        base::BindOnce(
            [](bool* invoked, bool success) { *invoked = true; },
            base::Unretained(&callback_invoked)));
  } else {
    proxy.SetSharedMemory(
        base::WritableSharedMemoryRegion(),
        base::BindOnce(
            [](bool* invoked, bool success) { *invoked = true; },
            base::Unretained(&callback_invoked)));
  }
}

void FuzzDemandDrawWithoutShm(FuzzedDataProvider& fdp,
                              blink::SoftwareCompositorProxyOhos& proxy) {
  float sw = fdp.ConsumeFloatingPointInRange<float>(0.0f, kMaxFloat);
  float sh = fdp.ConsumeFloatingPointInRange<float>(0.0f, kMaxFloat);

  auto params =
      blink::mojom::blink::SoftwareCompositorDemandDrawSwParams::New();
  params->size = gfx::SizeF(sw, sh);
  params->offset = gfx::PointF(0.0f, 0.0f);

  proxy.DemandDrawSwAsync(
      std::move(params),
      base::BindOnce([](bool success) {}));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  if (!data || size < sizeof(int32_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  blink::SoftwareCompositorProxyOhos proxy;
  FakeSoftwareCompositorRendererOhos renderer;
  proxy.SetSoftwareRenderer(&renderer);

  uint8_t setup = fdp.ConsumeIntegral<uint8_t>();
  auto region = base::WritableSharedMemoryRegion::Create(kShmSize);
  proxy.SetSharedMemory(std::move(region),
                          base::BindOnce([](bool success) {}));

  while (fdp.remaining_bytes() >= 8) {
    uint8_t op = fdp.ConsumeIntegral<uint8_t>();
    switch (op % 4) {
      case 0:
        FuzzDemandDrawSwAsync(fdp, proxy);
        break;
      case 1:
        FuzzDrawRect(fdp, proxy);
        break;
      case 2:
        FuzzSetSharedMemory(fdp, proxy);
        break;
      case 3:
        FuzzDemandDrawWithoutShm(fdp, proxy);
        break;
    }
  }

  return 0;
}
