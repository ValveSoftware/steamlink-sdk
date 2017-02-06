/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
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

#include "platform/fonts/shaping/HarfBuzzFace.h"

#include "platform/Histogram.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/UnicodeRangeSet.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "wtf/HashMap.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include <memory>

#include <hb-ot.h>
#include <hb.h>
#if OS(MACOSX)
#include <hb-coretext.h>
#endif

#include <SkPaint.h>
#include <SkPath.h>
#include <SkPoint.h>
#include <SkRect.h>
#include <SkStream.h>
#include <SkTypeface.h>


namespace blink {

struct HbFontDeleter {
    void operator()(hb_font_t* font)
    {
        if (font)
            hb_font_destroy(font);
    }
};

using HbFontUniquePtr = std::unique_ptr<hb_font_t, HbFontDeleter>;

struct HbFaceDeleter {
    void operator()(hb_face_t* face)
    {
        if (face)
            hb_face_destroy(face);
    }
};

using HbFaceUniquePtr = std::unique_ptr<hb_face_t, HbFaceDeleter>;

// struct to carry user-pointer data for hb_font_t callback functions.
struct HarfBuzzFontData {
    USING_FAST_MALLOC(HarfBuzzFontData);
    WTF_MAKE_NONCOPYABLE(HarfBuzzFontData);
public:

    HarfBuzzFontData()
        : m_paint()
        , m_simpleFontData(nullptr)
        , m_rangeSet(nullptr)
    {
    }

    SkPaint m_paint;
    SimpleFontData* m_simpleFontData;
    RefPtr<UnicodeRangeSet> m_rangeSet;
};

// Though we have FontCache class, which provides the cache mechanism for
// WebKit's font objects, we also need additional caching layer for HarfBuzz to
// reduce the number of hb_font_t objects created. Without it, we would create
// an hb_font_t object for every FontPlatformData object. But insted, we only
// need one for each unique SkTypeface.
// FIXME, crbug.com/609099: We should fix the FontCache to only keep one
// FontPlatformData object independent of size, then consider using this here.
class HbFontCacheEntry : public RefCounted<HbFontCacheEntry> {
public:
    static PassRefPtr<HbFontCacheEntry> create(hb_font_t* hbFont)
    {
        ASSERT(hbFont);
        return adoptRef(new HbFontCacheEntry(hbFont));
    }

    hb_font_t* hbFont() { return m_hbFont.get(); }
    HarfBuzzFontData* hbFontData() { return m_hbFontData.get(); }

private:
    explicit HbFontCacheEntry(hb_font_t* font)
        : m_hbFont(HbFontUniquePtr(font))
        , m_hbFontData(wrapUnique(new HarfBuzzFontData()))
    { };

