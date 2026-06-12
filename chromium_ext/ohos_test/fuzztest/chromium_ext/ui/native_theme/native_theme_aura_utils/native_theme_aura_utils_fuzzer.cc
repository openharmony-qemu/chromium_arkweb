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

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>

#include "base/command_line.h"
#include "cc/paint/record_paint_canvas.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_ARKWEB)
#define private public
#include "arkweb/chromium_ext/ui/native_theme/native_theme_aura_utils.h"
#undef private
#endif

namespace ui {

static NativeTheme* g_native_theme;
static NativeThemeAuraUtils* g_native_theme_aura_utils;

void FuzzGetPartSize(FuzzedDataProvider* fdp) {
  if (!g_native_theme_aura_utils) {
    return;
  }
  int part_value = fdp->ConsumeIntegralInRange<int>(0, 15);
  int state_value = fdp->ConsumeIntegralInRange<int>(0, 6);
  NativeTheme::Part part = static_cast<NativeTheme::Part>(part_value);
  NativeTheme::State state = static_cast<NativeTheme::State>(state_value);
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  g_native_theme_aura_utils->GetPartSize(part, state, extra_params);
}

void FuzzGetNinePatchCanvasSize(FuzzedDataProvider* fdp) {
  if (!g_native_theme_aura_utils) {
    return;
  }
  int part_value = fdp->ConsumeIntegralInRange<int>(0, 15);
  NativeTheme::Part part = static_cast<NativeTheme::Part>(part_value);
  g_native_theme_aura_utils->GetNinePatchCanvasSize(part);
}

void FuzzGetNinePatchAperture(FuzzedDataProvider* fdp) {
  if (!g_native_theme_aura_utils) {
    return;
  }
  int part_value = fdp->ConsumeIntegralInRange<int>(0, 15);
  NativeTheme::Part part = static_cast<NativeTheme::Part>(part_value);
  g_native_theme_aura_utils->GetNinePatchAperture(part);
}

void FuzzPaintOverlayScrollbarThumb(FuzzedDataProvider* fdp) {
  if (!g_native_theme_aura_utils) {
    return;
  }
  cc::RecordPaintCanvas canvas;
  int x = fdp->ConsumeIntegral<int>();
  int y = fdp->ConsumeIntegral<int>();
  int width = fdp->ConsumeIntegralInRange<int>(0, 500);
  int height = fdp->ConsumeIntegralInRange<int>(0, 500);
  gfx::Rect rect(x, y, width, height);
  SkColor scrollbar_color = fdp->ConsumeIntegral<SkColor>();
  int part_value = fdp->ConsumeIntegralInRange<int>(0, 15);
  NativeTheme::Part part = static_cast<NativeTheme::Part>(part_value);
  SkColor thumb_color;
  cc::PaintFlags fill_flags;
  g_native_theme_aura_utils->PaintOverlayScrollbarThumb(
      &canvas, rect, scrollbar_color, part, thumb_color, fill_flags);
}

void FuzzPaintScrollbarThumbWithColor(FuzzedDataProvider* fdp) {
  if (!g_native_theme_aura_utils) {
    return;
  }
  cc::RecordPaintCanvas canvas;
  int x = fdp->ConsumeIntegral<int>();
  int y = fdp->ConsumeIntegral<int>();
  int width = fdp->ConsumeIntegralInRange<int>(0, 500);
  int height = fdp->ConsumeIntegralInRange<int>(0, 500);
  gfx::Rect rect(x, y, width, height);
  SkColor scrollbar_color = fdp->ConsumeIntegral<SkColor>();
  int color_scheme_value = fdp->ConsumeIntegralInRange<int>(0, 2);
  NativeTheme::PreferredColorScheme color_scheme =
      static_cast<NativeTheme::PreferredColorScheme>(color_scheme_value);
  int part_value = fdp->ConsumeIntegralInRange<int>(0, 15);
  NativeTheme::Part part = static_cast<NativeTheme::Part>(part_value);
  int state_value = fdp->ConsumeIntegralInRange<int>(0, 6);
  NativeTheme::State state = static_cast<NativeTheme::State>(state_value);
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  if (fdp->ConsumeBool()) {
    extra_params.thumb_color = fdp->ConsumeIntegral<SkColor>();
  }
  g_native_theme_aura_utils->PaintScrollbarThumbWithColor(
      &canvas, rect, scrollbar_color, color_scheme, part, state, extra_params);
}

void FuzzSetScrollbarThumbWidth(FuzzedDataProvider* fdp) {
  if (!g_native_theme_aura_utils) {
    return;
  }
  g_native_theme_aura_utils->SetScrollbarThumbWidth();
}

void FuzzUseOverlayScrollbar(FuzzedDataProvider* fdp) {
  if (!g_native_theme || !g_native_theme_aura_utils) {
    return;
  }
  bool use_overlay = fdp->ConsumeBool();
  g_native_theme->set_use_overlay_scrollbar(use_overlay);
  int part_value = fdp->ConsumeIntegralInRange<int>(0, 15);
  int state_value = fdp->ConsumeIntegralInRange<int>(0, 6);
  NativeTheme::Part part = static_cast<NativeTheme::Part>(part_value);
  NativeTheme::State state = static_cast<NativeTheme::State>(state_value);
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  g_native_theme_aura_utils->GetPartSize(part, state, extra_params);
}

}  // namespace ui

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static bool initialized = false;
  if (!initialized) {
    base::CommandLine::Init(0, nullptr);
    initialized = true;
  }

  ui::g_native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  ui::g_native_theme_aura_utils =
      new ui::NativeThemeAuraUtils(static_cast<ui::NativeThemeAura*>(ui::g_native_theme));

  FuzzedDataProvider fdp(data, size);

  int selector = fdp.ConsumeIntegralInRange<int>(0, 6);
  switch (selector) {
    case 0:
      ui::FuzzGetPartSize(&fdp);
      break;
    case 1:
      ui::FuzzGetNinePatchCanvasSize(&fdp);
      break;
    case 2:
      ui::FuzzGetNinePatchAperture(&fdp);
      break;
    case 3:
      ui::FuzzPaintOverlayScrollbarThumb(&fdp);
      break;
    case 4:
      ui::FuzzPaintScrollbarThumbWithColor(&fdp);
      break;
    case 5:
      ui::FuzzSetScrollbarThumbWidth(&fdp);
      break;
    case 6:
      ui::FuzzUseOverlayScrollbar(&fdp);
      break;
  }

  delete ui::g_native_theme_aura_utils;
  return 0;
}