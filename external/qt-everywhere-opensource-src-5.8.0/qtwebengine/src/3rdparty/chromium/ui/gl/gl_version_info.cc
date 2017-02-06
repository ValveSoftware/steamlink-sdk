// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_version_info.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"

namespace {

bool DesktopCoreCommonCheck(
    bool is_es, unsigned major_version, unsigned minor_version) {
  return (!is_es &&
          ((major_version == 3 && minor_version >= 2) ||
           major_version > 3));
}

}

namespace gl {

GLVersionInfo::GLVersionInfo(const char* version_str,
                             const char* renderer_str,
                             const char* extensions_str)
    : GLVersionInfo() {
  std::set<std::string> extensions;
  if (extensions_str) {
    auto split = base::SplitString(extensions_str, " ", base::KEEP_WHITESPACE,
                                   base::SPLIT_WANT_NONEMPTY);
    extensions.insert(split.begin(), split.end());
  }
  Initialize(version_str, renderer_str, extensions);
}

GLVersionInfo::GLVersionInfo(const char* version_str,
                             const char* renderer_str,
                             const std::set<std::string>& extensions)
    : GLVersionInfo() {
  Initialize(version_str, renderer_str, extensions);
}

GLVersionInfo::GLVersionInfo()
    : is_es(false),
      is_angle(false),
      major_version(0),
      minor_version(0),
      is_es2(false),
      is_es3(false),
      is_desktop_core_profile(false),
      is_es3_capable(false) {}

void GLVersionInfo::Initialize(const char* version_str,
                               const char* renderer_str,
                               const std::set<std::string>& extensions) {
  if (version_str) {
    ParseVersionString(version_str, &major_version, &minor_version,
                       &is_es, &is_es2, &is_es3);
  }
  if (renderer_str) {
    is_angle = base::StartsWith(renderer_str, "ANGLE",
                                base::CompareCase::SENSITIVE);
  }
  is_desktop_core_profile =
      DesktopCoreCommonCheck(is_es, major_version, minor_version) &&
      extensions.find("GL_ARB_compatibility") == extensions.end();
  is_es3_capable = IsES3Capable(extensions);
}

bool GLVersionInfo::IsES3Capable(
    const std::set<std::string>& extensions) const {
  auto has_extension = [&extensions](std::string extension) -> bool {
    return extensions.find(extension) != extensions.end();
  };

  // Version ES3 capable without extensions needed.
  if (IsAtLeastGLES(3, 0) || IsAtLeastGL(4, 2)) {
    return true;
  }

  // Don't try supporting ES3 on ES2, or desktop before 3.2 because there are no
  // extension that allow querying for GL_MAX_FRAGMENT_INPUT_COMPONENTS
  if (is_es || !IsAtLeastGL(3, 2)) {
    return false;
  }

  bool has_shader_packing = has_extension("GL_ARB_shading_bit_packing") ||
                            has_extension("GL_ARB_shader_bit_encoding");
  bool has_transform_feedback =
      IsAtLeastGL(4, 0) || has_extension("GL_ARB_transform_feedback2");
  // Technically this is required for ES2 as well, but some drivers silently
  // support it, without the required extensions. To avoid breaking WebGL for
  // some users, only require detectable support for ES3.
  bool has_sampler_indexing =
      IsAtLeastGL(4, 0) || is_es || has_extension("GL_ARB_gpu_shader5");
  bool has_samplers =
      IsAtLeastGL(3, 3) || has_extension("GL_ARB_sampler_object");
  bool has_swizzle = IsAtLeastGL(3, 3) ||
                     has_extension("GL_ARB_texture_swizzle") ||
                     has_extension("GL_EXT_texture_swizzle");
  bool has_attrib_location =
      IsAtLeastGL(3, 3) || has_extension("GL_ARB_explicit_attrib_location");

  // TODO(cwallez) check for texture related extensions. See crbug.com/623577

  return has_shader_packing && has_transform_feedback && has_sampler_indexing &&
         has_samplers && has_swizzle && has_attrib_location;
}

void GLVersionInfo::ParseVersionString(const char* version_str,
                                       unsigned* major_version,
                                       unsigned* minor_version,
                                       bool* is_es,
                                       bool* is_es2,
                                       bool* is_es3) {
  // Make sure the outputs are always initialized.
  *major_version = 0;
  *minor_version = 0;
  *is_es = false;
  *is_es2 = false;
  *is_es3 = false;
  if (!version_str)
    return;
  std::string lstr(base::ToLowerASCII(version_str));
  *is_es = (lstr.length() > 12) && (lstr.substr(0, 9) == "opengl es");
  if (*is_es)
    lstr = lstr.substr(10, 3);
  base::StringTokenizer tokenizer(lstr.begin(), lstr.end(), ". ");
  unsigned major, minor;
  if (tokenizer.GetNext() &&
      base::StringToUint(tokenizer.token_piece(), &major)) {
    *major_version = major;
    if (tokenizer.GetNext() &&
        base::StringToUint(tokenizer.token_piece(), &minor)) {
      *minor_version = minor;
    }
  }
  if (*is_es && *major_version == 2)
    *is_es2 = true;
  if (*is_es && *major_version == 3)
    *is_es3 = true;
  DCHECK(major_version != 0);
}

}  // namespace gl
