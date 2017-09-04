/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#ifndef SimpleFontData_h
#define SimpleFontData_h

#include "platform/PlatformExport.h"
#include "platform/fonts/CustomFontData.h"
#include "platform/fonts/FontBaseline.h"
#include "platform/fonts/FontData.h"
#include "platform/fonts/FontMetrics.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/TypesettingFeatures.h"
#include "platform/fonts/opentype/OpenTypeVerticalData.h"
#include "platform/geometry/FloatRect.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringHash.h"
#include <SkPaint.h>
#include <memory>

#if OS(MACOSX)
#include "platform/fonts/GlyphMetricsMap.h"
#endif

namespace blink {

// Holds the glyph index and the corresponding SimpleFontData information for a
// given
// character.
struct GlyphData {
  GlyphData(Glyph g = 0, const SimpleFontData* f = 0) : glyph(g), fontData(f) {}
  Glyph glyph;
  const SimpleFontData* fontData;
};

class FontDescription;

enum FontDataVariant {
  AutoVariant,
  NormalVariant,
  SmallCapsVariant,
  EmphasisMarkVariant
};

class PLATFORM_EXPORT SimpleFontData : public FontData {
 public:
  // Used to create platform fonts.
  static PassRefPtr<SimpleFontData> create(
      const FontPlatformData& platformData,
      PassRefPtr<CustomFontData> customData = nullptr,
      bool isTextOrientationFallback = false,
      bool subpixelAscentDescent = false) {
    return adoptRef(new SimpleFontData(platformData, std::move(customData),
                                       isTextOrientationFallback,
                                       subpixelAscentDescent));
  }

  const FontPlatformData& platformData() const { return m_platformData; }
  const OpenTypeVerticalData* verticalData() const {
    return m_verticalData.get();
  }

  PassRefPtr<SimpleFontData> smallCapsFontData(const FontDescription&) const;
  PassRefPtr<SimpleFontData> emphasisMarkFontData(const FontDescription&) const;

  PassRefPtr<SimpleFontData> variantFontData(const FontDescription& description,
                                             FontDataVariant variant) const {
    switch (variant) {
      case SmallCapsVariant:
        return smallCapsFontData(description);
      case EmphasisMarkVariant:
        return emphasisMarkFontData(description);
      case AutoVariant:
      case NormalVariant:
        break;
    }
    ASSERT_NOT_REACHED();
    return const_cast<SimpleFontData*>(this);
  }

  PassRefPtr<SimpleFontData> verticalRightOrientationFontData() const;
  PassRefPtr<SimpleFontData> uprightOrientationFontData() const;

  bool hasVerticalGlyphs() const { return m_hasVerticalGlyphs; }
  bool isTextOrientationFallback() const { return m_isTextOrientationFallback; }
  bool isTextOrientationFallbackOf(const SimpleFontData*) const;

  FontMetrics& getFontMetrics() { return m_fontMetrics; }
  const FontMetrics& getFontMetrics() const { return m_fontMetrics; }
  float sizePerUnit() const {
    return platformData().size() /
           (getFontMetrics().unitsPerEm() ? getFontMetrics().unitsPerEm() : 1);
  }
  float internalLeading() const {
    return getFontMetrics().floatHeight() - platformData().size();
  }

  float maxCharWidth() const { return m_maxCharWidth; }
  void setMaxCharWidth(float maxCharWidth) { m_maxCharWidth = maxCharWidth; }

  float avgCharWidth() const { return m_avgCharWidth; }
  void setAvgCharWidth(float avgCharWidth) { m_avgCharWidth = avgCharWidth; }

  FloatRect boundsForGlyph(Glyph) const;
  FloatRect platformBoundsForGlyph(Glyph) const;
  float widthForGlyph(Glyph) const;
  float platformWidthForGlyph(Glyph) const;

  float spaceWidth() const { return m_spaceWidth; }
  void setSpaceWidth(float spaceWidth) { m_spaceWidth = spaceWidth; }

  Glyph spaceGlyph() const { return m_spaceGlyph; }
  void setSpaceGlyph(Glyph spaceGlyph) { m_spaceGlyph = spaceGlyph; }
  Glyph zeroGlyph() const { return m_zeroGlyph; }
  void setZeroGlyph(Glyph zeroGlyph) { m_zeroGlyph = zeroGlyph; }

  const SimpleFontData* fontDataForCharacter(UChar32) const override;

  Glyph glyphForCharacter(UChar32) const;

