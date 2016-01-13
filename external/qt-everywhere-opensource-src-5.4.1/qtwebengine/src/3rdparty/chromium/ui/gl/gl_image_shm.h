// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_SHM_H_
#define UI_GL_GL_IMAGE_SHM_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

namespace gfx {

class GL_EXPORT GLImageShm : public GLImage {
 public:
  GLImageShm(gfx::Size size, unsigned internalformat);

  bool Initialize(gfx::GpuMemoryBufferHandle buffer);

  // Overridden from GLImage:
  virtual void Destroy() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void ReleaseTexImage(unsigned target) OVERRIDE {}
  virtual void WillUseTexImage() OVERRIDE {}
  virtual void DidUseTexImage() OVERRIDE {}
  virtual void WillModifyTexImage() OVERRIDE {}
  virtual void DidModifyTexImage() OVERRIDE {}

 protected:
  virtual ~GLImageShm();

 private:
  scoped_ptr<base::SharedMemory> shared_memory_;
  gfx::Size size_;
  unsigned internalformat_;
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  GLuint egl_texture_id_;
  EGLImageKHR egl_image_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GLImageShm);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_SHM_H_
