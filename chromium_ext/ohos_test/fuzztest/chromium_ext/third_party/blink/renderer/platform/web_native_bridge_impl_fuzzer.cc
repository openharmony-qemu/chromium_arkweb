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

#include <cstdint>
#include <memory>
#include <utility>

#include "arkweb/chromium_ext/third_party/blink/renderer/platform/web_native_bridge_impl.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "cc/layers/layer.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/public/test/blink_test_environment.h"
#include "media/base/renderer.h"
#include "media/base/renderer_factory.h"
#include "media/base/renderer_factory_selector.h"
#include "third_party/blink/public/platform/media/web_native_delegate.h"
#include "third_party/blink/public/platform/web_native_client.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/media/video_frame_compositor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

class FakeCcLayer : public cc::Layer {
 public:
  FakeCcLayer() : cc::Layer() {}

 private:
  ~FakeCcLayer() override = default;
};

class FakeWebVideoFrameSubmitter : public WebVideoFrameSubmitter {
 public:
  void StopUsingProvider() override {}
  void Initialize(cc::VideoFrameProvider*, bool) override {}
  void SetTransform(media::VideoTransformation) override {}
  void EnableSubmission(viz::SurfaceId) override {}
  void SetIsSurfaceVisible(bool) override {}
  void SetIsPageVisible(bool) override {}
  void SetForceBeginFrames(bool) override {}
  void SetForceSubmit(bool) override {}
  void StartRendering() override {}
  void StopRendering() override {}
  bool IsDrivingFrameUpdates() const override { return true; }
  void DidReceiveFrame() override {}
};

class FakeWebNativeClient final : public WebNativeClient {
 public:
  ~FakeWebNativeClient() = default;
  void Repaint() override {}
  void SetCcLayer(cc::Layer*) override {}
  void OnCreateNativeSurface(int, RectChangeCB) override {}
  void OnLayerRectChange(const gfx::Rect&) override {}
  void OnLayerRectVisibilityChange(bool) override {}
  void CleanupVisibilityForRemovedLayer(bool) override {}
  void OnDestroyNativeSurface() override {}
  void SetStretchContentToFillBounds(bool) override {}
  bool GetStretchContentToFillBounds() override { return stretch_; }
  gfx::ColorSpace TargetColorSpace() override { return gfx::ColorSpace(); }

  void set_stretch(bool s) { stretch_ = s; }

 private:
  bool stretch_ = false;
};

class FakeWebNativeDelegate : public WebNativeDelegate {
 public:
  FakeWebNativeDelegate() : weak_factory_(this) {}
  ~FakeWebNativeDelegate() override = default;

  bool IsFrameHidden() override { return false; }
  int AddObserver(Observer* observer) override {
    last_observer_ = observer;
    return ++next_id_;
  }
  void RemoveObserver(int) override { last_observer_ = nullptr; }

  base::WeakPtr<FakeWebNativeDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  int next_id_ = 0;
  raw_ptr<Observer> last_observer_ = nullptr;
  base::WeakPtrFactory<FakeWebNativeDelegate> weak_factory_;
};

class FakeRendererFactory : public media::RendererFactory {
 public:
  std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>&,
      const scoped_refptr<base::TaskRunner>&,
      media::AudioRendererSink*,
      media::VideoRendererSink*,
      media::RequestOverlayInfoCB,
      const gfx::ColorSpace&) override;
};

namespace {

using RequestSurfaceCB = media::RequestSurfaceCB;
using VideoDecoderChangedCB = media::VideoDecoderChangedCB;
using PipelineStatusCallback = media::PipelineStatusCallback;
using MediaResource = media::MediaResource;
using RendererClient = media::RendererClient;
using RendererType = media::RendererType;

}  // namespace

