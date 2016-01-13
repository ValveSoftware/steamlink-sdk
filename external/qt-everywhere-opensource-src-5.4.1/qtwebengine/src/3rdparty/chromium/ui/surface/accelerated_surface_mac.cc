// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/accelerated_surface_mac.h"

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "ui/gfx/rect.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/scoped_make_current.h"

// Note that this must be included after gl_bindings.h to avoid conflicts.
#include <OpenGL/CGLIOSurface.h>

AcceleratedSurface::AcceleratedSurface()
    : io_surface_id_(0),
      allocate_fbo_(false),
      texture_(0),
      fbo_(0) {
}

AcceleratedSurface::~AcceleratedSurface() {}

bool AcceleratedSurface::Initialize(
    gfx::GLContext* share_context,
    bool allocate_fbo,
    gfx::GpuPreference gpu_preference) {
  allocate_fbo_ = allocate_fbo;

  // GL should be initialized by content::SupportsCoreAnimationPlugins().
  DCHECK_NE(gfx::GetGLImplementation(), gfx::kGLImplementationNone);

  // Drawing to IOSurfaces via OpenGL only works with Apple's GL and
  // not with the OSMesa software renderer.
  if (gfx::GetGLImplementation() != gfx::kGLImplementationDesktopGL &&
      gfx::GetGLImplementation() != gfx::kGLImplementationAppleGL)
    return false;

  gl_surface_ = gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size(1, 1));
  if (!gl_surface_.get()) {
    Destroy();
    return false;
  }

  gfx::GLShareGroup* share_group =
      share_context ? share_context->share_group() : NULL;

  gl_context_ = gfx::GLContext::CreateGLContext(
      share_group,
      gl_surface_.get(),
      gpu_preference);
  if (!gl_context_.get()) {
    Destroy();
    return false;
  }

  // Now we're ready to handle SetSurfaceSize calls, which will
  // allocate and/or reallocate the IOSurface and associated offscreen
  // OpenGL structures for rendering.
  return true;
}

void AcceleratedSurface::Destroy() {
  // The FBO and texture objects will be destroyed when the OpenGL context,
  // and any other contexts sharing resources with it, is. We don't want to
  // make the context current one last time here just in order to delete
  // these objects.
  gl_context_ = NULL;
  gl_surface_ = NULL;
}

// Call after making changes to the surface which require a visual update.
// Makes the rendering show up in other processes.
void AcceleratedSurface::SwapBuffers() {
  if (io_surface_.get() != NULL) {
    if (allocate_fbo_) {
      // Bind and unbind the framebuffer to make changes to the
      // IOSurface show up in the other process.
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
      glFlush();
    } else {
      // Copy the current framebuffer's contents into our "live" texture.
      // Note that the current GL context might not be ours at this point!
      // This is deliberate, so that surrounding code using GL can produce
      // rendering results consumed by the AcceleratedSurface.
      // Need to save and restore OpenGL state around this call.
      GLint current_texture = 0;
      GLenum target_binding = GL_TEXTURE_BINDING_RECTANGLE_ARB;
      GLenum target = GL_TEXTURE_RECTANGLE_ARB;
      glGetIntegerv(target_binding, &current_texture);
      glBindTexture(target, texture_);
      glCopyTexSubImage2D(target, 0,
                          0, 0,
                          0, 0,
                          real_surface_size_.width(),
                          real_surface_size_.height());
      glBindTexture(target, current_texture);
      // This flush is absolutely essential -- it guarantees that the
      // rendering results are seen by the other process.
      glFlush();
    }
  }
}

static void AddBooleanValue(CFMutableDictionaryRef dictionary,
                            const CFStringRef key,
                            bool value) {
  CFDictionaryAddValue(dictionary, key,
                       (value ? kCFBooleanTrue : kCFBooleanFalse));
}

static void AddIntegerValue(CFMutableDictionaryRef dictionary,
                            const CFStringRef key,
                            int32 value) {
  base::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(NULL, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}

// Creates a new OpenGL texture object bound to the given texture target.
// Caller owns the returned texture.
static GLuint CreateTexture(GLenum target) {
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(target, texture);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return texture;
}

void AcceleratedSurface::AllocateRenderBuffers(GLenum target,
                                               const gfx::Size& size) {
  if (!texture_) {
    // Generate the texture object.
    texture_ = CreateTexture(target);
    // Generate and bind the framebuffer object.
    glGenFramebuffersEXT(1, &fbo_);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
  }

  // Make sure that subsequent set-up code affects the render texture.
  glBindTexture(target, texture_);
}

bool AcceleratedSurface::SetupFrameBufferObject(GLenum target) {
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
  GLenum fbo_status;
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                            GL_COLOR_ATTACHMENT0_EXT,
                            target,
                            texture_,
                            0);
  fbo_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  return fbo_status == GL_FRAMEBUFFER_COMPLETE_EXT;
}

