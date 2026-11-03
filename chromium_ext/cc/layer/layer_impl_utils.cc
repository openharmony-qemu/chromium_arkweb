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

#include "arkweb/chromium_ext/cc/layer/layer_impl_utils.h"

#include "cc/layers/layer_impl.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
namespace cc {

LayerImplUtils::LayerImplUtils(LayerImpl* layer_impl)
    : layer_impl_(layer_impl),
#if BUILDFLAG(ARKWEB_SAME_LAYER)
      may_contain_native_(false),
      may_contain_video_(false),
      native_embed_id_(false)
#endif
{
}

#if BUILDFLAG(ARKWEB_SAME_LAYER)
void LayerImplUtils::LayerImplPushPropertiesTo(LayerImpl* layer) {
  layer->layer_impl_utils()->set_may_contain_native(may_contain_native_);
  layer->layer_impl_utils()->set_may_contain_video(may_contain_video_);
  layer->layer_impl_utils()->set_native_embed_id(native_embed_id_);
  layer->layer_impl_utils()->SetNativeRect(native_rect_);
  layer->layer_impl_utils()->SetInitScale(init_scale_);
  layer->layer_impl_utils()->set_native_embed_transform_rotate_and_skew_enabled(
      native_embed_transform_rotate_and_skew_enabled_);
}

gfx::RectF LayerImplUtils::NativeRect() const {
  if (!may_contain_native()) {
    return native_rect_;
  }

  auto viewport_bounds_delta = gfx::ToCeiledVector2d(
      layer_impl_->GetPropertyTrees()->inner_viewport_scroll_bounds_delta());
  return gfx::RectF(native_rect_.x(), native_rect_.y(),
                    native_rect_.width() + viewport_bounds_delta.x(),
                    native_rect_.height() + viewport_bounds_delta.y());
}

void LayerImplUtils::SetNativeRect(const gfx::RectF& rect) {
  if (native_rect_ == rect) {
    return;
  }
  native_rect_ = rect;
  if (init_scale_ == -1.0f) {
    init_scale_ = layer_impl_->GetIdealContentsScaleKey();
  }

  layer_impl_->NoteLayerPropertyChanged();
}

void LayerImplUtils::SetInitScale(float scale) {
  if (init_scale_ == scale) {
    return;
  }
  init_scale_ = scale;
}

gfx::RectF LayerImplUtils::GetNativeRect() {
  if (!may_contain_native()) {
    return native_rect_;
  }
  gfx::Transform transform = layer_impl_->ScreenSpaceTransform();
  gfx::RectF rf = NativeRect();
  if (rf.IsEmpty()) {
    rf.set_width(layer_impl_->bounds().width());
    rf.set_height(layer_impl_->bounds().height());
  }
  if (native_embed_transform_rotate_and_skew_enabled_ &&
      !transform.IsScaleOrTranslation()) {
    gfx::PointF mapped_origin = transform.MapPoint(rf.origin());
    gfx::Vector2dF scale =
        gfx::ComputeTransform2dScaleComponents(transform, 1.0f);
    rf.set_origin(mapped_origin);
    rf.set_width(rf.width() * scale.x());
    rf.set_height(rf.height() * scale.y());
    return rf;
  }
  return transform.MapRect(rf);
}
#endif

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
void LayerImplUtils::SetShouldInterceptTouchEvent(bool intercept) {
  should_intercept_touch_event_ = intercept;
}

bool LayerImplUtils::ShouldInterceptTouchEvent() const {
  return should_intercept_touch_event_;
}
#endif

#if BUILDFLAG(ARKWEB_SCROLLBAR)
void LayerImplUtils::PopulateSharedQuadStateForScrollLayer(
    viz::SharedQuadState* state,
    bool contents_opaque,
    gfx::RRectF& rounded_corner_bounds) const {
  EffectNode* effect_node =
      layer_impl_->GetEffectTree().Node(layer_impl_->effect_tree_index());
  std::optional<gfx::Rect> clip_rect;
  if (layer_impl_->draw_properties().is_clipped) {
    clip_rect = layer_impl_->draw_properties().clip_rect;
  }
  gfx::MaskFilterInfo mask_filter_info(rounded_corner_bounds);
  state->SetAll(
      layer_impl_->draw_properties().target_space_transform,
      gfx::Rect(layer_impl_->bounds()),
      layer_impl_->draw_properties().visible_layer_rect, mask_filter_info,
      clip_rect, contents_opaque, layer_impl_->draw_properties().opacity,
      effect_node->HasRenderSurface() ? SkBlendMode::kSrcOver
                                      : effect_node->blend_mode,
      layer_impl_->GetSortingContextId(),
      static_cast<uint32_t>(layer_impl_->stable_id_for_shared_quad_state()),
      layer_impl_->draw_properties().is_fast_rounded_corner);
  state->is_fast_rounded_corner = true;
}
#endif  // BUILDFLAG(ARKWEB_SCROLLBAR)
}  // namespace cc
