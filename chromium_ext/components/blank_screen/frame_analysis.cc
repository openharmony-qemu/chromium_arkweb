// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/components/blank_screen/frame_analysis.h"

#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blank_screen {

namespace {

// Returns true for transparent or near-white colors.
bool IsBlankColor(SkColor color) {
  if (SkColorGetA(color) < 5) {
    return true;
  }
  int r = SkColorGetR(color);
  int g = SkColorGetG(color);
  int b = SkColorGetB(color);
  return (r > 245 && g > 245 && b > 245);
}

// Processes a single draw quad, updating stats accordingly.
void ProcessQuad(const viz::DrawQuad* quad, float quad_area,
                 FrameStats& stats) {
  stats.total_quads++;

  switch (quad->material) {
    case viz::DrawQuad::Material::kSolidColor: {
      stats.solid_color_quads++;
      const auto* solid_quad = viz::SolidColorDrawQuad::MaterialCast(quad);
      SkColor color = solid_quad->color.toSkColor();
      if (IsBlankColor(color)) {
        stats.white_area += quad_area;
      }
      break;
    }

    // Content quads: real rendered content on the page.
    case viz::DrawQuad::Material::kPictureContent:
    case viz::DrawQuad::Material::kTiledContent:
      stats.content_quads++;
      stats.content_area += quad_area;
      // Text-like content has transparency (glyph gaps).
      if (!quad->shared_quad_state->are_contents_opaque ||
          quad->needs_blending) {
        stats.has_text_like_content = true;
      }
      break;

    case viz::DrawQuad::Material::kTextureContent:
      stats.content_quads++;
      stats.content_area += quad_area;
      break;

    case viz::DrawQuad::Material::kSurfaceContent:
      stats.content_quads++;
      stats.content_area += quad_area;
      break;

    default:
      // RenderPass, DebugBorder, VideoHole, etc.: not counted.
      break;
  }
}

}  // namespace

FrameStats AnalyzeFrameOnVizThread(const viz::CompositorFrame& frame,
                                   const gfx::Size& viewport_size) {
  FrameStats stats;

  const auto& render_pass_list = frame.render_pass_list;

  if (render_pass_list.empty()) {
    stats.white_area_ratio = 1.0f;
    return stats;
  }

  float viewport_area = static_cast<float>(viewport_size.width()) *
                        static_cast<float>(viewport_size.height());
  if (viewport_area <= 0.0f) {
    return stats;
  }

  for (const auto& render_pass : render_pass_list) {
    for (auto it = render_pass->quad_list.cbegin();
         it != render_pass->quad_list.cend(); ++it) {
      const viz::DrawQuad* quad = *it;
      float quad_area = static_cast<float>(quad->visible_rect.width()) *
                        static_cast<float>(quad->visible_rect.height());
      ProcessQuad(quad, quad_area, stats);
    }
  }

  stats.white_area_ratio = stats.white_area / viewport_area;
  stats.content_area_ratio = stats.content_area / viewport_area;

  return stats;
}

}  // namespace blank_screen
