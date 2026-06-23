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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/loader/native_loader.h"
#include "base/command_line.h"
#include "cc/layers/layer.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_native.h"
#include "third_party/blink/renderer/core/paint/native_painter.h"
#include "third_party/blink/renderer/core/paint/paint_flags.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/web_native_bridge.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

// Concrete stub for the abstract WebNativeBridge so GetWebNativeBridge()
// returns non-null and PaintReplaced can pass the early-return gate.
class StubWebNativeBridge : public WebNativeBridge {
 public:
  StubWebNativeBridge() = default;
  ~StubWebNativeBridge() override = default;
  void StartPipeline() override {}
  void OnLayerRectChange(const gfx::Rect&) override {}
  void OnLayerRectVisibilityChange(bool) override {}
  void CleanupVisibilityForRemovedLayer(bool) override {}
  gfx::Size NaturalSize() const override { return gfx::Size(); }
  void SetStretchContentToFillBounds(bool) override {}
  void UpdateDeviceScaleFactor(float) override {}
};

// Test-controlled NativeLoader exposing settable cc::Layer and bridge.
// Mirrors the pattern used in native_painter_unittest.cc.
class TestNativeLoader final : public NativeLoader {
 public:
  explicit TestNativeLoader(HTMLPlugInElement* plugin) : NativeLoader(plugin) {}

  cc::Layer* CcLayer() const { return cc_layer_.get(); }
  WebNativeBridge* GetWebNativeBridge() { return bridge_.get(); }
  String DebugName() const override { return "TestNativeLoader"; }

  void SetCcLayerForFuzz(scoped_refptr<cc::Layer> layer) {
    cc_layer_ = std::move(layer);
  }
  void SetBridgeForFuzz(std::unique_ptr<WebNativeBridge> bridge) {
    bridge_ = std::move(bridge);
  }

 private:
  scoped_refptr<cc::Layer> cc_layer_;
  std::unique_ptr<WebNativeBridge> bridge_;
};

// HTMLPlugInElement subclass returning a controllable TestNativeLoader.
class TestHTMLPlugInElement final : public HTMLPlugInElement {
 public:
  explicit TestHTMLPlugInElement(Document& doc)
      : HTMLPlugInElement(html_names::kEmbedTag, doc, CreateElementFlags()) {}

  TestNativeLoader* NativeLoader() const { return loader_.Get(); }
  void SetTestNativeLoader(TestNativeLoader* loader) { loader_ = loader; }

  bool HasFallbackContent() const override { return false; }
  bool UseFallbackContent() const override { return false; }
  LayoutEmbeddedContent* LayoutEmbeddedContentForJSBindings() const override {
    return nullptr;
  }
  LayoutEmbeddedContent* ExistingLayoutEmbeddedContent() const override {
    return nullptr;
  }
  void UpdatePluginInternal() override {}
  FrameOwnerElementType OwnerType() const override {
    return FrameOwnerElementType::kNone;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    HTMLPlugInElement::Trace(visitor);
  }

 private:
  Member<TestNativeLoader> loader_;
};

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }
  ~Environment() { page_holder.reset(); }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
  std::unique_ptr<DummyPageHolder> page_holder;
};

Environment& GetEnv() {
  static Environment env;
  return env;
}

constexpr PaintPhase kAllPhases[] = {
    PaintPhase::kBlockBackground,
    PaintPhase::kDescendantBlockBackgroundsOnly,
    PaintPhase::kFloat,
    PaintPhase::kForeground,
    PaintPhase::kOutline,
    PaintPhase::kSelectionDragImage,
    PaintPhase::kDescendantOutlinesOnly,
    PaintPhase::kMask,
    PaintPhase::kOverlayOverflowControls,
};

constexpr Document::PaintPreviewState kPreviewStates[] = {
    Document::kNotPaintingPreview,
    Document::kPaintingPreview,
    Document::kPaintingPreviewSkipAcceleratedContent,
};

// Fixture per fuzzer iteration: full plugin/loader chain, sized layout.
struct PaintFixture {
  Persistent<TestHTMLPlugInElement> plugin;
  Persistent<TestNativeLoader> loader;
  Persistent<LayoutNative> layout_native;

  PaintFixture(Document& doc,
               bool attach_loader,
               bool attach_bridge,
               bool attach_cc_layer) {
    plugin = MakeGarbageCollected<TestHTMLPlugInElement>(doc);
    doc.body()->AppendChild(plugin);
    layout_native = MakeGarbageCollected<LayoutNative>(plugin);
    if (attach_loader) {
      loader = MakeGarbageCollected<TestNativeLoader>(plugin);
      if (attach_bridge) {
        loader->SetBridgeForFuzz(std::make_unique<StubWebNativeBridge>());
      }
      if (attach_cc_layer) {
        loader->SetCcLayerForFuzz(cc::Layer::Create());
      }
      plugin->SetTestNativeLoader(loader);
    }
  }

