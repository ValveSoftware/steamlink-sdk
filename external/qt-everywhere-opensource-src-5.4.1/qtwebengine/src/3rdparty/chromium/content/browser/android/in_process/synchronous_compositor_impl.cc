// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_impl.h"

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "cc/input/input_handler.h"
#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ui/gl/gl_surface.h"

namespace content {

namespace {

int GetInProcessRendererId() {
  content::RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();
  if (it.IsAtEnd()) {
    // There should always be one RPH in single process mode.
    NOTREACHED();
    return 0;
  }

  int id = it.GetCurrentValue()->GetID();
  it.Advance();
  DCHECK(it.IsAtEnd());  // Not multiprocess compatible.
  return id;
}

base::LazyInstance<SynchronousCompositorFactoryImpl>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SynchronousCompositorImpl);

// static
SynchronousCompositorImpl* SynchronousCompositorImpl::FromID(int process_id,
                                                             int routing_id) {
  if (g_factory == NULL)
    return NULL;
  RenderViewHost* rvh = RenderViewHost::FromID(process_id, routing_id);
  if (!rvh)
    return NULL;
  WebContents* contents = WebContents::FromRenderViewHost(rvh);
  if (!contents)
    return NULL;
  return FromWebContents(contents);
}

SynchronousCompositorImpl* SynchronousCompositorImpl::FromRoutingID(
    int routing_id) {
  return FromID(GetInProcessRendererId(), routing_id);
}

SynchronousCompositorImpl::SynchronousCompositorImpl(WebContents* contents)
    : compositor_client_(NULL),
      output_surface_(NULL),
      contents_(contents),
      input_handler_(NULL),
      weak_ptr_factory_(this) {
  DCHECK(contents);
}

SynchronousCompositorImpl::~SynchronousCompositorImpl() {
  if (compositor_client_)
    compositor_client_->DidDestroyCompositor(this);
  SetInputHandler(NULL);
}

void SynchronousCompositorImpl::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  compositor_client_ = compositor_client;
}

// static
void SynchronousCompositor::SetGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  g_factory.Get().SetDeferredGpuService(service);
}

// static
void SynchronousCompositor::SetRecordFullDocument(bool record_full_document) {
  g_factory.Get().SetRecordFullDocument(record_full_document);
}

bool SynchronousCompositorImpl::InitializeHwDraw() {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  scoped_refptr<cc::ContextProvider> onscreen_context =
      g_factory.Get().CreateOnscreenContextProviderForCompositorThread();

  bool success = output_surface_->InitializeHwDraw(onscreen_context);

  if (success)
    g_factory.Get().CompositorInitializedHardwareDraw();
  return success;
}

void SynchronousCompositorImpl::ReleaseHwDraw() {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  output_surface_->ReleaseHwDraw();
  g_factory.Get().CompositorReleasedHardwareDraw();
}

gpu::GLInProcessContext* SynchronousCompositorImpl::GetShareContext() {
  DCHECK(CalledOnValidThread());
  return g_factory.Get().GetShareContext();
}

scoped_ptr<cc::CompositorFrame> SynchronousCompositorImpl::DemandDrawHw(
    gfx::Size surface_size,
    const gfx::Transform& transform,
    gfx::Rect viewport,
    gfx::Rect clip) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  scoped_ptr<cc::CompositorFrame> frame =
      output_surface_->DemandDrawHw(surface_size, transform, viewport, clip);
  if (frame.get())
    UpdateFrameMetaData(frame->metadata);
  return frame.Pass();
}

void SynchronousCompositorImpl::ReturnResources(
    const cc::CompositorFrameAck& frame_ack) {
  DCHECK(CalledOnValidThread());
  output_surface_->ReturnResources(frame_ack);
}

bool SynchronousCompositorImpl::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  scoped_ptr<cc::CompositorFrame> frame = output_surface_->DemandDrawSw(canvas);
  if (frame.get())
    UpdateFrameMetaData(frame->metadata);
  return !!frame.get();
}

