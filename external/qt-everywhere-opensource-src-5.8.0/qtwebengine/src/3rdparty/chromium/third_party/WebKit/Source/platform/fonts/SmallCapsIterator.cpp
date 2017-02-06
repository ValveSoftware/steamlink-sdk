// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SmallCapsIterator.h"

#include "wtf/PtrUtil.h"
#include <unicode/utypes.h>

namespace blink {

SmallCapsIterator::SmallCapsIterator(const UChar* buffer, unsigned bufferSize)
    : m_utf16Iterator(wrapUnique(new UTF16TextIterator(buffer, bufferSize)))
    , m_bufferSize(bufferSize)
    , m_nextUChar32(0)
    , m_atEnd(bufferSize == 0)
    , m_currentSmallCapsBehavior(SmallCapsInvalid)
{
}

bool SmallCapsIterator::consume(unsigned *capsLimit, SmallCapsBehavior* smallCapsBehavior)
{
    if (m_atEnd)
        return false;

    while (m_utf16Iterator->consume(m_nextUChar32)) {
        m_previousSmallCapsBehavior = m_currentSmallCapsBehavior;
        // Skipping over combining marks, as these combine with the small-caps
        // uppercased text as well and we do not need to split by their
        // individual case-ness.
        if (!u_getCombiningClass(m_nextUChar32)) {
            m_currentSmallCapsBehavior = u_hasBinaryProperty(m_nextUChar32, UCHAR_CHANGES_WHEN_UPPERCASED)
                ? SmallCapsUppercaseNeeded
                : SmallCapsSameCase;
        }

        if (m_previousSmallCapsBehavior != m_currentSmallCapsBehavior
            && m_previousSmallCapsBehavior != SmallCapsInvalid) {
            *capsLimit = m_utf16Iterator->offset();
            *smallCapsBehavior = m_previousSmallCapsBehavior;
            return true;
        }
        m_utf16Iterator->advance();
    }
    *capsLimit = m_bufferSize;
    *smallCapsBehavior = m_currentSmallCapsBehavior;
    m_atEnd = true;
    return true;
}

} // namespace blink
