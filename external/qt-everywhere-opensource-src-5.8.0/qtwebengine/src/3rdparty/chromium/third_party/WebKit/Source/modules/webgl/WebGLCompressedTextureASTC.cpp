// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLCompressedTextureASTC.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

const WebGLCompressedTextureASTC::BlockSizeCompressASTC
    WebGLCompressedTextureASTC::kBlockSizeCompressASTC[] = {
        {GL_COMPRESSED_RGBA_ASTC_4x4_KHR,           4, 4},
        {GL_COMPRESSED_RGBA_ASTC_5x4_KHR,           5, 4},
        {GL_COMPRESSED_RGBA_ASTC_5x5_KHR,           5, 5},
        {GL_COMPRESSED_RGBA_ASTC_6x5_KHR,           6, 5},
        {GL_COMPRESSED_RGBA_ASTC_6x6_KHR,           6, 6},
        {GL_COMPRESSED_RGBA_ASTC_8x5_KHR,           8, 5},
        {GL_COMPRESSED_RGBA_ASTC_8x6_KHR,           8, 6},
        {GL_COMPRESSED_RGBA_ASTC_8x8_KHR,           8, 8},
        {GL_COMPRESSED_RGBA_ASTC_10x5_KHR,          10, 5},
        {GL_COMPRESSED_RGBA_ASTC_10x6_KHR,          10, 6},
        {GL_COMPRESSED_RGBA_ASTC_10x8_KHR,          10, 8},
        {GL_COMPRESSED_RGBA_ASTC_10x10_KHR,         10, 10},
        {GL_COMPRESSED_RGBA_ASTC_12x10_KHR,         12, 10},
        {GL_COMPRESSED_RGBA_ASTC_12x12_KHR,         12, 12}};

WebGLCompressedTextureASTC::WebGLCompressedTextureASTC(WebGLRenderingContextBase* context)
    : WebGLExtension(context)
{
    const int kAlphaFormatGap =
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR - GL_COMPRESSED_RGBA_ASTC_4x4_KHR;

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(WebGLCompressedTextureASTC::kBlockSizeCompressASTC); i++) {
        /* GL_COMPRESSED_RGBA_ASTC(0x93B0 ~ 0x93BD) */
        context->addCompressedTextureFormat(
            WebGLCompressedTextureASTC::kBlockSizeCompressASTC[i].CompressType);
        /* GL_COMPRESSED_SRGB8_ALPHA8_ASTC(0x93D0 ~ 0x93DD) */
        context->addCompressedTextureFormat(
            WebGLCompressedTextureASTC::kBlockSizeCompressASTC[i].CompressType + kAlphaFormatGap);
    }
}

WebGLCompressedTextureASTC::~WebGLCompressedTextureASTC()
{
}

WebGLExtensionName WebGLCompressedTextureASTC::name() const
{
    return WebGLCompressedTextureASTCName;
}

WebGLCompressedTextureASTC* WebGLCompressedTextureASTC::create(WebGLRenderingContextBase* context)
{
    return new WebGLCompressedTextureASTC(context);
}

bool WebGLCompressedTextureASTC::supported(WebGLRenderingContextBase* context)
{
    Extensions3DUtil* extensionsUtil = context->extensionsUtil();
    return extensionsUtil->supportsExtension("GL_KHR_texture_compression_astc_ldr");
}

const char* WebGLCompressedTextureASTC::extensionName()
{
    // TODO(cyzero.kim): implement extension for GL_KHR_texture_compression_astc_hdr.
    return "WEBGL_compressed_texture_astc";
}

} // namespace blink
