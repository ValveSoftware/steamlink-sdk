// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGL2RenderingContext_h
#define WebGL2RenderingContext_h

#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"
#include <memory>

namespace blink {

class CanvasContextCreationAttributes;
class EXTColorBufferFloat;

class WebGL2RenderingContext : public WebGL2RenderingContextBase {
    DEFINE_WRAPPERTYPEINFO();
public:
    class Factory : public CanvasRenderingContextFactory {
        WTF_MAKE_NONCOPYABLE(Factory);
    public:
        Factory() {}
        ~Factory() override {}

        CanvasRenderingContext* create(HTMLCanvasElement*, const CanvasContextCreationAttributes&, Document&) override;
        CanvasRenderingContext::ContextType getContextType() const override { return CanvasRenderingContext::ContextWebgl2; }
        void onError(HTMLCanvasElement*, const String& error) override;
    };

    ~WebGL2RenderingContext() override;

    CanvasRenderingContext::ContextType getContextType() const override { return CanvasRenderingContext::ContextWebgl2; }
    ImageBitmap* transferToImageBitmap(ExceptionState&) final;
    unsigned version() const override { return 2; }
    String contextName() const override { return "WebGL2RenderingContext"; }
    void registerContextExtensions() override;
    void setCanvasGetContextResult(RenderingContext&) final;
    void setOffscreenCanvasGetContextResult(OffscreenRenderingContext&) final;

    DECLARE_VIRTUAL_TRACE();

    DECLARE_VIRTUAL_TRACE_WRAPPERS();

protected:
    WebGL2RenderingContext(HTMLCanvasElement* passedCanvas, std::unique_ptr<WebGraphicsContext3DProvider>, const WebGLContextAttributes& requestedAttributes);

    Member<EXTColorBufferFloat> m_extColorBufferFloat;
    Member<EXTDisjointTimerQuery> m_extDisjointTimerQuery;
    Member<EXTTextureFilterAnisotropic> m_extTextureFilterAnisotropic;
    Member<OESTextureFloatLinear> m_oesTextureFloatLinear;
    Member<WebGLCompressedTextureASTC> m_webglCompressedTextureASTC;
    Member<WebGLCompressedTextureATC> m_webglCompressedTextureATC;
    Member<WebGLCompressedTextureETC1> m_webglCompressedTextureETC1;
    Member<WebGLCompressedTexturePVRTC> m_webglCompressedTexturePVRTC;
    Member<WebGLCompressedTextureS3TC> m_webglCompressedTextureS3TC;
    Member<WebGLDebugRendererInfo> m_webglDebugRendererInfo;
    Member<WebGLDebugShaders> m_webglDebugShaders;
    Member<WebGLLoseContext> m_webglLoseContext;
};

DEFINE_TYPE_CASTS(WebGL2RenderingContext, CanvasRenderingContext, context,
    context->is3d() && WebGLRenderingContextBase::getWebGLVersion(context) == 2,
    context.is3d() && WebGLRenderingContextBase::getWebGLVersion(&context) == 2);

} // namespace blink

#endif
