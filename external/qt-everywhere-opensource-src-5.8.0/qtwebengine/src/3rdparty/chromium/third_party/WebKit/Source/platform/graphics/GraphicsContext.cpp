/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/GraphicsContext.h"

#include "platform/TraceEvent.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/ColorSpace.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/Path.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/weborigin/KURL.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "third_party/skia/include/effects/SkPictureImageFilter.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "wtf/Assertions.h"
#include "wtf/MathExtras.h"
#include <memory>

namespace blink {

GraphicsContext::GraphicsContext(PaintController& paintController, DisabledMode disableContextOrPainting, SkMetaData* metaData)
    : m_canvas(nullptr)
    , m_paintController(paintController)
    , m_paintStateStack()
    , m_paintStateIndex(0)
#if ENABLE(ASSERT)
    , m_layerCount(0)
    , m_disableDestructionChecks(false)
    , m_inDrawingRecorder(false)
#endif
    , m_disabledState(disableContextOrPainting)
    , m_deviceScaleFactor(1.0f)
    , m_printing(false)
    , m_hasMetaData(!!metaData)
{
    if (metaData)
        m_metaData = *metaData;

    // FIXME: Do some tests to determine how many states are typically used, and allocate
    // several here.
    m_paintStateStack.append(GraphicsContextState::create());
    m_paintState = m_paintStateStack.last().get();

    if (contextDisabled()) {
        DEFINE_STATIC_REF(SkCanvas, nullCanvas, (adoptRef(SkCreateNullCanvas())));
        m_canvas = nullCanvas;
    }
}

GraphicsContext::~GraphicsContext()
{
#if ENABLE(ASSERT)
    if (!m_disableDestructionChecks) {
        ASSERT(!m_paintStateIndex);
        ASSERT(!m_paintState->saveCount());
        ASSERT(!m_layerCount);
        ASSERT(!saveCount());
    }
#endif
}

void GraphicsContext::save()
{
    if (contextDisabled())
        return;

    m_paintState->incrementSaveCount();

    ASSERT(m_canvas);
    m_canvas->save();
}

void GraphicsContext::restore()
{
    if (contextDisabled())
        return;

    if (!m_paintStateIndex && !m_paintState->saveCount()) {
        DLOG(ERROR) << "ERROR void GraphicsContext::restore() stack is empty";
        return;
    }

    if (m_paintState->saveCount()) {
        m_paintState->decrementSaveCount();
    } else {
        m_paintStateIndex--;
        m_paintState = m_paintStateStack[m_paintStateIndex].get();
    }

    ASSERT(m_canvas);
    m_canvas->restore();
}

#if ENABLE(ASSERT)
unsigned GraphicsContext::saveCount() const
{
    // Each m_paintStateStack entry implies an additional save op
    // (on top of its own saveCount), except for the first frame.
    unsigned count = m_paintStateIndex;
    ASSERT(m_paintStateStack.size() > m_paintStateIndex);
    for (unsigned i = 0; i <= m_paintStateIndex; ++i)
        count += m_paintStateStack[i]->saveCount();

    return count;
}
#endif

void GraphicsContext::saveLayer(const SkRect* bounds, const SkPaint* paint)
{
    if (contextDisabled())
        return;

    ASSERT(m_canvas);

    m_canvas->saveLayer(bounds, paint);
}

void GraphicsContext::restoreLayer()
{
    if (contextDisabled())
        return;

    ASSERT(m_canvas);

    m_canvas->restore();
}

#if ENABLE(ASSERT)
void GraphicsContext::setInDrawingRecorder(bool val)
{
    // Nested drawing recorers are not allowed.
    ASSERT(!val || !m_inDrawingRecorder);
    m_inDrawingRecorder = val;
}
#endif

void GraphicsContext::setShadow(const FloatSize& offset, float blur, const Color& color,
    DrawLooperBuilder::ShadowTransformMode shadowTransformMode,
    DrawLooperBuilder::ShadowAlphaMode shadowAlphaMode, ShadowMode shadowMode)
{
    if (contextDisabled())
        return;

    std::unique_ptr<DrawLooperBuilder> drawLooperBuilder = DrawLooperBuilder::create();
    if (!color.alpha()) {
        // When shadow-only but there is no shadow, we use an empty draw looper
        // to disable rendering of the source primitive.  When not shadow-only, we
        // clear the looper.
        if (shadowMode != DrawShadowOnly)
            drawLooperBuilder.reset();

        setDrawLooper(std::move(drawLooperBuilder));
        return;
    }

    drawLooperBuilder->addShadow(offset, blur, color, shadowTransformMode, shadowAlphaMode);
    if (shadowMode == DrawShadowAndForeground) {
        drawLooperBuilder->addUnmodifiedContent();
    }
    setDrawLooper(std::move(drawLooperBuilder));
}

void GraphicsContext::setDrawLooper(std::unique_ptr<DrawLooperBuilder> drawLooperBuilder)
{
    if (contextDisabled())
        return;

    mutableState()->setDrawLooper(drawLooperBuilder ? drawLooperBuilder->detachDrawLooper() : nullptr);
}

SkColorFilter* GraphicsContext::colorFilter() const
{
    return immutableState()->colorFilter();
}

void GraphicsContext::setColorFilter(ColorFilter colorFilter)
{
    GraphicsContextState* stateToSet = mutableState();

    // We only support one active color filter at the moment. If (when) this becomes a problem,
    // we should switch to using color filter chains (Skia work in progress).
    ASSERT(!stateToSet->colorFilter());
    stateToSet->setColorFilter(WebCoreColorFilterToSkiaColorFilter(colorFilter));
}

void GraphicsContext::concat(const SkMatrix& matrix)
{
    if (contextDisabled())
        return;

    ASSERT(m_canvas);

    m_canvas->concat(matrix);
}

void GraphicsContext::beginLayer(float opacity, SkXfermode::Mode xfermode, const FloatRect* bounds, ColorFilter colorFilter, sk_sp<SkImageFilter> imageFilter)
{
    if (contextDisabled())
        return;

    SkPaint layerPaint;
    layerPaint.setAlpha(static_cast<unsigned char>(opacity * 255));
    layerPaint.setXfermodeMode(xfermode);
    layerPaint.setColorFilter(toSkSp(WebCoreColorFilterToSkiaColorFilter(colorFilter)));
    layerPaint.setImageFilter(std::move(imageFilter));

    if (bounds) {
        SkRect skBounds = *bounds;
        saveLayer(&skBounds, &layerPaint);
    } else {
        saveLayer(nullptr, &layerPaint);
    }

#if ENABLE(ASSERT)
    ++m_layerCount;
#endif
}

void GraphicsContext::endLayer()
{
    if (contextDisabled())
        return;

    restoreLayer();

    ASSERT(m_layerCount-- > 0);
}

void GraphicsContext::beginRecording(const FloatRect& bounds)
{
    if (contextDisabled())
        return;

    m_canvas = m_pictureRecorder.beginRecording(bounds, nullptr);
    if (m_hasMetaData)
        skia::GetMetaData(*m_canvas) = m_metaData;
}

namespace {

PassRefPtr<SkPicture> createEmptyPicture()
{
    SkPictureRecorder recorder;
    recorder.beginRecording(SkRect::MakeEmpty(), nullptr);
    return fromSkSp(recorder.finishRecordingAsPicture());
}

} // anonymous namespace

PassRefPtr<SkPicture> GraphicsContext::endRecording()
{
    if (contextDisabled()) {
        // Clients expect endRecording() to always return a non-null picture.
        // Cache an empty SKP to minimize overhead when disabled.
        DEFINE_STATIC_REF(SkPicture, emptyPicture, createEmptyPicture());
        return emptyPicture;
    }

    RefPtr<SkPicture> picture = fromSkSp(m_pictureRecorder.finishRecordingAsPicture());
    m_canvas = nullptr;
    ASSERT(picture);
    return picture.release();
}

void GraphicsContext::drawPicture(const SkPicture* picture)
{
    if (contextDisabled() || !picture || picture->cullRect().isEmpty())
        return;

    ASSERT(m_canvas);
    m_canvas->drawPicture(picture);
}

void GraphicsContext::compositePicture(PassRefPtr<SkPicture> picture, const FloatRect& dest, const FloatRect& src, SkXfermode::Mode op)
{
    if (contextDisabled() || !picture)
        return;
    ASSERT(m_canvas);

    SkPaint picturePaint;
    picturePaint.setXfermodeMode(op);
    m_canvas->save();
    SkRect sourceBounds = src;
    SkRect skBounds = dest;
    SkMatrix pictureTransform;
    pictureTransform.setRectToRect(sourceBounds, skBounds, SkMatrix::kFill_ScaleToFit);
    m_canvas->concat(pictureTransform);
    picturePaint.setImageFilter(SkPictureImageFilter::MakeForLocalSpace(toSkSp(picture), sourceBounds, static_cast<SkFilterQuality>(imageInterpolationQuality())));
    m_canvas->saveLayer(&sourceBounds, &picturePaint);
    m_canvas->restore();
    m_canvas->restore();
}

void GraphicsContext::drawFocusRingPath(const SkPath& path, const Color& color, int width)
{
    drawPlatformFocusRing(path, m_canvas, color.rgb(), width);
}

void GraphicsContext::drawFocusRingRect(const SkRect& rect, const Color& color, int width)
{
    drawPlatformFocusRing(rect, m_canvas, color.rgb(), width);
}

void GraphicsContext::drawFocusRing(const Path& focusRingPath, int width, int offset, const Color& color)
{
    // FIXME: Implement support for offset.
    if (contextDisabled())
        return;

    drawFocusRingPath(focusRingPath.getSkPath(), color, width);
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
{
    if (contextDisabled())
        return;

    unsigned rectCount = rects.size();
    if (!rectCount)
        return;

    SkRegion focusRingRegion;
    offset = focusRingOffset(offset);
    for (unsigned i = 0; i < rectCount; i++) {
        SkIRect r = rects[i];
        if (r.isEmpty())
            continue;
        r.inset(-offset, -offset);
        focusRingRegion.op(r, SkRegion::kUnion_Op);
    }

    if (focusRingRegion.isEmpty())
        return;

    if (focusRingRegion.isRect()) {
        drawFocusRingRect(SkRect::Make(focusRingRegion.getBounds()), color, width);
    } else {
        SkPath path;
        if (focusRingRegion.getBoundaryPath(&path))
            drawFocusRingPath(path, color, width);
    }
}

static inline FloatRect areaCastingShadowInHole(const FloatRect& holeRect, float shadowBlur,
    float shadowSpread, const FloatSize& shadowOffset)
{
    FloatRect bounds(holeRect);

    bounds.inflate(shadowBlur);

    if (shadowSpread < 0)
        bounds.inflate(-shadowSpread);

    FloatRect offsetBounds = bounds;
    offsetBounds.move(-shadowOffset);
    return unionRect(bounds, offsetBounds);
}

void GraphicsContext::drawInnerShadow(const FloatRoundedRect& rect, const Color& shadowColor,
    const FloatSize& shadowOffset, float shadowBlur, float shadowSpread, Edges clippedEdges)
{
    if (contextDisabled())
        return;

    FloatRect holeRect(rect.rect());
    holeRect.inflate(-shadowSpread);

    if (holeRect.isEmpty()) {
        fillRoundedRect(rect, shadowColor);
        return;
    }

    if (clippedEdges & LeftEdge) {
        holeRect.move(-std::max(shadowOffset.width(), 0.0f) - shadowBlur, 0);
        holeRect.setWidth(holeRect.width() + std::max(shadowOffset.width(), 0.0f) + shadowBlur);
    }
    if (clippedEdges & TopEdge) {
        holeRect.move(0, -std::max(shadowOffset.height(), 0.0f) - shadowBlur);
        holeRect.setHeight(holeRect.height() + std::max(shadowOffset.height(), 0.0f) + shadowBlur);
    }
    if (clippedEdges & RightEdge)
        holeRect.setWidth(holeRect.width() - std::min(shadowOffset.width(), 0.0f) + shadowBlur);
    if (clippedEdges & BottomEdge)
        holeRect.setHeight(holeRect.height() - std::min(shadowOffset.height(), 0.0f) + shadowBlur);

    Color fillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), 255);

