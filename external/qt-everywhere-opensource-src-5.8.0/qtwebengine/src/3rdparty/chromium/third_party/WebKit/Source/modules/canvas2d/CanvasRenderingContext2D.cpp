/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012, 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#include "modules/canvas2d/CanvasRenderingContext2D.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/modules/v8/RenderingContext.h"
#include "core/CSSPropertyNames.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/StyleEngine.h"
#include "core/events/Event.h"
#include "core/events/MouseEvent.h"
#include "core/frame/Settings.h"
#include "core/html/TextMetrics.h"
#include "core/html/canvas/CanvasFontCache.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutTheme.h"
#include "modules/canvas2d/CanvasStyle.h"
#include "modules/canvas2d/HitRegion.h"
#include "modules/canvas2d/Path2D.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/DrawLooperBuilder.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StrokeData.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/text/BidiTextRun.h"
#include "public/platform/Platform.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "wtf/MathExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/typed_arrays/ArrayBufferContents.h"

namespace blink {

static const char defaultFont[] = "10px sans-serif";
static const char inherit[] = "inherit";
static const char rtl[] = "rtl";
static const char ltr[] = "ltr";
static const double TryRestoreContextInterval = 0.5;
static const unsigned MaxTryRestoreContextAttempts = 4;
static const double cDeviceScaleFactor = 1.0; // Canvas is device independent

static bool contextLostRestoredEventsEnabled()
{
    return RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled();
}

// Drawing methods need to use this instead of SkAutoCanvasRestore in case overdraw
// detection substitutes the recording canvas (to discard overdrawn draw calls).
class CanvasRenderingContext2DAutoRestoreSkCanvas {
    STACK_ALLOCATED();
public:
    explicit CanvasRenderingContext2DAutoRestoreSkCanvas(CanvasRenderingContext2D* context)
        : m_context(context)
        , m_saveCount(0)
    {
        ASSERT(m_context);
        SkCanvas* c = m_context->drawingCanvas();
        if (c) {
            m_saveCount = c->getSaveCount();
        }
    }

    ~CanvasRenderingContext2DAutoRestoreSkCanvas()
    {
        SkCanvas* c = m_context->drawingCanvas();
        if (c)
            c->restoreToCount(m_saveCount);
        m_context->validateStateStack();
    }
private:
    Member<CanvasRenderingContext2D> m_context;
    int m_saveCount;
};

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* canvas, const CanvasContextCreationAttributes& attrs, Document& document)
    : CanvasRenderingContext(canvas)
    , m_hasAlpha(attrs.alpha())
    , m_contextLostMode(NotLostContext)
    , m_contextRestorable(true)
    , m_tryRestoreContextAttemptCount(0)
    , m_dispatchContextLostEventTimer(this, &CanvasRenderingContext2D::dispatchContextLostEvent)
    , m_dispatchContextRestoredEventTimer(this, &CanvasRenderingContext2D::dispatchContextRestoredEvent)
    , m_tryRestoreContextEventTimer(this, &CanvasRenderingContext2D::tryRestoreContextEvent)
    , m_pruneLocalFontCacheScheduled(false)
{
    if (document.settings() && document.settings()->antialiasedClips2dCanvasEnabled())
        m_clipAntialiasing = AntiAliased;
    setShouldAntialias(true);
    ThreadState::current()->registerPreFinalizer(this);
}

void CanvasRenderingContext2D::setCanvasGetContextResult(RenderingContext& result)
{
    result.setCanvasRenderingContext2D(this);
}

void CanvasRenderingContext2D::unwindStateStack()
{
    if (size_t stackSize = m_stateStack.size()) {
        if (SkCanvas* skCanvas = canvas()->existingDrawingCanvas()) {
            while (--stackSize)
                skCanvas->restore();
        }
    }
}

CanvasRenderingContext2D::~CanvasRenderingContext2D() { }

void CanvasRenderingContext2D::dispose()
{
    if (m_pruneLocalFontCacheScheduled)
        Platform::current()->currentThread()->removeTaskObserver(this);
}

void CanvasRenderingContext2D::validateStateStack()
{
#if ENABLE(ASSERT)
    SkCanvas* skCanvas = canvas()->existingDrawingCanvas();
    if (skCanvas && m_contextLostMode == NotLostContext) {
        ASSERT(static_cast<size_t>(skCanvas->getSaveCount()) == m_stateStack.size());
    }
#endif
}

bool CanvasRenderingContext2D::isAccelerated() const
{
    if (!canvas()->hasImageBuffer())
        return false;
    return canvas()->buffer()->isAccelerated();
}

