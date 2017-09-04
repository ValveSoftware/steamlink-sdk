// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/renderer_settings.h"
#include "cc/output/software_renderer.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/buffer_types.h"

#if defined(ENABLE_VULKAN)
#include "cc/output/vulkan_renderer.h"
#endif

namespace cc {

Display::Display(SharedBitmapManager* bitmap_manager,
                 gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                 const RendererSettings& settings,
                 const FrameSinkId& frame_sink_id,
                 std::unique_ptr<BeginFrameSource> begin_frame_source,
                 std::unique_ptr<OutputSurface> output_surface,
                 std::unique_ptr<DisplayScheduler> scheduler,
                 std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter)
    : bitmap_manager_(bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      settings_(settings),
      frame_sink_id_(frame_sink_id),
      begin_frame_source_(std::move(begin_frame_source)),
      output_surface_(std::move(output_surface)),
      scheduler_(std::move(scheduler)),
      texture_mailbox_deleter_(std::move(texture_mailbox_deleter)) {
  DCHECK(output_surface_);
  DCHECK_EQ(!scheduler_, !begin_frame_source_);
  DCHECK(frame_sink_id_.is_valid());
  if (scheduler_)
    scheduler_->SetClient(this);
}

Display::~Display() {
  // Only do this if Initialize() happened.
  if (client_) {
    if (auto* context = output_surface_->context_provider())
      context->SetLostContextCallback(base::Closure());
    if (begin_frame_source_)
      surface_manager_->UnregisterBeginFrameSource(begin_frame_source_.get());
    surface_manager_->RemoveObserver(this);
  }
  if (aggregator_) {
    for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
      Surface* surface = surface_manager_->GetSurfaceForId(id_entry.first);
      if (surface)
        surface->RunDrawCallbacks();
    }
  }
}

void Display::Initialize(DisplayClient* client,
                         SurfaceManager* surface_manager) {
  DCHECK(client);
  DCHECK(surface_manager);
  client_ = client;
  surface_manager_ = surface_manager;

  surface_manager_->AddObserver(this);

  // This must be done in Initialize() so that the caller can delay this until
  // they are ready to receive a BeginFrameSource.
  if (begin_frame_source_) {
    surface_manager_->RegisterBeginFrameSource(begin_frame_source_.get(),
                                               frame_sink_id_);
  }

  output_surface_->BindToClient(this);
  InitializeRenderer();

  if (auto* context = output_surface_->context_provider()) {
    // This depends on assumptions that Display::Initialize will happen
    // on the same callstack as the ContextProvider being created/initialized
    // or else it could miss a callback before setting this.
    context->SetLostContextCallback(base::Bind(
        &Display::DidLoseContextProvider,
        // Unretained is safe since the callback is unset in this class'
        // destructor and is never posted.
        base::Unretained(this)));
  }
}

void Display::SetLocalFrameId(const LocalFrameId& id,
                              float device_scale_factor) {
  if (current_surface_id_.local_frame_id() == id &&
      device_scale_factor_ == device_scale_factor) {
    return;
  }

  TRACE_EVENT0("cc", "Display::SetSurfaceId");
  current_surface_id_ = SurfaceId(frame_sink_id_, id);
  device_scale_factor_ = device_scale_factor;

  UpdateRootSurfaceResourcesLocked();
  if (scheduler_)
    scheduler_->SetNewRootSurface(current_surface_id_);
}

void Display::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "Display::SetVisible", "visible", visible);
  if (renderer_)
    renderer_->SetVisible(visible);
  if (scheduler_)
    scheduler_->SetVisible(visible);
  visible_ = visible;

  if (!visible) {
    // Damage tracker needs a full reset as renderer resources are dropped when
    // not visible.
    if (aggregator_ && current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }
}

void Display::Resize(const gfx::Size& size) {
  if (size == current_surface_size_)
    return;

  TRACE_EVENT0("cc", "Display::Resize");

  // Need to ensure all pending swaps have executed before the window is
  // resized, or D3D11 will scale the swap output.
  if (settings_.finish_rendering_on_resize) {
    if (!swapped_since_resize_ && scheduler_)
      scheduler_->ForceImmediateSwapIfPossible();
    if (swapped_since_resize_ && output_surface_ &&
        output_surface_->context_provider())
      output_surface_->context_provider()->ContextGL()->ShallowFinishCHROMIUM();
  }
  swapped_since_resize_ = false;
  current_surface_size_ = size;
  if (scheduler_)
    scheduler_->DisplayResized();
}

void Display::SetColorSpace(const gfx::ColorSpace& color_space) {
  device_color_space_ = color_space;
}

