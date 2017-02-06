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

#ifndef CanvasRenderingContext_h
#define CanvasRenderingContext_h

#include "core/CoreExport.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/StringHash.h"

class SkCanvas;

namespace blink { class WebLayer; }

namespace blink {

class CanvasImageSource;
class HTMLCanvasElement;
class ImageData;
class ImageBitmap;

class CORE_EXPORT CanvasRenderingContext : public GarbageCollectedFinalized<CanvasRenderingContext>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(CanvasRenderingContext);
    USING_PRE_FINALIZER(CanvasRenderingContext, dispose);
public:
    virtual ~CanvasRenderingContext() { }

    // A Canvas can either be "2D" or "webgl" but never both. If you request a 2D canvas and the existing
    // context is already 2D, just return that. If the existing context is WebGL, then destroy it
    // before creating a new 2D context. Vice versa when requesting a WebGL canvas. Requesting a
    // context with any other type string will destroy any existing context.
    enum ContextType {
        // Do not change assigned numbers of existing items: add new features to the end of the list.
        Context2d = 0,
        ContextExperimentalWebgl = 2,
        ContextWebgl = 3,
        ContextWebgl2 = 4,
        ContextImageBitmap = 5,
        ContextTypeCount,
    };

    static ContextType contextTypeFromId(const String& id);
    static ContextType resolveContextTypeAliases(ContextType);

    HTMLCanvasElement* canvas() const { return m_canvas; }

    virtual ContextType getContextType() const = 0;
    virtual bool isAccelerated() const { return false; }
    virtual bool shouldAntialias() const { return false; }
    virtual bool hasAlpha() const { return true; }
    virtual void setIsHidden(bool) = 0;
    virtual bool isContextLost() const { return true; }
    virtual void setCanvasGetContextResult(RenderingContext&) { NOTREACHED(); };
    virtual void setOffscreenCanvasGetContextResult(OffscreenRenderingContext&) { NOTREACHED(); }

    // Return true if the content is updated.
    virtual bool paintRenderingResultsToCanvas(SourceDrawingBuffer) { return false; }

    virtual WebLayer* platformLayer() const { return nullptr; }

    enum LostContextMode {
        NotLostContext,

        // Lost context occurred at the graphics system level.
        RealLostContext,

        // Lost context provoked by WEBGL_lose_context.
        WebGLLoseContextLostContext,

        // Lost context occurred due to internal implementation reasons.
        SyntheticLostContext,
    };
    virtual void loseContext(LostContextMode) { }

    // Canvas2D-specific interface
    virtual bool is2d() const { return false; }
    virtual void restoreCanvasMatrixClipStack(SkCanvas*) const { }
    virtual void reset() { }
    virtual void clearRect(double x, double y, double width, double height) { }
    virtual void didSetSurfaceSize() { }
    virtual void setShouldAntialias(bool) { }
    virtual unsigned hitRegionsCount() const { return 0; }
    virtual void setFont(const String&) { }
    virtual void styleDidChange(const ComputedStyle* oldStyle, const ComputedStyle& newStyle) { }
    virtual std::pair<Element*, String> getControlAndIdIfHitRegionExists(const LayoutPoint& location) { NOTREACHED(); return std::make_pair(nullptr, String()); }
    virtual String getIdFromControl(const Element* element) { return String(); }

    // WebGL-specific interface
    virtual bool is3d() const { return false; }
    virtual void setFilterQuality(SkFilterQuality) { NOTREACHED(); }
    virtual void reshape(int width, int height) { NOTREACHED(); }
    virtual void markLayerComposited() { NOTREACHED(); }
    virtual ImageData* paintRenderingResultsToImageData(SourceDrawingBuffer) { NOTREACHED(); return nullptr; }
    virtual int externallyAllocatedBytesPerPixel() { NOTREACHED(); return 0; }

    // ImageBitmap-specific interface
    virtual bool paint(GraphicsContext&, const IntRect&) { return false; }

    bool wouldTaintOrigin(CanvasImageSource*, SecurityOrigin* = nullptr);
    void didMoveToNewDocument(Document*);

    // OffscreenCanvas-specific methods
    OffscreenCanvas* getOffscreenCanvas() const { return m_offscreenCanvas; }
    virtual ImageBitmap* transferToImageBitmap(ExceptionState&) { return nullptr; }

    void detachCanvas() { m_canvas = nullptr; }

protected:
    CanvasRenderingContext(HTMLCanvasElement* = nullptr, OffscreenCanvas* = nullptr);
    DECLARE_VIRTUAL_TRACE();
    virtual void stop() = 0;

private:
    void dispose();

    Member<HTMLCanvasElement> m_canvas;
    Member<OffscreenCanvas> m_offscreenCanvas;
    HashSet<String> m_cleanURLs;
    HashSet<String> m_dirtyURLs;
};

} // namespace blink

#endif
