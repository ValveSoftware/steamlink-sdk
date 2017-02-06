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

#include "core/editing/iterators/CharacterIterator.h"

namespace blink {

template <typename Strategy>
CharacterIteratorAlgorithm<Strategy>::CharacterIteratorAlgorithm(const PositionTemplate<Strategy>& start, const PositionTemplate<Strategy>& end, TextIteratorBehaviorFlags behavior)
    : m_offset(0)
    , m_runOffset(0)
    , m_atBreak(true)
    , m_textIterator(start, end, behavior)
{
    initialize();
}

template <typename Strategy>
CharacterIteratorAlgorithm<Strategy>::CharacterIteratorAlgorithm(const EphemeralRangeTemplate<Strategy>& range, TextIteratorBehaviorFlags behavior)
    : CharacterIteratorAlgorithm(range.startPosition(), range.endPosition(), behavior)
{
}

template <typename Strategy>
void CharacterIteratorAlgorithm<Strategy>::initialize()
{
    while (!atEnd() && !m_textIterator.length())
        m_textIterator.advance();
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy> CharacterIteratorAlgorithm<Strategy>::range() const
{
    EphemeralRangeTemplate<Strategy> range(m_textIterator.range());
    if (m_textIterator.atEnd() || m_textIterator.length() <= 1)
        return range;
    PositionTemplate<Strategy> startPosition = range.startPosition().parentAnchoredEquivalent();
    PositionTemplate<Strategy> endPosition = range.endPosition().parentAnchoredEquivalent();
    Node* node = startPosition.computeContainerNode();
    ASSERT_UNUSED(endPosition, node == endPosition.computeContainerNode());
    int offset = startPosition.offsetInContainerNode() + m_runOffset;
    return EphemeralRangeTemplate<Strategy>(PositionTemplate<Strategy>(node, offset), PositionTemplate<Strategy>(node, offset + 1));
}

template <typename Strategy>
Document* CharacterIteratorAlgorithm<Strategy>::ownerDocument() const
{
    return m_textIterator.ownerDocument();
}

template <typename Strategy>
Node* CharacterIteratorAlgorithm<Strategy>::currentContainer() const
{
    return m_textIterator.currentContainer();
}

template <typename Strategy>
int CharacterIteratorAlgorithm<Strategy>::startOffset() const
{
    if (!m_textIterator.atEnd()) {
        if (m_textIterator.length() > 1)
            return m_textIterator.startOffsetInCurrentContainer() + m_runOffset;
        DCHECK(!m_runOffset);
    }
    return m_textIterator.startOffsetInCurrentContainer();
}

template <typename Strategy>
int CharacterIteratorAlgorithm<Strategy>::endOffset() const
{
    if (!m_textIterator.atEnd()) {
        if (m_textIterator.length() > 1)
            return m_textIterator.startOffsetInCurrentContainer() + m_runOffset + 1;
        DCHECK(!m_runOffset);
    }
    return m_textIterator.endOffsetInCurrentContainer();
}

template <typename Strategy>
PositionTemplate<Strategy> CharacterIteratorAlgorithm<Strategy>::startPosition() const
{
    if (!m_textIterator.atEnd()) {
        if (m_textIterator.length() > 1) {
            Node* n = m_textIterator.currentContainer();
            int offset = m_textIterator.startOffsetInCurrentContainer() + m_runOffset;
            return PositionTemplate<Strategy>::editingPositionOf(n, offset);
        }
        DCHECK(!m_runOffset);
    }
    return m_textIterator.startPositionInCurrentContainer();
}

template <typename Strategy>
PositionTemplate<Strategy> CharacterIteratorAlgorithm<Strategy>::endPosition() const
{
    if (!m_textIterator.atEnd()) {
        if (m_textIterator.length() > 1) {
            Node* n = m_textIterator.currentContainer();
            int offset = m_textIterator.startOffsetInCurrentContainer() + m_runOffset;
            return PositionTemplate<Strategy>::editingPositionOf(n, offset + 1);
        }
        DCHECK(!m_runOffset);
    }
    return m_textIterator.endPositionInCurrentContainer();
}

template <typename Strategy>
void CharacterIteratorAlgorithm<Strategy>::advance(int count)
{
    if (count <= 0) {
        DCHECK(!count);
        return;
    }

    m_atBreak = false;

    // easy if there is enough left in the current m_textIterator run
    int remaining = m_textIterator.length() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    // exhaust the current m_textIterator run
    count -= remaining;
    m_offset += remaining;

    // move to a subsequent m_textIterator run
    for (m_textIterator.advance(); !atEnd(); m_textIterator.advance()) {
        int runLength = m_textIterator.length();
        if (!runLength) {
            m_atBreak = m_textIterator.breaksAtReplacedElement();
        } else {
            // see whether this is m_textIterator to use
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }

            // exhaust this m_textIterator run
            count -= runLength;
            m_offset += runLength;
        }
    }

    // ran to the end of the m_textIterator... no more runs left
    m_atBreak = true;
    m_runOffset = 0;
}

template <typename Strategy>
void CharacterIteratorAlgorithm<Strategy>::copyTextTo(ForwardsTextBuffer* output)
{
    m_textIterator.copyTextTo(output, m_runOffset);
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy> CharacterIteratorAlgorithm<Strategy>::calculateCharacterSubrange(int offset, int length)
{
    advance(offset);
    const PositionTemplate<Strategy> startPos = startPosition();

    if (length > 1)
        advance(length - 1);
    return EphemeralRangeTemplate<Strategy>(startPos, endPosition());
}

EphemeralRange calculateCharacterSubrange(const EphemeralRange& range, int characterOffset, int characterCount)
{
    CharacterIterator entireRangeIterator(range, TextIteratorEmitsObjectReplacementCharacter);
    return entireRangeIterator.calculateCharacterSubrange(characterOffset, characterCount);
}

template class CORE_TEMPLATE_EXPORT CharacterIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT CharacterIteratorAlgorithm<EditingInFlatTreeStrategy>;

} // namespace blink
