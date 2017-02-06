/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
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
 * along with this library; see the file COPYING.LIother.m_  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef FontDescription_h
#define FontDescription_h

#include "SkFontStyle.h"
#include "platform/FontFamilyNames.h"
#include "platform/fonts/FontCacheKey.h"
#include "platform/fonts/FontFamily.h"
#include "platform/fonts/FontFeatureSettings.h"
#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/FontSmoothingMode.h"
#include "platform/fonts/FontTraits.h"
#include "platform/fonts/FontVariantNumeric.h"
#include "platform/fonts/FontWidthVariant.h"
#include "platform/fonts/TextRenderingMode.h"
#include "platform/fonts/TypesettingFeatures.h"
#include "platform/text/LocaleToScriptMapping.h"
#include "wtf/Allocator.h"
#include "wtf/MathExtras.h"

#include "wtf/RefPtr.h"

#include <unicode/uscript.h>

namespace blink {

const float FontSizeAdjustNone = -1;
typedef struct {
    uint32_t parts[2];
} FieldsAsUnsignedType;

class PLATFORM_EXPORT FontDescription {
    USING_FAST_MALLOC(FontDescription);
public:
    enum GenericFamilyType { NoFamily, StandardFamily, SerifFamily, SansSerifFamily,
                             MonospaceFamily, CursiveFamily, FantasyFamily, PictographFamily };

    enum Kerning { AutoKerning, NormalKerning, NoneKerning };

    enum LigaturesState { NormalLigaturesState, DisabledLigaturesState, EnabledLigaturesState };

    enum FontVariantCaps { CapsNormal, SmallCaps, AllSmallCaps, PetiteCaps, AllPetiteCaps, Unicase, TitlingCaps };

    FontDescription()
        : m_specifiedSize(0)
        , m_computedSize(0)
        , m_adjustedSize(0)
        , m_sizeAdjust(FontSizeAdjustNone)
        , m_letterSpacing(0)
        , m_wordSpacing(0)
    {
        m_fieldsAsUnsigned.parts[0] = 0;
        m_fieldsAsUnsigned.parts[1] = 0;
        m_fields.m_orientation = static_cast<unsigned>(FontOrientation::Horizontal);
        m_fields.m_widthVariant = RegularWidth;
        m_fields.m_style = FontStyleNormal;
        m_fields.m_variantCaps = CapsNormal;
        m_fields.m_isAbsoluteSize = false;
        m_fields.m_weight = FontWeightNormal;
        m_fields.m_stretch = FontStretchNormal;
        m_fields.m_genericFamily = NoFamily;
        m_fields.m_kerning = AutoKerning;
        m_fields.m_commonLigaturesState = NormalLigaturesState;
        m_fields.m_discretionaryLigaturesState = NormalLigaturesState;
        m_fields.m_historicalLigaturesState = NormalLigaturesState;
        m_fields.m_contextualLigaturesState = NormalLigaturesState;
        m_fields.m_keywordSize = 0;
        m_fields.m_fontSmoothing = AutoSmoothing;
        m_fields.m_textRendering = AutoTextRendering;
        m_fields.m_script = USCRIPT_COMMON;
        m_fields.m_syntheticBold = false;
        m_fields.m_syntheticItalic = false;
        m_fields.m_subpixelTextPosition = s_useSubpixelTextPositioning;
        m_fields.m_typesettingFeatures = s_defaultTypesettingFeatures;
        m_fields.m_variantNumeric = FontVariantNumeric().m_fieldsAsUnsigned;
    }

    bool operator==(const FontDescription&) const;
    bool operator!=(const FontDescription& other) const { return !(*this == other); }

    struct VariantLigatures {
        STACK_ALLOCATED();
        VariantLigatures(LigaturesState state = NormalLigaturesState)
            : common(state)
            , discretionary(state)
            , historical(state)
            , contextual(state)
        {
        }

        unsigned common : 2;
        unsigned discretionary : 2;
        unsigned historical : 2;
        unsigned contextual : 2;
    };

