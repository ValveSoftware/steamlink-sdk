// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_STREAM_TEXTURE_IMAGE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_STREAM_TEXTURE_IMAGE_H_

#include "gpu/gpu_export.h"
#include "ui/gl/gl_image.h"

namespace gpu {
namespace gles2 {

// Specialization of GLImage that allows us to support (stream) textures
// that supply a texture matrix.
class GPU_EXPORT GLStreamTextureImage : public gl::GLImage {
 public:
  GLStreamTextureImage() {}

  // Get the matrix.
  // Copy the texture matrix for this image into |matrix|.
  // Subclasses must return a matrix appropriate for a coordinate system where
  // UV=(0,0) corresponds to the bottom left corner of the image.
  virtual void GetTextureMatrix(float matrix[16]) = 0;

  // Copy the texture matrix for this image into |matrix|, returning a matrix
  // for which UV=(0,0) corresponds to the top left of corner of the image,
  // which is what Chromium generally expects.
  void GetFlippedTextureMatrix(float matrix[16]) {
    GetTextureMatrix(matrix);
    matrix[13] += matrix[5];
    matrix[5] = -matrix[5];
  }
 protected:
  ~GLStreamTextureImage() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GLStreamTextureImage);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_STREAM_TEXTURE_IMAGE_H_