    FloatRect outerRect = areaCastingShadowInHole(rect.rect(), shadowBlur, shadowSpread, shadowOffset);
    FloatRoundedRect roundedHole(holeRect, rect.getRadii());

    GraphicsContextStateSaver stateSaver(*this);
    if (rect.isRounded()) {
        clipRoundedRect(rect);
        if (shadowSpread < 0)
            roundedHole.expandRadii(-shadowSpread);
        else
            roundedHole.shrinkRadii(shadowSpread);
    } else {
        clip(rect.rect());
    }

    std::unique_ptr<DrawLooperBuilder> drawLooperBuilder = DrawLooperBuilder::create();
    drawLooperBuilder->addShadow(FloatSize(shadowOffset), shadowBlur, shadowColor,
        DrawLooperBuilder::ShadowRespectsTransforms, DrawLooperBuilder::ShadowIgnoresAlpha);
    setDrawLooper(std::move(drawLooperBuilder));
    fillRectWithRoundedHole(outerRect, roundedHole, fillColor);
}

void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    StrokeStyle penStyle = getStrokeStyle();
    if (penStyle == NoStroke)
        return;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    int width = roundf(strokeThickness());

    // We know these are vertical or horizontal lines, so the length will just
    // be the sum of the displacement component vectors give or take 1 -
    // probably worth the speed up of no square root, which also won't be exact.
    FloatSize disp = p2 - p1;
    int length = SkScalarRoundToInt(disp.width() + disp.height());
    SkPaint paint(immutableState()->strokePaint(length));

    if (getStrokeStyle() == DottedStroke || getStrokeStyle() == DashedStroke) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        SkRect r1, r2;
        r1.set(p1.x(), p1.y(), p1.x() + width, p1.y() + width);
        r2.set(p2.x(), p2.y(), p2.x() + width, p2.y() + width);

        if (isVerticalLine) {
            r1.offset(-width / 2, 0);
            r2.offset(-width / 2, -width);
        } else {
            r1.offset(0, -width / 2);
            r2.offset(-width, -width / 2);
        }
        SkPaint fillPaint;
        fillPaint.setColor(paint.getColor());
        drawRect(r1, fillPaint);
        drawRect(r2, fillPaint);
    }

    adjustLineToPixelBoundaries(p1, p2, width, penStyle);
    m_canvas->drawLine(p1.x(), p1.y(), p2.x(), p2.y(), paint);
}