    struct Size {
        STACK_ALLOCATED();
        Size(unsigned keyword, float value, bool isAbsolute)
            : keyword(keyword)
            , isAbsolute(isAbsolute)
            , value(value)
        {
        }
        unsigned keyword : 4; // FontDescription::keywordSize
        unsigned isAbsolute : 1; // FontDescription::isAbsoluteSize
        float value;
    };

    struct FamilyDescription {
        STACK_ALLOCATED();
        FamilyDescription(GenericFamilyType genericFamily) : genericFamily(genericFamily) { }
        FamilyDescription(GenericFamilyType genericFamily, const FontFamily& family)
            : genericFamily(genericFamily)
            , family(family)
        {
        }
        GenericFamilyType genericFamily;
        FontFamily family;
    };

    const FontFamily& family() const { return m_familyList; }
    FamilyDescription getFamilyDescription() const { return FamilyDescription(genericFamily(), family()); }
    FontFamily& firstFamily() { return m_familyList; }
    Size getSize() const { return Size(keywordSize(), specifiedSize(), isAbsoluteSize()); }
    float specifiedSize() const { return m_specifiedSize; }
    float computedSize() const { return m_computedSize; }
    float adjustedSize() const { return m_adjustedSize; }
    float sizeAdjust() const { return m_sizeAdjust; }
    bool hasSizeAdjust() const { return m_sizeAdjust != FontSizeAdjustNone; }
    FontStyle style() const { return static_cast<FontStyle>(m_fields.m_style); }
    int computedPixelSize() const { return int(m_computedSize + 0.5f); }
    FontVariantCaps variantCaps() const { return static_cast<FontVariantCaps>(m_fields.m_variantCaps); }
    bool isAbsoluteSize() const { return m_fields.m_isAbsoluteSize; }
    FontWeight weight() const { return static_cast<FontWeight>(m_fields.m_weight); }
    FontStretch stretch() const { return static_cast<FontStretch>(m_fields.m_stretch); }
    static FontWeight lighterWeight(FontWeight);
    static FontWeight bolderWeight(FontWeight);
    static Size largerSize(const Size&);
    static Size smallerSize(const Size&);
    GenericFamilyType genericFamily() const { return static_cast<GenericFamilyType>(m_fields.m_genericFamily); }

    // only use fixed default size when there is only one font family, and that family is "monospace"
    bool isMonospace() const
    {
        return genericFamily() == MonospaceFamily && !family().next() && family().family() == FontFamilyNames::webkit_monospace;
    }
    Kerning getKerning() const { return static_cast<Kerning>(m_fields.m_kerning); }
    VariantLigatures getVariantLigatures() const;
    FontVariantNumeric variantNumeric() const  { return FontVariantNumeric::initializeFromUnsigned(m_fields.m_variantNumeric); };
    LigaturesState commonLigaturesState() const { return static_cast<LigaturesState>(m_fields.m_commonLigaturesState); }
    LigaturesState discretionaryLigaturesState() const { return static_cast<LigaturesState>(m_fields.m_discretionaryLigaturesState); }
    LigaturesState historicalLigaturesState() const { return static_cast<LigaturesState>(m_fields.m_historicalLigaturesState); }
    LigaturesState contextualLigaturesState() const { return static_cast<LigaturesState>(m_fields.m_contextualLigaturesState); }
    unsigned keywordSize() const { return m_fields.m_keywordSize; }
    FontSmoothingMode fontSmoothing() const { return static_cast<FontSmoothingMode>(m_fields.m_fontSmoothing); }
    TextRenderingMode textRendering() const { return static_cast<TextRenderingMode>(m_fields.m_textRendering); }
    UScriptCode script() const { return static_cast<UScriptCode>(m_fields.m_script); }
    const AtomicString& locale(bool includeDefault = true) const;
    bool isSyntheticBold() const { return m_fields.m_syntheticBold; }
    bool isSyntheticItalic() const { return m_fields.m_syntheticItalic; }
    bool useSubpixelPositioning() const { return m_fields.m_subpixelTextPosition; }

