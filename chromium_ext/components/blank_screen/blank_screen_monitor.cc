// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/components/blank_screen/blank_screen_monitor.h"
#include "arkweb/chromium_ext/components/blank_screen/gpu_evidence_collector.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace blank_screen {

constexpr char kBlankScreenEvent[] = "ARKWEB_BLANK_SCREEN_EVENT";

BlankScreenMonitor::BlankScreenMonitor(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  if (web_contents) {
    auto* rwhv = web_contents->GetRenderWidgetHostView();
    if (rwhv) {
      auto* rwh = rwhv->GetRenderWidgetHost();
      if (rwh) {
        frame_sink_id_ = rwh->GetFrameSinkId();
      }
    }
  }

  if (frame_sink_id_.is_valid()) {
    auto weak_ptr = weak_factory_.GetWeakPtr();
    FrameStatsBridge::GetInstance().Register(
        frame_sink_id_,
        base::BindRepeating(
            &BlankScreenMonitor::OnFrameStatsReceived, weak_ptr));
    FrameStatsBridge::GetInstance().RegisterJsExceptionCallback(
        frame_sink_id_,
        base::BindRepeating(
            &BlankScreenMonitor::OnJsExceptionReceived, weak_ptr));
    LOG(DEBUG) << "[BlankScreenMonitor] Detector created, registered FrameSinkId="
              << frame_sink_id_.ToString();
  } else {
    LOG(DEBUG) << "[BlankScreenMonitor] Detector created, FrameSinkId not yet available";
  }

  // GPU crash is a global event affecting all WebViews.
  gpu_callback_id_ = GpuEvidenceCollector::GetInstance().RegisterCallback(
      base::BindRepeating(&BlankScreenMonitor::OnGpuProcessEvent,
                          weak_factory_.GetWeakPtr()));
}

BlankScreenMonitor::~BlankScreenMonitor() {
  GpuEvidenceCollector::GetInstance().UnregisterCallback(gpu_callback_id_);
  if (frame_sink_id_.is_valid()) {
    FrameStatsBridge::GetInstance().Unregister(frame_sink_id_);
    FrameStatsBridge::GetInstance().UnregisterJsExceptionCallback(frame_sink_id_);
    LOG(DEBUG) << "[BlankScreenMonitor] Detector destroyed, unregistered FrameSinkId="
              << frame_sink_id_.ToString();
  }
}

void BlankScreenMonitor::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  // Skip browser-internal pages.
  const auto& url = navigation_handle->GetURL();
  if (url.SchemeIs("hwweb") || url.SchemeIs("chrome") ||
      url.SchemeIs("chrome-error")) {
    LOG(DEBUG) << "[BlankScreenMonitor] Skip internal scheme";
    return;
  }

  // Skip detection when WebView is not visible.
  if (web_contents() &&
      web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    page_visible_ = false;
    LOG(DEBUG) << "[BlankScreenMonitor] WebView not visible, skip detection";
    return;
  }

  LOG(DEBUG) << "[BlankScreenMonitor] DidStartNavigation(main)"
            << " gen=" << (nav_generation_ + 1);

  nav_generation_++;
  ResetForNewNavigation();
  state_ = State::kNavStarted;
}

void BlankScreenMonitor::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  const auto& url = navigation_handle->GetURL();
  if (url.SchemeIs("hwweb") || url.SchemeIs("chrome") ||
      url.SchemeIs("chrome-error"))
    return;

  if (state_ == State::kIdle)
    return;

  int net_error = navigation_handle->GetNetErrorCode();
  // ERR_ABORTED means navigation was superseded, not a real error.
  if (net_error == net::ERR_ABORTED) {
    LOG(DEBUG) << "[BlankScreenMonitor] Navigation aborted (replaced), ignoring";
    return;
  }

  // BFCache restore / prerender activation: content already rendered.
  if (navigation_handle->IsPageActivation()) {
    LOG(DEBUG) << "[BlankScreenMonitor] Page activation (BFCache/prerender), "
              << "content already rendered";
    state_ = State::kFinished;
    load_watchdog_timer_.Stop();
    grace_timer_.Stop();
    recheck_timer_.Stop();
    return;
  }

  if (state_ == State::kFinished) {
    LOG(DEBUG) << "[BlankScreenMonitor] Content already confirmed (fast path), "
              << "skipping error handling";
    return;
  }

  // Case A: Network failure / error page.
  // Case B: HTTP 4xx/5xx.
  if (navigation_handle->IsErrorPage() || net_error != net::OK ||
      (navigation_handle->HasCommitted() &&
       navigation_handle->GetResponseHeaders() &&
       navigation_handle->GetResponseHeaders()->response_code() >= 400)) {
    HandleErrorNavigation(navigation_handle, net_error);
    return;
  }

  if (!navigation_handle->HasCommitted())
    return;

  // Case C: Normal commit.
  HandleNormalCommit(navigation_handle);
}

