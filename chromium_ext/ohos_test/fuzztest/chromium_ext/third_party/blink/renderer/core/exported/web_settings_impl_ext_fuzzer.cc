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
#include <string>

#include "arkweb/build/features/features.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/exported/web_settings_impl_ext.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "ui/gfx/geometry/size.h"
#include "third_party/blink/renderer/core/dom/document.h"
#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
#include "third_party/blink/renderer/modules/media_controls/media_controls_resource_loader.h"
#endif

namespace {

static std::unique_ptr<blink::DummyPageHolder> g_page_holder;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder =
        std::make_unique<blink::DummyPageHolder>(gfx::Size(800, 600));
  }
  ~Environment() { g_page_holder.reset(); }
  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

#if BUILDFLAG(ARKWEB_COPY_OPTION)
constexpr blink::mojom::blink::CopyOptionMode kCopyOptionModes[] = {
    blink::mojom::blink::CopyOptionMode::NONE,
    blink::mojom::blink::CopyOptionMode::IN_APP,
    blink::mojom::blink::CopyOptionMode::LOCAL_DEVICE,
    blink::mojom::blink::CopyOptionMode::CROSS_DEVICE,
};
#endif

void FuzzOneOperation(blink::WebSettingsImplExt* impl,
                      FuzzedDataProvider* fdp) {
  int op = fdp->ConsumeIntegralInRange<int>(0, 32);
  switch (op) {
    case 0: {
#if BUILDFLAG(ARKWEB_SCROLLBAR)
      impl->SetScrollBarColor(fdp->ConsumeIntegral<uint32_t>());
#endif
      break;
    }
    case 1: {
#if BUILDFLAG(ARKWEB_AI)
      impl->SetImageAnalyzerEnabled(fdp->ConsumeBool());
#endif
      break;
    }
    case 2: {
#if BUILDFLAG(ARKWEB_AI)
      impl->SetArkwebAgentEnabled(fdp->ConsumeBool());
#endif
      break;
    }
    case 3: {
#if BUILDFLAG(ARKWEB_AI)
      impl->SetAgentNeedHighlight(fdp->ConsumeBool());
#endif
      break;
    }
    case 4: {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
      impl->SetVerticalHideScrollbars(fdp->ConsumeBool());
#endif
      break;
    }
    case 5: {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
      impl->SetHorizontalHideScrollbars(fdp->ConsumeBool());
#endif
      break;
    }
    case 6: {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
      impl->SetScrollable(fdp->ConsumeBool());
#endif
      break;
    }
    case 7: {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
      impl->SetScrollbarLayoutPolicy(fdp->ConsumeIntegral<int>());
      (void)impl->GetScrollbarLayoutPolicy();
#endif
      break;
    }
    case 8: {
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
      impl->SetIsSystemRtlEnable(fdp->ConsumeBool());
      (void)impl->GetIsSystemRtlEnabled();
#endif
      break;
    }
    case 9: {
#if BUILDFLAG(ARKWEB_ZOOM)
      impl->SetTextZoomFactor(fdp->ConsumeFloatingPoint<float>());
#endif
      break;
    }
    case 10: {
#if BUILDFLAG(ARKWEB_COPY_OPTION)
      size_t idx = fdp->ConsumeIntegralInRange<size_t>(
          0, std::size(kCopyOptionModes) - 1);
      impl->SetCopyOption(kCopyOptionModes[idx]);
#endif
      break;
    }
    case 11: {
#if BUILDFLAG(ARKWEB_FOCUS)
      impl->SetGestureFocusMode(fdp->ConsumeIntegral<int>());
#endif
      break;
    }
    case 12: {
#if BUILDFLAG(ARKWEB_EXT_FREE_COPY)
      impl->SetContextMenuCustomization(fdp->ConsumeBool());
#endif
      break;
    }
    case 13: {
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
      impl->SetDrawMode(fdp->ConsumeIntegral<int>());
#endif
      break;
    }
    case 14: {
#if BUILDFLAG(ARKWEB_CSS_FONT)
      impl->SetFontWeightScale(fdp->ConsumeFloatingPoint<float>());
#endif
      break;
    }
    case 15: {
#if BUILDFLAG(ARKWEB_SAME_LAYER)
      impl->SetNativeEmbedModeEnabled(fdp->ConsumeBool());
      (void)impl->NativeEmbedModeEnabled();
#endif
      break;
    }
    case 16: {
#if BUILDFLAG(ARKWEB_SAME_LAYER)
      impl->SetIntrinsicSizeEnabled(fdp->ConsumeBool());
#endif
      break;
    }
    case 17: {
#if BUILDFLAG(ARKWEB_SAME_LAYER)
      impl->SetCssDisplayChangeEnabled(fdp->ConsumeBool());
#endif
      break;
    }
    case 18: {
#if BUILDFLAG(ARKWEB_SAME_LAYER)
      std::string tag = fdp->ConsumeRandomLengthString(32);
      std::string type = fdp->ConsumeRandomLengthString(32);
      impl->RegisterNativeEmbedRule(blink::WebString::FromUTF8(tag),
                                    blink::WebString::FromUTF8(type));
#endif
      break;
    }
    case 19: {
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      impl->SetCustomVideoPlayerEnabled(fdp->ConsumeBool());
#endif
      break;
    }
    case 20: {
#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
      impl->SetCustomVideoPlayerOverlay(fdp->ConsumeBool());
#endif
      break;
    }
    case 21: {
#if BUILDFLAG(ARKWEB_MEDIA_CAPABILITIES_ENHANCE)
      impl->SetUsageScenario(fdp->ConsumeIntegral<int32_t>());
#endif
      break;
    }
    case 22: {
#if BUILDFLAG(ARKWEB_LOGGER_REPORT)
      (void)impl->GetUsageScenario();
#endif
      break;
    }
    case 23: {
#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
      impl->SetVideoAssistantEnabled(fdp->ConsumeBool());
#endif
      break;
    }
    case 24: {
#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
      impl->SetCustomMediaPlayerEnabled(fdp->ConsumeBool());
      (void)impl->GetCustomMediaPlayerEnabled();
#endif
      break;
    }
    case 25: {
#if BUILDFLAG(ARKWEB_BFCACHE)
      impl->SetMediaResumeFromBFCachePage(fdp->ConsumeBool());
      (void)impl->GetMediaResumeFromBFCachePage();
#endif
      break;
    }
    case 26: {
#if BUILDFLAG(ARKWEB_SCROLLBAR_AVOID_CORNER)
      double tl = fdp->ConsumeFloatingPoint<double>();
      double tr = fdp->ConsumeFloatingPoint<double>();
      double bl = fdp->ConsumeFloatingPoint<double>();
      double br = fdp->ConsumeFloatingPoint<double>();
      impl->SetBorderRadiusFromWeb(tl, tr, bl, br);
      (void)impl->GetBorderRadiusTopLeft();
      (void)impl->GetBorderRadiusTopRight();
      (void)impl->GetBorderRadiusBottomLeft();
      (void)impl->GetBorderRadiusBottomRight();
#endif
      break;
    }
    case 27: {
#if BUILDFLAG(ARKWEB_PASSWORD_AUTOFILL)
      impl->SetEnableAutoFill(fdp->ConsumeBool());
      (void)impl->GetEnableAutoFill();
#endif
      break;
    }
    case 28: {
#if BUILDFLAG(ARKWEB_DRAG_DROP)
      impl->SetEnableDrag(fdp->ConsumeBool());
      (void)impl->GetEnableDrag();
#endif
      break;
    }
    case 29: {
#if BUILDFLAG(ARKWEB_CLIPBOARD)
      impl->SetClipboardSitePermissionEnabled(fdp->ConsumeBool());
#endif
      break;
    }
    case 30: {
#if BUILDFLAG(ARKWEB_MENU)
      impl->SetTouchHandleExistState(fdp->ConsumeBool());
      (void)impl->IsTouchHandleExist();
#endif
      break;
    }
    case 31: {
#if BUILDFLAG(ARKWEB_MENU)
      impl->SetViewportScaleState(fdp->ConsumeBool());
      (void)impl->IsViewportScale();
#endif
      break;
    }
    case 32: {
#if BUILDFLAG(ARKWEB_MEDIA_CAST)
      impl->SetCastEnabled(fdp->ConsumeBool());
#endif
      break;
    }
    default:
      break;
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  if (!g_page_holder) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  blink::Settings* settings =
      &g_page_holder->GetDocument().GetPage()->GetSettings();
  blink::WebSettingsImplExt settings_impl(settings, nullptr);

  int num_ops = fdp.ConsumeIntegralInRange<int>(1, 16);
  for (int i = 0; i < num_ops && fdp.remaining_bytes() > 0; ++i) {
    FuzzOneOperation(&settings_impl, &fdp);
  }

  return 0;
}
