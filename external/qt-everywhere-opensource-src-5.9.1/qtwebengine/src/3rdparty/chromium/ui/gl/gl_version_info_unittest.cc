// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

TEST(GLVersionInfoTest, MajorMinorVersionTest) {
  const char* version_str[] = {"4.3 (Core Profile) Mesa 11.2.0",
      "4.5.0 NVIDIA 364.19",
      "OpenGL ES 2.0 (ANGLE 2.1.0.cd1b12260360)",
      "2.1 INTEL-10.6.33"};
  const char* renderer_str[] = {NULL, NULL, NULL, NULL};
  const char* extensions_str[] = {"extensions", "extensions",
                                  "extensions", "extensions"};
  unsigned expected_major[] = {4, 4, 2, 2};
  unsigned expected_minor[] = {3, 5, 0, 1};
  std::unique_ptr<GLVersionInfo> version_info;
  for (unsigned i = 0; i < arraysize(version_str); i++) {
    version_info.reset(new GLVersionInfo(version_str[i],
        renderer_str[i], extensions_str[i]));
    unsigned major, minor;
    bool is_es, is_es2, is_es3;
    version_info->ParseVersionString(version_str[i], &major, &minor,
                                     &is_es, &is_es2, &is_es3);
    EXPECT_EQ(major, expected_major[i]);
    EXPECT_EQ(minor, expected_minor[i]);
  }
}

}
