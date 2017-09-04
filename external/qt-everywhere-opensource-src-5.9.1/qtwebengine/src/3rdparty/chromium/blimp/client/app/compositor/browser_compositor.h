// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_COMPOSITOR_BROWSER_COMPOSITOR_H_
#define BLIMP_CLIENT_APP_COMPOSITOR_BROWSER_COMPOSITOR_H_

#include "base/macros.h"
#include "blimp/client/support/compositor/blimp_embedder_compositor.h"
#include "ui/gfx/native_widget_types.h"

namespace blimp {
namespace client {
class CompositorDependencies;

// A version of a BlimpEmbedderCompositor that supplies a ContextProvider backed
// by an AcceleratedWidget.  The AcceleratedWidget is a platform specific hook
// that the ContextProvider can use to render to the screen.
class BrowserCompositor : public BlimpEmbedderCompositor {
 public:
  explicit BrowserCompositor(CompositorDependencies* compositor_dependencies);
  ~BrowserCompositor() override;

  // Sets the AcceleratedWidget that will be used to display to.  Once provided,
  // the compositor will become visible and start drawing to the widget.  When
  // the widget is taken away by passing gfx::kNullAcceleratedWidget, the
  // compositor will become invisible and will drop all resources being used to
  // draw to the screen.
  void SetAcceleratedWidget(gfx::AcceleratedWidget widget);

  // A callback to get notifed when the compositor performs a successful swap.
  void set_did_complete_swap_buffers_callback(base::Closure callback) {
    did_complete_swap_buffers_ = callback;
  }

 protected:
  // BlimpEmbedderCompositor implementation.
  void DidReceiveCompositorFrameAck() override;

  base::Closure did_complete_swap_buffers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCompositor);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_COMPOSITOR_BROWSER_COMPOSITOR_H_