    FontTraits traits() const;
    float wordSpacing() const { return m_wordSpacing; }
    float letterSpacing() const { return m_letterSpacing; }
    FontOrientation orientation() const { return static_cast<FontOrientation>(m_fields.m_orientation); }
    bool isVerticalAnyUpright() const { return blink::isVerticalAnyUpright(orientation()); }
    bool isVerticalNonCJKUpright() const { return blink::isVerticalNonCJKUpright(orientation()); }
    bool isVerticalUpright(UChar32 character) const { return blink::isVerticalUpright(orientation(), character); }
    bool isVerticalBaseline() const { return blink::isVerticalBaseline(orientation()); }
    FontWidthVariant widthVariant() const { return static_cast<FontWidthVariant>(m_fields.m_widthVariant); }
    FontFeatureSettings* featureSettings() const { return m_featureSettings.get(); }

    float effectiveFontSize() const; // Returns either the computedSize or the computedPixelSize
    FontCacheKey cacheKey(const FontFaceCreationParams&, FontTraits desiredTraits = FontTraits(0)) const;

    void setFamily(const FontFamily& family) { m_familyList = family; }
    void setComputedSize(float s) { m_computedSize = clampTo<float>(s); }
    void setSpecifiedSize(float s) { m_specifiedSize = clampTo<float>(s); }
    void setAdjustedSize(float s) { m_adjustedSize = clampTo<float>(s); }
    void setSizeAdjust(float aspect) { m_sizeAdjust = clampTo<float>(aspect); }
    void setStyle(FontStyle i) { m_fields.m_style = i; }
    void setVariantCaps(FontVariantCaps);
    void setVariantLigatures(const VariantLigatures&);
    void setVariantNumeric(const FontVariantNumeric&);
    void setIsAbsoluteSize(bool s) { m_fields.m_isAbsoluteSize = s; }
    void setWeight(FontWeight w) { m_fields.m_weight = w; }
    void setStretch(FontStretch s) { m_fields.m_stretch = s; }
    void setGenericFamily(GenericFamilyType genericFamily) { m_fields.m_genericFamily = genericFamily; }
    void setKerning(Kerning kerning) { m_fields.m_kerning = kerning; updateTypesettingFeatures(); }
    void setKeywordSize(unsigned s) { m_fields.m_keywordSize = s; }
    void setFontSmoothing(FontSmoothingMode smoothing) { m_fields.m_fontSmoothing = smoothing; }
    void setTextRendering(TextRenderingMode rendering) { m_fields.m_textRendering = rendering; updateTypesettingFeatures(); }
    void setOrientation(FontOrientation orientation) { m_fields.m_orientation = static_cast<unsigned>(orientation); }
    void setWidthVariant(FontWidthVariant widthVariant) { m_fields.m_widthVariant = widthVariant; }
    void setLocale(const AtomicString& locale)
    {
        m_locale = locale;
        m_fields.m_script = localeToScriptCodeForFontSelection(locale);
    }
    void setSyntheticBold(bool syntheticBold) { m_fields.m_syntheticBold = syntheticBold; }
    void setSyntheticItalic(bool syntheticItalic) { m_fields.m_syntheticItalic = syntheticItalic; }
    void setFeatureSettings(PassRefPtr<FontFeatureSettings> settings) { m_featureSettings = settings; }
    void setTraits(FontTraits);
    void setWordSpacing(float s) { m_wordSpacing = s; }
    void setLetterSpacing(float s) { m_letterSpacing = s; updateTypesettingFeatures(); }

    TypesettingFeatures getTypesettingFeatures() const { return static_cast<TypesettingFeatures>(m_fields.m_typesettingFeatures); }

    static void setSubpixelPositioning(bool b) { s_useSubpixelTextPositioning = b; }
    static bool subpixelPositioning() { return s_useSubpixelTextPositioning; }

    static void setDefaultTypesettingFeatures(TypesettingFeatures);
    static TypesettingFeatures defaultTypesettingFeatures();

