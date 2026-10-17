// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLANK_SCREEN_MONITOR_HOST_IMPL_H_
#define CONTENT_BROWSER_BLANK_SCREEN_MONITOR_HOST_IMPL_H_

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "arkweb/chromium_ext/components/blank_screen/blank_screen_monitor.mojom.h"
#include "arkweb/chromium_ext/components/blank_screen/frame_analysis.h"

namespace content {

class RenderFrameHostImpl;

// Browser-side implementation of BlankScreenMonitorHost Mojo interface.
// Receives frame analysis results from Renderer, forwards to FrameStatsBridge.
class BlankScreenMonitorHostImpl
    : public blank_screen::mojom::BlankScreenMonitorHost {
 public:
  BlankScreenMonitorHostImpl() = default;
  ~BlankScreenMonitorHostImpl() override = default;

  BlankScreenMonitorHostImpl(const BlankScreenMonitorHostImpl&) = delete;
  BlankScreenMonitorHostImpl& operator=(const BlankScreenMonitorHostImpl&) =
      delete;

  // Creates a self-owned receiver that auto-deletes when the pipe disconnects.
  static void ProcessPendingReceiver(
      mojo::PendingReceiver<blank_screen::mojom::BlankScreenMonitorHost>
          receiver) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<BlankScreenMonitorHostImpl>(),
                                std::move(receiver));
  }

  // BlankScreenMonitorHost Mojo interface implementation.
  // Forwards frame analysis results and JS exceptions from Renderer to
  // FrameStatsBridge on the browser UI thread.
  void OnFrameAnalyzed(
      const viz::FrameSinkId& frame_sink_id,
      int32_t total_quads,
      int32_t solid_color_quads,
      int32_t content_quads,
      float white_area_ratio,
      float content_area_ratio,
      bool has_text_like_content) override;

  void OnJsException(const viz::FrameSinkId& frame_sink_id,
                     int32_t source,
                     const std::string& message,
                     const std::string& url,
                     int32_t line) override;
};

}  // namespace content

#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#endif  // CONTENT_BROWSER_BLANK_SCREEN_MONITOR_HOST_IMPL_H_
