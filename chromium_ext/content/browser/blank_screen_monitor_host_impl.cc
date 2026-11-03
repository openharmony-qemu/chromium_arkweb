// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/content/browser/blank_screen_monitor_host_impl.h"

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#include "arkweb/chromium_ext/components/blank_screen/frame_stats_bridge.h"

namespace content {

void BlankScreenMonitorHostImpl::OnFrameAnalyzed(
    const viz::FrameSinkId& frame_sink_id,
    int32_t total_quads,
    int32_t solid_color_quads,
    int32_t content_quads,
    float white_area_ratio,
    float content_area_ratio,
    bool has_text_like_content) {
  blank_screen::FrameStats stats;
  stats.total_quads = total_quads;
  stats.solid_color_quads = solid_color_quads;
  stats.content_quads = content_quads;
  stats.white_area_ratio = white_area_ratio;
  stats.content_area_ratio = content_area_ratio;
  stats.has_text_like_content = has_text_like_content;

  blank_screen::FrameStatsBridge::GetInstance().OnFrameAnalyzedFromGpuProcess(
      frame_sink_id, stats);
}

void BlankScreenMonitorHostImpl::OnJsException(
    const viz::FrameSinkId& frame_sink_id,
    int32_t source,
    const std::string& message,
    const std::string& url,
    int32_t line) {
  blank_screen::FrameStatsBridge::GetInstance().OnJsExceptionFromRenderer(
      frame_sink_id, source, message, url, line);
}

}  // namespace content

#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)
