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
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/page/drag_image_ext.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace blink {

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();
  }

  ~Environment() {}

  content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

static PaintImage CreateWhitePaintImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorWHITE);
  return PaintImage::CreateFromBitmap(bitmap);
}

void FuzzHwClampedImageScale(FuzzedDataProvider* fdp) {
  int image_width = fdp->ConsumeIntegral<int>();
  int image_height = fdp->ConsumeIntegral<int>();
  int element_width = fdp->ConsumeIntegral<int>();
  int element_height = fdp->ConsumeIntegral<int>();
  float target_scale = fdp->ConsumeFloatingPoint<float>();

  gfx::Size image_size(image_width, image_height);
  gfx::Size element_size(element_width, element_height);

  gfx::Vector2dF result = DragImageExt::HwClampedImageScale(image_size, element_size, target_scale);
  (void)result;
}

void FuzzFilterNonPrintable(FuzzedDataProvider* fdp) {
  std::string input = fdp->ConsumeRandomLengthString();
  String result = DragImageExt::filterNonPrintable(String::FromUTF8(input.c_str()));
  (void)result;
}

void FuzzFilterNonPrintableEdgeCases() {
  String empty = String();
  String result1 = DragImageExt::filterNonPrintable(empty);
  (void)result1;

  String all_printable = String::FromUTF8("Hello World 123!");
  String result2 = DragImageExt::filterNonPrintable(all_printable);
  (void)result2;

  String all_non_printable = String::FromUTF8("\x01\x02\x03\x04\x05");
  String result3 = DragImageExt::filterNonPrintable(all_non_printable);
  (void)result3;

  String mixed = String::FromUTF8("Hello\x00World\x7FTest");
  String result4 = DragImageExt::filterNonPrintable(mixed);
  (void)result4;
}

void FuzzCreateClippedByVisualViewportNullImage() {
  DragImageExt::CreateClippedByVisualViewport(
      nullptr,
      gfx::Rect(0, 0, 100, 100),
      kRespectImageOrientation,
      1.0f,
      kInterpolationDefault,
      1.0f,
      gfx::Vector2dF(1, 1));
}

void FuzzCreateClippedByVisualViewportWithImage(FuzzedDataProvider* fdp) {
  int img_w = fdp->ConsumeIntegralInRange<int>(1, 500);
  int img_h = fdp->ConsumeIntegralInRange<int>(1, 500);
  int clip_x = fdp->ConsumeIntegral<int>();
  int clip_y = fdp->ConsumeIntegral<int>();
  int clip_w = fdp->ConsumeIntegralInRange<int>(1, 1000);
  int clip_h = fdp->ConsumeIntegralInRange<int>(1, 1000);
  int scale_x = fdp->ConsumeIntegralInRange<int>(1, 100);
  int scale_y = fdp->ConsumeIntegralInRange<int>(1, 100);
  float opacity = fdp->ConsumeFloatingPointInRange<float>(0.0f, 1.0f);

  PaintImage paint_image = CreateWhitePaintImage(img_w, img_h);
  if (!paint_image) {
    return;
  }
  scoped_refptr<StaticBitmapImage> bitmap_image = StaticBitmapImage::Create(paint_image);
  if (!bitmap_image || !bitmap_image->PaintImageForCurrentFrame()) {
    return;
  }
  Image* image = bitmap_image.get();

  DragImageExt::CreateClippedByVisualViewport(
      image,
      gfx::Rect(clip_x, clip_y, clip_w, clip_h),
      kRespectImageOrientation,
      1.0f,
      kInterpolationDefault,
      opacity,
      gfx::Vector2dF(scale_x, scale_y));

  DragImageExt::CreateClippedByVisualViewport(
      image,
      gfx::Rect(clip_x, clip_y, clip_w, clip_h),
      kDoNotRespectImageOrientation,
      2.0f,
      kInterpolationNone,
      0.5f,
      gfx::Vector2dF(scale_x, scale_y));

  DragImageExt::CreateClippedByVisualViewport(
      image,
      gfx::Rect(0, 0, clip_w, clip_h),
      kRespectImageOrientation,
      1.0f,
      kInterpolationMedium,
      0.0f,
      gfx::Vector2dF(scale_x, scale_y));

  DragImageExt::CreateClippedByVisualViewport(
      image,
      gfx::Rect(clip_x, clip_y, clip_w, clip_h),
      kRespectImageOrientation,
      1.0f,
      kInterpolationDefault,
      1.0f,
      gfx::Vector2dF(0, 0));
}

