/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebGLRenderingContext_h
#define WebGLRenderingContext_h

#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include <memory>

namespace blink {

class CanvasContextCreationAttributes;

class WebGLRenderingContext final : public WebGLRenderingContextBase {
    DEFINE_WRAPPERTYPEINFO();
public:
    class Factory : public CanvasRenderingContextFactory {
        WTF_MAKE_NONCOPYABLE(Factory);
    public:
        Factory() {}
        ~Factory() override {}

        CanvasRenderingContext* create(HTMLCanvasElement*, const CanvasContextCreationAttributes&, Document&) override;
        CanvasRenderingContext* create(ScriptState*, OffscreenCanvas*, const CanvasContextCreationAttributes&) override;
        CanvasRenderingContext::ContextType getContextType() const override { return CanvasRenderingContext::ContextWebgl; }
        void onError(HTMLCanvasElement*, const String& error) override;
    };

    ~WebGLRenderingContext() override;

    CanvasRenderingContext::ContextType getContextType() const override { return CanvasRenderingContext::ContextWebgl; }
    ImageBitmap* transferToImageBitmap(ExceptionState&) final;
    unsigned version() const override { return 1; }
    String contextName() const override { return "WebGLRenderingContext"; }
    void registerContextExtensions() override;
    void setCanvasGetContextResult(RenderingContext&) final;
    void setOffscreenCanvasGetContextResult(OffscreenRenderingContext&) final;

    EAGERLY_FINALIZE();
    DECLARE_VIRTUAL_TRACE();

    DECLARE_VIRTUAL_TRACE_WRAPPERS();

private:
    WebGLRenderingContext(HTMLCanvasElement*, std::unique_ptr<WebGraphicsContext3DProvider>, const WebGLContextAttributes&);
    WebGLRenderingContext(OffscreenCanvas*, std::unique_ptr<WebGraphicsContext3DProvider>, const WebGLContextAttributes&);

    // Enabled extension objects.
    Member<ANGLEInstancedArrays> m_angleInstancedArrays;
    Member<EXTBlendMinMax> m_extBlendMinMax;
    Member<EXTDisjointTimerQuery> m_extDisjointTimerQuery;
    Member<EXTFragDepth> m_extFragDepth;
    Member<EXTShaderTextureLOD> m_extShaderTextureLOD;
    Member<EXTsRGB> m_extsRGB;
    Member<EXTTextureFilterAnisotropic> m_extTextureFilterAnisotropic;
    Member<OESTextureFloat> m_oesTextureFloat;
    Member<OESTextureFloatLinear> m_oesTextureFloatLinear;
    Member<OESTextureHalfFloat> m_oesTextureHalfFloat;
    Member<OESTextureHalfFloatLinear> m_oesTextureHalfFloatLinear;
    Member<OESStandardDerivatives> m_oesStandardDerivatives;
    Member<OESVertexArrayObject> m_oesVertexArrayObject;
    Member<OESElementIndexUint> m_oesElementIndexUint;
    Member<WebGLLoseContext> m_webglLoseContext;
    Member<WebGLDebugRendererInfo> m_webglDebugRendererInfo;
    Member<WebGLDebugShaders> m_webglDebugShaders;
    Member<WebGLDrawBuffers> m_webglDrawBuffers;
    Member<WebGLCompressedTextureASTC> m_webglCompressedTextureASTC;
    Member<WebGLCompressedTextureATC> m_webglCompressedTextureATC;
    Member<WebGLCompressedTextureETC1> m_webglCompressedTextureETC1;
    Member<WebGLCompressedTexturePVRTC> m_webglCompressedTexturePVRTC;
    Member<WebGLCompressedTextureS3TC> m_webglCompressedTextureS3TC;
    Member<WebGLDepthTexture> m_webglDepthTexture;
};

DEFINE_TYPE_CASTS(WebGLRenderingContext, CanvasRenderingContext, context,
    context->is3d() && WebGLRenderingContextBase::getWebGLVersion(context) == 1,
    context.is3d() && WebGLRenderingContextBase::getWebGLVersion(&context) == 1);

} // namespace blink

#endif // WebGLRenderingContext_h