std::unique_ptr<media::Renderer> FakeRendererFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>&,
    const scoped_refptr<base::TaskRunner>&,
    media::AudioRendererSink*,
    media::VideoRendererSink*,
    media::RequestOverlayInfoCB,
    const gfx::ColorSpace&) {
  class FakeRenderer : public media::Renderer {
   public:
    FakeRenderer() = default;
    ~FakeRenderer() override = default;

    void Initialize(MediaResource*,
                    RendererClient*,
                    RequestSurfaceCB,
                    VideoDecoderChangedCB,
                    PipelineStatusCallback init_cb) override {
      if (init_cb) {
        std::move(init_cb).Run(media::PIPELINE_OK);
      }
    }
    void Flush(base::OnceClosure cb) override {
      if (cb) {
        std::move(cb).Run();
      }
    }
    void StartPlayingFrom(base::TimeDelta) override {}
    void SetPlaybackRate(double) override {}
    void SetVolume(float) override {}
    base::TimeDelta GetMediaTime() override { return base::TimeDelta(); }
    void SetLatencyHint(std::optional<base::TimeDelta>) override {}
    RendererType GetRendererType() override {
      return RendererType::kTest;
    }
  };
  return std::make_unique<FakeRenderer>();
}

class FakeWebSurfaceLayerBridge : public WebSurfaceLayerBridge {
 public:
  FakeWebSurfaceLayerBridge() : frame_sink_id_(1, 1) {
    cc_layer_ = base::MakeRefCounted<FakeCcLayer>();
  }
  ~FakeWebSurfaceLayerBridge() override = default;

  cc::Layer* GetCcLayer() const override { return cc_layer_.get(); }
  const viz::FrameSinkId& GetFrameSinkId() const override {
    return frame_sink_id_;
  }
  const viz::SurfaceId& GetSurfaceId() const override { return surface_id_; }
  void SetContentsOpaque(bool) override {}
  void CreateSurfaceLayer() override {}
  void ClearObserver() override {}
  void RegisterFrameSinkHierarchy() override {}
  void UnregisterFrameSinkHierarchy() override {}

#if BUILDFLAG(ARKWEB_SAME_LAYER)
  scoped_refptr<cc::SurfaceLayer> CreateSurfaceLayer(
      cc::SurfaceLayer::RectChangeCallback,
      cc::SurfaceLayer::RectVisibilityChangeCallback,
      cc::SurfaceLayer::LayerRemovedVisibilityCallback) override {
    return cc::SurfaceLayer::Create();
  }
  void SetStretchContentToFillBounds(bool) override {}
#endif

 private:
  viz::FrameSinkId frame_sink_id_;
  viz::SurfaceId surface_id_;
  scoped_refptr<cc::Layer> cc_layer_;
};

struct GlobalFixture {
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper;
  std::unique_ptr<FakeWebNativeClient> client;
  std::unique_ptr<FakeWebNativeDelegate> delegate;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
};

static std::unique_ptr<GlobalFixture> g_fixture;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();

    auto fixture = std::make_unique<GlobalFixture>();
    fixture->web_view_helper =
        std::make_unique<frame_test_helpers::WebViewHelper>();
    fixture->web_view_helper->Initialize();
    fixture->client = std::make_unique<FakeWebNativeClient>();
    fixture->delegate = std::make_unique<FakeWebNativeDelegate>();
    fixture->task_runner =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    g_fixture = std::move(fixture);
  }
  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

CreateSurfaceLayerBridgeCB MakeBridgeCB() {
  return base::BindOnce(
      [](WebSurfaceLayerBridgeObserver*, cc::UpdateSubmissionStateCB)
          -> std::unique_ptr<WebSurfaceLayerBridge> {
        return std::make_unique<FakeWebSurfaceLayerBridge>();
      });
}

std::unique_ptr<WebNativeBridgeImpl> MakeBridge() {
  WebLocalFrame* frame = g_fixture->web_view_helper->LocalMainFrame();
  auto factory_selector = std::make_unique<media::RendererFactorySelector>();
  factory_selector->AddBaseFactory(media::RendererType::kRendererImpl,
                                   std::make_unique<FakeRendererFactory>());
  auto submitter = std::make_unique<FakeWebVideoFrameSubmitter>();
  auto compositor = std::make_unique<VideoFrameCompositor>(
      g_fixture->task_runner, std::move(submitter));

  return std::make_unique<WebNativeBridgeImpl>(
      frame, g_fixture->client.get(), g_fixture->delegate->GetWeakPtr(),
      std::move(factory_selector), std::move(compositor),
      g_fixture->task_runner, g_fixture->task_runner, MakeBridgeCB());
}

