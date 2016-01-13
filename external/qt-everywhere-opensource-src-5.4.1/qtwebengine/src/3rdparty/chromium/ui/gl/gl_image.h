// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_H_
#define UI_GL_GL_IMAGE_H_

#include "base/memory/ref_counted.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_export.h"

namespace gfx {

class GLSurface;

// Encapsulates an image that can be bound to a texture, hiding platform
// specific management.
class GL_EXPORT GLImage : public base::RefCounted<GLImage> {
 public:
  GLImage();

  // Destroys the image.
  virtual void Destroy() = 0;

  // Get the size of the image.
  virtual gfx::Size GetSize() = 0;

  // Bind image to texture currently bound to |target|.
  virtual bool BindTexImage(unsigned target) = 0;

  // Release image from texture currently bound to |target|.
  virtual void ReleaseTexImage(unsigned target) = 0;

  // Called before the texture is used for drawing.
  virtual void WillUseTexImage() = 0;

  // Called after the texture has been used for drawing.
  virtual void DidUseTexImage() = 0;

  // Called before the texture image data will be modified.
  virtual void WillModifyTexImage() = 0;

  // Called after the texture image data has been modified.
  virtual void DidModifyTexImage() = 0;

  // Indicate that image should be released after use.
  // (For an Android work-around only).
  virtual void SetReleaseAfterUse();

  // Create a GL image for a window.
  static scoped_refptr<GLImage> CreateGLImage(gfx::PluginWindowHandle window);

  // Create a GL image for a GPU Memory buffer.
  static scoped_refptr<GLImage> CreateGLImageForGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle buffer,
      gfx::Size size,
      unsigned internalformat);

 protected:
  virtual ~GLImage();

 private:
  friend class base::RefCounted<GLImage>;

  DISALLOW_COPY_AND_ASSIGN(GLImage);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_H_