void GraphicsContext::drawLineForDocumentMarker(const FloatPoint& pt, float width, DocumentMarkerLineStyle style)
{
    if (contextDisabled())
        return;

    // Use 2x resources for a device scale factor of 1.5 or above.
    int deviceScaleFactor = m_deviceScaleFactor > 1.5f ? 2 : 1;

    // Create the pattern we'll use to draw the underline.
    int index = style == DocumentMarkerGrammarLineStyle ? 1 : 0;
    static SkBitmap* misspellBitmap1x[2] = { 0, 0 };
    static SkBitmap* misspellBitmap2x[2] = { 0, 0 };
    SkBitmap** misspellBitmap = deviceScaleFactor == 2 ? misspellBitmap2x : misspellBitmap1x;
    if (!misspellBitmap[index]) {
#if OS(MACOSX)
        // Match the artwork used by the Mac.
        const int rowPixels = 4 * deviceScaleFactor;
        const int colPixels = 3 * deviceScaleFactor;
        SkBitmap bitmap;
        if (!bitmap.tryAllocN32Pixels(rowPixels, colPixels))
            return;

        bitmap.eraseARGB(0, 0, 0, 0);
        const uint32_t transparentColor = 0x00000000;

        if (deviceScaleFactor == 1) {
            const uint32_t colors[2][6] = {
                { 0x2a2a0600, 0x57571000,  0xa8a81b00, 0xbfbf1f00,  0x70701200, 0xe0e02400 },
                { 0x2a0f0f0f, 0x571e1e1e,  0xa83d3d3d, 0xbf454545,  0x70282828, 0xe0515151 }
            };

            // Pattern: a b a   a b a
            //          c d c   c d c
            //          e f e   e f e
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = bitmap.getAddr32(0, x);
                row[0] = colors[index][x * 2];
                row[1] = colors[index][x * 2 + 1];
                row[2] = colors[index][x * 2];
                row[3] = transparentColor;
            }
        } else if (deviceScaleFactor == 2) {
            const uint32_t colors[2][18] = {
                { 0x0a090101, 0x33320806, 0x55540f0a,  0x37360906, 0x6e6c120c, 0x6e6c120c,  0x7674140d, 0x8d8b1810, 0x8d8b1810,
                  0x96941a11, 0xb3b01f15, 0xb3b01f15,  0x6d6b130c, 0xd9d62619, 0xd9d62619,  0x19180402, 0x7c7a150e, 0xcecb2418 },
                { 0x0a020202, 0x33141414, 0x55232323,  0x37161616, 0x6e2e2e2e, 0x6e2e2e2e,  0x76313131, 0x8d3a3a3a, 0x8d3a3a3a,
                  0x963e3e3e, 0xb34b4b4b, 0xb34b4b4b,  0x6d2d2d2d, 0xd95b5b5b, 0xd95b5b5b,  0x19090909, 0x7c343434, 0xce575757 }
            };

            // Pattern: a b c c b a
            //          d e f f e d
            //          g h j j h g
            //          k l m m l k
            //          n o p p o n
            //          q r s s r q
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = bitmap.getAddr32(0, x);
                row[0] = colors[index][x * 3];
                row[1] = colors[index][x * 3 + 1];
                row[2] = colors[index][x * 3 + 2];
                row[3] = colors[index][x * 3 + 2];
                row[4] = colors[index][x * 3 + 1];
                row[5] = colors[index][x * 3];
                row[6] = transparentColor;
                row[7] = transparentColor;
            }
        } else
            ASSERT_NOT_REACHED();

        misspellBitmap[index] = new SkBitmap(bitmap);
