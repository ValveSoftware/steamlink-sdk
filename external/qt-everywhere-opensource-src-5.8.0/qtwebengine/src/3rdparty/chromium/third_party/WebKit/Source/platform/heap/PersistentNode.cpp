// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/PersistentNode.h"

#include "platform/heap/Handle.h"

namespace blink {

namespace {

class DummyGCBase final : public GarbageCollected<DummyGCBase> {
public:
    DEFINE_INLINE_TRACE() { }
};

}

PersistentRegion::~PersistentRegion()
{
    PersistentNodeSlots* slots = m_slots;
    while (slots) {
        PersistentNodeSlots* deadSlots = slots;
        slots = slots->m_next;
        delete deadSlots;
    }
}

int PersistentRegion::numberOfPersistents()
{
    int persistentCount = 0;
    for (PersistentNodeSlots* slots = m_slots; slots; slots = slots->m_next) {
        for (int i = 0; i < PersistentNodeSlots::slotCount; ++i) {
            if (!slots->m_slot[i].isUnused())
                ++persistentCount;
        }
    }
    ASSERT(persistentCount == m_persistentCount);
    return persistentCount;
}

void PersistentRegion::ensurePersistentNodeSlots(void* self, TraceCallback trace)
{
    ASSERT(!m_freeListHead);
    PersistentNodeSlots* slots = new PersistentNodeSlots;
    for (int i = 0; i < PersistentNodeSlots::slotCount; ++i) {
        PersistentNode* node = &slots->m_slot[i];
        node->setFreeListNext(m_freeListHead);
        m_freeListHead = node;
        ASSERT(node->isUnused());
    }
    slots->m_next = m_slots;
    m_slots = slots;
}

void PersistentRegion::releasePersistentNode(PersistentNode* persistentNode, ThreadState::PersistentClearCallback callback)
{
    ASSERT(!persistentNode->isUnused());
    // 'self' is in use, containing the persistent wrapper object.
    void* self = persistentNode->self();
    if (callback) {
        (*callback)(self);
        ASSERT(persistentNode->isUnused());
        return;
    }
    Persistent<DummyGCBase>* persistent = reinterpret_cast<Persistent<DummyGCBase>*>(self);
    persistent->clear();
    ASSERT(persistentNode->isUnused());
}

// This function traces all PersistentNodes. If we encounter
// a PersistentNodeSlot that contains only freed PersistentNodes,
// we delete the PersistentNodeSlot. This function rebuilds the free
// list of PersistentNodes.
void PersistentRegion::tracePersistentNodes(Visitor* visitor, ShouldTraceCallback shouldTrace)
{
    size_t debugMarkedObjectSize = ProcessHeap::totalMarkedObjectSize();
    base::debug::Alias(&debugMarkedObjectSize);

    m_freeListHead = nullptr;
    int persistentCount = 0;
    PersistentNodeSlots** prevNext = &m_slots;
    PersistentNodeSlots* slots = m_slots;
    while (slots) {
        PersistentNode* freeListNext = nullptr;
        PersistentNode* freeListLast = nullptr;
        int freeCount = 0;
        for (int i = 0; i < PersistentNodeSlots::slotCount; ++i) {
            PersistentNode* node = &slots->m_slot[i];
            if (node->isUnused()) {
                if (!freeListNext)
                    freeListLast = node;
                node->setFreeListNext(freeListNext);
                freeListNext = node;
                ++freeCount;
            } else {
                ++persistentCount;
                if (!shouldTrace(visitor, node))
                    continue;
                node->tracePersistentNode(visitor);
                debugMarkedObjectSize = ProcessHeap::totalMarkedObjectSize();
            }
        }
        if (freeCount == PersistentNodeSlots::slotCount) {
            PersistentNodeSlots* deadSlots = slots;
            *prevNext = slots->m_next;
            slots = slots->m_next;
            delete deadSlots;
        } else {
            if (freeListLast) {
                ASSERT(freeListNext);
                ASSERT(!freeListLast->freeListNext());
                freeListLast->setFreeListNext(m_freeListHead);
                m_freeListHead = freeListNext;
            }
            prevNext = &slots->m_next;
            slots = slots->m_next;
        }
    }
    ASSERT(persistentCount == m_persistentCount);
}

bool CrossThreadPersistentRegion::shouldTracePersistentNode(Visitor* visitor, PersistentNode* node)
{
    CrossThreadPersistent<DummyGCBase>* persistent = reinterpret_cast<CrossThreadPersistent<DummyGCBase>*>(node->self());
    DCHECK(persistent);
    DCHECK(!persistent->isHashTableDeletedValue());
    Address rawObject = reinterpret_cast<Address>(persistent->get());
    if (!rawObject)
        return false;
    return &visitor->heap() == &ThreadState::fromObject(rawObject)->heap();
}

void CrossThreadPersistentRegion::prepareForThreadStateTermination(ThreadState* threadState)
{
    // For heaps belonging to a thread that's detaching, any cross-thread persistents
    // pointing into them needs to be disabled. Do that by clearing out the underlying
    // heap reference.
    MutexLocker lock(m_mutex);

    // TODO(sof): consider ways of reducing overhead. (e.g., tracking number of active
    // CrossThreadPersistent<>s pointing into the heaps of each ThreadState and use that
    // count to bail out early.)
    PersistentNodeSlots* slots = m_persistentRegion->m_slots;
    while (slots) {
        for (int i = 0; i < PersistentNodeSlots::slotCount; ++i) {
            if (slots->m_slot[i].isUnused())
                continue;

            // 'self' is in use, containing the cross-thread persistent wrapper object.
            CrossThreadPersistent<DummyGCBase>* persistent = reinterpret_cast<CrossThreadPersistent<DummyGCBase>*>(slots->m_slot[i].self());
            ASSERT(persistent);
            void* rawObject = persistent->atomicGet();
            if (!rawObject)
                continue;
            BasePage* page = pageFromObject(rawObject);
            ASSERT(page);
            // The main thread will upon detach just mark its heap pages as orphaned,
            // but not invalidate its CrossThreadPersistent<>s.
            if (page->orphaned())
                continue;
            if (page->arena()->getThreadState() == threadState) {
                persistent->clear();
                ASSERT(slots->m_slot[i].isUnused());
            }
        }
        slots = slots->m_next;
    }
}

} // namespace blink
