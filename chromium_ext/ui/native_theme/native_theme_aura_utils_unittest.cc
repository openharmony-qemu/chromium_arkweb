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
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>

#include "base/containers/flat_map.h"
#define private public
#define protected public
#include "arkweb/chromium_ext/ui/native_theme/native_theme_aura_utils.h"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_sys_info_util_ext.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/record_paint_canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_aura.h"

using namespace testing;

namespace ui {
using testing::Return;
extern int scrollbar_hot_size_;

class NativeThemeAuraUtilsTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  std::shared_ptr<NativeThemeAuraUtils> native_theme_aura_utils_;
  std::shared_ptr<NativeThemeAura> native_theme_aura_;
  float original_pixel_ratio_ = 1.0f;
  int original_scrollbar_hot_size_ = 0;
};

void NativeThemeAuraUtilsTest::SetUp() {
  original_pixel_ratio_ = base::ohos::GetPixelRatio();
  original_scrollbar_hot_size_ = scrollbar_hot_size_;
  base::ohos::SetPixelRatio(1.0f);
  native_theme_aura_ = std::make_shared<NativeThemeAura>(
      /*use_overlay_scrollbars=*/false);
  ASSERT_NE(native_theme_aura_, nullptr);
  native_theme_aura_utils_ =
      std::make_shared<NativeThemeAuraUtils>(native_theme_aura_.get());
  ASSERT_NE(native_theme_aura_utils_, nullptr);
}

void NativeThemeAuraUtilsTest::TearDown() {
  native_theme_aura_ = nullptr;
  native_theme_aura_utils_ = nullptr;
  base::ohos::SetPixelRatio(original_pixel_ratio_);
  scrollbar_hot_size_ = original_scrollbar_hot_size_;
  testing::Mock::VerifyAndClearExpectations(
      &base::ohos::SysInfoUtilsMock::GetInstance());
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
}

TEST_F(NativeThemeAuraUtilsTest, PaintOverlayScrollbarThumb_001) {
  cc::RecordPaintCanvas canvas;
  const gfx::Rect rect;
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  SkColor thumb_color;
  cc::PaintFlags fill_flags;

  native_theme_aura_utils_->PaintOverlayScrollbarThumb(
      &canvas, rect, scrollbar_color, part, thumb_color, fill_flags);

  part = NativeTheme::kScrollbarVerticalThumb;
  native_theme_aura_utils_->PaintOverlayScrollbarThumb(
      &canvas, rect, scrollbar_color, part, thumb_color, fill_flags);
}

