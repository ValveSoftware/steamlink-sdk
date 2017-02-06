// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLExtensionName_h
#define WebGLExtensionName_h

namespace blink {

// Extension names are needed to properly wrap instances in JavaScript objects.
enum WebGLExtensionName {
    ANGLEInstancedArraysName,
    EXTBlendMinMaxName,
    EXTColorBufferFloatName,
    EXTDisjointTimerQueryName,
    EXTFragDepthName,
    EXTShaderTextureLODName,
    EXTsRGBName,
    EXTTextureFilterAnisotropicName,
    OESElementIndexUintName,
    OESStandardDerivativesName,
    OESTextureFloatLinearName,
    OESTextureFloatName,
    OESTextureHalfFloatLinearName,
    OESTextureHalfFloatName,
    OESVertexArrayObjectName,
    WebGLCompressedTextureASTCName,
    WebGLCompressedTextureATCName,
    WebGLCompressedTextureETC1Name,
    WebGLCompressedTexturePVRTCName,
    WebGLCompressedTextureS3TCName,
    WebGLDebugRendererInfoName,
    WebGLDebugShadersName,
    WebGLDepthTextureName,
    WebGLDrawBuffersName,
    WebGLLoseContextName,
    WebGLExtensionNameCount, // Must be the last entry
};
} // namespace blink

#endif // WebGLExtensionName_h
