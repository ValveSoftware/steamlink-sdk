// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Hyphenation_h
#define Hyphenation_h

#include "platform/PlatformExport.h"
#include "platform/fonts/Font.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

namespace blink {

class PLATFORM_EXPORT Hyphenation : public RefCounted<Hyphenation> {
public:
    virtual ~Hyphenation() {}

    static Hyphenation* get(const AtomicString& locale);

    virtual size_t lastHyphenLocation(const StringView&, size_t beforeIndex) const = 0;
    virtual Vector<size_t, 8> hyphenLocations(const StringView&) const;

    static const unsigned minimumPrefixLength = 2;
    static const unsigned minimumSuffixLength = 2;
    static int minimumPrefixWidth(const Font&);

    static void setForTesting(const AtomicString& locale, PassRefPtr<Hyphenation>);
    static void clearForTesting();

private:
    using HyphenationMap = HashMap<AtomicString, RefPtr<Hyphenation>, CaseFoldingHash>;

    static HyphenationMap& getHyphenationMap();
    static PassRefPtr<Hyphenation> platformGetHyphenation(const AtomicString& locale);
};

inline int Hyphenation::minimumPrefixWidth(const Font& font)
{
    // If the maximum width available for the prefix before the hyphen is small, then it is very unlikely
    // that an hyphenation opportunity exists, so do not bother to look for it.
    // These are heuristic numbers for performance added in http://wkb.ug/45606
    const int minimumPrefixWidthNumerator = 5;
    const int minimumPrefixWidthDenominator = 4;
    return font.getFontDescription().computedPixelSize()
        * minimumPrefixWidthNumerator / minimumPrefixWidthDenominator;
}

} // namespace blink

#endif // Hyphenation_h
