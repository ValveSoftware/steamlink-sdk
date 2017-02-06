// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "wtf/text/StringView.h"

namespace blink {

Hyphenation::HyphenationMap& Hyphenation::getHyphenationMap()
{
    DEFINE_STATIC_LOCAL(HyphenationMap, hyphenationMap, ());
    return hyphenationMap;
}

Hyphenation* Hyphenation::get(const AtomicString& locale)
{
    DCHECK(!locale.isNull());
    Hyphenation::HyphenationMap& hyphenationMap = getHyphenationMap();
    auto result = hyphenationMap.add(locale, nullptr);
    if (result.isNewEntry)
        result.storedValue->value = platformGetHyphenation(locale);
    return result.storedValue->value.get();
}

void Hyphenation::setForTesting(const AtomicString& locale, PassRefPtr<Hyphenation> hyphenation)
{
    getHyphenationMap().set(locale, hyphenation);
}

void Hyphenation::clearForTesting()
{
    getHyphenationMap().clear();
}

Vector<size_t, 8> Hyphenation::hyphenLocations(const StringView& text) const
{
    Vector<size_t, 8> hyphenLocations;
    size_t hyphenLocation = text.length();
    if (hyphenLocation <= minimumSuffixLength)
        return hyphenLocations;
    hyphenLocation -= minimumSuffixLength;

    while ((hyphenLocation = lastHyphenLocation(text, hyphenLocation)) >= minimumPrefixLength)
        hyphenLocations.append(hyphenLocation);

    return hyphenLocations;
}

} // namespace blink
