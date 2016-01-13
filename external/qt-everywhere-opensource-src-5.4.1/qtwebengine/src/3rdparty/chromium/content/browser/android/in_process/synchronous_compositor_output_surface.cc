// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_output_surface.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace content {

namespace {

void DidActivatePendingTree(int routing_id) {
  SynchronousCompositorOutputSurfaceDelegate* delegate =
      SynchronousCompositorImpl::FromRoutingID(routing_id);
  if (delegate)
    delegate->DidActivatePendingTree();
}

} // namespace

class SynchronousCompositorOutputSurface::SoftwareDevice
  : public cc::SoftwareOutputDevice {
 public:
  SoftwareDevice(SynchronousCompositorOutputSurface* surface)
    : surface_(surface) {
  }
  virtual void Resize(const gfx::Size& pixel_size,
                      float scale_factor) OVERRIDE {
    // Intentional no-op: canvas size is controlled by the embedder.
  }
  virtual SkCanvas* BeginPaint(const gfx::Rect& damage_rect) OVERRIDE {
    if (!surface_->current_sw_canvas_) {
      NOTREACHED() << "BeginPaint with no canvas set";
      return &null_canvas_;
    }
    LOG_IF(WARNING, surface_->frame_holder_.get())
        << "Mutliple calls to BeginPaint per frame";
    return surface_->current_sw_canvas_;
  }
  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE {
  }
  virtual void CopyToPixels(const gfx::Rect& rect, void* pixels) OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  SynchronousCompositorOutputSurface* surface_;
  SkCanvas null_canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareDevice);
};

SynchronousCompositorOutputSurface::SynchronousCompositorOutputSurface(
    int routing_id)
    : cc::OutputSurface(
          scoped_ptr<cc::SoftwareOutputDevice>(new SoftwareDevice(this))),
      routing_id_(routing_id),
      needs_begin_frame_(false),
      invoking_composite_(false),
      current_sw_canvas_(NULL),
      memory_policy_(0),
      output_surface_client_(NULL) {
  capabilities_.deferred_gl_initialization = true;
  capabilities_.draw_and_swap_full_viewport_every_frame = true;
  capabilities_.adjust_deadline_for_parent = false;
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
  // Cannot call out to GetDelegate() here as the output surface is not
  // constructed on the correct thread.

  memory_policy_.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
}

SynchronousCompositorOutputSurface::~SynchronousCompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  SynchronousCompositorOutputSurfaceDelegate* delegate = GetDelegate();
  if (delegate)
    delegate->DidDestroySynchronousOutputSurface(this);
}

bool SynchronousCompositorOutputSurface::ForcedDrawToSoftwareDevice() const {
  // |current_sw_canvas_| indicates we're in a DemandDrawSw call. In addition
  // |invoking_composite_| == false indicates an attempt to draw outside of
  // the synchronous compositor's control: force it into SW path and hence to
  // the null canvas (and will log a warning there).
  return current_sw_canvas_ != NULL || !invoking_composite_;
}

bool SynchronousCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* surface_client) {
  DCHECK(CalledOnValidThread());
  if (!cc::OutputSurface::BindToClient(surface_client))
    return false;

  output_surface_client_ = surface_client;
  output_surface_client_->SetTreeActivationCallback(
      base::Bind(&DidActivatePendingTree, routing_id_));
  output_surface_client_->SetMemoryPolicy(memory_policy_);

  SynchronousCompositorOutputSurfaceDelegate* delegate = GetDelegate();
  if (delegate)
    delegate->DidBindOutputSurface(this);

  return true;
}

void SynchronousCompositorOutputSurface::Reshape(
    const gfx::Size& size, float scale_factor) {
  // Intentional no-op: surface size is controlled by the embedder.
}

void SynchronousCompositorOutputSurface::SetNeedsBeginFrame(bool enable) {
  DCHECK(CalledOnValidThread());
  needs_begin_frame_ = enable;
  SynchronousCompositorOutputSurfaceDelegate* delegate = GetDelegate();
  if (delegate && !invoking_composite_)
    delegate->SetContinuousInvalidate(needs_begin_frame_);
}

void SynchronousCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  DCHECK(CalledOnValidThread());

  frame_holder_.reset(new cc::CompositorFrame);
  frame->AssignTo(frame_holder_.get());

  client_->DidSwapBuffers();
}

