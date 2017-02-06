/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "core/css/CSSSegmentedFontFace.h"

#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontSelector.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFaceCreationParams.h"
#include "platform/fonts/SegmentedFontData.h"
#include "platform/fonts/SimpleFontData.h"

namespace blink {

CSSSegmentedFontFace::CSSSegmentedFontFace(CSSFontSelector* fontSelector, FontTraits traits)
    : m_fontSelector(fontSelector)
    , m_traits(traits)
    , m_firstNonCssConnectedFace(m_fontFaces.end())
    , m_approximateCharacterCount(0)
{
}

CSSSegmentedFontFace::~CSSSegmentedFontFace()
{
}

void CSSSegmentedFontFace::pruneTable()
{
    // Make sure the glyph page tree prunes out all uses of this custom font.
    if (m_fontDataTable.isEmpty())
        return;

    m_fontDataTable.clear();
}

bool CSSSegmentedFontFace::isValid() const
{
    // Valid if at least one font face is valid.
    for (const auto& fontFace : m_fontFaces) {
        if (fontFace->cssFontFace()->isValid())
            return true;
    }
    return false;
}

void CSSSegmentedFontFace::fontFaceInvalidated()
{
    pruneTable();
}

void CSSSegmentedFontFace::addFontFace(FontFace* fontFace, bool cssConnected)
{
    pruneTable();
    fontFace->cssFontFace()->setSegmentedFontFace(this);
    if (cssConnected) {
        m_fontFaces.insertBefore(m_firstNonCssConnectedFace, fontFace);
    } else {
        // This is the only place in Blink that is using addReturnIterator.
        FontFaceList::iterator iterator = m_fontFaces.addReturnIterator(fontFace);
        if (m_firstNonCssConnectedFace == m_fontFaces.end())
            m_firstNonCssConnectedFace = iterator;
    }
}

void CSSSegmentedFontFace::removeFontFace(FontFace* fontFace)
{
    FontFaceList::iterator it = m_fontFaces.find(fontFace);
    if (it == m_fontFaces.end())
        return;

    if (it == m_firstNonCssConnectedFace)
        ++m_firstNonCssConnectedFace;
    m_fontFaces.remove(it);

    pruneTable();
    fontFace->cssFontFace()->clearSegmentedFontFace();
}

PassRefPtr<FontData> CSSSegmentedFontFace::getFontData(const FontDescription& fontDescription)
{
    if (!isValid())
        return nullptr;

    FontTraits desiredTraits = fontDescription.traits();
    FontCacheKey key = fontDescription.cacheKey(FontFaceCreationParams(), desiredTraits);

    RefPtr<SegmentedFontData>& fontData = m_fontDataTable.add(key, nullptr).storedValue->value;
    if (fontData && fontData->numFaces())
        return fontData; // No release, we have a reference to an object in the cache which should retain the ref count it has.

    if (!fontData)
        fontData = SegmentedFontData::create();

    FontDescription requestedFontDescription(fontDescription);
    requestedFontDescription.setTraits(m_traits);
    requestedFontDescription.setSyntheticBold(m_traits.weight() < FontWeight600 && desiredTraits.weight() >= FontWeight600);
    requestedFontDescription.setSyntheticItalic(m_traits.style() == FontStyleNormal && desiredTraits.style() == FontStyleItalic);

    for (FontFaceList::reverse_iterator it = m_fontFaces.rbegin(); it != m_fontFaces.rend(); ++it) {
        if (!(*it)->cssFontFace()->isValid())
            continue;
        if (RefPtr<SimpleFontData> faceFontData = (*it)->cssFontFace()->getFontData(requestedFontDescription)) {
            ASSERT(!faceFontData->isSegmented());
            if (faceFontData->isCustomFont())
                fontData->appendFace(adoptRef(new FontDataForRangeSet(faceFontData.release(), (*it)->cssFontFace()->ranges())));
            else
                fontData->appendFace(adoptRef(new FontDataForRangeSetFromCache(faceFontData.release(), (*it)->cssFontFace()->ranges())));
        }
    }
    if (fontData->numFaces())
        return fontData; // No release, we have a reference to an object in the cache which should retain the ref count it has.

    return nullptr;
}

void CSSSegmentedFontFace::willUseFontData(const FontDescription& fontDescription, const String& text)
{
    m_approximateCharacterCount += text.length();
    for (FontFaceList::reverse_iterator it = m_fontFaces.rbegin(); it != m_fontFaces.rend(); ++it) {
        if ((*it)->loadStatus() != FontFace::Unloaded)
            break;
        if ((*it)->cssFontFace()->maybeLoadFont(fontDescription, text))
            break;
    }
}

void CSSSegmentedFontFace::willUseRange(const blink::FontDescription& fontDescription, const blink::FontDataForRangeSet& rangeSet)
{
    // Iterating backwards since later defined unicode-range faces override
    // previously defined ones, according to the CSS3 fonts module.
    // https://drafts.csswg.org/css-fonts/#composite-fonts
    for (FontFaceList::reverse_iterator it = m_fontFaces.rbegin(); it != m_fontFaces.rend(); ++it) {
        CSSFontFace* cssFontFace = (*it)->cssFontFace();
        if (cssFontFace->maybeLoadFont(fontDescription, rangeSet))
            break;
    }
}

bool CSSSegmentedFontFace::checkFont(const String& text) const
{
    for (const auto& fontFace : m_fontFaces) {
        if (fontFace->loadStatus() != FontFace::Loaded && fontFace->cssFontFace()->ranges()->intersectsWith(text))
            return false;
    }
    return true;
}

void CSSSegmentedFontFace::match(const String& text, HeapVector<Member<FontFace>>& faces) const
{
    for (const auto& fontFace : m_fontFaces) {
        if (fontFace->cssFontFace()->ranges()->intersectsWith(text))
            faces.append(fontFace);
    }
}

DEFINE_TRACE(CSSSegmentedFontFace)
{
    visitor->trace(m_fontSelector);
    visitor->trace(m_fontFaces);
}

} // namespace blink