void CanvasRenderingContext2D::stop()
{
    if (!isContextLost()) {
        // Never attempt to restore the context because the page is being torn down.
        loseContext(SyntheticLostContext);
    }
}

bool CanvasRenderingContext2D::isContextLost() const
{
    return m_contextLostMode != NotLostContext;
}

void CanvasRenderingContext2D::loseContext(LostContextMode lostMode)
{
    if (m_contextLostMode != NotLostContext)
        return;
    m_contextLostMode = lostMode;
    if (m_contextLostMode == SyntheticLostContext && canvas()) {
        canvas()->discardImageBuffer();
    }
    m_dispatchContextLostEventTimer.startOneShot(0, BLINK_FROM_HERE);
}

void CanvasRenderingContext2D::didSetSurfaceSize()
{
    if (!m_contextRestorable)
        return;
    // This code path is for restoring from an eviction
    // Restoring from surface failure is handled internally
    ASSERT(m_contextLostMode != NotLostContext && !canvas()->hasImageBuffer());

    if (canvas()->buffer()) {
        if (contextLostRestoredEventsEnabled()) {
            m_dispatchContextRestoredEventTimer.startOneShot(0, BLINK_FROM_HERE);
        } else {
            // legacy synchronous context restoration.
            reset();
            m_contextLostMode = NotLostContext;
        }
    }
}

DEFINE_TRACE(CanvasRenderingContext2D)
{
    visitor->trace(m_hitRegionManager);
    CanvasRenderingContext::trace(visitor);
    BaseRenderingContext2D::trace(visitor);
    SVGResourceClient::trace(visitor);
}

void CanvasRenderingContext2D::dispatchContextLostEvent(Timer<CanvasRenderingContext2D>*)
{
    if (canvas() && contextLostRestoredEventsEnabled()) {
        Event* event = Event::createCancelable(EventTypeNames::contextlost);
        canvas()->dispatchEvent(event);
        if (event->defaultPrevented()) {
            m_contextRestorable = false;
        }
    }

    // If RealLostContext, it means the context was not lost due to surface failure
    // but rather due to a an eviction, which means image buffer exists.
    if (m_contextRestorable && m_contextLostMode == RealLostContext) {
        m_tryRestoreContextAttemptCount = 0;
        m_tryRestoreContextEventTimer.startRepeating(TryRestoreContextInterval, BLINK_FROM_HERE);
    }
}

void CanvasRenderingContext2D::tryRestoreContextEvent(Timer<CanvasRenderingContext2D>* timer)
{
    if (m_contextLostMode == NotLostContext) {
        // Canvas was already restored (possibly thanks to a resize), so stop trying.
        m_tryRestoreContextEventTimer.stop();
        return;
    }

    DCHECK(m_contextLostMode == RealLostContext);
    if (canvas()->hasImageBuffer() && canvas()->buffer()->restoreSurface()) {
        m_tryRestoreContextEventTimer.stop();
        dispatchContextRestoredEvent(nullptr);
    }

    if (++m_tryRestoreContextAttemptCount > MaxTryRestoreContextAttempts) {
        // final attempt: allocate a brand new image buffer instead of restoring
        canvas()->discardImageBuffer();
        m_tryRestoreContextEventTimer.stop();
        if (canvas()->buffer())
            dispatchContextRestoredEvent(nullptr);
    }
}

void CanvasRenderingContext2D::dispatchContextRestoredEvent(Timer<CanvasRenderingContext2D>*)
{
    if (m_contextLostMode == NotLostContext)
        return;
    reset();
    m_contextLostMode = NotLostContext;
    if (contextLostRestoredEventsEnabled()) {
        Event* event(Event::create(EventTypeNames::contextrestored));
        canvas()->dispatchEvent(event);
    }
}

void CanvasRenderingContext2D::reset()
{
    validateStateStack();
    unwindStateStack();
    m_stateStack.resize(1);
    m_stateStack.first() = CanvasRenderingContext2DState::create();
    m_path.clear();
    SkCanvas* c = canvas()->existingDrawingCanvas();
    if (c) {
        c->resetMatrix();
        c->clipRect(SkRect::MakeWH(canvas()->width(), canvas()->height()), SkRegion::kReplace_Op);
    }
    validateStateStack();
}

void CanvasRenderingContext2D::restoreCanvasMatrixClipStack(SkCanvas* c) const
{
    restoreMatrixClipStack(c);
}

