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


#include "testing/gtest/include/gtest/gtest.h"
#include "night_mode_inverter.cc"

namespace blink {

class NightModeInverterTest : public testing::Test {
 public:
  void SetUp() override {
  }

  void TearDown() override {
  }
};

#if BUILDFLAG(ARKWEB_NIGHT_MODE)
TEST_F(NightModeInverterTest, GetInverterReturnsSingleton) {
  auto& inverter1 = NightModeInverter::GetInverter();
  auto& inverter2 = NightModeInverter::GetInverter();
  EXPECT_EQ(&inverter1, &inverter2);
}

TEST_F(NightModeInverterTest, InvertElementColorWithStyleNightModeDisabled) {
  const ComputedStyle& initial = *ComputedStyle::GetInitialStyleSingleton();
  ComputedStyleBuilder builder(initial);
  builder.SetNightModeEnabled(false);
  auto* style = builder.TakeStyle();
  NightModeInverter& inverter = NightModeInverter::GetInverter();

  Color input(255, 255, 255);
  Color result =
      inverter.InvertElementColor(*style, input, NightModeInverter::ElementRole::kForeground);

  EXPECT_EQ(result, input);
}

TEST_F(NightModeInverterTest, InvertElementColorForegroundBranch) {
  const ComputedStyle& initial = *ComputedStyle::GetInitialStyleSingleton();
  ComputedStyleBuilder builder(initial);
  builder.SetNightModeEnabled(true);
  auto* style = builder.TakeStyle();
  NightModeInverter& inverter = NightModeInverter::GetInverter();

  Color bright_text(255, 255, 255);
  Color result =
      inverter.InvertElementColor(*style, bright_text, NightModeInverter::ElementRole::kForeground);

  EXPECT_NE(result, bright_text);
}

TEST_F(NightModeInverterTest, InvertElementColorBackgroundBranch) {
  const ComputedStyle& initial = *ComputedStyle::GetInitialStyleSingleton();
  ComputedStyleBuilder builder(initial);
  builder.SetNightModeEnabled(true);
  auto* style = builder.TakeStyle();
  NightModeInverter& inverter = NightModeInverter::GetInverter();

  Color bg(255, 255, 255);
  Color result =
      inverter.InvertElementColor(*style, bg, NightModeInverter::ElementRole::kBackground);

  EXPECT_NE(result, bg);
}

TEST_F(NightModeInverterTest, InvertElementColorBorderBranch) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();

  Color gray_border(200, 200, 200);
  Color result =
      inverter.InvertElementColor(gray_border, NightModeInverter::ElementRole::kBorder);

  EXPECT_EQ(result, gray_border);
}

TEST_F(NightModeInverterTest, InvertElementColorLinkHighlightBranch) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetBorderColor(Color(10, 10, 10));
  Color link(100, 100, 255);
  Color result =
      inverter.InvertElementColor(link, NightModeInverter::ElementRole::kLinkHighlight);

  EXPECT_EQ(link.Blue(), result.Blue());
}

TEST_F(NightModeInverterTest, ComputeRoleColorForegroundBrightnessBelowThreshold) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetBorderColor(Color(10, 10, 10));

  Color dark_fg(50, 50, 50);
  Color result =
      inverter.InvertElementColor(dark_fg, NightModeInverter::ElementRole::kForeground);

  EXPECT_GE(color_utils::GetLuma(result.Rgb()), color_utils::GetLuma(dark_fg.Rgb()));
}

TEST_F(NightModeInverterTest, ComputeRoleColorBackgroundBrightnessAboveThreshold) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();

  Color bright_bg(255, 255, 255);
  Color result =
      inverter.InvertElementColor(bright_bg, NightModeInverter::ElementRole::kBackground);

  EXPECT_NE(result, bright_bg);
}

TEST_F(NightModeInverterTest, DecreaseBrightnessColorHighContrastMaxBranch) {
  Color white(255, 255, 255);
  Color result = DecreaseBrightnessColor(white.Rgb(), true);

  EXPECT_LT(color_utils::GetLuma(result.Rgb()), color_utils::GetLuma(white.Rgb()));
}

