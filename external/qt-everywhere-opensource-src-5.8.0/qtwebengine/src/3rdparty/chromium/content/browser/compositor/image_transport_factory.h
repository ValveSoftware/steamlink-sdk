// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "content/common/content_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class SurfaceManager;
}

namespace gfx {
class Size;
enum class SwapResult;
}

namespace ui {
class Compositor;
class ContextFactory;
class ContextFactoryObserver;
class Texture;
}

namespace display_compositor {
class GLHelper;
}

namespace content {

// This class provides the interface for creating the support for the
// cross-process image transport, both for creating the shared surface handle
// (destination surface for the GPU process) and the transport client (logic for
// using that surface as a texture). The factory is a process-wide singleton.
class CONTENT_EXPORT ImageTransportFactory {
 public:
  virtual ~ImageTransportFactory() {}

  // Initializes the global transport factory.
  static void Initialize();

  // Initializes the global transport factory for unit tests using the provided
  // context factory.
  static void InitializeForUnitTests(
      std::unique_ptr<ImageTransportFactory> factory);

  // Terminates the global transport factory.
  static void Terminate();

  // Gets the factory instance.
  static ImageTransportFactory* GetInstance();

  // Gets the image transport factory as a context factory for the compositor.
  virtual ui::ContextFactory* GetContextFactory() = 0;

  virtual cc::SurfaceManager* GetSurfaceManager() = 0;

  // Gets a GLHelper instance, associated with the shared context. This
  // GLHelper will get destroyed whenever the shared context is lost
  // (ImageTransportFactoryObserver::OnLostResources is called).
  virtual display_compositor::GLHelper* GetGLHelper() = 0;

#if defined(OS_MACOSX)
  // Called with |suspended| as true when the ui::Compositor has been
  // disconnected from an NSView and may be attached to another one. Called
  // with |suspended| as false after the ui::Compositor has been connected to
  // a new NSView and the first commit targeted at the new NSView has
  // completed. This ensures that content and frames intended for the old
  // NSView will not flash in the new NSView.
  virtual void SetCompositorSuspendedForRecycle(ui::Compositor* compositor,
                                                bool suspended) = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_
