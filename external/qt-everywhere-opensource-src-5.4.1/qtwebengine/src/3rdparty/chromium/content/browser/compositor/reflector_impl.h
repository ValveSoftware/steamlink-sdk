// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_REFLECTOR_IMPL_H_
#define CONTENT_BROWSER_COMPOSITOR_REFLECTOR_IMPL_H_

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/compositor/reflector.h"
#include "ui/gfx/size.h"

namespace base { class MessageLoopProxy; }

namespace gfx { class Rect; }

namespace ui {
class Compositor;
class Layer;
}

namespace content {

class OwnedMailbox;
class BrowserCompositorOutputSurface;

// A reflector implementation that copies the framebuffer content
// to the texture, then draw it onto the mirroring compositor.
class ReflectorImpl : public base::SupportsWeakPtr<ReflectorImpl>,
                      public ui::Reflector {
 public:
  ReflectorImpl(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer,
      IDMap<BrowserCompositorOutputSurface>* output_surface_map,
      base::MessageLoopProxy* compositor_thread_loop,
      int surface_id);

  ui::Compositor* mirrored_compositor() {
    return GetMain().mirrored_compositor;
  }

  void InitOnImplThread(const gpu::MailboxHolder& mailbox_holder);
  void Shutdown();
  void ShutdownOnImplThread();

  // Post a task to attach the reflector to the output surface onto ImplThread.
  void ReattachToOutputSurfaceFromMainThread(
      BrowserCompositorOutputSurface* surface);

  // ui::Reflector implementation.
  virtual void OnMirroringCompositorResized() OVERRIDE;

  // Called in |BrowserCompositorOutputSurface::SwapBuffers| to copy
  // the full screen image to the |texture_id_|. This must be called
  // on ImplThread.
  void OnSwapBuffers();

  // Called in |BrowserCompositorOutputSurface::PostSubBuffer| copy
  // the sub image given by |rect| to the texture.This must be called
  // on ImplThread.
  void OnPostSubBuffer(gfx::Rect rect);

  // Create a shared texture that will be used to copy the content of
  // mirrored compositor to the mirroring compositor.  This should
  // be posted to the main thread when the output is attached in
  // impl thread.
  void CreateSharedTextureOnMainThread(gfx::Size size);

  // Called when the source surface is bound and available. This must
  // be called on ImplThread.
  void OnSourceSurfaceReady(BrowserCompositorOutputSurface* surface);

  void DetachFromOutputSurface();

 private:
  struct MainThreadData {
    MainThreadData(ui::Compositor* mirrored_compositor,
                   ui::Layer* mirroring_layer);
    ~MainThreadData();
    scoped_refptr<OwnedMailbox> mailbox;
    bool needs_set_mailbox;
    ui::Compositor* mirrored_compositor;
    ui::Layer* mirroring_layer;
  };

  struct ImplThreadData {
    explicit ImplThreadData(
        IDMap<BrowserCompositorOutputSurface>* output_surface_map);
    ~ImplThreadData();
    IDMap<BrowserCompositorOutputSurface>* output_surface_map;
    BrowserCompositorOutputSurface* output_surface;
    scoped_ptr<GLHelper> gl_helper;
    unsigned texture_id;
    gpu::MailboxHolder mailbox_holder;
  };

  virtual ~ReflectorImpl();

  void AttachToOutputSurfaceOnImplThread(
      const gpu::MailboxHolder& mailbox_holder,
      BrowserCompositorOutputSurface* surface);

  void UpdateTextureSizeOnMainThread(gfx::Size size);

  // Request full redraw on mirroring compositor.
  void FullRedrawOnMainThread(gfx::Size size);

  void UpdateSubBufferOnMainThread(gfx::Size size, gfx::Rect rect);

  // Request full redraw on mirrored compositor so that
  // the full content will be copied to mirroring compositor.
  void FullRedrawContentOnMainThread();

  // This exists just to hold a reference to a ReflectorImpl in a post task,
  // so the ReflectorImpl gets deleted when the function returns.
  static void DeleteOnMainThread(scoped_refptr<ReflectorImpl> reflector) {}

  MainThreadData& GetMain();
  ImplThreadData& GetImpl();

  // Must be accessed only on ImplThread, through GetImpl().
  ImplThreadData impl_unsafe_;

  // Must be accessed only on MainThread, through GetMain().
  MainThreadData main_unsafe_;

  // Can be accessed on both.
  scoped_refptr<base::MessageLoopProxy> impl_message_loop_;
  scoped_refptr<base::MessageLoopProxy> main_message_loop_;
  int surface_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_REFLECTOR_IMPL_H_
