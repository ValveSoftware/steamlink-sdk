// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {
#include <X11/extensions/Xcomposite.h>
}

#include "ui/gl/gl_image_glx.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_glx.h"

namespace gfx {

namespace {

// scoped_ptr functor for XFree(). Use as follows:
//   scoped_ptr<XVisualInfo, ScopedPtrXFree> foo(...);
// where "XVisualInfo" is any X type that is freed with XFree.
struct ScopedPtrXFree {
  void operator()(void* x) const { ::XFree(x); }
};

int BindToTextureFormat(int depth) {
  if (depth == 32)
    return GLX_BIND_TO_TEXTURE_RGBA_EXT;

  return GLX_BIND_TO_TEXTURE_RGB_EXT;
}

int TextureFormat(int depth) {
  if (depth == 32)
    return GLX_TEXTURE_FORMAT_RGBA_EXT;

  return GLX_TEXTURE_FORMAT_RGB_EXT;
}

}  // namespace anonymous

GLImageGLX::GLImageGLX(gfx::PluginWindowHandle window)
    : display_(gfx::GetXDisplay()),
      window_(window),
      pixmap_(0),
      glx_pixmap_(0) {}

GLImageGLX::~GLImageGLX() { Destroy(); }

bool GLImageGLX::Initialize() {
  if (!GLSurfaceGLX::IsTextureFromPixmapSupported()) {
    LOG(ERROR) << "GLX_EXT_texture_from_pixmap not supported.";
    return false;
  }

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(display_, window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << window_ << ".";
    return false;
  }

  XVisualInfo templ;
  templ.visualid = XVisualIDFromVisual(attributes.visual);
  int num_visinfo = 0;
  scoped_ptr<XVisualInfo, ScopedPtrXFree> visinfo(
      XGetVisualInfo(display_, VisualIDMask, &templ, &num_visinfo));
  if (!visinfo.get()) {
    LOG(ERROR) << "XGetVisualInfo failed for visual id " << templ.visualid
               << ".";
    return false;
  }
  if (!num_visinfo) {
    LOG(ERROR) << "XGetVisualInfo returned 0 elements.";
    return false;
  }

  int config_attribs[] = {
      static_cast<int>(GLX_VISUAL_ID),     static_cast<int>(visinfo->visualid),
      GLX_DRAWABLE_TYPE,                   GLX_PIXMAP_BIT,
      GLX_BIND_TO_TEXTURE_TARGETS_EXT,     GLX_TEXTURE_2D_EXT,
      BindToTextureFormat(visinfo->depth), GL_TRUE,
      0};
  int num_elements = 0;
  scoped_ptr<GLXFBConfig, ScopedPtrXFree> config(glXChooseFBConfig(
      display_, DefaultScreen(display_), config_attribs, &num_elements));
  if (!config.get()) {
    LOG(ERROR) << "glXChooseFBConfig failed.";
    return false;
  }
  if (!num_elements) {
    LOG(ERROR) << "glXChooseFBConfig returned 0 elements.";
    return false;
  }

  // Create backing pixmap reference.
  pixmap_ = XCompositeNameWindowPixmap(display_, window_);

  XID root = 0;
  int x = 0;
  int y = 0;
  unsigned int width = 0;
  unsigned int height = 0;
  unsigned int bw = 0;
  unsigned int depth = 0;
  if (!XGetGeometry(
          display_, pixmap_, &root, &x, &y, &width, &height, &bw, &depth)) {
    LOG(ERROR) << "XGetGeometry failed for pixmap " << pixmap_ << ".";
    return false;
  }

  int pixmap_attribs[] = {GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
                          GLX_TEXTURE_FORMAT_EXT, TextureFormat(visinfo->depth),
                          0};
  glx_pixmap_ =
      glXCreatePixmap(display_, *config.get(), pixmap_, pixmap_attribs);
  if (!glx_pixmap_) {
    LOG(ERROR) << "glXCreatePixmap failed.";
    return false;
  }

  size_ = gfx::Size(width, height);
  return true;
}

void GLImageGLX::Destroy() {
  if (glx_pixmap_) {
    glXDestroyGLXPixmap(display_, glx_pixmap_);
    glx_pixmap_ = 0;
  }
  if (pixmap_) {
    XFreePixmap(display_, pixmap_);
    pixmap_ = 0;
  }
}

gfx::Size GLImageGLX::GetSize() { return size_; }

bool GLImageGLX::BindTexImage(unsigned target) {
  if (!glx_pixmap_)
    return false;

  // Requires TEXTURE_2D target.
  if (target != GL_TEXTURE_2D)
    return false;

  glXBindTexImageEXT(display_, glx_pixmap_, GLX_FRONT_LEFT_EXT, 0);
  return true;
}

void GLImageGLX::ReleaseTexImage(unsigned target) {
  DCHECK(glx_pixmap_);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), target);

  glXReleaseTexImageEXT(display_, glx_pixmap_, GLX_FRONT_LEFT_EXT);
}

}  // namespace gfx