#else
        // We use a 2-pixel-high misspelling indicator because that seems to be
        // what WebKit is designed for, and how much room there is in a typical
        // page for it.
        const int rowPixels = 32 * deviceScaleFactor; // Must be multiple of 4 for pattern below.
        const int colPixels = 2 * deviceScaleFactor;
        SkBitmap bitmap;
        if (!bitmap.tryAllocN32Pixels(rowPixels, colPixels))
            return;

        bitmap.eraseARGB(0, 0, 0, 0);
        if (deviceScaleFactor == 1)
            draw1xMarker(&bitmap, index);
        else if (deviceScaleFactor == 2)
            draw2xMarker(&bitmap, index);
        else
            ASSERT_NOT_REACHED();

        misspellBitmap[index] = new SkBitmap(bitmap);
#endif
    }

#if OS(MACOSX)
    SkScalar originX = WebCoreFloatToSkScalar(pt.x()) * deviceScaleFactor;
    SkScalar originY = WebCoreFloatToSkScalar(pt.y()) * deviceScaleFactor;

    // Make sure to draw only complete dots.
    int rowPixels = misspellBitmap[index]->width();
    float widthMod = fmodf(width * deviceScaleFactor, rowPixels);
    if (rowPixels - widthMod > deviceScaleFactor)
        width -= widthMod / deviceScaleFactor;
#else
    SkScalar originX = WebCoreFloatToSkScalar(pt.x());

    // Offset it vertically by 1 so that there's some space under the text.
    SkScalar originY = WebCoreFloatToSkScalar(pt.y()) + 1;
    originX *= deviceScaleFactor;
    originY *= deviceScaleFactor;
#endif

    SkMatrix localMatrix;
    localMatrix.setTranslate(originX, originY);

    SkPaint paint;
    paint.setShader(SkShader::MakeBitmapShader(
        *misspellBitmap[index], SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix));

    SkRect rect;
    rect.set(originX, originY, originX + WebCoreFloatToSkScalar(width) * deviceScaleFactor, originY + SkIntToScalar(misspellBitmap[index]->height()));

    if (deviceScaleFactor == 2) {
        save();
        scale(0.5, 0.5);
    }
    drawRect(rect, paint);
    if (deviceScaleFactor == 2)
        restore();
}

void GraphicsContext::drawLineForText(const FloatPoint& pt, float width, bool printing)
{
    if (contextDisabled())
        return;

    if (width <= 0)
        return;

    SkPaint paint;
    switch (getStrokeStyle()) {
    case NoStroke:
    case SolidStroke:
    case DoubleStroke:
    case WavyStroke: {
        int thickness = SkMax32(static_cast<int>(strokeThickness()), 1);
        SkRect r;
        r.fLeft = WebCoreFloatToSkScalar(pt.x());
        // Avoid anti-aliasing lines. Currently, these are always horizontal.
        // Round to nearest pixel to match text and other content.
        r.fTop = WebCoreFloatToSkScalar(floorf(pt.y() + 0.5f));
        r.fRight = r.fLeft + WebCoreFloatToSkScalar(width);
        r.fBottom = r.fTop + SkIntToScalar(thickness);
        paint = immutableState()->fillPaint();
        // Text lines are drawn using the stroke color.
        paint.setColor(strokeColor().rgb());
        drawRect(r, paint);
        return;
    }
    case DottedStroke:
    case DashedStroke: {
        int y = floorf(pt.y() + std::max<float>(strokeThickness() / 2.0f, 0.5f));
        drawLine(IntPoint(pt.x(), y), IntPoint(pt.x() + width, y));
        return;
    }
    }

    ASSERT_NOT_REACHED();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (contextDisabled())
        return;

    ASSERT(!rect.isEmpty());
    if (rect.isEmpty())
        return;

    SkRect skRect = rect;
    if (immutableState()->fillColor().alpha())
        drawRect(skRect, immutableState()->fillPaint());

    if (immutableState()->getStrokeData().style() != NoStroke
        && immutableState()->strokeColor().alpha()) {
        // Stroke a width: 1 inset border
        SkPaint paint(immutableState()->fillPaint());
        paint.setColor(strokeColor().rgb());
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(1);

        skRect.inset(0.5f, 0.5f);
        drawRect(skRect, paint);
    }
}