namespace {
void AdjustTransform(gfx::Transform* transform, gfx::Rect viewport) {
  // CC's draw origin starts at the viewport.
  transform->matrix().postTranslate(-viewport.x(), -viewport.y(), 0);
}
} // namespace

bool SynchronousCompositorOutputSurface::InitializeHwDraw(
    scoped_refptr<cc::ContextProvider> onscreen_context_provider) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  DCHECK(!context_provider_);

  return InitializeAndSetContext3d(onscreen_context_provider);
}

void SynchronousCompositorOutputSurface::ReleaseHwDraw() {
  DCHECK(CalledOnValidThread());
  cc::OutputSurface::ReleaseGL();
}

scoped_ptr<cc::CompositorFrame>
SynchronousCompositorOutputSurface::DemandDrawHw(
    gfx::Size surface_size,
    const gfx::Transform& transform,
    gfx::Rect viewport,
    gfx::Rect clip) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  DCHECK(context_provider_);

  surface_size_ = surface_size;
  InvokeComposite(transform, viewport, clip, true);

  return frame_holder_.Pass();
}

scoped_ptr<cc::CompositorFrame>
SynchronousCompositorOutputSurface::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(canvas);
  DCHECK(!current_sw_canvas_);
  base::AutoReset<SkCanvas*> canvas_resetter(&current_sw_canvas_, canvas);

  SkIRect canvas_clip;
  canvas->getClipDeviceBounds(&canvas_clip);
  gfx::Rect clip = gfx::SkIRectToRect(canvas_clip);

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = canvas->getTotalMatrix();  // Converts 3x3 matrix to 4x4.

  surface_size_ = gfx::Size(canvas->getDeviceSize().width(),
                            canvas->getDeviceSize().height());

  InvokeComposite(transform, clip, clip, false);

  return frame_holder_.Pass();
}

void SynchronousCompositorOutputSurface::InvokeComposite(
    const gfx::Transform& transform,
    gfx::Rect viewport,
    gfx::Rect clip,
    bool valid_for_tile_management) {
  DCHECK(!invoking_composite_);
  DCHECK(!frame_holder_.get());
  base::AutoReset<bool> invoking_composite_resetter(&invoking_composite_, true);

  gfx::Transform adjusted_transform = transform;
  AdjustTransform(&adjusted_transform, viewport);
  SetExternalDrawConstraints(
      adjusted_transform, viewport, clip, valid_for_tile_management);
  SetNeedsRedrawRect(gfx::Rect(viewport.size()));
  client_->BeginFrame(cc::BeginFrameArgs::CreateForSynchronousCompositor());

  // After software draws (which might move the viewport arbitrarily), restore
  // the previous hardware viewport to allow CC's tile manager to prioritize
  // properly.
  if (valid_for_tile_management) {
    cached_hw_transform_ = adjusted_transform;
    cached_hw_viewport_ = viewport;
    cached_hw_clip_ = clip;
  } else {
    SetExternalDrawConstraints(
        cached_hw_transform_, cached_hw_viewport_, cached_hw_clip_, true);
  }

  if (frame_holder_.get())
    client_->DidSwapBuffersComplete();

  SynchronousCompositorOutputSurfaceDelegate* delegate = GetDelegate();
  if (delegate)
    delegate->SetContinuousInvalidate(needs_begin_frame_);
}

void SynchronousCompositorOutputSurface::ReturnResources(
    const cc::CompositorFrameAck& frame_ack) {
  ReclaimResources(&frame_ack);
}

void SynchronousCompositorOutputSurface::SetMemoryPolicy(
    const SynchronousCompositorMemoryPolicy& policy) {
  DCHECK(CalledOnValidThread());
  memory_policy_.bytes_limit_when_visible = policy.bytes_limit;
  memory_policy_.num_resources_limit = policy.num_resources_limit;

  if (output_surface_client_)
    output_surface_client_->SetMemoryPolicy(memory_policy_);
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorOutputSurface() must only be used on the UI
// thread.
bool SynchronousCompositorOutputSurface::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

SynchronousCompositorOutputSurfaceDelegate*
SynchronousCompositorOutputSurface::GetDelegate() {
  return SynchronousCompositorImpl::FromRoutingID(routing_id_);
}

}  // namespace content