void Display::SetOutputIsSecure(bool secure) {
  if (secure == output_is_secure_)
    return;
  output_is_secure_ = secure;

  if (aggregator_) {
    aggregator_->set_output_is_secure(secure);
    // Force a redraw.
    if (current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }
}

void Display::InitializeRenderer() {
  // Not relevant for display compositor since it's not delegated.
  bool delegated_sync_points_required = false;
  resource_provider_.reset(new ResourceProvider(
      output_surface_->context_provider(), bitmap_manager_,
      gpu_memory_buffer_manager_, nullptr, settings_.highp_threshold_min,
      settings_.texture_id_allocation_chunk_size,
      delegated_sync_points_required, settings_.use_gpu_memory_buffer_resources,
      false, settings_.buffer_to_texture_target_map));

  if (output_surface_->context_provider()) {
    DCHECK(texture_mailbox_deleter_);
    renderer_ = base::MakeUnique<GLRenderer>(
        &settings_, output_surface_.get(), resource_provider_.get(),
        texture_mailbox_deleter_.get(), settings_.highp_threshold_min);
  } else if (output_surface_->vulkan_context_provider()) {
#if defined(ENABLE_VULKAN)
    DCHECK(texture_mailbox_deleter_);
    renderer_ = base::MakeUnique<VulkanRenderer>(
        &settings_, output_surface_.get(), resource_provider_.get(),
        texture_mailbox_deleter_.get(), settings_.highp_threshold_min);
#else
    NOTREACHED();
#endif
  } else {
    auto renderer = base::MakeUnique<SoftwareRenderer>(
        &settings_, output_surface_.get(), resource_provider_.get());
    software_renderer_ = renderer.get();
    renderer_ = std::move(renderer);
  }

  renderer_->Initialize();
  renderer_->SetVisible(visible_);

  // TODO(jbauman): Outputting an incomplete quad list doesn't work when using
  // overlays.
  bool output_partial_list = renderer_->use_partial_swap() &&
                             !output_surface_->GetOverlayCandidateValidator();
  aggregator_.reset(new SurfaceAggregator(
      surface_manager_, resource_provider_.get(), output_partial_list));
  aggregator_->set_output_is_secure(output_is_secure_);
}

void Display::UpdateRootSurfaceResourcesLocked() {
  Surface* surface = surface_manager_->GetSurfaceForId(current_surface_id_);
  bool root_surface_resources_locked =
      !surface || !surface->GetEligibleFrame().delegated_frame_data;
  if (scheduler_)
    scheduler_->SetRootSurfaceResourcesLocked(root_surface_resources_locked);
}

void Display::DidLoseContextProvider() {
  if (scheduler_)
    scheduler_->OutputSurfaceLost();
  // WARNING: The client may delete the Display in this method call. Do not
  // make any additional references to members after this call.
  client_->DisplayOutputSurfaceLost();
}

bool Display::DrawAndSwap() {
  TRACE_EVENT0("cc", "Display::DrawAndSwap");

  if (!current_surface_id_.is_valid()) {
    TRACE_EVENT_INSTANT0("cc", "No root surface.", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (!output_surface_) {
    TRACE_EVENT_INSTANT0("cc", "No output surface", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  CompositorFrame frame = aggregator_->Aggregate(current_surface_id_);
  if (!frame.delegated_frame_data) {
    TRACE_EVENT_INSTANT0("cc", "Empty aggregated frame.",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // Run callbacks early to allow pipelining.
  for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
    Surface* surface = surface_manager_->GetSurfaceForId(id_entry.first);
    if (surface)
      surface->RunDrawCallbacks();
  }

  DelegatedFrameData* frame_data = frame.delegated_frame_data.get();

  frame.metadata.latency_info.insert(frame.metadata.latency_info.end(),
                                     stored_latency_info_.begin(),
                                     stored_latency_info_.end());
  stored_latency_info_.clear();
  bool have_copy_requests = false;
  for (const auto& pass : frame_data->render_pass_list) {
    have_copy_requests |= !pass->copy_requests.empty();
  }

  gfx::Size surface_size;
  bool have_damage = false;
  if (!frame_data->render_pass_list.empty()) {
    RenderPass& last_render_pass = *frame_data->render_pass_list.back();
    if (last_render_pass.output_rect.size() != current_surface_size_ &&
        last_render_pass.damage_rect == last_render_pass.output_rect &&
        !current_surface_size_.IsEmpty()) {
      // Resize the output rect to the current surface size so that we won't
      // skip the draw and so that the GL swap won't stretch the output.
      last_render_pass.output_rect.set_size(current_surface_size_);
      last_render_pass.damage_rect = last_render_pass.output_rect;
    }
    surface_size = last_render_pass.output_rect.size();
    have_damage = !last_render_pass.damage_rect.size().IsEmpty();
  }

  bool size_matches = surface_size == current_surface_size_;
  if (!size_matches)
    TRACE_EVENT_INSTANT0("cc", "Size mismatch.", TRACE_EVENT_SCOPE_THREAD);

  bool should_draw = have_copy_requests || (have_damage && size_matches);

  // If the surface is suspended then the resources to be used by the draw are
  // likely destroyed.
  if (output_surface_->SurfaceIsSuspendForRecycle()) {
    TRACE_EVENT_INSTANT0("cc", "Surface is suspended for recycle.",
                         TRACE_EVENT_SCOPE_THREAD);
    should_draw = false;
  }

  client_->DisplayWillDrawAndSwap(should_draw, frame_data->render_pass_list);

  if (should_draw) {
    bool disable_image_filtering =
        frame.metadata.is_resourceless_software_draw_with_scroll_or_animation;
    if (software_renderer_) {
      software_renderer_->SetDisablePictureQuadImageFiltering(
          disable_image_filtering);
    } else {
      // This should only be set for software draws in synchronous compositor.
      DCHECK(!disable_image_filtering);
    }

    renderer_->DecideRenderPassAllocationsForFrame(
        frame_data->render_pass_list);
    renderer_->DrawFrame(&frame_data->render_pass_list, device_scale_factor_,
                         device_color_space_, current_surface_size_);
  } else {
    TRACE_EVENT_INSTANT0("cc", "Draw skipped.", TRACE_EVENT_SCOPE_THREAD);
  }

  bool should_swap = should_draw && size_matches;
  if (should_swap) {
    swapped_since_resize_ = true;
    for (auto& latency : frame.metadata.latency_info) {
      TRACE_EVENT_WITH_FLOW1(
          "input,benchmark", "LatencyInfo.Flow",
          TRACE_ID_DONT_MANGLE(latency.trace_id()),
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
          "Display::DrawAndSwap");
    }
    benchmark_instrumentation::IssueDisplayRenderingStatsEvent();
    renderer_->SwapBuffers(std::move(frame.metadata.latency_info));
    if (scheduler_)
      scheduler_->DidSwapBuffers();
  } else {
    if (have_damage && !size_matches)
      aggregator_->SetFullDamageForSurface(current_surface_id_);
    TRACE_EVENT_INSTANT0("cc", "Swap skipped.", TRACE_EVENT_SCOPE_THREAD);
    stored_latency_info_.insert(stored_latency_info_.end(),
                                frame.metadata.latency_info.begin(),
                                frame.metadata.latency_info.end());
    if (scheduler_) {
      scheduler_->DidSwapBuffers();
      scheduler_->DidReceiveSwapBuffersAck();
    }
  }

  client_->DisplayDidDrawAndSwap();
  return true;
}

void Display::DidReceiveSwapBuffersAck() {
  if (scheduler_)
    scheduler_->DidReceiveSwapBuffersAck();
  if (renderer_)
    renderer_->SwapBuffersComplete();
}

void Display::DidReceiveTextureInUseResponses(
    const gpu::TextureInUseResponses& responses) {
  if (renderer_)
    renderer_->DidReceiveTextureInUseResponses(responses);
}

void Display::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  aggregator_->SetFullDamageForSurface(current_surface_id_);
  if (scheduler_)
    scheduler_->SurfaceDamaged(current_surface_id_);
}

void Display::OnSurfaceDamaged(const SurfaceId& surface_id, bool* changed) {
  if (aggregator_ &&
      aggregator_->previous_contained_surfaces().count(surface_id)) {
    Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
    if (surface) {
      const CompositorFrame& current_frame = surface->GetEligibleFrame();
      if (!current_frame.delegated_frame_data ||
          current_frame.delegated_frame_data->resource_list.empty()) {
        aggregator_->ReleaseResources(surface_id);
      }
    }
    if (scheduler_)
      scheduler_->SurfaceDamaged(surface_id);
    *changed = true;
  } else if (surface_id == current_surface_id_) {
    if (scheduler_)
      scheduler_->SurfaceDamaged(surface_id);
    *changed = true;
  }

  if (surface_id == current_surface_id_)
    UpdateRootSurfaceResourcesLocked();
}

void Display::OnSurfaceCreated(const SurfaceId& surface_id,
                               const gfx::Size& frame,
                               float device_scale_factor) {}

const SurfaceId& Display::CurrentSurfaceId() {
  return current_surface_id_;
}

void Display::ForceImmediateDrawAndSwapIfPossible() {
  if (scheduler_)
    scheduler_->ForceImmediateSwapIfPossible();
}

}  // namespace cc