bool CanvasRenderingContext2D::shouldAntialias() const
{
    return state().shouldAntialias();
}

void CanvasRenderingContext2D::setShouldAntialias(bool doAA)
{
    modifiableState().setShouldAntialias(doAA);
}

void CanvasRenderingContext2D::scrollPathIntoView()
{
    scrollPathIntoViewInternal(m_path);
}

void CanvasRenderingContext2D::scrollPathIntoView(Path2D* path2d)
{
    scrollPathIntoViewInternal(path2d->path());
}

void CanvasRenderingContext2D::scrollPathIntoViewInternal(const Path& path)
{
    if (!state().isTransformInvertible() || path.isEmpty())
        return;

    canvas()->document().updateStyleAndLayoutIgnorePendingStylesheets();

    LayoutObject* renderer = canvas()->layoutObject();
    LayoutBox* layoutBox = canvas()->layoutBox();
    if (!renderer || !layoutBox)
        return;

    // Apply transformation and get the bounding rect
    Path transformedPath = path;
    transformedPath.transform(state().transform());
    FloatRect boundingRect = transformedPath.boundingRect();

    // Offset by the canvas rect
    LayoutRect pathRect(boundingRect);
    IntRect canvasRect = layoutBox->absoluteContentBox();
    pathRect.moveBy(canvasRect.location());

    renderer->scrollRectToVisible(
        pathRect, ScrollAlignment::alignCenterAlways, ScrollAlignment::alignTopAlways);

    // TODO: should implement "inform the user" that the caret and/or
    // selection the specified rectangle of the canvas. See http://crbug.com/357987
}

void CanvasRenderingContext2D::clearRect(double x, double y, double width, double height)
{
    BaseRenderingContext2D::clearRect(x, y, width, height);

    if (m_hitRegionManager) {
        FloatRect rect(x, y, width, height);
        m_hitRegionManager->removeHitRegionsInRect(rect, state().transform());
    }
}

void CanvasRenderingContext2D::didDraw(const SkIRect& dirtyRect)
{
    if (dirtyRect.isEmpty())
        return;

    if (ExpensiveCanvasHeuristicParameters::BlurredShadowsAreExpensive && state().shouldDrawShadows() && state().shadowBlur() > 0) {
        ImageBuffer* buffer = canvas()->buffer();
        if (buffer)
            buffer->setHasExpensiveOp();
    }

    canvas()->didDraw(SkRect::Make(dirtyRect));
}

bool CanvasRenderingContext2D::stateHasFilter()
{
    return state().hasFilter(canvas(), canvas()->size(), this);
}

SkImageFilter* CanvasRenderingContext2D::stateGetFilter()
{
    return state().getFilter(canvas(), canvas()->size(), this);
}

void CanvasRenderingContext2D::snapshotStateForFilter()
{
    // The style resolution required for fonts is not available in frame-less documents.
    if (!canvas()->document().frame())
        return;

    modifiableState().setFontForFilter(accessFont());
}

SkCanvas* CanvasRenderingContext2D::drawingCanvas() const
{
    if (isContextLost())
        return nullptr;
    return canvas()->drawingCanvas();
}

SkCanvas* CanvasRenderingContext2D::existingDrawingCanvas() const
{
    return canvas()->existingDrawingCanvas();
}

void CanvasRenderingContext2D::disableDeferral(DisableDeferralReason reason)
{
    canvas()->disableDeferral(reason);
}

AffineTransform CanvasRenderingContext2D::baseTransform() const
{
    return canvas()->baseTransform();
}

