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

// Coverage strategy
// -----------------
// WebFrameWidgetImplExt is a renderer-side widget extension whose ctor
// requires a fully wired WebView + Mojo channels. The same harness used in
// the unittest (frame_test_helpers::WebViewHelper) builds all of that and,
// because BUILDFLAG(ARKWEB_UNITTESTS) is enabled in the target toolchain
// (see frame_test_helpers.h:256), the test widget alias is itself defined
// to inherit WebFrameWidgetImplExt — so MainFrameWidget() is the SUT.
//
// We build that fixture once in Environment ctor (process-wide), load a
// small static HTML, then per-iteration drive each public method behind
// the BUILDFLAGs the production .cc enables. We avoid:
//   * CreateOverlay/NotifyOverlayStateChanged (calls associated host →
//     mojo dispatch without a real renderer-host bridge),
//   * ParseLanguage (depends on a WidgetInputHandlerManager being wired),
//   * RegisterClippedVisualViewportSelectionBounds (requires
//     LayerTreeHost composition state).
// These are safer to leave to focused integration tests.

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>
#include <string>

#include "arkweb/build/features/features.h"
#include "arkweb/chromium_ext/third_party/blink/public/mojom/page/text_recognize_result.mojom-blink.h"
#include "arkweb/chromium_ext/third_party/blink/renderer/core/frame/web_frame_widget_impl_ext.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    web_view_helper = std::make_unique<frame_test_helpers::WebViewHelper>();
    web_view_helper->Initialize();
    web_view_helper->GetWebView()->MainFrameViewWidget()->Resize(
        gfx::Size(800, 600));
    // Load a tiny page so document/layout exist for hit-testing &
    // FocusedWebLocalFrameInWidget paths.
    frame_test_helpers::LoadHTMLString(
        web_view_helper->GetWebView()->MainFrameImpl(),
        "<html><body><div id='d'>hi</div>"
        "<input id='i' type='text' value='x'/>"
        "</body></html>",
        url_test_helpers::ToKURL("about:blank"));
  }
  // No dtor: tearing WebViewHelper down at process atexit can race with
  // child threads. libFuzzer terminates the process so leaking is fine.
  content::BlinkTestEnvironmentWithIsolate blink_environment_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper;
};

Environment& GetEnv() {
  static Environment env;
  return env;
}

WebFrameWidgetImplExt* GetWidget() {
#if BUILDFLAG(ARKWEB_UNITTESTS)
  // TestWebFrameWidget IS WebFrameWidgetImplExt under ARKWEB_UNITTESTS.
  auto* helper = GetEnv().web_view_helper.get();
  if (!helper || !helper->GetWebView() ||
      !helper->GetWebView()->MainFrameViewWidget()) {
    return nullptr;
  }
  // MainFrameViewWidget() returns WebFrameWidgetImpl*, the test fixture
  // builds TestWebFrameWidget which derives from WebFrameWidgetImplExt.
  return static_cast<WebFrameWidgetImplExt*>(
      helper->GetWebView()->MainFrameViewWidget());
#else
  return nullptr;
#endif
}

gfx::Point FuzzPoint(FuzzedDataProvider* fdp) {
  return gfx::Point(fdp->ConsumeIntegralInRange<int>(-2000, 2000),
                    fdp->ConsumeIntegralInRange<int>(-2000, 2000));
}

gfx::Rect FuzzRect(FuzzedDataProvider* fdp) {
  return gfx::Rect(fdp->ConsumeIntegralInRange<int>(-2000, 2000),
                   fdp->ConsumeIntegralInRange<int>(-2000, 2000),
                   fdp->ConsumeIntegralInRange<int>(0, 2000),
                   fdp->ConsumeIntegralInRange<int>(0, 2000));
}

String FuzzWtfString(FuzzedDataProvider* fdp, size_t max_len) {
  std::string s = fdp->ConsumeRandomLengthString(max_len);
  return String::FromUTF8(s);
}

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void FuzzInputEvents(FuzzedDataProvider* fdp, WebFrameWidgetImplExt* w) {
  float zoom = fdp->ConsumeFloatingPointInRange<float>(-5.0f, 5.0f);
  w->SetZoomLevel(zoom, FuzzPoint(fdp));
  w->SetOverscrollMode(fdp->ConsumeIntegral<int32_t>());

  HashMap<String, String> attrs;
  w->GetInputElementAttributes(attrs);

  // IsElementExist drives DocumentXPathEvaluator::evaluate which tolerates
  // arbitrary strings and returns false on failure.
  std::string xpath = fdp->ConsumeRandomLengthString(128);
  (void)w->IsElementExist(xpath);
}
#endif