TEST_F(NightModeInverterTest, DecreaseBrightnessColorLowContrastFallback) {
  Color low_contrast(30, 30, 30);
  Color result = DecreaseBrightnessColor(low_contrast.Rgb(), true);

  EXPECT_EQ(result, low_contrast);
}

TEST_F(NightModeInverterTest, DecreaseBrightnessColorLowChromaBranch) {
  Color low_chroma(120, 120, 120);
  Color result = DecreaseBrightnessColor(low_chroma.Rgb(), true);

  EXPECT_EQ(result, low_chroma);
}

TEST_F(NightModeInverterTest, BorderColorNonGrayReturnsOriginal) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();

  Color colored(255, 200, 200);
  Color result =
      inverter.InvertElementColor(colored, NightModeInverter::ElementRole::kBorder);

  EXPECT_EQ(result, colored);
}

TEST_F(NightModeInverterTest, LinkHighlightForceOverrideDisabled) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();

  Color link(100, 100, 255);
  Color result =
      inverter.InvertElementColor(link, NightModeInverter::ElementRole::kLinkHighlight);

  EXPECT_EQ(result, link);
}

TEST_F(NightModeInverterTest, BlendColorFilterForImageNormalCase) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  cc::PaintFlags flags;

  inverter.BlendColorFilterForImage(flags);

  EXPECT_FALSE(flags.getColorFilter());
}

TEST_F(NightModeInverterTest, BlendColorFilterForImageGrayCase) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  cc::PaintFlags flags;

  inverter.BlendColorFilterForImage(flags, 0.0f);

  EXPECT_FALSE(flags.getColorFilter());
}

TEST_F(NightModeInverterTest, IsApproximateGrayBorderTrueBoundary) {
  Color gray(128, 128, 128);
  EXPECT_TRUE(IsApproximateGrayBorder(gray));
}

TEST_F(NightModeInverterTest, IsApproximateGrayBorderFalseBoundary) {
  Color non_gray(255, 200, 200);
  EXPECT_FALSE(IsApproximateGrayBorder(non_gray));
}

TEST_F(NightModeInverterTest, IsLowChromaTrueBoundary) {
  Color low_chroma(120, 120, 120);
  EXPECT_TRUE(IsLowChroma(low_chroma.Rgb()));
}

TEST_F(NightModeInverterTest, IsLowChromaFalseBoundary) {
  Color high_chroma(255, 100, 100);
  EXPECT_FALSE(IsLowChroma(high_chroma.Rgb()));
}

// ---- Additional branch-coverage tests ----
// Each configures the singleton via its public setters; toggles set in one test
// are reset at the end of that test to avoid affecting later ones.

// InvertElementColor (line 119): kGradient case -> ComputeRoleColor. Also
// covers ComputeRoleColor's `brightness <= threshold` true sub-condition (line
// 193, is_foreground=false) and DecreaseBrightnessColor's background/else arm.
TEST_F(NightModeInverterTest, InvertElementColorGradientRole) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  Color c(200, 200, 200);
  Color result = inverter.InvertElementColor(
      c, NightModeInverter::ElementRole::kGradient);
  EXPECT_NE(result, Color());
}

// BlendColorFilterForImage (line 130): ShouldUseGrayImage() true arm.
// SetColorFilterWithZeroSaturation() only modifies an existing color filter;
// on a fresh PaintFlags (no filter set), it is a no-op, so getColorFilter()
// remains false.
TEST_F(NightModeInverterTest, BlendColorFilterForImageGrayImage) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetTurnImageGray(true);
  cc::PaintFlags flags;
  inverter.BlendColorFilterForImage(flags);
  inverter.SetTurnImageGray(false);
  EXPECT_FALSE(flags.getColorFilter());
}

// BlendColorFilterForImage (line 134 false + 141-144): blend path taken when
// image_brightness_scale_ > 0 and images are not skipped, divisor > epsilon.
TEST_F(NightModeInverterTest, BlendColorFilterForImageBlendPath) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetSkipProcessImages(false);
  inverter.SetImageBrightnessScale(1.0f);
  cc::PaintFlags flags;
  inverter.BlendColorFilterForImage(flags, 1.0f);
  inverter.SetImageBrightnessScale(0.0f);
  EXPECT_TRUE(flags.getColorFilter());
}

