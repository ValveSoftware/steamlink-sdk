// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include <memory>

#include "gpu/ipc/service/child_window_surface_win.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/vsync_provider_win.h"

namespace gpu {

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    SurfaceHandle surface_handle,
    gl::GLSurface::Format format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  scoped_refptr<gl::GLSurface> surface;
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2 &&
      gl::GLSurfaceEGL::IsDirectCompositionSupported()) {
    scoped_refptr<ChildWindowSurfaceWin> egl_surface(
        new ChildWindowSurfaceWin(manager, surface_handle));
    surface = egl_surface;

    // TODO(jbauman): Get frame statistics from DirectComposition
    std::unique_ptr<gfx::VSyncProvider> vsync_provider(
        new gl::VSyncProviderWin(surface_handle));
    if (!egl_surface->Initialize(std::move(vsync_provider)))
      return nullptr;
  } else {
    surface = gl::init::CreateViewGLSurface(surface_handle);
    if (!surface)
      return nullptr;
  }

  return scoped_refptr<gl::GLSurface>(
      new PassThroughImageTransportSurface(manager, stub, surface.get()));
}

}  // namespace gpu
