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

#include "arkweb/build/features/features.h"
#include "base/memory/raw_ptr.h"
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#define private public
#include "arkweb/chromium_ext/cc/layer/layer_impl_utils.h"
#undef private
#include "arkweb/chromium_ext/cc/layer/layer_utils.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

class LayerImplUtilsTest : public LayerTreeImplTestBase, public testing::Test {
 public:
  using LayerTreeImplTestBase::LayerTreeImplTestBase;
  void SetUp() override {
    host_impl()->CreatePendingTree();
    layer_impl_ = EnsureRootLayerInPendingTree();
    utils_ = std::make_unique<LayerImplUtils>(layer_impl_);
  }
  LayerImpl* layer_impl_;
  std::unique_ptr<LayerImplUtils> utils_;
};

TEST_F(LayerImplUtilsTest, SetNativeRect001) {
  gfx::RectF original_rect(10, 20, 30, 40);
  gfx::RectF new_rect(50, 60, 70, 80);
  utils_->SetNativeRect(original_rect);
  utils_->SetNativeRect(new_rect);
  EXPECT_EQ(utils_->NativeRect(), new_rect);
}

TEST_F(LayerImplUtilsTest, SetNativeRect002) {
  gfx::RectF test_rect(10, 20, 30, 40);
  utils_->SetNativeRect(test_rect);
  utils_->SetNativeRect(test_rect);
  EXPECT_EQ(utils_->NativeRect(), test_rect);
}

TEST_F(LayerImplUtilsTest, SetInitScale001) {
  const float kOriginalScale = 1.0f;
  const float kNewScale = 2.0f;
  utils_->SetInitScale(kOriginalScale);
  utils_->SetInitScale(kNewScale);
  EXPECT_EQ(utils_->GetInitScale(), kNewScale);
}

TEST_F(LayerImplUtilsTest, SetInitScale002) {
  const float kTestScale = 1.5f;
  utils_->SetInitScale(kTestScale);
  utils_->SetInitScale(kTestScale);
  EXPECT_EQ(utils_->GetInitScale(), kTestScale);
}

TEST_F(LayerImplUtilsTest, NativeRect001) {
  gfx::RectF test_rect(5, 10, 15, 20);
  utils_->SetNativeRect(test_rect);
  EXPECT_EQ(utils_->NativeRect(), test_rect);
}

TEST_F(LayerImplUtilsTest, GetNativeRect001) {
  gfx::RectF test_rect(5, 10, 15, 20);
  utils_->SetNativeRect(test_rect);
  EXPECT_EQ(utils_->GetNativeRect(), test_rect);
}

TEST_F(LayerImplUtilsTest, GetNativeRect002) {
  gfx::RectF test_rect(5, 10, 15, 20);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_EQ(result, utils_->native_rect_);
}

TEST_F(LayerImplUtilsTest, GetNativeRect003) {
  gfx::RectF test_rect(0, 0, 0, 0);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  test_rect.set_width(utils_->layer_impl_->bounds().width());
  test_rect.set_height(utils_->layer_impl_->bounds().height());
  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_EQ(result, test_rect);
}

TEST_F(LayerImplUtilsTest, NativeRect002) {
  gfx::RectF test_rect(5, 10, 15, 20);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  gfx::RectF result = utils_->NativeRect();
  EXPECT_EQ(result, utils_->native_rect_);
}

TEST_F(LayerImplUtilsTest, PopulateSharedQuadStateForScrollLayer_Basic) {
  viz::SharedQuadState state;
  gfx::RRectF rounded_corner_bounds(gfx::RectF(0, 0, 100, 100), 10.0f);

  layer_impl_->SetBounds(gfx::Size(100, 100));

  utils_->PopulateSharedQuadStateForScrollLayer(&state, true,
                                                rounded_corner_bounds);

  EXPECT_EQ(state.quad_layer_rect, gfx::Rect(100, 100));
  EXPECT_TRUE(state.are_contents_opaque);
  EXPECT_TRUE(state.is_fast_rounded_corner);
}

#if BUILDFLAG(ARKWEB_SAME_LAYER)
static void SetupScreenSpaceTransform(LayerImpl* layer,
                                      const gfx::Transform& transform) {
  layer->set_contributes_to_drawn_render_surface(true);
  layer->draw_properties().screen_space_transform = transform;
}

TEST_F(LayerImplUtilsTest, TransformRotateAndSkew_SetterGetter) {
  EXPECT_FALSE(utils_->native_embed_transform_rotate_and_skew_enabled());
  utils_->set_native_embed_transform_rotate_and_skew_enabled(true);
  EXPECT_TRUE(utils_->native_embed_transform_rotate_and_skew_enabled());
  utils_->set_native_embed_transform_rotate_and_skew_enabled(false);
  EXPECT_FALSE(utils_->native_embed_transform_rotate_and_skew_enabled());
}

TEST_F(LayerImplUtilsTest, TransformRotateAndSkew_DefaultValue) {
  auto fresh_utils = std::make_unique<LayerImplUtils>(layer_impl_);
  EXPECT_FALSE(fresh_utils->native_embed_transform_rotate_and_skew_enabled());
}