  ~PaintFixture() {
    if (plugin && plugin->parentNode()) {
      plugin->remove();
    }
  }
};

PhysicalOffset FuzzOffset(FuzzedDataProvider* fdp) {
  // Mix small / extreme / pixel-snapped boundary values.
  static constexpr int32_t kSpecial[] = {
      0, 1, -1, 8, -8, 800, -800, 32767, -32767,
  };
  int32_t x = fdp->ConsumeBool()
                  ? kSpecial[fdp->ConsumeIntegralInRange<size_t>(
                        0, std::size(kSpecial) - 1)]
                  : fdp->ConsumeIntegralInRange<int32_t>(-10000, 10000);
  int32_t y = fdp->ConsumeBool()
                  ? kSpecial[fdp->ConsumeIntegralInRange<size_t>(
                        0, std::size(kSpecial) - 1)]
                  : fdp->ConsumeIntegralInRange<int32_t>(-10000, 10000);
  return PhysicalOffset(LayoutUnit(x), LayoutUnit(y));
}

PaintFlags FuzzFlags(FuzzedDataProvider* fdp) {
  PaintFlags flags = PaintFlag::kNoFlag;
  uint8_t bits = fdp->ConsumeIntegral<uint8_t>();
  if (bits & 0x01) {
    flags |= PaintFlag::kOmitCompositingInfo;
  }
  if (bits & 0x02) {
    flags |= PaintFlag::kAddUrlMetadata;
  }
  if (bits & 0x04) {
    flags |= PaintFlag::kPaintingResourceSubtree;
  }
  return flags;
}

PaintPhase FuzzPhase(FuzzedDataProvider* fdp) {
  return kAllPhases[fdp->ConsumeIntegralInRange<size_t>(
      0, std::size(kAllPhases) - 1)];
}

void RunPaintOnce(FuzzedDataProvider* fdp, PaintFixture& fix) {
  PaintController controller;
  GraphicsContext context(controller);
  CullRect cull_rect =
      fdp->ConsumeBool() ? CullRect::Infinite() : CullRect();
  PaintPhase phase = FuzzPhase(fdp);
  PaintFlags flags = FuzzFlags(fdp);
  bool descendant_blocked = fdp->ConsumeBool();

  PaintInfo paint_info(context, cull_rect, phase, descendant_blocked, flags);
  NativePainter painter(*fix.layout_native);
  painter.PaintReplaced(paint_info, FuzzOffset(fdp));
}

// Exercise the early-return phase check (kBlockBackground etc).
void FuzzNonForegroundPhase(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  PaintFixture fix(doc, /*loader=*/true, /*bridge=*/true, /*cc_layer=*/true);
  PaintController controller;
  GraphicsContext context(controller);
  PaintPhase non_fg_phases[] = {PaintPhase::kBlockBackground,
                                PaintPhase::kFloat,
                                PaintPhase::kOutline,
                                PaintPhase::kMask};
  PaintPhase phase = non_fg_phases[fdp->ConsumeIntegralInRange<size_t>(
      0, std::size(non_fg_phases) - 1)];
  PaintInfo paint_info(context, CullRect::Infinite(), phase, false);
  NativePainter painter(*fix.layout_native);
  painter.PaintReplaced(paint_info, FuzzOffset(fdp));
}

// Plugin element present but NativeLoader missing.
void FuzzMissingLoader(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  PaintFixture fix(doc, /*loader=*/false, /*bridge=*/false, /*cc_layer=*/false);
  RunPaintOnce(fdp, fix);
}

// Loader exists, WebNativeBridge missing.
void FuzzMissingBridge(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  PaintFixture fix(doc, /*loader=*/true, /*bridge=*/false, /*cc_layer=*/false);
  RunPaintOnce(fdp, fix);
}

// Full chain + cc::Layer present → foreign-layer path.
void FuzzForeignLayerPath(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  PaintFixture fix(doc, /*loader=*/true, /*bridge=*/true, /*cc_layer=*/true);

  PaintController controller;
  GraphicsContext context(controller);
  // Force kForeground without OmitCompositingInfo → paint_with_foreign_layer.
  PaintInfo paint_info(context, CullRect::Infinite(), PaintPhase::kForeground,
                       /*descendant_blocked=*/false);
  NativePainter painter(*fix.layout_native);
  painter.PaintReplaced(paint_info, FuzzOffset(fdp));
}

