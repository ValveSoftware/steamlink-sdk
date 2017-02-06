// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PersistentNode_h
#define PersistentNode_h

#include "platform/PlatformExport.h"
#include "platform/heap/ThreadState.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include "wtf/ThreadingPrimitives.h"
#include <memory>

namespace blink {

class CrossThreadPersistentRegion;

class PersistentNode final {
    DISALLOW_NEW();
public:
    PersistentNode()
        : m_self(nullptr)
        , m_trace(nullptr)
    {
        ASSERT(isUnused());
    }

#if ENABLE(ASSERT)
    ~PersistentNode()
    {
        // If you hit this assert, it means that the thread finished
        // without clearing persistent handles that the thread created.
        // We don't enable the assert for the main thread because the
        // main thread finishes without clearing all persistent handles.
        ASSERT(isMainThread() || isUnused());
    }
#endif

    // It is dangerous to copy the PersistentNode because it breaks the
    // free list.
    PersistentNode& operator=(const PersistentNode& otherref) = delete;

    // Ideally the trace method should be virtual and automatically dispatch
    // to the most specific implementation. However having a virtual method
    // on PersistentNode leads to too eager template instantiation with MSVC
    // which leads to include cycles.
    // Instead we call the constructor with a TraceCallback which knows the
    // type of the most specific child and calls trace directly. See
    // TraceMethodDelegate in Visitor.h for how this is done.
    void tracePersistentNode(Visitor* visitor)
    {
        ASSERT(!isUnused());
        ASSERT(m_trace);
        m_trace(visitor, m_self);
    }

    void initialize(void* self, TraceCallback trace)
    {
        ASSERT(isUnused());
        m_self = self;
        m_trace = trace;
    }

    void setFreeListNext(PersistentNode* node)
    {
        ASSERT(!node || node->isUnused());
        m_self = node;
        m_trace = nullptr;
        ASSERT(isUnused());
    }

    PersistentNode* freeListNext()
    {
        ASSERT(isUnused());
        PersistentNode* node = reinterpret_cast<PersistentNode*>(m_self);
        ASSERT(!node || node->isUnused());
        return node;
    }

    bool isUnused() const
    {
        return !m_trace;
    }

    void* self() const
    {
        return m_self;
    }

private:
    // If this PersistentNode is in use:
    //   - m_self points to the corresponding Persistent handle.
    //   - m_trace points to the trace method.
    // If this PersistentNode is freed:
    //   - m_self points to the next freed PersistentNode.
    //   - m_trace is nullptr.
    void* m_self;
    TraceCallback m_trace;
};

struct PersistentNodeSlots final {
    USING_FAST_MALLOC(PersistentNodeSlots);
private:
    static const int slotCount = 256;
    PersistentNodeSlots* m_next;
    PersistentNode m_slot[slotCount];
    friend class PersistentRegion;
    friend class CrossThreadPersistentRegion;
};

// PersistentRegion provides a region of PersistentNodes. PersistentRegion
// holds a linked list of PersistentNodeSlots, each of which stores
// a predefined number of PersistentNodes. You can call allocatePersistentNode/
// freePersistentNode to allocate/free a PersistentNode on the region.
class PLATFORM_EXPORT PersistentRegion final {
    USING_FAST_MALLOC(PersistentRegion);
public:
    PersistentRegion()
        : m_freeListHead(nullptr)
        , m_slots(nullptr)
#if ENABLE(ASSERT)
        , m_persistentCount(0)
#endif
    {
    }
    ~PersistentRegion();

    PersistentNode* allocatePersistentNode(void* self, TraceCallback trace)
    {
#if ENABLE(ASSERT)
        ++m_persistentCount;
#endif
        if (UNLIKELY(!m_freeListHead))
            ensurePersistentNodeSlots(self, trace);
        ASSERT(m_freeListHead);
        PersistentNode* node = m_freeListHead;
        m_freeListHead = m_freeListHead->freeListNext();
        node->initialize(self, trace);
        ASSERT(!node->isUnused());
        return node;
    }

