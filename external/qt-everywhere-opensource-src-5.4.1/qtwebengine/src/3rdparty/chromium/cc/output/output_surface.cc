// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

using std::set;
using std::string;
using std::vector;

namespace {

const size_t kGpuLatencyHistorySize = 60;
const double kGpuLatencyEstimationPercentile = 100.0;

}

namespace cc {

OutputSurface::OutputSurface(scoped_refptr<ContextProvider> context_provider)
    : client_(NULL),
      context_provider_(context_provider),
      device_scale_factor_(-1),
      external_stencil_test_enabled_(false),
      weak_ptr_factory_(this),
      gpu_latency_history_(kGpuLatencyHistorySize) {
}

OutputSurface::OutputSurface(scoped_ptr<SoftwareOutputDevice> software_device)
    : client_(NULL),
      software_device_(software_device.Pass()),
      device_scale_factor_(-1),
      external_stencil_test_enabled_(false),
      weak_ptr_factory_(this),
      gpu_latency_history_(kGpuLatencyHistorySize) {
}

OutputSurface::OutputSurface(scoped_refptr<ContextProvider> context_provider,
                             scoped_ptr<SoftwareOutputDevice> software_device)
    : client_(NULL),
      context_provider_(context_provider),
      software_device_(software_device.Pass()),
      device_scale_factor_(-1),
      external_stencil_test_enabled_(false),
      weak_ptr_factory_(this),
      gpu_latency_history_(kGpuLatencyHistorySize) {
}

void OutputSurface::CommitVSyncParameters(base::TimeTicks timebase,
                                          base::TimeDelta interval) {
  TRACE_EVENT2("cc",
               "OutputSurface::CommitVSyncParameters",
               "timebase",
               (timebase - base::TimeTicks()).InSecondsF(),
               "interval",
               interval.InSecondsF());
  client_->CommitVSyncParameters(timebase, interval);
}

// Forwarded to OutputSurfaceClient
void OutputSurface::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "OutputSurface::SetNeedsRedrawRect");
  client_->SetNeedsRedrawRect(damage_rect);
}

void OutputSurface::ReclaimResources(const CompositorFrameAck* ack) {
  client_->ReclaimResources(ack);
}

void OutputSurface::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "OutputSurface::DidLoseOutputSurface");
  pending_gpu_latency_query_ids_.clear();
  available_gpu_latency_query_ids_.clear();
  client_->DidLoseOutputSurface();
}

void OutputSurface::SetExternalStencilTest(bool enabled) {
  external_stencil_test_enabled_ = enabled;
}

void OutputSurface::SetExternalDrawConstraints(const gfx::Transform& transform,
                                               const gfx::Rect& viewport,
                                               const gfx::Rect& clip,
                                               bool valid_for_tile_management) {
  client_->SetExternalDrawConstraints(
      transform, viewport, clip, valid_for_tile_management);
}

OutputSurface::~OutputSurface() {
  ResetContext3d();
}

bool OutputSurface::HasExternalStencilTest() const {
  return external_stencil_test_enabled_;
}

bool OutputSurface::ForcedDrawToSoftwareDevice() const { return false; }

bool OutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  client_ = client;
  bool success = true;

  if (context_provider_) {
    success = context_provider_->BindToCurrentThread();
    if (success)
      SetUpContext3d();
  }

  if (!success)
    client_ = NULL;

  return success;
}

bool OutputSurface::InitializeAndSetContext3d(
    scoped_refptr<ContextProvider> context_provider) {
  DCHECK(!context_provider_);
  DCHECK(context_provider);
  DCHECK(client_);

  bool success = false;
  if (context_provider->BindToCurrentThread()) {
    context_provider_ = context_provider;
    SetUpContext3d();
    client_->DeferredInitialize();
    success = true;
  }

  if (!success)
    ResetContext3d();

  return success;
}

void OutputSurface::ReleaseGL() {
  DCHECK(client_);
  DCHECK(context_provider_);
  client_->ReleaseGL();
  DCHECK(!context_provider_);
}

void OutputSurface::SetUpContext3d() {
  DCHECK(context_provider_);
  DCHECK(client_);

  context_provider_->SetLostContextCallback(
      base::Bind(&OutputSurface::DidLoseOutputSurface,
                 base::Unretained(this)));
  context_provider_->ContextSupport()->SetSwapBuffersCompleteCallback(
      base::Bind(&OutputSurface::OnSwapBuffersComplete,
                 base::Unretained(this)));
  context_provider_->SetMemoryPolicyChangedCallback(
      base::Bind(&OutputSurface::SetMemoryPolicy,
                 base::Unretained(this)));
}

void OutputSurface::ReleaseContextProvider() {
  DCHECK(client_);
  DCHECK(context_provider_);
  ResetContext3d();
}

void OutputSurface::ResetContext3d() {
  if (context_provider_.get()) {
    while (!pending_gpu_latency_query_ids_.empty()) {
      unsigned query_id = pending_gpu_latency_query_ids_.front();
      pending_gpu_latency_query_ids_.pop_front();
      context_provider_->ContextGL()->DeleteQueriesEXT(1, &query_id);
    }
    while (!available_gpu_latency_query_ids_.empty()) {
      unsigned query_id = available_gpu_latency_query_ids_.front();
      available_gpu_latency_query_ids_.pop_front();
      context_provider_->ContextGL()->DeleteQueriesEXT(1, &query_id);
    }
    context_provider_->SetLostContextCallback(
        ContextProvider::LostContextCallback());
    context_provider_->SetMemoryPolicyChangedCallback(
        ContextProvider::MemoryPolicyChangedCallback());
    if (gpu::ContextSupport* support = context_provider_->ContextSupport())
      support->SetSwapBuffersCompleteCallback(base::Closure());
  }
  context_provider_ = NULL;
}

