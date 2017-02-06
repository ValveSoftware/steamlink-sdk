/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (c) 2007, 2008, 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "platform/fonts/Font.h"

#include "platform/LayoutTestSupport.h"
#include "platform/LayoutUnit.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontFallbackIterator.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/GlyphPageTreeNode.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/CachingWordShaper.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/SimpleShaper.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/Character.h"
#include "platform/text/TextRun.h"
#include "platform/text/TextRunIterator.h"
#include "platform/transforms/AffineTransform.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"

using namespace WTF;
using namespace Unicode;

namespace blink {

Font::Font()
    : m_canShapeWordByWord(0)
    , m_shapeWordByWordComputed(0)
{
}

Font::Font(const FontDescription& fd)
    : m_fontDescription(fd)
    , m_canShapeWordByWord(0)
    , m_shapeWordByWordComputed(0)
{
}

Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription)
    , m_fontFallbackList(other.m_fontFallbackList)
    // TODO(yosin): We should have a comment the reason why don't we copy
    // |m_canShapeWordByWord| and |m_shapeWordByWordComputed| from |other|,
    // since |operator=()| copies them from |other|.
    , m_canShapeWordByWord(0)
    , m_shapeWordByWordComputed(0)
{
}

Font& Font::operator=(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_fontFallbackList = other.m_fontFallbackList;
    m_canShapeWordByWord = other.m_canShapeWordByWord;
    m_shapeWordByWordComputed = other.m_shapeWordByWordComputed;
    return *this;
}

bool Font::operator==(const Font& other) const
{
    FontSelector* first = m_fontFallbackList ? m_fontFallbackList->getFontSelector() : 0;
    FontSelector* second = other.m_fontFallbackList ? other.m_fontFallbackList->getFontSelector() : 0;

    return first == second
        && m_fontDescription == other.m_fontDescription
        && (m_fontFallbackList ? m_fontFallbackList->fontSelectorVersion() : 0) == (other.m_fontFallbackList ? other.m_fontFallbackList->fontSelectorVersion() : 0)
        && (m_fontFallbackList ? m_fontFallbackList->generation() : 0) == (other.m_fontFallbackList ? other.m_fontFallbackList->generation() : 0);
}

void Font::update(FontSelector* fontSelector) const
{
    // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr, but it ends up
    // being reasonably safe (because inherited fonts in the render tree pick up the new
    // style anyway. Other copies are transient, e.g., the state in the GraphicsContext, and
    // won't stick around long enough to get you in trouble). Still, this is pretty disgusting,
    // and could eventually be rectified by using RefPtrs for Fonts themselves.
    if (!m_fontFallbackList)
        m_fontFallbackList = FontFallbackList::create();
    m_fontFallbackList->invalidate(fontSelector);
}

float Font::buildGlyphBuffer(const TextRunPaintInfo& runInfo, GlyphBuffer& glyphBuffer,
    const GlyphData* emphasisData) const
{
    if (codePath(runInfo) == ComplexPath) {
        float width;
        CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
        if (emphasisData) {
            width = shaper.fillGlyphBufferForTextEmphasis(this, runInfo.run,
                emphasisData, &glyphBuffer, runInfo.from, runInfo.to);
        } else {
            width = shaper.fillGlyphBuffer(this, runInfo.run, nullptr,
                &glyphBuffer, runInfo.from, runInfo.to);
        }

        return width;
    }

    SimpleShaper shaper(this, runInfo.run, emphasisData, nullptr /* fallbackFonts */, nullptr);
    shaper.advance(runInfo.from);
    shaper.advance(runInfo.to, &glyphBuffer);
    float width = shaper.runWidthSoFar();

    if (runInfo.run.rtl()) {
        // Glyphs are shaped & stored in RTL advance order - reverse them for LTR drawing.
        shaper.advance(runInfo.run.length());
        glyphBuffer.reverseForSimpleRTL(width, shaper.runWidthSoFar());
    }

    return width;
}

