// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_BLANK_SCREEN_MONITOR_H_
#define ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_BLANK_SCREEN_MONITOR_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/timer/timer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "arkweb/chromium_ext/components/blank_screen/frame_analysis.h"
#include "arkweb/chromium_ext/components/blank_screen/frame_stats_bridge.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "arkweb/chromium_ext/components/blank_screen/attribution_engine.h"
#include "arkweb/chromium_ext/components/blank_screen/gpu_evidence_collector.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "url/gurl.h"

namespace blank_screen {

// White screen detection V2: based on CompositorFrame Quad analysis.

class BlankScreenMonitor : public content::WebContentsObserver {
 public:
  explicit BlankScreenMonitor(content::WebContents* web_contents);
  ~BlankScreenMonitor() override;

  BlankScreenMonitor(const BlankScreenMonitor&) = delete;
  BlankScreenMonitor& operator=(const BlankScreenMonitor&) = delete;

  // Called when frame analysis result arrives from viz thread to UI thread.
  void OnFrameStatsReceived(const FrameStats& stats);

  // Called when JS exception arrives from renderer process via Mojo.
  void OnJsExceptionReceived(int source,
                             const std::string& message,
                             const std::string& url,
                             int line);

  // Called on WebView resize (from NWebDelegate::Resize).
  // Reports kWebViewZeroSize if size is 0 with no active navigation.
  void NotifyViewResize(uint32_t width, uint32_t height);

  base::WeakPtr<BlankScreenMonitor> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void SetCallbackEnabled(bool enabled) { callback_enabled_ = enabled; }

 private:
  enum class State {
    kIdle,              // Waiting for navigation
    kNavStarted,        // Main frame DidStartNavigation received
    kNavCommitted,      // Main frame navigation committed
    kLoadFinished,      // Main frame DidFinishLoad triggered
    kDetectWindowOpen,  // Detection window open, frame analysis allowed
    kFinished,          // Conclusion reached, no further detection
  };

  // WebContentsObserver overrides
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;
  void OnDidAddMessageToConsole(
      content::RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;
  void OnFirstContentfulPaintInPrimaryMainFrame() override;

  void ResetForNewNavigation();

  // Timer callbacks
  void OnLoadWatchdogTimeout(uint64_t gen);
  void OnGraceTimeout(uint64_t gen);
  void OnRecheckTimeout(uint64_t gen);
  void OnContentConfirmTimeout(uint64_t gen);
  void OnZeroSizeConfirmTimeout();

  void OpenDetectWindow();

  bool IsActiveGen(uint64_t gen) const;

  // GPU process event from GpuEvidenceCollector
  void OnGpuProcessEvent(bool is_watchdog, int exit_code,
                         const std::string& reason);

  void Report(RootCauseReport report);

  void ClearGpuEvidence();

  // Helper methods for ResourceLoadComplete
  void DetectBlockedResource(
      const blink::mojom::ResourceLoadInfo& resource_load_info,
      int net_error,
      bool& is_blocked);
  void TrackFailedResource(
      const blink::mojom::ResourceLoadInfo& resource_load_info,
      int net_error,
      bool is_script,
      bool is_blocked);

  // Helper methods for DidFinishNavigation
  void HandleErrorNavigation(content::NavigationHandle* navigation_handle,
                             int net_error);
  void HandleNormalCommit(content::NavigationHandle* navigation_handle);

  // Helper method for OnFrameStatsReceived — processes a frame in
  // the kDetectWindowOpen state.
  void ProcessFrameInDetectWindow(const FrameStats& stats);

  // Starts the load watchdog timer for the current navigation generation.
  void StartLoadWatchdog();

  viz::FrameSinkId frame_sink_id_;

  State state_ = State::kIdle;
  uint64_t nav_generation_ = 0;
  bool page_visible_ = true;
  bool fcp_received_ = false;

  // GPU process state (global, not reset per navigation)
  bool gpu_crashed_ = false;
  bool gpu_watchdog_ = false;
  int gpu_exit_code_ = 0;
  std::string gpu_crash_reason_;

  GURL committed_url_;

  int blank_streak_ = 0;
  int content_streak_ = 0;  // Consecutive content frames (500ms interval each)
  FrameStats last_stats_;

  AttributionContext attribution_ctx_;

  // Timers
  base::OneShotTimer load_watchdog_timer_;  // Fallback after commit if no DidFinishLoad
  base::OneShotTimer grace_timer_;          // Grace period after load
  base::OneShotTimer recheck_timer_;        // Recheck to confirm persistent blank
  base::OneShotTimer zero_size_timer_;      // 0x0 confirmation timer

  // Configuration constants
  static constexpr base::TimeDelta kLoadWatchdogTimeout = base::Seconds(8);
  static constexpr base::TimeDelta kGracePeriod = base::Seconds(3);
  static constexpr base::TimeDelta kRecheckDelay = base::Seconds(1);
  static constexpr base::TimeDelta kZeroSizeConfirmDelay = base::Seconds(3);
  static constexpr int kMinBlankStreak = 3;
  static constexpr int kMinContentStreak = 3;  // 3 consecutive content frames (3 × 500ms = 1.5s)
  static constexpr base::TimeDelta kContentConfirmInterval = base::Milliseconds(500);

  // Subresource loading records (FIFO, max 50)
  struct ResourceLoadEntry {
    GURL url;
    int net_error = 0;
    int http_status_code = 0;
  };
  std::vector<ResourceLoadEntry> resource_entries_;
  static constexpr size_t kMaxResourceEntries = 50;

  static constexpr size_t kMaxCollectedMessages = 3;
  static constexpr size_t kMaxMessageLength = 500;

  // Console error deduplication (path C)
  std::set<std::string> seen_console_messages_;

  // Marks current navigation as network/HTTP error navigation.
  // Error page subresources and JS exceptions should not count as blank screen evidence.
  bool is_error_navigation_ = false;

  bool callback_enabled_ = false;

  GpuEvidenceCollector::CallbackId gpu_callback_id_ = 0;

  base::WeakPtrFactory<BlankScreenMonitor> weak_factory_{this};
};

}  // namespace blank_screen

#endif  // ARKWEB_CHROMIUM_EXT_COMPONENTS_BLANK_SCREEN_BLANK_SCREEN_MONITOR_H_
