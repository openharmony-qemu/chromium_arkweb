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

#include "arkweb/chromium_ext/third_party/blink/renderer/core/frame/arkweb_visual_viewport_utils.h"

#include "base/feature_list.h"
#include "cc/base/features.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"

namespace blink {

ArkWebVisualViewportUtils::ArkWebVisualViewportUtils(
    const VisualViewport* viewport)
    : viewport_(viewport) {
  DCHECK(viewport);
}

ArkWebVisualViewportUtils::~ArkWebVisualViewportUtils() = default;

void ArkWebVisualViewportUtils::
    TriggerUpdateAfterLayoutForNonOverlayScrollbar() {
  if (!viewport_) {
    return;
  }

  const Document* document = viewport_->GetDocument();
  if (!document) {
    return;
  }

  const LocalFrame* local_frame = document->GetFrame();
  if (!local_frame) {
    return;
  }

  LocalFrameView* view = local_frame->View();
  if (!view) {
    return;
  }

  PaintLayerScrollableArea* layout_viewport = view->LayoutViewport();
  if (!layout_viewport) {
    return;
  }

  bool has_non_overlay_scrollbar = false;

  if (Scrollbar* horizontal_scrollbar =
          layout_viewport->HorizontalScrollbar()) {
    if (!horizontal_scrollbar->IsOverlayScrollbar()) {
      has_non_overlay_scrollbar = true;
    }
  }

  if (!has_non_overlay_scrollbar) {
    if (Scrollbar* vertical_scrollbar = layout_viewport->VerticalScrollbar()) {
      if (!vertical_scrollbar->IsOverlayScrollbar()) {
        has_non_overlay_scrollbar = true;
      }
    }
  }

  if (has_non_overlay_scrollbar) {
    view->SetNeedsLayout();
  }
}
void ArkWebVisualViewportUtils::Trace(Visitor* visitor) const {
  visitor->Trace(viewport_);
}

ArkWebVisualViewportUtils::ScrollbarAvoidData
ArkWebVisualViewportUtils::GetScrollbarAvoidData() {
  if (!viewport_ || !viewport_->GetDocument() ||
      !viewport_->GetDocument()->GetFrame() ||
      !viewport_->GetDocument()->GetFrame()->GetSettings() ||
      !viewport_->GetDocument()->GetFrame()->View() ||
      !viewport_->GetDocument()->GetFrame()->View()->LayoutViewport()) {
    return {0, 0, 0, 0};
  }
  auto settings = viewport_->GetDocument()->GetFrame()->GetSettings();
  auto borderRadiusTopRight =
      settings->GetBorderRadiusTopRight() * viewport_->ScaleFromDIP();
  auto borderRadiusTopLeft =
      settings->GetBorderRadiusTopRight() * viewport_->ScaleFromDIP();
  auto borderRadiusBottomRight =
      settings->GetBorderRadiusBottomRight() * viewport_->ScaleFromDIP();
  auto borderRadiusBottomLeft =
      settings->GetBorderRadiusBottomLeft() * viewport_->ScaleFromDIP();
  auto scrollbar_avoid_area_top_ = viewport_->GetDocument()
                                       ->GetFrame()
                                       ->View()
                                       ->LayoutViewport()
                                       ->AsPaintLayerScrollableAreaExt()
                                       ->GetScrollbarAvoidAreaTopOrigin();
  auto scrollbar_avoid_area_bottom_ = viewport_->GetDocument()
                                          ->GetFrame()
                                          ->View()
                                          ->LayoutViewport()
                                          ->AsPaintLayerScrollableAreaExt()
                                          ->GetScrollbarAvoidAreaBottomOrigin();
  auto verticalScrollbarTopAvoidArea =
      scrollbar_avoid_area_top_ > borderRadiusTopRight
          ? scrollbar_avoid_area_top_
          : borderRadiusTopRight;
  auto verticalScrollbarBootomAvoidArea =
      scrollbar_avoid_area_bottom_ > borderRadiusBottomRight
          ? scrollbar_avoid_area_bottom_
          : borderRadiusBottomRight;
  ScrollbarAvoidData avoidData = {
      .verticalTopAvoidArea = verticalScrollbarTopAvoidArea,
      .verticalBootomAvoidArea = verticalScrollbarBootomAvoidArea,
      .horizontalBottomaAvoidRight = borderRadiusBottomRight,
      .horizontalBottomaAvoidLeft = borderRadiusBottomLeft,
  };
  return avoidData;
}

gfx::Point ArkWebVisualViewportUtils::GetScrollbarLayerOrigin(
    bool is_horizontal,
    int scrollbar_thickness) {
  if (!viewport_ || !viewport_->GetDocument() ||
      !viewport_->GetDocument()->GetFrame() ||
      !viewport_->GetDocument()->GetFrame()->GetSettings()) {
    if (is_horizontal) {
      return gfx::Point(0, viewport_->Size().height() - scrollbar_thickness);
    } else {
      return gfx::Point(viewport_->Size().width() - scrollbar_thickness, 0);
    }
  }
  auto settings = viewport_->GetDocument()->GetFrame()->GetSettings();
  ScrollbarAvoidData avoidData = GetScrollbarAvoidData();
  gfx::Size size = viewport_->Size();
  bool isSystemRtlEnabled = settings->GetIsSystemRtlEnabled();
  LayoutPolicy layoutPolicy =
      static_cast<LayoutPolicy>(settings->GetScrollbarLayoutPolicy());
  if (is_horizontal) {
    return gfx::Point(
        avoidData.horizontalBottomaAvoidLeft,
        size.height() - scrollbar_thickness - viewport_->ScaleFromDIP() * 4);
  } else {
    if (isSystemRtlEnabled && layoutPolicy == LayoutPolicy::kSystem) {
      return gfx::Point(0,
                        avoidData.verticalTopAvoidArea);
    }
    return gfx::Point(
        size.width() - scrollbar_thickness - 4 * viewport_->ScaleFromDIP(),
        avoidData.verticalTopAvoidArea);
  }
}

gfx::Size ArkWebVisualViewportUtils::GetScrollbarLayerBounds(
    bool is_horizontal,
    int scrollbar_thickness) {
  ScrollbarAvoidData avoidData = GetScrollbarAvoidData();
  gfx::Size size = viewport_->Size();

  if (is_horizontal) {
    return gfx::Size(size.width() - scrollbar_thickness -
                         avoidData.horizontalBottomaAvoidRight -
                         avoidData.horizontalBottomaAvoidLeft,
                     scrollbar_thickness * 2);
  } else {
    return gfx::Size(scrollbar_thickness * 2,
                     size.height() - scrollbar_thickness -
                         avoidData.verticalTopAvoidArea -
                         avoidData.verticalBootomAvoidArea);
  }
}

void ArkWebVisualViewportUtils::SetupOverlayVisualScrollbarLayer(
    cc::SolidColorScrollbarLayer& layer,
    cc::Layer* scroll_layer) {
  if (base::FeatureList::IsEnabled(::features::kOverlayVisualScrollbar) &&
      scroll_layer) {
    layer.SetScrollElementId(scroll_layer->element_id());
    layer.SetIsDrawable(true);
  }
}

void ArkWebVisualViewportUtils::UpdateScrollbarColorForLayer(
    cc::SolidColorScrollbarLayer& layer) {
  if (viewport_->GetOverlayScrollbarColorScheme() ==
      static_cast<mojom::blink::ColorScheme>(
          mojom::blink::ColorScheme::kLight)) {
    layer.SetColor(SkColor4f::FromColor(SkColorSetA(SK_ColorBLACK, 102)));
  } else {
    layer.SetColor(SkColor4f::FromColor(SkColorSetA(SK_ColorWHITE, 102)));
  }
}

}  // namespace blink