void BlankScreenMonitor::HandleErrorNavigation(
    content::NavigationHandle* navigation_handle,
    int net_error) {
  const auto* headers = navigation_handle->GetResponseHeaders();
  int http_status = headers ? headers->response_code() : 0;

  if (navigation_handle->IsErrorPage() || net_error != net::OK) {
    LOG(DEBUG) << "[BlankScreenMonitor] Network error: "
                 << net::ErrorToString(net_error);
    attribution_ctx_.net_error = net_error;
    attribution_ctx_.http_status = http_status;
    attribution_ctx_.has_response_headers = (headers != nullptr);
  } else {
    LOG(DEBUG) << "[BlankScreenMonitor] HTTP error: " << http_status;
    attribution_ctx_.net_error = net::OK;
    attribution_ctx_.http_status = http_status;
    attribution_ctx_.has_response_headers = (headers != nullptr);
  }

  attribution_ctx_.url = navigation_handle->GetURL();
  is_error_navigation_ = true;
  committed_url_ = navigation_handle->GetURL();
  state_ = State::kNavCommitted;
  LOG(DEBUG) << "[BlankScreenMonitor] Error stored, entering detection flow";

  StartLoadWatchdog();
}

void BlankScreenMonitor::HandleNormalCommit(
    content::NavigationHandle* navigation_handle) {
  const auto* headers = navigation_handle->GetResponseHeaders();

  committed_url_ = navigation_handle->GetURL();
  attribution_ctx_.url = navigation_handle->GetURL();
  attribution_ctx_.http_status = headers ? headers->response_code() : 0;
  attribution_ctx_.has_response_headers = (headers != nullptr);

  if (headers) {
    auto cl = headers->GetContentLength();
    attribution_ctx_.content_length = cl.has_value() ? cl->InBytes() : -1;
    // CSP-blocked subresources don't trigger ResourceLoadComplete in browser
    // process, so CSP header presence + failed_js_count is used as inference.
    if (headers->HasHeader("Content-Security-Policy") ||
        headers->HasHeader("Content-Security-Policy-Report-Only")) {
      attribution_ctx_.has_csp_header = true;
      LOG(DEBUG) << "[BlankScreenMonitor] CSP header detected on main document";
    }
  }

  state_ = State::kNavCommitted;
  LOG(DEBUG) << "[BlankScreenMonitor] Committed, gen=" << nav_generation_;

  StartLoadWatchdog();
}

void BlankScreenMonitor::StartLoadWatchdog() {
  uint64_t gen = nav_generation_;
  load_watchdog_timer_.Start(
      FROM_HERE, kLoadWatchdogTimeout,
      base::BindOnce(&BlankScreenMonitor::OnLoadWatchdogTimeout,
                     weak_factory_.GetWeakPtr(), gen));
}

void BlankScreenMonitor::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host != web_contents()->GetPrimaryMainFrame())
    return;
  if (state_ == State::kFinished)
    return;
  if (state_ < State::kNavCommitted)
    return;

  LOG(DEBUG) << "[BlankScreenMonitor] DidFinishLoad, starting grace period ("
            << kGracePeriod.InSeconds() << "s) gen=" << nav_generation_;

  state_ = State::kLoadFinished;

  load_watchdog_timer_.Stop();

  uint64_t gen = nav_generation_;
  grace_timer_.Start(
      FROM_HERE, kGracePeriod,
      base::BindOnce(&BlankScreenMonitor::OnGraceTimeout,
                     weak_factory_.GetWeakPtr(), gen));
}

