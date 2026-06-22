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
#include "night_mode_inverter.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_lab_color_space.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "ui/gfx/color_utils.h"

namespace blink {

namespace {

const constexpr float kContrastRatio = 0.314f;
const constexpr float kContrastRatioBase = 1.0f - kContrastRatio;
const constexpr float kTextBrightnessRatio = 0.76f;
const constexpr int kDefaultTextBrightnessThreshold = 150;
const constexpr int kDefaultBackgroundBrightnessThreshold = 205;
const constexpr float kContrastRatioThresholdMin = 2.2f;
const constexpr float kContrastRatioThresholdMax = 15.7f;
const constexpr float kContrastRatioThresholdLowChroma = 3.f;
const constexpr int kBackgroundColor = 0xff121212;
const constexpr int kChromaColorThreshold = 1600;
const constexpr float kEpsilon = 0.00001f;

bool IsApproximateGrayBorder(const Color& color) {
  int minComponent =
      std::min(std::min(color.Red(), color.Green()), color.Blue());
  int maxComponent =
      std::max(std::max(color.Red(), color.Green()), color.Blue());

  float deviation = minComponent / 10.f;
 
  return maxComponent - minComponent < deviation;
}

bool IsLowChroma(SkColor color) {
  lab::DarkModeSRGBLABTransformer transformer;
  SkV3 rgb = {SkColorGetR(color) / 255.0f, SkColorGetG(color) / 255.0f,
              SkColorGetB(color) / 255.0f};
  SkV3 lab = transformer.SRGBToLAB(rgb);
  return (lab.y * lab.y + lab.z * lab.z) < kChromaColorThreshold;
}

Color DecreaseBrightnessColor(SkColor color, bool is_foreground) {
  float black_contrast = color_utils::GetContrastRatio(kBackgroundColor, color);
  Color dst_color = Color::FromSkColor(color);
  if (is_foreground) {
    if (black_contrast > kContrastRatioThresholdMax) {
      dst_color = dst_color.Dark(kTextBrightnessRatio);
    } else if (black_contrast > kContrastRatioThresholdMin && black_contrast <= kContrastRatioThresholdMax) {
      dst_color = dst_color.Dark();
    }

    float black_contrast_after =
        color_utils::GetContrastRatio(kBackgroundColor, dst_color.Rgb());
    if (IsLowChroma(color)) {
      if (black_contrast_after < kContrastRatioThresholdLowChroma) {
        dst_color = Color::FromSkColor(color);
      }
    } else {
      if (black_contrast_after < kContrastRatioThresholdMin) {
        dst_color = Color::FromSkColor(color);
      }
    }
  } else {
    if (black_contrast > kContrastRatioThresholdMin) {
      dst_color = dst_color.Dark();
    }
  }
  return dst_color;
}

}  // namespace

NightModeInverter& NightModeInverter::GetInverter() {
  static base::NoDestructor<NightModeInverter> instance;
  return *instance;
}

NightModeInverter::NightModeInverter() {
  DarkModeSettings settings;
  settings.foreground_brightness_threshold = kDefaultTextBrightnessThreshold;
  settings.background_brightness_threshold =
      kDefaultBackgroundBrightnessThreshold;
  dark_mode_filter_ = std::make_unique<DarkModeFilter>(settings);
}

Color NightModeInverter::InvertElementColor(const ComputedStyle& style,
                                            const Color& color,
                                            ElementRole role) {
  if (!style.NightModeEnabled()) {
    return color;
  }
  return InvertElementColor(color, role);
}

Color NightModeInverter::InvertElementColor(const Color& color,
                                            ElementRole role) {
  switch (role) {
    case ElementRole::kLinkHighlight:
      return LinkHighlight(color);
    case ElementRole::kForeground:
    case ElementRole::kBackground:
    case ElementRole::kGradient:
      return ComputeRoleColor(color, role);
    case ElementRole::kBorder:
      return BorderColor(color);
    default:
      return color;
  }
}

void NightModeInverter::BlendColorFilterForImage(cc::PaintFlags& image_flags,
                                                 float divisor) {
  if (ShouldUseGrayImage()) {
    image_flags.SetColorFilterWithZeroSaturation();
    return;
  }
  if (!ShouldBlendForImage()) {
    return;
  }
  if (divisor < kEpsilon) {
    return;
  }

  float brightnessScale = image_brightness_scale_ / divisor;

  image_flags.setColorFilter(
      cc::ColorFilter::MakeBlend(SkColor4f(0.f, 0.f, 0.f, 1.f - brightnessScale), SkBlendMode::kSrcATop));
}

void NightModeInverter::BlendColorFilterForImage(cc::PaintFlags& image_flags) {
  BlendColorFilterForImage(image_flags, 1.f);
}

bool NightModeInverter::ShouldBlendForImage() {
  return !ShouldSkipProcessImages() && HasImageBrightnessScale();
}

Color NightModeInverter::LinkHighlight(const Color& color) {
  if (!NightModeForceOverride()) {
    return color;
  }
  return color.Dark();
}

Color NightModeInverter::BorderColor(const Color& color) {
  if (!HasBorderColor() || color == border_color_) {
    return color;
  }

  if (!NightModeForceOverride()) {
    if (IsApproximateGrayBorder(color)) {
      float avgComponent = (color.Red() + color.Green() + color.Blue()) / 3.f;
      float ratio =
          kContrastRatioBase + ((avgComponent) / 255.0f) * kContrastRatio;
      return border_color_.AdjustBrightness(ratio, color.Alpha());
    }
    return color;
  }

  if (0xff != color.Alpha()) {
    return Color(border_color_.Red(), border_color_.Green(),
                 border_color_.Blue(), color.Alpha());
  }

  return border_color_;
}

Color NightModeInverter::ComputeRoleColor(Color color, ElementRole role) {
  bool is_foreground = role == ElementRole::kForeground;
  if (!dark_mode_filter_) {
    return Color();
  }
  SkColor dst_color = dark_mode_filter_->InvertColorForNightModeIfNeeded(
      color.Rgb(), is_foreground);
  uint8_t brightness = color_utils::GetLuma(color.Rgb());
  if (is_foreground || (brightness <= kDefaultBackgroundBrightnessThreshold)) {
    return DecreaseBrightnessColor(dst_color, is_foreground);
  }
  return Color::FromSkColor(dst_color);
}
}  // namespace blink