// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/EXTColorBufferFloat.h"

namespace blink {

EXTColorBufferFloat::EXTColorBufferFloat(WebGLRenderingContextBase* context)
    : WebGLExtension(context)
{
    context->extensionsUtil()->ensureExtensionEnabled("GL_EXT_color_buffer_float");
}

EXTColorBufferFloat::~EXTColorBufferFloat()
{
}

WebGLExtensionName EXTColorBufferFloat::name() const
{
    return EXTColorBufferFloatName;
}

EXTColorBufferFloat* EXTColorBufferFloat::create(WebGLRenderingContextBase* context)
{
    return new EXTColorBufferFloat(context);
}

bool EXTColorBufferFloat::supported(WebGLRenderingContextBase* context)
{
    return context->extensionsUtil()->supportsExtension("GL_EXT_color_buffer_float");
}

const char* EXTColorBufferFloat::extensionName()
{
    return "EXT_color_buffer_float";
}

} // namespace blink