String CanvasRenderingContext2D::font() const
{
    if (!state().hasRealizedFont())
        return defaultFont;

    canvas()->document().canvasFontCache()->willUseCurrentFont();
    StringBuilder serializedFont;
    const FontDescription& fontDescription = state().font().getFontDescription();

    if (fontDescription.style() == FontStyleItalic)
        serializedFont.append("italic ");
    if (fontDescription.weight() == FontWeightBold)
        serializedFont.append("bold ");
    if (fontDescription.variantCaps() == FontDescription::SmallCaps)
        serializedFont.append("small-caps ");

    serializedFont.appendNumber(fontDescription.computedPixelSize());
    serializedFont.append("px");

    const FontFamily& firstFontFamily = fontDescription.family();
    for (const FontFamily* fontFamily = &firstFontFamily; fontFamily; fontFamily = fontFamily->next()) {
        if (fontFamily != &firstFontFamily)
            serializedFont.append(',');

        // FIXME: We should append family directly to serializedFont rather than building a temporary string.
        String family = fontFamily->family();
        if (family.startsWith("-webkit-"))
            family = family.substring(8);
        if (family.contains(' '))
            family = "\"" + family + "\"";

        serializedFont.append(' ');
        serializedFont.append(family);
    }

    return serializedFont.toString();
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return;

    canvas()->document().updateStyleAndLayoutTreeForNode(canvas());

    // The following early exit is dependent on the cache not being empty
    // because an empty cache may indicate that a style change has occured
    // which would require that the font be re-resolved. This check has to
    // come after the layout tree update to flush pending style changes.
    if (newFont == state().unparsedFont() && state().hasRealizedFont() && m_fontsResolvedUsingCurrentStyle.size() > 0)
        return;

    CanvasFontCache* canvasFontCache = canvas()->document().canvasFontCache();

    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    RefPtr<ComputedStyle> fontStyle;
    const ComputedStyle* computedStyle = canvas()->ensureComputedStyle();
    if (computedStyle) {
        HashMap<String, Font>::iterator i = m_fontsResolvedUsingCurrentStyle.find(newFont);
        if (i != m_fontsResolvedUsingCurrentStyle.end()) {
            ASSERT(m_fontLRUList.contains(newFont));
            m_fontLRUList.remove(newFont);
            m_fontLRUList.add(newFont);
            modifiableState().setFont(i->value, canvas()->document().styleEngine().fontSelector());
        } else {
            MutableStylePropertySet* parsedStyle = canvasFontCache->parseFont(newFont);
            if (!parsedStyle)
                return;
            fontStyle = ComputedStyle::create();
            FontDescription elementFontDescription(computedStyle->getFontDescription());
            // Reset the computed size to avoid inheriting the zoom factor from the <canvas> element.
            elementFontDescription.setComputedSize(elementFontDescription.specifiedSize());
            fontStyle->setFontDescription(elementFontDescription);
            fontStyle->font().update(fontStyle->font().getFontSelector());
            canvas()->document().ensureStyleResolver().computeFont(fontStyle.get(), *parsedStyle);
            m_fontsResolvedUsingCurrentStyle.add(newFont, fontStyle->font());
            ASSERT(!m_fontLRUList.contains(newFont));
            m_fontLRUList.add(newFont);
            pruneLocalFontCache(canvasFontCache->hardMaxFonts()); // hard limit
            schedulePruneLocalFontCacheIfNeeded(); // soft limit
            modifiableState().setFont(fontStyle->font(), canvas()->document().styleEngine().fontSelector());
        }
    } else {
        Font resolvedFont;
        if (!canvasFontCache->getFontUsingDefaultStyle(newFont, resolvedFont))
            return;
        modifiableState().setFont(resolvedFont, canvas()->document().styleEngine().fontSelector());
    }

    // The parse succeeded.
    String newFontSafeCopy(newFont); // Create a string copy since newFont can be deleted inside realizeSaves.
    modifiableState().setUnparsedFont(newFontSafeCopy);
}

void CanvasRenderingContext2D::schedulePruneLocalFontCacheIfNeeded()
{
    if (m_pruneLocalFontCacheScheduled)
        return;
    m_pruneLocalFontCacheScheduled = true;
    Platform::current()->currentThread()->addTaskObserver(this);
}

void CanvasRenderingContext2D::didProcessTask()
{
    Platform::current()->currentThread()->removeTaskObserver(this);

    // This should be the only place where canvas() needs to be checked for nullness
    // because the circular refence with HTMLCanvasElement mean the canvas and the
    // context keep each other alive as long as the pair is referenced the task
    // observer is the only persisten refernce to this object that is not traced,
    // so didProcessTask() may be call at a time when the canvas has been garbage
    // collected but not the context.
    if (!canvas())
        return;

    // The rendering surface needs to be prepared now because it will be too late
    // to create a layer once we are in the paint invalidation phase.
    canvas()->prepareSurfaceForPaintingIfNeeded();

    pruneLocalFontCache(canvas()->document().canvasFontCache()->maxFonts());
    m_pruneLocalFontCacheScheduled = false;
}

void CanvasRenderingContext2D::pruneLocalFontCache(size_t targetSize)
{
    if (targetSize == 0) {
        // Short cut: LRU does not matter when evicting everything
        m_fontLRUList.clear();
        m_fontsResolvedUsingCurrentStyle.clear();
        return;
    }
    while (m_fontLRUList.size() > targetSize) {
        m_fontsResolvedUsingCurrentStyle.remove(m_fontLRUList.first());
        m_fontLRUList.removeFirst();
    }
}