#if BUILDFLAG(ARKWEB_MENU)
void FuzzMenuApis(FuzzedDataProvider* fdp, WebFrameWidgetImplExt* w) {
  w->SelectRangeV2(FuzzPoint(fdp), fdp->ConsumeBool());
  w->CleanFocusCache();
  // NotifySelectionRangeEmpty: pass an explicitly-null WebRange and the
  // focused frame; the function only calls into client when range IsNull.
  WebLocalFrame* focused =
      GetEnv().web_view_helper->GetWebView()->FocusedFrame();
  WebRange empty_range;  // default-constructed ⇒ IsNull() == true
  w->NotifySelectionRangeEmpty(empty_range, focused);
}
#endif

#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
void FuzzScrollOffset(FuzzedDataProvider* fdp, WebFrameWidgetImplExt* w) {
  (void)w->GetOverScrollOffset();
  float ox = fdp->ConsumeFloatingPointInRange<float>(-10000.0f, 10000.0f);
  float oy = fdp->ConsumeFloatingPointInRange<float>(-10000.0f, 10000.0f);
  w->OnOverScrollOffsetChanged(ox, oy);
}
#endif

#if BUILDFLAG(ARKWEB_DRAG_DROP)
void FuzzVisibleRect(FuzzedDataProvider* /*fdp*/, WebFrameWidgetImplExt* w) {
  // GetVisibleRectToWeb safely returns empty rect when host is null/binder
  // returns nothing.
  (void)w->GetVisibleRectToWeb();
}
#endif

#if BUILDFLAG(ARKWEB_PINCH_SMOOTH)
void FuzzPinchSmooth(FuzzedDataProvider* fdp, WebFrameWidgetImplExt* w) {
  w->SetPinchSmoothMode(fdp->ConsumeBool());
}
#endif

#if BUILDFLAG(ARKWEB_AI)
void FuzzAiCallbacks(FuzzedDataProvider* fdp, WebFrameWidgetImplExt* w) {
  // Install harmless callbacks first (most code paths early-return on
  // missing callback otherwise).
  w->on_text_recognize_callback_ = base::BindRepeating(
      [](std::vector<String>, std::vector<gfx::PointF>, float) {});
  w->on_text_selected_callback_ =
      base::BindRepeating([](bool) {});
  w->on_destroy_image_overlay_callback_ =
      base::BindRepeating([]() {});
  w->get_rect_callback_ = base::BindRepeating(
      [](gfx::RectF& r) { r = gfx::RectF(0, 0, 10, 10); });

  // OnTextRecognized: build a Vector with N entries of fuzz-driven corners.
  Vector<mojom::blink::TextRecognizeResultPtr> res;
  size_t count = fdp->ConsumeIntegralInRange<size_t>(0, 4);
  for (size_t i = 0; i < count; ++i) {
    auto entry = mojom::blink::TextRecognizeResult::New();
    entry->raw_value = FuzzWtfString(fdp, 32);
    entry->bounding_box = gfx::RectF(
        fdp->ConsumeFloatingPointInRange<float>(-1000.0f, 1000.0f),
        fdp->ConsumeFloatingPointInRange<float>(-1000.0f, 1000.0f),
        fdp->ConsumeFloatingPointInRange<float>(0.0f, 1000.0f),
        fdp->ConsumeFloatingPointInRange<float>(0.0f, 1000.0f));
    // Production code reads exactly 4 corners (WORD_CORNER_NUM).
    entry->corner_points = {
        gfx::PointF(fdp->ConsumeFloatingPointInRange<float>(-1e3f, 1e3f),
                    fdp->ConsumeFloatingPointInRange<float>(-1e3f, 1e3f)),
        gfx::PointF(fdp->ConsumeFloatingPointInRange<float>(-1e3f, 1e3f),
                    fdp->ConsumeFloatingPointInRange<float>(-1e3f, 1e3f)),
        gfx::PointF(fdp->ConsumeFloatingPointInRange<float>(-1e3f, 1e3f),
                    fdp->ConsumeFloatingPointInRange<float>(-1e3f, 1e3f)),
        gfx::PointF(fdp->ConsumeFloatingPointInRange<float>(-1e3f, 1e3f),
                    fdp->ConsumeFloatingPointInRange<float>(-1e3f, 1e3f)),
    };
    res.push_back(std::move(entry));
  }
  w->OnTextRecognized(std::move(res),
                      fdp->ConsumeFloatingPointInRange<float>(0.1f, 5.0f));

  // OnTextSelected / OnDestroyImageAnalyzerOverlay just dispatch callbacks.
  w->OnTextSelected(fdp->ConsumeBool());
  w->OnDestroyImageAnalyzerOverlay();

  // Fold status round-trip.
  uint32_t fs = fdp->ConsumeIntegral<uint32_t>();
  w->OnFoldStatusChanged(fs);
  (void)w->GetFoldStatus();

  // GetImageRect: callback receives result of GetImageRectInner.
  w->GetImageRect(base::BindOnce([](const gfx::Rect&) {}));

  // OnDataDetectorSelectText: only acts when a focused frame exists.
  w->OnDataDetectorSelectText();
}
#endif

