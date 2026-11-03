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
#include "ui/gfx/color_utils.h"

namespace blink {

#if BUILDFLAG(ARKWEB_NIGHT_MODE)
Color Color::AdjustBrightness(float ratio, int alpha) const {
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(toSkColor4f().toSkColor(), &hsl);
  return FromHSLA(hsl.h * 6.0f , hsl.s, hsl.l * ratio, alpha);
}

Color Color::Dark(float ratio) const {
  Color srgb_color = *this;
  srgb_color.ConvertToColorSpace(ColorSpace::kSRGB);

  const float v =
      std::max({srgb_color.Param0(), srgb_color.Param1(), srgb_color.Param2()});
  const float multiplier = (v == 0.0f) ? 0.0f : std::max(0.0f, (v - 0.33f) / v);
  srgb_color.param0_ = QuantizeTo8Bit(srgb_color.param0_ * multiplier * ratio);
  srgb_color.param1_ = QuantizeTo8Bit(srgb_color.param1_ * multiplier * ratio);
  srgb_color.param2_ = QuantizeTo8Bit(srgb_color.param2_ * multiplier * ratio);
  return srgb_color;
}
#endif
}  // namespace blink
