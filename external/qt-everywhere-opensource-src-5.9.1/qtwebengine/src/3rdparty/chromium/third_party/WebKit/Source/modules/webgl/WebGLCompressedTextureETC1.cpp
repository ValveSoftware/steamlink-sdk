// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLCompressedTextureETC1.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLCompressedTextureETC1::WebGLCompressedTextureETC1(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->addCompressedTextureFormat(GL_ETC1_RGB8_OES);
}

WebGLCompressedTextureETC1::~WebGLCompressedTextureETC1() {}

WebGLExtensionName WebGLCompressedTextureETC1::name() const {
  return WebGLCompressedTextureETC1Name;
}

WebGLCompressedTextureETC1* WebGLCompressedTextureETC1::create(
    WebGLRenderingContextBase* context) {
  return new WebGLCompressedTextureETC1(context);
}

bool WebGLCompressedTextureETC1::supported(WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensionsUtil = context->extensionsUtil();
  bool webgl1 = !context->isWebGL2OrHigher();
  bool etc1 =
      extensionsUtil->supportsExtension("GL_OES_compressed_ETC1_RGB8_texture");
  bool etc =
      extensionsUtil->supportsExtension("GL_CHROMIUM_compressed_texture_etc");
  return (webgl1 || etc) && etc1;
}

const char* WebGLCompressedTextureETC1::extensionName() {
  return "WEBGL_compressed_texture_etc1";
}

}  // namespace blink
