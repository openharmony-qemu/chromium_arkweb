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

#include "arkweb/build/features/features.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/layers/layer.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#if BUILDFLAG(ARKWEB_TEST)
#define private public
#include "arkweb/chromium_ext/third_party/blink/renderer/core/frame/arkweb_visual_viewport_utils.h"
#endif

namespace blink {

namespace {
#if BUILDFLAG(ARKWEB_TEST)
TEST_P(VisualViewportTest,
       TriggerUpdateAfterLayoutForNonOverlayScrollbar_NullViewport) {
  auto utils = MakeGarbageCollected<ArkWebVisualViewportUtils>(nullptr);
  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();

  const ArkWebVisualViewportUtils::ScrollbarAvoidData avoid_data =
      utils->GetScrollbarAvoidData();
  EXPECT_EQ(avoid_data.verticalTopAvoidArea, 0);
  EXPECT_EQ(avoid_data.verticalBootomAvoidArea, 0);
  EXPECT_EQ(avoid_data.horizontalBottomaAvoidRight, 0);
  EXPECT_EQ(avoid_data.horizontalBottomaAvoidLeft, 0);
}

TEST_P(VisualViewportTest,
       TriggerUpdateAfterLayoutForNonOverlayScrollbar_WithNonOverlayScrollbar) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  LocalFrameView* view = WebView()->MainFrameImpl()->GetFrameView();
  ASSERT_NE(view, nullptr);

  PaintLayerScrollableArea* layout_viewport = view->LayoutViewport();
  ASSERT_NE(layout_viewport, nullptr);

  bool has_scrollbar_before = layout_viewport->HasScrollbar();
  ASSERT_TRUE(has_scrollbar_before);

  bool needs_layout_before = view->NeedsLayout();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);
  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();

  bool needs_layout_after = view->NeedsLayout();
  ASSERT_TRUE(needs_layout_after);

  if (!needs_layout_before) {
    ASSERT_TRUE(needs_layout_after);
  }
}

TEST_P(VisualViewportTest,
       TriggerUpdateAfterLayoutForNonOverlayScrollbar_WithOverlayScrollbar) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  LocalFrameView* view = WebView()->MainFrameImpl()->GetFrameView();
  ASSERT_NE(view, nullptr);

  PaintLayerScrollableArea* layout_viewport = view->LayoutViewport();
  ASSERT_NE(layout_viewport, nullptr);

  bool needs_layout_before = view->NeedsLayout();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);
  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();

  bool needs_layout_after = view->NeedsLayout();

  ASSERT_EQ(needs_layout_before, needs_layout_after);
}

TEST_P(VisualViewportTest,
       TriggerUpdateAfterLayoutForNonOverlayScrollbar_WithoutScrollbar) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  LocalFrameView* view = WebView()->MainFrameImpl()->GetFrameView();
  ASSERT_NE(view, nullptr);

  PaintLayerScrollableArea* layout_viewport = view->LayoutViewport();
  ASSERT_NE(layout_viewport, nullptr);

  bool has_scrollbar_before = layout_viewport->HasScrollbar();
  ASSERT_FALSE(has_scrollbar_before);

  bool needs_layout_before = view->NeedsLayout();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);
  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();

  bool needs_layout_after = view->NeedsLayout();

  ASSERT_EQ(needs_layout_before, needs_layout_after);
}

TEST_P(VisualViewportTest,
       TriggerUpdateAfterLayoutForNonOverlayScrollbar_MultipleCalls) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  LocalFrameView* view = WebView()->MainFrameImpl()->GetFrameView();
  ASSERT_NE(view, nullptr);

  PaintLayerScrollableArea* layout_viewport = view->LayoutViewport();
  ASSERT_NE(layout_viewport, nullptr);

  bool has_scrollbar = layout_viewport->HasScrollbar();
  ASSERT_TRUE(has_scrollbar);

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);

  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();
  ASSERT_TRUE(view->NeedsLayout());

  UpdateAllLifecyclePhases();

  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();
  ASSERT_TRUE(view->NeedsLayout());

  UpdateAllLifecyclePhases();

  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();
  ASSERT_TRUE(view->NeedsLayout());
}

