// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_utils.h"

#include "base/metrics/histogram.h"

namespace gpu {
namespace gles2 {

std::vector<int> GetAllGLErrors() {
  int gl_errors[] = {
    GL_NO_ERROR,
    GL_INVALID_ENUM,
    GL_INVALID_VALUE,
    GL_INVALID_OPERATION,
    GL_INVALID_FRAMEBUFFER_OPERATION,
    GL_OUT_OF_MEMORY,
  };
  return base::CustomHistogram::ArrayToCustomRanges(gl_errors,
                                                    arraysize(gl_errors));
}

}  // namespace gles2
}  // namespace gpu
