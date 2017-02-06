/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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


#include "core/css/FontFaceCache.h"

#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/CSSValueList.h"
#include "core/css/FontFace.h"
#include "core/css/FontStyleMatcher.h"
#include "core/css/StyleRule.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/FontFamilyNames.h"
#include "platform/fonts/FontDescription.h"
#include "wtf/text/AtomicString.h"


namespace blink {

FontFaceCache::FontFaceCache()
    : m_version(0)
{
}

void FontFaceCache::add(CSSFontSelector* cssFontSelector, const StyleRuleFontFace* fontFaceRule, FontFace* fontFace)
{
    if (!m_styleRuleToFontFace.add(fontFaceRule, fontFace).isNewEntry)
        return;
    addFontFace(cssFontSelector, fontFace, true);
}

void FontFaceCache::addFontFace(CSSFontSelector* cssFontSelector, FontFace* fontFace, bool cssConnected)
{
    FamilyToTraitsMap::AddResult traitsResult = m_fontFaces.add(fontFace->family(), nullptr);
    if (!traitsResult.storedValue->value)
        traitsResult.storedValue->value = new TraitsMap;

    TraitsMap::AddResult segmentedFontFaceResult = traitsResult.storedValue->value->add(fontFace->traits().bitfield(), nullptr);
    if (!segmentedFontFaceResult.storedValue->value)
        segmentedFontFaceResult.storedValue->value = CSSSegmentedFontFace::create(cssFontSelector, fontFace->traits());

    segmentedFontFaceResult.storedValue->value->addFontFace(fontFace, cssConnected);
    if (cssConnected)
        m_cssConnectedFontFaces.add(fontFace);

    m_fonts.remove(fontFace->family());
    ++m_version;
}

void FontFaceCache::remove(const StyleRuleFontFace* fontFaceRule)
{
    StyleRuleToFontFace::iterator it = m_styleRuleToFontFace.find(fontFaceRule);
    if (it != m_styleRuleToFontFace.end()) {
        removeFontFace(it->value.get(), true);
        m_styleRuleToFontFace.remove(it);
    }
}

void FontFaceCache::removeFontFace(FontFace* fontFace, bool cssConnected)
{
    FamilyToTraitsMap::iterator fontFacesIter = m_fontFaces.find(fontFace->family());
    if (fontFacesIter == m_fontFaces.end())
        return;
    TraitsMap* familyFontFaces = fontFacesIter->value.get();

    TraitsMap::iterator familyFontFacesIter = familyFontFaces->find(fontFace->traits().bitfield());
    if (familyFontFacesIter == familyFontFaces->end())
        return;
    CSSSegmentedFontFace* segmentedFontFace = familyFontFacesIter->value;

    segmentedFontFace->removeFontFace(fontFace);
    if (segmentedFontFace->isEmpty()) {
        familyFontFaces->remove(familyFontFacesIter);
        if (familyFontFaces->isEmpty())
            m_fontFaces.remove(fontFacesIter);
    }
    m_fonts.remove(fontFace->family());
    if (cssConnected)
        m_cssConnectedFontFaces.remove(fontFace);

    ++m_version;
}

void FontFaceCache::clearCSSConnected()
{
    for (const auto& item : m_styleRuleToFontFace)
        removeFontFace(item.value.get(), true);
    m_styleRuleToFontFace.clear();
}

void FontFaceCache::clearAll()
{
    if (m_fontFaces.isEmpty())
        return;

    m_fontFaces.clear();
    m_fonts.clear();
    m_styleRuleToFontFace.clear();
    m_cssConnectedFontFaces.clear();
    ++m_version;
}

CSSSegmentedFontFace* FontFaceCache::get(const FontDescription& fontDescription, const AtomicString& family)
{
    if (family.isEmpty())
        return nullptr;

    TraitsMap* familyFontFaces = m_fontFaces.get(family);
    if (!familyFontFaces || familyFontFaces->isEmpty())
        return nullptr;

    FamilyToTraitsMap::AddResult traitsResult = m_fonts.add(family, nullptr);
    if (!traitsResult.storedValue->value)
        traitsResult.storedValue->value = new TraitsMap;

    FontTraits traits = fontDescription.traits();
    TraitsMap::AddResult faceResult = traitsResult.storedValue->value->add(traits.bitfield(), nullptr);
    if (!faceResult.storedValue->value) {
        for (const auto& item : *familyFontFaces) {
            CSSSegmentedFontFace* candidate = item.value.get();
            FontStyleMatcher styleMatcher(traits);
            if (!faceResult.storedValue->value || styleMatcher.isCandidateBetter(candidate, faceResult.storedValue->value.get()))
                faceResult.storedValue->value = candidate;
        }
    }
    return faceResult.storedValue->value.get();
}

DEFINE_TRACE(FontFaceCache)
{
    visitor->trace(m_fontFaces);
    visitor->trace(m_fonts);
    visitor->trace(m_styleRuleToFontFace);
    visitor->trace(m_cssConnectedFontFaces);
}

} // namespace blink