void CanvasRenderingContext2D::styleDidChange(const ComputedStyle* oldStyle, const ComputedStyle& newStyle)
{
    if (oldStyle && oldStyle->font() == newStyle.font())
        return;
    pruneLocalFontCache(0);
}

void CanvasRenderingContext2D::filterNeedsInvalidation()
{
    state().clearResolvedFilter();
}

bool CanvasRenderingContext2D::originClean() const
{
    return canvas()->originClean();
}

void CanvasRenderingContext2D::setOriginTainted()
{
    return canvas()->setOriginTainted();
}

int CanvasRenderingContext2D::width() const
{
    return canvas()->width();
}

int CanvasRenderingContext2D::height() const
{
    return canvas()->height();
}

bool CanvasRenderingContext2D::hasImageBuffer() const
{
    return canvas()->hasImageBuffer();
}

ImageBuffer* CanvasRenderingContext2D::imageBuffer() const
{
    return canvas()->buffer();
}

bool CanvasRenderingContext2D::parseColorOrCurrentColor(Color& color, const String& colorString) const
{
    return ::blink::parseColorOrCurrentColor(color, colorString, canvas());
}

std::pair<Element*, String> CanvasRenderingContext2D::getControlAndIdIfHitRegionExists(const LayoutPoint& location)
{
    if (hitRegionsCount() <= 0)
        return std::make_pair(nullptr, String());

    LayoutBox* box = canvas()->layoutBox();
    FloatPoint localPos = box->absoluteToLocal(FloatPoint(location), UseTransforms);
    if (box->hasBorderOrPadding())
        localPos.move(-box->contentBoxOffset());
    localPos.scale(canvas()->width() / box->contentWidth(), canvas()->height() / box->contentHeight());

    HitRegion* hitRegion = hitRegionAtPoint(localPos);
    if (hitRegion) {
        Element* control = hitRegion->control();
        if (control && canvas()->isSupportedInteractiveCanvasFallback(*control))
            return std::make_pair(hitRegion->control(), hitRegion->id());
        return std::make_pair(nullptr, hitRegion->id());
    }
    return std::make_pair(nullptr, String());
}

String CanvasRenderingContext2D::getIdFromControl(const Element* element)
{
    if (hitRegionsCount() <= 0)
        return String();

    if (HitRegion* hitRegion = m_hitRegionManager->getHitRegionByControl(element))
        return hitRegion->id();
    return String();
}

String CanvasRenderingContext2D::textAlign() const
{
    return textAlignName(state().getTextAlign());
}

void CanvasRenderingContext2D::setTextAlign(const String& s)
{
    TextAlign align;
    if (!parseTextAlign(s, align))
        return;
    if (state().getTextAlign() == align)
        return;
    modifiableState().setTextAlign(align);
}

String CanvasRenderingContext2D::textBaseline() const
{
    return textBaselineName(state().getTextBaseline());
}

void CanvasRenderingContext2D::setTextBaseline(const String& s)
{
    TextBaseline baseline;
    if (!parseTextBaseline(s, baseline))
        return;
    if (state().getTextBaseline() == baseline)
        return;
    modifiableState().setTextBaseline(baseline);
}

static inline TextDirection toTextDirection(CanvasRenderingContext2DState::Direction direction, HTMLCanvasElement* canvas, const ComputedStyle** computedStyle = 0)
{
    const ComputedStyle* style = (computedStyle || direction == CanvasRenderingContext2DState::DirectionInherit) ? canvas->ensureComputedStyle() : nullptr;
    if (computedStyle)
        *computedStyle = style;
    switch (direction) {
    case CanvasRenderingContext2DState::DirectionInherit:
        return style ? style->direction() : LTR;
    case CanvasRenderingContext2DState::DirectionRTL:
        return RTL;
    case CanvasRenderingContext2DState::DirectionLTR:
        return LTR;
    }
    ASSERT_NOT_REACHED();
    return LTR;
}

String CanvasRenderingContext2D::direction() const
{
    if (state().getDirection() == CanvasRenderingContext2DState::DirectionInherit)
        canvas()->document().updateStyleAndLayoutTreeForNode(canvas());
    return toTextDirection(state().getDirection(), canvas()) == RTL ? rtl : ltr;
}

