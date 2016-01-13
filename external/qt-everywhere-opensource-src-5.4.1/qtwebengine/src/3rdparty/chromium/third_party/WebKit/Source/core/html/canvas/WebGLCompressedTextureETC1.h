// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLCompressedTextureETC1_h
#define WebGLCompressedTextureETC1_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/canvas/WebGLExtension.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class WebGLTexture;

class WebGLCompressedTextureETC1 FINAL : public WebGLExtension, public ScriptWrappable {
public:
    static PassRefPtr<WebGLCompressedTextureETC1> create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    virtual ~WebGLCompressedTextureETC1();
    virtual WebGLExtensionName name() const OVERRIDE;

private:
    WebGLCompressedTextureETC1(WebGLRenderingContextBase*);
};

} // namespace WebCore

#endif // WebGLCompressedTextureETC1_h