TEST_P(VisualViewportTest,
       TriggerUpdateAfterLayoutForNonOverlayScrollbar_LifecycleUpdate) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  LocalFrameView* view = WebView()->MainFrameImpl()->GetFrameView();
  ASSERT_NE(view, nullptr);

  PaintLayerScrollableArea* layout_viewport = view->LayoutViewport();
  ASSERT_NE(layout_viewport, nullptr);

  bool has_scrollbar = layout_viewport->HasScrollbar();
  ASSERT_TRUE(has_scrollbar);

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);

  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();

  ASSERT_TRUE(view->NeedsLayout());

  UpdateAllLifecyclePhases();

  ASSERT_FALSE(view->NeedsLayout());

  bool has_scrollbar_after = layout_viewport->HasScrollbar();
  ASSERT_TRUE(has_scrollbar_after);
}

TEST_P(VisualViewportTest,
       TriggerUpdateAfterLayoutForNonOverlayScrollbar_HorizontalScrollbarOnly) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  frame_test_helpers::LoadHTMLString(WebView()->MainFrameImpl(),
                                     R"HTML(
        <!DOCTYPE html>
        <style>
          html { overflow-x: scroll; overflow-y: hidden; }
          body { margin: 0; }
        </style>
        <div style="width: 800px; height: 1px"></div>
      )HTML",
                                     url_test_helpers::ToKURL("about:blank"));
  ForceFullCompositingUpdate();

  LocalFrameView* view = WebView()->MainFrameImpl()->GetFrameView();
  ASSERT_NE(view, nullptr);

  PaintLayerScrollableArea* layout_viewport = view->LayoutViewport();
  ASSERT_NE(layout_viewport, nullptr);

  Scrollbar* horizontal_scrollbar = layout_viewport->HorizontalScrollbar();
  ASSERT_NE(horizontal_scrollbar, nullptr);
  ASSERT_FALSE(horizontal_scrollbar->IsOverlayScrollbar());
  ASSERT_FALSE(layout_viewport->HasVerticalScrollbar());
  ASSERT_FALSE(view->NeedsLayout());

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);

  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();

  ASSERT_TRUE(view->NeedsLayout());
}

TEST_P(VisualViewportTest,
       TriggerUpdateAfterLayoutForNonOverlayScrollbar_VerticalScrollbarOnly) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  LocalFrameView* view = WebView()->MainFrameImpl()->GetFrameView();
  ASSERT_NE(view, nullptr);

  PaintLayerScrollableArea* layout_viewport = view->LayoutViewport();
  ASSERT_NE(layout_viewport, nullptr);

  bool has_vertical_scrollbar = layout_viewport->HasVerticalScrollbar();
  ASSERT_TRUE(has_vertical_scrollbar);

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);

  utils->TriggerUpdateAfterLayoutForNonOverlayScrollbar();

  ASSERT_TRUE(view->NeedsLayout());
}

TEST_P(VisualViewportTest, GetScrollbarLayerOrigin_Horizontal) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);

  int scrollbar_thickness = 10;
  gfx::Point origin = utils->GetScrollbarLayerOrigin(true, scrollbar_thickness);

  ASSERT_GE(origin.x(), 0);
  ASSERT_GE(origin.y(), 0);
  ASSERT_LT(origin.y(), 300);
}

TEST_P(VisualViewportTest, GetScrollbarLayerOrigin_Vertical) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);

  int scrollbar_thickness = 10;
  gfx::Point origin =
      utils->GetScrollbarLayerOrigin(false, scrollbar_thickness);

  ASSERT_GE(origin.x(), 0);
  ASSERT_LT(origin.x(), 400);
  ASSERT_GE(origin.y(), 0);

  Settings* settings = GetFrame()->GetSettings();
  ASSERT_NE(settings, nullptr);
  settings->SetIsSystemRtlEnable(true);
  settings->SetScrollbarLayoutPolicy(static_cast<int>(LayoutPolicy::kSystem));
  origin = utils->GetScrollbarLayerOrigin(false, scrollbar_thickness);
  EXPECT_EQ(origin.x(), 0);
}

TEST_P(VisualViewportTest, GetScrollbarLayerBounds_Horizontal) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);

  int scrollbar_thickness = 10;
  gfx::Size bounds = utils->GetScrollbarLayerBounds(true, scrollbar_thickness);

  ASSERT_GT(bounds.width(), 0);
  ASSERT_GT(bounds.height(), 0);
  ASSERT_LE(bounds.width(), 400);
}