void CanvasRenderingContext2D::setDirection(const String& directionString)
{
    CanvasRenderingContext2DState::Direction direction;
    if (directionString == inherit)
        direction = CanvasRenderingContext2DState::DirectionInherit;
    else if (directionString == rtl)
        direction = CanvasRenderingContext2DState::DirectionRTL;
    else if (directionString == ltr)
        direction = CanvasRenderingContext2DState::DirectionLTR;
    else
        return;

    if (state().getDirection() == direction)
        return;

    modifiableState().setDirection(direction);
}

void CanvasRenderingContext2D::fillText(const String& text, double x, double y)
{
    trackDrawCall(FillText);
    drawTextInternal(text, x, y, CanvasRenderingContext2DState::FillPaintType);
}

void CanvasRenderingContext2D::fillText(const String& text, double x, double y, double maxWidth)
{
    trackDrawCall(FillText);
    drawTextInternal(text, x, y, CanvasRenderingContext2DState::FillPaintType, &maxWidth);
}

void CanvasRenderingContext2D::strokeText(const String& text, double x, double y)
{
    trackDrawCall(StrokeText);
    drawTextInternal(text, x, y, CanvasRenderingContext2DState::StrokePaintType);
}

void CanvasRenderingContext2D::strokeText(const String& text, double x, double y, double maxWidth)
{
    trackDrawCall(StrokeText);
    drawTextInternal(text, x, y, CanvasRenderingContext2DState::StrokePaintType, &maxWidth);
}

TextMetrics* CanvasRenderingContext2D::measureText(const String& text)
{
    TextMetrics* metrics = TextMetrics::create();

    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return metrics;

    canvas()->document().updateStyleAndLayoutTreeForNode(canvas());
    const Font& font = accessFont();

    TextDirection direction;
    if (state().getDirection() == CanvasRenderingContext2DState::DirectionInherit)
        direction = determineDirectionality(text);
    else
        direction = toTextDirection(state().getDirection(), canvas());
    TextRun textRun(text, 0, 0, TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion, direction, false);
    textRun.setNormalizeSpace(true);
    FloatRect textBounds = font.selectionRectForText(textRun, FloatPoint(), font.getFontDescription().computedSize(), 0, -1, true);

    // x direction
    metrics->setWidth(font.width(textRun));
    metrics->setActualBoundingBoxLeft(-textBounds.x());
    metrics->setActualBoundingBoxRight(textBounds.maxX());

    // y direction
    const FontMetrics& fontMetrics = font.getFontMetrics();
    const float ascent = fontMetrics.floatAscent();
    const float descent = fontMetrics.floatDescent();
    const float baselineY = getFontBaseline(fontMetrics);

    metrics->setFontBoundingBoxAscent(ascent - baselineY);
    metrics->setFontBoundingBoxDescent(descent + baselineY);
    metrics->setActualBoundingBoxAscent(-textBounds.y() - baselineY);
    metrics->setActualBoundingBoxDescent(textBounds.maxY() + baselineY);

    // Note : top/bottom and ascend/descend are currently the same, so there's no difference
    //        between the EM box's top and bottom and the font's ascend and descend
    metrics->setEmHeightAscent(0);
    metrics->setEmHeightDescent(0);

    metrics->setHangingBaseline(-0.8f * ascent + baselineY);
    metrics->setAlphabeticBaseline(baselineY);
    metrics->setIdeographicBaseline(descent + baselineY);
    return metrics;
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, double x, double y, CanvasRenderingContext2DState::PaintType paintType, double* maxWidth)
{
    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return;

    // accessFont needs the style to be up to date, but updating style can cause script to run,
    // (e.g. due to autofocus) which can free the canvas (set size to 0, for example), so update
    // style before grabbing the drawingCanvas.
    canvas()->document().updateStyleAndLayoutTreeForNode(canvas());

    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    if (maxWidth && (!std::isfinite(*maxWidth) || *maxWidth <= 0))
        return;

    // Currently, SkPictureImageFilter does not support subpixel text anti-aliasing, which
    // is expected when !m_hasAlpha, so we need to fall out of display list mode when
    // drawing text to an opaque canvas
    // crbug.com/583809
    if (!m_hasAlpha && !isAccelerated())
        canvas()->disableDeferral(DisableDeferralReasonSubPixelTextAntiAliasingSupport);

    const Font& font = accessFont();
    if (!font.primaryFont())
        return;

    const FontMetrics& fontMetrics = font.getFontMetrics();

    // FIXME: Need to turn off font smoothing.

    const ComputedStyle* computedStyle = 0;
    TextDirection direction = toTextDirection(state().getDirection(), canvas(), &computedStyle);
    bool isRTL = direction == RTL;
    bool override = computedStyle ? isOverride(computedStyle->unicodeBidi()) : false;

    TextRun textRun(text, 0, 0, TextRun::AllowTrailingExpansion, direction, override);
    textRun.setNormalizeSpace(true);
    // Draw the item text at the correct point.
    FloatPoint location(x, y + getFontBaseline(fontMetrics));
    double fontWidth = font.width(textRun);

    bool useMaxWidth = (maxWidth && *maxWidth < fontWidth);
    double width = useMaxWidth ? *maxWidth : fontWidth;

    TextAlign align = state().getTextAlign();
    if (align == StartTextAlign)
        align = isRTL ? RightTextAlign : LeftTextAlign;
    else if (align == EndTextAlign)
        align = isRTL ? LeftTextAlign : RightTextAlign;

    switch (align) {
    case CenterTextAlign:
        location.setX(location.x() - width / 2);
        break;
    case RightTextAlign:
        location.setX(location.x() - width);
        break;
    default:
        break;
    }

    // The slop built in to this mask rect matches the heuristic used in FontCGWin.cpp for GDI text.
    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = FloatRect(location.x() - fontMetrics.height() / 2,
        location.y() - fontMetrics.ascent() - fontMetrics.lineGap(),
        width + fontMetrics.height(),
        fontMetrics.lineSpacing());
    if (paintType == CanvasRenderingContext2DState::StrokePaintType)
        inflateStrokeRect(textRunPaintInfo.bounds);

    CanvasRenderingContext2DAutoRestoreSkCanvas stateRestorer(this);
    if (useMaxWidth) {
        drawingCanvas()->save();
        drawingCanvas()->translate(location.x(), location.y());
        // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
        drawingCanvas()->scale((fontWidth > 0 ? (width / fontWidth) : 0), 1);
        location = FloatPoint();
    }

    draw(
        [&font, this, &textRunPaintInfo, &location](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            font.drawBidiText(c, textRunPaintInfo, location, Font::UseFallbackIfFontNotReady, cDeviceScaleFactor, *paint);
        },
        [](const SkIRect& rect) // overdraw test lambda
        {
            return false;
        },
        textRunPaintInfo.bounds, paintType);
}