void GraphicsContext::drawText(const Font& font, const TextRunPaintInfo& runInfo, const FloatPoint& point, const SkPaint& paint)
{
    if (contextDisabled())
        return;

    if (font.drawText(m_canvas, runInfo, point, m_deviceScaleFactor, paint))
        m_paintController.setTextPainted();
}

template<typename DrawTextFunc>
void GraphicsContext::drawTextPasses(const DrawTextFunc& drawText)
{
    TextDrawingModeFlags modeFlags = textDrawingMode();

    if (modeFlags & TextModeFill) {
        drawText(immutableState()->fillPaint());
    }

    if ((modeFlags & TextModeStroke) && getStrokeStyle() != NoStroke && strokeThickness() > 0) {
        SkPaint paintForStroking(immutableState()->strokePaint());
        if (modeFlags & TextModeFill) {
            paintForStroking.setLooper(0); // shadow was already applied during fill pass
        }
        drawText(paintForStroking);
    }
}

void GraphicsContext::drawText(const Font& font, const TextRunPaintInfo& runInfo, const FloatPoint& point)
{
    if (contextDisabled())
        return;

    drawTextPasses([&font, &runInfo, &point, this](const SkPaint& paint) {
        if (font.drawText(m_canvas, runInfo, point, m_deviceScaleFactor, paint))
            m_paintController.setTextPainted();
    });
}

void GraphicsContext::drawEmphasisMarks(const Font& font, const TextRunPaintInfo& runInfo, const AtomicString& mark, const FloatPoint& point)
{
    if (contextDisabled())
        return;

    drawTextPasses([&font, &runInfo, &mark, &point, this](const SkPaint& paint) {
        font.drawEmphasisMarks(m_canvas, runInfo, mark, point, m_deviceScaleFactor, paint);
    });
}

void GraphicsContext::drawBidiText(const Font& font, const TextRunPaintInfo& runInfo, const FloatPoint& point, Font::CustomFontNotReadyAction customFontNotReadyAction)
{
    if (contextDisabled())
        return;

    drawTextPasses([&font, &runInfo, &point, customFontNotReadyAction, this](const SkPaint& paint) {
        if (font.drawBidiText(m_canvas, runInfo, point, customFontNotReadyAction, m_deviceScaleFactor, paint))
            m_paintController.setTextPainted();
    });
}

void GraphicsContext::drawHighlightForText(const Font& font, const TextRun& run, const FloatPoint& point, int h, const Color& backgroundColor, int from, int to)
{
    if (contextDisabled())
        return;

    fillRect(font.selectionRectForText(run, point, h, from, to), backgroundColor);
}

void GraphicsContext::drawImage(Image* image, const FloatRect& dest, const FloatRect* srcPtr,
    SkXfermode::Mode op, RespectImageOrientationEnum shouldRespectImageOrientation)
{
    if (contextDisabled() || !image)
        return;

    const FloatRect src = srcPtr ? *srcPtr : image->rect();

    SkPaint imagePaint = immutableState()->fillPaint();
    imagePaint.setXfermodeMode(op);
    imagePaint.setColor(SK_ColorBLACK);
    imagePaint.setFilterQuality(computeFilterQuality(image, dest, src));
    imagePaint.setAntiAlias(shouldAntialias());
    image->draw(m_canvas, imagePaint, dest, src, shouldRespectImageOrientation, Image::ClampImageToSourceRect);
    m_paintController.setImagePainted();
}

void GraphicsContext::drawImageRRect(Image* image, const FloatRoundedRect& dest,
    const FloatRect& srcRect, SkXfermode::Mode op, RespectImageOrientationEnum respectOrientation)
{
    if (contextDisabled() || !image)
        return;

    if (!dest.isRounded()) {
        drawImage(image, dest.rect(), &srcRect, op, respectOrientation);
        return;
    }

    DCHECK(dest.isRenderable());

    const FloatRect visibleSrc = intersection(srcRect, image->rect());
    if (dest.isEmpty() || visibleSrc.isEmpty())
        return;

    SkPaint imagePaint = immutableState()->fillPaint();
    imagePaint.setXfermodeMode(op);
    imagePaint.setColor(SK_ColorBLACK);
    imagePaint.setFilterQuality(computeFilterQuality(image, dest.rect(), srcRect));
    imagePaint.setAntiAlias(shouldAntialias());

    bool useShader = (visibleSrc == srcRect) && (respectOrientation == DoNotRespectImageOrientation);
    if (useShader) {
        const SkMatrix localMatrix =
            SkMatrix::MakeRectToRect(visibleSrc, dest.rect(), SkMatrix::kFill_ScaleToFit);
        useShader = image->applyShader(imagePaint, localMatrix);
    }

    if (useShader) {
        // Shader-based fast path.
        m_canvas->drawRRect(dest, imagePaint);
    } else {
        // Clip-based fallback.
        SkAutoCanvasRestore autoRestore(m_canvas, true);
        m_canvas->clipRRect(dest, SkRegion::kIntersect_Op, imagePaint.isAntiAlias());
        image->draw(m_canvas, imagePaint, dest.rect(), srcRect, respectOrientation, Image::ClampImageToSourceRect);
    }

    m_paintController.setImagePainted();
}

