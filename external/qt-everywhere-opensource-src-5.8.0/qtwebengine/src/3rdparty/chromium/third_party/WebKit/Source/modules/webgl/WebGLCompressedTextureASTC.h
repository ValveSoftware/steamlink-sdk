// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLCompressedTextureASTC_h
#define WebGLCompressedTextureASTC_h

#include "modules/webgl/WebGLExtension.h"

namespace blink {

class WebGLCompressedTextureASTC final : public WebGLExtension {
    DEFINE_WRAPPERTYPEINFO();
public:
    typedef struct {
        int CompressType;
        int blockWidth;
        int blockHeight;
    } BlockSizeCompressASTC;

    static WebGLCompressedTextureASTC* create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    ~WebGLCompressedTextureASTC() override;
    WebGLExtensionName name() const override;
    static const BlockSizeCompressASTC kBlockSizeCompressASTC[];

private:
    explicit WebGLCompressedTextureASTC(WebGLRenderingContextBase*);
};

} // namespace blink

#endif // WebGLCompressedTextureASTC_h
