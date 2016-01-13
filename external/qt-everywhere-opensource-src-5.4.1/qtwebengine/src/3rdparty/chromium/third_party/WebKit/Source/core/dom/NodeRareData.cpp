/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/dom/NodeRareData.h"
#include "core/dom/Element.h"
#include "core/dom/ElementRareData.h"
#include "platform/heap/Handle.h"

namespace WebCore {

struct SameSizeAsNodeRareData {
    void* m_pointer[2];
    OwnPtrWillBeMember<NodeMutationObserverData> m_mutationObserverData;
    unsigned m_bitfields;
};

COMPILE_ASSERT(sizeof(NodeRareData) == sizeof(SameSizeAsNodeRareData), NodeRareDataShouldStaySmall);

void NodeListsNodeData::invalidateCaches(const QualifiedName* attrName)
{
    NodeListAtomicNameCacheMap::const_iterator atomicNameCacheEnd = m_atomicNameCaches.end();
    for (NodeListAtomicNameCacheMap::const_iterator it = m_atomicNameCaches.begin(); it != atomicNameCacheEnd; ++it)
        it->value->invalidateCacheForAttribute(attrName);

    if (attrName)
        return;

    TagCollectionCacheNS::iterator tagCacheEnd = m_tagCollectionCacheNS.end();
    for (TagCollectionCacheNS::iterator it = m_tagCollectionCacheNS.begin(); it != tagCacheEnd; ++it)
        it->value->invalidateCache();
}

void NodeListsNodeData::trace(Visitor* visitor)
{
    visitor->trace(m_childNodeList);
    visitor->trace(m_atomicNameCaches);
    visitor->trace(m_tagCollectionCacheNS);
}

void NodeRareData::traceAfterDispatch(Visitor* visitor)
{
    visitor->trace(m_mutationObserverData);
    // Do not keep empty NodeListsNodeData objects around.
    if (m_nodeLists && m_nodeLists->isEmpty())
        m_nodeLists.clear();
    else
        visitor->trace(m_nodeLists);
}

void NodeRareData::trace(Visitor* visitor)
{
    if (m_isElementRareData)
        static_cast<ElementRareData*>(this)->traceAfterDispatch(visitor);
    else
        traceAfterDispatch(visitor);
}

void NodeRareData::finalizeGarbageCollectedObject()
{
    RELEASE_ASSERT(!renderer());
    if (m_isElementRareData)
        static_cast<ElementRareData*>(this)->~ElementRareData();
    else
        this->~NodeRareData();
}

// Ensure the 10 bits reserved for the m_connectedFrameCount cannot overflow
COMPILE_ASSERT(Page::maxNumberOfFrames < (1 << NodeRareData::ConnectedFrameCountBits), Frame_limit_should_fit_in_rare_data_count);

} // namespace WebCore