void BlankScreenMonitor::OnVisibilityChanged(
    content::Visibility visibility) {
  bool was_visible = page_visible_;
  page_visible_ = (visibility == content::Visibility::VISIBLE);

  if (!page_visible_ && was_visible) {
    // Background: cancel ongoing detection.
    if (state_ != State::kFinished && state_ != State::kIdle) {
      LOG(DEBUG) << "[BlankScreenMonitor] WebView became invisible during detection, cancelling";
      state_ = State::kIdle;
      load_watchdog_timer_.Stop();
      grace_timer_.Stop();
      recheck_timer_.Stop();
      attribution_ctx_.Reset();
    }
    return;
  }

  if (page_visible_ && !was_visible) {
    // Foreground: reset streak counters.
    blank_streak_ = 0;
    content_streak_ = 0;
    recheck_timer_.Stop();
    LOG(DEBUG) << "[BlankScreenMonitor] Resumed from background, reset streaks";

    if (state_ == State::kDetectWindowOpen && last_stats_.total_quads > 0) {
      LOG(DEBUG) << "[BlankScreenMonitor] Re-evaluating stored frame after resume";
      if (IsBlankFrame(last_stats_)) {
        blank_streak_ = 1;
        uint64_t gen = nav_generation_;
        recheck_timer_.Start(
            FROM_HERE, kRecheckDelay,
            base::BindOnce(&BlankScreenMonitor::OnRecheckTimeout,
                           weak_factory_.GetWeakPtr(), gen));
      } else {
        content_streak_ = 1;
        LOG(DEBUG) << "[BlankScreenMonitor] Stored frame has content after resume, "
                  << "content_streak=1/" << kMinContentStreak;
        uint64_t gen = nav_generation_;
        recheck_timer_.Start(
            FROM_HERE, kContentConfirmInterval,
            base::BindOnce(&BlankScreenMonitor::OnContentConfirmTimeout,
                           weak_factory_.GetWeakPtr(), gen));
      }
    }
  }
}

void BlankScreenMonitor::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  const char* status_str = "UNKNOWN";
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      status_str = "NORMAL_TERMINATION"; break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      status_str = "ABNORMAL_TERMINATION"; break;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      status_str = "PROCESS_WAS_KILLED"; break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      status_str = "PROCESS_CRASHED"; break;
    case base::TERMINATION_STATUS_STILL_RUNNING:
      status_str = "STILL_RUNNING"; break;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      status_str = "LAUNCH_FAILED"; break;
    default: break;
  }
  LOG(ERROR) << "[BlankScreenMonitor] Renderer gone, status=" << static_cast<int>(status)
             << " (" << status_str << ")";

  // No active navigation — nothing to attribute.
  if (state_ == State::kIdle) {
    LOG(DEBUG) << "[BlankScreenMonitor] Renderer gone but state=kIdle, skip report";
    return;
  }

  // Report even if detection already finished — crash may be the real cause.
  LOG(ERROR) << "[BlankScreenMonitor] Renderer gone, direct report";

  attribution_ctx_.renderer_gone = true;
  attribution_ctx_.renderer_exit_status = static_cast<int>(status);
  attribution_ctx_.url = committed_url_;

  RootCauseReport r;
  Report(std::move(r));
}

void BlankScreenMonitor::OnLoadWatchdogTimeout(uint64_t gen) {
  if (!IsActiveGen(gen))
    return;
  if (state_ == State::kFinished)
    return;

  LOG(DEBUG) << "[BlankScreenMonitor] Load watchdog timeout ("
               << kLoadWatchdogTimeout.InSeconds()
               << "s), opening detect window";

  if (state_ == State::kNavCommitted) {
    OpenDetectWindow();
  }
}

void BlankScreenMonitor::OnGraceTimeout(uint64_t gen) {
  if (!IsActiveGen(gen))
    return;
  if (state_ == State::kFinished)
    return;

  LOG(DEBUG) << "[BlankScreenMonitor] Grace period expired, opening detect window";
  OpenDetectWindow();
}

void BlankScreenMonitor::OnRecheckTimeout(uint64_t gen) {
  if (!IsActiveGen(gen))
    return;
  if (state_ != State::kDetectWindowOpen)
    return;

  if (IsBlankFrame(last_stats_)) {
    RootCauseReport r;
    Report(std::move(r));
  } else {
    LOG(INFO) << "[BlankScreenMonitor] Recheck passed: content appeared, "
              << "blank_streak=" << blank_streak_;
  }
}

