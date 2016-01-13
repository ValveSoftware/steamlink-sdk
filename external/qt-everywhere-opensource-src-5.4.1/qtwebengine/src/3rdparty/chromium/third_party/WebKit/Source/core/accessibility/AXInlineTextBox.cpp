/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/accessibility/AXInlineTextBox.h"

#include "core/accessibility/AXObjectCache.h"
#include "core/dom/Range.h"
#include "core/rendering/RenderText.h"
#include "platform/LayoutUnit.h"


namespace WebCore {

using namespace HTMLNames;

AXInlineTextBox::AXInlineTextBox(PassRefPtr<AbstractInlineTextBox> inlineTextBox)
    : m_inlineTextBox(inlineTextBox)
{
    RenderText* renderText = m_inlineTextBox->renderText();
    m_axObjectCache = renderText->document().axObjectCache();
}

AXInlineTextBox::~AXInlineTextBox()
{
    if (m_axObjectCache && m_inlineTextBox)
        m_axObjectCache->remove(m_inlineTextBox.get());
}

PassRefPtr<AXInlineTextBox> AXInlineTextBox::create(PassRefPtr<AbstractInlineTextBox> inlineTextBox)
{
    return adoptRef(new AXInlineTextBox(inlineTextBox));
}

void AXInlineTextBox::init()
{
}

void AXInlineTextBox::detach()
{
    m_inlineTextBox = nullptr;
    m_axObjectCache = 0;
    AXObject::detach();
}

LayoutRect AXInlineTextBox::elementRect() const
{
    if (!m_inlineTextBox)
        return LayoutRect();

    return m_inlineTextBox->bounds();
}

bool AXInlineTextBox::computeAccessibilityIsIgnored() const
{
    if (AXObject* parent = parentObject())
        return parent->accessibilityIsIgnored();

    return false;
}

void AXInlineTextBox::textCharacterOffsets(Vector<int>& offsets) const
{
    if (!m_inlineTextBox)
        return;

    unsigned len = m_inlineTextBox->len();
    Vector<float> widths;
    m_inlineTextBox->characterWidths(widths);
    ASSERT(widths.size() == len);
    offsets.resize(len);

    float widthSoFar = 0;
    for (unsigned i = 0; i < len; i++) {
        widthSoFar += widths[i];
        offsets[i] = lroundf(widthSoFar);
    }
}

void AXInlineTextBox::wordBoundaries(Vector<PlainTextRange>& words) const
{
    if (!m_inlineTextBox)
        return;

    Vector<AbstractInlineTextBox::WordBoundaries> wordBoundaries;
    m_inlineTextBox->wordBoundaries(wordBoundaries);
    words.resize(wordBoundaries.size());
    for (unsigned i = 0; i < wordBoundaries.size(); i++)
        words[i] = PlainTextRange(wordBoundaries[i].startIndex, wordBoundaries[i].endIndex - wordBoundaries[i].startIndex);
}

String AXInlineTextBox::stringValue() const
{
    if (!m_inlineTextBox)
        return String();

    return m_inlineTextBox->text();
}

AXObject* AXInlineTextBox::parentObject() const
{
    if (!m_inlineTextBox || !m_axObjectCache)
        return 0;

    RenderText* renderText = m_inlineTextBox->renderText();
    return m_axObjectCache->getOrCreate(renderText);
}

AccessibilityTextDirection AXInlineTextBox::textDirection() const
{
    if (!m_inlineTextBox)
        return AccessibilityTextDirectionLeftToRight;

    switch (m_inlineTextBox->direction()) {
    case AbstractInlineTextBox::LeftToRight:
        return AccessibilityTextDirectionLeftToRight;
    case AbstractInlineTextBox::RightToLeft:
        return AccessibilityTextDirectionRightToLeft;
    case AbstractInlineTextBox::TopToBottom:
        return AccessibilityTextDirectionTopToBottom;
    case AbstractInlineTextBox::BottomToTop:
        return AccessibilityTextDirectionBottomToTop;
    }

    return AccessibilityTextDirectionLeftToRight;
}

} // namespace WebCore
