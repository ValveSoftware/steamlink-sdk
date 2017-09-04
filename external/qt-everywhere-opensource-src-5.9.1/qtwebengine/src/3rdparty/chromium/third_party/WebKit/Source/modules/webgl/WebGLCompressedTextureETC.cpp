// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLCompressedTextureETC.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLCompressedTextureETC::WebGLCompressedTextureETC(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->addCompressedTextureFormat(GL_COMPRESSED_R11_EAC);
  context->addCompressedTextureFormat(GL_COMPRESSED_SIGNED_R11_EAC);
  context->addCompressedTextureFormat(GL_COMPRESSED_RGB8_ETC2);
  context->addCompressedTextureFormat(GL_COMPRESSED_SRGB8_ETC2);
  context->addCompressedTextureFormat(
      GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
  context->addCompressedTextureFormat(
      GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
  context->addCompressedTextureFormat(GL_COMPRESSED_RG11_EAC);
  context->addCompressedTextureFormat(GL_COMPRESSED_SIGNED_RG11_EAC);
  context->addCompressedTextureFormat(GL_COMPRESSED_RGBA8_ETC2_EAC);
  context->addCompressedTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
}

WebGLCompressedTextureETC::~WebGLCompressedTextureETC() {}

WebGLExtensionName WebGLCompressedTextureETC::name() const {
  return WebGLCompressedTextureETCName;
}

WebGLCompressedTextureETC* WebGLCompressedTextureETC::create(
    WebGLRenderingContextBase* context) {
  return new WebGLCompressedTextureETC(context);
}

bool WebGLCompressedTextureETC::supported(WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensionsUtil = context->extensionsUtil();
  return extensionsUtil->supportsExtension(
      "GL_CHROMIUM_compressed_texture_etc");
}

const char* WebGLCompressedTextureETC::extensionName() {
  return "WEBGL_compressed_texture_etc";
}

}  // namespace blink