    unsigned styleHashWithoutFamilyList() const;
    // TODO(drott): We should not expose internal structure here, but rather introduce
    // a hash function here.
    unsigned bitmapFields() const { return m_fieldsAsUnsigned.parts[0]; }
    unsigned auxiliaryBitmapFields() const { return m_fieldsAsUnsigned.parts[1]; }

    SkFontStyle skiaFontStyle() const;

private:
    FontFamily m_familyList; // The list of font families to be used.
    RefPtr<FontFeatureSettings> m_featureSettings;
    AtomicString m_locale;

    void updateTypesettingFeatures();

    float m_specifiedSize;   // Specified CSS value. Independent of rendering issues such as integer
                             // rounding, minimum font sizes, and zooming.
    float m_computedSize;    // Computed size adjusted for the minimum font size and the zoom factor.

    // (Given aspect value / aspect value of a font family) * specifiedSize.
    // This value is adjusted for the minimum font size and the zoom factor
    // as well as a computed size is.
    float m_adjustedSize;

    // Given aspect value, i.e. font-size-adjust.
    float m_sizeAdjust;

    float m_letterSpacing;
    float m_wordSpacing;

    struct BitFields {
        DISALLOW_NEW();
        unsigned m_orientation : static_cast<unsigned>(FontOrientation::BitCount);

        unsigned m_widthVariant : 2; // FontWidthVariant

        unsigned m_style : 2; // FontStyle
        unsigned m_variantCaps : 3; // FontVariantCaps
        unsigned m_isAbsoluteSize : 1; // Whether or not CSS specified an explicit size
        // (logical sizes like "medium" don't count).
        unsigned m_weight : 4; // FontWeight
        unsigned m_stretch : 4; // FontStretch
        unsigned m_genericFamily : 3; // GenericFamilyType

        unsigned m_kerning : 2; // Kerning

        unsigned m_commonLigaturesState : 2;
        unsigned m_discretionaryLigaturesState : 2;
        unsigned m_historicalLigaturesState : 2;
        unsigned m_contextualLigaturesState : 2;

        unsigned m_keywordSize : 4; // We cache whether or not a font is currently represented by a CSS keyword (e.g., medium).  If so,
        // then we can accurately translate across different generic families to adjust for different preference settings
        // (e.g., 13px monospace vs. 16px everything else).  Sizes are 1-8 (like the HTML size values for <font>).

        unsigned m_fontSmoothing : 2; // FontSmoothingMode
        unsigned m_textRendering : 2; // TextRenderingMode
        unsigned m_script : 7; // Used to help choose an appropriate font for generic font families.
        unsigned m_syntheticBold : 1;
        unsigned m_syntheticItalic : 1;
        unsigned m_subpixelTextPosition : 1;
        unsigned m_typesettingFeatures : 3;
        unsigned m_variantNumeric : 8;
    };

    static_assert(sizeof(BitFields) == sizeof(FieldsAsUnsignedType),
        "Mapped bitfield datatypes must have identical size.");
    union {
        BitFields m_fields;
        FieldsAsUnsignedType m_fieldsAsUnsigned;
    };

    static TypesettingFeatures s_defaultTypesettingFeatures;

    static bool s_useSubpixelTextPositioning;
};

inline bool FontDescription::operator==(const FontDescription& other) const
{
    return m_familyList == other.m_familyList
        && m_locale == other.m_locale
        && m_specifiedSize == other.m_specifiedSize
        && m_computedSize == other.m_computedSize
        && m_adjustedSize == other.m_adjustedSize
        && m_sizeAdjust == other.m_sizeAdjust
        && m_letterSpacing == other.m_letterSpacing
        && m_wordSpacing == other.m_wordSpacing
        && m_fieldsAsUnsigned.parts[0] == other.m_fieldsAsUnsigned.parts[0]
        && m_fieldsAsUnsigned.parts[1] == other.m_fieldsAsUnsigned.parts[1]
        && (m_featureSettings == other.m_featureSettings || (m_featureSettings && other.m_featureSettings && *m_featureSettings == *other.m_featureSettings));
}

} // namespace blink

#endif
