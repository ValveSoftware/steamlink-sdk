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

#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/dom/MutationObserverRegistration.h"
#include "core/dom/NodeListsNodeData.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"

namespace blink {

class NodeMutationObserverData final
    : public GarbageCollected<NodeMutationObserverData> {
  WTF_MAKE_NONCOPYABLE(NodeMutationObserverData);

 public:
  static NodeMutationObserverData* create() {
    return new NodeMutationObserverData;
  }

  const HeapVector<TraceWrapperMember<MutationObserverRegistration>>&
  registry() {
    return m_registry;
  }

  const HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>&
  transientRegistry() {
    return m_transientRegistry;
  }

  void addTransientRegistration(MutationObserverRegistration* registration) {
    m_transientRegistry.add(
        TraceWrapperMember<MutationObserverRegistration>(this, registration));
  }

  void removeTransientRegistration(MutationObserverRegistration* registration) {
    DCHECK(m_transientRegistry.contains(registration));
    m_transientRegistry.remove(registration);
  }

  void addRegistration(MutationObserverRegistration* registration) {
    m_registry.append(
        TraceWrapperMember<MutationObserverRegistration>(this, registration));
  }

  void removeRegistration(MutationObserverRegistration* registration) {
    DCHECK(m_registry.contains(registration));
    m_registry.remove(m_registry.find(registration));
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_registry);
    visitor->trace(m_transientRegistry);
  }

  DECLARE_TRACE_WRAPPERS() {
    for (auto registration : m_registry) {
      visitor->traceWrappers(registration);
    }
    for (auto registration : m_transientRegistry) {
      visitor->traceWrappers(registration);
    }
  }

 private:
  NodeMutationObserverData() {}

  HeapVector<TraceWrapperMember<MutationObserverRegistration>> m_registry;
  HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>
      m_transientRegistry;
};

class NodeRareData : public GarbageCollectedFinalized<NodeRareData>,
                     public NodeRareDataBase {
  WTF_MAKE_NONCOPYABLE(NodeRareData);

 public:
  static NodeRareData* create(LayoutObject* layoutObject) {
    return new NodeRareData(layoutObject);
  }

  void clearNodeLists() { m_nodeLists.clear(); }
  NodeListsNodeData* nodeLists() const { return m_nodeLists.get(); }
  // ensureNodeLists() and a following NodeListsNodeData functions must be
  // wrapped with a ThreadState::GCForbiddenScope in order to avoid an
  // initialized m_nodeLists is cleared by NodeRareData::traceAfterDispatch().
  NodeListsNodeData& ensureNodeLists() {
    DCHECK(ThreadState::current()->isGCForbidden());
    if (!m_nodeLists) {
      m_nodeLists = NodeListsNodeData::create();
      ScriptWrappableVisitor::writeBarrier(this, m_nodeLists);
    }
    return *m_nodeLists;
  }

  NodeMutationObserverData* mutationObserverData() {
    return m_mutationObserverData.get();
  }
  NodeMutationObserverData& ensureMutationObserverData() {
    if (!m_mutationObserverData) {
      m_mutationObserverData = NodeMutationObserverData::create();
      ScriptWrappableVisitor::writeBarrier(this, m_mutationObserverData);
    }
    return *m_mutationObserverData;
  }

  unsigned connectedSubframeCount() const { return m_connectedFrameCount; }
  void incrementConnectedSubframeCount();
  void decrementConnectedSubframeCount() {
    DCHECK(m_connectedFrameCount);
    --m_connectedFrameCount;
  }

  bool hasElementFlag(ElementFlags mask) const { return m_elementFlags & mask; }
  void setElementFlag(ElementFlags mask, bool value) {
    m_elementFlags = (m_elementFlags & ~mask) | (-(int32_t)value & mask);
  }
  void clearElementFlag(ElementFlags mask) { m_elementFlags &= ~mask; }

  bool hasRestyleFlag(DynamicRestyleFlags mask) const {
    return m_restyleFlags & mask;
  }
  void setRestyleFlag(DynamicRestyleFlags mask) {
    m_restyleFlags |= mask;
    RELEASE_ASSERT(m_restyleFlags);
  }
  bool hasRestyleFlags() const { return m_restyleFlags; }
  void clearRestyleFlags() { m_restyleFlags = 0; }

  enum {
    ConnectedFrameCountBits = 10,  // Must fit Page::maxNumberOfFrames.
  };

  DECLARE_TRACE();

  DECLARE_TRACE_AFTER_DISPATCH();
  void finalizeGarbageCollectedObject();

  DECLARE_TRACE_WRAPPERS();
  DECLARE_TRACE_WRAPPERS_AFTER_DISPATCH();

 protected:
  explicit NodeRareData(LayoutObject* layoutObject)
      : NodeRareDataBase(layoutObject),
        m_connectedFrameCount(0),
        m_elementFlags(0),
        m_restyleFlags(0),
        m_isElementRareData(false) {}

 private:
  Member<NodeListsNodeData> m_nodeLists;
  Member<NodeMutationObserverData> m_mutationObserverData;

  unsigned m_connectedFrameCount : ConnectedFrameCountBits;
  unsigned m_elementFlags : NumberOfElementFlags;
  unsigned m_restyleFlags : NumberOfDynamicRestyleFlags;

 protected:
  unsigned m_isElementRareData : 1;
};

}  // namespace blink

#endif  // NodeRareData_h