gfx::Size AcceleratedSurface::ClampToValidDimensions(const gfx::Size& size) {
  return gfx::Size(std::max(size.width(), 1), std::max(size.height(), 1));
}

bool AcceleratedSurface::MakeCurrent() {
  if (!gl_context_.get())
    return false;
  return gl_context_->MakeCurrent(gl_surface_.get());
}

void AcceleratedSurface::Clear(const gfx::Rect& rect) {
  DCHECK(gl_context_->IsCurrent(gl_surface_.get()));
  glClearColor(0, 0, 0, 0);
  glViewport(0, 0, rect.width(), rect.height());
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, rect.width(), 0, rect.height(), -1, 1);
  glClear(GL_COLOR_BUFFER_BIT);
}

uint32 AcceleratedSurface::SetSurfaceSize(const gfx::Size& size) {
  if (surface_size_ == size) {
    // Return 0 to indicate to the caller that no new backing store
    // allocation occurred.
    return 0;
  }

  // Only support IO surfaces if the GL implementation is the native desktop GL.
  // IO surfaces will not work with, for example, OSMesa software renderer
  // GL contexts.
  if (gfx::GetGLImplementation() != gfx::kGLImplementationDesktopGL)
    return 0;

  ui::ScopedMakeCurrent make_current(gl_context_.get(), gl_surface_.get());
  if (!make_current.Succeeded())
    return 0;

  gfx::Size clamped_size = ClampToValidDimensions(size);

  // GL_TEXTURE_RECTANGLE_ARB is the best supported render target on
  // Mac OS X and is required for IOSurface interoperability.
  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  if (allocate_fbo_) {
    AllocateRenderBuffers(target, clamped_size);
  } else if (!texture_) {
    // Generate the texture object.
    texture_ = CreateTexture(target);
  }

  // Allocate a new IOSurface, which is the GPU resource that can be
  // shared across processes.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> properties;
  properties.reset(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                             0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties, kIOSurfaceWidth, clamped_size.width());
  AddIntegerValue(properties, kIOSurfaceHeight, clamped_size.height());
  AddIntegerValue(properties, kIOSurfaceBytesPerElement, 4);
  AddBooleanValue(properties, kIOSurfaceIsGlobal, true);
  // I believe we should be able to unreference the IOSurfaces without
  // synchronizing with the browser process because they are
  // ultimately reference counted by the operating system.
  io_surface_.reset(IOSurfaceCreate(properties));

  // Don't think we need to identify a plane.
  GLuint plane = 0;
  CGLError error = CGLTexImageIOSurface2D(
      static_cast<CGLContextObj>(gl_context_->GetHandle()),
      target,
      GL_RGBA,
      clamped_size.width(),
      clamped_size.height(),
      GL_BGRA,
      GL_UNSIGNED_INT_8_8_8_8_REV,
      io_surface_.get(),
      plane);
  if (error != kCGLNoError) {
    DLOG(ERROR) << "CGL error " << error << " during CGLTexImageIOSurface2D";
  }
  if (allocate_fbo_) {
    // Set up the frame buffer object.
    if (!SetupFrameBufferObject(target)) {
      DLOG(ERROR) << "Failed to set up frame buffer object";
    }
  }
  surface_size_ = size;
  real_surface_size_ = clamped_size;

  // Now send back an identifier for the IOSurface. We originally
  // intended to send back a mach port from IOSurfaceCreateMachPort
  // but it looks like Chrome IPC would need to be modified to
  // properly send mach ports between processes. For the time being we
  // make our IOSurfaces global and send back their identifiers. On
  // the browser process side the identifier is reconstituted into an
  // IOSurface for on-screen rendering.
  io_surface_id_ = IOSurfaceGetID(io_surface_);
  return io_surface_id_;
}

uint32 AcceleratedSurface::GetSurfaceId() {
  return io_surface_id_;
}
