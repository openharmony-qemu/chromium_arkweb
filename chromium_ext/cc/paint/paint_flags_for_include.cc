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
#include "third_party/skia/include/effects/SkColorMatrix.h"

namespace cc {
#if BUILDFLAG(ARKWEB_NIGHT_MODE)
void PaintFlags::SetColorFilterWithZeroSaturation() {
  if (!getColorFilter().get()) {
    return;
  }
  sk_sp<SkColorFilter> colorFilter = getColorFilter()->sk_color_filter_;
  SkColorMatrix grayscale_matrix;
  float matrix[20];
  if (colorFilter.get()) {
    bool res = colorFilter->asAColorMatrix(matrix);
    if (res) {
      grayscale_matrix.setRowMajor(matrix);
    }
  }
  grayscale_matrix.setSaturation(0.0f);
  grayscale_matrix.getRowMajor(matrix);
  setColorFilter(ColorFilter::MakeMatrix(matrix));
}
#endif
}