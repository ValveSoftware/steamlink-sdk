// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLCompressedTextureS3TCsRGB_h
#define WebGLCompressedTextureS3TCsRGB_h

#include "modules/webgl/WebGLExtension.h"

namespace blink {

class WebGLCompressedTextureS3TCsRGB final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebGLCompressedTextureS3TCsRGB* create(WebGLRenderingContextBase*);
  static bool supported(WebGLRenderingContextBase*);
  static const char* extensionName();

  ~WebGLCompressedTextureS3TCsRGB() override;
  WebGLExtensionName name() const override;

 private:
  explicit WebGLCompressedTextureS3TCsRGB(WebGLRenderingContextBase*);
};

}  // namespace blink

#endif  // WebGLCompressedTextureS3TCsRGB_h
