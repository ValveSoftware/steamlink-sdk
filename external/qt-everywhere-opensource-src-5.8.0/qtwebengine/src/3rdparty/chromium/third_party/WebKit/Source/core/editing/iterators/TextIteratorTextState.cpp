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

#include "core/editing/iterators/TextIteratorTextState.h"

#include "core/layout/LayoutText.h"

namespace blink {

TextIteratorTextState::TextIteratorTextState(bool emitsOriginalText)
    : m_textLength(0)
    , m_singleCharacterBuffer(0)
    , m_positionNode(nullptr)
    , m_positionStartOffset(0)
    , m_positionEndOffset(0)
    , m_hasEmitted(false)
    , m_lastCharacter(0)
    , m_emitsOriginalText(emitsOriginalText)
    { }

UChar TextIteratorTextState::characterAt(unsigned index) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < static_cast<unsigned>(length()));
    if (!(index < static_cast<unsigned>(length())))
        return 0;

    if (m_singleCharacterBuffer) {
        DCHECK_EQ(index, 0u);
        DCHECK_EQ(length(), 1);
        return m_singleCharacterBuffer;
    }

    return string()[positionStartOffset() + index];
}

String TextIteratorTextState::substring(unsigned position, unsigned length) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(position <= static_cast<unsigned>(this->length()));
    ASSERT_WITH_SECURITY_IMPLICATION(position + length <= static_cast<unsigned>(this->length()));
    if (!length)
        return emptyString();
    if (m_singleCharacterBuffer) {
        DCHECK_EQ(position, 0u);
        DCHECK_EQ(length, 1u);
        return String(&m_singleCharacterBuffer, 1);
    }
    return string().substring(positionStartOffset() + position, length);
}

void TextIteratorTextState::appendTextToStringBuilder(StringBuilder& builder, unsigned position, unsigned maxLength) const
{
    unsigned lengthToAppend = std::min(static_cast<unsigned>(length()) - position, maxLength);
    if (!lengthToAppend)
        return;
    if (m_singleCharacterBuffer) {
        DCHECK_EQ(position, 0u);
        builder.append(m_singleCharacterBuffer);
    } else {
        builder.append(string(), positionStartOffset() + position, lengthToAppend);
    }
}

void TextIteratorTextState::updateForReplacedElement(Node* baseNode)
{
    m_hasEmitted = true;
    m_positionNode = baseNode->parentNode();
    m_positionOffsetBaseNode = baseNode;
    m_positionStartOffset = 0;
    m_positionEndOffset = 1;
    m_singleCharacterBuffer = 0;

    m_textLength = 0;
    m_lastCharacter = 0;
}

void TextIteratorTextState::emitAltText(Node* node)
{
    m_text = toHTMLElement(node)->altText();
    m_textLength = m_text.length();
    m_lastCharacter = m_textLength ? m_text[m_textLength - 1] : 0;
}

void TextIteratorTextState::flushPositionOffsets() const
{
    if (!m_positionOffsetBaseNode)
        return;
    int index = m_positionOffsetBaseNode->nodeIndex();
    m_positionStartOffset += index;
    m_positionEndOffset += index;
    m_positionOffsetBaseNode = nullptr;
}

void TextIteratorTextState::spliceBuffer(UChar c, Node* textNode, Node* offsetBaseNode, int textStartOffset, int textEndOffset)
{
    DCHECK(textNode);
    m_hasEmitted = true;

    // Remember information with which to construct the TextIterator::range().
    // NOTE: textNode is often not a text node, so the range will specify child nodes of positionNode
    m_positionNode = textNode;
    m_positionOffsetBaseNode = offsetBaseNode;
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;

    // remember information with which to construct the TextIterator::characters() and length()
    m_singleCharacterBuffer = c;
    DCHECK(m_singleCharacterBuffer);
    m_textLength = 1;

    // remember some iteration state
    m_lastCharacter = c;
}

void TextIteratorTextState::emitText(Node* textNode, LayoutText* layoutObject, int textStartOffset, int textEndOffset)
{
    DCHECK(textNode);
    m_text = m_emitsOriginalText ? layoutObject->originalText() : layoutObject->text();
    DCHECK(!m_text.isEmpty());
    DCHECK_LE(0, textStartOffset);
    DCHECK_LT(textStartOffset, static_cast<int>(m_text.length()));
    DCHECK_LE(0, textEndOffset);
    DCHECK_LE(textEndOffset, static_cast<int>(m_text.length()));
    DCHECK_LE(textStartOffset, textEndOffset);

    m_positionNode = textNode;
    m_positionOffsetBaseNode = nullptr;
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;
    m_singleCharacterBuffer = 0;
    m_textLength = textEndOffset - textStartOffset;
    m_lastCharacter = m_text[textEndOffset - 1];

    m_hasEmitted = true;
}

void TextIteratorTextState::appendTextTo(ForwardsTextBuffer* output, unsigned position, unsigned lengthToAppend) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(position + lengthToAppend <= static_cast<unsigned>(length()));
    // Make sure there's no integer overflow.
    ASSERT_WITH_SECURITY_IMPLICATION(position + lengthToAppend >= position);
    if (!lengthToAppend)
        return;
    DCHECK(output);
    if (m_singleCharacterBuffer) {
        DCHECK_EQ(position, 0u);
        DCHECK_EQ(length(), 1);
        output->pushCharacters(m_singleCharacterBuffer, 1);
        return;
    }
    if (positionNode()) {
        flushPositionOffsets();
        unsigned offset = positionStartOffset() + position;
        if (string().is8Bit())
            output->pushRange(string().characters8() + offset, lengthToAppend);
        else
            output->pushRange(string().characters16() + offset, lengthToAppend);
        return;
    }
    NOTREACHED(); // "We shouldn't be attempting to append text that doesn't exist.";
}

} // namespace blink