const Font& CanvasRenderingContext2D::accessFont()
{
    if (!state().hasRealizedFont())
        setFont(state().unparsedFont());
    canvas()->document().canvasFontCache()->willUseCurrentFont();
    return state().font();
}

int CanvasRenderingContext2D::getFontBaseline(const FontMetrics& fontMetrics) const
{
    switch (state().getTextBaseline()) {
    case TopTextBaseline:
        return fontMetrics.ascent();
    case HangingTextBaseline:
        // According to http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
        // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of the ascender height"
        return (fontMetrics.ascent() * 4) / 5;
    case BottomTextBaseline:
    case IdeographicTextBaseline:
        return -fontMetrics.descent();
    case MiddleTextBaseline:
        return -fontMetrics.descent() + fontMetrics.height() / 2;
    case AlphabeticTextBaseline:
    default:
        // Do nothing.
        break;
    }
    return 0;
}

void CanvasRenderingContext2D::setIsHidden(bool hidden)
{
    if (canvas()->hasImageBuffer())
        canvas()->buffer()->setIsHidden(hidden);
    if (hidden) {
        pruneLocalFontCache(0);
    }
}

bool CanvasRenderingContext2D::isTransformInvertible() const
{
    return state().isTransformInvertible();
}

WebLayer* CanvasRenderingContext2D::platformLayer() const
{
    return canvas()->buffer() ? canvas()->buffer()->platformLayer() : 0;
}

void CanvasRenderingContext2D::getContextAttributes(Canvas2DContextAttributes& attrs) const
{
    attrs.setAlpha(m_hasAlpha);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Element* element)
{
    drawFocusIfNeededInternal(m_path, element);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Path2D* path2d, Element* element)
{
    drawFocusIfNeededInternal(path2d->path(), element);
}

void CanvasRenderingContext2D::drawFocusIfNeededInternal(const Path& path, Element* element)
{
    if (!focusRingCallIsValid(path, element))
        return;

    // Note: we need to check document->focusedElement() rather than just calling
    // element->focused(), because element->focused() isn't updated until after
    // focus events fire.
    if (element->document().focusedElement() == element) {
        scrollPathIntoViewInternal(path);
        drawFocusRing(path);
    }

    // Update its accessible bounds whether it's focused or not.
    updateElementAccessibility(path, element);
}