bool Font::drawText(SkCanvas* canvas, const TextRunPaintInfo& runInfo,
    const FloatPoint& point, float deviceScaleFactor, const SkPaint& paint) const
{
    // Don't draw anything while we are using custom fonts that are in the process of loading.
    if (shouldSkipDrawing())
        return false;

    if (runInfo.cachedTextBlob && runInfo.cachedTextBlob->get()) {
        // we have a pre-cached blob -- happy joy!
        canvas->drawTextBlob(runInfo.cachedTextBlob->get(), point.x(), point.y(), paint);
        return true;
    }

    GlyphBuffer glyphBuffer;
    buildGlyphBuffer(runInfo, glyphBuffer);

    drawGlyphBuffer(canvas, paint, runInfo, glyphBuffer, point, deviceScaleFactor);
    return true;
}

bool Font::drawBidiText(SkCanvas* canvas, const TextRunPaintInfo& runInfo, const FloatPoint& point, CustomFontNotReadyAction customFontNotReadyAction, float deviceScaleFactor, const SkPaint& paint) const
{
    // Don't draw anything while we are using custom fonts that are in the process of loading,
    // except if the 'force' argument is set to true (in which case it will use a fallback
    // font).
    if (shouldSkipDrawing() && customFontNotReadyAction == DoNotPaintIfFontNotReady)
        return false;

    // sub-run painting is not supported for Bidi text.
    const TextRun& run = runInfo.run;
    ASSERT((runInfo.from == 0) && (runInfo.to == run.length()));
    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    bidiResolver.setStatus(BidiStatus(run.direction(), run.directionalOverride()));
    bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));

    // FIXME: This ownership should be reversed. We should pass BidiRunList
    // to BidiResolver in createBidiRunsForLine.
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));
    if (!bidiRuns.runCount())
        return true;

    FloatPoint currPoint = point;
    BidiCharacterRun* bidiRun = bidiRuns.firstRun();
    while (bidiRun) {
        TextRun subrun = run.subRun(bidiRun->start(), bidiRun->stop() - bidiRun->start());
        bool isRTL = bidiRun->level() % 2;
        subrun.setDirection(isRTL ? RTL : LTR);
        subrun.setDirectionalOverride(bidiRun->dirOverride(false));

        TextRunPaintInfo subrunInfo(subrun);
        subrunInfo.bounds = runInfo.bounds;

        // TODO: investigate blob consolidation/caching (technically,
        //       all subruns could be part of the same blob).
        GlyphBuffer glyphBuffer;
        float runWidth = buildGlyphBuffer(subrunInfo, glyphBuffer);
        drawGlyphBuffer(canvas, paint, subrunInfo, glyphBuffer, currPoint, deviceScaleFactor);

        bidiRun = bidiRun->next();
        currPoint.move(runWidth, 0);
    }

    bidiRuns.deleteRuns();
    return true;
}

void Font::drawEmphasisMarks(SkCanvas* canvas, const TextRunPaintInfo& runInfo, const AtomicString& mark, const FloatPoint& point, float deviceScaleFactor, const SkPaint& paint) const
{
    if (shouldSkipDrawing())
        return;

    FontCachePurgePreventer purgePreventer;

    GlyphData emphasisGlyphData;
    if (!getEmphasisMarkGlyphData(mark, emphasisGlyphData))
        return;

    ASSERT(emphasisGlyphData.fontData);
    if (!emphasisGlyphData.fontData)
        return;

    GlyphBuffer glyphBuffer;
    buildGlyphBuffer(runInfo, glyphBuffer, &emphasisGlyphData);

    if (glyphBuffer.isEmpty())
        return;

    drawGlyphBuffer(canvas, paint, runInfo, glyphBuffer, point, deviceScaleFactor);
}

float Font::width(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, FloatRect* glyphBounds) const
{
    FontCachePurgePreventer purgePreventer;

    if (codePath(TextRunPaintInfo(run)) == ComplexPath)
        return floatWidthForComplexText(run, fallbackFonts, glyphBounds);
    return floatWidthForSimpleText(run, fallbackFonts, glyphBounds);
}

namespace {

enum BlobRotation {
    NoRotation,
    CCWRotation,
};

class GlyphBufferBloberizer {
    STACK_ALLOCATED()
public:
    GlyphBufferBloberizer(const GlyphBuffer& buffer, const Font* font, float deviceScaleFactor)
        : m_buffer(buffer)
        , m_font(font)
        , m_deviceScaleFactor(deviceScaleFactor)
        , m_hasVerticalOffsets(buffer.hasVerticalOffsets())
        , m_index(0)
        , m_blobCount(0)
        , m_rotation(buffer.isEmpty() ? NoRotation : computeBlobRotation(buffer.fontDataAt(0)))
    { }

