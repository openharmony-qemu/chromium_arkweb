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

namespace blink {

#if BUILDFLAG(ARKWEB_NIGHT_MODE)
SkColor DarkModeFilter::InvertColorForNightModeIfNeeded(SkColor color,
                                                        bool is_foreground) {
  if (!immutable_.color_filter || !immutable_.foreground_classifier || !inverted_color_cache_) {
    return color;
  }

  if ((is_foreground && (immutable_.foreground_classifier->ShouldInvertColor(
                             color) != DarkModeResult::kApplyFilter)) ||
      (!is_foreground && (immutable_.background_classifier->ShouldInvertColor(
                              color) != DarkModeResult::kApplyFilter))) {
    return color;
  }

  return inverted_color_cache_
      ->GetInvertedColor(immutable_.color_filter.get(),
                         SkColor4f::FromColor(color))
      .toSkColor();
}
#endif
}  // namespace blink