TEST_P(VisualViewportTest, GetScrollbarLayerBounds_Vertical) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(400, 300), gfx::Size(400, 300),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);

  int scrollbar_thickness = 10;
  gfx::Size bounds = utils->GetScrollbarLayerBounds(false, scrollbar_thickness);

  ASSERT_GT(bounds.width(), 0);
  ASSERT_GT(bounds.height(), 0);
  ASSERT_LE(bounds.height(), 300);
}

TEST_P(VisualViewportTest, UpdateScrollbarColorForLayer_LightScheme) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 300));
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);

  auto layer = cc::SolidColorScrollbarLayer::Create(
      cc::ScrollbarOrientation::kHorizontal, 10, 0, false);

  utils->UpdateScrollbarColorForLayer(*layer);

  const SkColor4f color = layer->color();
  EXPECT_FLOAT_EQ(color.fR, 0.0f);
  EXPECT_FLOAT_EQ(color.fG, 0.0f);
  EXPECT_FLOAT_EQ(color.fB, 0.0f);
  EXPECT_FLOAT_EQ(color.fA, 102.0f / 255.0f);
}

TEST_P(VisualViewportTest, UpdateScrollbarColorForLayer_DarkScheme) {
  InitializeWithDesktopSettings();
  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetOverlayScrollbarColorScheme(
      mojom::blink::ColorScheme::kDark);
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);
  auto layer = cc::SolidColorScrollbarLayer::Create(
      cc::ScrollbarOrientation::kHorizontal, 10, 0, false);

  utils->UpdateScrollbarColorForLayer(*layer);

  const SkColor4f color = layer->color();
  EXPECT_FLOAT_EQ(color.fR, 1.0f);
  EXPECT_FLOAT_EQ(color.fG, 1.0f);
  EXPECT_FLOAT_EQ(color.fB, 1.0f);
  EXPECT_FLOAT_EQ(color.fA, 102.0f / 255.0f);
}

TEST_P(VisualViewportTest, SetupOverlayVisualScrollbarLayer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kOverlayVisualScrollbar);
  InitializeWithDesktopSettings();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);
  auto scrollbar_layer = cc::SolidColorScrollbarLayer::Create(
      cc::ScrollbarOrientation::kVertical, 10, 0, false);
  scoped_refptr<cc::Layer> scroll_layer = cc::Layer::Create();
  const cc::ElementId element_id(17);
  scroll_layer->SetElementId(element_id);

  utils->SetupOverlayVisualScrollbarLayer(*scrollbar_layer, scroll_layer.get());

  EXPECT_EQ(scrollbar_layer->scroll_element_id(), element_id);
  EXPECT_TRUE(scrollbar_layer->draws_content());
}

TEST_P(VisualViewportTest, SetupOverlayVisualScrollbarLayerRejectsNullLayer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kOverlayVisualScrollbar);
  InitializeWithDesktopSettings();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);
  auto scrollbar_layer = cc::SolidColorScrollbarLayer::Create(
      cc::ScrollbarOrientation::kVertical, 10, 0, false);

  utils->SetupOverlayVisualScrollbarLayer(*scrollbar_layer, nullptr);

  EXPECT_EQ(scrollbar_layer->scroll_element_id(), cc::ElementId());
}

TEST_P(VisualViewportTest, SetupOverlayVisualScrollbarLayerFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      ::features::kOverlayVisualScrollbar);
  InitializeWithDesktopSettings();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto utils =
      MakeGarbageCollected<ArkWebVisualViewportUtils>(&visual_viewport);
  auto scrollbar_layer = cc::SolidColorScrollbarLayer::Create(
      cc::ScrollbarOrientation::kVertical, 10, 0, false);
  scoped_refptr<cc::Layer> scroll_layer = cc::Layer::Create();
  scroll_layer->SetElementId(cc::ElementId(23));

  utils->SetupOverlayVisualScrollbarLayer(*scrollbar_layer, scroll_layer.get());

  EXPECT_EQ(scrollbar_layer->scroll_element_id(), cc::ElementId());
  EXPECT_FALSE(scrollbar_layer->draws_content());
}
#endif
}  // namespace
}  // namespace blink