    bool done() const { return m_index >= m_buffer.size(); }
    unsigned blobCount() const { return m_blobCount; }

    std::pair<RefPtr<const SkTextBlob>, BlobRotation> next()
    {
        ASSERT(!done());
        const BlobRotation currentRotation = m_rotation;

        while (m_index < m_buffer.size()) {
            const SimpleFontData* fontData = m_buffer.fontDataAt(m_index);
            ASSERT(fontData);

            const BlobRotation newRotation = computeBlobRotation(fontData);
            if (newRotation != m_rotation) {
                // We're switching to an orientation which requires a different rotation
                //   => emit the pending blob (and start a new one with the new rotation).
                m_rotation = newRotation;
                break;
            }

            const unsigned start = m_index++;
            while (m_index < m_buffer.size() && m_buffer.fontDataAt(m_index) == fontData)
                m_index++;

            appendRun(start, m_index - start, fontData);
        }

        m_blobCount++;
        return std::make_pair(adoptRef(m_builder.build()), currentRotation);
    }

private:
    static BlobRotation computeBlobRotation(const SimpleFontData* font)
    {
        // For vertical upright text we need to compensate the inherited 90deg CW rotation
        // (using a 90deg CCW rotation).
        return (font->platformData().isVerticalAnyUpright() && font->verticalData()) ?
            CCWRotation : NoRotation;
    }

    void appendRun(unsigned start, unsigned count, const SimpleFontData* fontData)
    {
        SkPaint paint;
        fontData->platformData().setupPaint(&paint, m_deviceScaleFactor, m_font);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        const SkTextBlobBuilder::RunBuffer& buffer = m_hasVerticalOffsets
            ? m_builder.allocRunPos(paint, count)
            : m_builder.allocRunPosH(paint, count, 0);

        const uint16_t* glyphs = m_buffer.glyphs(start);
        const float* offsets = m_buffer.offsets(start);
        std::copy(glyphs, glyphs + count, buffer.glyphs);

        if (m_rotation == NoRotation) {
            std::copy(offsets, offsets + (m_hasVerticalOffsets ? 2 * count : count), buffer.pos);
        } else {
            ASSERT(m_hasVerticalOffsets);

            const float verticalBaselineXOffset = fontData->getFontMetrics().floatAscent()
                - fontData->getFontMetrics().floatAscent(IdeographicBaseline);

            // TODO(fmalita): why don't we apply this adjustment when building the glyph buffer?
            for (unsigned i = 0; i < 2 * count; i += 2) {
                buffer.pos[i] = SkFloatToScalar(offsets[i] + verticalBaselineXOffset);
                buffer.pos[i + 1] = SkFloatToScalar(offsets[i + 1]);
            }
        }
    }

    const GlyphBuffer& m_buffer;
    const Font* m_font;
    const float m_deviceScaleFactor;
    const bool m_hasVerticalOffsets;

    SkTextBlobBuilder m_builder;
    unsigned m_index;
    unsigned m_blobCount;
    BlobRotation m_rotation;
};

} // anonymous namespace

void Font::drawGlyphBuffer(SkCanvas* canvas, const SkPaint& paint, const TextRunPaintInfo& runInfo,
    const GlyphBuffer& glyphBuffer, const FloatPoint& point, float deviceScaleFactor) const
{
    GlyphBufferBloberizer bloberizer(glyphBuffer, this, deviceScaleFactor);
    std::pair<RefPtr<const SkTextBlob>, BlobRotation> blob;

    while (!bloberizer.done()) {
        blob = bloberizer.next();
        ASSERT(blob.first);

        SkAutoCanvasRestore autoRestore(canvas, false);
        if (blob.second == CCWRotation) {
            canvas->save();

            SkMatrix m;
            m.setSinCos(-1, 0, point.x(), point.y());
            canvas->concat(m);
        }

        canvas->drawTextBlob(blob.first.get(), point.x(), point.y(), paint);
    }

    // Cache results when
    //   1) requested by clients, and
    //   2) the glyph buffer is encoded as a single blob, and
    //   3) the blob is not upright/rotated
    if (runInfo.cachedTextBlob && bloberizer.blobCount() == 1 && blob.second == NoRotation) {
        ASSERT(!*runInfo.cachedTextBlob);
        *runInfo.cachedTextBlob = blob.first.release();
        ASSERT(*runInfo.cachedTextBlob);
    }
}

