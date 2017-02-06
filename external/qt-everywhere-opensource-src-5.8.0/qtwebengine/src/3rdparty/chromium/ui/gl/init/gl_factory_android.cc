// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_osmesa.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"

namespace gl {
namespace init {

namespace {

// Used to render into an already current context+surface,
// that we do not have ownership of (draw callback).
// TODO(boliu): Make this inherit from GLContextEGL.
class GLNonOwnedContext : public GLContextReal {
 public:
  explicit GLNonOwnedContext(GLShareGroup* share_group);

  // Implement GLContext.
  bool Initialize(GLSurface* compatible_surface,
                  GpuPreference gpu_preference) override;
  bool MakeCurrent(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override {}
  bool IsCurrent(GLSurface* surface) override { return true; }
  void* GetHandle() override { return nullptr; }
  void OnSetSwapInterval(int interval) override {}
  std::string GetExtensions() override;

 protected:
  ~GLNonOwnedContext() override {}

 private:
  EGLDisplay display_;

  DISALLOW_COPY_AND_ASSIGN(GLNonOwnedContext);
};

GLNonOwnedContext::GLNonOwnedContext(GLShareGroup* share_group)
    : GLContextReal(share_group), display_(nullptr) {}

bool GLNonOwnedContext::Initialize(GLSurface* compatible_surface,
                                   GpuPreference gpu_preference) {
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  return true;
}

bool GLNonOwnedContext::MakeCurrent(GLSurface* surface) {
  SetCurrent(surface);
  SetRealGLApi();
  return true;
}

std::string GLNonOwnedContext::GetExtensions() {
  const char* extensions = eglQueryString(display_, EGL_EXTENSIONS);
  if (!extensions)
    return GLContext::GetExtensions();

  return GLContext::GetExtensions() + " " + extensions;
}

}  // namespace

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         GpuPreference gpu_preference) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");
  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
      return scoped_refptr<GLContext>(new GLContextStub(share_group));
    case kGLImplementationOSMesaGL:
      return InitializeGLContext(new GLContextOSMesa(share_group),
                                 compatible_surface, gpu_preference);
    default:
      if (compatible_surface->GetHandle()) {
        return InitializeGLContext(new GLContextEGL(share_group),
                                   compatible_surface, gpu_preference);
      } else {
        return InitializeGLContext(new GLNonOwnedContext(share_group),
                                   compatible_surface, gpu_preference);
      }
  }
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");
  CHECK_NE(kGLImplementationNone, GetGLImplementation());
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL:
      return InitializeGLSurface(new GLSurfaceOSMesaHeadless());
    case kGLImplementationEGLGLES2:
      if (window != gfx::kNullAcceleratedWidget) {
        return InitializeGLSurface(new NativeViewGLSurfaceEGL(window));
      } else {
        return InitializeGLSurface(new GLSurfaceStub());
      }
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateOffscreenGLSurface(const gfx::Size& size) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");
  CHECK_NE(kGLImplementationNone, GetGLImplementation());
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      return InitializeGLSurface(
          new GLSurfaceOSMesa(GLSurface::SURFACE_OSMESA_BGRA, size));
    }
    case kGLImplementationEGLGLES2: {
      scoped_refptr<GLSurface> surface;
      if (GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
          (size.width() == 0 && size.height() == 0)) {
        return InitializeGLSurface(new SurfacelessEGL(size));
      } else {
        return InitializeGLSurface(new PbufferGLSurfaceEGL(size));
      }
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace init
}  // namespace gl
