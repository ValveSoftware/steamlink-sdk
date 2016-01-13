/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/dom/LiveNodeList.h"

namespace WebCore {

static inline bool isMatchingElement(const LiveNodeList& nodeList, const Element& element)
{
    return nodeList.elementMatches(element);
}

Node* LiveNodeList::virtualOwnerNode() const
{
    return &ownerNode();
}

void LiveNodeList::invalidateCache(Document*) const
{
    m_collectionIndexCache.invalidate();
}

Element* LiveNodeList::traverseToFirstElement() const
{
    return firstMatchingElement(*this);
}

Element* LiveNodeList::traverseToLastElement() const
{
    return lastMatchingElement(*this);
}

Element* LiveNodeList::traverseForwardToOffset(unsigned offset, Element& currentNode, unsigned& currentOffset) const
{
    return traverseMatchingElementsForwardToOffset(*this, offset, currentNode, currentOffset);
}

Element* LiveNodeList::traverseBackwardToOffset(unsigned offset, Element& currentNode, unsigned& currentOffset) const
{
    return traverseMatchingElementsBackwardToOffset(*this, offset, currentNode, currentOffset);
}

void LiveNodeList::trace(Visitor* visitor)
{
    visitor->trace(m_collectionIndexCache);
    LiveNodeListBase::trace(visitor);
    NodeList::trace(visitor);
}

} // namespace WebCore
