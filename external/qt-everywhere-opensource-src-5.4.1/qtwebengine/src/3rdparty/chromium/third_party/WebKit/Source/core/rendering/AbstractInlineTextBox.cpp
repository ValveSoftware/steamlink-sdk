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

#include "config.h"
#include "core/rendering/AbstractInlineTextBox.h"

#include "core/editing/TextIterator.h"
#include "platform/text/TextBreakIterator.h"

namespace WebCore {

AbstractInlineTextBox::InlineToAbstractInlineTextBoxHashMap* AbstractInlineTextBox::gAbstractInlineTextBoxMap = 0;

PassRefPtr<AbstractInlineTextBox> AbstractInlineTextBox::getOrCreate(RenderText* renderText, InlineTextBox* inlineTextBox)
{
    if (!inlineTextBox)
        return nullptr;

    if (!gAbstractInlineTextBoxMap)
        gAbstractInlineTextBoxMap = new InlineToAbstractInlineTextBoxHashMap();

    InlineToAbstractInlineTextBoxHashMap::const_iterator it = gAbstractInlineTextBoxMap->find(inlineTextBox);
    if (it != gAbstractInlineTextBoxMap->end())
        return it->value;

    RefPtr<AbstractInlineTextBox> obj = adoptRef(new AbstractInlineTextBox(renderText, inlineTextBox));
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

void AbstractInlineTextBox::detach()
{
    m_renderText = 0;
    m_inlineTextBox = 0;
}

PassRefPtr<AbstractInlineTextBox> AbstractInlineTextBox::nextInlineTextBox() const
{
    if (!m_inlineTextBox)
        return nullptr;

    return getOrCreate(m_renderText, m_inlineTextBox->nextTextBox());
}

LayoutRect AbstractInlineTextBox::bounds() const
{
    if (!m_inlineTextBox || !m_renderText)
        return LayoutRect();

    FloatRect boundaries = m_inlineTextBox->calculateBoundaries();
    return m_renderText->localToAbsoluteQuad(boundaries).enclosingBoundingBox();
}

unsigned AbstractInlineTextBox::start() const
{
    if (!m_inlineTextBox)
        return 0;

    return m_inlineTextBox->start();
}

unsigned AbstractInlineTextBox::len() const
{
    if (!m_inlineTextBox)
        return 0;

    return m_inlineTextBox->len();
}

AbstractInlineTextBox::Direction AbstractInlineTextBox::direction() const
{
    if (!m_inlineTextBox || !m_renderText)
        return LeftToRight;

    if (m_renderText->style()->isHorizontalWritingMode())
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
    if (!m_inlineTextBox || !m_renderText)
        return String();

    unsigned start = m_inlineTextBox->start();
    unsigned len = m_inlineTextBox->len();
    if (Node* node = m_renderText->node()) {
        RefPtrWillBeRawPtr<Range> range = Range::create(node->document());
        range->setStart(node, start, IGNORE_EXCEPTION);
        range->setEnd(node, start + len, IGNORE_EXCEPTION);
        return plainText(range.get(), TextIteratorIgnoresStyleVisibility);
    }

    String result = m_renderText->text().substring(start, len).simplifyWhiteSpace(WTF::DoNotStripWhiteSpace);
    if (m_inlineTextBox->nextTextBox() && m_inlineTextBox->nextTextBox()->start() > m_inlineTextBox->end() && result.length() && !result.right(1).containsOnlyWhitespace())
        return result + " ";
    return result;
}

} // namespace WebCore
