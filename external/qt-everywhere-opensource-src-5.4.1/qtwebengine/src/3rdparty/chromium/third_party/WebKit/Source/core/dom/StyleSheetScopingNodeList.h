/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef StyleSheetScopingNodeList_h
#define StyleSheetScopingNodeList_h

#include "core/dom/ContainerNode.h"
#include "core/dom/DocumentOrderedList.h"
#include "wtf/FastAllocBase.h"
#include "wtf/ListHashSet.h"

namespace WebCore {

class StyleSheetScopingNodeList {
    WTF_MAKE_NONCOPYABLE(StyleSheetScopingNodeList); WTF_MAKE_FAST_ALLOCATED;
public:
    StyleSheetScopingNodeList() { }

    void add(ContainerNode*);
    void remove(ContainerNode*);
    void clear()
    {
        m_scopingNodes.clear();
        m_scopingNodesRemoved.clear();
    }

    void didRemoveScopingNodes()
    {
        if (!m_scopingNodesRemoved)
            return;
        m_scopingNodesRemoved->clear();
    }

    DocumentOrderedList* scopingNodes() { return m_scopingNodes.get(); }
    ListHashSet<Node*, 4>* scopingNodesRemoved() { return m_scopingNodesRemoved.get(); }

private:
    OwnPtr<DocumentOrderedList> m_scopingNodes;
    OwnPtr<ListHashSet<Node*, 4> > m_scopingNodesRemoved;
};

}

#endif
