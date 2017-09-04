// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_
#define UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_

#include <memory>

#include "base/macros.h"
#include "ui/ozone/demo/gl_renderer.h"

namespace gl {
class GLImage;
}

namespace ui {

class SurfacelessGlRenderer : public GlRenderer {
 public:
  SurfacelessGlRenderer(gfx::AcceleratedWidget widget,
                        const scoped_refptr<gl::GLSurface>& surface,
                        const gfx::Size& size);
  ~SurfacelessGlRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  // GlRenderer:
  void RenderFrame() override;
  void PostRenderFrameTask(gfx::SwapResult result) override;

  class BufferWrapper {
   public:
    BufferWrapper();
    ~BufferWrapper();

    gl::GLImage* image() const { return image_.get(); }

    bool Initialize(gfx::AcceleratedWidget widget, const gfx::Size& size);
    void BindFramebuffer();

   private:
    gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
    gfx::Size size_;

    scoped_refptr<gl::GLImage> image_;
    unsigned int gl_fb_ = 0;
    unsigned int gl_tex_ = 0;
  };

  std::unique_ptr<BufferWrapper> buffers_[2];
  int back_buffer_ = 0;

  base::WeakPtrFactory<SurfacelessGlRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfacelessGlRenderer);
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_
