// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_IO_SURFACE_H_
#define UI_GL_GL_IMAGE_IO_SURFACE_H_

#include <IOSurface/IOSurfaceAPI.h>

#include "base/mac/scoped_cftyperef.h"
#include "ui/gl/gl_image.h"

namespace gfx {

class GL_EXPORT GLImageIOSurface : public GLImage {
 public:
  explicit GLImageIOSurface(gfx::Size size);

  bool Initialize(gfx::GpuMemoryBufferHandle buffer);

  // Overridden from GLImage:
  virtual void Destroy() OVERRIDE {}
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void ReleaseTexImage(unsigned target) OVERRIDE {}
  virtual void WillUseTexImage() OVERRIDE {}
  virtual void DidUseTexImage() OVERRIDE {}
  virtual void WillModifyTexImage() OVERRIDE {}
  virtual void DidModifyTexImage() OVERRIDE {}

 protected:
  virtual ~GLImageIOSurface();

 private:
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(GLImageIOSurface);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_IO_SURFACE_H_
