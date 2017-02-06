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

#include "core/editing/PlainTextRange.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIterator.h"

namespace blink {

PlainTextRange::PlainTextRange()
    : m_start(kNotFound)
    , m_end(kNotFound)
{
}

PlainTextRange::PlainTextRange(int location)
    : m_start(location)
    , m_end(location)
{
    DCHECK_GE(location, 0);
}

PlainTextRange::PlainTextRange(int start, int end)
    : m_start(start)
    , m_end(end)
{
    DCHECK_GE(start, 0);
    DCHECK_GE(end, 0);
    DCHECK_LE(start, end);
}

EphemeralRange PlainTextRange::createRange(const ContainerNode& scope) const
{
    return createRangeFor(scope, ForGeneric);
}

EphemeralRange PlainTextRange::createRangeForSelection(const ContainerNode& scope) const
{
    return createRangeFor(scope, ForSelection);
}

EphemeralRange PlainTextRange::createRangeFor(const ContainerNode& scope, GetRangeFor getRangeFor) const
{
    DCHECK(isNotNull());

    size_t docTextPosition = 0;
    bool startRangeFound = false;

    Position textRunStartPosition;
    Position textRunEndPosition;

    TextIteratorBehaviorFlags behaviorFlags = TextIteratorEmitsObjectReplacementCharacter;
    if (getRangeFor == ForSelection)
        behaviorFlags |= TextIteratorEmitsCharactersBetweenAllVisiblePositions;
    auto range = EphemeralRange::rangeOfContents(scope);

    // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
    // see http://crbug.com/590369 for more details.
    scope.document().updateStyleAndLayoutIgnorePendingStylesheets();

    TextIterator it(range.startPosition(), range.endPosition(), behaviorFlags);

    // FIXME: the atEnd() check shouldn't be necessary, workaround for
    // <http://bugs.webkit.org/show_bug.cgi?id=6289>.
    if (!start() && !length() && it.atEnd())
        return EphemeralRange(Position(it.currentContainer(), 0));

    Position resultStart = Position(&scope.document(), 0);
    Position resultEnd = resultStart;

    for (; !it.atEnd(); it.advance()) {
        int len = it.length();

        textRunStartPosition = it.startPositionInCurrentContainer();
        textRunEndPosition = it.endPositionInCurrentContainer();

        bool foundStart = start() >= docTextPosition && start() <= docTextPosition + len;
        bool foundEnd = end() >= docTextPosition && end() <= docTextPosition + len;

        // Fix textRunRange->endPosition(), but only if foundStart || foundEnd, because it is only
        // in those cases that textRunRange is used.
        if (foundEnd) {
            // FIXME: This is a workaround for the fact that the end of a run
            // is often at the wrong position for emitted '\n's or if the
            // layoutObject of the current node is a replaced element.
            if (len == 1 && (it.characterAt(0) == '\n' || it.isInsideAtomicInlineElement())) {
                it.advance();
                if (!it.atEnd()) {
                    textRunEndPosition = it.startPositionInCurrentContainer();
                } else {
                    Position runEnd = nextPositionOf(createVisiblePosition(textRunStartPosition)).deepEquivalent();
                    if (runEnd.isNotNull())
                        textRunEndPosition = runEnd;
                }
            }
        }

        if (foundStart) {
            startRangeFound = true;
            if (textRunStartPosition.computeContainerNode()->isTextNode()) {
                int offset = start() - docTextPosition;
                resultStart = Position(textRunStartPosition.computeContainerNode(), offset + textRunStartPosition.offsetInContainerNode());
            } else {
                if (start() == docTextPosition)
                    resultStart = textRunStartPosition;
                else
                    resultStart = textRunEndPosition;
            }
        }

        if (foundEnd) {
            if (textRunStartPosition.computeContainerNode()->isTextNode()) {
                int offset = end() - docTextPosition;
                resultEnd = Position(textRunStartPosition.computeContainerNode(), offset + textRunStartPosition.offsetInContainerNode());
            } else {
                if (end() == docTextPosition)
                    resultEnd = textRunStartPosition;
                else
                    resultEnd = textRunEndPosition;
            }
            docTextPosition += len;
            break;
        }
        docTextPosition += len;
    }

    if (!startRangeFound)
        return EphemeralRange();

    if (length() && end() > docTextPosition) { // end() is out of bounds
        resultEnd = textRunEndPosition;
    }

    return EphemeralRange(resultStart.toOffsetInAnchor(), resultEnd.toOffsetInAnchor());
}

PlainTextRange PlainTextRange::create(const ContainerNode& scope, const EphemeralRange& range)
{
    if (range.isNull())
        return PlainTextRange();

    // The critical assumption is that this only gets called with ranges that
    // concentrate on a given area containing the selection root. This is done
    // because of text fields and textareas. The DOM for those is not
    // directly in the document DOM, so ensure that the range does not cross a
    // boundary of one of those.
    Node* startContainer = range.startPosition().computeContainerNode();
    if (startContainer != &scope && !startContainer->isDescendantOf(&scope))
        return PlainTextRange();
    Node* endContainer = range.endPosition().computeContainerNode();
    if (endContainer != scope && !endContainer->isDescendantOf(&scope))
        return PlainTextRange();

    size_t start = TextIterator::rangeLength(Position(&const_cast<ContainerNode&>(scope), 0), range.startPosition());
    size_t end = TextIterator::rangeLength(Position(&const_cast<ContainerNode&>(scope), 0), range.endPosition());

    return PlainTextRange(start, end);
}

PlainTextRange PlainTextRange::create(const ContainerNode& scope, const Range& range)
{
    return create(scope, EphemeralRange(&range));
}

} // namespace blink
