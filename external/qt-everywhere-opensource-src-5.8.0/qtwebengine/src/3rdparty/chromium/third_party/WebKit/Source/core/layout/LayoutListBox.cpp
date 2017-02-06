/*
 * Copyright (C) 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 *               2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "core/layout/LayoutListBox.h"

#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/paint/PaintLayer.h"

namespace blink {

// Default size when the multiple attribute is present but size attribute is absent.
const int defaultSize = 4;

const int defaultPaddingBottom = 1;

LayoutListBox::LayoutListBox(Element* element)
    : LayoutBlockFlow(element)
{
    ASSERT(element);
    ASSERT(element->isHTMLElement());
    ASSERT(isHTMLSelectElement(element));
}

LayoutListBox::~LayoutListBox()
{
}

inline HTMLSelectElement* LayoutListBox::selectElement() const
{
    return toHTMLSelectElement(node());
}

unsigned LayoutListBox::size() const
{
    unsigned specifiedSize = selectElement()->size();
    if (specifiedSize >= 1)
        return specifiedSize;

    return defaultSize;
}

LayoutUnit LayoutListBox::defaultItemHeight() const
{
    return LayoutUnit(style()->getFontMetrics().height() + defaultPaddingBottom);
}

LayoutUnit LayoutListBox::itemHeight() const
{
    HTMLSelectElement* select = selectElement();
    if (!select)
        return LayoutUnit();

    const auto& items = select->listItems();
    if (items.isEmpty())
        return defaultItemHeight();

    LayoutUnit maxHeight;
    for (Element* element : items) {
        if (isHTMLOptGroupElement(element))
            element = &toHTMLOptGroupElement(element)->optGroupLabelElement();
        LayoutObject* layoutObject = element->layoutObject();
        LayoutUnit itemHeight;
        if (layoutObject && layoutObject->isBox())
            itemHeight = toLayoutBox(layoutObject)->size().height();
        else
            itemHeight = defaultItemHeight();
        maxHeight = std::max(maxHeight, itemHeight);
    }
    return maxHeight;
}

void LayoutListBox::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    LayoutUnit height = itemHeight() * size();
    // FIXME: The item height should have been added before updateLogicalHeight was called to avoid this hack.
    setIntrinsicContentLogicalHeight(height);

    height += borderAndPaddingHeight();

    LayoutBox::computeLogicalHeight(height, logicalTop, computedValues);
}

void LayoutListBox::stopAutoscroll()
{
    HTMLSelectElement* select = selectElement();
    if (select->isDisabledFormControl())
        return;
    select->handleMouseRelease();
}

void LayoutListBox::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    LayoutBlockFlow::computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);
    if (style()->width().hasPercent())
        minLogicalWidth = LayoutUnit();
}

void LayoutListBox::scrollToRect(const LayoutRect& rect)
{
    if (hasOverflowClip()) {
        ASSERT(layer());
        ASSERT(layer()->getScrollableArea());
        layer()->getScrollableArea()->scrollIntoView(rect, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
    }
}

} // namespace blink