void SynchronousCompositorImpl::UpdateFrameMetaData(
    const cc::CompositorFrameMetadata& frame_metadata) {
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      contents_->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->SynchronousFrameMetadata(frame_metadata);
}

void SynchronousCompositorImpl::SetMemoryPolicy(
    const SynchronousCompositorMemoryPolicy& policy) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  output_surface_->SetMemoryPolicy(policy);
}

void SynchronousCompositorImpl::DidChangeRootLayerScrollOffset() {
  if (input_handler_)
    input_handler_->OnRootLayerDelegatedScrollOffsetChanged();
}

void SynchronousCompositorImpl::DidBindOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());
  output_surface_ = output_surface;
  if (compositor_client_)
    compositor_client_->DidInitializeCompositor(this);
}

void SynchronousCompositorImpl::DidDestroySynchronousOutputSurface(
       SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());

  // Allow for transient hand-over when two output surfaces may refer to
  // a single delegate.
  if (output_surface_ == output_surface) {
    output_surface_ = NULL;
    if (compositor_client_)
      compositor_client_->DidDestroyCompositor(this);
    compositor_client_ = NULL;
  }
}

void SynchronousCompositorImpl::SetInputHandler(
    cc::InputHandler* input_handler) {
  DCHECK(CalledOnValidThread());

  if (input_handler_)
    input_handler_->SetRootLayerScrollOffsetDelegate(NULL);

  input_handler_ = input_handler;

  if (input_handler_)
    input_handler_->SetRootLayerScrollOffsetDelegate(this);
}

void SynchronousCompositorImpl::DidOverscroll(
    const DidOverscrollParams& params) {
  if (compositor_client_) {
    compositor_client_->DidOverscroll(params.accumulated_overscroll,
                                      params.latest_overscroll_delta,
                                      params.current_fling_velocity);
  }
}

void SynchronousCompositorImpl::DidStopFlinging() {
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      contents_->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->DidStopFlinging();
}

void SynchronousCompositorImpl::SetContinuousInvalidate(bool enable) {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->SetContinuousInvalidate(enable);
}

InputEventAckState SynchronousCompositorImpl::HandleInputEvent(
    const blink::WebInputEvent& input_event) {
  DCHECK(CalledOnValidThread());
  return g_factory.Get().synchronous_input_event_filter()->HandleInputEvent(
      contents_->GetRoutingID(), input_event);
}

void SynchronousCompositorImpl::DidActivatePendingTree() {
  if (compositor_client_)
    compositor_client_->DidUpdateContent();
}

gfx::Vector2dF SynchronousCompositorImpl::GetTotalScrollOffset() {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    return compositor_client_->GetTotalRootLayerScrollOffset();
  return gfx::Vector2dF();
}

bool SynchronousCompositorImpl::IsExternalFlingActive() const {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    return compositor_client_->IsExternalFlingActive();
  return false;
}

void SynchronousCompositorImpl::UpdateRootLayerState(
    const gfx::Vector2dF& total_scroll_offset,
    const gfx::Vector2dF& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  DCHECK(CalledOnValidThread());
  if (!compositor_client_)
    return;

  compositor_client_->UpdateRootLayerState(total_scroll_offset,
                                           max_scroll_offset,
                                           scrollable_size,
                                           page_scale_factor,
                                           min_page_scale_factor,
                                           max_page_scale_factor);
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorImpl() must only be used on the UI thread.
bool SynchronousCompositorImpl::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

// static
void SynchronousCompositor::SetClientForWebContents(
    WebContents* contents,
    SynchronousCompositorClient* client) {
  DCHECK(contents);
  if (client) {
    g_factory.Get();  // Ensure it's initialized.
    SynchronousCompositorImpl::CreateForWebContents(contents);
  }
  if (SynchronousCompositorImpl* instance =
      SynchronousCompositorImpl::FromWebContents(contents)) {
    instance->SetClient(client);
  }
}

}  // namespace content
