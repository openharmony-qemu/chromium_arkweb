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

#include "base/trace_event/trace_event.h"
#include "arkweb/chromium_ext/cc/mojo_embedder/async_layer_tree_frame_sink_utils.h"


namespace cc {
namespace mojo_embedder {

AsyncLayerTreeFrameSinkUtils::AsyncLayerTreeFrameSinkUtils(AsyncLayerTreeFrameSink* sink)
{
  this->asyncLayerTreeFrameSink = sink;
}

#if BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING)
void AsyncLayerTreeFrameSinkUtils::BindToClientArkWebPerSch(
    const raw_ptr<viz::mojom::CompositorFrameSink>& compositor_frame_sink_ptr_,
    int32_t currentRealPid_,
    int32_t getCurrentRealPid_)
{
  std::vector<int32_t> thread_ids;
  thread_ids.push_back(currentRealPid_);
  compositor_frame_sink_ptr_->ReportKeyThreadIds(
      thread_ids, getCurrentRealPid_, true);
}
#endif

#if BUILDFLAG(ARKWEB_SOFTWARE_COMPOSITOR)
void AsyncLayerTreeFrameSinkUtils::BindToClientArkWebSoftCom(
    LayerTreeFrameSinkClient* client,
    viz::BeginFrameSource* source)
{
  if (software_renderer_ohos_) {
    software_renderer_ohos_->BindToClient(client, source);
  }
}

void AsyncLayerTreeFrameSinkUtils::DetachFromClientArkWebSoftCom()
{
  if (software_renderer_ohos_) {
    software_renderer_ohos_->DetachFromClient();
  }
}

bool AsyncLayerTreeFrameSinkUtils::SubmitCompositorFrameArkWebSoftCom(viz::CompositorFrame& frame)
{
  if (software_renderer_ohos_ && software_renderer_ohos_->InSoftwareDraw()) {
    software_renderer_ohos_->DrawAndSwapOnRenderer(std::move(frame));
    return true;
  }
  return false;
}

void AsyncLayerTreeFrameSinkUtils::InitSoftComRenderArkWebSoftCom(SoftwareCompositorRegistryOhos* registry)
{
  software_renderer_ohos_ =
    std::make_unique<SoftwareCompositorRendererOhos>(this->asyncLayerTreeFrameSink, registry);
}
#endif

#if BUILDFLAG(ARKWEB_DFX_DUMP)
void AsyncLayerTreeFrameSinkUtils::SubmitCompositorFrameDfxDumpLog(viz::LocalSurfaceId local_surface_id_)
{
  if (is_first_submit_) {
    is_first_submit_ = false;
    LOG(INFO) << "web render log: first call SubmitCompositorFrame, "
                "local_surface_id = "
              << local_surface_id_.ToString();
  }
}
#endif

#if BUILDFLAG(ARKWEB_DFX_TRACING)
void AsyncLayerTreeFrameSinkUtils::SubmitCompositorFrameDfxDumpTrace(int64_t trace_id)
{
  OHOS_TRACE_EVENT2("viz,benchmark", "Graphics.Pipeline", "trace_id",
                    std::to_string(trace_id),
                    "step",
                    "SubmitCompositorFrame");
}

void AsyncLayerTreeFrameSinkUtils::OnBeginFrameArkWebDfxTrace(int64_t trace_id)
{
  OHOS_TRACE_EVENT2("viz,benchmark", "Graphics.Pipeline", "trace_id",
                    std::to_string(trace_id),
                    "step",
                    "ReceiveBeginFrame");
}
#endif

#if BUILDFLAG(ARKWEB_SWAP_BUFFER_TRACE)
void AsyncLayerTreeFrameSinkUtils::SubmitCompositorFrameArkWebSwapBuTr(
    int64_t trace_id,
    FrameSkippedReason reason)
{
  std::string trace_content = "step: DidNotProduceFrame, reason: " +
                            std::to_string(static_cast<int32_t>(reason));
  OHOS_TRACE_EVENT2("viz,benchmark", "Graphics.Pipeline", "trace_id",
                    std::to_string(trace_id), "trace_content",
                    trace_content);
}

void AsyncLayerTreeFrameSinkUtils::OnBeginFrameArkWebSwBuTr(int64_t trace_id)
{
  OHOS_TRACE_EVENT2("viz,benchmark", "Graphics.Pipeline", "trace_id",
                    std::to_string(trace_id), "step",
                    "ReceiveBeginFrameDiscard");
}
#endif

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)
void AsyncLayerTreeFrameSinkUtils::SetFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
}

void AsyncLayerTreeFrameSinkUtils::AnalyzeFrameForBlankScreen(
    const viz::CompositorFrame& frame) {
  // Need FrameSinkId to send frame analysis results to the correct Browser side
  if (!frame_sink_id_.is_valid()) {
    return;
  }

  auto& reporter = blank_screen::BlankScreenReporter::GetInstance();

  // Content already confirmed, skip analysis (until reset by new navigation)
  if (reporter.IsContentConfirmed(frame_sink_id_)) {
    return;
  }

  auto host = reporter.GetHost(frame_sink_id_);
  if (!host.is_bound()) {
    return;
  }

  // Rate limit: 500ms interval
  auto now = base::TimeTicks::Now();
  if (!last_blank_screen_analyze_time_.is_null() &&
      (now - last_blank_screen_analyze_time_) < kBlankScreenAnalyzeIntervalBlank) {
    return;
  }
  last_blank_screen_analyze_time_ = now;

  gfx::Size viewport_size = frame.size_in_pixels();
  auto stats = blank_screen::AnalyzeFrameOnVizThread(frame, viewport_size);

  LOG(DEBUG) << "[BlankScreenMonitor] Renderer frame analyzed: frame_sink_id="
            << frame_sink_id_.ToString()
            << " viewport=" << viewport_size.width() << "x" << viewport_size.height()
            << " total_quads=" << stats.total_quads
            << " content_quads=" << stats.content_quads
            << " white_ratio=" << stats.white_area_ratio
            << " content_ratio=" << stats.content_area_ratio
            << " has_text_like_content=" << stats.has_text_like_content;

  host->OnFrameAnalyzed(
      frame_sink_id_,
      stats.total_quads,
      stats.solid_color_quads,
      stats.content_quads,
      stats.white_area_ratio,
      stats.content_area_ratio,
      stats.has_text_like_content);

  // Track consecutive content frames to stop analysis once content is stable.
  // This mirrors the browser-side 3-frame confirmation and prevents the
  // renderer from analyzing every frame indefinitely.
  if (stats.content_quads > 0) {
    renderer_content_streak_++;
    if (renderer_content_streak_ >= kMinRendererContentStreak) {
      reporter.SetContentConfirmed(frame_sink_id_);
      LOG(DEBUG) << "[BlankScreenMonitor] Renderer content confirmed after "
                << renderer_content_streak_ << " consecutive content frames, "
                << "stopping frame analysis for " << frame_sink_id_.ToString();
    }
  } else {
    renderer_content_streak_ = 0;
  }
}
#endif

}  // namespace mojo_embedder
}  // namespace cc
