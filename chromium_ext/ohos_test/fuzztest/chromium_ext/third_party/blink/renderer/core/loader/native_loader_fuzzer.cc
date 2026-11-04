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

#include <memory>
#include <string>

#include "arkweb/chromium_ext/third_party/blink/renderer/core/html/html_native_loader.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/html/html_plugin_element_utils.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "cc/layers/layer.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class FakeCcLayer : public cc::Layer {
 public:
  FakeCcLayer() : cc::Layer() {}
};

static frame_test_helpers::WebViewHelper* g_web_view_helper = nullptr;
static Persistent<HTMLNativeLoader>* g_loader = nullptr;
static scoped_refptr<FakeCcLayer> g_cc_layer;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();

    g_web_view_helper = new frame_test_helpers::WebViewHelper();
    g_web_view_helper->Initialize();

    auto* frame = g_web_view_helper->LocalMainFrame()->GetFrame();
    frame->GetSettings()->SetNativeEmbedModeEnabled(true);

    auto* doc = static_cast<Document*>(
        g_web_view_helper->LocalMainFrame()->GetDocument());
    doc->body()->SetHTMLUnsafeWithoutTrustedTypes(
        "<embed id='test-plugin' type='test/native'>");

    auto* plugin_element = DynamicTo<HTMLPlugInElement>(
        doc->getElementById(AtomicString("test-plugin")));

    g_loader = new Persistent<HTMLNativeLoader>(
        MakeGarbageCollected<HTMLNativeLoader>(plugin_element));

    g_cc_layer = base::MakeRefCounted<FakeCcLayer>();
  }

  ~Environment() {
    delete g_loader;
    g_loader = nullptr;
    g_cc_layer.reset();
    delete g_web_view_helper;
    g_web_view_helper = nullptr;
  }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

static gfx::Rect FuzzRect(FuzzedDataProvider* fdp) {
  int x = fdp->ConsumeIntegralInRange<int>(0, 400);
  int y = fdp->ConsumeIntegralInRange<int>(0, 400);
  int w = fdp->ConsumeIntegralInRange<int>(0, 800);
  int h = fdp->ConsumeIntegralInRange<int>(0, 800);
  return gfx::Rect(x, y, w, h);
}

void FuzzLayerLifecycle(HTMLNativeLoader* loader, FuzzedDataProvider* fdp) {
  loader->SetCcLayer(g_cc_layer.get());

  int embed_id = fdp->ConsumeIntegralInRange<int>(-1, 1000);
  loader->OnCreateNativeSurface(embed_id, base::DoNothing());

  loader->OnLayerRectChange(FuzzRect(fdp));
  loader->OnLayerRectVisibilityChange(fdp->ConsumeBool());
  loader->NotifyVisibilityChange(fdp->ConsumeBool());
  loader->Repaint();
  loader->OnDestroyNativeSurface();
}

void FuzzOverlayTypes(HTMLNativeLoader* loader, FuzzedDataProvider* fdp) {
  static const char* kTypes[] = {
      "native/overlay-infinity",
      "native/overlay",
      "native/normal",
  };
  auto* doc = static_cast<Document*>(
      g_web_view_helper->LocalMainFrame()->GetDocument());
  auto* embed =
      To<HTMLEmbedElement>(doc->getElementById(AtomicString("test-plugin")));
  if (!embed) {
    return;
  }

  int type_idx = fdp->ConsumeIntegralInRange<int>(0, 2);
  embed->setAttribute(html_names::kTypeAttr, AtomicString(kTypes[type_idx]));

  loader->SetCcLayer(g_cc_layer.get());
  if (fdp->ConsumeBool()) {
    loader->SetNativeEmbedOverlayInfinity(fdp->ConsumeBool());
  }
  if (fdp->ConsumeBool()) {
    loader->SetNativeEmbedOverlay(fdp->ConsumeBool());
  }
}