void FuzzCreateClippedByVisualViewportEdgeCases(FuzzedDataProvider* fdp) {
  int clip_x = fdp->ConsumeIntegral<int>();
  int clip_y = fdp->ConsumeIntegral<int>();
  int clip_w = fdp->ConsumeIntegralInRange<int>(1, 1000);
  int clip_h = fdp->ConsumeIntegralInRange<int>(1, 1000);
  int scale_x = fdp->ConsumeIntegralInRange<int>(1, 100);
  int scale_y = fdp->ConsumeIntegralInRange<int>(1, 100);

  DragImageExt::CreateClippedByVisualViewport(
      nullptr,
      gfx::Rect(clip_x, clip_y, clip_w, clip_h),
      kRespectImageOrientation,
      1.0f,
      kInterpolationDefault,
      1.0f,
      gfx::Vector2dF(scale_x, scale_y));

  DragImageExt::CreateClippedByVisualViewport(
      nullptr,
      gfx::Rect(clip_x, clip_y, clip_w, clip_h),
      kDoNotRespectImageOrientation,
      2.0f,
      kInterpolationNone,
      0.5f,
      gfx::Vector2dF(scale_x, scale_y));

  DragImageExt::CreateClippedByVisualViewport(
      nullptr,
      gfx::Rect(0, 0, clip_w, clip_h),
      kRespectImageOrientation,
      1.0f,
      kInterpolationMedium,
      0.0f,
      gfx::Vector2dF(scale_x, scale_y));

  DragImageExt::CreateClippedByVisualViewport(
      nullptr,
      gfx::Rect(clip_x, clip_y, clip_w, clip_h),
      kRespectImageOrientation,
      1.0f,
      kInterpolationDefault,
      1.0f,
      gfx::Vector2dF(0, 0));

  DragImageExt::CreateClippedByVisualViewport(
      nullptr,
      gfx::Rect(0, 0, 0, 0),
      kRespectImageOrientation,
      1.0f,
      kInterpolationLow,
      1.0f,
      gfx::Vector2dF(1, 1));
}

void FuzzHwClampedImageScaleEdgeCases() {
  DragImageExt::HwClampedImageScale(gfx::Size(0, 0), gfx::Size(100, 100), 1.0f);
  DragImageExt::HwClampedImageScale(gfx::Size(100, 100), gfx::Size(0, 0), 1.0f);
  DragImageExt::HwClampedImageScale(gfx::Size(100, 100), gfx::Size(100, 100), 0.0f);
  DragImageExt::HwClampedImageScale(gfx::Size(100, 100), gfx::Size(100, 100), -1.0f);
  DragImageExt::HwClampedImageScale(gfx::Size(1, 1), gfx::Size(10000, 10000), 10.0f);
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  static blink::Environment env;

  FuzzedDataProvider fdp(data, size);

  int selector = fdp.ConsumeIntegralInRange<int>(0, 6);
  switch (selector) {
    case 0:
      blink::FuzzHwClampedImageScale(&fdp);
      break;
    case 1:
      blink::FuzzFilterNonPrintable(&fdp);
      break;
    case 2:
      blink::FuzzFilterNonPrintableEdgeCases();
      break;
    case 3:
      blink::FuzzCreateClippedByVisualViewportNullImage();
      break;
    case 4:
      blink::FuzzCreateClippedByVisualViewportEdgeCases(&fdp);
      break;
    case 5:
      blink::FuzzHwClampedImageScaleEdgeCases();
      break;
    case 6:
      blink::FuzzCreateClippedByVisualViewportWithImage(&fdp);
      break;
  }

  return 0;
}