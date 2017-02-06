// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "wtf/RetainPtr.h"
#include "wtf/text/StringView.h"

namespace blink {

class HyphenationCF : public Hyphenation {
public:
    HyphenationCF(RetainPtr<CFLocaleRef>& localeCF)
        : m_localeCF(localeCF)
    {
        DCHECK(m_localeCF);
    }

    size_t lastHyphenLocation(const StringView& text, size_t beforeIndex) const override
    {
        CFIndex result = CFStringGetHyphenationLocationBeforeIndex(
            text.toString().impl()->createCFString().get(), beforeIndex,
            CFRangeMake(0, text.length()), 0, m_localeCF.get(), 0);
        return result == kCFNotFound ? 0 : result;
    }

private:
    RetainPtr<CFLocaleRef> m_localeCF;
};

PassRefPtr<Hyphenation> Hyphenation::platformGetHyphenation(const AtomicString& locale)
{
    RetainPtr<CFStringRef> localeCFString = locale.impl()->createCFString();
    RetainPtr<CFLocaleRef> localeCF = adoptCF(CFLocaleCreate(kCFAllocatorDefault, localeCFString.get()));
    if (!CFStringIsHyphenationAvailableForLocale(localeCF.get()))
        return nullptr;
    return adoptRef(new HyphenationCF(localeCF));
}

} // namespace blink
