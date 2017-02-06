// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_H_
#define UI_GL_GL_CONTEXT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/cancellation_flag.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_state_restorer.h"
#include "ui/gl/gpu_preference.h"

namespace gl {
class YUVToRGBConverter;
}  // namespace gl

namespace gpu {
class GLContextVirtual;
}  // namespace gpu

namespace gl {

class GLSurface;
class GPUTiming;
class GPUTimingClient;
class VirtualGLApi;
struct GLVersionInfo;

// Encapsulates an OpenGL context, hiding platform specific management.
class GL_EXPORT GLContext : public base::RefCounted<GLContext> {
 public:
  explicit GLContext(GLShareGroup* share_group);

  // Initializes the GL context to be compatible with the given surface. The GL
  // context can be made with other surface's of the same type. The compatible
  // surface is only needed for certain platforms like WGL, OSMesa and GLX. It
  // should be specific for all platforms though.
  virtual bool Initialize(
      GLSurface* compatible_surface, GpuPreference gpu_preference) = 0;

  // Makes the GL context and a surface current on the current thread.
  virtual bool MakeCurrent(GLSurface* surface) = 0;

  // Releases this GL context and surface as current on the current thread.
  virtual void ReleaseCurrent(GLSurface* surface) = 0;

  // Returns true if this context and surface is current. Pass a null surface
  // if the current surface is not important.
  virtual bool IsCurrent(GLSurface* surface) = 0;

  // Get the underlying platform specific GL context "handle".
  virtual void* GetHandle() = 0;

  // Creates a GPUTimingClient class which abstracts various GPU Timing exts.
  virtual scoped_refptr<GPUTimingClient> CreateGPUTimingClient() = 0;

  // Gets the GLStateRestorer for the context.
  GLStateRestorer* GetGLStateRestorer();

  // Sets the GLStateRestorer for the context (takes ownership).
  void SetGLStateRestorer(GLStateRestorer* state_restorer);

  // Set swap interval. This context must be current.
  void SetSwapInterval(int interval);

  // Forces the swap interval to zero (no vsync) regardless of any future values
  // passed to SetSwapInterval.
  void ForceSwapIntervalZero(bool force);

  // Returns space separated list of extensions. The context must be current.
  virtual std::string GetExtensions();

  // Indicate that it is safe to force this context to switch GPUs, since
  // transitioning can cause corruption and hangs (OS X only).
  virtual void SetSafeToForceGpuSwitch();

  // Attempt to force the context to move to the GPU of its sharegroup. Return
  // false only in the event of an unexpected error on the context.
  virtual bool ForceGpuSwitchIfNeeded();

  // Indicate that the real context switches should unbind the FBO first
  // (For an Android work-around only).
  virtual void SetUnbindFboOnMakeCurrent();

  // Returns whether the current context supports the named extension. The
  // context must be current.
  bool HasExtension(const char* name);

  // Returns version info of the underlying GL context. The context must be
  // current.
  const GLVersionInfo* GetVersionInfo();

  GLShareGroup* share_group();

  // Create a GL context that is compatible with the given surface.
  // |share_group|, if non-NULL, is a group of contexts which the
  // internally created OpenGL context shares textures and other resources.
  // DEPRECATED(kylechar): Use gl::init::CreateGLContext from gl_factory.h.
  static scoped_refptr<GLContext> CreateGLContext(
      GLShareGroup* share_group,
      GLSurface* compatible_surface,
      GpuPreference gpu_preference);

  static bool LosesAllContextsOnContextLost();

  // Returns the last GLContext made current, virtual or real.
  static GLContext* GetCurrent();

  virtual bool WasAllocatedUsingRobustnessExtension();

  // Use this context for virtualization.
  void SetupForVirtualization();

  // Make this context current when used for context virtualization.
  bool MakeVirtuallyCurrent(GLContext* virtual_context, GLSurface* surface);

  // Notify this context that |virtual_context|, that was using us, is
  // being released or destroyed.
  void OnReleaseVirtuallyCurrent(GLContext* virtual_context);

  // Returns the GL version string. The context must be current.
  virtual std::string GetGLVersion();

  // Returns the GL renderer string. The context must be current.
  virtual std::string GetGLRenderer();

  // Returns a helper structure to convert YUV textures to RGB textures.
  virtual YUVToRGBConverter* GetYUVToRGBConverter();

 protected:
  virtual ~GLContext();

  // Will release the current context when going out of scope, unless canceled.
  class ScopedReleaseCurrent {
   public:
    ScopedReleaseCurrent();
    ~ScopedReleaseCurrent();

    void Cancel();

   private:
    bool canceled_;
  };

  // Sets the GL api to the real hardware API (vs the VirtualAPI)
  static void SetRealGLApi();
  virtual void SetCurrent(GLSurface* surface);

  // Initialize function pointers to functions where the bound version depends
  // on GL version or supported extensions. Should be called immediately after
  // this context is made current.
  bool InitializeDynamicBindings();

  // Returns the last real (non-virtual) GLContext made current.
  static GLContext* GetRealCurrent();

  virtual void OnSetSwapInterval(int interval) = 0;

 private:
  friend class base::RefCounted<GLContext>;

  // For GetRealCurrent.
  friend class VirtualGLApi;
  friend class gpu::GLContextVirtual;

  scoped_refptr<GLShareGroup> share_group_;
  std::unique_ptr<VirtualGLApi> virtual_gl_api_;
  bool state_dirtied_externally_;
  std::unique_ptr<GLStateRestorer> state_restorer_;
  std::unique_ptr<GLVersionInfo> version_info_;

  int swap_interval_;
  bool force_swap_interval_zero_;

  DISALLOW_COPY_AND_ASSIGN(GLContext);
};

class GL_EXPORT GLContextReal : public GLContext {
 public:
  explicit GLContextReal(GLShareGroup* share_group);
  scoped_refptr<GPUTimingClient> CreateGPUTimingClient() override;

 protected:
  ~GLContextReal() override;

  void SetCurrent(GLSurface* surface) override;

 private:
  std::unique_ptr<GPUTiming> gpu_timing_;
  DISALLOW_COPY_AND_ASSIGN(GLContextReal);
};

// Wraps GLContext in scoped_refptr and tries to initializes it. Returns a
// scoped_refptr containing the initialized GLContext or nullptr if
// initialization fails.
GL_EXPORT scoped_refptr<GLContext> InitializeGLContext(
    scoped_refptr<GLContext> context,
    GLSurface* compatible_surface,
    GpuPreference gpu_preference);

}  // namespace gl

#endif  // UI_GL_GL_CONTEXT_H_
