// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_osmesa.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_context_wgl.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_osmesa_win.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_surface_wgl.h"
#include "ui/gl/vsync_provider_win.h"

namespace gl {
namespace init {

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         GpuPreference gpu_preference) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL:
      return InitializeGLContext(new GLContextOSMesa(share_group),
                                 compatible_surface, gpu_preference);
    case kGLImplementationEGLGLES2:
      return InitializeGLContext(new GLContextEGL(share_group),
                                 compatible_surface, gpu_preference);
    case kGLImplementationDesktopGL:
      return InitializeGLContext(new GLContextWGL(share_group),
                                 compatible_surface, gpu_preference);
    case kGLImplementationMockGL:
      return new GLContextStub(share_group);
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL:
      return InitializeGLSurface(new GLSurfaceOSMesaWin(window));
    case kGLImplementationEGLGLES2: {
      DCHECK(window != gfx::kNullAcceleratedWidget);
      scoped_refptr<NativeViewGLSurfaceEGL> surface(
          new NativeViewGLSurfaceEGL(window));
      std::unique_ptr<gfx::VSyncProvider> sync_provider;
      sync_provider.reset(new VSyncProviderWin(window));
      if (!surface->Initialize(std::move(sync_provider)))
        return nullptr;

      return surface;
    }
    case kGLImplementationDesktopGL:
      return InitializeGLSurface(new NativeViewGLSurfaceWGL(window));
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateOffscreenGLSurface(const gfx::Size& size) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL:
      return InitializeGLSurface(
          new GLSurfaceOSMesa(GLSurface::SURFACE_OSMESA_RGBA, size));
    case kGLImplementationEGLGLES2:
      return InitializeGLSurface(new PbufferGLSurfaceEGL(size));
    case kGLImplementationDesktopGL:
      return InitializeGLSurface(new PbufferGLSurfaceWGL(size));
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace init
}  // namespace gl
