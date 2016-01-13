// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_H_
#define UI_GL_GL_SURFACE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_implementation.h"

namespace gfx {

class GLContext;
class VSyncProvider;

// Encapsulates a surface that can be rendered to with GL, hiding platform
// specific management.
class GL_EXPORT GLSurface : public base::RefCounted<GLSurface> {
 public:
  GLSurface();

  // (Re)create the surface. TODO(apatrick): This is an ugly hack to allow the
  // EGL surface associated to be recreated without destroying the associated
  // context. The implementation of this function for other GLSurface derived
  // classes is in a pending changelist.
  virtual bool Initialize();

  // Destroys the surface.
  virtual void Destroy() = 0;

  virtual bool Resize(const gfx::Size& size);

  // Recreate the surface without changing the size.
  virtual bool Recreate();

  // Unschedule the GpuScheduler and return true to abort the processing of
  // a GL draw call to this surface and defer it until the GpuScheduler is
  // rescheduled.
  virtual bool DeferDraws();

  // Returns true if this surface is offscreen.
  virtual bool IsOffscreen() = 0;

  // Swaps front and back buffers. This has no effect for off-screen
  // contexts.
  virtual bool SwapBuffers() = 0;

  // Get the size of the surface.
  virtual gfx::Size GetSize() = 0;

  // Get the underlying platform specific surface "handle".
  virtual void* GetHandle() = 0;

  // Returns whether or not the surface supports PostSubBuffer.
  virtual bool SupportsPostSubBuffer();

  // Returns the internal frame buffer object name if the surface is backed by
  // FBO. Otherwise returns 0.
  virtual unsigned int GetBackingFrameBufferObject();

  // Copy part of the backbuffer to the frontbuffer.
  virtual bool PostSubBuffer(int x, int y, int width, int height);

  // Initialize GL bindings.
  static bool InitializeOneOff();

  // Unit tests should call these instead of InitializeOneOff() to set up
  // GL bindings appropriate for tests.
  static void InitializeOneOffForTests();
  static void InitializeOneOffWithMockBindingsForTests();
  static void InitializeDynamicMockBindingsForTests(GLContext* context);

  // Called after a context is made current with this surface. Returns false
  // on error.
  virtual bool OnMakeCurrent(GLContext* context);

  // Used for explicit buffer management.
  virtual bool SetBackbufferAllocation(bool allocated);
  virtual void SetFrontbufferAllocation(bool allocated);

  // Get a handle used to share the surface with another process. Returns null
  // if this is not possible.
  virtual void* GetShareHandle();

  // Get the platform specific display on which this surface resides, if
  // available.
  virtual void* GetDisplay();

  // Get the platfrom specific configuration for this surface, if available.
  virtual void* GetConfig();

  // Get the GL pixel format of the surface, if available.
  virtual unsigned GetFormat();

  // Get access to a helper providing time of recent refresh and period
  // of screen refresh. If unavailable, returns NULL.
  virtual VSyncProvider* GetVSyncProvider();

  // Create a GL surface that renders directly to a view.
  static scoped_refptr<GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window);

  // Create a GL surface used for offscreen rendering.
  static scoped_refptr<GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size);

  static GLSurface* GetCurrent();

 protected:
  virtual ~GLSurface();
  static bool InitializeOneOffImplementation(GLImplementation impl,
                                             bool fallback_to_osmesa,
                                             bool gpu_service_logging,
                                             bool disable_gl_drawing);
  static bool InitializeOneOffInternal();
  static void SetCurrent(GLSurface* surface);

  static bool ExtensionsContain(const char* extensions, const char* name);

 private:
  friend class base::RefCounted<GLSurface>;
  friend class GLContext;

  DISALLOW_COPY_AND_ASSIGN(GLSurface);
};

// Implementation of GLSurface that forwards all calls through to another
// GLSurface.
class GL_EXPORT GLSurfaceAdapter : public GLSurface {
 public:
  explicit GLSurfaceAdapter(GLSurface* surface);

  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool Resize(const gfx::Size& size) OVERRIDE;
  virtual bool Recreate() OVERRIDE;
  virtual bool DeferDraws() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual bool SupportsPostSubBuffer() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void* GetHandle() OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;
  virtual bool OnMakeCurrent(GLContext* context) OVERRIDE;
  virtual bool SetBackbufferAllocation(bool allocated) OVERRIDE;
  virtual void SetFrontbufferAllocation(bool allocated) OVERRIDE;
  virtual void* GetShareHandle() OVERRIDE;
  virtual void* GetDisplay() OVERRIDE;
  virtual void* GetConfig() OVERRIDE;
  virtual unsigned GetFormat() OVERRIDE;
  virtual VSyncProvider* GetVSyncProvider() OVERRIDE;

  GLSurface* surface() const { return surface_.get(); }

 protected:
  virtual ~GLSurfaceAdapter();

 private:
  scoped_refptr<GLSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceAdapter);
};

}  // namespace gfx

#endif  // UI_GL_GL_SURFACE_H_