SkFilterQuality GraphicsContext::computeFilterQuality(Image* image, const FloatRect& dest, const FloatRect& src) const
{
    InterpolationQuality resampling;
    if (printing()) {
        resampling = InterpolationNone;
    } else if (image->currentFrameIsLazyDecoded()) {
        resampling = InterpolationHigh;
    } else {
        resampling = computeInterpolationQuality(
            SkScalarToFloat(src.width()), SkScalarToFloat(src.height()),
            SkScalarToFloat(dest.width()), SkScalarToFloat(dest.height()),
            image->currentFrameIsComplete());

        if (resampling == InterpolationNone) {
            // FIXME: This is to not break tests (it results in the filter bitmap flag
            // being set to true). We need to decide if we respect InterpolationNone
            // being returned from computeInterpolationQuality.
            resampling = InterpolationLow;
        }
    }
    return static_cast<SkFilterQuality>(limitInterpolationQuality(*this, resampling));
}

void GraphicsContext::drawTiledImage(Image* image, const FloatRect& destRect, const FloatPoint& srcPoint, const FloatSize& tileSize, SkXfermode::Mode op, const FloatSize& repeatSpacing)
{
    if (contextDisabled() || !image)
        return;
    image->drawTiled(*this, destRect, srcPoint, tileSize, op, repeatSpacing);
}

void GraphicsContext::drawTiledImage(Image* image, const FloatRect& dest, const FloatRect& srcRect,
    const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, SkXfermode::Mode op)
{
    if (contextDisabled() || !image)
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile) {
        // Just do a scale.
        drawImage(image, dest, &srcRect, op);
        return;
    }

    image->drawTiled(*this, dest, srcRect, tileScaleFactor, hRule, vRule, op);
}

void GraphicsContext::drawOval(const SkRect& oval, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawOval(oval, paint);
}

void GraphicsContext::drawPath(const SkPath& path, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawPath(path, paint);
}

void GraphicsContext::drawRect(const SkRect& rect, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawRect(rect, paint);
}

void GraphicsContext::drawRRect(const SkRRect& rrect, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawRRect(rrect, paint);
}

void GraphicsContext::fillPath(const Path& pathToFill)
{
    if (contextDisabled() || pathToFill.isEmpty())
        return;

    drawPath(pathToFill.getSkPath(), immutableState()->fillPaint());
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (contextDisabled())
        return;

    drawRect(rect, immutableState()->fillPaint());
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, SkXfermode::Mode xferMode)
{
    if (contextDisabled())
        return;

    SkPaint paint = immutableState()->fillPaint();
    paint.setColor(color.rgb());
    paint.setXfermodeMode(xferMode);

    drawRect(rect, paint);
}

void GraphicsContext::fillRoundedRect(const FloatRoundedRect& rrect, const Color& color)
{
    if (contextDisabled())
        return;

    if (!rrect.isRounded() || !rrect.isRenderable()) {
        fillRect(rrect.rect(), color);
        return;
    }

    if (color == fillColor()) {
        drawRRect(rrect, immutableState()->fillPaint());
        return;
    }

    SkPaint paint = immutableState()->fillPaint();
    paint.setColor(color.rgb());

    drawRRect(rrect, paint);
}

namespace {

bool isSimpleDRRect(const FloatRoundedRect& outer, const FloatRoundedRect& inner)
{
    // A DRRect is "simple" (i.e. can be drawn as a rrect stroke) if
    //   1) all sides have the same width
    const FloatSize strokeSize = inner.rect().minXMinYCorner() - outer.rect().minXMinYCorner();
    if (!WebCoreFloatNearlyEqual(strokeSize.aspectRatio(), 1)
        || !WebCoreFloatNearlyEqual(strokeSize.width(), outer.rect().maxX() - inner.rect().maxX())
        || !WebCoreFloatNearlyEqual(strokeSize.height(), outer.rect().maxY() - inner.rect().maxY()))
        return false;

    // and
    //   2) the inner radii are not constrained
    const FloatRoundedRect::Radii& oRadii = outer.getRadii();
    const FloatRoundedRect::Radii& iRadii = inner.getRadii();
    if (!WebCoreFloatNearlyEqual(oRadii.topLeft().width() - strokeSize.width(), iRadii.topLeft().width())
        || !WebCoreFloatNearlyEqual(oRadii.topLeft().height() - strokeSize.height(), iRadii.topLeft().height())
        || !WebCoreFloatNearlyEqual(oRadii.topRight().width() - strokeSize.width(), iRadii.topRight().width())
        || !WebCoreFloatNearlyEqual(oRadii.topRight().height() - strokeSize.height(), iRadii.topRight().height())
        || !WebCoreFloatNearlyEqual(oRadii.bottomRight().width() - strokeSize.width(), iRadii.bottomRight().width())
        || !WebCoreFloatNearlyEqual(oRadii.bottomRight().height() - strokeSize.height(), iRadii.bottomRight().height())
        || !WebCoreFloatNearlyEqual(oRadii.bottomLeft().width() - strokeSize.width(), iRadii.bottomLeft().width())
        || !WebCoreFloatNearlyEqual(oRadii.bottomLeft().height() - strokeSize.height(), iRadii.bottomLeft().height()))
        return false;

    // We also ignore DRRects with a very thick relative stroke (shapes which are mostly filled by
    // the stroke): Skia's stroke outline can diverge significantly from the outer/inner contours
    // in some edge cases, so we fall back to drawDRRect instead.
    const float kMaxStrokeToSizeRatio = 0.75f;
    if (2 * strokeSize.width() / outer.rect().width() > kMaxStrokeToSizeRatio
        || 2 * strokeSize.height() / outer.rect().height() > kMaxStrokeToSizeRatio)
        return false;

    return true;
}

} // anonymous namespace