void BlankScreenMonitor::OnContentConfirmTimeout(uint64_t gen) {
  if (!IsActiveGen(gen))
    return;
  if (state_ != State::kDetectWindowOpen)
    return;

  // Check if the latest stored frame still has content.
  if (IsBlankFrame(last_stats_)) {
    content_streak_ = 0;
    LOG(DEBUG) << "[BlankScreenMonitor] Content confirm timeout: frame went blank, "
              << "resetting content_streak";
    return;
  }

  content_streak_++;
  LOG(DEBUG) << "[BlankScreenMonitor] Content confirm timeout: content_streak="
            << content_streak_ << "/" << kMinContentStreak;

  if (content_streak_ >= kMinContentStreak) {
    LOG(INFO) << "[BlankScreenMonitor] Content confirmed ("
              << kMinContentStreak << " consecutive frames via timer), "
              << "detection stopped";
    state_ = State::kFinished;
    recheck_timer_.Stop();
    return;
  }

  // Continue polling.
  recheck_timer_.Start(
      FROM_HERE, kContentConfirmInterval,
      base::BindOnce(&BlankScreenMonitor::OnContentConfirmTimeout,
                     weak_factory_.GetWeakPtr(), gen));
}

void BlankScreenMonitor::OnZeroSizeConfirmTimeout() {
  if (state_ == State::kFinished)
    return;

  LOG(DEBUG) << "[BlankScreenMonitor] Zero-size confirmed after "
               << kZeroSizeConfirmDelay.InSeconds()
               << "s, reporting";
  attribution_ctx_.url = web_contents() ? web_contents()->GetVisibleURL() : GURL();
  RootCauseReport r;
  Report(std::move(r));
}

void BlankScreenMonitor::OpenDetectWindow() {
  state_ = State::kDetectWindowOpen;
  blank_streak_ = 0;
  // content_streak_ is NOT reset here — it carries over from pre-window states.
  // If we already accumulated content frames before the window opened, they count.
  LOG(DEBUG) << "[BlankScreenMonitor] Detect window OPEN, gen=" << nav_generation_
            << " content_streak=" << content_streak_;

  // Evaluate stored frame immediately (static pages may produce only one frame).
  if (last_stats_.total_quads > 0) {
    LOG(DEBUG) << "[BlankScreenMonitor] Evaluating stored frame: "
              << "total_quads=" << last_stats_.total_quads
              << " content_quads=" << last_stats_.content_quads
              << " white_ratio=" << last_stats_.white_area_ratio
              << " content_ratio=" << last_stats_.content_area_ratio
              << " has_text_like_content=" << last_stats_.has_text_like_content;
    if (IsBlankFrame(last_stats_)) {
      blank_streak_ = 1;
      LOG(DEBUG) << "[BlankScreenMonitor] Stored frame is blank, "
                   << "scheduling recheck (" << kRecheckDelay.InSeconds()
                   << "s)";
      uint64_t gen = nav_generation_;
      recheck_timer_.Start(
          FROM_HERE, kRecheckDelay,
          base::BindOnce(&BlankScreenMonitor::OnRecheckTimeout,
                         weak_factory_.GetWeakPtr(), gen));
    } else {
      // Stored frame has content — count as first content frame.
      // For static pages that stop rendering, we use a timer to poll for
      // the remaining content confirmations at 500ms intervals.
      content_streak_ = 1;
      LOG(DEBUG) << "[BlankScreenMonitor] Stored frame has content, content_streak=1/"
                << kMinContentStreak << ", starting content confirmation timer";
      uint64_t gen = nav_generation_;
      recheck_timer_.Start(
          FROM_HERE, kContentConfirmInterval,
          base::BindOnce(&BlankScreenMonitor::OnContentConfirmTimeout,
                         weak_factory_.GetWeakPtr(), gen));
    }
  } else {
    // No frame stats yet — schedule a recheck.
    LOG(DEBUG) << "[BlankScreenMonitor] Detect window open, no frame stats yet, "
              << "scheduling recheck (" << kRecheckDelay.InSeconds() << "s)";
    uint64_t gen = nav_generation_;
    recheck_timer_.Start(
        FROM_HERE, kRecheckDelay,
        base::BindOnce(&BlankScreenMonitor::OnRecheckTimeout,
                       weak_factory_.GetWeakPtr(), gen));
  }
}

