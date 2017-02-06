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

#include "core/dom/NodeIteratorBase.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Node.h"
#include "core/dom/NodeFilter.h"

namespace blink {

NodeIteratorBase::NodeIteratorBase(Node* rootNode, unsigned whatToShow, NodeFilter* nodeFilter)
    : m_root(rootNode)
    , m_whatToShow(whatToShow)
    , m_filter(nodeFilter)
{
}

unsigned NodeIteratorBase::acceptNode(Node* node, ExceptionState& exceptionState) const
{
    // The bit twiddling here is done to map DOM node types, which are given as integers from
    // 1 through 14, to whatToShow bit masks.
    if (!(((1 << (node->getNodeType() - 1)) & m_whatToShow)))
        return NodeFilter::FILTER_SKIP;
    if (!m_filter)
        return NodeFilter::FILTER_ACCEPT;
    return m_filter->acceptNode(node, exceptionState);
}

DEFINE_TRACE(NodeIteratorBase)
{
    visitor->trace(m_root);
    visitor->trace(m_filter);
}

DEFINE_TRACE_WRAPPERS(NodeIteratorBase)
{
    visitor->traceWrappers(m_filter);
}

} // namespace blink