void GraphicsContext::fillDRRect(const FloatRoundedRect& outer,
    const FloatRoundedRect& inner, const Color& color)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    if (!isSimpleDRRect(outer, inner)) {
        if (color == fillColor()) {
            m_canvas->drawDRRect(outer, inner, immutableState()->fillPaint());
        } else {
            SkPaint paint(immutableState()->fillPaint());
            paint.setColor(color.rgb());
            m_canvas->drawDRRect(outer, inner, paint);
        }

        return;
    }

    // We can draw this as a stroked rrect.
    float strokeWidth = inner.rect().x() - outer.rect().x();
    SkRRect strokeRRect = outer;
    strokeRRect.inset(strokeWidth / 2, strokeWidth / 2);

    SkPaint strokePaint(immutableState()->fillPaint());
    strokePaint.setColor(color.rgb());
    strokePaint.setStyle(SkPaint::kStroke_Style);
    strokePaint.setStrokeWidth(strokeWidth);

    m_canvas->drawRRect(strokeRRect, strokePaint);
}

void GraphicsContext::fillEllipse(const FloatRect& ellipse)
{
    if (contextDisabled())
        return;

    drawOval(ellipse, immutableState()->fillPaint());
}

void GraphicsContext::strokePath(const Path& pathToStroke)
{
    if (contextDisabled() || pathToStroke.isEmpty())
        return;

    drawPath(pathToStroke.getSkPath(), immutableState()->strokePaint());
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (contextDisabled())
        return;

    SkPaint paint(immutableState()->strokePaint());
    paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));
    // Reset the dash effect to account for the width
    immutableState()->getStrokeData().setupPaintDashPathEffect(&paint, 0);
    // strokerect has special rules for CSS when the rect is degenerate:
    // if width==0 && height==0, do nothing
    // if width==0 || height==0, then just draw line for the other dimension
    SkRect r(rect);
    bool validW = r.width() > 0;
    bool validH = r.height() > 0;
    if (validW && validH) {
        drawRect(r, paint);
    } else if (validW || validH) {
        // we are expected to respect the lineJoin, so we can't just call
        // drawLine -- we have to create a path that doubles back on itself.
        SkPath path;
        path.moveTo(r.fLeft, r.fTop);
        path.lineTo(r.fRight, r.fBottom);
        path.close();
        drawPath(path, paint);
    }
}

void GraphicsContext::strokeEllipse(const FloatRect& ellipse)
{
    if (contextDisabled())
        return;

    drawOval(ellipse, immutableState()->strokePaint());
}

void GraphicsContext::clipRoundedRect(const FloatRoundedRect& rrect, SkRegion::Op regionOp, AntiAliasingMode shouldAntialias)
{
    if (contextDisabled())
        return;

    if (!rrect.isRounded()) {
        clipRect(rrect.rect(), shouldAntialias, regionOp);
        return;
    }

    clipRRect(rrect, shouldAntialias, regionOp);
}

void GraphicsContext::clipOut(const Path& pathToClip)
{
    if (contextDisabled())
        return;

    // Use const_cast and temporarily toggle the inverse fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToClip.getSkPath());
    path.toggleInverseFillType();
    clipPath(path, AntiAliased);
    path.toggleInverseFillType();
}

void GraphicsContext::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    if (contextDisabled())
        return;

    clipRoundedRect(rect, SkRegion::kDifference_Op);
}

void GraphicsContext::clipRect(const SkRect& rect, AntiAliasingMode aa, SkRegion::Op op)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->clipRect(rect, op, aa == AntiAliased);
}

void GraphicsContext::clipPath(const SkPath& path, AntiAliasingMode aa, SkRegion::Op op)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->clipPath(path, op, aa == AntiAliased);
}

void GraphicsContext::clipRRect(const SkRRect& rect, AntiAliasingMode aa, SkRegion::Op op)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->clipRRect(rect, op, aa == AntiAliased);
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->rotate(WebCoreFloatToSkScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float x, float y)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    if (!x && !y)
        return;

    m_canvas->translate(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
}

void GraphicsContext::scale(float x, float y)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->scale(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    SkAutoDataUnref url(SkData::NewWithCString(link.getString().utf8().data()));
    SkAnnotateRectWithURL(m_canvas, destRect, url.get());
}

void GraphicsContext::setURLFragmentForRect(const String& destName, const IntRect& rect)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    SkAutoDataUnref skDestName(SkData::NewWithCString(destName.utf8().data()));
    SkAnnotateLinkToDestination(m_canvas, rect, skDestName.get());
}