bool CanvasRenderingContext2D::focusRingCallIsValid(const Path& path, Element* element)
{
    ASSERT(element);
    if (!state().isTransformInvertible())
        return false;
    if (path.isEmpty())
        return false;
    if (!element->isDescendantOf(canvas()))
        return false;

    return true;
}

void CanvasRenderingContext2D::drawFocusRing(const Path& path)
{
    m_usageCounters.numDrawFocusCalls++;
    if (!drawingCanvas())
        return;

    SkColor color = LayoutTheme::theme().focusRingColor().rgb();
    const int focusRingWidth = 5;

    drawPlatformFocusRing(path.getSkPath(), drawingCanvas(), color, focusRingWidth);

    // We need to add focusRingWidth to dirtyRect.
    StrokeData strokeData;
    strokeData.setThickness(focusRingWidth);

    SkIRect dirtyRect;
    if (!computeDirtyRect(path.strokeBoundingRect(strokeData), &dirtyRect))
        return;

    didDraw(dirtyRect);
}

void CanvasRenderingContext2D::updateElementAccessibility(const Path& path, Element* element)
{
    element->document().updateStyleAndLayoutIgnorePendingStylesheets();
    AXObjectCache* axObjectCache = element->document().existingAXObjectCache();
    LayoutBoxModelObject* lbmo = canvas()->layoutBoxModelObject();
    LayoutObject* renderer = canvas()->layoutObject();
    if (!axObjectCache || !lbmo || !renderer)
        return;

    // Get the transformed path.
    Path transformedPath = path;
    transformedPath.transform(state().transform());

    // Offset by the canvas rect, taking border and padding into account.
    IntRect canvasRect = renderer->absoluteBoundingBoxRect();
    canvasRect.move(lbmo->borderLeft() + lbmo->paddingLeft(), lbmo->borderTop() + lbmo->paddingTop());
    LayoutRect elementRect = enclosingLayoutRect(transformedPath.boundingRect());
    elementRect.moveBy(canvasRect.location());
    axObjectCache->setCanvasObjectBounds(element, elementRect);
}

void CanvasRenderingContext2D::addHitRegion(const HitRegionOptions& options, ExceptionState& exceptionState)
{
    if (options.id().isEmpty() && !options.control()) {
        exceptionState.throwDOMException(NotSupportedError, "Both id and control are null.");
        return;
    }

    if (options.control() && !canvas()->isSupportedInteractiveCanvasFallback(*options.control())) {
        exceptionState.throwDOMException(NotSupportedError, "The control is neither null nor a supported interactive canvas fallback element.");
        return;
    }

    Path hitRegionPath = options.hasPath() ? options.path()->path() : m_path;

    SkCanvas* c = drawingCanvas();

    if (hitRegionPath.isEmpty() || !c || !state().isTransformInvertible()
        || !c->getClipDeviceBounds(0)) {
        exceptionState.throwDOMException(NotSupportedError, "The specified path has no pixels.");
        return;
    }

    hitRegionPath.transform(state().transform());

    if (state().hasClip()) {
        hitRegionPath.intersectPath(state().getCurrentClipPath());
        if (hitRegionPath.isEmpty())
            exceptionState.throwDOMException(NotSupportedError, "The specified path has no pixels.");
    }

    if (!m_hitRegionManager)
        m_hitRegionManager = HitRegionManager::create();

    // Remove previous region (with id or control)
    m_hitRegionManager->removeHitRegionById(options.id());
    m_hitRegionManager->removeHitRegionByControl(options.control());

    HitRegion* hitRegion = HitRegion::create(hitRegionPath, options);
    Element* element = hitRegion->control();
    if (element && element->isDescendantOf(canvas()))
        updateElementAccessibility(hitRegion->path(), hitRegion->control());
    m_hitRegionManager->addHitRegion(hitRegion);
}

void CanvasRenderingContext2D::removeHitRegion(const String& id)
{
    if (m_hitRegionManager)
        m_hitRegionManager->removeHitRegionById(id);
}

void CanvasRenderingContext2D::clearHitRegions()
{
    if (m_hitRegionManager)
        m_hitRegionManager->removeAllHitRegions();
}

HitRegion* CanvasRenderingContext2D::hitRegionAtPoint(const FloatPoint& point)
{
    if (m_hitRegionManager)
        return m_hitRegionManager->getHitRegionAtPoint(point);

    return nullptr;
}

unsigned CanvasRenderingContext2D::hitRegionsCount() const
{
    if (m_hitRegionManager)
        return m_hitRegionManager->getHitRegionsCount();

    return 0;
}

} // namespace blink