TEST_F(NativeThemeAuraUtilsTest, PaintScrollbarThumbWithColor_001) {
  cc::RecordPaintCanvas canvas;
  const gfx::Rect rect;
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarVerticalThumb;
  NativeTheme::State state = NativeTheme::kNormal;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  NativeTheme::PreferredColorScheme color_scheme =
      NativeTheme::PreferredColorScheme::kNoPreference;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);

  color_scheme = NativeTheme::PreferredColorScheme::kDark;
  extra_params.thumb_color = SK_ColorLTGRAY;
  part = NativeTheme::kScrollbarHorizontalThumb;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);

  state = NativeTheme::kHovered;
  part = NativeTheme::kScrollbarVerticalThumb;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);

  part = NativeTheme::kScrollbarHorizontalThumb;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);

  state = NativeTheme::kPressed;
  part = NativeTheme::kScrollbarVerticalThumb;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);

  part = NativeTheme::kScrollbarHorizontalThumb;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest, GetPartSize_001) {
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  NativeTheme::State state = NativeTheme::kNormal;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  native_theme_aura_utils_->native_theme_aura_->set_use_overlay_scrollbar(true);
  const int button_width =
      native_theme_aura_->GetVerticalScrollbarButtonSize().width();
  EXPECT_EQ(native_theme_aura_utils_->GetPartSize(part, state, extra_params),
            gfx::Size(52, button_width));

  part = NativeTheme::kScrollbarVerticalThumb;
  EXPECT_EQ(native_theme_aura_utils_->GetPartSize(part, state, extra_params),
            gfx::Size(button_width, 52));

  part = NativeTheme::kScrollbarHorizontalTrack;
  native_theme_aura_utils_->GetPartSize(part, state, extra_params);

  native_theme_aura_utils_->native_theme_aura_->set_use_overlay_scrollbar(
      false);
  part = NativeTheme::kScrollbarDownArrow;
  native_theme_aura_utils_->GetPartSize(part, state, extra_params);

  part = NativeTheme::kScrollbarUpArrow;
  native_theme_aura_utils_->GetPartSize(part, state, extra_params);

  part = NativeTheme::kScrollbarLeftArrow;
  native_theme_aura_utils_->GetPartSize(part, state, extra_params);

  part = NativeTheme::kScrollbarRightArrow;
  native_theme_aura_utils_->GetPartSize(part, state, extra_params);

  part = NativeTheme::kScrollbarHorizontalThumb;
  native_theme_aura_utils_->GetPartSize(part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest, GetNinePatchAperture_001) {
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  const int button_width =
      native_theme_aura_->GetVerticalScrollbarButtonSize().width();
  EXPECT_EQ(native_theme_aura_utils_->GetNinePatchAperture(part),
            gfx::Rect(button_width / 2, 0, 1, button_width));

  part = NativeTheme::kScrollbarVerticalThumb;
  EXPECT_EQ(native_theme_aura_utils_->GetNinePatchAperture(part),
            gfx::Rect(0, button_width / 2, button_width, 1));
}

TEST_F(NativeThemeAuraUtilsTest, PCDeviceTest_001) {
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
  auto& system_properties_mock = base::ohos::SysInfoUtilsMock::GetInstance();
  EXPECT_CALL(system_properties_mock, IsPcDevice())
      .WillOnce(Return(true))
      .WillRepeatedly(Return(true));

  native_theme_aura_utils_->SetScrollbarThumbWidth();

  cc::RecordPaintCanvas canvas;
  const gfx::Rect rect;
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  SkColor thumb_color;
  cc::PaintFlags fill_flags;

  native_theme_aura_utils_->PaintOverlayScrollbarThumb(
      &canvas, rect, scrollbar_color, part, thumb_color, fill_flags);

  part = NativeTheme::kScrollbarVerticalThumb;
  native_theme_aura_utils_->PaintOverlayScrollbarThumb(
      &canvas, rect, scrollbar_color, part, thumb_color, fill_flags);
  EXPECT_TRUE(canvas.HasRecordedDrawOps());
  EXPECT_EQ(thumb_color, SK_ColorTRANSPARENT);
  const cc::PaintRecord record = canvas.CopyAsRecord();
  const auto& first_op =
      static_cast<const cc::DrawRRectOp&>(record.GetFirstOp());
  EXPECT_FLOAT_EQ(first_op.rrect.rect().y(), 0.0f);
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
}

TEST_F(NativeThemeAuraUtilsTest, NonPCDevicePreservesScrollbarHotSize) {
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
  auto& system_properties_mock = base::ohos::SysInfoUtilsMock::GetInstance();
  EXPECT_CALL(system_properties_mock, IsPcDevice()).WillOnce(Return(false));
  const int hot_size_before = scrollbar_hot_size_;

  native_theme_aura_utils_->SetScrollbarThumbWidth();

  EXPECT_EQ(scrollbar_hot_size_, hot_size_before);
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
}

TEST_F(NativeThemeAuraUtilsTest, PaintOverlayScrollbarThumb_NonPCVertical) {
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
  auto& system_properties_mock = base::ohos::SysInfoUtilsMock::GetInstance();
  EXPECT_CALL(system_properties_mock, IsPcDevice())
      .WillRepeatedly(Return(false));

  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 100, 200);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarVerticalThumb;
  SkColor thumb_color;
  cc::PaintFlags fill_flags;

  native_theme_aura_utils_->PaintOverlayScrollbarThumb(
      &canvas, rect, scrollbar_color, part, thumb_color, fill_flags);
  EXPECT_EQ(thumb_color, SK_ColorTRANSPARENT);
}

TEST_F(NativeThemeAuraUtilsTest, PaintOverlayScrollbarThumb_NonPCHorizontal) {
  base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
  auto& system_properties_mock = base::ohos::SysInfoUtilsMock::GetInstance();
  EXPECT_CALL(system_properties_mock, IsPcDevice())
      .WillRepeatedly(Return(false));

  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 200, 100);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  SkColor thumb_color;
  cc::PaintFlags fill_flags;

  native_theme_aura_utils_->PaintOverlayScrollbarThumb(
      &canvas, rect, scrollbar_color, part, thumb_color, fill_flags);
  EXPECT_EQ(thumb_color, SK_ColorTRANSPARENT);
}