    void freePersistentNode(PersistentNode* persistentNode)
    {
        ASSERT(m_persistentCount > 0);
        persistentNode->setFreeListNext(m_freeListHead);
        m_freeListHead = persistentNode;
#if ENABLE(ASSERT)
        --m_persistentCount;
#endif
    }

    static bool shouldTracePersistentNode(Visitor*, PersistentNode*) { return true; }

    void releasePersistentNode(PersistentNode*, ThreadState::PersistentClearCallback);
    using ShouldTraceCallback = bool (*)(Visitor*, PersistentNode*);
    void tracePersistentNodes(Visitor*, ShouldTraceCallback = PersistentRegion::shouldTracePersistentNode);
    int numberOfPersistents();

private:
    friend CrossThreadPersistentRegion;

    void ensurePersistentNodeSlots(void*, TraceCallback);

    PersistentNode* m_freeListHead;
    PersistentNodeSlots* m_slots;
#if ENABLE(ASSERT)
    int m_persistentCount;
#endif
};

class CrossThreadPersistentRegion final {
    USING_FAST_MALLOC(CrossThreadPersistentRegion);
public:
    CrossThreadPersistentRegion() : m_persistentRegion(wrapUnique(new PersistentRegion)) { }

    void allocatePersistentNode(PersistentNode*& persistentNode, void* self, TraceCallback trace)
    {
        MutexLocker lock(m_mutex);
        PersistentNode* node = m_persistentRegion->allocatePersistentNode(self, trace);
        releaseStore(reinterpret_cast<void* volatile*>(&persistentNode), node);
    }

    void freePersistentNode(PersistentNode*& persistentNode)
    {
        MutexLocker lock(m_mutex);
        // When the thread that holds the heap object that the cross-thread persistent shuts down,
        // prepareForThreadStateTermination() will clear out the associated CrossThreadPersistent<>
        // and PersistentNode so as to avoid unsafe access. This can overlap with a holder of
        // the CrossThreadPersistent<> also clearing the persistent and freeing the PersistentNode.
        //
        // The lock ensures the updating is ordered, but by the time lock has been acquired the
        // PersistentNode reference may have been cleared out already; check for this.
        if (!persistentNode)
            return;
        m_persistentRegion->freePersistentNode(persistentNode);
        releaseStore(reinterpret_cast<void* volatile*>(&persistentNode), nullptr);
    }

    class LockScope final {
        STACK_ALLOCATED();
    public:
        LockScope(CrossThreadPersistentRegion& persistentRegion)
            : m_persistentRegion(persistentRegion)
        {
            m_persistentRegion.lock();
        }
        ~LockScope()
        {
            m_persistentRegion.unlock();
        }
    private:
        CrossThreadPersistentRegion& m_persistentRegion;
    };

    void tracePersistentNodes(Visitor* visitor)
    {
        // If this assert triggers, you're tracing without being in a LockScope.
#if ENABLE(ASSERT)
        DCHECK(m_mutex.locked());
#endif
        m_persistentRegion->tracePersistentNodes(visitor, CrossThreadPersistentRegion::shouldTracePersistentNode);
    }

    void prepareForThreadStateTermination(ThreadState*);

    static bool shouldTracePersistentNode(Visitor*, PersistentNode*);

private:
    friend class LockScope;

    void lock()
    {
        m_mutex.lock();
    }

    void unlock()
    {
        m_mutex.unlock();
    }

    // We don't make CrossThreadPersistentRegion inherit from PersistentRegion
    // because we don't want to virtualize performance-sensitive methods
    // such as PersistentRegion::allocate/freePersistentNode.
    std::unique_ptr<PersistentRegion> m_persistentRegion;

    // Recursive as prepareForThreadStateTermination() clears a PersistentNode's
    // associated Persistent<> -- it in turn freeing the PersistentNode. And both
    // CrossThreadPersistentRegion operations need a lock on the region before
    // mutating.
    RecursiveMutex m_mutex;
};

} // namespace blink

#endif
