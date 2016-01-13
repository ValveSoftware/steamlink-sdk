/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/fonts/Font.h"

#include "platform/LayoutTestSupport.h"
#include "platform/fonts/FontPlatformFeatures.h"
#include "platform/fonts/FontSmoothingMode.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/graphics/GraphicsContext.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace WebCore {

bool FontPlatformFeatures::canReturnFallbackFontsForComplexText()
{
    return true;
}

bool FontPlatformFeatures::canExpandAroundIdeographsInComplexText()
{
    return true;
}

static void setupPaint(SkPaint* paint, const SimpleFontData* fontData, const Font* font, bool shouldAntialias, bool shouldSmoothFonts)
{
    const FontPlatformData& platformData = fontData->platformData();
    const float textSize = platformData.m_size >= 0 ? platformData.m_size : 12;

    paint->setAntiAlias(shouldAntialias);
    paint->setEmbeddedBitmapText(false);
    paint->setTextSize(SkFloatToScalar(textSize));
    paint->setVerticalText(platformData.orientation() == Vertical);
    paint->setTypeface(platformData.typeface());
    paint->setFakeBoldText(platformData.m_syntheticBold);
    paint->setTextSkewX(platformData.m_syntheticOblique ? -SK_Scalar1 / 4 : 0);
    paint->setAutohinted(false); // freetype specific
    paint->setLCDRenderText(shouldSmoothFonts);
    paint->setSubpixelText(true);

    // When using CoreGraphics, disable hinting when webkit-font-smoothing:antialiased is used.
    // See crbug.com/152304
    if (font->fontDescription().fontSmoothing() == Antialiased || font->fontDescription().textRendering() == GeometricPrecision)
        paint->setHinting(SkPaint::kNo_Hinting);
}

// TODO: This needs to be split into helper functions to better scope the
// inputs/outputs, and reduce duplicate code.
// This issue is tracked in https://bugs.webkit.org/show_bug.cgi?id=62989
void Font::drawGlyphs(GraphicsContext* gc, const SimpleFontData* font,
    const GlyphBuffer& glyphBuffer, unsigned from, unsigned numGlyphs,
    const FloatPoint& point, const FloatRect& textRect) const
{
    bool shouldSmoothFonts = true;
    bool shouldAntialias = true;

    switch (fontDescription().fontSmoothing()) {
    case Antialiased:
        shouldSmoothFonts = false;
        break;
    case SubpixelAntialiased:
        break;
    case NoSmoothing:
        shouldAntialias = false;
        shouldSmoothFonts = false;
        break;
    case AutoSmoothing:
        // For the AutoSmooth case, don't do anything! Keep the default settings.
        break;
    }

    if (isRunningLayoutTest()) {
        shouldSmoothFonts = false;
        shouldAntialias = shouldAntialias && isFontAntialiasingEnabledForTest();
    }

    const Glyph* glyphs = glyphBuffer.glyphs(from);
    SkScalar x = SkFloatToScalar(point.x());
    SkScalar y = SkFloatToScalar(point.y());

    if (font->platformData().orientation() == Vertical)
        y += SkFloatToScalar(font->fontMetrics().floatAscent(IdeographicBaseline) - font->fontMetrics().floatAscent());
    // FIXME: text rendering speed:
    // Android has code in their WebCore fork to special case when the
    // GlyphBuffer has no advances other than the defaults. In that case the
    // text drawing can proceed faster. However, it's unclear when those
    // patches may be upstreamed to WebKit so we always use the slower path
    // here.
    const FloatSize* adv = glyphBuffer.advances(from);
    SkAutoSTMalloc<32, SkPoint> storage(numGlyphs);
    SkPoint* pos = storage.get();

    for (unsigned i = 0; i < numGlyphs; i++) {
        pos[i].set(x, y);
        x += SkFloatToScalar(adv[i].width());
        y += SkFloatToScalar(adv[i].height());
    }

    if (font->platformData().orientation() == Vertical) {
        gc->save();
        gc->rotate(-0.5 * SK_ScalarPI);
        SkMatrix rotator;
        rotator.reset();
        rotator.setRotate(90);
        rotator.mapPoints(pos, numGlyphs);
    }
    TextDrawingModeFlags textMode = gc->textDrawingMode();

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & TextModeFill) {
        SkPaint paint = gc->fillPaint();
        setupPaint(&paint, font, this, shouldAntialias, shouldSmoothFonts);
        gc->adjustTextRenderMode(&paint);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        gc->drawPosText(glyphs, numGlyphs * sizeof(Glyph), pos, textRect, paint);
    }

    if ((textMode & TextModeStroke)
        && gc->strokeStyle() != NoStroke
        && gc->strokeThickness() > 0) {

        SkPaint paint = gc->strokePaint();
        setupPaint(&paint, font, this, shouldAntialias, shouldSmoothFonts);
        gc->adjustTextRenderMode(&paint);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        if (textMode & TextModeFill) {
            // If we also filled, we don't want to draw shadows twice.
            // See comment in FontHarfBuzz.cpp::paintGlyphs() for more details.
            paint.setLooper(0);
        }

        gc->drawPosText(glyphs, numGlyphs * sizeof(Glyph), pos, textRect, paint);
    }
    if (font->platformData().orientation() == Vertical)
        gc->restore();
}

} // namespace WebCore
