/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "core/accessibility/AXListBox.h"

#include "core/accessibility/AXListBoxOption.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/html/HTMLSelectElement.h"
#include "core/rendering/RenderListBox.h"


namespace WebCore {

using namespace HTMLNames;

AXListBox::AXListBox(RenderObject* renderer)
    : AXRenderObject(renderer)
{
}

AXListBox::~AXListBox()
{
}

PassRefPtr<AXListBox> AXListBox::create(RenderObject* renderer)
{
    return adoptRef(new AXListBox(renderer));
}

void AXListBox::addChildren()
{
    Node* selectNode = m_renderer->node();
    if (!selectNode)
        return;

    m_haveChildren = true;

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = toHTMLSelectElement(selectNode)->listItems();
    unsigned length = listItems.size();
    for (unsigned i = 0; i < length; i++) {
        // The cast to HTMLElement below is safe because the only other possible listItem type
        // would be a WMLElement, but WML builds don't use accessibility features at all.
        AXObject* listOption = listBoxOptionAXObject(listItems[i]);
        if (listOption && !listOption->accessibilityIsIgnored())
            m_children.append(listOption);
    }
}

AXObject* AXListBox::listBoxOptionAXObject(HTMLElement* element) const
{
    // skip hr elements
    if (!element || isHTMLHRElement(*element))
        return 0;

    AXObject* listBoxObject = m_renderer->document().axObjectCache()->getOrCreate(ListBoxOptionRole);
    toAXListBoxOption(listBoxObject)->setHTMLElement(element);

    return listBoxObject;
}

AXObject* AXListBox::elementAccessibilityHitTest(const IntPoint& point) const
{
    // the internal HTMLSelectElement methods for returning a listbox option at a point
    // ignore optgroup elements.
    if (!m_renderer)
        return 0;

    Node* node = m_renderer->node();
    if (!node)
        return 0;

    LayoutRect parentRect = elementRect();

    AXObject* listBoxOption = 0;
    unsigned length = m_children.size();
    for (unsigned i = 0; i < length; i++) {
        LayoutRect rect = toRenderListBox(m_renderer)->itemBoundingBoxRect(parentRect.location(), i);
        // The cast to HTMLElement below is safe because the only other possible listItem type
        // would be a WMLElement, but WML builds don't use accessibility features at all.
        if (rect.contains(point)) {
            listBoxOption = m_children[i].get();
            break;
        }
    }

    if (listBoxOption && !listBoxOption->accessibilityIsIgnored())
        return listBoxOption;

    return axObjectCache()->getOrCreate(m_renderer);
}

} // namespace WebCore
