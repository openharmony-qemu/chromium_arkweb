// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_WHITE_SCREEN_FRAME_ANALYSIS_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_WHITE_SCREEN_FRAME_ANALYSIS_H_

#include "ui/gfx/geometry/size.h"

namespace viz {
class CompositorFrame;
}

namespace blank_screen {

// Frame analysis result (viz thread → UI thread).
struct FrameStats {
  int total_quads = 0;
  int solid_color_quads = 0;
  int content_quads = 0;       // tile/picture + texture + surface
  float white_area = 0.0f;
  float content_area = 0.0f;
  float white_area_ratio = 0.0f;
  float content_area_ratio = 0.0f;
  // True if content has transparency (text/graphic shapes, not solid background).
  bool has_text_like_content = false;
};

// Analyze a CompositorFrame on the viz thread. Iterates DrawQuads to
// compute content/blank area ratios.
FrameStats AnalyzeFrameOnVizThread(const viz::CompositorFrame& frame,
                                   const gfx::Size& viewport_size);

// Single-frame blank detection thresholds.
constexpr float kWhiteAreaThreshold = 0.95f;
constexpr float kContentAreaThreshold = 0.05f;
constexpr float kNoTextContentAreaThreshold = 0.2f;
constexpr int kMinTotalQuadsForContent = 3;

// Returns true if the frame is blank (any one condition matches).
inline bool IsBlankFrame(const FrameStats& stats) {
  // Rule 1: no content quads + mostly white area.
  if (stats.content_quads == 0 && stats.white_area_ratio > kWhiteAreaThreshold)
    return true;
  // Rule 2: very few quads + tiny content area.
  if (stats.total_quads <= kMinTotalQuadsForContent &&
      stats.content_area_ratio < kContentAreaThreshold)
    return true;
  // Rule 3: no text-like content + low content area ratio.
  if (!stats.has_text_like_content &&
      stats.content_area_ratio < kNoTextContentAreaThreshold)
    return true;
  return false;
}

}  // namespace blank_screen

#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_WHITE_SCREEN_FRAME_ANALYSIS_H_
