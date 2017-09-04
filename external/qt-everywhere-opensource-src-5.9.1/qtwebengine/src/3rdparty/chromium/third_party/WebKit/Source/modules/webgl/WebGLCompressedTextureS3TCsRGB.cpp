// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLCompressedTextureS3TCsRGB.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLCompressedTextureS3TCsRGB::WebGLCompressedTextureS3TCsRGB(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  // TODO(kainino): update these with _EXT versions once
  // GL_EXT_compressed_texture_s3tc_srgb is ratified
  context->addCompressedTextureFormat(GL_COMPRESSED_SRGB_S3TC_DXT1_NV);
  context->addCompressedTextureFormat(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV);
  context->addCompressedTextureFormat(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV);
  context->addCompressedTextureFormat(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV);
}

WebGLCompressedTextureS3TCsRGB::~WebGLCompressedTextureS3TCsRGB() {}

WebGLExtensionName WebGLCompressedTextureS3TCsRGB::name() const {
  return WebGLCompressedTextureS3TCsRGBName;
}

WebGLCompressedTextureS3TCsRGB* WebGLCompressedTextureS3TCsRGB::create(
    WebGLRenderingContextBase* context) {
  return new WebGLCompressedTextureS3TCsRGB(context);
}

bool WebGLCompressedTextureS3TCsRGB::supported(
    WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensionsUtil = context->extensionsUtil();
  return extensionsUtil->supportsExtension(
      "GL_EXT_texture_compression_s3tc_srgb");
}

const char* WebGLCompressedTextureS3TCsRGB::extensionName() {
  return "WEBGL_compressed_texture_s3tc_srgb";
}

}  // namespace blink