void OutputSurface::EnsureBackbuffer() {
  if (software_device_)
    software_device_->EnsureBackbuffer();
}

void OutputSurface::DiscardBackbuffer() {
  if (context_provider_)
    context_provider_->ContextGL()->DiscardBackbufferCHROMIUM();
  if (software_device_)
    software_device_->DiscardBackbuffer();
}

void OutputSurface::Reshape(const gfx::Size& size, float scale_factor) {
  if (size == surface_size_ && scale_factor == device_scale_factor_)
    return;

  surface_size_ = size;
  device_scale_factor_ = scale_factor;
  if (context_provider_) {
    context_provider_->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), scale_factor);
  }
  if (software_device_)
    software_device_->Resize(size, scale_factor);
}

gfx::Size OutputSurface::SurfaceSize() const {
  return surface_size_;
}

void OutputSurface::BindFramebuffer() {
  DCHECK(context_provider_);
  context_provider_->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OutputSurface::SwapBuffers(CompositorFrame* frame) {
  if (frame->software_frame_data) {
    PostSwapBuffersComplete();
    client_->DidSwapBuffers();
    return;
  }

  DCHECK(context_provider_);
  DCHECK(frame->gl_frame_data);

  UpdateAndMeasureGpuLatency();
  if (frame->gl_frame_data->sub_buffer_rect ==
      gfx::Rect(frame->gl_frame_data->size)) {
    context_provider_->ContextSupport()->Swap();
  } else {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        frame->gl_frame_data->sub_buffer_rect);
  }

  client_->DidSwapBuffers();
}

base::TimeDelta OutputSurface::GpuLatencyEstimate() {
  if (context_provider_ && !capabilities_.adjust_deadline_for_parent)
    return gpu_latency_history_.Percentile(kGpuLatencyEstimationPercentile);
  else
    return base::TimeDelta();
}

void OutputSurface::UpdateAndMeasureGpuLatency() {
  // http://crbug.com/306690  tracks re-enabling latency queries.
#if 0
  // We only care about GPU latency for surfaces that do not have a parent
  // compositor, since surfaces that do have a parent compositor can use
  // mailboxes or delegated rendering to send frames to their parent without
  // incurring GPU latency.
  if (capabilities_.adjust_deadline_for_parent)
    return;

  while (pending_gpu_latency_query_ids_.size()) {
    unsigned query_id = pending_gpu_latency_query_ids_.front();
    unsigned query_complete = 1;
    context_provider_->ContextGL()->GetQueryObjectuivEXT(
        query_id, GL_QUERY_RESULT_AVAILABLE_EXT, &query_complete);
    if (!query_complete)
      break;

    unsigned value = 0;
    context_provider_->ContextGL()->GetQueryObjectuivEXT(
        query_id, GL_QUERY_RESULT_EXT, &value);
    pending_gpu_latency_query_ids_.pop_front();
    available_gpu_latency_query_ids_.push_back(query_id);

    base::TimeDelta latency = base::TimeDelta::FromMicroseconds(value);
    base::TimeDelta latency_estimate = GpuLatencyEstimate();
    gpu_latency_history_.InsertSample(latency);

    base::TimeDelta latency_overestimate;
    base::TimeDelta latency_underestimate;
    if (latency > latency_estimate)
      latency_underestimate = latency - latency_estimate;
    else
      latency_overestimate = latency_estimate - latency;
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.GpuLatency",
                               latency,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.GpuLatencyUnderestimate",
                               latency_underestimate,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.GpuLatencyOverestimate",
                               latency_overestimate,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
  }

  unsigned gpu_latency_query_id;
  if (available_gpu_latency_query_ids_.size()) {
    gpu_latency_query_id = available_gpu_latency_query_ids_.front();
    available_gpu_latency_query_ids_.pop_front();
  } else {
    context_provider_->ContextGL()->GenQueriesEXT(1, &gpu_latency_query_id);
  }

  context_provider_->ContextGL()->BeginQueryEXT(GL_LATENCY_QUERY_CHROMIUM,
                                                gpu_latency_query_id);
  context_provider_->ContextGL()->EndQueryEXT(GL_LATENCY_QUERY_CHROMIUM);
  pending_gpu_latency_query_ids_.push_back(gpu_latency_query_id);
#endif
}

void OutputSurface::PostSwapBuffersComplete() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&OutputSurface::OnSwapBuffersComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

// We don't post tasks bound to the client directly since they might run
// after the OutputSurface has been destroyed.
void OutputSurface::OnSwapBuffersComplete() {
  client_->DidSwapBuffersComplete();
}

void OutputSurface::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  TRACE_EVENT1("cc", "OutputSurface::SetMemoryPolicy",
               "bytes_limit_when_visible", policy.bytes_limit_when_visible);
  // Just ignore the memory manager when it says to set the limit to zero
  // bytes. This will happen when the memory manager thinks that the renderer
  // is not visible (which the renderer knows better).
  if (policy.bytes_limit_when_visible)
    client_->SetMemoryPolicy(policy);
}

}  // namespace cc
