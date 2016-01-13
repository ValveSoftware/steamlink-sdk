/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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
 *
 */

#ifndef NodeIteratorBase_h
#define NodeIteratorBase_h

#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class ExceptionState;
class Node;
class NodeFilter;

class NodeIteratorBase : public WillBeGarbageCollectedMixin {
public:
    virtual ~NodeIteratorBase() { }

    Node* root() const { return m_root.get(); }
    unsigned whatToShow() const { return m_whatToShow; }
    NodeFilter* filter() const { return m_filter.get(); }
    // |expandEntityReferences| first appeared in "DOM Level 2 Traversal and Range". However, this argument was
    // never implemented, and, in DOM4, the function argument |expandEntityReferences| is removed from
    // Document.createNodeIterator() and Document.createTreeWalker().
    bool expandEntityReferences() const { return false; }

    virtual void trace(Visitor*);

protected:
    NodeIteratorBase(PassRefPtrWillBeRawPtr<Node>, unsigned whatToShow, PassRefPtrWillBeRawPtr<NodeFilter>);
    short acceptNode(Node*, ExceptionState&) const;

private:
    RefPtrWillBeMember<Node> m_root;
    unsigned m_whatToShow;
    RefPtrWillBeMember<NodeFilter> m_filter;
};

} // namespace WebCore

#endif // NodeIteratorBase_h
