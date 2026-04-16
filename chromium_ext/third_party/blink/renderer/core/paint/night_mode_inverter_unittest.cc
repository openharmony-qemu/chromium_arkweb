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
#endif
}