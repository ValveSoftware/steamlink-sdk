/*
 * Copyright (C) 2008, 2011, 2012 Apple Inc. All rights reserved.
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
#include "core/html/HTMLTableRowsCollection.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/HTMLTableRowElement.h"

namespace WebCore {

using namespace HTMLNames;

static bool isInHead(Element* row)
{
    return row->parentNode() && toElement(row->parentNode())->hasLocalName(theadTag);
}

static bool isInBody(Element* row)
{
    return row->parentNode() && toElement(row->parentNode())->hasLocalName(tbodyTag);
}

static bool isInFoot(Element* row)
{
    return row->parentNode() && toElement(row->parentNode())->hasLocalName(tfootTag);
}

HTMLTableRowElement* HTMLTableRowsCollection::rowAfter(HTMLTableElement& table, HTMLTableRowElement* previous)
{
    // Start by looking for the next row in this section.
    // Continue only if there is none.
    if (previous && previous->parentNode() != table) {
        if (HTMLTableRowElement* row = Traversal<HTMLTableRowElement>::nextSibling(*previous))
            return row;
    }

    // If still looking at head sections, find the first row in the next head section.
    HTMLElement* child = 0;
    if (!previous)
        child = Traversal<HTMLElement>::firstChild(table);
    else if (isInHead(previous))
        child = Traversal<HTMLElement>::nextSibling(*previous->parentNode());
    for (; child; child = Traversal<HTMLElement>::nextSibling(*child)) {
        if (child->hasLocalName(theadTag)) {
            if (HTMLTableRowElement* row = Traversal<HTMLTableRowElement>::firstChild(*child))
                return row;
        }
    }

    // If still looking at top level and bodies, find the next row in top level or the first in the next body section.
    if (!previous || isInHead(previous))
        child = Traversal<HTMLElement>::firstChild(table);
    else if (previous->parentNode() == table)
        child = Traversal<HTMLElement>::nextSibling(*previous);
    else if (isInBody(previous))
        child = Traversal<HTMLElement>::nextSibling(*previous->parentNode());
    for (; child; child = Traversal<HTMLElement>::nextSibling(*child)) {
        if (isHTMLTableRowElement(child))
            return toHTMLTableRowElement(child);
        if (child->hasLocalName(tbodyTag)) {
            if (HTMLTableRowElement* row = Traversal<HTMLTableRowElement>::firstChild(*child))
                return row;
        }
    }

    // Find the first row in the next foot section.
    if (!previous || !isInFoot(previous))
        child = Traversal<HTMLElement>::firstChild(table);
    else
        child = Traversal<HTMLElement>::nextSibling(*previous->parentNode());
    for (; child; child = Traversal<HTMLElement>::nextSibling(*child)) {
        if (child->hasLocalName(tfootTag)) {
            if (HTMLTableRowElement* row = Traversal<HTMLTableRowElement>::firstChild(*child))
                return row;
        }
    }

    return 0;
}

HTMLTableRowElement* HTMLTableRowsCollection::lastRow(HTMLTableElement& table)
{
    for (HTMLElement* child = Traversal<HTMLElement>::lastChild(table); child; child = Traversal<HTMLElement>::previousSibling(*child)) {
        if (child->hasLocalName(tfootTag)) {
            if (HTMLTableRowElement* lastRow = Traversal<HTMLTableRowElement>::lastChild(*child))
                return lastRow;
        }
    }

    for (HTMLElement* child = Traversal<HTMLElement>::lastChild(table); child; child = Traversal<HTMLElement>::previousSibling(*child)) {
        if (isHTMLTableRowElement(child))
            return toHTMLTableRowElement(child);
        if (child->hasLocalName(tbodyTag)) {
            if (HTMLTableRowElement* lastRow = Traversal<HTMLTableRowElement>::lastChild(*child))
                return lastRow;
        }
    }

    for (HTMLElement* child = Traversal<HTMLElement>::lastChild(table); child; child = Traversal<HTMLElement>::previousSibling(*child)) {
        if (child->hasLocalName(theadTag)) {
            if (HTMLTableRowElement* lastRow = Traversal<HTMLTableRowElement>::lastChild(*child))
                return lastRow;
        }
    }

    return 0;
}

// Must call get() on the table in case that argument is compiled before dereferencing the
// table to get at the collection cache. Order of argument evaluation is undefined and can
// differ between compilers.
HTMLTableRowsCollection::HTMLTableRowsCollection(ContainerNode& table)
    : HTMLCollection(table, TableRows, OverridesItemAfter)
{
    ASSERT(isHTMLTableElement(table));
}

PassRefPtrWillBeRawPtr<HTMLTableRowsCollection> HTMLTableRowsCollection::create(ContainerNode& table, CollectionType type)
{
    ASSERT_UNUSED(type, type == TableRows);
    return adoptRefWillBeNoop(new HTMLTableRowsCollection(table));
}

Element* HTMLTableRowsCollection::virtualItemAfter(Element* previous) const
{
    return rowAfter(toHTMLTableElement(ownerNode()), toHTMLTableRowElement(previous));
}

}
