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

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/frame/local_frame_for_include.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>
#include <string>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

namespace {
constexpr uint8_t MAX_STRING_LENGTH = 255;
constexpr float MAX_FLOAT_SIZE = 1000.0f;
}  // namespace

static std::unique_ptr<DummyPageHolder> g_page_holder = nullptr;

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    g_page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }

  ~Environment() { g_page_holder.reset(); }

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

void FuzzSetLayoutAndTextZoomFactorsPage(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();
  Page* page = &(g_page_holder->GetPage());
  CHECK(page);

  float layout_zoom_factor =
      fdp->ConsumeFloatingPointInRange<float>(0.0f, MAX_FLOAT_SIZE);
  float text_zoom_factor = 0.0f;

  LocalFrameUtil::SetLayoutAndTextZoomFactorsPage(&frame, layout_zoom_factor,
                                                   text_zoom_factor, page);
}

void FuzzSetLayoutAndTextZoomFactorsExt(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();
  Page* page = &(g_page_holder->GetPage());
  CHECK(page);

  float layout_zoom_factor =
      fdp->ConsumeFloatingPointInRange<float>(0.0f, MAX_FLOAT_SIZE);
  float text_zoom_factor = 0.0f;
  bool layout_zoom_changed = fdp->ConsumeBool();

  LocalFrameUtil::SetLayoutAndTextZoomFactorsExt(
      &frame, layout_zoom_factor, text_zoom_factor, layout_zoom_changed, page);
}

void FuzzSetTextZoomFactorsExt(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  LocalFrameUtil::SetTextZoomFactorsExt(&frame);
}

#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
void FuzzOnOverScrollOffsetChanged(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  float offset_x = fdp->ConsumeFloatingPoint<float>();
  float offset_y = fdp->ConsumeFloatingPoint<float>();

  frame.OnOverScrollOffsetChanged(offset_x, offset_y);
}
#endif  // BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
void FuzzGetBlankScreenDetector(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  bool force = fdp->ConsumeBool();

  frame.GetBlankScreenDetector(force);
}

void FuzzOnDetectedBlankScreen(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  std::string url_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  String url = String::FromUTF8(url_str.c_str());
  int32_t reason = fdp->ConsumeIntegral<int32_t>();
  int32_t count = fdp->ConsumeIntegral<int32_t>();

  frame.OnDetectedBlankScreen(url, reason, count);
}
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)

#if BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)
void FuzzOnFirstScreenPaint(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  std::string url_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  String url = String::FromUTF8(url_str.c_str());

  frame.OnFirstScreenPaint(url, base::TimeTicks(), base::TimeTicks());
}
#endif  // BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)

#if BUILDFLAG(ARKWEB_ADBLOCK)
void FuzzSetAdBlockEnableForSite(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  bool enabled = fdp->ConsumeBool();

  frame.SetAdBlockEnableForSite(enabled);
}

void FuzzSetHasElemHideTypeOption(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  bool has = fdp->ConsumeBool();

  frame.SetHasElemHideTypeOption(has);
}

void FuzzSetHasDocumentTypeOption(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  bool has = fdp->ConsumeBool();

  frame.SetHasDocumentTypeOption(has);
}

void FuzzSetHasGenericHideTypeOption(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  bool has = fdp->ConsumeBool();

  frame.SetHasGenericHideTypeOption(has);
}
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)

#if BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
void FuzzSetVideoIsPlaying(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  std::string id = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  bool playing = fdp->ConsumeBool();

  frame.SetVideoIsPlaying(id, playing);
}
#endif  // BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)

#if BUILDFLAG(ARKWEB_PDF)
void FuzzIsPDF(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  frame.IsPDF();
}
#endif  // BUILDFLAG(ARKWEB_PDF)

#if BUILDFLAG(ARKWEB_AI)
void FuzzStartHighlightFadeTimer(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  int64_t delay_ms = fdp->ConsumeIntegralInRange<int64_t>(0, 10000);

  frame.StartHighlightFadeTimer(base::Milliseconds(delay_ms));
}

void FuzzClearHighlight(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  bool ignore_setting = fdp->ConsumeBool();

  frame.ClearHighlight(ignore_setting);
}

void FuzzProcessFragment(FuzzedDataProvider* fdp) {
  if (!g_page_holder) {
    return;
  }
  LocalFrame& frame = g_page_holder->GetFrame();

  std::string url_str = fdp->ConsumeRandomLengthString(MAX_STRING_LENGTH);
  KURL url(String::FromUTF8(url_str.c_str()));

  frame.ProcessFragment(url);
}
#endif  // BUILDFLAG(ARKWEB_AI)

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 15);

  switch (selector) {
    case 0:
      blink::FuzzSetLayoutAndTextZoomFactorsPage(&fdp);
      break;
    case 1:
      blink::FuzzSetLayoutAndTextZoomFactorsExt(&fdp);
      break;
    case 2:
      blink::FuzzSetTextZoomFactorsExt(&fdp);
      break;
#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
    case 3:
      blink::FuzzOnOverScrollOffsetChanged(&fdp);
      break;
#endif  // BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
    case 4:
      blink::FuzzGetBlankScreenDetector(&fdp);
      break;
    case 5:
      blink::FuzzOnDetectedBlankScreen(&fdp);
      break;
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
#if BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)
    case 6:
      blink::FuzzOnFirstScreenPaint(&fdp);
      break;
#endif  // BUILDFLAG(ARKWEB_FIRST_SCREEN_PAINT)
#if BUILDFLAG(ARKWEB_ADBLOCK)
    case 7:
      blink::FuzzSetAdBlockEnableForSite(&fdp);
      break;
    case 8:
      blink::FuzzSetHasElemHideTypeOption(&fdp);
      break;
    case 9:
      blink::FuzzSetHasDocumentTypeOption(&fdp);
      break;
    case 10:
      blink::FuzzSetHasGenericHideTypeOption(&fdp);
      break;
#endif  // BUILDFLAG(ARKWEB_ADBLOCK)
#if BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
    case 11:
      blink::FuzzSetVideoIsPlaying(&fdp);
      break;
#endif  // BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
#if BUILDFLAG(ARKWEB_PDF)
    case 12:
      blink::FuzzIsPDF(&fdp);
      break;
#endif  // BUILDFLAG(ARKWEB_PDF)
#if BUILDFLAG(ARKWEB_AI)
    case 13:
      blink::FuzzStartHighlightFadeTimer(&fdp);
      break;
    case 14:
      blink::FuzzClearHighlight(&fdp);
      break;
    case 15:
      blink::FuzzProcessFragment(&fdp);
      break;
#endif  // BUILDFLAG(ARKWEB_AI)
    default:
      blink::FuzzSetLayoutAndTextZoomFactorsPage(&fdp);
      break;
  }

  return 0;
}