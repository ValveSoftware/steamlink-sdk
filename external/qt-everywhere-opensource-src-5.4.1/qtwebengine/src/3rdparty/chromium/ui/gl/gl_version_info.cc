// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_version_info.h"

#include "base/strings/string_util.h"

namespace gfx {

GLVersionInfo::GLVersionInfo(const char* version_str, const char* renderer_str)
    : is_es(false),
      is_es1(false),
      is_es2(false),
      is_es3(false),
      is_gl1(false),
      is_gl2(false),
      is_gl3(false),
      is_gl4(false),
      is_angle(false) {
  if (version_str) {
    std::string lstr(StringToLowerASCII(std::string(version_str)));
    is_es = (lstr.substr(0, 9) == "opengl es");
    if (is_es) {
      is_es1 = (lstr.substr(9, 2) == "-c" && lstr.substr(13, 2) == "1.");
      is_es2 = (lstr.substr(9, 3) == " 2.");
      is_es3 = (lstr.substr(9, 3) == " 3.");
    } else {
      is_gl2 = (lstr.substr(0, 2) == "2.");
      is_gl3 = (lstr.substr(0, 2) == "3.");
      is_gl4 = (lstr.substr(0, 2) == "4.");
      // In early GL versions, GetString output format is implementation
      // dependent.
      is_gl1 = !is_gl2 && !is_gl3 && !is_gl4;
    }
  }
  if (renderer_str) {
    is_angle = StartsWithASCII(renderer_str, "ANGLE", true);
  }
}

}  // namespace gfx
