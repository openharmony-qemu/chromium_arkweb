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

#include "arkweb/chromium_ext/third_party/blink/renderer/platform/graphics/image_ext.h"
#include "base/command_line.h"
#include "cc/paint/paint_image_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_types.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation_enum.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace blink {

static PaintImage CreateTestPaintImage(int width, int height) {
  SkBitmap bitmap;
  if (width <= 0 || height <= 0) {
    return PaintImage();
  }
  if (width > 10000 || height > 10000) {
    width = 100;
    height = 100;
  }
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorWHITE);
  return cc::PaintImage::CreateFromBitmap(bitmap);
}

void FuzzClipResizeAndOrientImage(FuzzedDataProvider* fdp) {
  int width = fdp->ConsumeIntegralInRange<int>(1, 1000);
  int height = fdp->ConsumeIntegralInRange<int>(1, 1000);
  PaintImage image = CreateTestPaintImage(width, height);
  if (image.IsLazyGenerated()) {
    return;
  }

  int orientation_value = fdp->ConsumeIntegralInRange<int>(1, 8);
  ImageOrientationEnum orientation =
      static_cast<ImageOrientationEnum>(orientation_value);

  int clip_x = fdp->ConsumeIntegral<int>();
  int clip_y = fdp->ConsumeIntegral<int>();
  int clip_w = fdp->ConsumeIntegralInRange<int>(1, 1000);
  int clip_h = fdp->ConsumeIntegralInRange<int>(1, 1000);
  gfx::Rect clip_rect(clip_x, clip_y, clip_w, clip_h);

  float scale_x = fdp->ConsumeFloatingPointInRange<float>(0.01f, 10.0f);
  float scale_y = fdp->ConsumeFloatingPointInRange<float>(0.01f, 10.0f);
  gfx::Vector2dF image_scale(scale_x, scale_y);

  float opacity = fdp->ConsumeFloatingPointInRange<float>(0.0f, 1.0f);

  int iq_value = fdp->ConsumeIntegralInRange<int>(0, 3);
  InterpolationQuality interpolation_quality =
      static_cast<InterpolationQuality>(iq_value);

  ImageExt::ClipResizeAndOrientImage(image, orientation, clip_rect,
                                     image_scale, opacity,
                                     interpolation_quality);
}

void FuzzClipResizeAndOrientImageEmptySize(FuzzedDataProvider* fdp) {
  int width = fdp->ConsumeIntegralInRange<int>(1, 100);
  int height = fdp->ConsumeIntegralInRange<int>(1, 100);
  PaintImage image = CreateTestPaintImage(width, height);
  if (image.IsLazyGenerated()) {
    return;
  }

  ImageOrientationEnum orientation = ImageOrientationEnum::kDefault;

  float scale_x = fdp->ConsumeFloatingPointInRange<float>(0.001f, 0.01f);
  float scale_y = fdp->ConsumeFloatingPointInRange<float>(0.001f, 0.01f);
  gfx::Vector2dF image_scale(scale_x, scale_y);

  int scaled_w = static_cast<int>(width * scale_x);
  int scaled_h = static_cast<int>(height * scale_y);
  if (scaled_w <= 0 || scaled_h <= 0) {
    return;
  }
  gfx::Rect clip_rect(0, 0, scaled_w, scaled_h);

  float opacity = 1.0f;
  InterpolationQuality interpolation_quality = kInterpolationNone;

  ImageExt::ClipResizeAndOrientImage(image, orientation, clip_rect,
                                     image_scale, opacity,
                                     interpolation_quality);
}

void FuzzClipResizeAndOrientImageIdentity(FuzzedDataProvider* fdp) {
  int width = fdp->ConsumeIntegralInRange<int>(1, 500);
  int height = fdp->ConsumeIntegralInRange<int>(1, 500);
  PaintImage image = CreateTestPaintImage(width, height);
  if (image.IsLazyGenerated()) {
    return;
  }

  ImageOrientationEnum orientation = ImageOrientationEnum::kDefault;

  gfx::Rect clip_rect(0, 0, width, height);

  gfx::Vector2dF image_scale(1.0f, 1.0f);

  float opacity = 1.0f;
  InterpolationQuality interpolation_quality = kInterpolationNone;

  ImageExt::ClipResizeAndOrientImage(image, orientation, clip_rect,
                                     image_scale, opacity,
                                     interpolation_quality);
}

void FuzzClipResizeAndOrientImageTranspose(FuzzedDataProvider* fdp) {
  int width = fdp->ConsumeIntegralInRange<int>(50, 500);
  int height = fdp->ConsumeIntegralInRange<int>(50, 500);
  PaintImage image = CreateTestPaintImage(width, height);
  if (image.IsLazyGenerated()) {
    return;
  }

  int orientation_value = fdp->ConsumeIntegralInRange<int>(5, 8);
  ImageOrientationEnum orientation =
      static_cast<ImageOrientationEnum>(orientation_value);

  gfx::Rect clip_rect(0, 0, width, height);

  gfx::Vector2dF image_scale(1.0f, 1.0f);

  float opacity = fdp->ConsumeFloatingPointInRange<float>(0.5f, 1.0f);
  InterpolationQuality interpolation_quality =
      fdp->ConsumeBool() ? kInterpolationNone : kInterpolationLow;

  ImageExt::ClipResizeAndOrientImage(image, orientation, clip_rect,
                                     image_scale, opacity,
                                     interpolation_quality);
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static bool initialized = false;
  if (!initialized) {
    base::CommandLine::Init(0, nullptr);
    initialized = true;
  }

  FuzzedDataProvider fdp(data, size);

  int selector = fdp.ConsumeIntegralInRange<int>(0, 3);
  switch (selector) {
    case 0:
      blink::FuzzClipResizeAndOrientImage(&fdp);
      break;
    case 1:
      blink::FuzzClipResizeAndOrientImageEmptySize(&fdp);
      break;
    case 2:
      blink::FuzzClipResizeAndOrientImageIdentity(&fdp);
      break;
    case 3:
      blink::FuzzClipResizeAndOrientImageTranspose(&fdp);
      break;
  }

  return 0;
}