gfx::Rect FuzzRect(FuzzedDataProvider* fdp) {
  int x = fdp->ConsumeIntegralInRange<int>(-1000, 1000);
  int y = fdp->ConsumeIntegralInRange<int>(-1000, 1000);
  int w = fdp->ConsumeIntegralInRange<int>(0, 2000);
  int h = fdp->ConsumeIntegralInRange<int>(0, 2000);
  return gfx::Rect(x, y, w, h);
}

void DrainTasks() { base::RunLoop().RunUntilIdle(); }

// 1. Cover construction + observer/delegate-id wiring + immediate destruction.
//    Forces the DestructionHelper path through the media task runner.
void FuzzConstructDestruct(FuzzedDataProvider* fdp) {
  auto bridge = MakeBridge();
  (void)bridge->GetDelegateId();
  (void)bridge->NaturalSize();
  bridge.reset();
  DrainTasks();
}

// 2. Drive the WebSurfaceLayerBridgeObserver surface methods without
//    activating the surface layer first (bridge_ is null).
void FuzzObserverWithoutBridge(FuzzedDataProvider* fdp) {
  auto bridge = MakeBridge();
  bridge->OnWebLayerUpdated();
  bridge->UnregisterContentsLayer(nullptr);
  bridge->OnSurfaceIdUpdated(viz::SurfaceId());
  bridge->SetStretchContentToFillBounds(fdp->ConsumeBool());
  bridge->UpdateDeviceScaleFactor(fdp->ConsumeFloatingPoint<float>());
  bridge.reset();
  DrainTasks();
}

// 3. Exercise the same-layer activation path: OnSetLayer() builds the bridge,
//    then RegisterContentsLayer / SetStretchContentToFillBounds hit the
//    non-null bridge_ branches.
void FuzzActivateSurfaceLayer(FuzzedDataProvider* fdp) {
  auto bridge = MakeBridge();
  bridge->OnSetLayer();
  // Second call should be a no-op because surface_layer_for_same_layer_ is set.
  if (fdp->ConsumeBool()) {
    bridge->OnSetLayer();
  }
  auto fake_layer = base::MakeRefCounted<FakeCcLayer>();
  bridge->RegisterContentsLayer(fake_layer.get());
  bridge->UnregisterContentsLayer(fake_layer.get());
  bridge->SetStretchContentToFillBounds(fdp->ConsumeBool());
  bridge->UpdateDeviceScaleFactor(fdp->ConsumeFloatingPoint<float>());
  bridge.reset();
  DrainTasks();
}

// 4. Drive layer rect / visibility transitions (each forwards to the
//    WebNativeClient).
void FuzzLayerRectAndVisibility(FuzzedDataProvider* fdp) {
  auto bridge = MakeBridge();
  int ops = fdp->ConsumeIntegralInRange<int>(1, 6);
  for (int i = 0; i < ops && fdp->remaining_bytes() > 4; ++i) {
    int op = fdp->ConsumeIntegralInRange<int>(0, 3);
    switch (op) {
      case 0:
        bridge->OnLayerRectChange(FuzzRect(fdp));
        break;
      case 1:
        bridge->OnLayerRectVisibilityChange(fdp->ConsumeBool());
        break;
      case 2:
        bridge->CleanupVisibilityForRemovedLayer(fdp->ConsumeBool());
        break;
      case 3:
        (void)bridge->NaturalSize();
        break;
    }
  }
  bridge.reset();
  DrainTasks();
}

// 5. NativePipeline::Client overrides — drive OnSetLayer through the
//    NativePipelineController::Start path (which posts to the media runner).
void FuzzStartPipeline(FuzzedDataProvider* fdp) {
  auto bridge = MakeBridge();
  bridge->OnSetLayer();
  DrainTasks();
  if (fdp->ConsumeBool()) {
    bridge->OnFirstFrame(base::TimeTicks::Now(), fdp->ConsumeBool());
  }
  bridge.reset();
  DrainTasks();
}

// 6. Surface lifecycle — OnSurfaceCreated/OnSurfaceDestroyed call into the
//    WebNativeClient with a fuzz-controlled embed id.
void FuzzSurfaceLifecycle(FuzzedDataProvider* fdp) {
  auto bridge = MakeBridge();
  if (fdp->ConsumeBool()) {
    bridge->OnSetLayer();
  }
  int embed_id = fdp->ConsumeIntegralInRange<int>(-1, 1000);
  bridge->OnSurfaceCreated(base::DoNothing(), embed_id);
  if (fdp->ConsumeBool()) {
    bridge->OnLayerRectChange(FuzzRect(fdp));
  }
  bridge->OnSurfaceDestroyed();
  bridge.reset();
  DrainTasks();
}

