/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
#include "core/dom/StyleSheetScopingNodeList.h"

#include "core/dom/Document.h"
#include "core/dom/Node.h"

namespace WebCore {

void StyleSheetScopingNodeList::add(ContainerNode* node)
{
    ASSERT(node && node->inDocument());
    if (isTreeScopeRoot(node))
        return;

    if (!m_scopingNodes)
        m_scopingNodes = adoptPtr(new DocumentOrderedList());
    m_scopingNodes->add(node);

    if (m_scopingNodesRemoved)
        m_scopingNodesRemoved->remove(node);
}

void StyleSheetScopingNodeList::remove(ContainerNode* node)
{
    if (isTreeScopeRoot(node) || !m_scopingNodes)
        return;

    // If the node is still working as a scoping node, we cannot remove.
    if (node->inDocument())
        return;

    m_scopingNodes->remove(node);
    if (!m_scopingNodesRemoved)
        m_scopingNodesRemoved = adoptPtr(new ListHashSet<Node*, 4>());
    m_scopingNodesRemoved->add(node);
}

}
