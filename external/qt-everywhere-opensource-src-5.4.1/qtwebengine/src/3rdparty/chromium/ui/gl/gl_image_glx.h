// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_GLX_H_
#define UI_GL_GL_IMAGE_GLX_H_

#include "ui/gfx/size.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gfx {

class GL_EXPORT GLImageGLX : public GLImage {
 public:
  explicit GLImageGLX(gfx::PluginWindowHandle window);

  virtual bool Initialize();

  // Overridden from GLImage:
  virtual void Destroy() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void ReleaseTexImage(unsigned target) OVERRIDE;
  virtual void WillUseTexImage() OVERRIDE {}
  virtual void DidUseTexImage() OVERRIDE {}
  virtual void WillModifyTexImage() OVERRIDE {}
  virtual void DidModifyTexImage() OVERRIDE {}

 protected:
  virtual ~GLImageGLX();

 private:
  XDisplay* display_;
  gfx::PluginWindowHandle window_;
  XID pixmap_;
  XID glx_pixmap_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(GLImageGLX);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_GLX_H_
