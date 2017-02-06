/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/line/AbstractInlineTextBox.h"

#include "core/dom/AXObjectCache.h"
#include "core/editing/iterators/TextIterator.h"
#include "platform/text/TextBreakIterator.h"

namespace blink {

AbstractInlineTextBox::InlineToAbstractInlineTextBoxHashMap* AbstractInlineTextBox::gAbstractInlineTextBoxMap = nullptr;

PassRefPtr<AbstractInlineTextBox> AbstractInlineTextBox::getOrCreate(LineLayoutText lineLayoutText, InlineTextBox* inlineTextBox)
{
    if (!inlineTextBox)
        return nullptr;

    if (!gAbstractInlineTextBoxMap)
        gAbstractInlineTextBoxMap = new InlineToAbstractInlineTextBoxHashMap();

    InlineToAbstractInlineTextBoxHashMap::const_iterator it = gAbstractInlineTextBoxMap->find(inlineTextBox);
    if (it != gAbstractInlineTextBoxMap->end())
        return it->value;

    RefPtr<AbstractInlineTextBox> obj = adoptRef(new AbstractInlineTextBox(lineLayoutText, inlineTextBox));
    gAbstractInlineTextBoxMap->set(inlineTextBox, obj);
    return obj;
}

void AbstractInlineTextBox::willDestroy(InlineTextBox* inlineTextBox)
{
    if (!gAbstractInlineTextBoxMap)
        return;

    InlineToAbstractInlineTextBoxHashMap::const_iterator it = gAbstractInlineTextBoxMap->find(inlineTextBox);
    if (it != gAbstractInlineTextBoxMap->end()) {
        it->value->detach();
        gAbstractInlineTextBoxMap->remove(inlineTextBox);
    }
}

AbstractInlineTextBox::~AbstractInlineTextBox()
{
    ASSERT(!m_lineLayoutItem);
    ASSERT(!m_inlineTextBox);
}

void AbstractInlineTextBox::detach()
{
    if (Node* node = m_lineLayoutItem.node()) {
        if (AXObjectCache* cache = node->document().existingAXObjectCache())
            cache->remove(this);
    }

    m_lineLayoutItem = LineLayoutText(nullptr);
    m_inlineTextBox = nullptr;
}

PassRefPtr<AbstractInlineTextBox> AbstractInlineTextBox::nextInlineTextBox() const
{
    ASSERT(!m_inlineTextBox || !m_inlineTextBox->getLineLayoutItem().needsLayout());
    if (!m_inlineTextBox)
        return nullptr;

    return getOrCreate(m_lineLayoutItem, m_inlineTextBox->nextTextBox());
}

LayoutRect AbstractInlineTextBox::bounds() const
{
    if (!m_inlineTextBox || !m_lineLayoutItem)
        return LayoutRect();

    FloatRect boundaries(m_inlineTextBox->calculateBoundaries());
    return LayoutRect(m_lineLayoutItem.localToAbsoluteQuad(boundaries).enclosingBoundingBox());
}

unsigned AbstractInlineTextBox::len() const
{
    if (!m_inlineTextBox)
        return 0;

    return m_inlineTextBox->len();
}

AbstractInlineTextBox::Direction AbstractInlineTextBox::getDirection() const
{
    if (!m_inlineTextBox || !m_lineLayoutItem)
        return LeftToRight;

    if (m_lineLayoutItem.style()->isHorizontalWritingMode())
        return (m_inlineTextBox->direction() == RTL ? RightToLeft : LeftToRight);
    return (m_inlineTextBox->direction() == RTL ? BottomToTop : TopToBottom);
}

void AbstractInlineTextBox::characterWidths(Vector<float>& widths) const
{
    if (!m_inlineTextBox)
        return;

    m_inlineTextBox->characterWidths(widths);
}

void AbstractInlineTextBox::wordBoundaries(Vector<WordBoundaries>& words) const
{
    if (!m_inlineTextBox)
        return;

    String text = this->text();
    int len = text.length();
    TextBreakIterator* iterator = wordBreakIterator(text, 0, len);

    // FIXME: When http://crbug.com/411764 is fixed, replace this with an ASSERT.
    if (!iterator)
        return;

    int pos = iterator->first();
    while (pos >= 0 && pos < len) {
        int next = iterator->next();
        if (isWordTextBreak(iterator))
            words.append(WordBoundaries(pos, next));
        pos = next;
    }
}

String AbstractInlineTextBox::text() const
{
    if (!m_inlineTextBox || !m_lineLayoutItem)
        return String();

    unsigned start = m_inlineTextBox->start();
    unsigned len = m_inlineTextBox->len();
    if (Node* node = m_lineLayoutItem.node()) {
        if (node->isTextNode())
            return plainText(EphemeralRange(Position(node, start), Position(node, start + len)), TextIteratorIgnoresStyleVisibility);
        return plainText(EphemeralRange(Position(node, PositionAnchorType::BeforeAnchor), Position(node, PositionAnchorType::AfterAnchor)), TextIteratorIgnoresStyleVisibility);
    }

    String result = m_lineLayoutItem.text().substring(start, len).simplifyWhiteSpace(WTF::DoNotStripWhiteSpace);
    if (m_inlineTextBox->nextTextBox() && m_inlineTextBox->nextTextBox()->start() > m_inlineTextBox->end() && result.length() && !result.right(1).containsOnlyWhitespace())
        return result + " ";
    return result;
}

bool AbstractInlineTextBox::isFirst() const
{
    ASSERT(!m_inlineTextBox || !m_inlineTextBox->getLineLayoutItem().needsLayout());
    return !m_inlineTextBox || !m_inlineTextBox->prevTextBox();
}

bool AbstractInlineTextBox::isLast() const
{
    ASSERT(!m_inlineTextBox || !m_inlineTextBox->getLineLayoutItem().needsLayout());
    return !m_inlineTextBox || !m_inlineTextBox->nextTextBox();
}

PassRefPtr<AbstractInlineTextBox> AbstractInlineTextBox::nextOnLine() const
{
    ASSERT(!m_inlineTextBox || !m_inlineTextBox->getLineLayoutItem().needsLayout());
    if (!m_inlineTextBox)
        return nullptr;

    InlineBox* next = m_inlineTextBox->nextOnLine();
    if (next && next->isInlineTextBox())
        return getOrCreate(toInlineTextBox(next)->getLineLayoutItem(), toInlineTextBox(next));

    return nullptr;
}

PassRefPtr<AbstractInlineTextBox> AbstractInlineTextBox::previousOnLine() const
{
    ASSERT(!m_inlineTextBox || !m_inlineTextBox->getLineLayoutItem().needsLayout());
    if (!m_inlineTextBox)
        return nullptr;

    InlineBox* previous = m_inlineTextBox->prevOnLine();
    if (previous && previous->isInlineTextBox())
        return getOrCreate(toInlineTextBox(previous)->getLineLayoutItem(), toInlineTextBox(previous));

    return nullptr;
}

} // namespace blink
