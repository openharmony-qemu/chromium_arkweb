/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "arkweb/chromium_ext/content/renderer/ark_web_render_frame_impl.h"
#include "content/child/child_process.h"

#include "arkweb/build/features/features.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#endif

#if BUILDFLAG(ARKWEB_LOGGER_REPORT)
#include "base/base_switches.h"
#include "base/command_line.h"
#include "url/ohos/log_utils.h"
#endif

using blink::WebDocumentLoader;

namespace content {

std::optional<blink::WebString> ArkWebUserAgentOverride(
    content::RenderFrame* render_frame) {
  if (render_frame &&
      render_frame->GetWebView()->MainFrame()->IsWebRemoteFrame()) {
    return blink::WebString::FromUTF8(
        render_frame->GetWebView()
            ->GetRendererPreferences()
            .user_agent_override.ua_string_override);
  }
  return std::nullopt;
}

#if BUILDFLAG(ARKWEB_JSPROXY)
void RenderFrameImpl::RunScriptsAtHeadReady() {
  if (!initialized_) {
    return;
  }

  if (GetContentClient() && GetContentClient()->renderer()) {
    GetContentClient()->renderer()->RunScriptsAtHeadReady(this);
  }
}
#endif

#if BUILDFLAG(ARKWEB_ADBLOCK)
void RenderFrameImpl::DidSubresourceFiltered() {
  TRACE_EVENT1("navigation,benchmark,rail",
               "RenderFrameImpl::DidSubresourceFiltered", "frame_token",
               frame_token_);
  for (auto& observer : observers_) {
    observer.DidSubresourceFiltered();
  }
}

bool RenderFrameImpl::GetGlobalAdblockEnabled() {
  return GetRendererPreferences().is_global_adblock_enabled;
}

void RenderFrameImpl::OnUpdateAdBlockEnabledToRender(
    bool site_adblock_enabled) {
  LOG(DEBUG) << "[Adblock] render frame OnUpdateAdBlockEnabledToRender: "
            << site_adblock_enabled;
  // 1) Persist the adblock site switch into the LocalFrameRoot via the WebLocalFrame
  //    layer. In strict site isolation mode the subframe lives in a separate
  //    process, so each frame must apply its own copy of the state.
  if (GetWebFrame()) {
    GetWebFrame()->UpdateAdBlockEnabledToRender(site_adblock_enabled);
  }

  // 2) Refresh the subresource filter activation state for the already-loaded
  //    document so that in-flight subresource requests honor the new switch
  //    without requiring a reload.
  if (!frame_) {
    return;
  }
  blink::WebDocumentLoader* document_loader = frame_->GetDocumentLoader();
  if (!document_loader) {
    return;
  }
  blink::WebDocumentSubresourceFilter* filter =
      document_loader->GetWebSubresourceFilter();

  blink::WebDocumentSubresourceFilter* user_filter =
      document_loader->GetWebUserSubresourceFilter();
  if (filter) {
    filter->set_activation_state(site_adblock_enabled);
    return;
  }

  if (user_filter) {
    user_filter->set_activation_state(site_adblock_enabled);
  }
}
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
void RenderFrameImpl::NotifyLcpForBlankless() {
  if (blankless_key_ == base::ohos::BlanklessController::INVALID_BLANKLESS_KEY) {
    return;
  }
  if (RenderThreadImpl* render_thread = RenderThreadImpl::current()) {
    const blink::WebPerformanceMetricsForReporting& perf = GetWebFrame()->PerformanceMetricsForReporting();
    double start = perf.NavigationStart();
    blink::LargestContentfulPaintDetailsForReporting lcp_details = perf.LargestContentfulDetailsForMetrics();
    double ms = (std::max(lcp_details.image_paint_time, lcp_details.text_paint_time) - start) * 1000;
    int32_t lcp_time = ms > INT32_MAX ? INT32_MAX : static_cast<int32_t>(ms);
    render_thread->SetBlanklessDumpInfo(nweb_id_, blankless_key_, frame_sink_id_, lcp_time, pref_hash_);
  }
}

void RenderFrameImpl::SendBlanklessKeyToRenderFrame(uint32_t nweb_id,
                                                    uint64_t blankless_key,
                                                    uint64_t frame_sink_id,
                                                    int64_t pref_hash)
{
  nweb_id_ = nweb_id;
  blankless_key_ = blankless_key;
  frame_sink_id_ = frame_sink_id;
  pref_hash_ = pref_hash;
  if (blankless_key == base::ohos::BlanklessController::INVALID_BLANKLESS_KEY) {
    return;
  }
  // 暂时简化实现，因为 Chromium 141 中无法访问 blink 内部 API
// TODO: 需要使用公共 API 重新实现此功能
LOG(DEBUG) << "blankless optimization: functionality temporarily disabled pending API migration"
           << ", nweb_id:" << nweb_id << ", key:" << blankless_key
           << ", frame_sink_id:" << frame_sink_id << ", pref_hash:" << pref_hash;
}
#endif

RenderFrameImplUtils::RenderFrameImplUtils(RenderFrameImpl* impl) {
  this->renderFrameImpl = impl;
}
#if BUILDFLAG(ARKWEB_DFX_TRACING)
int64_t RenderFrameImplUtils::GetCurrentTimestampMS() {
  auto currentTime = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(currentTime)
              .count() /
          kMicrosecondsPerMillisecond;
}

void RenderFrameImplUtils::ReportRenderInitBlock() {
  int64_t initialize_time = GetCurrentTimestampMS();
  std::string mode = "ReportRenderInitBlock";
  if (is_complete_initialize) {
    is_complete_initialize = false;
#if !defined(COMPONENT_BUILD)
    int64_t block_time = initialize_time - commit_navigation_time_;
    if(ChildProcess::current()) {
      ChildProcess::current()->ReportHisyevent(block_time, mode);
    }
#endif
  }
}

void RenderFrameImplUtils::ChangeCommitNavigationTime(int64_t time) {
  commit_navigation_time_ = time;
}

void RenderFrameImplUtils::ChangeCompleteInitialize(bool complete) {
  is_complete_initialize = complete;
}
#if !defined(COMPONENT_BUILD)
void RenderFrameImpl::SendCommitNavigationTime(int64_t start_time) {
  implUtils->ChangeCommitNavigationTime(start_time);
  implUtils->ChangeCompleteInitialize(true);
  if (initialized_) {
    implUtils->ReportRenderInitBlock();
  }
}
#endif
#endif

#if BUILDFLAG(ARKWEB_LOGGER_REPORT)
bool RenderFrameImpl::IsStrictLogMode() {
  if (GetWebView()) {
    return GetWebView()->IsStrictLogMode();
  }
  return false;
}
#endif

#if BUILDFLAG(ARKWEB_LOGGER_REPORT) && !BUILDFLAG(ARKWEB_NWEB_EX)
void RenderFrameImpl::OnCommitNavigation(
    const GURL& url,
    bool is_client_redirect,
    blink::mojom::NavigationType navigation_type,
    const base::UnguessableToken& devtools_navigation_token,
    network::mojom::NavigationDeliveryType navigation_delivery_type) {}
void RenderFrameImpl::OnDidCommitNavigation(
    WebDocumentLoader* document_loader,
    blink::WebHistoryCommitType commit_type) {}
 
void RenderFrameImpl::OnDidDispatchDOMContentLoadedEvent() {}
 
void RenderFrameImpl::OnDidHandleOnloadEvents() {}
#endif  // BUILDFLAG(ARKWEB_LOGGER_REPORT) && !BUILDFLAG(ARKWEB_NWEB_EX)

}  // namespace content
