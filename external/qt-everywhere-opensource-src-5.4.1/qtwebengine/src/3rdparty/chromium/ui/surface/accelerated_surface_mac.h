// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SURFACE_ACCELERATED_SURFACE_MAC_H_
#define UI_SURFACE_ACCELERATED_SURFACE_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurfaceAPI.h>

#include "base/callback.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"
#include "ui/surface/surface_export.h"

// Should not include GL headers in a header file. Forward declare these types
// instead.
typedef struct _CGLContextObject* CGLContextObj;
typedef unsigned int GLenum;
typedef unsigned int GLuint;

namespace gfx {
class Rect;
}

// Encapsulates an accelerated GL surface that can be shared across processes
// on systems that support it (10.6 and above).

class SURFACE_EXPORT AcceleratedSurface {
 public:
  AcceleratedSurface();
  virtual ~AcceleratedSurface();

  // Set up internal buffers. |share_context|, if non-NULL, is a context
  // with which the internally created OpenGL context shares textures and
  // other resources. |allocate_fbo| indicates whether or not this surface
  // should allocate an offscreen frame buffer object (FBO) internally. If
  // not, then the user is expected to allocate one. NOTE that allocating
  // an FBO internally does NOT work properly with client code which uses
  // OpenGL (i.e., via GLES2 command buffers), because the GLES2
  // implementation does not know to bind the accelerated surface's
  // internal FBO when the default FBO is bound. |gpu_preference| indicates
  // the GPU preference for the internally allocated GLContext. If
  // |share_context| is non-NULL, then on platforms supporting dual GPUs,
  // its GPU preference must match the passed one. Returns false upon
  // failure.
  bool Initialize(gfx::GLContext* share_context,
                  bool allocate_fbo,
                  gfx::GpuPreference gpu_preference);
  // Tear down. Must be called before destructor to prevent leaks.
  void Destroy();

  // These methods are used only once the accelerated surface is initialized.

  // Sets the accelerated surface to the given size, creating a new one if
  // the height or width changes. Returns a unique id of the IOSurface to
  // which the surface is bound, or 0 if no changes were made or an error
  // occurred. MakeCurrent() will have been called on the new surface.
  uint32 SetSurfaceSize(const gfx::Size& size);

  // Returns the id of this surface's IOSurface.
  uint32 GetSurfaceId();

  // Sets the GL context to be the current one for drawing. Returns true if
  // it succeeded.
  bool MakeCurrent();
  // Clear the surface to be transparent. Assumes the caller has already called
  // MakeCurrent().
  void Clear(const gfx::Rect& rect);
  // Call after making changes to the surface which require a visual update.
  // Makes the rendering show up in other processes. Assumes the caller has
  // already called MakeCurrent().
  //
  // If this AcceleratedSurface is configured with its own FBO, then
  // this call causes the color buffer to be transmitted. Otherwise,
  // it causes the frame buffer of the current GL context to be copied
  // into an internal texture via glCopyTexSubImage2D.
  //
  // The size of the rectangle copied is the size last specified via
  // SetSurfaceSize.  If another GL context than the one this
  // AcceleratedSurface contains is responsible for the production of
  // the pixels, then when this entry point is called, the color
  // buffer must be in a state where a glCopyTexSubImage2D is
  // legal. (For example, if using multisampled FBOs, the FBO must
  // have been resolved into a non-multisampled color texture.)
  // Additionally, in this situation, the contexts must share
  // server-side GL objects, so that this AcceleratedSurface's texture
  // is a legal name in the namespace of the current context.
  void SwapBuffers();

  CGLContextObj context() {
    return static_cast<CGLContextObj>(gl_context_->GetHandle());
  }

  // Get the accelerated surface size.
  gfx::Size GetSize() const { return surface_size_; }

 private:
  // Helper function to generate names for the backing texture and FBO.  On
  // return, the resulting names can be attached to |fbo_|.  |target| is
  // the target type for the color buffer.
  void AllocateRenderBuffers(GLenum target, const gfx::Size& size);

  // Helper function to attach the buffers previously allocated by a call to
  // AllocateRenderBuffers().  On return, |fbo_| can be used for
  // rendering.  |target| must be the same value as used in the call to
  // AllocateRenderBuffers().  Returns |true| if the resulting framebuffer
  // object is valid.
  bool SetupFrameBufferObject(GLenum target);

  gfx::Size ClampToValidDimensions(const gfx::Size& size);

  // The OpenGL context, and pbuffer drawable, used to transfer data
  // to the shared region (IOSurface).
  scoped_refptr<gfx::GLSurface> gl_surface_;
  scoped_refptr<gfx::GLContext> gl_context_;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;

  // The id of |io_surface_| or 0 if that's NULL.
  uint32 io_surface_id_;

  gfx::Size surface_size_;
  // It's important to avoid allocating zero-width or zero-height
  // IOSurfaces and textures on the Mac, so we clamp each to a minimum
  // of 1. This is the real size of the surface; surface_size_ is what
  // the user requested.
  gfx::Size real_surface_size_;
  // TODO(kbr): the FBO management should not be in this class at all.
  // However, if it is factored out, care needs to be taken to not
  // introduce another copy of the color data on the GPU; the direct
  // binding of the internal texture to the IOSurface saves a copy.
  bool allocate_fbo_;
  // This texture object is always allocated, regardless of whether
  // the user requests an FBO be allocated.
  GLuint texture_;
  // The FBO and renderbuffer are only allocated if allocate_fbo_ is
  // true.
  GLuint fbo_;
};

#endif  // UI_SURFACE_ACCELERATED_SURFACE_MAC_H_