// Full chain, no cc::Layer, kForeground, no OmitCompositing → BoxDrawingRecorder
// + ImagePainter software path.
void FuzzSoftwarePaintNoLayer(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  PaintFixture fix(doc, /*loader=*/true, /*bridge=*/true, /*cc_layer=*/false);

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect::Infinite(), PaintPhase::kForeground,
                       /*descendant_blocked=*/false);
  NativePainter painter(*fix.layout_native);
  painter.PaintReplaced(paint_info, FuzzOffset(fdp));
}

// Full chain + OmitCompositingInfo → force_software_video_paint branch.
void FuzzForceSoftware(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  bool with_layer = fdp->ConsumeBool();
  PaintFixture fix(doc, /*loader=*/true, /*bridge=*/true, with_layer);

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect::Infinite(),
                       fdp->ConsumeBool() ? PaintPhase::kForeground
                                          : PaintPhase::kSelectionDragImage,
                       /*descendant_blocked=*/false,
                       PaintFlag::kOmitCompositingInfo);
  NativePainter painter(*fix.layout_native);
  painter.PaintReplaced(paint_info, FuzzOffset(fdp));
}

// kSelectionDragImage with full chain — distinct from kForeground, never
// triggers paint_with_foreign_layer.
void FuzzSelectionDragImage(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  PaintFixture fix(doc, /*loader=*/true, /*bridge=*/true,
                   fdp->ConsumeBool());

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect::Infinite(),
                       PaintPhase::kSelectionDragImage,
                       /*descendant_blocked=*/false, FuzzFlags(fdp));
  NativePainter painter(*fix.layout_native);
  painter.PaintReplaced(paint_info, FuzzOffset(fdp));
}

// Wrap one of the deep paths in a PaintPreviewScope so the
// SetURLForRect branch executes.
void FuzzPaintPreviewPath(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  Document::PaintPreviewState state = kPreviewStates[fdp->ConsumeIntegralInRange<
      size_t>(1, std::size(kPreviewStates) - 1)];
  Document::PaintPreviewScope scope(doc, state,
                                    /*allow_scrollbars=*/fdp->ConsumeBool());

  bool with_layer = fdp->ConsumeBool();
  PaintFixture fix(doc, /*loader=*/true, /*bridge=*/true, with_layer);

  PaintController controller;
  GraphicsContext context(controller);
  PaintFlags flags = FuzzFlags(fdp);
  PaintInfo paint_info(context, CullRect::Infinite(),
                       fdp->ConsumeBool() ? PaintPhase::kForeground
                                          : PaintPhase::kSelectionDragImage,
                       /*descendant_blocked=*/false, flags);
  NativePainter painter(*fix.layout_native);
  painter.PaintReplaced(paint_info, FuzzOffset(fdp));
}

// Re-paint twice with identical parameters so the second invocation hits
// the UseCachedDrawingIfPossible branch.
void FuzzCachedDrawingPath(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  PaintFixture fix(doc, /*loader=*/true, /*bridge=*/true,
                   /*cc_layer=*/false);

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect::Infinite(), PaintPhase::kForeground,
                       /*descendant_blocked=*/false,
                       PaintFlag::kOmitCompositingInfo);
  PhysicalOffset offset = FuzzOffset(fdp);
  NativePainter painter(*fix.layout_native);
  painter.PaintReplaced(paint_info, offset);
  // Same controller, same display item client, same phase, same offset.
  painter.PaintReplaced(paint_info, offset);
}

// Free-form: cover any phase / flag / offset combination.
void FuzzFreeform(FuzzedDataProvider* fdp) {
  Document& doc = GetEnv().page_holder->GetDocument();
  PaintFixture fix(doc, fdp->ConsumeBool(), fdp->ConsumeBool(),
                   fdp->ConsumeBool());
  RunPaintOnce(fdp, fix);
}

}  // namespace
}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  blink::GetEnv();  // one-time init
  if (!blink::GetEnv().page_holder) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 8);

  switch (selector) {
    case 0:
      blink::FuzzNonForegroundPhase(&fdp);
      break;
    case 1:
      blink::FuzzMissingLoader(&fdp);
      break;
    case 2:
      blink::FuzzMissingBridge(&fdp);
      break;
    case 3:
      blink::FuzzForeignLayerPath(&fdp);
      break;
    case 4:
      blink::FuzzSoftwarePaintNoLayer(&fdp);
      break;
    case 5:
      blink::FuzzForceSoftware(&fdp);
      break;
    case 6:
      blink::FuzzSelectionDragImage(&fdp);
      break;
    case 7:
      blink::FuzzPaintPreviewPath(&fdp);
      break;
    case 8:
      blink::FuzzCachedDrawingPath(&fdp);
      break;
    default:
      blink::FuzzFreeform(&fdp);
      break;
  }

  return 0;
}