void FuzzParamChanges(HTMLNativeLoader* loader, FuzzedDataProvider* fdp) {
  int num_params = fdp->ConsumeIntegralInRange<int>(0, 502);
  Vector<ParamChangeInfo> changes;
  for (int i = 0; i < num_params && fdp->remaining_bytes() > 4; ++i) {
    int status_int = fdp->ConsumeIntegralInRange<int>(0, 2);
    ParamChangeInfo::Status status;
    switch (status_int) {
      case 0:
        status = ParamChangeInfo::Status::kAdd;
        break;
      case 1:
        status = ParamChangeInfo::Status::kUpdate;
        break;
      default:
        status = ParamChangeInfo::Status::kDelete;
        break;
    }
    AtomicString id(fdp->ConsumeRandomLengthString(32).c_str());
    AtomicString name(fdp->ConsumeRandomLengthString(32).c_str());
    AtomicString value(fdp->ConsumeRandomLengthString(32).c_str());
    changes.push_back(ParamChangeInfo(status, id, name, value));
  }
  loader->ProcessParamChanges(changes);

  if (fdp->ConsumeBool()) {
    loader->OnCreateNativeSurface(fdp->ConsumeIntegralInRange<int>(1, 100),
                                  base::DoNothing());
    loader->ProcessPendingParamChanges();
  }
}

void FuzzVisibilityAndStretch(HTMLNativeLoader* loader,
                              FuzzedDataProvider* fdp) {
  loader->NotifyVisibilityChange(fdp->ConsumeBool());
  loader->OnLayerRectVisibilityChange(fdp->ConsumeBool());
  loader->CleanupVisibilityForRemovedLayer(fdp->ConsumeBool());
  loader->SetStretchContentToFillBounds(fdp->ConsumeBool());
  loader->GetStretchContentToFillBounds();
}

void FuzzStateSequence(HTMLNativeLoader* loader, FuzzedDataProvider* fdp) {
  int num_ops = fdp->ConsumeIntegralInRange<int>(2, 12);
  for (int i = 0; i < num_ops && fdp->remaining_bytes() > 4; ++i) {
    int op = fdp->ConsumeIntegralInRange<int>(0, 8);
    switch (op) {
      case 0:
        loader->SetCcLayer(g_cc_layer.get());
        break;
      case 1:
        loader->SetCcLayer(nullptr);
        break;
      case 2:
        loader->OnCreateNativeSurface(
            fdp->ConsumeIntegralInRange<int>(-1, 999), base::DoNothing());
        break;
      case 3:
        loader->OnLayerRectChange(FuzzRect(fdp));
        break;
      case 4:
        loader->OnLayerRectVisibilityChange(fdp->ConsumeBool());
        break;
      case 5:
        loader->OnDestroyNativeSurface();
        break;
      case 6:
        loader->NotifyVisibilityChange(fdp->ConsumeBool());
        break;
      case 7:
        loader->Repaint();
        break;
      case 8:
        loader->SetStretchContentToFillBounds(fdp->ConsumeBool());
        break;
    }
  }
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::Environment env;

  if (!blink::g_loader || !*blink::g_loader) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  blink::HTMLNativeLoader* loader = blink::g_loader->Get();

  int32_t scenario = fdp.ConsumeIntegralInRange<int32_t>(0, 4);
  switch (scenario) {
    case 0:
      blink::FuzzLayerLifecycle(loader, &fdp);
      break;
    case 1:
      blink::FuzzOverlayTypes(loader, &fdp);
      break;
    case 2:
      blink::FuzzParamChanges(loader, &fdp);
      break;
    case 3:
      blink::FuzzVisibilityAndStretch(loader, &fdp);
      break;
    case 4:
      blink::FuzzStateSequence(loader, &fdp);
      break;
    default:
      blink::FuzzLayerLifecycle(loader, &fdp);
      break;
  }

  loader->SetCcLayer(nullptr);
  return 0;
}
