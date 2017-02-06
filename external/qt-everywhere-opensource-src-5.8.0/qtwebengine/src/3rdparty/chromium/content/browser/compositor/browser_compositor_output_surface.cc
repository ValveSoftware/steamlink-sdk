// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "cc/base/switches.h"
#include "cc/output/output_surface_client.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"

namespace content {

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    scoped_refptr<cc::ContextProvider> context_provider,
    scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
    cc::SyntheticBeginFrameSource* begin_frame_source,
    std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
        overlay_candidate_validator)
    : OutputSurface(std::move(context_provider), nullptr, nullptr),
      vsync_manager_(std::move(vsync_manager)),
      synthetic_begin_frame_source_(begin_frame_source),
      reflector_(nullptr),
      use_begin_frame_scheduling_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              cc::switches::kEnableBeginFrameScheduling)) {
  overlay_candidate_validator_ = std::move(overlay_candidate_validator);
  Initialize();
}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    std::unique_ptr<cc::SoftwareOutputDevice> software_device,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
    cc::SyntheticBeginFrameSource* begin_frame_source)
    : OutputSurface(nullptr, nullptr, std::move(software_device)),
      vsync_manager_(vsync_manager),
      synthetic_begin_frame_source_(begin_frame_source),
      reflector_(nullptr),
      use_begin_frame_scheduling_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              cc::switches::kEnableBeginFrameScheduling)) {
  Initialize();
}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    const scoped_refptr<cc::VulkanContextProvider>& vulkan_context_provider,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
    cc::SyntheticBeginFrameSource* begin_frame_source)
    : OutputSurface(std::move(vulkan_context_provider)),
      vsync_manager_(vsync_manager),
      synthetic_begin_frame_source_(begin_frame_source),
      reflector_(nullptr) {
  Initialize();
}

BrowserCompositorOutputSurface::~BrowserCompositorOutputSurface() {
  if (reflector_)
    reflector_->DetachFromOutputSurface();
  DCHECK(!reflector_);
  if (!HasClient())
    return;

  // When BeginFrame scheduling is enabled, vsync info is not routed to renderer
  // by using |vsync_manager_|. Instead, BeginFrame message is used.
  if (!use_begin_frame_scheduling_)
    vsync_manager_->RemoveObserver(this);
}

void BrowserCompositorOutputSurface::Initialize() {
  capabilities_.adjust_deadline_for_parent = false;
}

bool BrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  if (!OutputSurface::BindToClient(client))
    return false;

  // Don't want vsync notifications until there is a client.
  if (!use_begin_frame_scheduling_)
    vsync_manager_->AddObserver(this);
  return true;
}

void BrowserCompositorOutputSurface::UpdateVSyncParametersInternal(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (interval.is_zero()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval = cc::BeginFrameArgs::DefaultInterval();
  }
  synthetic_begin_frame_source_->OnUpdateVSyncParameters(timebase, interval);
}

void BrowserCompositorOutputSurface::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(HasClient());
  DCHECK(!use_begin_frame_scheduling_);
  UpdateVSyncParametersInternal(timebase, interval);
}

void BrowserCompositorOutputSurface::OnUpdateVSyncParametersFromGpu(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(HasClient());
  if (use_begin_frame_scheduling_) {
    UpdateVSyncParametersInternal(timebase, interval);
    return;
  }

  vsync_manager_->UpdateVSyncParameters(timebase, interval);
}

void BrowserCompositorOutputSurface::SetReflector(ReflectorImpl* reflector) {
  // Software mirroring is done by doing a GL copy out of the framebuffer - if
  // we have overlays then that data will be missing.
  if (overlay_candidate_validator_) {
    overlay_candidate_validator_->SetSoftwareMirrorMode(reflector != nullptr);
  }
  reflector_ = reflector;

  OnReflectorChanged();
}

void BrowserCompositorOutputSurface::OnReflectorChanged() {
}

base::Closure
BrowserCompositorOutputSurface::CreateCompositionStartedCallback() {
  return base::Closure();
}

cc::OverlayCandidateValidator*
BrowserCompositorOutputSurface::GetOverlayCandidateValidator() const {
  return overlay_candidate_validator_.get();
}

}  // namespace content
