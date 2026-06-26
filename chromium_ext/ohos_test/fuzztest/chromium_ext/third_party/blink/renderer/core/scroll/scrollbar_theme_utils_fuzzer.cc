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

#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/test/blink_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "ui/gfx/geometry/point.h"

#if BUILDFLAG(IS_ARKWEB)
#include "arkweb/chromium_ext/third_party/blink/renderer/core/scroll/scrollbar_theme_utils.h"
#endif

namespace blink {

class StubScrollbarThemeForFuzzer : public ScrollbarTheme {
 public:
  StubScrollbarThemeForFuzzer() : ScrollbarTheme() {}

  bool NativeThemeHasButtons() const override { return false; }
  bool HasThumb(const Scrollbar&) const override { return true; }
  gfx::Rect BackButtonRect(const Scrollbar&) const override { return gfx::Rect(0, 0, 0, 0); }
  gfx::Rect ForwardButtonRect(const Scrollbar&) const override { return gfx::Rect(0, 0, 0, 0); }
  gfx::Rect TrackRect(const Scrollbar&) const override { return gfx::Rect(0, 0, 100, 100); }
  int MinimumThumbLength(const Scrollbar&) const override { return 10; }
};

static StubScrollbarThemeForFuzzer g_static_theme;

class MockChromeClientForScrollbar : public EmptyChromeClient {
 public:
  MockChromeClientForScrollbar() : is_popup_(false) {}
  bool IsPopup() override { return is_popup_; }
  void SetIsPopup(bool is_popup) { is_popup_ = is_popup; }
  float WindowToViewportScalar(LocalFrame*, const float) const override { return 0; }

 private:
  bool is_popup_;
};

class MockScrollableAreaForFuzzer : public GarbageCollected<MockScrollableAreaForFuzzer>,
                                   public ScrollableArea {
 public:
  MockScrollableAreaForFuzzer()
      : ScrollableArea(blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        chrome_client_(MakeGarbageCollected<MockChromeClientForScrollbar>()) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(chrome_client_);
    ScrollableArea::Trace(visitor);
  }

  MOCK_CONST_METHOD0(IsActive, bool());
  MOCK_CONST_METHOD0(IsThrottled, bool());
  MOCK_CONST_METHOD1(ScrollSize, int(ScrollbarOrientation));
  MOCK_CONST_METHOD0(IsScrollCornerVisible, bool());
  MOCK_CONST_METHOD0(ScrollCornerRect, gfx::Rect());
  MOCK_CONST_METHOD0(EnclosingScrollableArea, ScrollableArea*());
  MOCK_CONST_METHOD1(VisibleContentRect, gfx::Rect(IncludeScrollbarsInRect));
  MOCK_CONST_METHOD0(ContentsSize, gfx::Size());
  MOCK_CONST_METHOD0(LayerForHorizontalScrollbar, cc::Layer*());
  MOCK_CONST_METHOD0(LayerForVerticalScrollbar, cc::Layer*());
  MOCK_CONST_METHOD0(HorizontalScrollbar, Scrollbar*());
  MOCK_CONST_METHOD0(VerticalScrollbar, Scrollbar*());
  MOCK_CONST_METHOD0(ScrollbarsHiddenIfOverlay, bool());
  MOCK_METHOD0(ScheduleAnimation, bool());
  MOCK_CONST_METHOD0(UsedColorSchemeScrollbars, mojom::blink::ColorScheme());
  MOCK_CONST_METHOD0(UsesCompositedScrolling, bool());

  PhysicalOffset LocalToScrollOriginOffset() const override { return {}; }
  bool UserInputScrollable(ScrollbarOrientation) const override { return true; }
  bool ScrollbarsCanBeActive() const override { return true; }
  bool ShouldPlaceVerticalScrollbarOnLeft() const override { return false; }
  void UpdateScrollOffset(const ScrollOffset&, mojom::blink::ScrollType, cc::ScrollSourceType) override {}
  gfx::Vector2d ScrollOffsetInt() const override { return gfx::Vector2d(); }
  ScrollOffset GetScrollOffset() const override { return ScrollOffset(); }
  gfx::Vector2d MinimumScrollOffsetInt() const override { return gfx::Vector2d(); }
  gfx::Vector2d MaximumScrollOffsetInt() const override { return gfx::Vector2d(100, 100); }
  int VisibleHeight() const override { return 768; }
  int VisibleWidth() const override { return 1024; }
  CompositorElementId GetScrollElementId() const override { return CompositorElementId(); }
  bool ScrollAnimatorEnabled() const override { return true; }
  int PageStep(ScrollbarOrientation) const override { return 0; }
  void ScrollControlWasSetNeedsPaintInvalidation() override {}
  gfx::Point ConvertFromRootFrame(const gfx::Point&) const override { return gfx::Point(); }
  gfx::Point ConvertFromContainingEmbeddedContentViewToScrollbar(
      const Scrollbar&, const gfx::Point& parent_point) const override {
    return parent_point;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner() const final {
    return blink::scheduler::GetSingleThreadTaskRunnerForTesting();
  }

  ChromeClient* GetChromeClient() const override {
    return chrome_client_.Get();
  }

  ScrollbarTheme& GetPageScrollbarTheme() const override {
    return g_static_theme;
  }

  float ScaleFromDIP() const override { return 1.0f; }

  using ScrollableArea::ClearNeedsPaintInvalidationForScrollControls;
  using ScrollableArea::HorizontalScrollbarNeedsPaintInvalidation;
  using ScrollableArea::ShowNonMacOverlayScrollbars;
  using ScrollableArea::VerticalScrollbarNeedsPaintInvalidation;

 private:
  Member<MockChromeClientForScrollbar> chrome_client_;
  ScrollOffset scroll_offset_;
};

struct FuzzData {
  Persistent<MockScrollableAreaForFuzzer> scrollable_area;
  StubScrollbarThemeForFuzzer* theme;
  ScrollbarThemeUtils* utils;
};

static FuzzData* g_fuzz_data = nullptr;

void InitializeFuzzData() {
  if (g_fuzz_data) {
    return;
  }
  auto* data = new FuzzData();
  data->scrollable_area = MakeGarbageCollected<MockScrollableAreaForFuzzer>();
  data->theme = &g_static_theme;
  data->utils = data->theme->GetUtils().get();
  g_fuzz_data = data;
}

void FuzzOffsetPointForHitTest(FuzzedDataProvider* fdp) {
  if (!g_fuzz_data || !g_fuzz_data->utils) {
    return;
  }
  StubScrollbarThemeForFuzzer theme;
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      g_fuzz_data->scrollable_area.Get(), kVerticalScrollbar, &theme);
  if (!scrollbar) {
    return;
  }
  int x = fdp->ConsumeIntegral<int>();
  int y = fdp->ConsumeIntegral<int>();
  gfx::Point point(x, y);
  g_fuzz_data->utils->OffsetPointForHitTest(*scrollbar, point);
}

void FuzzHitTestUtils(FuzzedDataProvider* fdp) {
  if (!g_fuzz_data || !g_fuzz_data->utils) {
    return;
  }
  StubScrollbarThemeForFuzzer theme;
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      g_fuzz_data->scrollable_area.Get(), kVerticalScrollbar, &theme);
  if (!scrollbar) {
    return;
  }
  int x = fdp->ConsumeIntegral<int>();
  int y = fdp->ConsumeIntegral<int>();
  gfx::Point point(x, y);
  ScrollbarPart part = g_fuzz_data->utils->HitTestUtils(*scrollbar, point);
  (void)part;
}

