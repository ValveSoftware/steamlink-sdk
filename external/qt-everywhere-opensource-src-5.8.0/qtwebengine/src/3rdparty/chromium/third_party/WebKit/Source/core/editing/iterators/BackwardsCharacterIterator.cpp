/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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

#include "core/editing/iterators/BackwardsCharacterIterator.h"

namespace blink {

template <typename Strategy>
BackwardsCharacterIteratorAlgorithm<Strategy>::BackwardsCharacterIteratorAlgorithm(const PositionTemplate<Strategy>& start, const PositionTemplate<Strategy>& end, TextIteratorBehaviorFlags behavior)
    : m_offset(0)
    , m_runOffset(0)
    , m_atBreak(true)
    , m_textIterator(start, end, behavior)
{
    while (!atEnd() && !m_textIterator.length())
        m_textIterator.advance();
}

template <typename Strategy>
PositionTemplate<Strategy> BackwardsCharacterIteratorAlgorithm<Strategy>::endPosition() const
{
    if (!m_textIterator.atEnd()) {
        if (m_textIterator.length() > 1) {
            Node* n = m_textIterator.startContainer();
            return PositionTemplate<Strategy>::editingPositionOf(n, m_textIterator.endOffset() - m_runOffset);
        }
        DCHECK(!m_runOffset);
    }
    return m_textIterator.endPosition();
}

template <typename Strategy>
void BackwardsCharacterIteratorAlgorithm<Strategy>::advance(int count)
{
    if (count <= 0) {
        DCHECK(!count);
        return;
    }

    m_atBreak = false;

    int remaining = m_textIterator.length() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    count -= remaining;
    m_offset += remaining;

    for (m_textIterator.advance(); !atEnd(); m_textIterator.advance()) {
        int runLength = m_textIterator.length();
        if (!runLength) {
            m_atBreak = true;
        } else {
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }

            count -= runLength;
            m_offset += runLength;
        }
    }

    m_atBreak = true;
    m_runOffset = 0;
}

template class CORE_TEMPLATE_EXPORT BackwardsCharacterIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT BackwardsCharacterIteratorAlgorithm<EditingInFlatTreeStrategy>;

} // namespace blink