void GraphicsContext::setURLDestinationLocation(const String& name, const IntPoint& location)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    SkAutoDataUnref skName(SkData::NewWithCString(name.utf8().data()));
    SkAnnotateNamedDestination(m_canvas, SkPoint::Make(location.x(), location.y()), skName);
}

void GraphicsContext::concatCTM(const AffineTransform& affine)
{
    concat(affineTransformToSkMatrix(affine));
}

void GraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    if (contextDisabled())
        return;

    SkPaint paint(immutableState()->fillPaint());
    paint.setColor(color.rgb());
    m_canvas->drawDRRect(SkRRect::MakeRect(rect), roundedHoleRect, paint);
}

void GraphicsContext::adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle penStyle)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, WebKit will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == DottedStroke || penStyle == DashedStroke) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        } else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }

    if (static_cast<int>(strokeWidth) % 2) { //odd
        if (p1.x() == p2.x()) {
            // We're a vertical line.  Adjust our x.
            p1.setX(p1.x() + 0.5f);
            p2.setX(p2.x() + 0.5f);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.setY(p1.y() + 0.5f);
            p2.setY(p2.y() + 0.5f);
        }
    }
}

PassRefPtr<SkColorFilter> GraphicsContext::WebCoreColorFilterToSkiaColorFilter(ColorFilter colorFilter)
{
    switch (colorFilter) {
    case ColorFilterLuminanceToAlpha:
        return fromSkSp(SkLumaColorFilter::Make());
    case ColorFilterLinearRGBToSRGB:
        return ColorSpaceUtilities::createColorSpaceFilter(ColorSpaceLinearRGB, ColorSpaceDeviceRGB);
    case ColorFilterSRGBToLinearRGB:
        return ColorSpaceUtilities::createColorSpaceFilter(ColorSpaceDeviceRGB, ColorSpaceLinearRGB);
    case ColorFilterNone:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return nullptr;
}

#if !OS(MACOSX)
void GraphicsContext::draw2xMarker(SkBitmap* bitmap, int index)
{
    const SkPMColor lineColor = lineColors(index);
    const SkPMColor antiColor1 = antiColors1(index);
    const SkPMColor antiColor2 = antiColors2(index);

    uint32_t* row1 = bitmap->getAddr32(0, 0);
    uint32_t* row2 = bitmap->getAddr32(0, 1);
    uint32_t* row3 = bitmap->getAddr32(0, 2);
    uint32_t* row4 = bitmap->getAddr32(0, 3);

    // Pattern: X0o   o0X0o   o0
    //          XX0o o0XXX0o o0X
    //           o0XXX0o o0XXX0o
    //            o0X0o   o0X0o
    const SkPMColor row1Color[] = { lineColor, antiColor1, antiColor2, 0,          0,         0,          antiColor2, antiColor1 };
    const SkPMColor row2Color[] = { lineColor, lineColor,  antiColor1, antiColor2, 0,         antiColor2, antiColor1, lineColor };
    const SkPMColor row3Color[] = { 0,         antiColor2, antiColor1, lineColor,  lineColor, lineColor,  antiColor1, antiColor2 };
    const SkPMColor row4Color[] = { 0,         0,          antiColor2, antiColor1, lineColor, antiColor1, antiColor2, 0 };

    for (int x = 0; x < bitmap->width() + 8; x += 8) {
        int count = std::min(bitmap->width() - x, 8);
        if (count > 0) {
            memcpy(row1 + x, row1Color, count * sizeof(SkPMColor));
            memcpy(row2 + x, row2Color, count * sizeof(SkPMColor));
            memcpy(row3 + x, row3Color, count * sizeof(SkPMColor));
            memcpy(row4 + x, row4Color, count * sizeof(SkPMColor));
        }
    }
}

void GraphicsContext::draw1xMarker(SkBitmap* bitmap, int index)
{
    const uint32_t lineColor = lineColors(index);
    const uint32_t antiColor = antiColors2(index);

    // Pattern: X o   o X o   o X
    //            o X o   o X o
    uint32_t* row1 = bitmap->getAddr32(0, 0);
    uint32_t* row2 = bitmap->getAddr32(0, 1);
    for (int x = 0; x < bitmap->width(); x++) {
        switch (x % 4) {
        case 0:
            row1[x] = lineColor;
            break;
        case 1:
            row1[x] = antiColor;
            row2[x] = antiColor;
            break;
        case 2:
            row2[x] = lineColor;
            break;
        case 3:
            row1[x] = antiColor;
            row2[x] = antiColor;
            break;
        }
    }
}

SkPMColor GraphicsContext::lineColors(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0xFF, 0xFF, 0x00, 0x00), // Opaque red.
        SkPreMultiplyARGB(0xFF, 0xC0, 0xC0, 0xC0) // Opaque gray.
    };

    return colors[index];
}

SkPMColor GraphicsContext::antiColors1(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0xB0, 0xFF, 0x00, 0x00), // Semitransparent red.
        SkPreMultiplyARGB(0xB0, 0xC0, 0xC0, 0xC0)  // Semitransparent gray.
    };

    return colors[index];
}

SkPMColor GraphicsContext::antiColors2(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0x60, 0xFF, 0x00, 0x00), // More transparent red
        SkPreMultiplyARGB(0x60, 0xC0, 0xC0, 0xC0)  // More transparent gray
    };

    return colors[index];
}
#endif

} // namespace blink