static inline FloatRect pixelSnappedSelectionRect(FloatRect rect)
{
    // Using roundf() rather than ceilf() for the right edge as a compromise to
    // ensure correct caret positioning.
    float roundedX = roundf(rect.x());
    return FloatRect(roundedX, rect.y(), roundf(rect.maxX() - roundedX), rect.height());
}

FloatRect Font::selectionRectForText(const TextRun& run, const FloatPoint& point, int h, int from, int to, bool accountForGlyphBounds) const
{
    to = (to == -1 ? run.length() : to);

    TextRunPaintInfo runInfo(run);
    runInfo.from = from;
    runInfo.to = to;

    FontCachePurgePreventer purgePreventer;

    if (codePath(runInfo) != ComplexPath)
        return pixelSnappedSelectionRect(selectionRectForSimpleText(run, point, h, from, to, accountForGlyphBounds));
    return pixelSnappedSelectionRect(selectionRectForComplexText(run, point, h, from, to));
}

int Font::offsetForPosition(const TextRun& run, float x, bool includePartialGlyphs) const
{
    FontCachePurgePreventer purgePreventer;

    if (codePath(TextRunPaintInfo(run)) != ComplexPath && !getFontDescription().getTypesettingFeatures())
        return offsetForPositionForSimpleText(run, x, includePartialGlyphs);

    return offsetForPositionForComplexText(run, x, includePartialGlyphs);
}

CodePath Font::codePath(const TextRunPaintInfo& runInfo) const
{
// TODO(eae): Disable the always use complex text feature on Android for now as
// it caused a memory regression for webview. crbug.com/577306
#if !OS(ANDROID)
    if (RuntimeEnabledFeatures::alwaysUseComplexTextEnabled()
        || LayoutTestSupport::alwaysUseComplexTextForTest()) {
        return ComplexPath;
    }
#endif

    const TextRun& run = runInfo.run;

    if (getFontDescription().getTypesettingFeatures())
        return ComplexPath;

    if (m_fontDescription.featureSettings() && m_fontDescription.featureSettings()->size() > 0)
        return ComplexPath;

    if (m_fontDescription.isVerticalBaseline())
        return ComplexPath;

    if (m_fontDescription.widthVariant() != RegularWidth)
        return ComplexPath;

    // FIXME: This really shouldn't be needed but for some reason the
    // TextRendering setting doesn't propagate to typesettingFeatures in time
    // for the prefs width calculation.
    if (getFontDescription().textRendering() == OptimizeLegibility || getFontDescription().textRendering() == GeometricPrecision)
        return ComplexPath;

    if (run.is8Bit())
        return SimplePath;

    // Start from 0 since drawing and highlighting also measure the characters before run->from.
    return Character::characterRangeCodePath(run.characters16(), run.length());
}

bool Font::canShapeWordByWord() const
{
    if (!m_shapeWordByWordComputed) {
        m_canShapeWordByWord = computeCanShapeWordByWord();
        m_shapeWordByWordComputed = true;
    }
    return m_canShapeWordByWord;
};

bool Font::computeCanShapeWordByWord() const
{
    if (!getFontDescription().getTypesettingFeatures())
        return true;

    if (!primaryFont())
        return false;

    const FontPlatformData& platformData = primaryFont()->platformData();
    TypesettingFeatures features = getFontDescription().getTypesettingFeatures();
    return !platformData.hasSpaceInLigaturesOrKerning(features);
};

void Font::willUseFontData(const String& text) const
{
    const FontFamily& family = getFontDescription().family();
    if (m_fontFallbackList && m_fontFallbackList->getFontSelector() && !family.familyIsEmpty())
        m_fontFallbackList->getFontSelector()->willUseFontData(getFontDescription(), family.family(), text);
}