void BlankScreenMonitor::OnFrameStatsReceived(const FrameStats& stats) {
  LOG(DEBUG) << "[BlankScreenMonitor] OnFrameStatsReceived: state="
            << static_cast<int>(state_)
            << " total_quads=" << stats.total_quads
            << " content_quads=" << stats.content_quads
            << " white_ratio=" << stats.white_area_ratio
            << " content_ratio=" << stats.content_area_ratio
            << " has_text_like_content=" << stats.has_text_like_content;

  if (state_ == State::kFinished || state_ == State::kIdle)
    return;

  // During kNavStarted, old-page frames may still arrive.  Ignore them
  // completely — don't store in last_stats_, don't evaluate.
  if (state_ == State::kNavStarted) {
    LOG(DEBUG) << "[BlankScreenMonitor] Frame during kNavStarted, ignoring (old page)";
    return;
  }

  // Update last_stats_: don't let blank transition frames overwrite content frames.
  if (state_ != State::kDetectWindowOpen) {
    if (IsBlankFrame(stats) && last_stats_.total_quads > 0 &&
        !IsBlankFrame(last_stats_)) {
      // Keep existing content frame.
    } else {
      last_stats_ = stats;
    }
  } else {
    last_stats_ = stats;
  }

  // Fast path: outside the detection window, a single content frame confirms
  // the page is not blank.  kNavStarted already filtered old-page frames, so
  // any content frame here belongs to the new page.
  if (state_ != State::kDetectWindowOpen) {
    if (!IsBlankFrame(stats)) {
      LOG(INFO) << "[BlankScreenMonitor] Content confirmed (fast path, state="
                << static_cast<int>(state_) << "), detection stopped";
      state_ = State::kFinished;
      load_watchdog_timer_.Stop();
      grace_timer_.Stop();
      recheck_timer_.Stop();
    }
    return;
  }

  // === Detection window (kDetectWindowOpen) ===
  if (!page_visible_)
    return;

  ProcessFrameInDetectWindow(stats);
}

void BlankScreenMonitor::ProcessFrameInDetectWindow(const FrameStats& stats) {
  // Content confirmation in detection window: require kMinContentStreak
  // consecutive content frames (at 500ms frame stats interval) before
  // terminating detection.  This avoids false negatives from stale frames.
  if (!IsBlankFrame(stats)) {
    content_streak_++;
    LOG(DEBUG) << "[BlankScreenMonitor] Content frame in detect window, "
              << "content_streak=" << content_streak_ << "/" << kMinContentStreak;

    if (content_streak_ >= kMinContentStreak) {
      LOG(INFO) << "[BlankScreenMonitor] Content confirmed ("
                << kMinContentStreak << " consecutive frames in detect window), "
                << "detection stopped";
      state_ = State::kFinished;
      recheck_timer_.Stop();
      return;
    }

    // Content frame seen but not enough streak yet — clear blank streak
    // and start content confirmation timer for static pages that stop rendering.
    if (blank_streak_ > 0) {
      LOG(DEBUG) << "[BlankScreenMonitor] Content frame, clearing blank_streak (was "
                << blank_streak_ << ")";
    }
    blank_streak_ = 0;
    uint64_t gen = nav_generation_;
    recheck_timer_.Start(
        FROM_HERE, kContentConfirmInterval,
        base::BindOnce(&BlankScreenMonitor::OnContentConfirmTimeout,
                       weak_factory_.GetWeakPtr(), gen));
    return;
  }

  // Blank frame: reset content streak and stop any content confirmation timer.
  if (content_streak_ > 0) {
    LOG(DEBUG) << "[BlankScreenMonitor] Blank frame, resetting content_streak (was "
              << content_streak_ << ")";
  }
  content_streak_ = 0;
  recheck_timer_.Stop();

  blank_streak_++;
  if (blank_streak_ >= kMinBlankStreak) {
    LOG(DEBUG) << "[BlankScreenMonitor] " << blank_streak_
               << " consecutive blank frames, scheduling recheck ("
               << kRecheckDelay.InSeconds() << "s)";

    uint64_t gen = nav_generation_;
    recheck_timer_.Start(
        FROM_HERE, kRecheckDelay,
        base::BindOnce(&BlankScreenMonitor::OnRecheckTimeout,
                       weak_factory_.GetWeakPtr(), gen));
  }
}

void BlankScreenMonitor::NotifyViewResize(uint32_t width, uint32_t height) {
  LOG(DEBUG) << "[BlankScreen] BlankScreenMonitor::NotifyViewResize "
               << width << "x" << height
               << " state=" << static_cast<int>(state_);

  if (width > 1 && height > 1) {
    attribution_ctx_.webview_size_empty = false;
    zero_size_timer_.Stop();
    return;
  }

  attribution_ctx_.webview_size_empty = true;
  zero_size_timer_.Start(
      FROM_HERE, kZeroSizeConfirmDelay,
      base::BindOnce(&BlankScreenMonitor::OnZeroSizeConfirmTimeout,
                     weak_factory_.GetWeakPtr()));
}

