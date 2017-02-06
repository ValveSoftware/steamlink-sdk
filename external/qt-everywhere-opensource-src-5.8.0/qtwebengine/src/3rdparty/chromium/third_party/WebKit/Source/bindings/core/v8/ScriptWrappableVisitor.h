// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitor_h
#define ScriptWrappableVisitor_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/WrapperVisitor.h"
#include "wtf/Deque.h"
#include "wtf/Vector.h"
#include <v8.h>


namespace blink {

class HeapObjectHeader;
template<typename T> class Member;

class WrapperMarkingData {
public:
    friend class ScriptWrappableVisitor;

    WrapperMarkingData(
        void (*traceWrappersCallback)(const WrapperVisitor*, const void*),
        HeapObjectHeader* (*heapObjectHeaderCallback)(const void*),
        const void* object)
        : m_traceWrappersCallback(traceWrappersCallback)
        , m_heapObjectHeaderCallback(heapObjectHeaderCallback)
        , m_object(object)
    {
        DCHECK(m_traceWrappersCallback);
        DCHECK(m_heapObjectHeaderCallback);
        DCHECK(m_object);
    }

    inline void traceWrappers(WrapperVisitor* visitor)
    {
        if (m_object) {
            m_traceWrappersCallback(visitor, m_object);
        }
    }

    /**
     * Returns true when object was marked. Ignores (returns true) invalidated
     * objects.
     */
    inline bool isWrapperHeaderMarked()
    {
        return !m_object || heapObjectHeader()->isWrapperHeaderMarked();
    }

private:
    inline bool shouldBeInvalidated()
    {
        return m_object && !heapObjectHeader()->isMarked();
    }

    inline void invalidate()
    {
        m_object = nullptr;
    }

    inline const HeapObjectHeader* heapObjectHeader()
    {
        DCHECK(m_object);
        return m_heapObjectHeaderCallback(m_object);
    }

    void (*m_traceWrappersCallback)(const WrapperVisitor*, const void*);
    HeapObjectHeader* (*m_heapObjectHeaderCallback)(const void*);
    const void* m_object;
};

/**
 * ScriptWrappableVisitor is able to trace through the objects to get all
 * wrappers. It is used during V8 garbage collection.  When this visitor is
 * set to the v8::Isolate as its embedder heap tracer, V8 will call it during
 * its garbage collection. At the beginning, it will call TracePrologue, then
 * repeatedly it will call AdvanceTracing, and at the end it will call
 * TraceEpilogue. Everytime V8 finds new wrappers, it will let the tracer know
 * using RegisterV8References.
 */
class CORE_EXPORT ScriptWrappableVisitor : public WrapperVisitor, public v8::EmbedderHeapTracer {
public:
    ScriptWrappableVisitor(v8::Isolate* isolate) : m_isolate(isolate) {};
    ~ScriptWrappableVisitor() override;
    /**
     * Replace all dead objects in the marking deque with nullptr after oilpan
     * gc.
     */
    static void invalidateDeadObjectsInMarkingDeque(v8::Isolate*);

    void TracePrologue() override;
    void RegisterV8References(const std::vector<std::pair<void*, void*>>& internalFieldsOfPotentialWrappers) override;
    void RegisterV8Reference(const std::pair<void*, void*>& internalFields);
    bool AdvanceTracing(double deadlineInMs, v8::EmbedderHeapTracer::AdvanceTracingActions) override;
    void TraceEpilogue() override;
    void AbortTracing() override;
    void EnterFinalPause() override;

    void dispatchTraceWrappers(const ScriptWrappable*) const override;
#define DECLARE_DISPATCH_TRACE_WRAPPERS(className)                   \
    void dispatchTraceWrappers(const className*) const override;

    WRAPPER_VISITOR_SPECIAL_CLASSES(DECLARE_DISPATCH_TRACE_WRAPPERS);

#undef DECLARE_DISPATCH_TRACE_WRAPPERS
    void dispatchTraceWrappers(const void*) const override {}

    void traceWrappers(const ScopedPersistent<v8::Value>*) const override;
    void traceWrappers(const ScopedPersistent<v8::Object>*) const override;
    void markWrapper(const v8::PersistentBase<v8::Value>* handle) const;
    void markWrapper(const v8::PersistentBase<v8::Object>* handle) const override;

    void invalidateDeadObjectsInMarkingDeque();

    void pushToMarkingDeque(
        void (*traceWrappersCallback)(const WrapperVisitor*, const void*),
        HeapObjectHeader* (*heapObjectHeaderCallback)(const void*),
        const void* object) const override
    {
        m_markingDeque.append(WrapperMarkingData(
            traceWrappersCallback,
            heapObjectHeaderCallback,
            object));
#if DCHECK_IS_ON()
        if (!m_advancingTracing) {
            m_verifierDeque.append(WrapperMarkingData(
                traceWrappersCallback,
                heapObjectHeaderCallback,
                object));
        }
#endif
    }

    bool markWrapperHeader(HeapObjectHeader*) const;
    /**
     * Mark wrappers in all worlds for the given script wrappable as alive in
     * V8.
     */
    void markWrappersInAllWorlds(const ScriptWrappable*) const override;
    void markWrappersInAllWorlds(const void*) const override {}
private:
    /**
     * Is wrapper tracing currently in progress? True if TracePrologue has been
     * called, and TraceEpilogue has not yet been called.
     */
    bool m_tracingInProgress = false;
    /**
     * Is AdvanceTracing currently running? If not, we know that all calls of
     * pushToMarkingDeque are from V8 or new wrapper associations. And this
     * information is used by the verifier feature.
     */
    bool m_advancingTracing = false;
    void performCleanup();
    /**
     * Collection of objects we need to trace from. We assume it is safe to hold
     * on to the raw pointers because:
     *     * oilpan object cannot move
     *     * oilpan gc will call invalidateDeadObjectsInMarkingDeque to delete
     *       all obsolete objects
     */
    mutable WTF::Deque<WrapperMarkingData> m_markingDeque;
    /**
     * Collection of objects we started tracing from. We assume it is safe to hold
     * on to the raw pointers because:
     *     * oilpan object cannot move
     *     * oilpan gc will call invalidateDeadObjectsInMarkingDeque to delete
     *       all obsolete objects
     *
     * These objects are used when TraceWrappablesVerifier feature is enabled to
     * verify that all objects reachable in the atomic pause were marked
     * incrementally. If not, there is one or multiple write barriers missing.
     */
    mutable WTF::Deque<WrapperMarkingData> m_verifierDeque;
    /**
     * Collection of headers we need to unmark after the tracing finished. We
     * assume it is safe to hold on to the headers because:
     *     * oilpan objects cannot move
     *     * objects this headers belong to are invalidated by the oilpan
     *       gc in invalidateDeadObjectsInMarkingDeque.
     */
    mutable WTF::Vector<HeapObjectHeader*> m_headersToUnmark;
    v8::Isolate* m_isolate;
};

}
#endif