void FuzzOffsetPointBoundary(FuzzedDataProvider* fdp) {
  if (!g_fuzz_data || !g_fuzz_data->utils) {
    return;
  }
  StubScrollbarThemeForFuzzer theme;
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      g_fuzz_data->scrollable_area.Get(), kVerticalScrollbar, &theme);
  if (!scrollbar) {
    return;
  }
  int boundary_values[] = {0, 1, -1, 100, -100, 1000, -1000, INT_MAX, INT_MIN};
  for (int val : boundary_values) {
    gfx::Point point(val, val);
    g_fuzz_data->utils->OffsetPointForHitTest(*scrollbar, point);
  }
}

void FuzzHitTestBoundary(FuzzedDataProvider* fdp) {
  if (!g_fuzz_data || !g_fuzz_data->utils) {
    return;
  }
  StubScrollbarThemeForFuzzer theme;
  Scrollbar* scrollbar = Scrollbar::CreateForTesting(
      g_fuzz_data->scrollable_area.Get(), kVerticalScrollbar, &theme);
  if (!scrollbar) {
    return;
  }
  int boundary_values[] = {0, 1, -1, 100, -100, 1000, -1000, INT_MAX, INT_MIN};
  for (int val : boundary_values) {
    gfx::Point point(val, val);
    g_fuzz_data->utils->HitTestUtils(*scrollbar, point);
  }
}

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
    InitializeFuzzData();
  }

  ~Environment() {}

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  int selector = fdp.ConsumeIntegralInRange<int>(0, 3);
  switch (selector) {
    case 0:
      blink::FuzzOffsetPointForHitTest(&fdp);
      break;
    case 1:
      blink::FuzzHitTestUtils(&fdp);
      break;
    case 2:
      blink::FuzzOffsetPointBoundary(&fdp);
      break;
    case 3:
      blink::FuzzHitTestBoundary(&fdp);
      break;
  }

  return 0;
}