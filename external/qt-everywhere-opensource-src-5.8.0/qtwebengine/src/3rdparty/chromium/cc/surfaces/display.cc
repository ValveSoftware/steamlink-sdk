// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
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

Display::Display(SurfaceManager* manager,
                 SharedBitmapManager* bitmap_manager,
                 gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                 const RendererSettings& settings,
                 uint32_t compositor_surface_namespace,
                 std::unique_ptr<BeginFrameSource> begin_frame_source,
                 std::unique_ptr<OutputSurface> output_surface,
                 std::unique_ptr<DisplayScheduler> scheduler,
                 std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter)
    : surface_manager_(manager),
      bitmap_manager_(bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      settings_(settings),
      compositor_surface_namespace_(compositor_surface_namespace),
      begin_frame_source_(std::move(begin_frame_source)),
      output_surface_(std::move(output_surface)),
      scheduler_(std::move(scheduler)),
      texture_mailbox_deleter_(std::move(texture_mailbox_deleter)) {
  DCHECK(surface_manager_);
  DCHECK(output_surface_);
  DCHECK(texture_mailbox_deleter_);
  DCHECK_EQ(!scheduler_, !begin_frame_source_);

  surface_manager_->AddObserver(this);

  if (scheduler_)
    scheduler_->SetClient(this);
}

Display::~Display() {
  // Only do this if Initialize() happened.
  if (begin_frame_source_ && client_)
    surface_manager_->UnregisterBeginFrameSource(begin_frame_source_.get());

  surface_manager_->RemoveObserver(this);
  if (aggregator_) {
    for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
      Surface* surface = surface_manager_->GetSurfaceForId(id_entry.first);
      if (surface)
        surface->RunDrawCallbacks(SurfaceDrawStatus::DRAW_SKIPPED);
    }
  }
}

void Display::Initialize(DisplayClient* client) {
  DCHECK(client);
  client_ = client;

  // This must be done in Initialize() so that the caller can delay this until
  // they are ready to receive a BeginFrameSource.
  if (begin_frame_source_) {
    surface_manager_->RegisterBeginFrameSource(begin_frame_source_.get(),
                                               compositor_surface_namespace_);
  }

  bool ok = output_surface_->BindToClient(this);
  // The context given to the Display's OutputSurface should already be
  // initialized, so Bind can not fail.
  DCHECK(ok);
}

