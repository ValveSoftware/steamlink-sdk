// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_EGL_H_
#define UI_GL_GL_CONTEXT_EGL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_export.h"

typedef void* EGLContext;
typedef void* EGLDisplay;
typedef void* EGLConfig;

namespace gl {

class GLSurface;

// Encapsulates an EGL OpenGL ES context.
class GL_EXPORT GLContextEGL : public GLContextReal {
 public:
  explicit GLContextEGL(GLShareGroup* share_group);

  // Implement GLContext.
  bool Initialize(GLSurface* compatible_surface,
                  GpuPreference gpu_preference) override;
  bool MakeCurrent(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override;
  bool IsCurrent(GLSurface* surface) override;
  void* GetHandle() override;
  void OnSetSwapInterval(int interval) override;
  std::string GetExtensions() override;
  bool WasAllocatedUsingRobustnessExtension() override;
  void SetUnbindFboOnMakeCurrent() override;

 protected:
  ~GLContextEGL() override;

 private:
  void Destroy();

  EGLContext context_;
  EGLDisplay display_;
  EGLConfig config_;
  bool unbind_fbo_on_makecurrent_;
  int swap_interval_;

  DISALLOW_COPY_AND_ASSIGN(GLContextEGL);
};

}  // namespace gl

#endif  // UI_GL_GL_CONTEXT_EGL_H_