bool BlankScreenMonitor::IsActiveGen(uint64_t gen) const {
  return gen == nav_generation_;
}

void BlankScreenMonitor::ResetForNewNavigation() {
  state_ = State::kIdle;
  committed_url_ = GURL();
  blank_streak_ = 0;
  content_streak_ = 0;
  last_stats_ = FrameStats();
  attribution_ctx_.Reset();
  resource_entries_.clear();
  seen_console_messages_.clear();
  is_error_navigation_ = false;
  fcp_received_ = false;
  // GPU evidence persists across navigations — cleared in Report() after consumption.

  load_watchdog_timer_.Stop();
  grace_timer_.Stop();
  recheck_timer_.Stop();
  zero_size_timer_.Stop();
}

void BlankScreenMonitor::DetectBlockedResource(
    const blink::mojom::ResourceLoadInfo& resource_load_info,
    int net_error,
    bool& is_blocked) {
  if (net_error == net::ERR_BLOCKED_BY_CSP) {
    attribution_ctx_.has_csp_blocked = true;
    is_blocked = true;
    LOG(DEBUG) << "[BlankScreenMonitor] Resource blocked by CSP";
  }
  if (net_error == net::ERR_BLOCKED_BY_RESPONSE) {
    attribution_ctx_.has_cors_blocked = true;
    is_blocked = true;
    LOG(DEBUG) << "[BlankScreenMonitor] Resource blocked by CORS";
  }
  // Mixed content: HTTPS main frame + HTTP subresource + load failure.
  if (net_error != net::OK) {
    const GURL& main_url = web_contents()->GetLastCommittedURL();
    if (main_url.SchemeIsCryptographic() &&
        resource_load_info.final_url.SchemeIs(url::kHttpScheme)) {
      attribution_ctx_.has_mixed_content_blocked = true;
      is_blocked = true;
      LOG(DEBUG) << "[BlankScreenMonitor] Mixed content blocked"
                << " error=" << net::ErrorToString(net_error);
    }
  }
  if (is_blocked &&
      attribution_ctx_.blocked_resource_messages.size() < kMaxCollectedMessages) {
    attribution_ctx_.blocked_resource_messages.push_back(
        resource_load_info.final_url.spec());
  }
  // CORS variant: some browsers report ERR_FAILED instead of
  // ERR_BLOCKED_BY_RESPONSE.
  if (net_error == net::ERR_FAILED && !is_blocked) {
    const GURL& main_url = web_contents()->GetLastCommittedURL();
    url::Origin main_origin = url::Origin::Create(main_url);
    url::Origin resource_origin =
        url::Origin::Create(resource_load_info.final_url);
    if (!main_origin.IsSameOriginWith(resource_origin)) {
      attribution_ctx_.has_cross_origin_failed = true;
      is_blocked = true;
      LOG(DEBUG) << "[BlankScreenMonitor] Cross-origin resource failed (CORS variant)";
      if (attribution_ctx_.blocked_resource_messages.size() <
          kMaxCollectedMessages) {
        attribution_ctx_.blocked_resource_messages.push_back(
            resource_load_info.final_url.spec());
      }
    }
  }
}

void BlankScreenMonitor::TrackFailedResource(
    const blink::mojom::ResourceLoadInfo& resource_load_info,
    int net_error,
    bool is_script,
    bool is_blocked) {
  bool load_failed = (net_error != net::OK && net_error != net::ERR_ABORTED) ||
                     resource_load_info.http_status_code >= 400;
  if (!load_failed)
    return;

  if (is_script) {
    attribution_ctx_.failed_js_count++;
    if (attribution_ctx_.failed_js_urls.size() < kMaxCollectedMessages) {
      attribution_ctx_.failed_js_urls.push_back(
          resource_load_info.final_url.spec());
    }
    LOG(DEBUG) << "[BlankScreenMonitor] JS load failed"
              << " error=" << net::ErrorToString(net_error)
              << " http_status=" << resource_load_info.http_status_code;
  } else {
    attribution_ctx_.failed_css_count++;
    if (attribution_ctx_.failed_css_urls.size() < kMaxCollectedMessages) {
      attribution_ctx_.failed_css_urls.push_back(
          resource_load_info.final_url.spec());
    }
    LOG(DEBUG) << "[BlankScreenMonitor] CSS load failed"
              << " error=" << net::ErrorToString(net_error)
              << " http_status=" << resource_load_info.http_status_code;
  }
}

