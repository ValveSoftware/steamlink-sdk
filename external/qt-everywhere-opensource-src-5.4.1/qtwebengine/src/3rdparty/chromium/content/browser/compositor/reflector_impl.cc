// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/reflector_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/common/gpu/client/gl_helper.h"
#include "ui/compositor/layer.h"

namespace content {

ReflectorImpl::ReflectorImpl(
    ui::Compositor* mirrored_compositor,
    ui::Layer* mirroring_layer,
    IDMap<BrowserCompositorOutputSurface>* output_surface_map,
    base::MessageLoopProxy* compositor_thread_loop,
    int surface_id)
    : impl_unsafe_(output_surface_map),
      main_unsafe_(mirrored_compositor, mirroring_layer),
      impl_message_loop_(compositor_thread_loop),
      main_message_loop_(base::MessageLoopProxy::current()),
      surface_id_(surface_id) {
  GLHelper* helper = ImageTransportFactory::GetInstance()->GetGLHelper();
  MainThreadData& main = GetMain();
  main.mailbox = new OwnedMailbox(helper);
  impl_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &ReflectorImpl::InitOnImplThread, this, main.mailbox->holder()));
}

ReflectorImpl::MainThreadData::MainThreadData(
    ui::Compositor* mirrored_compositor,
    ui::Layer* mirroring_layer)
    : needs_set_mailbox(true),
      mirrored_compositor(mirrored_compositor),
      mirroring_layer(mirroring_layer) {}

ReflectorImpl::MainThreadData::~MainThreadData() {}

ReflectorImpl::ImplThreadData::ImplThreadData(
    IDMap<BrowserCompositorOutputSurface>* output_surface_map)
    : output_surface_map(output_surface_map),
      output_surface(NULL),
      texture_id(0) {}

ReflectorImpl::ImplThreadData::~ImplThreadData() {}

ReflectorImpl::ImplThreadData& ReflectorImpl::GetImpl() {
  DCHECK(impl_message_loop_->BelongsToCurrentThread());
  return impl_unsafe_;
}

ReflectorImpl::MainThreadData& ReflectorImpl::GetMain() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  return main_unsafe_;
}

void ReflectorImpl::InitOnImplThread(const gpu::MailboxHolder& mailbox_holder) {
  ImplThreadData& impl = GetImpl();
  // Ignore if the reflector was shutdown before
  // initialized, or it's already initialized.
  if (!impl.output_surface_map || impl.gl_helper.get())
    return;

  impl.mailbox_holder = mailbox_holder;

  BrowserCompositorOutputSurface* source_surface =
      impl.output_surface_map->Lookup(surface_id_);
  // Skip if the source surface isn't ready yet. This will be
  // initialized when the source surface becomes ready.
  if (!source_surface)
    return;

  AttachToOutputSurfaceOnImplThread(impl.mailbox_holder, source_surface);
}

void ReflectorImpl::OnSourceSurfaceReady(
    BrowserCompositorOutputSurface* source_surface) {
  ImplThreadData& impl = GetImpl();
  AttachToOutputSurfaceOnImplThread(impl.mailbox_holder, source_surface);
}

void ReflectorImpl::Shutdown() {
  MainThreadData& main = GetMain();
  main.mailbox = NULL;
  main.mirroring_layer->SetShowPaintedContent();
  main.mirroring_layer = NULL;
  impl_message_loop_->PostTask(
      FROM_HERE, base::Bind(&ReflectorImpl::ShutdownOnImplThread, this));
}

void ReflectorImpl::DetachFromOutputSurface() {
  ImplThreadData& impl = GetImpl();
  DCHECK(impl.output_surface);
  impl.output_surface->SetReflector(NULL);
  DCHECK(impl.texture_id);
  impl.gl_helper->DeleteTexture(impl.texture_id);
  impl.texture_id = 0;
  impl.gl_helper.reset();
  impl.output_surface = NULL;
}

void ReflectorImpl::ShutdownOnImplThread() {
  ImplThreadData& impl = GetImpl();
  if (impl.output_surface)
    DetachFromOutputSurface();
  impl.output_surface_map = NULL;
  // The instance must be deleted on main thread.
  main_message_loop_->PostTask(FROM_HERE,
                               base::Bind(&ReflectorImpl::DeleteOnMainThread,
                                          scoped_refptr<ReflectorImpl>(this)));
}