// BlendColorFilterForImage (line 137 true): divisor < kEpsilon -> early return.
TEST_F(NightModeInverterTest, BlendColorFilterForImageDivisorZero) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetSkipProcessImages(false);
  inverter.SetImageBrightnessScale(1.0f);
  cc::PaintFlags flags;
  inverter.BlendColorFilterForImage(flags, 0.0f);
  inverter.SetImageBrightnessScale(0.0f);
  EXPECT_FALSE(flags.getColorFilter());
}

// LinkHighlight (line 156 false): NightModeForceOverride() true -> color.Dark().
TEST_F(NightModeInverterTest, LinkHighlightForceOverrideEnabled) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetImageBrightnessScale(1.0f);
  Color link(200, 200, 200);
  Color result = inverter.InvertElementColor(
      link, NightModeInverter::ElementRole::kLinkHighlight);
  inverter.SetImageBrightnessScale(0.0f);
  EXPECT_NE(result, link);
}

// BorderColor (line 163 second arm): color == border_color_ -> return color.
TEST_F(NightModeInverterTest, BorderColorEqualToSetBorder) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  Color border(60, 70, 80);
  inverter.SetBorderColor(border);
  Color result = inverter.InvertElementColor(
      border, NightModeInverter::ElementRole::kBorder);
  EXPECT_EQ(result, border);
}

// BorderColor (line 167 true, 168 true): no force override + gray border ->
// AdjustBrightness.
TEST_F(NightModeInverterTest, BorderColorGrayNoOverride) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetImageBrightnessScale(0.0f);
  inverter.SetBorderColor(Color(40, 40, 40));
  Color gray(180, 180, 180);
  Color result = inverter.InvertElementColor(
      gray, NightModeInverter::ElementRole::kBorder);
  EXPECT_NE(result, gray);
}

// BorderColor (line 167 true, 168 false): no force override + non-gray border
// -> return color unchanged.
TEST_F(NightModeInverterTest, BorderColorNonGrayNoOverride) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetImageBrightnessScale(0.0f);
  inverter.SetBorderColor(Color(40, 40, 40));
  Color colored(255, 50, 50);
  Color result = inverter.InvertElementColor(
      colored, NightModeInverter::ElementRole::kBorder);
  EXPECT_EQ(result, colored);
}

// BorderColor (line 167 false, 182): force override + opaque color ->
// border_color_. The result carries the border_color_'s RGB channels.
TEST_F(NightModeInverterTest, BorderColorOverrideOpaque) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetImageBrightnessScale(1.0f);
  Color border(40, 40, 40);
  inverter.SetBorderColor(border);
  Color input(180, 180, 180);
  Color result = inverter.InvertElementColor(
      input, NightModeInverter::ElementRole::kBorder);
  inverter.SetImageBrightnessScale(0.0f);
  EXPECT_EQ(result.Red(), border.Red());
  EXPECT_EQ(result.Green(), border.Green());
  EXPECT_EQ(result.Blue(), border.Blue());
}

// BorderColor (line 177 true): force override + non-opaque color ->
// Color(border rgb, input alpha). Verify RGB channels come from border_color_.
TEST_F(NightModeInverterTest, BorderColorOverrideNonOpaque) {
  NightModeInverter& inverter = NightModeInverter::GetInverter();
  inverter.SetImageBrightnessScale(1.0f);
  Color border(40, 40, 40);
  inverter.SetBorderColor(border);
  Color input(180, 180, 180, 0x80);
  Color result = inverter.InvertElementColor(
      input, NightModeInverter::ElementRole::kBorder);
  inverter.SetImageBrightnessScale(0.0f);
  EXPECT_EQ(result.Red(), border.Red());
  EXPECT_EQ(result.Green(), border.Green());
  EXPECT_EQ(result.Blue(), border.Blue());
}
#endif
}