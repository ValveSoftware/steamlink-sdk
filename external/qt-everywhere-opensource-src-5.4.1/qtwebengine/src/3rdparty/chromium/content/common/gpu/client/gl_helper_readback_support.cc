// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gl_helper_readback_support.h"
#include "base/logging.h"

namespace content {

GLHelperReadbackSupport::GLHelperReadbackSupport(gpu::gles2::GLES2Interface* gl)
    : gl_(gl) {
  InitializeReadbackSupport();
}

GLHelperReadbackSupport::~GLHelperReadbackSupport() {}

void GLHelperReadbackSupport::InitializeReadbackSupport() {
  // We are concerned about 16, 32-bit formats only.
  // The below are the most used 16, 32-bit formats.
  // In future if any new format support is needed that should be added here.
  // Initialize the array with FORMAT_NOT_SUPPORTED as we dont know the
  // supported formats yet.
  for (int i = 0; i < SkBitmap::kConfigCount; ++i) {
    format_support_table_[i] = FORMAT_NOT_SUPPORTED;
  }
  CheckForReadbackSupport(SkBitmap::kRGB_565_Config);
  CheckForReadbackSupport(SkBitmap::kARGB_4444_Config);
  CheckForReadbackSupport(SkBitmap::kARGB_8888_Config);
  // Further any formats, support should be checked here.
}

void GLHelperReadbackSupport::CheckForReadbackSupport(
    SkBitmap::Config texture_format) {
  bool supports_format = false;
  switch (texture_format) {
    case SkBitmap::kRGB_565_Config:
      supports_format = SupportsFormat(GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
      break;
    case SkBitmap::kARGB_8888_Config:
      // This is the baseline, assume always true.
      supports_format = true;
      break;
    case SkBitmap::kARGB_4444_Config:
      supports_format = false;
      break;
    default:
      NOTREACHED();
      supports_format = false;
      break;
  }
  DCHECK((int)texture_format < (int)SkBitmap::kConfigCount);
  format_support_table_[texture_format] =
      supports_format ? FORMAT_SUPPORTED : FORMAT_NOT_SUPPORTED;
}

void GLHelperReadbackSupport::GetAdditionalFormat(GLint format, GLint type,
                                                  GLint *format_out,
                                                  GLint *type_out) {
  for (unsigned int i = 0; i < format_cache_.size(); i++) {
    if (format_cache_[i].format == format && format_cache_[i].type == type) {
      *format_out = format_cache_[i].read_format;
      *type_out = format_cache_[i].read_type;
      return;
    }
  }

  const int kTestSize = 64;
  content::ScopedTexture dst_texture(gl_);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(gl_, dst_texture);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->TexImage2D(
      GL_TEXTURE_2D, 0, format, kTestSize, kTestSize, 0, format, type, NULL);
  ScopedFramebuffer dst_framebuffer(gl_);
  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl_,
                                                             dst_framebuffer);
  gl_->FramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_texture, 0);
  gl_->GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, format_out);
  gl_->GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, type_out);

  struct FormatCacheEntry entry = { format, type, *format_out, *type_out };
  format_cache_.push_back(entry);
}

bool GLHelperReadbackSupport::SupportsFormat(GLint format, GLint type) {
  // GLES2.0 Specification says this pairing is always supported
  // with additional format from GL_IMPLEMENTATION_COLOR_READ_FORMAT/TYPE
  if (format == GL_RGBA && type == GL_UNSIGNED_BYTE)
    return true;
  bool supports_format = false;
  GLint ext_format = 0, ext_type = 0;
  GetAdditionalFormat(format, type, &ext_format, &ext_type);
  if ((ext_format == format) && (ext_type == type)) {
    supports_format = true;
  }
  return supports_format;
}

bool GLHelperReadbackSupport::IsReadbackConfigSupported(
    SkBitmap::Config texture_format) {
  switch (format_support_table_[texture_format]) {
    case FORMAT_SUPPORTED:
      return true;
    case FORMAT_NOT_SUPPORTED:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace content