static inline GlyphData glyphDataForNonCJKCharacterWithGlyphOrientation(UChar32 character, bool isUpright, GlyphData& data, unsigned pageNumber)
{
    if (isUpright) {
        RefPtr<SimpleFontData> uprightFontData = data.fontData->uprightOrientationFontData();
        GlyphPageTreeNode* uprightNode = GlyphPageTreeNode::getNormalRootChild(uprightFontData.get(), pageNumber);
        GlyphPage* uprightPage = uprightNode->page();
        if (uprightPage) {
            GlyphData uprightData = uprightPage->glyphDataForCharacter(character);
            // If the glyphs are the same, then we know we can just use the horizontal glyph rotated vertically to be upright.
            if (data.glyph == uprightData.glyph)
                return data;
            // The glyphs are distinct, meaning that the font has a vertical-right glyph baked into it. We can't use that
            // glyph, so we fall back to the upright data and use the horizontal glyph.
            if (uprightData.fontData)
                return uprightData;
        }
    } else {
        RefPtr<SimpleFontData> verticalRightFontData = data.fontData->verticalRightOrientationFontData();
        GlyphPageTreeNode* verticalRightNode = GlyphPageTreeNode::getNormalRootChild(verticalRightFontData.get(), pageNumber);
        GlyphPage* verticalRightPage = verticalRightNode->page();
        if (verticalRightPage) {
            GlyphData verticalRightData = verticalRightPage->glyphDataForCharacter(character);
            // If the glyphs are distinct, we will make the assumption that the font has a vertical-right glyph baked
            // into it.
            if (data.glyph != verticalRightData.glyph)
                return data;
            // The glyphs are identical, meaning that we should just use the horizontal glyph.
            if (verticalRightData.fontData)
                return verticalRightData;
        }
    }
    return data;
}

PassRefPtr<FontFallbackIterator> Font::createFontFallbackIterator(
    FontFallbackPriority fallbackPriority) const
{
    return FontFallbackIterator::create(m_fontDescription,
        m_fontFallbackList,
        fallbackPriority);
}