  bool isCustomFont() const override { return m_customFontData.get(); }
  bool isLoading() const override {
    return m_customFontData ? m_customFontData->isLoading() : false;
  }
  bool isLoadingFallback() const override {
    return m_customFontData ? m_customFontData->isLoadingFallback() : false;
  }
  bool isSegmented() const override;
  bool shouldSkipDrawing() const override {
    return m_customFontData && m_customFontData->shouldSkipDrawing();
  }

  const GlyphData& missingGlyphData() const { return m_missingGlyphData; }
  void setMissingGlyphData(const GlyphData& glyphData) {
    m_missingGlyphData = glyphData;
  }

  bool canRenderCombiningCharacterSequence(const UChar*, size_t) const;

  CustomFontData* customFontData() const { return m_customFontData.get(); }

 protected:
  SimpleFontData(const FontPlatformData&,
                 PassRefPtr<CustomFontData> customData,
                 bool isTextOrientationFallback = false,
                 bool subpixelAscentDescent = false);

  SimpleFontData(PassRefPtr<CustomFontData> customData,
                 float fontSize,
                 bool syntheticBold,
                 bool syntheticItalic);

 private:
  void platformInit(bool subpixelAscentDescent);
  void platformGlyphInit();

  PassRefPtr<SimpleFontData> createScaledFontData(const FontDescription&,
                                                  float scaleFactor) const;

  FontMetrics m_fontMetrics;
  float m_maxCharWidth;
  float m_avgCharWidth;

  FontPlatformData m_platformData;
  SkPaint m_paint;

  bool m_isTextOrientationFallback;
  RefPtr<OpenTypeVerticalData> m_verticalData;
  bool m_hasVerticalGlyphs;

  Glyph m_spaceGlyph;
  float m_spaceWidth;
  Glyph m_zeroGlyph;

  GlyphData m_missingGlyphData;

  struct DerivedFontData {
    USING_FAST_MALLOC(DerivedFontData);
    WTF_MAKE_NONCOPYABLE(DerivedFontData);

   public:
    static std::unique_ptr<DerivedFontData> create();

    RefPtr<SimpleFontData> smallCaps;
    RefPtr<SimpleFontData> emphasisMark;
    RefPtr<SimpleFontData> verticalRightOrientation;
    RefPtr<SimpleFontData> uprightOrientation;

   private:
    DerivedFontData() {}
  };

  mutable std::unique_ptr<DerivedFontData> m_derivedFontData;

  RefPtr<CustomFontData> m_customFontData;

// See discussion on crbug.com/631032 and Skiaissue
// https://bugs.chromium.org/p/skia/issues/detail?id=5328 :
// On Mac we're still using path based glyph metrics, and they seem to be
// too slow to be able to remove the caching layer we have here.
#if OS(MACOSX)
  mutable std::unique_ptr<GlyphMetricsMap<FloatRect>> m_glyphToBoundsMap;
  mutable GlyphMetricsMap<float> m_glyphToWidthMap;
#endif
};

ALWAYS_INLINE FloatRect SimpleFontData::boundsForGlyph(Glyph glyph) const {
#if !OS(MACOSX)
  return platformBoundsForGlyph(glyph);
#else
  FloatRect boundsResult;
  if (m_glyphToBoundsMap) {
    boundsResult = m_glyphToBoundsMap->metricsForGlyph(glyph);
    if (boundsResult.width() != cGlyphSizeUnknown)
      return boundsResult;
  }

  boundsResult = platformBoundsForGlyph(glyph);
  if (!m_glyphToBoundsMap)
    m_glyphToBoundsMap = wrapUnique(new GlyphMetricsMap<FloatRect>);
  m_glyphToBoundsMap->setMetricsForGlyph(glyph, boundsResult);

  return boundsResult;
#endif
}

ALWAYS_INLINE float SimpleFontData::widthForGlyph(Glyph glyph) const {
#if !OS(MACOSX)
  return platformWidthForGlyph(glyph);
#else
  float width = m_glyphToWidthMap.metricsForGlyph(glyph);
  if (width != cGlyphSizeUnknown)
    return width;

  width = platformWidthForGlyph(glyph);

  m_glyphToWidthMap.setMetricsForGlyph(glyph, width);
  return width;
#endif
}

DEFINE_FONT_DATA_TYPE_CASTS(SimpleFontData, false);

}  // namespace blink
#endif  // SimpleFontData_h
