/*
 * Copyright (C) 2007 Nicholas Shanks <contact@nickshanks.com>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/FontDescription.h"

#include "platform/Language.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/StringHasher.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

namespace blink {

struct SameSizeAsFontDescription {
    DISALLOW_NEW();
    FontFamily familyList;
    RefPtr<FontFeatureSettings> m_featureSettings;
    AtomicString locale;
    float sizes[6];
    FieldsAsUnsignedType bitfields;
};

static_assert(sizeof(FontDescription) == sizeof(SameSizeAsFontDescription), "FontDescription should stay small");

TypesettingFeatures FontDescription::s_defaultTypesettingFeatures = 0;

bool FontDescription::s_useSubpixelTextPositioning = false;

FontWeight FontDescription::lighterWeight(FontWeight weight)
{
    switch (weight) {
        case FontWeight100:
        case FontWeight200:
        case FontWeight300:
        case FontWeight400:
        case FontWeight500:
            return FontWeight100;

        case FontWeight600:
        case FontWeight700:
            return FontWeight400;

        case FontWeight800:
        case FontWeight900:
            return FontWeight700;
    }
    ASSERT_NOT_REACHED();
    return FontWeightNormal;
}

FontWeight FontDescription::bolderWeight(FontWeight weight)
{
    switch (weight) {
        case FontWeight100:
        case FontWeight200:
        case FontWeight300:
            return FontWeight400;

        case FontWeight400:
        case FontWeight500:
            return FontWeight700;

        case FontWeight600:
        case FontWeight700:
        case FontWeight800:
        case FontWeight900:
            return FontWeight900;
    }
    ASSERT_NOT_REACHED();
    return FontWeightNormal;
}

FontDescription::Size FontDescription::largerSize(const Size& size)
{
    return Size(0, size.value * 1.2, size.isAbsolute);
}

FontDescription::Size FontDescription::smallerSize(const Size& size)
{
    return Size(0, size.value / 1.2, size.isAbsolute);
}

FontTraits FontDescription::traits() const
{
    return FontTraits(style(), weight(), stretch());
}

FontDescription::VariantLigatures FontDescription::getVariantLigatures() const
{
    VariantLigatures ligatures;

    ligatures.common = commonLigaturesState();
    ligatures.discretionary = discretionaryLigaturesState();
    ligatures.historical = historicalLigaturesState();
    ligatures.contextual = contextualLigaturesState();

    return ligatures;
}

static const AtomicString& defaultLocale()
{
    DEFINE_STATIC_LOCAL(AtomicString, locale, ());
    if (locale.isNull()) {
        AtomicString defaultLocale = defaultLanguage();
        if (!defaultLocale.isEmpty())
            locale = defaultLocale;
        else
            locale = AtomicString("en");
    }
    return locale;
}

const AtomicString& FontDescription::locale(bool includeDefault) const
{
    if (m_locale.isNull() && includeDefault)
        return defaultLocale();
    return m_locale;
}

void FontDescription::setTraits(FontTraits traits)
{
    setStyle(traits.style());
    setWeight(traits.weight());
    setStretch(traits.stretch());
}

void FontDescription::setVariantCaps(FontVariantCaps variantCaps)
{
    m_fields.m_variantCaps = variantCaps;

    updateTypesettingFeatures();
}

void FontDescription::setVariantLigatures(const VariantLigatures& ligatures)
{
    m_fields.m_commonLigaturesState = ligatures.common;
    m_fields.m_discretionaryLigaturesState = ligatures.discretionary;
    m_fields.m_historicalLigaturesState = ligatures.historical;
    m_fields.m_contextualLigaturesState = ligatures.contextual;

    updateTypesettingFeatures();
}

void FontDescription::setVariantNumeric(const FontVariantNumeric& variantNumeric)
{
    m_fields.m_variantNumeric = variantNumeric.m_fieldsAsUnsigned;

    updateTypesettingFeatures();
}

float FontDescription::effectiveFontSize() const
{
    // Ensure that the effective precision matches the font-cache precision.
    // This guarantees that the same precision is used regardless of cache status.
    float computedOrAdjustedSize = hasSizeAdjust() ? adjustedSize() : computedSize();
    return floorf(computedOrAdjustedSize * FontCacheKey::precisionMultiplier()) / FontCacheKey::precisionMultiplier();
}

FontCacheKey FontDescription::cacheKey(const FontFaceCreationParams& creationParams, FontTraits desiredTraits) const
{
    FontTraits fontTraits = desiredTraits.bitfield() ? desiredTraits : traits();

    unsigned options =
        static_cast<unsigned>(m_fields.m_syntheticItalic) << 6 | // bit 7
        static_cast<unsigned>(m_fields.m_syntheticBold) << 5 | // bit 6
        static_cast<unsigned>(m_fields.m_textRendering) << 3 | // bits 4-5
        static_cast<unsigned>(m_fields.m_orientation) << 1 | // bit 2-3
        static_cast<unsigned>(m_fields.m_subpixelTextPosition); // bit 1

    return FontCacheKey(creationParams, effectiveFontSize(), options | fontTraits.bitfield() << 8);
}

void FontDescription::setDefaultTypesettingFeatures(TypesettingFeatures typesettingFeatures)
{
    s_defaultTypesettingFeatures = typesettingFeatures;
}

TypesettingFeatures FontDescription::defaultTypesettingFeatures()
{
    return s_defaultTypesettingFeatures;
}

void FontDescription::updateTypesettingFeatures()
{
    m_fields.m_typesettingFeatures = s_defaultTypesettingFeatures;

    switch (textRendering()) {
    case AutoTextRendering:
        break;
    case OptimizeSpeed:
        m_fields.m_typesettingFeatures &= ~(blink::Kerning | Ligatures);
        break;
    case GeometricPrecision:
    case OptimizeLegibility:
        m_fields.m_typesettingFeatures |= blink::Kerning | Ligatures;
        break;
    }

    switch (getKerning()) {
    case FontDescription::NoneKerning:
        m_fields.m_typesettingFeatures &= ~blink::Kerning;
        break;
    case FontDescription::NormalKerning:
        m_fields.m_typesettingFeatures |= blink::Kerning;
        break;
    case FontDescription::AutoKerning:
        break;
    }

    // As per CSS (http://dev.w3.org/csswg/css-text-3/#letter-spacing-property),
    // When the effective letter-spacing between two characters is not zero (due to
    // either justification or non-zero computed letter-spacing), user agents should
    // not apply optional ligatures.
    if (m_letterSpacing == 0) {
        switch (commonLigaturesState()) {
        case FontDescription::DisabledLigaturesState:
            m_fields.m_typesettingFeatures &= ~blink::Ligatures;
            break;
        case FontDescription::EnabledLigaturesState:
            m_fields.m_typesettingFeatures |= blink::Ligatures;
            break;
        case FontDescription::NormalLigaturesState:
            break;
        }

        if (discretionaryLigaturesState() == FontDescription::EnabledLigaturesState
            || historicalLigaturesState() == FontDescription::EnabledLigaturesState
            || contextualLigaturesState() == FontDescription::EnabledLigaturesState) {
            m_fields.m_typesettingFeatures |= blink::Ligatures;
        }
    }

    if (variantCaps() != CapsNormal)
        m_fields.m_typesettingFeatures |= blink::Caps;
}

static inline void addToHash(unsigned& hash, unsigned key)
{
    hash = ((hash << 5) + hash) + key; // Djb2
}

static inline void addFloatToHash(unsigned& hash, float value)
{
    addToHash(hash, StringHasher::hashMemory(&value, sizeof(value)));
}

unsigned FontDescription::styleHashWithoutFamilyList() const
{
    unsigned hash = 0;
    StringHasher stringHasher;
    const FontFeatureSettings* settings = featureSettings();
    if (settings) {
        unsigned numFeatures = settings->size();
        for (unsigned i = 0; i < numFeatures; ++i) {
            const AtomicString& tag = settings->at(i).tag();
            for (unsigned j = 0; j < tag.length(); j++)
                stringHasher.addCharacter(tag[j]);
            addToHash(hash, settings->at(i).value());
        }
    }
    for (unsigned i = 0; i < m_locale.length(); i++)
        stringHasher.addCharacter(m_locale[i]);
    addToHash(hash, stringHasher.hash());

    addFloatToHash(hash, m_specifiedSize);
    addFloatToHash(hash, m_computedSize);
    addFloatToHash(hash, m_adjustedSize);
    addFloatToHash(hash, m_sizeAdjust);
    addFloatToHash(hash, m_letterSpacing);
    addFloatToHash(hash, m_wordSpacing);
    addToHash(hash, m_fieldsAsUnsigned.parts[0]);
    addToHash(hash, m_fieldsAsUnsigned.parts[1]);

    return hash;
}

SkFontStyle FontDescription::skiaFontStyle() const
{
    int width = static_cast<int>(stretch());
    SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
    switch (style()) {
    case FontStyleNormal: slant = SkFontStyle::kUpright_Slant; break;
    case FontStyleItalic: slant = SkFontStyle::kItalic_Slant; break;
    case FontStyleOblique: slant = SkFontStyle::kOblique_Slant; break;
    default: NOTREACHED(); break;
    }
    return SkFontStyle(numericFontWeight(weight()), width, slant);
    static_assert(
        static_cast<int>(FontStretchUltraCondensed) == static_cast<int>(SkFontStyle::kUltraCondensed_Width),
        "FontStretchUltraCondensed should map to kUltraCondensed_Width");
    static_assert(
        static_cast<int>(FontStretchNormal) == static_cast<int>(SkFontStyle::kNormal_Width),
        "FontStretchNormal should map to kNormal_Width");
    static_assert(
        static_cast<int>(FontStretchUltraExpanded) == static_cast<int>(SkFontStyle::kUltaExpanded_Width),
        "FontStretchUltraExpanded should map to kUltaExpanded_Width");
}

} // namespace blink
