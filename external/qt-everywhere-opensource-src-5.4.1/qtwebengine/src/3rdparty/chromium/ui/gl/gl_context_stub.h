// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_STUB_H_
#define UI_GL_GL_CONTEXT_STUB_H_

#include "ui/gl/gl_context.h"

namespace gfx {

// A GLContext that does nothing for unit tests.
class GL_EXPORT GLContextStub : public GLContextReal {
 public:
  GLContextStub();

  // Implement GLContext.
  virtual bool Initialize(GLSurface* compatible_surface,
                          GpuPreference gpu_preference) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool MakeCurrent(GLSurface* surface) OVERRIDE;
  virtual void ReleaseCurrent(GLSurface* surface) OVERRIDE;
  virtual bool IsCurrent(GLSurface* surface) OVERRIDE;
  virtual void* GetHandle() OVERRIDE;
  virtual void SetSwapInterval(int interval) OVERRIDE;
  virtual std::string GetExtensions() OVERRIDE;
  virtual std::string GetGLRenderer() OVERRIDE;

 protected:
  virtual ~GLContextStub();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLContextStub);
};

}  // namespace gfx

#endif  // UI_GL_GL_CONTEXT_STUB_H_
