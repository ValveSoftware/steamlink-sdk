// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SizesAttributeParser_h
#define SizesAttributeParser_h

#include "core/css/MediaValues.h"
#include "core/css/parser/MediaQueryParser.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class SizesAttributeParser {
    STACK_ALLOCATED();
public:
    static unsigned findEffectiveSize(const String& attribute, PassRefPtr<MediaValues>);

private:
    SizesAttributeParser(PassRefPtr<MediaValues> mediaValues)
        : m_mediaValues(mediaValues)
        , m_length(0)
        , m_lengthWasSet(false)
    {
    }

    bool parse(Vector<MediaQueryToken>& tokens);
    bool parseMediaConditionAndLength(MediaQueryTokenIterator startToken, MediaQueryTokenIterator endToken);
    unsigned effectiveSize();
    bool calculateLengthInPixels(MediaQueryTokenIterator startToken, MediaQueryTokenIterator endToken, unsigned& result);
    bool mediaConditionMatches(PassRefPtrWillBeRawPtr<MediaQuerySet> mediaCondition);
    unsigned effectiveSizeDefaultValue();

    RefPtrWillBeMember<MediaQuerySet> m_mediaCondition;
    RefPtr<MediaValues> m_mediaValues;
    unsigned m_length;
    bool m_lengthWasSet;
};

} // namespace

#endif