GlyphData Font::glyphDataForCharacter(UChar32& c, bool mirror, bool normalizeSpace, FontDataVariant variant) const
{
    ASSERT(isMainThread());

    if (variant == AutoVariant) {
        if (m_fontDescription.variantCaps() == FontDescription::SmallCaps) {
            bool includeDefault = false;
            UChar32 upperC = toUpper(c, m_fontDescription.locale(includeDefault));
            if (upperC != c) {
                c = upperC;
                variant = SmallCapsVariant;
            } else {
                variant = NormalVariant;
            }
        } else {
            variant = NormalVariant;
        }
    }

    if (normalizeSpace && Character::isNormalizedCanvasSpaceCharacter(c))
        c = spaceCharacter;

    if (mirror)
        c = mirroredChar(c);

    unsigned pageNumber = (c / GlyphPage::size);

    GlyphPageTreeNodeBase* node = m_fontFallbackList->getPageNode(pageNumber);
    if (!node) {
        node = GlyphPageTreeNode::getRootChild(fontDataAt(0), pageNumber);
        m_fontFallbackList->setPageNode(pageNumber, node);
    }
    RELEASE_ASSERT(node); // Diagnosing crbug.com/446566 crash bug.

    GlyphPage* page = 0;
    if (variant == NormalVariant) {
        // Fastest loop, for the common case (normal variant).
        while (true) {
            page = node->page(m_fontDescription.script());
            if (page) {
                GlyphData data = page->glyphDataForCharacter(c);
                if (data.fontData) {
                    if (!data.fontData->platformData().isVerticalAnyUpright() || data.fontData->isTextOrientationFallback())
                        return data;

                    bool isUpright = m_fontDescription.isVerticalUpright(c);
                    if (!isUpright || !Character::isCJKIdeographOrSymbol(c))
                        return glyphDataForNonCJKCharacterWithGlyphOrientation(c, isUpright, data, pageNumber);

                    return data;
                }

                if (node->isSystemFallback())
                    break;
            }

            // Proceed with the fallback list.
            node = toGlyphPageTreeNode(node)->getChild(fontDataAt(node->level()), pageNumber);
            m_fontFallbackList->setPageNode(pageNumber, node);
        }
    }
    if (variant != NormalVariant) {
        while (true) {
            page = node->page(m_fontDescription.script());
            if (page) {
                GlyphData data = page->glyphDataForCharacter(c);
                if (data.fontData) {
                    // The variantFontData function should not normally return 0.
                    // But if it does, we will just render the capital letter big.
                    RefPtr<SimpleFontData> variantFontData = data.fontData->variantFontData(m_fontDescription, variant);
                    if (!variantFontData)
                        return data;

                    GlyphPageTreeNode* variantNode = GlyphPageTreeNode::getNormalRootChild(variantFontData.get(), pageNumber);
                    GlyphPage* variantPage = variantNode->page();
                    if (variantPage) {
                        GlyphData data = variantPage->glyphDataForCharacter(c);
                        if (data.fontData)
                            return data;
                    }

                    // Do not attempt system fallback off the variantFontData. This is the very unlikely case that
                    // a font has the lowercase character but the small caps font does not have its uppercase version.
                    return variantFontData->missingGlyphData();
                }

                if (node->isSystemFallback())
                    break;
            }

            // Proceed with the fallback list.
            node = toGlyphPageTreeNode(node)->getChild(fontDataAt(node->level()), pageNumber);
            m_fontFallbackList->setPageNode(pageNumber, node);
        }
    }

    ASSERT(page);
    ASSERT(node->isSystemFallback());

    // System fallback is character-dependent. When we get here, we
    // know that the character in question isn't in the system fallback
    // font's glyph page. Try to lazily create it here.

    // FIXME: Unclear if this should normalizeSpaces above 0xFFFF.
    // Doing so changes fast/text/international/plane2-diffs.html
    UChar32 characterToRender = c;
    if (characterToRender <=  0xFFFF)
        characterToRender = Character::normalizeSpaces(characterToRender);

    const FontData* fontData = fontDataAt(0);
    if (fontData) {
        const SimpleFontData* fontDataToSubstitute = fontData->fontDataForCharacter(characterToRender);
        RefPtr<SimpleFontData> characterFontData = FontCache::fontCache()->fallbackFontForCharacter(m_fontDescription, characterToRender, fontDataToSubstitute);
        if (characterFontData && variant != NormalVariant) {
            characterFontData = characterFontData->variantFontData(m_fontDescription, variant);
        }
        if (characterFontData) {
            // Got the fallback glyph and font.
            unsigned pageNumberForRendering = characterToRender / GlyphPage::size;
            GlyphPage* fallbackPage = GlyphPageTreeNode::getRootChild(characterFontData.get(), pageNumberForRendering)->page();
            GlyphData data = fallbackPage && fallbackPage->glyphForCharacter(characterToRender) ? fallbackPage->glyphDataForCharacter(characterToRender) : characterFontData->missingGlyphData();
            // Cache it so we don't have to do system fallback again next time.
            if (variant == NormalVariant) {
                page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
                data.fontData->setMaxGlyphPageTreeLevel(std::max(data.fontData->maxGlyphPageTreeLevel(), node->level()));
                if (data.fontData->platformData().isVerticalAnyUpright() && !data.fontData->isTextOrientationFallback() && !Character::isCJKIdeographOrSymbol(characterToRender))
                    return glyphDataForNonCJKCharacterWithGlyphOrientation(characterToRender, m_fontDescription.isVerticalUpright(characterToRender), data, pageNumberForRendering);
            }
            return data;
        }
    }

    // Even system fallback can fail; use the missing glyph in that case.
    // FIXME: It would be nicer to use the missing glyph from the last resort font instead.
    ASSERT(primaryFont());
    GlyphData data = primaryFont()->missingGlyphData();
    if (variant == NormalVariant) {
        page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
        data.fontData->setMaxGlyphPageTreeLevel(std::max(data.fontData->maxGlyphPageTreeLevel(), node->level()));
    }
    return data;
}

bool Font::getEmphasisMarkGlyphData(const AtomicString& mark, GlyphData& glyphData) const
{
    if (mark.isEmpty())
        return false;

    TextRun emphasisMarkRun(mark, mark.length());
    TextRunPaintInfo emphasisPaintInfo(emphasisMarkRun);
    GlyphBuffer glyphBuffer;
    buildGlyphBuffer(emphasisPaintInfo, glyphBuffer);

    if (glyphBuffer.isEmpty())
        return false;

    ASSERT(glyphBuffer.fontDataAt(0));
    glyphData.fontData = glyphBuffer.fontDataAt(0)->emphasisMarkFontData(m_fontDescription).get();
    glyphData.glyph = glyphBuffer.glyphAt(0);

    return true;
}

int Font::emphasisMarkAscent(const AtomicString& mark) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return 0;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->getFontMetrics().ascent();
}

int Font::emphasisMarkDescent(const AtomicString& mark) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return 0;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->getFontMetrics().descent();
}

int Font::emphasisMarkHeight(const AtomicString& mark) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return 0;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->getFontMetrics().height();
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, FloatRect* glyphBounds) const
{
    CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
    float width = shaper.width(this, run, fallbackFonts, glyphBounds);
    return width;
}