#if BUILDFLAG(ARKWEB_AI)
void FuzzGetWordSelection(FuzzedDataProvider* fdp, WebFrameWidgetImplExt* w) {
  // text.IsNull() branch.
  (void)w->GetWordSelection(String(), fdp->ConsumeIntegral<int8_t>());
  // Non-null path: GetAssociatedFrameWidgetHost may not be wired; the
  // function falls back to {-1,-1} on failure, so it's safe.
  (void)w->GetWordSelection(FuzzWtfString(fdp, 64),
                            fdp->ConsumeIntegral<int8_t>());
}
#endif

#if BUILDFLAG(ARKWEB_DFX_TRACING)
void FuzzDfxTracing(FuzzedDataProvider* fdp, WebFrameWidgetImplExt* w) {
  (void)w->GetCurrentTimestampMS();
  int64_t start = fdp->ConsumeIntegral<int64_t>();
  int64_t end = fdp->ConsumeIntegral<int64_t>();
  w->ReportBlank(start, end);
}
#endif

#if BUILDFLAG(ARKWEB_MEDIA_INTERACTION)
void FuzzMediaInteraction(FuzzedDataProvider* fdp,
                          WebFrameWidgetImplExt* w) {
  // Increment then decrement via DisableBoost, exercising the
  // rawKeyDownTime_ <= 0 branch.
  w->rawKeyDownTime_ = fdp->ConsumeIntegralInRange<int64_t>(0, 4);
  w->DisableBoost();
}
#endif

void FuzzArkWebHandleTouchEvent(FuzzedDataProvider* fdp,
                                WebFrameWidgetImplExt* w) {
  WebInputEvent::Type types[] = {
      WebInputEvent::Type::kPointerUp,
      WebInputEvent::Type::kKeyUp,
      WebInputEvent::Type::kRawKeyDown,
      WebInputEvent::Type::kMouseUp,
      WebInputEvent::Type::kMouseMove,
      WebInputEvent::Type::kTouchStart,
  };
  WebInputEvent::Type t =
      types[fdp->ConsumeIntegralInRange<size_t>(0, std::size(types) - 1)];

  if (t == WebInputEvent::Type::kPointerUp) {
    WebTouchEvent touch_event;
    touch_event.SetType(WebInputEvent::Type::kTouchStart);
    touch_event.SetTimeStamp(base::TimeTicks::Now());
    touch_event.touches_length = 1;
    WebTouchPoint tp;
    tp.state = WebTouchPoint::State::kStatePressed;
    tp.SetPositionInWidget(gfx::PointF(10, 10));
    tp.SetPositionInScreen(gfx::PointF(10, 10));
    tp.radius_x = 5.0f;
    tp.radius_y = 5.0f;
    touch_event.touches[0] = tp;
    WebPointerEvent pe(touch_event, tp);
    w->ArkWebHandleTouchEvent(pe);
  } else if (t == WebInputEvent::Type::kRawKeyDown ||
             t == WebInputEvent::Type::kKeyUp) {
    WebKeyboardEvent kev(t, WebInputEvent::kNoModifiers,
                         WebInputEvent::GetStaticTimeStampForTests());
    w->ArkWebHandleTouchEvent(kev);
  } else {
    WebMouseEvent mev(t, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
    w->ArkWebHandleTouchEvent(mev);
  }
}

}  // namespace
}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  blink::GetEnv();  // one-time fixture
  blink::WebFrameWidgetImplExt* widget = blink::GetWidget();
  if (!widget) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  int32_t selector = fdp.ConsumeIntegralInRange<int32_t>(0, 9);

  switch (selector) {
    case 0:
      blink::FuzzArkWebHandleTouchEvent(&fdp, widget);
      break;
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
    case 1:
      blink::FuzzInputEvents(&fdp, widget);
      break;
#endif
#if BUILDFLAG(ARKWEB_MENU)
    case 2:
      blink::FuzzMenuApis(&fdp, widget);
      break;
#endif
#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
    case 3:
      blink::FuzzScrollOffset(&fdp, widget);
      break;
#endif
#if BUILDFLAG(ARKWEB_DRAG_DROP)
    case 4:
      blink::FuzzVisibleRect(&fdp, widget);
      break;
#endif
#if BUILDFLAG(ARKWEB_PINCH_SMOOTH)
    case 5:
      blink::FuzzPinchSmooth(&fdp, widget);
      break;
#endif
#if BUILDFLAG(ARKWEB_AI)
    case 6:
      blink::FuzzAiCallbacks(&fdp, widget);
      break;
    case 7:
      blink::FuzzGetWordSelection(&fdp, widget);
      break;
#endif
#if BUILDFLAG(ARKWEB_DFX_TRACING)
    case 8:
      blink::FuzzDfxTracing(&fdp, widget);
      break;
#endif
#if BUILDFLAG(ARKWEB_MEDIA_INTERACTION)
    case 9:
      blink::FuzzMediaInteraction(&fdp, widget);
      break;
#endif
    default:
      blink::FuzzArkWebHandleTouchEvent(&fdp, widget);
      break;
  }
  return 0;
}