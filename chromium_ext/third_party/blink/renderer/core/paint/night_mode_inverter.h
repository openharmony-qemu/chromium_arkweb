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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NIGHT_MODE_INVERTER_H
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NIGHT_MODE_INVERTER_H

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

class DarkModeFilter;
class ComputedStyle;

class PLATFORM_EXPORT NightModeInverter {
 public:
  explicit NightModeInverter();
  ~NightModeInverter() = default;

  static NightModeInverter& GetInverter();

  enum class ElementRole {
    kLinkHighlight,
    kBackground,
    kBorder,
    kForeground,
    kGradient
  };

  Color InvertElementColor(const ComputedStyle& style,
                           const Color&,
                           ElementRole);
  Color InvertElementColor(const Color&, ElementRole);
  void BlendColorFilterForImage(cc::PaintFlags&, float divisor);
  void BlendColorFilterForImage(cc::PaintFlags&);

  void SetBackgroundColor(const Color& color) { background_color_ = color; }
  void SetBorderColor(const Color& color) { border_color_ = color; }
  void SetImageBrightnessScale(float scale) { image_brightness_scale_ = scale; }
  void SetGradientExtent(float extent) { gradient_extent_ = extent; }
  void SetTurnImageGray(bool turn_image_gray) {
    turn_image_gray_ = turn_image_gray;
  }
  void SetTurnAllGray(bool turn_all_gray) { turn_all_gray_ = turn_all_gray; }
  void SetSkipProcessImages(bool skip_process_images) {
    skip_process_images_ = skip_process_images;
  }

  float ImageBrightnessScale() { return image_brightness_scale_; }
  float GradientExtent() { return gradient_extent_; }

  bool ShouldUseGrayImage() { return HasTurnImageGray() || HasTurnAllGray(); }
  bool HasTurnAllGray() { return turn_all_gray_; }
  bool ShouldSkipProcessImages() { return skip_process_images_; }
  bool HasTurnImageGray() { return turn_image_gray_; }

 private:
  Color LinkHighlight(const Color&);
  Color BorderColor(const Color&);
  bool ShouldBlendForImage();
  Color ComputeRoleColor(Color color, ElementRole role);
  bool NightModeForceOverride() { return HasImageBrightnessScale(); }
  bool HasBorderColor() { return border_color_.Alpha() > 0; }
  bool HasImageBrightnessScale() { return image_brightness_scale_ > 0.0f; }

  Color background_color_;
  Color border_color_;
  Color visited_link_text_color_;
  float image_brightness_scale_;
  float gradient_extent_;
  bool turn_image_gray_ = false;
  bool turn_all_gray_ = false;
  bool skip_process_images_ = false;
  std::unique_ptr<DarkModeFilter> dark_mode_filter_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NIGHT_MODE_INVERTER_H