TEST_F(NativeThemeAuraUtilsTest, PaintScrollbarThumbWithColor_NormalStateVertical) {
  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 100, 200);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarVerticalThumb;
  NativeTheme::State state = NativeTheme::kNormal;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  NativeTheme::PreferredColorScheme color_scheme = NativeTheme::PreferredColorScheme::kNoPreference;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest, PaintScrollbarThumbWithColor_NormalStateHorizontal) {
  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 200, 100);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  NativeTheme::State state = NativeTheme::kNormal;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  NativeTheme::PreferredColorScheme color_scheme = NativeTheme::PreferredColorScheme::kNoPreference;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest, PaintScrollbarThumbWithColor_HoveredStateVertical) {
  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 100, 200);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarVerticalThumb;
  NativeTheme::State state = NativeTheme::kHovered;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  NativeTheme::PreferredColorScheme color_scheme = NativeTheme::PreferredColorScheme::kNoPreference;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest, PaintScrollbarThumbWithColor_HoveredStateHorizontal) {
  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 200, 100);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  NativeTheme::State state = NativeTheme::kHovered;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  NativeTheme::PreferredColorScheme color_scheme = NativeTheme::PreferredColorScheme::kNoPreference;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest, PaintScrollbarThumbWithColor_PressedStateVertical) {
  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 100, 200);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarVerticalThumb;
  NativeTheme::State state = NativeTheme::kPressed;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  NativeTheme::PreferredColorScheme color_scheme = NativeTheme::PreferredColorScheme::kNoPreference;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest, PaintScrollbarThumbWithColor_PressedStateHorizontal) {
  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 200, 100);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  NativeTheme::State state = NativeTheme::kPressed;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  NativeTheme::PreferredColorScheme color_scheme = NativeTheme::PreferredColorScheme::kNoPreference;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest, PaintScrollbarThumbWithColor_DarkSchemeVertical) {
  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 100, 200);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarVerticalThumb;
  NativeTheme::State state = NativeTheme::kNormal;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  NativeTheme::PreferredColorScheme color_scheme = NativeTheme::PreferredColorScheme::kDark;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest,
       PaintScrollbarThumbWithColor_DarkSchemeHorizontal) {
  cc::RecordPaintCanvas canvas;
  gfx::Rect rect(0, 0, 200, 100);
  SkColor scrollbar_color = SK_ColorLTGRAY;
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  NativeTheme::State state = NativeTheme::kNormal;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  NativeTheme::PreferredColorScheme color_scheme =
      NativeTheme::PreferredColorScheme::kDark;
  native_theme_aura_utils_->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

TEST_F(NativeThemeAuraUtilsTest, GetPartSize_OverlayModeOtherPart) {
  NativeTheme::Part part = NativeTheme::kScrollbarDownArrow;
  NativeTheme::State state = NativeTheme::kNormal;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  native_theme_aura_utils_->native_theme_aura_->set_use_overlay_scrollbar(true);
  auto size = native_theme_aura_utils_->GetPartSize(part, state, extra_params);
  EXPECT_EQ(size.width(), 0);
  EXPECT_EQ(size.height(), 0);
}

TEST_F(NativeThemeAuraUtilsTest, GetPartSize_NonOverlayModeOtherPart) {
  NativeTheme::Part part = NativeTheme::kScrollbarVerticalThumb;
  NativeTheme::State state = NativeTheme::kNormal;
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  native_theme_aura_utils_->native_theme_aura_->set_use_overlay_scrollbar(
      false);
  auto size = native_theme_aura_utils_->GetPartSize(part, state, extra_params);
  EXPECT_EQ(size.width(), 0);
  EXPECT_EQ(size.height(), 0);
}

TEST_F(NativeThemeAuraUtilsTest, GetNinePatchCanvasSize_Test) {
  NativeTheme::Part part = NativeTheme::kScrollbarHorizontalThumb;
  auto size = native_theme_aura_utils_->GetNinePatchCanvasSize(part);
  const int button_width =
      native_theme_aura_->GetVerticalScrollbarButtonSize().width();
  EXPECT_EQ(size, gfx::Size(button_width, button_width));
}
}  // namespace ui
