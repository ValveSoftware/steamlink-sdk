// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "third_party/mesa/src/include/GL/osmesa.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_glx.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"

namespace gfx {

// This OSMesa GL surface can use XLib to swap the contents of the buffer to a
// view.
class NativeViewGLSurfaceOSMesa : public GLSurfaceOSMesa {
 public:
  explicit NativeViewGLSurfaceOSMesa(gfx::AcceleratedWidget window);

  static bool InitializeOneOff();

  // Implement a subset of GLSurface.
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool Resize(const gfx::Size& new_size) OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual bool SupportsPostSubBuffer() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;

 protected:
  virtual ~NativeViewGLSurfaceOSMesa();

 private:
  Display* xdisplay_;
  GC window_graphics_context_;
  gfx::AcceleratedWidget window_;
  GC pixmap_graphics_context_;
  Pixmap pixmap_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceOSMesa);
};

bool GLSurface::InitializeOneOffInternal() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      if (!GLSurfaceGLX::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceGLX::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationOSMesaGL:
      if (!NativeViewGLSurfaceOSMesa::InitializeOneOff()) {
        LOG(ERROR) << "NativeViewGLSurfaceOSMesa::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      break;
    default:
      break;
  }

  return true;
}

NativeViewGLSurfaceOSMesa::NativeViewGLSurfaceOSMesa(
    gfx::AcceleratedWidget window)
    : GLSurfaceOSMesa(OSMESA_BGRA, gfx::Size(1, 1)),
      xdisplay_(gfx::GetXDisplay()),
      window_graphics_context_(0),
      window_(window),
      pixmap_graphics_context_(0),
      pixmap_(0) {
  DCHECK(xdisplay_);
  DCHECK(window_);
}

// static
bool NativeViewGLSurfaceOSMesa::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  if (!gfx::GetXDisplay()) {
    LOG(ERROR) << "XOpenDisplay failed.";
    return false;
  }

  initialized = true;
  return true;
}

bool NativeViewGLSurfaceOSMesa::Initialize() {
  if (!GLSurfaceOSMesa::Initialize())
    return false;

  window_graphics_context_ = XCreateGC(xdisplay_, window_, 0, NULL);
  if (!window_graphics_context_) {
    LOG(ERROR) << "XCreateGC failed.";
    Destroy();
    return false;
  }

  return true;
}

void NativeViewGLSurfaceOSMesa::Destroy() {
  if (pixmap_graphics_context_) {
    XFreeGC(xdisplay_, pixmap_graphics_context_);
    pixmap_graphics_context_ = NULL;
  }

  if (pixmap_) {
    XFreePixmap(xdisplay_, pixmap_);
    pixmap_ = 0;
  }

  if (window_graphics_context_) {
    XFreeGC(xdisplay_, window_graphics_context_);
    window_graphics_context_ = NULL;
  }

  XSync(xdisplay_, False);
}

bool NativeViewGLSurfaceOSMesa::Resize(const gfx::Size& new_size) {
  if (!GLSurfaceOSMesa::Resize(new_size))
    return false;

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(xdisplay_, window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << window_ << ".";
    return false;
  }

  // Destroy the previous pixmap and graphics context.
  if (pixmap_graphics_context_) {
    XFreeGC(xdisplay_, pixmap_graphics_context_);
    pixmap_graphics_context_ = NULL;
  }
  if (pixmap_) {
    XFreePixmap(xdisplay_, pixmap_);
    pixmap_ = 0;
  }

  // Recreate a pixmap to hold the frame.
  pixmap_ = XCreatePixmap(xdisplay_,
                          window_,
                          new_size.width(),
                          new_size.height(),
                          attributes.depth);
  if (!pixmap_) {
    LOG(ERROR) << "XCreatePixmap failed.";
    return false;
  }

  // Recreate a graphics context for the pixmap.
  pixmap_graphics_context_ = XCreateGC(xdisplay_, pixmap_, 0, NULL);
  if (!pixmap_graphics_context_) {
    LOG(ERROR) << "XCreateGC failed";
    return false;
  }

  return true;
}

bool NativeViewGLSurfaceOSMesa::IsOffscreen() {
  return false;
}

bool NativeViewGLSurfaceOSMesa::SwapBuffers() {
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceOSMesa:RealSwapBuffers",
      "width", GetSize().width(),
      "height", GetSize().height());

  gfx::Size size = GetSize();

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(xdisplay_, window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << window_ << ".";
    return false;
  }

  // Copy the frame into the pixmap.
  gfx::PutARGBImage(xdisplay_,
                    attributes.visual,
                    attributes.depth,
                    pixmap_,
                    pixmap_graphics_context_,
                    static_cast<const uint8*>(GetHandle()),
                    size.width(),
                    size.height());

  // Copy the pixmap to the window.
  XCopyArea(xdisplay_,
            pixmap_,
            window_,
            window_graphics_context_,
            0,
            0,
            size.width(),
            size.height(),
            0,
            0);

  return true;
}

bool NativeViewGLSurfaceOSMesa::SupportsPostSubBuffer() {
  return true;
}

bool NativeViewGLSurfaceOSMesa::PostSubBuffer(
    int x, int y, int width, int height) {
  gfx::Size size = GetSize();

  // Move (0,0) from lower-left to upper-left
  y = size.height() - y - height;

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(xdisplay_, window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << window_ << ".";
    return false;
  }

  // Copy the frame into the pixmap.
  gfx::PutARGBImage(xdisplay_,
                    attributes.visual,
                    attributes.depth,
                    pixmap_,
                    pixmap_graphics_context_,
                    static_cast<const uint8*>(GetHandle()),
                    size.width(),
                    size.height(),
                    x,
                    y,
                    x,
                    y,
                    width,
                    height);

  // Copy the pixmap to the window.
  XCopyArea(xdisplay_,
            pixmap_,
            window_,
            window_graphics_context_,
            x,
            y,
            width,
            height,
            x,
            y);

  return true;
}

NativeViewGLSurfaceOSMesa::~NativeViewGLSurfaceOSMesa() {
  Destroy();
}

scoped_refptr<GLSurface> GLSurface::CreateViewGLSurface(
    gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "GLSurface::CreateViewGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(
          new NativeViewGLSurfaceOSMesa(window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new NativeViewGLSurfaceGLX(window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      DCHECK(window != gfx::kNullAcceleratedWidget);
      scoped_refptr<GLSurface> surface(new NativeViewGLSurfaceEGL(window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

scoped_refptr<GLSurface> GLSurface::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  TRACE_EVENT0("gpu", "GLSurface::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(new GLSurfaceOSMesa(OSMESA_RGBA,
                                                           size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceGLX(size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceEGL(size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  return gfx::GetXDisplay();
}

}  // namespace gfx
