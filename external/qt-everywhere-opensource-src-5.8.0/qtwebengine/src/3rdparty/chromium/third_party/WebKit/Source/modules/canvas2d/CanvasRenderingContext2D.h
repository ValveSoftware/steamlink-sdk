/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef CanvasRenderingContext2D_h
#define CanvasRenderingContext2D_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "core/svg/SVGResourceClient.h"
#include "modules/ModulesExport.h"
#include "modules/canvas2d/BaseRenderingContext2D.h"
#include "modules/canvas2d/Canvas2DContextAttributes.h"
#include "modules/canvas2d/CanvasRenderingContext2DState.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/heap/GarbageCollected.h"
#include "public/platform/WebThread.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink { class WebLayer; }

namespace blink {

class CanvasImageSource;
class Element;
class ExceptionState;
class FloatRect;
class FloatSize;
class Font;
class FontMetrics;
class HitRegion;
class HitRegionOptions;
class HitRegionManager;
class Path2D;
class SVGMatrixTearOff;
class TextMetrics;

typedef HTMLImageElementOrHTMLVideoElementOrHTMLCanvasElementOrImageBitmap CanvasImageSourceUnion;

class MODULES_EXPORT CanvasRenderingContext2D final : public CanvasRenderingContext, public BaseRenderingContext2D, public WebThread::TaskObserver, public SVGResourceClient {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(CanvasRenderingContext2D);
    USING_PRE_FINALIZER(CanvasRenderingContext2D, dispose);
public:
    class Factory : public CanvasRenderingContextFactory {
        WTF_MAKE_NONCOPYABLE(Factory);
    public:
        Factory() {}
        ~Factory() override {}

        CanvasRenderingContext* create(HTMLCanvasElement* canvas, const CanvasContextCreationAttributes& attrs, Document& document) override
        {
            return new CanvasRenderingContext2D(canvas, attrs, document);
        }
        CanvasRenderingContext::ContextType getContextType() const override { return CanvasRenderingContext::Context2d; }
    };

    ~CanvasRenderingContext2D() override;

    void setCanvasGetContextResult(RenderingContext&) final;

    bool isContextLost() const override;

    bool shouldAntialias() const override;
    void setShouldAntialias(bool) override;

    void scrollPathIntoView();
    void scrollPathIntoView(Path2D*);

    void clearRect(double x, double y, double width, double height) override;

    void reset() override;

    String font() const;
    void setFont(const String&) override;

    String textAlign() const;
    void setTextAlign(const String&);

    String textBaseline() const;
    void setTextBaseline(const String&);

    String direction() const;
    void setDirection(const String&);

    void fillText(const String& text, double x, double y);
    void fillText(const String& text, double x, double y, double maxWidth);
    void strokeText(const String& text, double x, double y);
    void strokeText(const String& text, double x, double y, double maxWidth);
    TextMetrics* measureText(const String& text);

    void getContextAttributes(Canvas2DContextAttributes&) const;

    void drawFocusIfNeeded(Element*);
    void drawFocusIfNeeded(Path2D*, Element*);

    void addHitRegion(const HitRegionOptions&, ExceptionState&);
    void removeHitRegion(const String& id);
    void clearHitRegions();
    HitRegion* hitRegionAtPoint(const FloatPoint&);
    unsigned hitRegionsCount() const override;

    void loseContext(LostContextMode) override;
    void didSetSurfaceSize() override;

    void restoreCanvasMatrixClipStack(SkCanvas*) const override;

    // TaskObserver implementation
    void didProcessTask() override;
    void willProcessTask() override { }

    void styleDidChange(const ComputedStyle* oldStyle, const ComputedStyle& newStyle) override;
    std::pair<Element*, String> getControlAndIdIfHitRegionExists(const LayoutPoint& location) override;
    String getIdFromControl(const Element*) override;

    // SVGResourceClient implementation
    void filterNeedsInvalidation() override;

    // BaseRenderingContext2D implementation
    bool originClean() const final;
    void setOriginTainted() final;
    bool wouldTaintOrigin(CanvasImageSource* source, ExecutionContext*) final { return CanvasRenderingContext::wouldTaintOrigin(source); }

    int width() const final;
    int height() const final;

    bool hasImageBuffer() const final;
    ImageBuffer* imageBuffer() const final;

    bool parseColorOrCurrentColor(Color&, const String& colorString) const final;

    SkCanvas* drawingCanvas() const final;
    SkCanvas* existingDrawingCanvas() const final;
    void disableDeferral(DisableDeferralReason) final;

    AffineTransform baseTransform() const final;
    void didDraw(const SkIRect& dirtyRect) final;

    bool stateHasFilter() final;
    SkImageFilter* stateGetFilter() final;
    void snapshotStateForFilter() final;

    void validateStateStack() final;

private:
    friend class CanvasRenderingContext2DAutoRestoreSkCanvas;

    CanvasRenderingContext2D(HTMLCanvasElement*, const CanvasContextCreationAttributes& attrs, Document&);

    void dispose();

    void dispatchContextLostEvent(Timer<CanvasRenderingContext2D>*);
    void dispatchContextRestoredEvent(Timer<CanvasRenderingContext2D>*);
    void tryRestoreContextEvent(Timer<CanvasRenderingContext2D>*);

    void unwindStateStack();

    void pruneLocalFontCache(size_t targetSize);
    void schedulePruneLocalFontCacheIfNeeded();

    void scrollPathIntoViewInternal(const Path&);

    void drawTextInternal(const String&, double x, double y, CanvasRenderingContext2DState::PaintType, double* maxWidth = nullptr);

    const Font& accessFont();
    int getFontBaseline(const FontMetrics&) const;

    void drawFocusIfNeededInternal(const Path&, Element*);
    bool focusRingCallIsValid(const Path&, Element*);
    void drawFocusRing(const Path&);
    void updateElementAccessibility(const Path&, Element*);

    CanvasRenderingContext::ContextType getContextType() const override { return CanvasRenderingContext::Context2d; }
    bool is2d() const override { return true; }
    bool isAccelerated() const override;
    bool hasAlpha() const override { return m_hasAlpha; }
    void setIsHidden(bool) override;
    void stop() final;
    DECLARE_VIRTUAL_TRACE();

    virtual bool isTransformInvertible() const;

    WebLayer* platformLayer() const override;

    Member<HitRegionManager> m_hitRegionManager;
    bool m_hasAlpha;
    LostContextMode m_contextLostMode;
    bool m_contextRestorable;
    unsigned m_tryRestoreContextAttemptCount;
    Timer<CanvasRenderingContext2D> m_dispatchContextLostEventTimer;
    Timer<CanvasRenderingContext2D> m_dispatchContextRestoredEventTimer;
    Timer<CanvasRenderingContext2D> m_tryRestoreContextEventTimer;

    HashMap<String, Font> m_fontsResolvedUsingCurrentStyle;
    bool m_pruneLocalFontCacheScheduled;
    ListHashSet<String> m_fontLRUList;
};

DEFINE_TYPE_CASTS(CanvasRenderingContext2D, CanvasRenderingContext, context, context->is2d(), context.is2d());

} // namespace blink

#endif // CanvasRenderingContext2D_h