TEST_F(LayerImplUtilsTest, LayerImplPushPropertiesTo_PropagatesFlag) {
  utils_->set_native_embed_transform_rotate_and_skew_enabled(true);
  utils_->set_may_contain_native(true);
  utils_->set_may_contain_video(false);
  utils_->set_native_embed_id(42);

  LayerImpl* target = AddLayerInActiveTree<LayerImpl>();
  ASSERT_TRUE(target);

  utils_->LayerImplPushPropertiesTo(target);
  EXPECT_TRUE(target->layer_impl_utils()
                  ->native_embed_transform_rotate_and_skew_enabled());
  EXPECT_TRUE(target->layer_impl_utils()->may_contain_native());
  EXPECT_FALSE(target->layer_impl_utils()->may_contain_video());
  EXPECT_EQ(target->layer_impl_utils()->native_embed_id(), 42);
}

TEST_F(LayerImplUtilsTest, LayerImplPushPropertiesTo_FlagOff) {
  utils_->set_native_embed_transform_rotate_and_skew_enabled(false);
  utils_->set_may_contain_native(true);

  LayerImpl* target = AddLayerInActiveTree<LayerImpl>();
  ASSERT_TRUE(target);

  utils_->LayerImplPushPropertiesTo(target);
  EXPECT_FALSE(target->layer_impl_utils()
                   ->native_embed_transform_rotate_and_skew_enabled());
}

TEST_F(LayerImplUtilsTest, GetNativeRect_RotateSkewFlagOff_UsesMapRect) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = false;

  gfx::Transform rotate;
  rotate.Rotate(90.0);
  SetupScreenSpaceTransform(layer_impl_, rotate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.width(), 50.0f);
  EXPECT_FLOAT_EQ(result.height(), 100.0f);
}

TEST_F(LayerImplUtilsTest, GetNativeRect_RotateSkewFlagOff_IdentityTransform) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = false;

  gfx::Transform identity;
  SetupScreenSpaceTransform(layer_impl_, identity);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_EQ(result, test_rect);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOff_TranslateTransform) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = false;

  gfx::Transform translate;
  translate.Translate(30.0f, 40.0f);
  SetupScreenSpaceTransform(layer_impl_, translate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 40.0f);
  EXPECT_FLOAT_EQ(result.y(), 60.0f);
  EXPECT_FLOAT_EQ(result.width(), 100.0f);
  EXPECT_FLOAT_EQ(result.height(), 50.0f);
}

TEST_F(LayerImplUtilsTest, GetNativeRect_RotateSkewFlagOn_ScaleTransform) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform scale;
  scale.Scale(2.0f, 3.0f);
  SetupScreenSpaceTransform(layer_impl_, scale);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 20.0f);
  EXPECT_FLOAT_EQ(result.y(), 60.0f);
  EXPECT_FLOAT_EQ(result.width(), 200.0f);
  EXPECT_FLOAT_EQ(result.height(), 150.0f);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_IdentityTransform) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform identity;
  SetupScreenSpaceTransform(layer_impl_, identity);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_EQ(result, test_rect);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_TranslateTransform) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform translate;
  translate.Translate(30.0f, 40.0f);
  SetupScreenSpaceTransform(layer_impl_, translate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 40.0f);
  EXPECT_FLOAT_EQ(result.y(), 60.0f);
  EXPECT_FLOAT_EQ(result.width(), 100.0f);
  EXPECT_FLOAT_EQ(result.height(), 50.0f);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_ScaleAndTranslate) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform scale_translate;
  scale_translate.Scale(2.0f, 2.0f);
  scale_translate.Translate(30.0f, 40.0f);
  SetupScreenSpaceTransform(layer_impl_, scale_translate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 80.0f);
  EXPECT_FLOAT_EQ(result.y(), 120.0f);
  EXPECT_FLOAT_EQ(result.width(), 200.0f);
  EXPECT_FLOAT_EQ(result.height(), 100.0f);
}

TEST_F(LayerImplUtilsTest, GetNativeRect_RotateSkewFlagOn_RotateTransform) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform rotate;
  rotate.Rotate(90.0);
  SetupScreenSpaceTransform(layer_impl_, rotate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), -20.0f);
  EXPECT_FLOAT_EQ(result.y(), 10.0f);
  EXPECT_FLOAT_EQ(result.width(), 100.0f);
  EXPECT_FLOAT_EQ(result.height(), 50.0f);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_Rotate180Transform) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform rotate;
  rotate.Rotate(180.0);
  SetupScreenSpaceTransform(layer_impl_, rotate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.width(), 100.0f);
  EXPECT_FLOAT_EQ(result.height(), 50.0f);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_Rotate45Transform) {
  gfx::RectF test_rect(0, 0, 100, 100);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform rotate;
  rotate.Rotate(45.0);
  SetupScreenSpaceTransform(layer_impl_, rotate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 0.0f);
  EXPECT_FLOAT_EQ(result.y(), 0.0f);
  EXPECT_FLOAT_EQ(result.width(), 100.0f);
  EXPECT_FLOAT_EQ(result.height(), 100.0f);
}