// Return the code point index for the given |x| offset into the text run.
int Font::offsetForPositionForComplexText(const TextRun& run, float xFloat,
    bool includePartialGlyphs) const
{
    CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
    return shaper.offsetForPosition(this, run, xFloat, includePartialGlyphs);
}

// Return the rectangle for selecting the given range of code-points in the TextRun.
FloatRect Font::selectionRectForComplexText(const TextRun& run,
    const FloatPoint& point, int height, int from, int to) const
{
    CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
    CharacterRange range = shaper.getCharacterRange(this, run, from, to);
    return FloatRect(point.x() + range.start, point.y(), range.width(), height);
}

CharacterRange Font::getCharacterRange(const TextRun& run, unsigned from, unsigned to) const
{
    FontCachePurgePreventer purgePreventer;
    CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
    return shaper.getCharacterRange(this, run, from, to);
}

Vector<CharacterRange> Font::individualCharacterRanges(const TextRun& run) const
{
    // TODO(pdr): Android is temporarily (crbug.com/577306) using the old simple
    // shaper and using the complex shaper here can show differences between
    // the two shapers. This function is currently only called through SVG
    // which now exclusively uses the complex shaper, so the primary difference
    // will be improved shaping in SVG when compared to HTML.
    FontCachePurgePreventer purgePreventer;
    CachingWordShaper shaper(m_fontFallbackList->shapeCache(m_fontDescription));
    auto ranges = shaper.individualCharacterRanges(this, run);
    // The shaper should return ranges.size == run.length but on some platforms
    // (OSX10.9.5) we are seeing cases in the upper end of the unicode range
    // where this is not true (see: crbug.com/620952). To catch these cases on
    // more popular platforms, and to protect users, we are using a CHECK here.
    CHECK_EQ(ranges.size(), run.length());
    return ranges;
}

float Font::floatWidthForSimpleText(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, FloatRect* glyphBounds) const
{
    SimpleShaper shaper(this, run, nullptr, fallbackFonts, glyphBounds);
    shaper.advance(run.length());
    return shaper.runWidthSoFar();
}

FloatRect Font::selectionRectForSimpleText(const TextRun& run, const FloatPoint& point, int h, int from, int to, bool accountForGlyphBounds) const
{
    FloatRect bounds;
    SimpleShaper shaper(this, run, nullptr, nullptr, accountForGlyphBounds ? &bounds : nullptr);
    shaper.advance(from);
    float fromX = shaper.runWidthSoFar();
    shaper.advance(to);
    float toX = shaper.runWidthSoFar();

    if (run.rtl()) {
        shaper.advance(run.length());
        float totalWidth = shaper.runWidthSoFar();
        float beforeWidth = fromX;
        float afterWidth = toX;
        fromX = totalWidth - afterWidth;
        toX = totalWidth - beforeWidth;
    }

    return FloatRect(point.x() + fromX,
        accountForGlyphBounds ? bounds.y(): point.y(),
        toX - fromX,
        accountForGlyphBounds ? bounds.maxY()- bounds.y(): h);
}

int Font::offsetForPositionForSimpleText(const TextRun& run, float x, bool includePartialGlyphs) const
{
    float delta = x;

    SimpleShaper shaper(this, run);
    unsigned offset;
    if (run.rtl()) {
        delta -= floatWidthForSimpleText(run);
        while (1) {
            offset = shaper.currentOffset();
            float w;
            if (!shaper.advanceOneCharacter(w))
                break;
            delta += w;
            if (includePartialGlyphs) {
                if (delta - w / 2 >= 0)
                    break;
            } else {
                if (delta >= 0)
                    break;
            }
        }
    } else {
        while (1) {
            offset = shaper.currentOffset();
            float w;
            if (!shaper.advanceOneCharacter(w))
                break;
            delta -= w;
            if (includePartialGlyphs) {
                if (delta + w / 2 <= 0)
                    break;
            } else {
                if (delta <= 0)
                    break;
            }
        }
    }

    return offset;
}

bool Font::loadingCustomFonts() const
{
    return m_fontFallbackList && m_fontFallbackList->loadingCustomFonts();
}

bool Font::isFallbackValid() const
{
    return !m_fontFallbackList || m_fontFallbackList->isValid();
}

} // namespace blink