void Display::SetSurfaceId(SurfaceId id, float device_scale_factor) {
  DCHECK_EQ(id.id_namespace(), compositor_surface_namespace_);
  if (current_surface_id_ == id && device_scale_factor_ == device_scale_factor)
    return;

  TRACE_EVENT0("cc", "Display::SetSurfaceId");
  current_surface_id_ = id;
  device_scale_factor_ = device_scale_factor;

  UpdateRootSurfaceResourcesLocked();
  if (scheduler_)
    scheduler_->SetNewRootSurface(id);
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

void Display::SetExternalClip(const gfx::Rect& clip) {
  external_clip_ = clip;
}

void Display::SetOutputIsSecure(bool secure) {
  if (secure == output_is_secure_)
    return;
  output_is_secure_ = secure;

  if (aggregator_) {
    aggregator_->set_output_is_secure(secure);
    // Force a redraw.
    if (!current_surface_id_.is_null())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }
}

void Display::InitializeRenderer() {
  if (resource_provider_)
    return;

  std::unique_ptr<ResourceProvider> resource_provider(new ResourceProvider(
      output_surface_->context_provider(), bitmap_manager_,
      gpu_memory_buffer_manager_, nullptr, settings_.highp_threshold_min,
      settings_.texture_id_allocation_chunk_size,
      output_surface_->capabilities().delegated_sync_points_required,
      settings_.use_gpu_memory_buffer_resources,
      std::vector<unsigned>(static_cast<size_t>(gfx::BufferFormat::LAST) + 1,
                            GL_TEXTURE_2D)));

  if (output_surface_->context_provider()) {
    std::unique_ptr<GLRenderer> renderer = GLRenderer::Create(
        this, &settings_, output_surface_.get(), resource_provider.get(),
        texture_mailbox_deleter_.get(), settings_.highp_threshold_min);
    if (!renderer)
      return;
    renderer_ = std::move(renderer);
  } else if (output_surface_->vulkan_context_provider()) {
#if defined(ENABLE_VULKAN)
    std::unique_ptr<VulkanRenderer> renderer = VulkanRenderer::Create(
        this, &settings_, output_surface_.get(), resource_provider.get(),
        texture_mailbox_deleter_.get(), settings_.highp_threshold_min);
    if (!renderer)
      return;
    renderer_ = std::move(renderer);
#else
    NOTREACHED();
#endif
  } else {
    std::unique_ptr<SoftwareRenderer> renderer = SoftwareRenderer::Create(
        this, &settings_, output_surface_.get(), resource_provider.get(),
        true /* use_image_hijack_canvas */);
    if (!renderer)
      return;
    renderer_ = std::move(renderer);
  }

  renderer_->SetEnlargePassTextureAmount(enlarge_texture_amount_);

  resource_provider_ = std::move(resource_provider);
  // TODO(jbauman): Outputting an incomplete quad list doesn't work when using
  // overlays.
  bool output_partial_list = renderer_->Capabilities().using_partial_swap &&
                             !output_surface_->GetOverlayCandidateValidator();
  aggregator_.reset(new SurfaceAggregator(
      surface_manager_, resource_provider_.get(), output_partial_list));
  aggregator_->set_output_is_secure(output_is_secure_);
}

void Display::DidLoseOutputSurface() {
  if (scheduler_)
    scheduler_->OutputSurfaceLost();
  // WARNING: The client may delete the Display in this method call. Do not
  // make any additional references to members after this call.
  client_->DisplayOutputSurfaceLost();
}

void Display::UpdateRootSurfaceResourcesLocked() {
  Surface* surface = surface_manager_->GetSurfaceForId(current_surface_id_);
  bool root_surface_resources_locked =
      !surface || !surface->GetEligibleFrame().delegated_frame_data;
  if (scheduler_)
    scheduler_->SetRootSurfaceResourcesLocked(root_surface_resources_locked);
}

bool Display::DrawAndSwap() {
  TRACE_EVENT0("cc", "Display::DrawAndSwap");

  if (current_surface_id_.is_null()) {
    TRACE_EVENT_INSTANT0("cc", "No root surface.", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  InitializeRenderer();
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
      surface->RunDrawCallbacks(SurfaceDrawStatus::DRAWN);
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

  if (should_draw) {
    gfx::Rect device_viewport_rect = gfx::Rect(current_surface_size_);
    gfx::Rect device_clip_rect =
        external_clip_.IsEmpty() ? device_viewport_rect : external_clip_;
    bool disable_picture_quad_image_filtering = false;

    renderer_->DecideRenderPassAllocationsForFrame(
        frame_data->render_pass_list);
    renderer_->DrawFrame(&frame_data->render_pass_list, device_scale_factor_,
                         device_color_space_, device_viewport_rect,
                         device_clip_rect,
                         disable_picture_quad_image_filtering);
  } else {
    TRACE_EVENT_INSTANT0("cc", "Draw skipped.", TRACE_EVENT_SCOPE_THREAD);
  }

  bool should_swap = should_draw && size_matches;
  if (should_swap) {
    swapped_since_resize_ = true;
    for (auto& latency : frame.metadata.latency_info) {
      TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
          TRACE_ID_DONT_MANGLE(latency.trace_id()),
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
          "step", "Display::DrawAndSwap");
    }
    benchmark_instrumentation::IssueDisplayRenderingStatsEvent();
    renderer_->SwapBuffers(std::move(frame.metadata));
  } else {
    if (have_damage && !size_matches)
      aggregator_->SetFullDamageForSurface(current_surface_id_);
    TRACE_EVENT_INSTANT0("cc", "Swap skipped.", TRACE_EVENT_SCOPE_THREAD);
    stored_latency_info_.insert(stored_latency_info_.end(),
                                frame.metadata.latency_info.begin(),
                                frame.metadata.latency_info.end());
    DidSwapBuffers();
    DidSwapBuffersComplete();
  }

  return true;
}

void Display::DidSwapBuffers() {
  if (scheduler_)
    scheduler_->DidSwapBuffers();
}

void Display::DidSwapBuffersComplete() {
  if (scheduler_)
    scheduler_->DidSwapBuffersComplete();
  if (renderer_)
    renderer_->SwapBuffersComplete();
}

void Display::CommitVSyncParameters(base::TimeTicks timebase,
                                    base::TimeDelta interval) {
  // Display uses a BeginFrameSource instead.
  NOTREACHED();
}

void Display::DidReceiveTextureInUseResponses(
    const gpu::TextureInUseResponses& responses) {
  if (renderer_)
    renderer_->DidReceiveTextureInUseResponses(responses);
}

void Display::SetBeginFrameSource(BeginFrameSource* source) {
  // The BeginFrameSource is set from the constructor, it doesn't come
  // from the OutputSurface for the Display.
  NOTREACHED();
}

void Display::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  client_->DisplaySetMemoryPolicy(policy);
}

void Display::OnDraw(const gfx::Transform& transform,
                     const gfx::Rect& viewport,
                     const gfx::Rect& clip,
                     bool resourceless_software_draw) {
  NOTREACHED();
}

void Display::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  aggregator_->SetFullDamageForSurface(current_surface_id_);
  if (scheduler_)
    scheduler_->SurfaceDamaged(current_surface_id_);
}

void Display::ReclaimResources(const CompositorFrameAck* ack) {
  NOTREACHED();
}

void Display::SetExternalTilePriorityConstraints(
    const gfx::Rect& viewport_rect,
    const gfx::Transform& transform) {
  NOTREACHED();
}

void Display::SetTreeActivationCallback(const base::Closure& callback) {
  NOTREACHED();
}

void Display::SetFullRootLayerDamage() {
  if (aggregator_ && !current_surface_id_.is_null())
    aggregator_->SetFullDamageForSurface(current_surface_id_);
}

void Display::OnSurfaceDamaged(SurfaceId surface_id, bool* changed) {
  if (aggregator_ &&
      aggregator_->previous_contained_surfaces().count(surface_id)) {
    Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
    if (surface) {
      const CompositorFrame& current_frame = surface->GetEligibleFrame();
      if (!current_frame.delegated_frame_data ||
          !current_frame.delegated_frame_data->resource_list.size()) {
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

SurfaceId Display::CurrentSurfaceId() {
  return current_surface_id_;
}

}  // namespace cc
