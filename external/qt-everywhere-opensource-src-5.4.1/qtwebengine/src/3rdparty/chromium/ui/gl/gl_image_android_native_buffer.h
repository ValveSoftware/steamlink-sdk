// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_ANDROID_NATIVE_BUFFER_H_
#define UI_GL_GL_IMAGE_ANDROID_NATIVE_BUFFER_H_

#include "ui/gl/gl_image_egl.h"

namespace gfx {

class GL_EXPORT GLImageAndroidNativeBuffer : public GLImageEGL {
 public:
  explicit GLImageAndroidNativeBuffer(gfx::Size size);

  bool Initialize(gfx::GpuMemoryBufferHandle buffer);

  // Overridden from GLImage:
  virtual void Destroy() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void WillUseTexImage() OVERRIDE;
  virtual void DidUseTexImage() OVERRIDE;
  virtual void SetReleaseAfterUse() OVERRIDE;

 protected:
  virtual ~GLImageAndroidNativeBuffer();

 private:
  bool release_after_use_;
  bool in_use_;
  unsigned target_;
  EGLImageKHR egl_image_for_unbind_;
  GLuint texture_id_for_unbind_;

  DISALLOW_COPY_AND_ASSIGN(GLImageAndroidNativeBuffer);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_ANDROID_NATIVE_BUFFER_H_
