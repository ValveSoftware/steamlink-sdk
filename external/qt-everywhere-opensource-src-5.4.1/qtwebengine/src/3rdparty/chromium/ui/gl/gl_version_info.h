// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_VERSION_INFO_H_
#define UI_GL_GL_VERSION_INFO_H_

#include <string>
#include "base/basictypes.h"

namespace gfx {

struct GLVersionInfo {
  GLVersionInfo(const char* version_str, const char* renderer_str);

  // New flags, such as is_gl4_4 could be introduced as needed.
  // For now, this level of granularity is enough.
  bool is_es;
  bool is_es1;
  bool is_es2;
  bool is_es3;

  bool is_gl1;
  bool is_gl2;
  bool is_gl3;
  bool is_gl4;

  bool is_angle;

private:
  DISALLOW_COPY_AND_ASSIGN(GLVersionInfo);
};

}  // namespace gfx

#endif // UI_GL_GL_VERSION_INFO_H_
