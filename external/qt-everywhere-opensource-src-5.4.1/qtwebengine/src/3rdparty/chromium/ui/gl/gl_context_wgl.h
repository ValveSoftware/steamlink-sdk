// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_WGL_H_
#define UI_GL_GL_CONTEXT_WGL_H_

#include <string>

#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"

namespace gfx {

class GLSurface;

// This class is a wrapper around a GL context.
class GLContextWGL : public GLContextReal {
 public:
  explicit GLContextWGL(GLShareGroup* share_group);
  virtual ~GLContextWGL();

  // Implement GLContext.
  virtual bool Initialize(
      GLSurface* compatible_surface, GpuPreference gpu_preference);
  virtual void Destroy();
  virtual bool MakeCurrent(GLSurface* surface);
  virtual void ReleaseCurrent(GLSurface* surface);
  virtual bool IsCurrent(GLSurface* surface);
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);
  virtual std::string GetExtensions();

 private:
  HGLRC context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextWGL);
};

}  // namespace gfx

#endif  // UI_GL_GL_CONTEXT_WGL_H_
