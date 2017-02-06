// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_REFLECTOR_IMPL_H_
#define CONTENT_BROWSER_COMPOSITOR_REFLECTOR_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/id_map.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/reflector.h"
#include "ui/gfx/geometry/size.h"

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
class CONTENT_EXPORT ReflectorImpl
    : public base::SupportsWeakPtr<ReflectorImpl>,
      public ui::Reflector,
      public ui::CompositorObserver {
 public:
  ReflectorImpl(ui::Compositor* mirrored_compositor,
                ui::Layer* mirroring_layer);
  ~ReflectorImpl() override;

  ui::Compositor* mirrored_compositor() { return mirrored_compositor_; }

  void Shutdown();

  void DetachFromOutputSurface();

  // ui::Reflector:
  void OnMirroringCompositorResized() override;
  void AddMirroringLayer(ui::Layer* layer) override;
  void RemoveMirroringLayer(ui::Layer* layer) override;

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override {}
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override {}
  void OnCompositingAborted(ui::Compositor* compositor) override {}
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override {}
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {}

  // Called in |BrowserCompositorOutputSurface::SwapBuffers| to copy
  // the full screen image to the |mailbox_| texture.
  void OnSourceSwapBuffers();

  // Called in |BrowserCompositorOutputSurface::PostSubBuffer| copy
  // the sub image given by |rect| to the |mailbox_| texture.
  void OnSourcePostSubBuffer(const gfx::Rect& rect);

  // Called when the source surface is bound and available.
  void OnSourceSurfaceReady(BrowserCompositorOutputSurface* surface);

  // Called when the mailbox which has the source surface's texture
  // is updated.
  void OnSourceTextureMailboxUpdated(scoped_refptr<OwnedMailbox> mailbox);

 private:
  struct LayerData;

  ScopedVector<ReflectorImpl::LayerData>::iterator FindLayerData(
      ui::Layer* layer);
  void UpdateTexture(LayerData* layer_data,
                     const gfx::Size& size,
                     const gfx::Rect& redraw_rect);

  ui::Compositor* mirrored_compositor_;
  ScopedVector<LayerData> mirroring_layers_;

  scoped_refptr<OwnedMailbox> mailbox_;
  bool flip_texture_;
  int composition_count_;
  BrowserCompositorOutputSurface* output_surface_;
  base::Closure composition_started_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_REFLECTOR_IMPL_H_
