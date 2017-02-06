/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
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

#ifndef NodeRareData_h
#define NodeRareData_h

#include "core/dom/MutationObserverRegistration.h"
#include "core/dom/NodeListsNodeData.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"

namespace blink {

class NodeMutationObserverData final : public GarbageCollected<NodeMutationObserverData> {
    WTF_MAKE_NONCOPYABLE(NodeMutationObserverData);
public:
    HeapVector<Member<MutationObserverRegistration>> registry;
    HeapHashSet<Member<MutationObserverRegistration>> transientRegistry;

    static NodeMutationObserverData* create()
    {
        return new NodeMutationObserverData;
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(registry);
        visitor->trace(transientRegistry);
    }

    DECLARE_TRACE_WRAPPERS()
    {
        for (auto registration : registry) {
            visitor->traceWrappers(registration);
        }
        for (auto registration : transientRegistry) {
            visitor->traceWrappers(registration);
        }
    }

private:
    NodeMutationObserverData() { }
};

class NodeRareData : public GarbageCollectedFinalized<NodeRareData>, public NodeRareDataBase {
    WTF_MAKE_NONCOPYABLE(NodeRareData);
public:
    static NodeRareData* create(LayoutObject* layoutObject)
    {
        return new NodeRareData(layoutObject);
    }

    void clearNodeLists() { m_nodeLists.clear(); }
    NodeListsNodeData* nodeLists() const { return m_nodeLists.get(); }
    NodeListsNodeData& ensureNodeLists()
    {
        if (!m_nodeLists)
            m_nodeLists = NodeListsNodeData::create();
        return *m_nodeLists;
    }

    NodeMutationObserverData* mutationObserverData() { return m_mutationObserverData.get(); }
    NodeMutationObserverData& ensureMutationObserverData()
    {
        if (!m_mutationObserverData)
            m_mutationObserverData = NodeMutationObserverData::create();
        return *m_mutationObserverData;
    }

    unsigned connectedSubframeCount() const { return m_connectedFrameCount; }
    void incrementConnectedSubframeCount();
    void decrementConnectedSubframeCount()
    {
        DCHECK(m_connectedFrameCount);
        --m_connectedFrameCount;
    }

    bool hasElementFlag(ElementFlags mask) const { return m_elementFlags & mask; }
    void setElementFlag(ElementFlags mask, bool value) { m_elementFlags = (m_elementFlags & ~mask) | (-(int32_t)value & mask); }
    void clearElementFlag(ElementFlags mask) { m_elementFlags &= ~mask; }

    bool hasRestyleFlag(DynamicRestyleFlags mask) const { return m_restyleFlags & mask; }
    void setRestyleFlag(DynamicRestyleFlags mask) { m_restyleFlags |= mask; RELEASE_ASSERT(m_restyleFlags); }
    bool hasRestyleFlags() const { return m_restyleFlags; }
    void clearRestyleFlags() { m_restyleFlags = 0; }

    enum {
        ConnectedFrameCountBits = 10, // Must fit Page::maxNumberOfFrames.
    };

    DECLARE_TRACE();

    DECLARE_TRACE_AFTER_DISPATCH();
    void finalizeGarbageCollectedObject();

    DECLARE_TRACE_WRAPPERS();
    DECLARE_TRACE_WRAPPERS_AFTER_DISPATCH();

protected:
    explicit NodeRareData(LayoutObject* layoutObject)
        : NodeRareDataBase(layoutObject)
        , m_connectedFrameCount(0)
        , m_elementFlags(0)
        , m_restyleFlags(0)
        , m_isElementRareData(false)
    { }

private:
    Member<NodeListsNodeData> m_nodeLists;
    Member<NodeMutationObserverData> m_mutationObserverData;

    unsigned m_connectedFrameCount : ConnectedFrameCountBits;
    unsigned m_elementFlags : NumberOfElementFlags;
    unsigned m_restyleFlags : NumberOfDynamicRestyleFlags;
protected:
    unsigned m_isElementRareData : 1;
};

} // namespace blink

#endif // NodeRareData_h
