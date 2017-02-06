// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_STUB_H_
#define UI_GL_GL_SURFACE_STUB_H_

#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"

namespace gl {

// A GLSurface that does nothing for unit tests.
class GL_EXPORT GLSurfaceStub : public GLSurface {
 public:
  void SetSize(const gfx::Size& size) { size_ = size; }
  void set_buffers_flipped(bool flipped) { buffers_flipped_ = flipped; }

  // Implement GLSurface.
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers() override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  bool BuffersFlipped() const override;

 protected:
  ~GLSurfaceStub() override;

 private:
  gfx::Size size_;
  bool buffers_flipped_ = false;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_STUB_H_