TEST_F(LayerImplUtilsTest, GetNativeRect_RotateSkewFlagOn_SkewTransform) {
  gfx::RectF test_rect(0, 0, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform skew;
  skew.SkewX(30.0);
  SetupScreenSpaceTransform(layer_impl_, skew);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 0.0f);
  EXPECT_FLOAT_EQ(result.y(), 0.0f);
  EXPECT_NEAR(result.width(), 100.0f, 0.01f);
  EXPECT_NEAR(result.height(), 50.0f / cos(30.0 * M_PI / 180.0), 0.01f);
}

TEST_F(LayerImplUtilsTest, GetNativeRect_RotateSkewFlagOn_SkewYTransform) {
  gfx::RectF test_rect(0, 0, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform skew;
  skew.SkewY(45.0);
  SetupScreenSpaceTransform(layer_impl_, skew);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 0.0f);
  EXPECT_FLOAT_EQ(result.y(), 0.0f);
}

TEST_F(LayerImplUtilsTest, GetNativeRect_RotateSkewFlagOn_SkewBoth) {
  gfx::RectF test_rect(0, 0, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform skew;
  skew.Skew(20.0, 15.0);
  SetupScreenSpaceTransform(layer_impl_, skew);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 0.0f);
  EXPECT_FLOAT_EQ(result.y(), 0.0f);
  EXPECT_GT(result.width(), 100.0f);
  EXPECT_GT(result.height(), 50.0f);
}

TEST_F(LayerImplUtilsTest, GetNativeRect_RotateSkewFlagOn_RotateAndScale) {
  gfx::RectF test_rect(0, 0, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform rotate_scale;
  rotate_scale.Scale(2.0f, 3.0f);
  rotate_scale.Rotate(90.0);
  SetupScreenSpaceTransform(layer_impl_, rotate_scale);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 0.0f);
  EXPECT_FLOAT_EQ(result.y(), 0.0f);
  EXPECT_FLOAT_EQ(result.width(), 300.0f);
  EXPECT_FLOAT_EQ(result.height(), 100.0f);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_RotateAndTranslate) {
  gfx::RectF test_rect(0, 0, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform rotate_translate;
  rotate_translate.Translate(50.0f, 60.0f);
  rotate_translate.Rotate(90.0);
  SetupScreenSpaceTransform(layer_impl_, rotate_translate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 50.0f);
  EXPECT_FLOAT_EQ(result.y(), 60.0f);
  EXPECT_FLOAT_EQ(result.width(), 100.0f);
  EXPECT_FLOAT_EQ(result.height(), 50.0f);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_RotateScaleTranslate) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform combined;
  combined.Translate(50.0f, 60.0f);
  combined.Scale(2.0f, 3.0f);
  combined.Rotate(90.0);
  SetupScreenSpaceTransform(layer_impl_, combined);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 10.0f);
  EXPECT_FLOAT_EQ(result.y(), 90.0f);
  EXPECT_FLOAT_EQ(result.width(), 300.0f);
  EXPECT_FLOAT_EQ(result.height(), 100.0f);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_EmptyRectUsesBounds) {
  gfx::RectF test_rect(0, 0, 0, 0);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  layer_impl_->SetBounds(gfx::Size(200, 100));

  gfx::Transform rotate;
  rotate.Rotate(90.0);
  SetupScreenSpaceTransform(layer_impl_, rotate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 0.0f);
  EXPECT_FLOAT_EQ(result.y(), 0.0f);
  EXPECT_FLOAT_EQ(result.width(), 200.0f);
  EXPECT_FLOAT_EQ(result.height(), 100.0f);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_EmptyRectWithScaleTransform) {
  gfx::RectF test_rect(0, 0, 0, 0);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = true;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  layer_impl_->SetBounds(gfx::Size(200, 100));

  gfx::Transform scale;
  scale.Scale(2.0f, 3.0f);
  SetupScreenSpaceTransform(layer_impl_, scale);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_FLOAT_EQ(result.x(), 0.0f);
  EXPECT_FLOAT_EQ(result.y(), 0.0f);
  EXPECT_FLOAT_EQ(result.width(), 400.0f);
  EXPECT_FLOAT_EQ(result.height(), 300.0f);
}

TEST_F(LayerImplUtilsTest,
       GetNativeRect_RotateSkewFlagOn_NotMayContainNative) {
  gfx::RectF test_rect(10, 20, 100, 50);
  utils_->native_rect_ = test_rect;
  utils_->may_contain_native_ = false;
  utils_->native_embed_transform_rotate_and_skew_enabled_ = true;

  gfx::Transform rotate;
  rotate.Rotate(90.0);
  SetupScreenSpaceTransform(layer_impl_, rotate);

  gfx::RectF result = utils_->GetNativeRect();
  EXPECT_EQ(result, test_rect);
}
#endif  // BUILDFLAG(ARKWEB_SAME_LAYER)
}  // namespace
}  // namespace cc
