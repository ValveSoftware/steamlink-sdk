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
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    SurfaceHandle surface_handle,
    gl::GLSurface::Format format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  scoped_refptr<gl::GLSurface> surface;
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2 &&
      gl::GLSurfaceEGL::IsDirectCompositionSupported()) {
    scoped_refptr<ChildWindowSurfaceWin> egl_surface(
        new ChildWindowSurfaceWin(delegate, surface_handle));
    surface = egl_surface;

    // TODO(stanisc): http://crbug.com/659844:
    // Force DWM based gl::VSyncProviderWin provider to avoid video playback
    // smoothness issues. Once that issue is fixed, passing a nullptr
    // vsync_provider would result in assigning a default VSyncProvider inside
    // the Initialize call.
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
      new PassThroughImageTransportSurface(delegate, surface.get()));
}

}  // namespace gpu