void ReflectorImpl::ReattachToOutputSurfaceFromMainThread(
    BrowserCompositorOutputSurface* output_surface) {
  MainThreadData& main = GetMain();
  GLHelper* helper = ImageTransportFactory::GetInstance()->GetGLHelper();
  main.mailbox = new OwnedMailbox(helper);
  main.needs_set_mailbox = true;
  main.mirroring_layer->SetShowPaintedContent();
  impl_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::AttachToOutputSurfaceOnImplThread,
                 this,
                 main.mailbox->holder(),
                 output_surface));
}

void ReflectorImpl::OnMirroringCompositorResized() {
  MainThreadData& main = GetMain();
  main.mirroring_layer->SchedulePaint(main.mirroring_layer->bounds());
}

void ReflectorImpl::OnSwapBuffers() {
  ImplThreadData& impl = GetImpl();
  gfx::Size size = impl.output_surface->SurfaceSize();
  if (impl.texture_id) {
    impl.gl_helper->CopyTextureFullImage(impl.texture_id, size);
    impl.gl_helper->Flush();
  }
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &ReflectorImpl::FullRedrawOnMainThread, this->AsWeakPtr(), size));
}

void ReflectorImpl::OnPostSubBuffer(gfx::Rect rect) {
  ImplThreadData& impl = GetImpl();
  if (impl.texture_id) {
    impl.gl_helper->CopyTextureSubImage(impl.texture_id, rect);
    impl.gl_helper->Flush();
  }
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::UpdateSubBufferOnMainThread,
                 this->AsWeakPtr(),
                 impl.output_surface->SurfaceSize(),
                 rect));
}

ReflectorImpl::~ReflectorImpl() {
  // Make sure the reflector is deleted on main thread.
  DCHECK_EQ(main_message_loop_.get(), base::MessageLoopProxy::current().get());
}

static void ReleaseMailbox(scoped_refptr<OwnedMailbox> mailbox,
                           unsigned int sync_point,
                           bool is_lost) {
  mailbox->UpdateSyncPoint(sync_point);
}

void ReflectorImpl::AttachToOutputSurfaceOnImplThread(
    const gpu::MailboxHolder& mailbox_holder,
    BrowserCompositorOutputSurface* output_surface) {
  ImplThreadData& impl = GetImpl();
  if (output_surface == impl.output_surface)
    return;
  if (impl.output_surface)
    DetachFromOutputSurface();
  impl.output_surface = output_surface;
  output_surface->context_provider()->BindToCurrentThread();
  impl.gl_helper.reset(
      new GLHelper(output_surface->context_provider()->ContextGL(),
                   output_surface->context_provider()->ContextSupport()));
  impl.texture_id = impl.gl_helper->ConsumeMailboxToTexture(
      mailbox_holder.mailbox, mailbox_holder.sync_point);
  impl.gl_helper->ResizeTexture(impl.texture_id, output_surface->SurfaceSize());
  impl.gl_helper->Flush();
  output_surface->SetReflector(this);
  // The texture doesn't have the data, so invokes full redraw now.
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::FullRedrawContentOnMainThread,
                 scoped_refptr<ReflectorImpl>(this)));
}

void ReflectorImpl::UpdateTextureSizeOnMainThread(gfx::Size size) {
  MainThreadData& main = GetMain();
  if (!main.mirroring_layer || !main.mailbox ||
      main.mailbox->mailbox().IsZero())
    return;
  if (main.needs_set_mailbox) {
    main.mirroring_layer->SetTextureMailbox(
        cc::TextureMailbox(main.mailbox->holder()),
        cc::SingleReleaseCallback::Create(
            base::Bind(ReleaseMailbox, main.mailbox)),
        size);
    main.needs_set_mailbox = false;
  } else {
    main.mirroring_layer->SetTextureSize(size);
  }
  main.mirroring_layer->SetBounds(gfx::Rect(size));
}

void ReflectorImpl::FullRedrawOnMainThread(gfx::Size size) {
  MainThreadData& main = GetMain();
  if (!main.mirroring_layer)
    return;
  UpdateTextureSizeOnMainThread(size);
  main.mirroring_layer->SchedulePaint(main.mirroring_layer->bounds());
}

void ReflectorImpl::UpdateSubBufferOnMainThread(gfx::Size size,
                                                gfx::Rect rect) {
  MainThreadData& main = GetMain();
  if (!main.mirroring_layer)
    return;
  UpdateTextureSizeOnMainThread(size);
  // Flip the coordinates to compositor's one.
  int y = size.height() - rect.y() - rect.height();
  gfx::Rect new_rect(rect.x(), y, rect.width(), rect.height());
  main.mirroring_layer->SchedulePaint(new_rect);
}

void ReflectorImpl::FullRedrawContentOnMainThread() {
  MainThreadData& main = GetMain();
  main.mirrored_compositor->ScheduleFullRedraw();
}

}  // namespace content
