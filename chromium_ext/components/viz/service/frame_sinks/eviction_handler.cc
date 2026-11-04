// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/eviction_handler.h"
#if BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)
#include <utility>
#include "base/functional/callback_helpers.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_bindings.h"
#include "components/viz/common/viz_utils.h"

namespace viz {

EvictionHandler::EvictionHandler(Display* display,
                                 CompositorFrameSinkSupport* support,
                                 ReservedResourceIdTracker* id_tracker)
    : display_(display), support_(support), id_tracker_(id_tracker) {
  support_->SetExternalReservedResourceDelegate(this);
}

EvictionHandler::~EvictionHandler() {
  support_->SetExternalReservedResourceDelegate(nullptr);
}

bool EvictionHandler::WillEvictSurface(const SurfaceId& surface_id) {
  const SurfaceId& current_surface_id = display_->CurrentSurfaceId();
  if (!current_surface_id.is_valid()) {
    return true;  // Okay to evict immediately.
  }
  DCHECK_EQ(surface_id.frame_sink_id(), current_surface_id.frame_sink_id());
  // We should only evict when the display is not visible. If the display is
  // visible, then we likely received two evictions for the root surface at the
  // same time, which should not happen.
  if (display_->visible()) {
    LOG(WARNING) << "Expected display to be not visible - in_progress_: " << in_progress_;
    MaybeFinishEvictionProcess();
    return false;
  }
  DCHECK(display_->has_scheduler());

  // This matches CompositorFrameSinkSupport's eviction logic, which will
  // evict `surface_id` or matching but older ones. Avoid overwriting the
  // contents of `current_surface_id` if it's newer here by doing the same
  // check.
  if (surface_id.local_surface_id().parent_sequence_number() >=
      current_surface_id.local_surface_id().parent_sequence_number()) {

    in_progress_ = true;
    SubmitPlaceholderContentForEviction(surface_id);
    // Don't evict immediately.
    // Delay eviction until the next draw to make sure that the draw is
    // successful (requires the surface not to be evicted). We need the draw (of
    // an empty CF) to be successful to push out and free resources.
    return false;
  }
  return true;  // Okay to evict immediately.
}

void EvictionHandler::MaybeFinishEvictionProcess() {
  // We don't modify `copy_output_results_` here, because they may still be in
  // use. They will be freed when no longer used, so just prevent eviction for
  // the root surface.
  to_evict_on_next_draw_and_swap_ = LocalSurfaceId();
  in_progress_ = false;
}

void EvictionHandler::DisplayDidDrawAndSwap() {
  if (!in_progress_) {
    return;
  }

  if (to_evict_on_next_draw_and_swap_.is_valid()) {
    display_->SetVisible(false);
    display_->InvalidateCurrentSurfaceId();

    support_->EvictSurface(to_evict_on_next_draw_and_swap_);

    // Trigger garbage collection immediately, otherwise the surface may not be
    // evicted for a long time (e.g. not before a frame is produced).
    support_->GarbageCollectSurfaces();

    MaybeFinishEvictionProcess();
    // We will get two unref calls and destroy `copy_output_results_` in
    // `UnrefResources`.
  }

  if (IsEvictUnlockFrameEnabled()) {
    to_evict_on_next_draw_and_swap_ = LocalSurfaceId();
  }
}

void EvictionHandler::SubmitPlaceholderContentForEviction(SurfaceId to_evict) {
  if (!in_progress_) {
    return;
  }
  // Push replacement compositor frame to root surface. This is so the resources
  // can be unreffed from both viz and the OS compositor (if required).
  CompositorFrame frame;

  auto& metadata = frame.metadata;
  metadata.frame_token = kLocalFrameToken;

  auto current_surface_id = display_->CurrentSurfaceId();
  auto* surface =
      support_->frame_sink_manager()->surface_manager()->GetSurfaceForId(
          current_surface_id);
  if (!surface) {
    LOG(WARNING) << "Current surface is nullptr no need to evict frame.";
    MaybeFinishEvictionProcess();
    return;
  }

  metadata.device_scale_factor = surface->device_scale_factor();
  frame.metadata.begin_frame_ack = BeginFrameAck::CreateManualAckWithDamage();

  frame.render_pass_list.push_back(CompositorRenderPass::Create());
  auto* render_pass = frame.render_pass_list.back().get();

  const CompositorRenderPassId kRenderPassId{1};
  auto surface_rect = gfx::Rect(surface->size_in_pixels());
  DCHECK(!surface_rect.IsEmpty());
  render_pass->SetNew(kRenderPassId, /*output_rect=*/
                      surface_rect,
                      /*damage_rect=*/surface_rect, gfx::Transform());

  SharedQuadState* quad_state = render_pass->CreateAndAppendSharedQuadState();

  quad_state->SetAll(gfx::Transform(), /*layer_rect=*/surface_rect,
                     /*visible_layer_rect=*/surface_rect,
                     /*filter_info=*/gfx::MaskFilterInfo(),
                     /*clip=*/std::nullopt,
                     /*contents_opaque=*/true, /*opacity_f=*/1.f,
                     /*blend=*/SkBlendMode::kSrcOver, /*sorting_context=*/0,
                     /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  // Draw a solid white quad to fill the frame
  SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(quad_state, surface_rect, surface_rect, SkColors::kWhite,
                      /*anti_aliasing_off=*/false);

  support_->SubmitCompositorFrameLocally(current_surface_id, std::move(frame),
                                         display_->settings());

  // Complete the eviction on next draw and swap.
  to_evict_on_next_draw_and_swap_ = to_evict.local_surface_id();
  display_->SetVisible(true);
  display_->ForceImmediateDrawAndSwapIfPossible();
}

void EvictionHandler::ReceiveFromChild(
    const std::vector<TransferableResource>& resources) {
  for (const auto& resource : resources) {
    if (copy_output_results_.contains(resource.id)) {
      id_tracker_->RefId(resource.id, /*count=*/1);
    }
  }
}

void EvictionHandler::RefResources(
    const std::vector<TransferableResource>& resources) {
  for (const auto& resource : resources) {
    if (copy_output_results_.contains(resource.id)) {
      id_tracker_->RefId(resource.id, /*count=*/1);
    }
  }
}

void EvictionHandler::UnrefResources(
    const std::vector<ReturnedResource>& resources) {
  for (const auto& resource : resources) {
    if (copy_output_results_.contains(resource.id)) {
      // There are no further references, destroy the `CopyOutputRequest`s which
      // will call their release callbacks.
      if (id_tracker_->UnrefId(resource.id, /*count=*/1)) {
        copy_output_results_.erase(resource.id);
      }
    }
  }
}

LocalSurfaceId EvictionHandler::GetNextEvictLocalSurfaceId() const {
  return to_evict_on_next_draw_and_swap_;
}
 
void EvictionHandler::SetNextEvictLocalSurfaceId(LocalSurfaceId surfaceId) {
  to_evict_on_next_draw_and_swap_ = surfaceId;
}
}  // namespace viz
#endif // ARKWEB_EVICT_UNLOCK_FRAMES