void BlankScreenMonitor::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  if (state_ == State::kFinished || state_ == State::kIdle) {
    return;
  }
  if (is_error_navigation_) {
    return;
  }
  if (render_frame_host != web_contents()->GetPrimaryMainFrame()) {
    return;
  }

  const auto& dest = resource_load_info.request_destination;
  bool is_script = (dest == network::mojom::RequestDestination::kScript);
  bool is_style = (dest == network::mojom::RequestDestination::kStyle);
  if (!is_script && !is_style) {
    return;
  }

  int net_error = resource_load_info.net_error;

  bool is_blocked = false;
  DetectBlockedResource(resource_load_info, net_error, is_blocked);
  TrackFailedResource(resource_load_info, net_error, is_script, is_blocked);

  // Store resource record (FIFO, max kMaxResourceEntries).
  if (resource_entries_.size() >= kMaxResourceEntries) {
    resource_entries_.erase(resource_entries_.begin());
  }
  ResourceLoadEntry entry;
  entry.url = resource_load_info.final_url;
  entry.net_error = net_error;
  entry.http_status_code = resource_load_info.http_status_code;
  resource_entries_.push_back(std::move(entry));
}

void BlankScreenMonitor::OnDidAddMessageToConsole(
    content::RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  if (state_ == State::kFinished)
    return;
  if (state_ == State::kIdle)
    return;
  if (is_error_navigation_)
    return;
  if (source_frame != web_contents()->GetPrimaryMainFrame())
    return;
  if (log_level != blink::mojom::ConsoleMessageLevel::kError)
    return;

  std::string msg_utf8 = base::UTF16ToUTF8(message);

  // When CSP is active, console errors are likely CSP violations, not JS runtime errors.
  if (attribution_ctx_.has_csp_blocked || attribution_ctx_.has_csp_header) {
    attribution_ctx_.csp_violation_count++;
    LOG(DEBUG) << "[BlankScreenMonitor] Console error counted as CSP violation: "
              << msg_utf8 << " csp_count="
              << attribution_ctx_.csp_violation_count;
    return;
  }

  // Deduplicate console messages.
  if (seen_console_messages_.count(msg_utf8))
    return;
  seen_console_messages_.insert(msg_utf8);

  // Path C: console error, weight 0.5.
  attribution_ctx_.js_exception_count_path_c++;
  attribution_ctx_.js_exception_weighted_score += 0.5;

  if (attribution_ctx_.js_exception_messages.size() < kMaxCollectedMessages) {
    std::string truncated = msg_utf8.length() > kMaxMessageLength
        ? msg_utf8.substr(0, kMaxMessageLength) : msg_utf8;
    attribution_ctx_.js_exception_messages.push_back(truncated);
  }

  LOG(DEBUG) << "[BlankScreenMonitor] Console error (path C): " << msg_utf8
            << " source=" << base::UTF16ToUTF8(source_id)
            << " line=" << line_no
            << " score=" << attribution_ctx_.js_exception_weighted_score;
}

void BlankScreenMonitor::OnJsExceptionReceived(int source,
                                                    const std::string& message,
                                                    const std::string& url,
                                                    int line) {
  if (state_ == State::kFinished)
    return;
  if (state_ == State::kIdle)
    return;
  if (is_error_navigation_)
    return;

  attribution_ctx_.js_exception_count_path_a++;
  attribution_ctx_.js_exception_weighted_score += 1.0;

  if (attribution_ctx_.js_exception_messages.size() < kMaxCollectedMessages) {
    std::string truncated = message.length() > kMaxMessageLength
        ? message.substr(0, kMaxMessageLength) : message;
    attribution_ctx_.js_exception_messages.push_back(truncated);
  }

  LOG(DEBUG) << "[BlankScreenMonitor] JS exception received (path A): " << message
            << " line=" << line
            << " score=" << attribution_ctx_.js_exception_weighted_score;
}