    HbFontUniquePtr m_hbFont;
    std::unique_ptr<HarfBuzzFontData> m_hbFontData;
};

typedef HashMap<uint64_t, RefPtr<HbFontCacheEntry>, WTF::IntHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> HarfBuzzFontCache;

static HarfBuzzFontCache* harfBuzzFontCache()
{
    DEFINE_STATIC_LOCAL(HarfBuzzFontCache, s_harfBuzzFontCache, ());
    return &s_harfBuzzFontCache;
}

static PassRefPtr<HbFontCacheEntry> createHbFontCacheEntry(hb_face_t*);

HarfBuzzFace::HarfBuzzFace(FontPlatformData* platformData, uint64_t uniqueID)
    : m_platformData(platformData)
    , m_uniqueID(uniqueID)
{
    HarfBuzzFontCache::AddResult result = harfBuzzFontCache()->add(m_uniqueID, nullptr);
    if (result.isNewEntry) {
        HbFaceUniquePtr face(createFace());
        result.storedValue->value = createHbFontCacheEntry(face.get());
    }
    result.storedValue->value->ref();
    m_unscaledFont = result.storedValue->value->hbFont();
    m_harfBuzzFontData = result.storedValue->value->hbFontData();
}

HarfBuzzFace::~HarfBuzzFace()
{
    HarfBuzzFontCache::iterator result = harfBuzzFontCache()->find(m_uniqueID);
    ASSERT_WITH_SECURITY_IMPLICATION(result != harfBuzzFontCache()->end());
    ASSERT(result.get()->value->refCount() > 1);
    result.get()->value->deref();
    if (result.get()->value->refCount() == 1)
        harfBuzzFontCache()->remove(m_uniqueID);
}

static hb_position_t SkiaScalarToHarfBuzzPosition(SkScalar value)
{
    // We treat HarfBuzz hb_position_t as 16.16 fixed-point.
    static const int kHbPosition1 = 1 << 16;
    return clampTo<int>(value * kHbPosition1);
}

static void SkiaGetGlyphWidthAndExtents(SkPaint* paint, hb_codepoint_t codepoint, hb_position_t* width, hb_glyph_extents_t* extents)
{
    ASSERT(codepoint <= 0xFFFF);
    paint->setTextEncoding(SkPaint::kGlyphID_TextEncoding);

    SkScalar skWidth;
    SkRect skBounds;
    uint16_t glyph = codepoint;

    paint->getTextWidths(&glyph, sizeof(glyph), &skWidth, &skBounds);
    if (width) {
        if (!paint->isSubpixelText())
            skWidth = SkScalarRoundToInt(skWidth);
        *width = SkiaScalarToHarfBuzzPosition(skWidth);
    }
    if (extents) {
        if (!paint->isSubpixelText()) {
            // Use roundOut() rather than round() to avoid rendering glyphs
            // outside the visual overflow rect. crbug.com/452914.
            SkIRect ir;
            skBounds.roundOut(&ir);
            skBounds.set(ir);
        }
        // Invert y-axis because Skia is y-grows-down but we set up HarfBuzz to be y-grows-up.
        extents->x_bearing = SkiaScalarToHarfBuzzPosition(skBounds.fLeft);
        extents->y_bearing = SkiaScalarToHarfBuzzPosition(-skBounds.fTop);
        extents->width = SkiaScalarToHarfBuzzPosition(skBounds.width());
        extents->height = SkiaScalarToHarfBuzzPosition(-skBounds.height());
    }
}

static hb_bool_t harfBuzzGetGlyph(hb_font_t* hbFont, void* fontData, hb_codepoint_t unicode, hb_codepoint_t variationSelector, hb_codepoint_t* glyph, void* userData)
{
    HarfBuzzFontData* hbFontData = reinterpret_cast<HarfBuzzFontData*>(fontData);

    RELEASE_ASSERT(hbFontData);
    if (hbFontData->m_rangeSet && !hbFontData->m_rangeSet->contains(unicode))
        return false;

    return hb_font_get_glyph(hb_font_get_parent(hbFont), unicode, variationSelector, glyph);
}

static hb_position_t harfBuzzGetGlyphHorizontalAdvance(hb_font_t* hbFont, void* fontData, hb_codepoint_t glyph, void* userData)
{
    HarfBuzzFontData* hbFontData = reinterpret_cast<HarfBuzzFontData*>(fontData);
    hb_position_t advance = 0;

    SkiaGetGlyphWidthAndExtents(&hbFontData->m_paint, glyph, &advance, 0);
    return advance;
}

static hb_bool_t harfBuzzGetGlyphVerticalOrigin(hb_font_t* hbFont, void* fontData, hb_codepoint_t glyph, hb_position_t* x, hb_position_t* y, void* userData)
{
    HarfBuzzFontData* hbFontData = reinterpret_cast<HarfBuzzFontData*>(fontData);
    const OpenTypeVerticalData* verticalData = hbFontData->m_simpleFontData->verticalData();
    if (!verticalData)
        return false;

    float result[] = { 0, 0 };
    Glyph theGlyph = glyph;
    verticalData->getVerticalTranslationsForGlyphs(hbFontData->m_simpleFontData, &theGlyph, 1, result);
    *x = SkiaScalarToHarfBuzzPosition(-result[0]);
    *y = SkiaScalarToHarfBuzzPosition(-result[1]);
    return true;
}

static hb_position_t harfBuzzGetGlyphVerticalAdvance(hb_font_t* hbFont, void* fontData, hb_codepoint_t glyph, void* userData)
{
    HarfBuzzFontData* hbFontData = reinterpret_cast<HarfBuzzFontData*>(fontData);
    const OpenTypeVerticalData* verticalData = hbFontData->m_simpleFontData->verticalData();
    if (!verticalData)
        return SkiaScalarToHarfBuzzPosition(hbFontData->m_simpleFontData->getFontMetrics().height());

    Glyph theGlyph = glyph;
    float advanceHeight = -verticalData->advanceHeight(hbFontData->m_simpleFontData, theGlyph);
    return SkiaScalarToHarfBuzzPosition(SkFloatToScalar(advanceHeight));
}

static hb_position_t harfBuzzGetGlyphHorizontalKerning(hb_font_t*, void* fontData, hb_codepoint_t leftGlyph, hb_codepoint_t rightGlyph, void*)
{
    HarfBuzzFontData* hbFontData = reinterpret_cast<HarfBuzzFontData*>(fontData);
    if (hbFontData->m_paint.isVerticalText()) {
        // We don't support cross-stream kerning
        return 0;
    }

    SkTypeface* typeface = hbFontData->m_paint.getTypeface();

    const uint16_t glyphs[2] = { static_cast<uint16_t>(leftGlyph), static_cast<uint16_t>(rightGlyph) };
    int32_t kerningAdjustments[1] = { 0 };

    if (typeface->getKerningPairAdjustments(glyphs, 2, kerningAdjustments)) {
        SkScalar upm = SkIntToScalar(typeface->getUnitsPerEm());
        SkScalar size = hbFontData->m_paint.getTextSize();
        return SkiaScalarToHarfBuzzPosition(SkScalarMulDiv(SkIntToScalar(kerningAdjustments[0]), size, upm));
    }

    return 0;
}

static hb_bool_t harfBuzzGetGlyphExtents(hb_font_t* hbFont, void* fontData, hb_codepoint_t glyph, hb_glyph_extents_t* extents, void* userData)
{
    HarfBuzzFontData* hbFontData = reinterpret_cast<HarfBuzzFontData*>(fontData);

    SkiaGetGlyphWidthAndExtents(&hbFontData->m_paint, glyph, 0, extents);
    return true;
}

static hb_font_funcs_t* harfBuzzSkiaGetFontFuncs()
{
    static hb_font_funcs_t* harfBuzzSkiaFontFuncs = 0;

    // We don't set callback functions which we can't support.
    // HarfBuzz will use the fallback implementation if they aren't set.
    if (!harfBuzzSkiaFontFuncs) {
        harfBuzzSkiaFontFuncs = hb_font_funcs_create();
        hb_font_funcs_set_glyph_func(harfBuzzSkiaFontFuncs, harfBuzzGetGlyph, 0, 0);
        hb_font_funcs_set_glyph_h_advance_func(harfBuzzSkiaFontFuncs, harfBuzzGetGlyphHorizontalAdvance, 0, 0);
        hb_font_funcs_set_glyph_h_kerning_func(harfBuzzSkiaFontFuncs, harfBuzzGetGlyphHorizontalKerning, 0, 0);
        hb_font_funcs_set_glyph_v_advance_func(harfBuzzSkiaFontFuncs, harfBuzzGetGlyphVerticalAdvance, 0, 0);
        hb_font_funcs_set_glyph_v_origin_func(harfBuzzSkiaFontFuncs, harfBuzzGetGlyphVerticalOrigin, 0, 0);
        hb_font_funcs_set_glyph_extents_func(harfBuzzSkiaFontFuncs, harfBuzzGetGlyphExtents, 0, 0);
        hb_font_funcs_make_immutable(harfBuzzSkiaFontFuncs);
    }
    return harfBuzzSkiaFontFuncs;
}

#if !OS(MACOSX)
static hb_blob_t* harfBuzzSkiaGetTable(hb_face_t* face, hb_tag_t tag, void* userData)
{
    SkTypeface* typeface = reinterpret_cast<SkTypeface*>(userData);

    const size_t tableSize = typeface->getTableSize(tag);
    if (!tableSize) {
        return nullptr;
    }

    char* buffer = reinterpret_cast<char*>(WTF::Partitions::fastMalloc(tableSize, WTF_HEAP_PROFILER_TYPE_NAME(HarfBuzzFontData)));
    if (!buffer)
        return nullptr;
    size_t actualSize = typeface->getTableData(tag, 0, tableSize, buffer);
    if (tableSize != actualSize) {
        WTF::Partitions::fastFree(buffer);
        return nullptr;
    }
    return hb_blob_create(const_cast<char*>(buffer), tableSize, HB_MEMORY_MODE_WRITABLE, buffer, WTF::Partitions::fastFree);
}
#endif

#if !OS(MACOSX)
static void deleteTypefaceStream(void* streamAssetPtr)
{
    SkStreamAsset* streamAsset = reinterpret_cast<SkStreamAsset*>(streamAssetPtr);
    delete streamAsset;
}
#endif

hb_face_t* HarfBuzzFace::createFace()
{
#if OS(MACOSX)
    hb_face_t* face = hb_coretext_face_create(m_platformData->cgFont());
#else
    hb_face_t* face = nullptr;

    DEFINE_STATIC_LOCAL(BooleanHistogram,
        zeroCopySuccessHistogram,
        ("Blink.Fonts.HarfBuzzFaceZeroCopyAccess"));
    SkTypeface* typeface = m_platformData->typeface();
    CHECK(typeface);
    int ttcIndex = 0;
    SkStreamAsset* typefaceStream = typeface->openStream(&ttcIndex);
    if (typefaceStream && typefaceStream->getMemoryBase()) {
        std::unique_ptr<hb_blob_t, void(*)(hb_blob_t*)> faceBlob(hb_blob_create(
            reinterpret_cast<const char*>(typefaceStream->getMemoryBase()),
            typefaceStream->getLength(),
            HB_MEMORY_MODE_READONLY,
            typefaceStream,
            deleteTypefaceStream),
            hb_blob_destroy);
        face = hb_face_create(faceBlob.get(), ttcIndex);
    }

    // Fallback to table copies if there is no in-memory access.
    if (!face) {
        face = hb_face_create_for_tables(harfBuzzSkiaGetTable, m_platformData->typeface(), 0);
        zeroCopySuccessHistogram.count(false);
    } else {
        zeroCopySuccessHistogram.count(true);
    }
#endif
    ASSERT(face);
    return face;
}

PassRefPtr<HbFontCacheEntry> createHbFontCacheEntry(hb_face_t* face)
{
    HbFontUniquePtr otFont(hb_font_create(face));
    hb_ot_font_set_funcs(otFont.get());
    // Creating a sub font means that non-available functions
    // are found from the parent.
    hb_font_t* unscaledFont = hb_font_create_sub_font(otFont.get());
    RefPtr<HbFontCacheEntry> cacheEntry = HbFontCacheEntry::create(unscaledFont);
    hb_font_set_funcs(unscaledFont, harfBuzzSkiaGetFontFuncs(), cacheEntry->hbFontData(), nullptr);
    return cacheEntry;
}

hb_font_t* HarfBuzzFace::getScaledFont(PassRefPtr<UnicodeRangeSet> rangeSet) const
{
    m_platformData->setupPaint(&m_harfBuzzFontData->m_paint);
    m_harfBuzzFontData->m_rangeSet = rangeSet;
    m_harfBuzzFontData->m_simpleFontData = FontCache::fontCache()->fontDataFromFontPlatformData(m_platformData).get();
    ASSERT(m_harfBuzzFontData->m_simpleFontData);
    int scale = SkiaScalarToHarfBuzzPosition(m_platformData->size());
    hb_font_set_scale(m_unscaledFont, scale, scale);
    return m_unscaledFont;
}

} // namespace blink