// 7. CreateRenderer — covers both branches (nullopt + explicit type) of the
//    factory_selector hand-off.
void FuzzCreateRenderer(FuzzedDataProvider* fdp) {
  auto bridge = MakeBridge();
  if (fdp->ConsumeBool()) {
    (void)bridge->CreateRenderer(absl::nullopt);
  } else {
    (void)bridge->CreateRenderer(media::RendererType::kRendererImpl);
  }
  bridge.reset();
  DrainTasks();
}

// 8. Stretch toggling — exercises the bridge_ null/non-null guard branches.
void FuzzStretchToggle(FuzzedDataProvider* fdp) {
  auto bridge = MakeBridge();
  g_fixture->client->set_stretch(fdp->ConsumeBool());
  if (fdp->ConsumeBool()) {
    bridge->OnSetLayer();
  }
  for (int i = 0; i < 4 && fdp->remaining_bytes() > 0; ++i) {
    bridge->SetStretchContentToFillBounds(fdp->ConsumeBool());
    bridge->UpdateDeviceScaleFactor(fdp->ConsumeFloatingPoint<float>());
  }
  bridge.reset();
  DrainTasks();
}

// 9. Freeform mixed sequence covering all observers in random order.
void FuzzFreeform(FuzzedDataProvider* fdp) {
  auto bridge = MakeBridge();
  bool activated = false;
  auto fake_layer = base::MakeRefCounted<FakeCcLayer>();
  int ops = fdp->ConsumeIntegralInRange<int>(1, 12);
  for (int i = 0; i < ops && fdp->remaining_bytes() > 4; ++i) {
    int op = fdp->ConsumeIntegralInRange<int>(0, 10);
    switch (op) {
      case 0:
        if (!activated) {
          bridge->OnSetLayer();
          activated = true;
        }
        break;
      case 1:
        bridge->OnWebLayerUpdated();
        break;
      case 2:
      if (activated) {
        bridge->RegisterContentsLayer(fake_layer.get());
      }
        break;
      case 3:
        bridge->UnregisterContentsLayer(fake_layer.get());
        break;
      case 4:
        bridge->OnSurfaceIdUpdated(viz::SurfaceId());
        break;
      case 5:
        bridge->OnLayerRectChange(FuzzRect(fdp));
        break;
      case 6:
        bridge->OnLayerRectVisibilityChange(fdp->ConsumeBool());
        break;
      case 7:
        bridge->CleanupVisibilityForRemovedLayer(fdp->ConsumeBool());
        break;
      case 8:
        bridge->SetStretchContentToFillBounds(fdp->ConsumeBool());
        break;
      case 9:
        bridge->UpdateDeviceScaleFactor(fdp->ConsumeFloatingPoint<float>());
        break;
      case 10:
        bridge->OnFirstFrame(base::TimeTicks::Now(), fdp->ConsumeBool());
        break;
    }
  }
  bridge.reset();
  DrainTasks();
}

}  // namespace
}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::Environment env;
  if (!blink::g_fixture) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 8);
  switch (selector) {
    case 0:
      blink::FuzzConstructDestruct(&fdp);
      break;
    case 1:
      blink::FuzzObserverWithoutBridge(&fdp);
      break;
    case 2:
      blink::FuzzActivateSurfaceLayer(&fdp);
      break;
    case 3:
      blink::FuzzLayerRectAndVisibility(&fdp);
      break;
    case 4:
      blink::FuzzStartPipeline(&fdp);
      break;
    case 5:
      blink::FuzzSurfaceLifecycle(&fdp);
      break;
    case 6:
      blink::FuzzCreateRenderer(&fdp);
      break;
    case 7:
      blink::FuzzStretchToggle(&fdp);
      break;
    case 8:
      blink::FuzzFreeform(&fdp);
      break;
    default:
      blink::FuzzFreeform(&fdp);
      break;
  }
  return 0;
}