void BlankScreenMonitor::OnFirstContentfulPaintInPrimaryMainFrame() {
  if (state_ == State::kFinished)
    return;
  LOG(INFO) << "[BlankScreenMonitor] FCP received, content confirmed, detection stopped";
  fcp_received_ = true;
  state_ = State::kFinished;
  load_watchdog_timer_.Stop();
  grace_timer_.Stop();
  recheck_timer_.Stop();
}

void BlankScreenMonitor::OnGpuProcessEvent(bool is_watchdog,
                                                 int exit_code,
                                                 const std::string& reason) {
  // No active navigation — nothing to attribute.
  if (state_ == State::kIdle)
    return;

  LOG(ERROR) << "[BlankScreenMonitor] GPU process "
             << (is_watchdog ? "watchdog timeout" : "crashed")
             << " exit_code=" << exit_code
             << " reason=" << reason
             << ", direct report";

  // GPU crash is a definitive root cause — report immediately.
  if (is_watchdog) {
    gpu_watchdog_ = true;
  } else {
    gpu_crashed_ = true;
  }
  gpu_exit_code_ = exit_code;
  gpu_crash_reason_ = reason;
  attribution_ctx_.url = committed_url_;

  RootCauseReport r;
  Report(std::move(r));
}

void BlankScreenMonitor::ClearGpuEvidence() {
  if (gpu_crashed_ || gpu_watchdog_) {
    LOG(DEBUG) << "[BlankScreenMonitor] Clearing GPU crash evidence";
  }
  gpu_crashed_ = false;
  gpu_watchdog_ = false;
  gpu_exit_code_ = 0;
  gpu_crash_reason_.clear();
}

void BlankScreenMonitor::Report(RootCauseReport report) {
  state_ = State::kFinished;

  load_watchdog_timer_.Stop();
  grace_timer_.Stop();
  recheck_timer_.Stop();

  // Skip all reporting when WebView is not visible (background).
  if (!page_visible_) {
    LOG(DEBUG) << "[BlankScreenMonitor] WebView not visible, skip report: "
              << report.ToString();
    ClearGpuEvidence();
    attribution_ctx_.Reset();
    return;
  }

  // Fill container state and frame analysis into attribution context.
  attribution_ctx_.page_visible = page_visible_;
  attribution_ctx_.total_quads = last_stats_.total_quads;
  attribution_ctx_.content_quads = last_stats_.content_quads;
  attribution_ctx_.white_area_ratio = last_stats_.white_area_ratio;
  attribution_ctx_.content_area_ratio = last_stats_.content_area_ratio;
  attribution_ctx_.has_text_like_content = last_stats_.has_text_like_content;

  // Apply GPU process state (persists across navigations).
  attribution_ctx_.gpu_process_crashed = gpu_crashed_;
  attribution_ctx_.gpu_watchdog_timeout = gpu_watchdog_;
  attribution_ctx_.gpu_exit_code = gpu_exit_code_;
  attribution_ctx_.gpu_crash_reason = gpu_crash_reason_;

  report = AttributionEngine::Attribute(attribution_ctx_);

  int32_t reason_code = static_cast<int32_t>(report.reason);
  OHOS::NWeb::OhosAdapterHelper::GetInstance()
      .GetHiSysEventAdapterInstance()
      .Write(kBlankScreenEvent,
             OHOS::NWeb::HiSysEventAdapter::EventType::FAULT,
             std::make_tuple(reason_code, 0));

  // Clear all consumed evidence.
  ClearGpuEvidence();
  attribution_ctx_.Reset();

  LOG(WARNING) << "[BlankScreenMonitor] Report: " << report.ToString();

  // Forward to RenderWidgetHostViewBase for NWeb callback propagation.
  // HiSysEvent is always reported (above). The app callback is gated by
  // callback_enabled_, which is controlled by PutWebBlankScreenMonitorEnabled.
  if (!callback_enabled_) {
    LOG(DEBUG) << "[BlankScreenMonitor] Callback disabled, skip OnMonitoredBlankScreen";
    return;
  }
  auto* rfh = web_contents()->GetPrimaryMainFrame();
  if (rfh) {
    auto* rwh = rfh->GetRenderWidgetHost();
    if (!rwh) {
      return;
    }
    auto* view = static_cast<content::RenderWidgetHostViewBase*>(
        rwh->GetView());
    if (view) {
      view->OnMonitoredBlankScreen(
          report.url.spec(),
          static_cast<int32_t>(report.reason),
          report.ToJson());
    }
  }
}

}  // namespace blank_screen
