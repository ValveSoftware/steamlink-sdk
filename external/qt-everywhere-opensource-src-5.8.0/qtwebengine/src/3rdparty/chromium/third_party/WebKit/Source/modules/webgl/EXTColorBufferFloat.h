// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTColorBufferFloat_h
#define EXTColorBufferFloat_h

#include "modules/webgl/WebGLExtension.h"

namespace blink {

class EXTColorBufferFloat final : public WebGLExtension {
    DEFINE_WRAPPERTYPEINFO();
public:
    static EXTColorBufferFloat* create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    ~EXTColorBufferFloat() override;
    WebGLExtensionName name() const override;

private:
    explicit EXTColorBufferFloat(WebGLRenderingContextBase*);
};

} // namespace blink

#endif // EXTColorBufferFloat_h
