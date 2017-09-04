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
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/ozone_util.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gl {
namespace init {

namespace {

bool HasDefaultImplementation(GLImplementation impl) {
  return impl == kGLImplementationOSMesaGL || impl == kGLImplementationMockGL;
}

scoped_refptr<GLSurface> CreateDefaultViewGLSurface(
    gfx::AcceleratedWidget window) {
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL:
      return InitializeGLSurface(new GLSurfaceOSMesaHeadless());
    case kGLImplementationMockGL:
      return InitializeGLSurface(new GLSurfaceStub());
    default:
      NOTREACHED();
  }
  return nullptr;
}

scoped_refptr<GLSurface> CreateDefaultOffscreenGLSurface(
    const gfx::Size& size) {
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL:
      return InitializeGLSurface(
          new GLSurfaceOSMesa(GLSurface::SURFACE_OSMESA_BGRA, size));
    case kGLImplementationMockGL:
      return InitializeGLSurface(new GLSurfaceStub);
    default:
      NOTREACHED();
  }
  return nullptr;
}

}  // namespace

std::vector<GLImplementation> GetAllowedGLImplementations() {
  ui::OzonePlatform::InitializeForGPU();
  return GetSurfaceFactoryOzone()->GetAllowedGLImplementations();
}

bool GetGLWindowSystemBindingInfo(GLWindowSystemBindingInfo* info) {
  if (HasGLOzone())
    return GetGLOzone()->GetGLWindowSystemBindingInfo(info);

  // TODO(kylechar): This is deprecated and can be removed once all Ozone
  // platforms use GLOzone instead.
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
      return GetGLWindowSystemBindingInfoEGL(info);
    default:
      return false;
  }
}

#if !defined(TOOLKIT_QT)
scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");

  if (HasGLOzone()) {
    return GetGLOzone()->CreateGLContext(share_group, compatible_surface,
                                         attribs);
  }

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
      return scoped_refptr<GLContext>(new GLContextStub(share_group));
    case kGLImplementationOSMesaGL:
      return InitializeGLContext(new GLContextOSMesa(share_group),
                                 compatible_surface, attribs);
    case kGLImplementationEGLGLES2:
      return InitializeGLContext(new GLContextEGL(share_group),
                                 compatible_surface, attribs);
    default:
      NOTREACHED();
  }
  return nullptr;
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");

  if (HasGLOzone())
    return GetGLOzone()->CreateViewGLSurface(window);

  if (HasDefaultImplementation(GetGLImplementation()))
    return CreateDefaultViewGLSurface(window);

  // TODO(kylechar): This is deprecated and can be removed once all Ozone
  // platforms use GLOzone instead.
  return GetSurfaceFactoryOzone()->CreateViewGLSurface(GetGLImplementation(),
                                                       window);
}

scoped_refptr<GLSurface> CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateSurfacelessViewGLSurface");

  if (HasGLOzone())
    return GetGLOzone()->CreateSurfacelessViewGLSurface(window);

  // TODO(kylechar): This is deprecated and can be removed once all Ozone
  // platforms use GLOzone instead.
  return GetSurfaceFactoryOzone()->CreateSurfacelessViewGLSurface(
      GetGLImplementation(), window);
}

scoped_refptr<GLSurface> CreateOffscreenGLSurface(const gfx::Size& size) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");

  if (HasGLOzone())
    return GetGLOzone()->CreateOffscreenGLSurface(size);

  if (HasDefaultImplementation(GetGLImplementation()))
    return CreateDefaultOffscreenGLSurface(size);

  // TODO(kylechar): This is deprecated and can be removed once all Ozone
  // platforms use GLOzone instead.
  return GetSurfaceFactoryOzone()->CreateOffscreenGLSurface(
      GetGLImplementation(), size);
}
#endif

}  // namespace init
}  // namespace gl
