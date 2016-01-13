// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_UTIL_H_
#define CC_BASE_UTIL_H_

#include <limits>

#include "base/basictypes.h"
#include "cc/resources/resource_provider.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

template <typename T> T RoundUp(T n, T mul) {
  COMPILE_ASSERT(std::numeric_limits<T>::is_integer, type_must_be_integral);
  return (n > 0) ? ((n + mul - 1) / mul) * mul
                 : (n / mul) * mul;
}

template <typename T> T RoundDown(T n, T mul) {
  COMPILE_ASSERT(std::numeric_limits<T>::is_integer, type_must_be_integral);
  return (n > 0) ? (n / mul) * mul
                 : (n == 0) ? 0
                 : ((n - mul + 1) / mul) * mul;
}

inline GLenum GLDataType(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const unsigned format_gl_data_type[RESOURCE_FORMAT_MAX + 1] = {
    GL_UNSIGNED_BYTE,           // RGBA_8888
    GL_UNSIGNED_SHORT_4_4_4_4,  // RGBA_4444
    GL_UNSIGNED_BYTE,           // BGRA_8888
    GL_UNSIGNED_BYTE,           // LUMINANCE_8
    GL_UNSIGNED_SHORT_5_6_5,    // RGB_565,
    GL_UNSIGNED_BYTE            // ETC1
  };
  return format_gl_data_type[format];
}

inline GLenum GLDataFormat(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const unsigned format_gl_data_format[RESOURCE_FORMAT_MAX + 1] = {
    GL_RGBA,           // RGBA_8888
    GL_RGBA,           // RGBA_4444
    GL_BGRA_EXT,       // BGRA_8888
    GL_LUMINANCE,      // LUMINANCE_8
    GL_RGB,            // RGB_565
    GL_ETC1_RGB8_OES   // ETC1
  };
  return format_gl_data_format[format];
}

inline GLenum GLInternalFormat(ResourceFormat format) {
  return GLDataFormat(format);
}

}  // namespace cc

#endif  // CC_BASE_UTIL_H_
